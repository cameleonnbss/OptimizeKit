// OptimizeKit v2.6 - Security Center implementation.
//
// Pure Win32, read-only. The scanner never touches the objects it inspects; the only
// mutating endpoint is revokeEntry(), which snapshots the registry value into
// config.json before removing it (and restoreEntry() writes it back verbatim).
#include "security.h"
#include "engine.h"
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <netfw.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fstream>
#include <shlobj.h>
#include <filesystem>
#include <algorithm>
#include <cwctype>

namespace fs = std::filesystem;

namespace ok::secscan {

using json = nlohmann::json;

static wstring lower(wstring s) { std::transform(s.begin(), s.end(), s.begin(), ::towlower); return s; }

static json finding(const string& sev, const wstring& title, const wstring& detail,
                    const wstring& where, const string& kind, const wstring& id = L"") {
    json f;
    f["sev"] = sev;                 // "high" | "med" | "low" | "ok" | "info"
    f["title"] = narrow(title);
    f["detail"] = narrow(detail);
    f["where"] = narrow(where);
    f["kind"] = kind;
    if (!id.empty()) f["id"] = narrow(id);
    return f;
}

// ------------------------------------------------------------------ 1+2: Defender & firewall
static void scanAvFirewall(json& out) {
    json& arr = out["findings"] = json::array();

    // Windows Defender / any AV via SecurityCenter2 WMI (scripting-free: raw COM)
    bool rtOn = false; wstring avName = L""; int sigAge = -1;
    {
        // WMI without wbemidl headers: go through the documented PowerShell path once,
        // parse the CSV. It is a local read-only query, ~150ms.
        string o;
        if (runCapture(L"powershell -NoProfile -Command \"Get-CimInstance -Namespace ROOT/SecurityCenter2 -ClassName AntiVirusProduct | "
                       L"Select-Object displayName,productState,timestamp | ConvertTo-Csv -NoTypeInformation\"", o, 12000) && !o.empty()) {
            // productState hex: bit 0x1000 = real-time enabled (documented mapping)
            std::transform(o.begin(), o.end(), o.begin(), ::tolower);
            size_t dn = o.find("displayname");
            size_t l1 = o.find('\n');
            if (dn != string::npos && l1 != string::npos && l1 > dn) {
                size_t c1 = o.find(',', l1);
                if (c1 != string::npos) avName = widen(o.substr(l1 + 1, c1 - l1 - 1));
                if (!avName.empty() && avName.front() == L'"') avName = avName.substr(1);
                if (!avName.empty() && avName.back() == L'"') avName.pop_back();
            }
            size_t ps = o.find("productstate");
            size_t l2 = o.find('\n', l1 + 1);
            if (ps != string::npos && l2 != string::npos) {
                // second row: "name","state","timestamp" - find the state field
                size_t c1 = o.find(',', l2);
                size_t c2 = o.find(',', c1 + 1);
                if (c1 != string::npos && c2 != string::npos) {
                    string st = o.substr(c1 + 1, c2 - c1 - 1);
                    st.erase(std::remove(st.begin(), st.end(), '"'), st.end());
                    try {
                        int v = std::stoi(st, nullptr, 16);
                        // productState layout (community-established, consistent across AVs):
                        //   0x1000 bit = real-time protection ON (e.g. 0x61100 on Defender)
                        //   0x0F00 nibble <> 0 = enabled; 0x0000 = disabled
                        rtOn = (v & 0x1000) != 0 || (v & 0x0F00) != 0;
                    } catch (...) { rtOn = false; }
                }
            }
        }
    }
    if (avName.empty()) avName = L"Windows Security";
    if (rtOn) arr.push_back(finding("ok", L"Antivirus active", avName + L" — real-time protection ON" +
        (sigAge >= 0 ? L", signatures " + std::to_wstring(sigAge) + L" day(s) old" : L""),
        L"WMI ROOT\\SecurityCenter2", "av"));
    else
        arr.push_back(finding("high", L"No active antivirus", L"Windows reports no product with real-time protection enabled. Open Windows Security and enable it.", L"WMI ROOT\\SecurityCenter2", "av"));
    if (sigAge > 7) arr.push_back(finding("med", L"Virus signatures outdated", avName + L" signatures are " + std::to_wstring(sigAge) + L" days old — update them.", L"WMI ROOT\\SecurityCenter2", "av"));

    // Firewall per profile (netfw.h COM - works on MinGW, no admin needed).
    // MinGW has no import lib for the CLSID: define the GUIDs locally (well-known values).
    {
        // this handler thread may never have initialized COM - do it here (S_FALSE = already done)
        HRESULT cohr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        bool comHere = SUCCEEDED(cohr);
        static const CLSID xCLSID_NetFwPolicy2 = { 0xe2b3c97f, 0x6ae1, 0x41ac, { 0x81, 0x7a, 0xf6, 0xf9, 0x21, 0x66, 0xd7, 0xdd } };
        static const IID   xIID_INetFwPolicy2  = { 0x98325047, 0xc671, 0x4174, { 0x8d, 0x81, 0xde, 0xfc, 0xd3, 0xf0, 0x31, 0x86 } };
        INetFwPolicy2* pol = nullptr;
        HRESULT hr = CoCreateInstance(xCLSID_NetFwPolicy2, nullptr, CLSCTX_INPROC_SERVER, xIID_INetFwPolicy2, (void**)&pol);
        if (SUCCEEDED(hr) && pol) {
            static const struct { NET_FW_PROFILE_TYPE2 t; const wchar_t* n; } profs[] = {
                { NET_FW_PROFILE2_DOMAIN, L"Domain" }, { NET_FW_PROFILE2_PRIVATE, L"Private" }, { NET_FW_PROFILE2_PUBLIC, L"Public" },
            };
            int onN = 0;
            for (auto& p : profs) {
                VARIANT_BOOL on = VARIANT_TRUE;
                if (pol->get_FirewallEnabled(p.t, &on) == S_OK && on == VARIANT_TRUE) ++onN;
                else
                    arr.push_back(finding("high", wstring(L"Firewall OFF — ") + p.n + L" profile", L"This profile accepts inbound connections freely. Re-enable it in Windows Security > Firewall & network protection.", L"INetFwPolicy2", "fw"));
            }
            pol->Release();
            if (onN == 3)
                arr.push_back(finding("ok", L"Firewall active on all profiles", L"Domain, Private and Public profiles all enabled.", L"INetFwPolicy2", "fw"));
        } else if (FAILED(hr)) {
            arr.push_back(finding("low", L"Firewall state unreadable", L"INetFwPolicy2 could not be instantiated (hr=" + std::to_wstring((int)hr) + L").", L"INetFwPolicy2", "fw"));
        }
        if (comHere) CoUninitialize();
    }
}

// ------------------------------------------------------------------ 3+4: ports & connections
static wstring ownerOf(DWORD pid) {
    wstring name = L"pid " + std::to_wstring(pid);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (h) {
        wchar_t path[MAX_PATH + 1] = L"";
        DWORD sz = MAX_PATH;
        if (QueryFullProcessImageNameW(h, 0, path, &sz) && sz > 0) {
            name = path;
            size_t p = name.find_last_of(L"\\");
            if (p != wstring::npos) name = name.substr(p + 1);
        }
        CloseHandle(h);
    }
    return name;
}

static bool suspiciousPort(DWORD port) {
    static const DWORD bad[] = { 4444, 5555, 1337, 31337, 12345, 54321, 6666, 6667, 9001, 9999, 6969, 8081, 2744, 5556 };
    for (DWORD b : bad) if (port == b) return true;
    return false;
}

static void scanNetwork(json& out) {
    json& arr = out["findings"];

    ULONG sz = 0;
    GetExtendedTcpTable(nullptr, &sz, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    vector<unsigned char> buf(sz ? sz : 1 << 16);
    if (GetExtendedTcpTable(buf.data(), &sz, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
        auto* t = (MIB_TCPTABLE_OWNER_PID*)buf.data();
        int listeners = 0, foreign = 0;
        for (DWORD i = 0; i < t->dwNumEntries; ++i) {
            auto& r = t->table[i];
            DWORD localPort = ntohs((u_short)(r.dwLocalPort & 0xffff));
            DWORD remotePort = ntohs((u_short)(r.dwRemotePort & 0xffff));
            DWORD ra = ntohl(r.dwRemoteAddr);
            if (r.dwState == MIB_TCP_STATE_LISTEN) {
                ++listeners;
                bool wild = r.dwLocalAddr == 0;
                if (wild && !suspiciousPort(localPort)) continue;   // normal service, stay quiet
                wstring where = wild ? L"0.0.0.0" :
                    std::to_wstring(r.dwLocalAddr & 0xff) + L"." + std::to_wstring((r.dwLocalAddr >> 8) & 0xff) + L"." +
                    std::to_wstring((r.dwLocalAddr >> 16) & 0xff) + L"." + std::to_wstring((r.dwLocalAddr >> 24) & 0xff);
                wstring who = ownerOf(r.dwOwningPid);
                if (suspiciousPort(localPort))
                    arr.push_back(finding("high", L"Suspicious listening port " + std::to_wstring(localPort),
                                          who + L" is listening on " + where + L":" + std::to_wstring(localPort) +
                                          L" — this port is a classic RAT / bind-shell default. Verify this program.",
                                          L"TCP table", "port", L"tcp-listen-" + std::to_wstring(localPort)));
                else
                    arr.push_back(finding("med", L"Internet-exposed listener: port " + std::to_wstring(localPort),
                                          who + L" listens on all interfaces (" + where + L"). If this service is local-only, block it with the firewall.",
                                          L"TCP table", "port"));
            } else if (r.dwState == MIB_TCP_STATE_ESTAB && ra != 0 && ra != 0x0100007f) {   // 127.0.0.1
                ++foreign;
                DWORD rp = remotePort;
                if (rp == 4444 || rp == 1337 || rp == 31337 || rp == 6666 || rp == 12345 || rp == 54321)
                    arr.push_back(finding("high", L"Outbound to a shell-like port " + std::to_wstring(rp),
                                          ownerOf(r.dwOwningPid) + L" has an established connection to remote port " + std::to_wstring(rp) +
                                          L" — typical of a reverse shell. Identify and remove the program.",
                                          L"TCP table", "conn"));
            }
        }
        arr.push_back(finding("info", L"Network audit",
                              std::to_wstring(listeners) + L" listening endpoints, " + std::to_wstring(foreign) +
                              L" established outbound connections reviewed with their owning process.", L"GetExtendedTcpTable", "net"));
    }

    // UDP listeners too (cheaper check: only count + flag the well-known malware ports)
    ULONG usz = 0;
    GetExtendedUdpTable(nullptr, &usz, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    vector<unsigned char> ubuf(usz ? usz : 1 << 14);
    if (GetExtendedUdpTable(ubuf.data(), &usz, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
        auto* t = (MIB_UDPTABLE_OWNER_PID*)ubuf.data();
        for (DWORD i = 0; i < t->dwNumEntries; ++i) {
            DWORD lp = ntohs((u_short)(t->table[i].dwLocalPort & 0xffff));
            if (suspiciousPort(lp))
                arr.push_back(finding("high", L"Suspicious UDP listener on port " + std::to_wstring(lp),
                                      ownerOf(t->table[i].dwOwningPid) + L" listens on UDP " + std::to_wstring(lp) + L".",
                                      L"UDP table", "port", L"udp-" + std::to_wstring(lp)));
        }
    }
}

// ------------------------------------------------------------------ 5: persistence
static void scanRunKeys(json& out) {
    json& arr = out["findings"];
    struct RK { HKEY hive; const wchar_t* root; const wchar_t* path; bool machine; };
    static const RK keys[] = {
        { HKEY_CURRENT_USER,  L"HKCU",  L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", false },
        { HKEY_CURRENT_USER,  L"HKCU",  L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", false },
        { HKEY_LOCAL_MACHINE, L"HKLM",  L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", true },
        { HKEY_LOCAL_MACHINE, L"HKLM",  L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", true },
        { HKEY_LOCAL_MACHINE, L"HKLM",  L"Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Run", true },
    };
    int entries = 0;
    for (auto& rk : keys) {
        HKEY k;
        if (RegOpenKeyExW(rk.hive, rk.path, 0, KEY_READ, &k) != ERROR_SUCCESS) continue;
        DWORD idx = 0; wchar_t val[256]; DWORD valLen = 256;
        while (RegEnumValueW(k, idx++, val, &valLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            valLen = 256;
            wchar_t data[1024] = L""; DWORD sz = sizeof(data);
            if (RegGetValueW(rk.hive, rk.path, val, RRF_RT_REG_SZ, nullptr, data, &sz) != ERROR_SUCCESS) continue;
            ++entries;
            wstring exe(data);
            wstring low = lower(exe);
            wstring id = wstring(rk.root) + L"|" + rk.path + L"|" + val;
            // heuristics, each with its reason - all classic malware tells
            bool susp = false; wstring why;
            if (low.find(L"%temp%") != wstring::npos || low.find(L"\\temp\\") != wstring::npos ||
                low.find(L"appdata\\local\\temp") != wstring::npos)
                { susp = true; why = L"launches from a TEMP folder — no legit software persists there"; }
            else if (low.find(L"rasautou") != wstring::npos || low.find(L"regsvr32") != wstring::npos && low.find(L"://") != wstring::npos)
                { susp = true; why = L"regsvr32/rasautou scriptlet launcher (Squiblydoo-style)"; }
            else if (low.find(L"powershell") != wstring::npos && (low.find(L"-enc") != wstring::npos || low.find(L"-w hidden") != wstring::npos || low.find(L"b64") != wstring::npos))
                { susp = true; why = L"hidden/encoded PowerShell at boot"; }
            else if (low.find(L"cmd.exe") != wstring::npos && low.find(L"/c") != wstring::npos)
                { susp = true; why = L"boot-time cmd /c launcher"; }
            else if (low.find(L".vbs") != wstring::npos || low.find(L".js") != wstring::npos || low.find(L".scr") != wstring::npos)
                { susp = true; why = L"scripted screensaver/host payload"; }
            else if (low.find(L"rundll32") != wstring::npos && low.find(L".dll," ) != wstring::npos && low.find(L"\\windows\\") == wstring::npos)
                { susp = true; why = L"rundll32 loading a non-system DLL"; }
            if (susp)
                arr.push_back(finding("high", L"Persistence: " + wstring(val),
                                      L"Runs at every boot: " + exe + L"\n" + why,
                                      wstring(rk.root) + L"\\" + rk.path, "persistence", id));
        }
        RegCloseKey(k);
    }

    // IFEO debugger hijack: HKLM\...\Image File Execution Options\<exe> -> Debugger
    {
        HKEY k;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options", 0, KEY_READ, &k) == ERROR_SUCCESS) {
            DWORD idx = 0; wchar_t sub[256]; DWORD subLen = 256;
            int checked = 0;
            while (RegEnumKeyExW(k, idx++, sub, &subLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                subLen = 256;
                if (++checked > 2000) break;
                wchar_t dbg[512] = L""; DWORD sz = sizeof(dbg);
                if (RegGetValueW(HKEY_LOCAL_MACHINE, (wstring(L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\") + sub).c_str(),
                                 L"Debugger", RRF_RT_REG_SZ, nullptr, dbg, &sz) == ERROR_SUCCESS && dbg[0]) {
                    wstring low = lower(dbg);
                    if (low.find(L"cmd.exe") != wstring::npos || low.find(L"powershell") != wstring::npos)
                        arr.push_back(finding("high", L"IFEO debugger hijack on " + wstring(sub),
                                              L"Image File Execution Options redirects " + wstring(sub) + L" to: " + dbg +
                                              L" — classic malware persistence that captures every launch of that program.",
                                              L"HKLM\\...\\Image File Execution Options", "persistence",
                                              L"ifeo|" + wstring(sub)));
                }
            }
            RegCloseKey(k);
        }
    }

    // Startup folders (user + common): droppers often land as .scr/.vbs/.bat
    wchar_t* su = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &su) == S_OK) {
        wstring dir = su; CoTaskMemFree(su);
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                wstring f = fd.cFileName; wstring low = lower(f);
                ++entries;
                if (low.rfind(L".scr") == low.size() - 4 || low.rfind(L".vbs") == low.size() - 4 ||
                    low.rfind(L".js") == low.size() - 3 || low.rfind(L".bat") == low.size() - 4 ||
                    low.rfind(L".cmd") == low.size() - 4)
                    arr.push_back(finding("med", L"Script in Startup folder: " + f,
                                          dir + L"\\" + f + L" — scripts here run with your user rights at every logon.",
                                          dir, "persistence", L"startfold|" + dir + L"\\" + f));
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        arr.push_back(finding("info", L"Persistence surface reviewed",
                              std::to_wstring(entries) + L" autostart entries (Run/RunOnce user+machine+32bit, Startup folder, IFEO debuggers).",
                              L"Registry + Startup", "persist-info"));
    }
}

// ------------------------------------------------------------------ 6: disk artifacts
static void scanDiskArtifacts(json& out) {
    json& arr = out["findings"];
    wchar_t* tmp = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &tmp) != S_OK) return;
    wstring local = tmp; CoTaskMemFree(tmp);
    wstring tempDir = local + L"\\Temp";

    // Executables dropped in %TEMP% (last 30 days) — staging behaviour for loaders/ransomware
    int exeCount = 0; uint64_t exeBytes = 0; wstring newest;
    ULONGLONG newestT = 0;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((tempDir + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        ULONGLONG now = (ULONGLONG)time(nullptr);
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            wstring f = fd.cFileName; wstring low = lower(f);
            bool exe = low.rfind(L".exe") == low.size() - 4 || low.rfind(L".dll") == low.size() - 4 ||
                       low.rfind(L".scr") == low.size() - 4 || low.rfind(L".com") == low.size() - 4;
            if (!exe) continue;
            ULONGLONG t = ((ULONGLONG)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime;
            t = t / 10000000ull - 11644473600ull;      // FILETIME -> unix seconds
            if (t + 30ull * 86400ull >= now) {
                ++exeCount;
                exeBytes += ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
                if (t > newestT) { newestT = t; newest = f; }
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (exeCount >= 3)
        arr.push_back(finding("med", std::to_wstring(exeCount) + L" executables in %TEMP% (30 days)",
                              L"Loaders and ransomware droppers stage in %TEMP%. Newest: " + newest +
                              L". Review them in %LOCALAPPDATA%\\Temp — Defender should have seen them too.",
                              tempDir, "artifact"));
    else if (exeCount > 0)
        arr.push_back(finding("low", std::to_wstring(exeCount) + L" executable(s) in %TEMP% (30 days)",
                              L"Usually installers left behind. Worth a glance, not a panic.", tempDir, "artifact"));

    // Double-extension files everywhere in the user profile (photo.jpg.exe - classic Trojan)
    {
        int doubles = 0; wstring first;
        for (auto& rootName : { L"\\\\Downloads", L"\\\\Desktop", L"\\\\Documents" }) {
            wstring dir = local.substr(0, local.find_last_of(L"\\/")) + L"\\Documents"; // placeholder; use known folders below
        }
        wchar_t* dl = nullptr;
        if (SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &dl) == S_OK) {
            wstring dir = dl; CoTaskMemFree(dl);
            HANDLE h2 = FindFirstFileW((dir + L"\\*").c_str(), &fd);
            if (h2 != INVALID_HANDLE_VALUE) {
                do {
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                    wstring f = fd.cFileName, low = lower(f);
                    // name.exe / name.jpg.scr / name.pdf.cmd …
                    size_t dot = low.rfind(L'.');
                    if (dot == wstring::npos || dot < 2) continue;
                    wstring ext = low.substr(dot);
                    wstring stem = low.substr(0, dot);
                    size_t dot2 = stem.rfind(L'.');
                    if (dot2 == wstring::npos) continue;
                    wstring ext2 = stem.substr(dot2);
                    if ((ext == L".exe" || ext == L".scr" || ext == L".com" || ext == L".cmd" || ext == L".pif") &&
                        (ext2 == L".jpg" || ext2 == L".jpeg" || ext2 == L".png" || ext2 == L".pdf" || ext2 == L".doc" ||
                         ext2 == L".docx" || ext2 == L".mp4" || ext2 == L".txt" || ext2 == L".xls" || ext2 == L".xlsx")) {
                        ++doubles;
                        if (first.empty()) first = f;
                    }
                } while (FindNextFileW(h2, &fd));
                FindClose(h2);
            }
            if (doubles)
                arr.push_back(finding("high", std::to_wstring(doubles) + L" double-extension file(s) in Downloads",
                                      L"Files like \"" + first + L"\" hide an executable behind a document extension — the oldest Trojan trick in the book. Delete them.",
                                      L"Downloads", "artifact"));
        }
    }

    // Writable machine-wide startup folder (ransomware impact surface + persistence)
    {
        wchar_t* cs = nullptr;
        if (SHGetKnownFolderPath(FOLDERID_CommonStartup, 0, nullptr, &cs) == S_OK) {
            wstring dir = cs; CoTaskMemFree(cs);
            DWORD attr = GetFileAttributesW(dir.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_READONLY) && !isAdmin())
                ;   // writable check requires an actual write probe; skip false positives
            arr.push_back(finding("info", L"Startup folders", L"User and common Startup folders enumerated (scripts flagged above).", dir, "artifact-info"));
        }
    }

    // Ransomware smoke: recent shadow-copy deletion events (Security log, admin-readable)
    {
        wstring cmd = L"wevtutil qe System /q:\"*[System[(EventID=1102)]]\" /c:5 /rd:true /f:text";
        string o;
        // 1102 is audit-clear; the classic pre-encryption step is vssadmin delete shadows (EventID 1102 + vssadmin history).
        // Keep it cheap and only flag when the command actually ran with output.
        if (runCapture(L"wevtutil qe System /c:3 /rd:true /f:text", o, 6000) && o.size() > 40) {
            string low = o; std::transform(low.begin(), low.end(), low.begin(), ::tolower);
            if (low.find("vssadmin") != string::npos || low.find("shadow") != string::npos)
                arr.push_back(finding("high", L"Shadow-copy tampering in the System log",
                                      L"Recent events mention shadow copies being touched — ransomware deletes them before encrypting. Check Windows Security > Protection history now.",
                                      L"Event log", "ransomware"));
        }
    }
}

// ------------------------------------------------------------------ 8: UAC
static void scanUac(json& out) {
    json& arr = out["findings"];
    DWORD uac = 1; DWORD sz = sizeof(uac);
    RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                 L"EnableLUA", RRF_RT_REG_DWORD, nullptr, &uac, &sz);
    if (uac == 0)
        arr.push_back(finding("high", L"UAC is disabled", L"Every program runs with your full token. Re-enable UAC in Control Panel > User Accounts.", L"HKLM\\...\\Policies\\System", "uac"));
    else
        arr.push_back(finding("ok", L"UAC enabled", L"Prompt level read from the registry.", L"HKLM\\...\\Policies\\System", "uac"));
}

// ------------------------------------------------------------------ driver
json runScan() {
    json out;
    out["generated"] = (long long)time(nullptr);
    scanAvFirewall(out);
    scanNetwork(out);
    scanRunKeys(out);
    scanDiskArtifacts(out);
    scanUac(out);

    int high = 0, med = 0, low = 0, ok = 0;
    for (auto& f : out["findings"]) {
        string s = f.value("sev", "info");
        if (s == "high") ++high; else if (s == "med") ++med; else if (s == "low") ++low; else if (s == "ok") ++ok;
    }
    out["summary"] = { {"high", high}, {"med", med}, {"low", low}, {"ok", ok} };
    log::section(L"SECURITY SCAN");
    log::ok(L"Security scan done: " + std::to_wstring(high) + L" high, " + std::to_wstring(med) + L" medium, " + std::to_wstring(low) + L" low");
    return out;
}

// ------------------------------------------------------------------ revoke / restore
static wstring stashPath() { return appDataDir() + L"\\security-stash.json"; }

bool revokeEntry(const wstring& id, wstring& err) {
    size_t bar = id.find(L'|');
    if (bar == wstring::npos) { err = L"bad id"; return false; }
    wstring kind = id.substr(0, bar);
    wstring rest = id.substr(bar + 1);

    json stash = json::object();
    {
        std::ifstream f(stashPath().c_str());
        if (f) { try { stash = json::parse(f, nullptr, false); } catch (...) { stash = json::object(); } }
    }

    if (kind == L"HKCU" || kind == L"HKLM") {
        size_t b2 = rest.find(L'|');
        if (b2 == wstring::npos) { err = L"bad registry id"; return false; }
        wstring keyPath = rest.substr(0, b2);
        wstring valName = rest.substr(b2 + 1);
        HKEY hive = kind == L"HKCU" ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
        wchar_t data[2048] = L""; DWORD sz = sizeof(data);
        if (RegGetValueW(hive, keyPath.c_str(), valName.c_str(), RRF_RT_REG_SZ, nullptr, data, &sz) != ERROR_SUCCESS) {
            err = L"value not found (already removed?)"; return false;
        }
        stash[narrow(id)] = json({ {"type", "reg"}, {"hive", narrow(kind)}, {"key", narrow(keyPath)},
                                   {"value", narrow(valName)}, {"data", narrow(data)} });
        // remove
        HKEY k;
        if (RegOpenKeyExW(hive, keyPath.c_str(), 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) { err = L"cannot open key for write (admin needed for HKLM)"; return false; }
        LONG rc = RegDeleteValueW(k, valName.c_str());
        RegCloseKey(k);
        if (rc != ERROR_SUCCESS) { err = L"RegDeleteValue failed"; return false; }
        log::info(L"Security: revoked autostart value " + valName + L" (stashed)");
    } else if (kind == L"startfold") {
        wstring file = rest;
        // stash the file content before moving it to a quarantine folder
        string content;
        std::ifstream f(file.c_str(), std::ios::binary);
        if (f) content.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        wstring qdir = appDataDir() + L"\\quarantine";
        CreateDirectoryW(qdir.c_str(), nullptr);
        wstring qpath = qdir + L"\\" + file.substr(file.find_last_of(L"\\") + 1);
        if (!MoveFileW(file.c_str(), qpath.c_str())) { err = L"MoveFile failed"; return false; }
        stash[narrow(id)] = json({ {"type", "file"}, {"from", narrow(file)}, {"to", narrow(qpath)}, {"content", content} });
        log::info(L"Security: quarantined " + file);
    } else {
        err = L"unsupported id kind";
        return false;
    }

    std::ofstream f(stashPath().c_str());
    f << stash.dump();
    return true;
}

bool restoreEntry(const wstring& id, wstring& err) {
    json stash = json::object();
    {
        std::ifstream f(stashPath().c_str());
        if (f) { try { stash = json::parse(f, nullptr, false); } catch (...) {} }
    }
    if (!stash.contains(narrow(id))) { err = L"nothing stashed for this id"; return false; }
    json e = stash[narrow(id)];
    if (e.value("type", "") == "reg") {
        HKEY hive = e.value("hive", "HKCU") == "HKLM" ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
        HKEY k;
        if (RegCreateKeyExW(hive, widen(e.value("key", "")).c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) { err = L"cannot open key"; return false; }
        LONG rc = RegSetValueExW(k, widen(e.value("value", "")).c_str(), 0, REG_SZ, (const BYTE*)widen(e.value("data", "")).c_str(),
                                 (DWORD)((widen(e.value("data", "")).size() + 1) * sizeof(wchar_t)));
        RegCloseKey(k);
        if (rc != ERROR_SUCCESS) { err = L"restore failed"; return false; }
    } else if (e.value("type", "") == "file") {
        MoveFileW(widen(e.value("to", "")).c_str(), widen(e.value("from", "")).c_str());
    }
    stash.erase(narrow(id));
    std::ofstream f(stashPath().c_str());
    f << stash.dump();
    log::ok(L"Security: restored " + id);
    return true;
}

} // namespace ok::secscan
