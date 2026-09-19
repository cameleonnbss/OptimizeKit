// OptimizeKit - network profile engine. Sources:
//  - Microsoft Learn: netsh int tcp, Set-NetAdapterPowerManagement, RSS/RSC docs
//  - Chris Titus WinUtil (MIT) network section
//  - technically valid settings only: no "0 ping" folklore.
#include "netprofile.h"
#include "engine.h"
#include "common.h"
#include <winsock2.h>
#include <iphlpapi.h>
#include <shellapi.h>
#include <objbase.h>
#include <sstream>
#include <mutex>

#pragma comment(lib, "iphlpapi.lib")

namespace ok::netprofile {

using json = nlohmann::json;

// ------------------------------------------------------------------ helpers
static bool runPs(const wstring& script, string& out, DWORD timeoutMs = 60000) {
    wstring cmd = L"powershell -NoProfile -NonInteractive -Command \"" + script + L"\"";
    return runCapture(cmd, out, timeoutMs);
}

static wstring getConnName(const wstring& guid) {
    wstring key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\NetworkList\\Profiles\\" + guid;
    wchar_t buf[256]; DWORD sz = sizeof(buf); DWORD type = 0;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, key.c_str(), L"ProfileName", RRF_RT_REG_SZ, &type, buf, &sz) == ERROR_SUCCESS)
        return buf;
    return L"Network";
}

// ------------------------------------------------------------------ adapters
vector<Adapter> adapters() {
    vector<Adapter> out;
    ULONG sz = 16 * 1024;
    vector<BYTE> buf(sz);
    auto* t = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_ALL_INTERFACES, nullptr, t, &sz) == ERROR_BUFFER_OVERFLOW) {
        buf.resize(sz); t = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    }
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_ALL_INTERFACES, nullptr, t, &sz) != NO_ERROR) return out;
    for (auto* a = t; a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        Adapter ad;
        ad.description = a->Description;
        ad.type = (a->IfType == IF_TYPE_IEEE80211) ? L"Wi-Fi" : L"Ethernet";
        wchar_t sp[64];
        unsigned long long mbps = a->TransmitLinkSpeed / 1000000ull;
        if (mbps > 0 && mbps < 1000000)
            swprintf(sp, 64, L"%llu Mbps", mbps);
        else swprintf(sp, 64, L"-");
        ad.speed = sp;
        wchar_t guid[64]; StringFromGUID2(a->NetworkGuid, guid, 64);
        wstring g = guid; if (!g.empty() && g.front() == L'{') g = g.substr(1, g.size() - 2);
        ad.name = getConnName(g) + L" (" + ad.description + L")";
        out.push_back(ad);
    }
    return out;
}

static wstring firstAdapterName() {
    auto ads = adapters();
    if (ads.empty()) return L"Ethernet";
    wstring n = ads[0].name;
    size_t p = n.find(L" (");
    if (p != wstring::npos) n = n.substr(0, p);
    return n;
}

// ------------------------------------------------------------------- status
Status status() {
    Status st;
    string out;
    if (runCapture(L"netsh int tcp show global", out, 15000)) {
        std::string s = out; for (auto& c : s) c = (char)tolower((unsigned char)c);
        if (s.find("disabled") != string::npos) st.autotuning = L"disabled";
        else if (s.find("experimental") != string::npos) st.autotuning = L"experimental";
        else st.autotuning = L"normal";
    }
    string ps;
    if (runPs(L"(Get-NetAdapterRsc | Select-Object -First 1).IPv4Enabled", ps, 20000)) {
        string s = ps; for (auto& c : s) c = (char)tolower((unsigned char)c);
        st.rscEnabled = s.find("true") != string::npos;
    }
    if (runPs(L"(Get-NetAdapterRss | Select-Object -First 1).Enabled", ps, 20000)) {
        string s = ps; for (auto& c : s) c = (char)tolower((unsigned char)c);
        st.rssEnabled = s.find("true") != string::npos;
    }
    string dns;
    if (runPs(L"(Get-DnsClientServerAddress -AddressFamily IPv4 | Where-Object {$_.ServerAddresses} | Select-Object -First 1).ServerAddresses -join ','", dns, 20000)) {
        while (!dns.empty() && (dns.back() == '\r' || dns.back() == '\n' || dns.back() == ' ')) dns.pop_back();
        st.dns = widen(dns);
    }
    string mtu;
    if (runCapture(L"netsh interface ipv4 show subinterfaces", mtu, 15000)) {
        std::istringstream ss(mtu); string line;
        while (std::getline(ss, line)) {
            if (line.find("Loopback") != string::npos || line.find("---") != string::npos) continue;
            // MTU is the FIRST column in every netsh locale (FR/EN/DE...); don't assume more columns
            int v = 0;
            if (sscanf(line.c_str(), " %d", &v) == 1 && v >= 576 && v <= 9000) { st.mtu = v; break; }
        }
    }
    return st;
}

json statusJson() {
    Status st = status();
    json j;
    j["autotuning"] = narrow(st.autotuning);
    j["rsc"] = st.rscEnabled;
    j["rss"] = st.rssEnabled;
    j["dns"] = narrow(st.dns);
    j["mtu"] = st.mtu;
    json arr = json::array();
    for (auto& a : adapters())
        arr.push_back({ {"name", narrow(a.name)}, {"type", narrow(a.type)}, {"speed", narrow(a.speed)} });
    j["adapters"] = arr;
    return j;
}

// ----------------------------------------------------------------- actions
bool setNicPowerSaving(bool disable, wstring& err) {
    // disable=true  -> stop Windows from turning the NIC off (lower latency)
    // disable=false -> Windows default (allow power off) = restore point
    string out;
    wstring script = L"Get-NetAdapter -Physical -ErrorAction SilentlyContinue | ForEach-Object { "
                     L"Set-NetAdapterPowerManagement -Name $_.Name -AllowComputerToTurnOffDevice:"
                     + wstring(disable ? L"$false" : L"$true") + L" -ErrorAction SilentlyContinue }";
    if (!runPs(script, out)) { err = L"Set-NetAdapterPowerManagement failed (admin required)"; return false; }
    log::ok(wstring(L"NIC power saving ") + (disable ? L"disabled" : L"restored (Windows default)"));
    return true;
}

bool setRsc(bool enabled, wstring& err) {
    string out;
    wstring cmd = L"netsh int tcp set global rsc=" + wstring(enabled ? L"enabled" : L"disabled");
    if (!runCapture(cmd, out, 20000)) { err = widen(out.empty() ? "netsh failed (admin?)" : out); return false; }
    log::ok(wstring(L"RSC ") + (enabled ? L"enabled" : L"disabled"));
    return true;
}

bool setAutotuning(const wstring& level, wstring& err) {
    string out;
    wstring cmd = L"netsh int tcp set global autotuninglevel=" + level;
    if (!runCapture(cmd, out, 20000)) { err = widen(out.empty() ? "netsh failed (admin?)" : out); return false; }
    log::ok(L"TCP autotuning -> " + level);
    return true;
}

bool setMtu(int mtu, wstring& err) {
    if (mtu < 576 || mtu > 9000) { err = L"MTU out of range (576-9000)"; return false; }
    string out;
    wstring cmd = L"netsh interface ipv4 set subinterface \"" + firstAdapterName() + L"\" mtu=" + std::to_wstring(mtu) + L" store=persistent";
    if (!runCapture(cmd, out, 20000)) { err = widen(out.empty() ? "netsh failed (admin?)" : out); return false; }
    log::ok(L"MTU -> " + std::to_wstring(mtu));
    return true;
}

bool flushDns(wstring& err) {
    string out;
    if (!runCapture(L"ipconfig /flushdns", out, 15000)) { err = widen(out); return false; }
    log::ok(L"DNS cache flushed");
    return true;
}

bool setDns(const wstring& primary, const wstring& secondary, wstring& err) {
    string out;
    wstring script = L"$a = Get-NetAdapter -Physical -ErrorAction Stop | Select-Object -First 1; "
                     L"Set-DnsClientServerAddress -InterfaceIndex $a.ifIndex -ServerAddresses " + primary +
                     (secondary.empty() ? L"" : (L"," + secondary));
    if (!runPs(script, out)) { err = L"Set-DnsClientServerAddress failed (admin required)"; return false; }
    log::ok(L"DNS -> " + primary + (secondary.empty() ? L"" : (L", " + secondary)));
    return true;
}

Result apply(const string& profile) {
    Result r;
    log::section(L"NETWORK PROFILE: " + widen(profile));

    auto step = [&](bool okRes, const wstring& e) {
        if (okRes) r.applied++;
        else { r.failed++; r.errors.push_back(e); log::fail(e); }
    };

    wstring e;
    if (profile == "restore") {
        step(setAutotuning(L"normal", e), e);
        step(setRsc(true, e), e);
        step(setNicPowerSaving(false, e), e);
        log::ok(L"Network defaults restored");
        return r;
    }
    if (profile == "balanced") {
        step(setAutotuning(L"normal", e), e);
        step(setRsc(true, e), e);
        step(flushDns(e), e);
        return r;
    }
    // gaming / low_latency / download
    step(setAutotuning(profile == "download" ? L"experimental" : L"normal", e), e);
    step(setRsc(false, e), e);
    step(setNicPowerSaving(true, e), e);
    if (profile == "low_latency" || profile == "gaming") {
        json cfg = engine::loadConfig();
        if (cfg.value("dns_managed", false)) {
            step(setDns(L"1.1.1.1", L"8.8.8.8", e), e);
        } else {
            log::info(L"DNS left untouched (enable 'dns_managed' in Settings > Network to allow it)");
        }
    }
    step(flushDns(e), e);
    log::ok(L"Network profile applied: " + widen(profile));
    return r;
}

} // namespace ok::netprofile
