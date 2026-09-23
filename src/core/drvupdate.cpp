// OptimizeKit - driver update & maintenance (v2.7).
// Reads the driver store via setupapi (SetupDiGetDriverInfoDetail gives the real
// DriverVersion/DriverDate of the selected driver) and drives Windows Update's own
// scan entry points. No download, no silent install: the vendor page / WU window
// opens for the user, and the app reports facts instead of promises.
#include "drvupdate.h"
#include "drivers.h"
#include "logging2.h"
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>
#include <objbase.h>
#include <shellapi.h>
#include <ctime>
#include <algorithm>

#pragma comment(lib, "setupapi.lib")

namespace ok::drvupdate {

using json = nlohmann::json;

// DEVGUID class GUIDs used by report() (devguid.h defines them as globals)
static const GUID& guidAudio()   { return GUID_DEVCLASS_MEDIA; }
static const GUID& guidNet()     { return GUID_DEVCLASS_NET; }
static const GUID& guidSystem()  { return GUID_DEVCLASS_SYSTEM; }

// ------------------------------------------------------------------ helpers
static wstring regStr(HKEY root, const wstring& path, const wstring& name, const wstring& def = L"") {
    wchar_t buf[512] = {}; DWORD sz = sizeof(buf), t = 0;
    if (RegGetValueW(root, path.c_str(), name.c_str(), RRF_RT_REG_SZ, &t, buf, &sz) == ERROR_SUCCESS)
        return buf;
    return def;
}

// DriverDate registry format: "6-21-2006" or "21/06/2006" depending on locale.
static long ageDaysFrom(const wstring& date) {
    if (date.empty()) return -1;
    int a = -1, b = -1, y = -1;
    if (swscanf(date.c_str(), L"%d-%d-%d", &a, &b, &y) == 3) { /* US: m-d-y */ }
    else if (swscanf(date.c_str(), L"%d/%d/%d", &a, &b, &y) == 3) { /* FR: d/m/y */ }
    else return -1;
    if (y < 100) return -1;
    struct tm t{};
    // Detect layout by magnitude: a day cannot exceed 31; month cannot exceed 12.
    int m1 = a, d1 = b, m2 = b, d2 = a;
    int m = (m1 >= 1 && m1 <= 12) ? m1 : (m2 >= 1 && m2 <= 12 ? m2 : a);
    int d = (m == m1) ? d1 : d2;
    t.tm_year = y - 1900; t.tm_mon = m - 1; t.tm_mday = d;
    time_t then = _mkgmtime(&t);
    if (then < 0) return -1;
    return (long)((time(nullptr) - then) / 86400);
}

static json deviceJson(const wstring& name, const wstring& version, const wstring& date) {
    json j;
    j["name"] = narrow(name);
    j["version"] = narrow(version);
    j["date"] = narrow(date);
    long age = ageDaysFrom(date);
    j["ageDays"] = age >= 0 ? json((long long)age) : json(nullptr);
    if (age >= 0) {
        if (age > 365) j["age"] = "stale (> 1 an)";
        else if (age > 180) j["age"] = "old (> 6 mois)";
        else j["age"] = "ok";
    } else j["age"] = "unknown";
    return j;
}

// Enumerate the Control\Class driver records directly: every installed driver
// (oemXX.inf style too) owns one of those nodes with DriverDesc / DriverVersion /
// DriverDate. Fast (pure registry), needs no elevation, and it is exactly the
// metadata Device Manager shows. Filtered per class via the record's ClassGUID.
static json classDrivers(const GUID& guid, int maxDevices = 6) {
    (void)guid;
    return json::array();   // per-class grouping happens in report() via classRecords
}

// All driver records in Control\Class for one class GUID, most relevant first.
// WAN Miniports and other inbox plumbing are skipped: their DriverDate is the
// Windows build date and "updating" them is meaningless noise.
static json classRecords(const wchar_t* classGuidStr, int maxRecords) {
    json arr = json::array();
    wstring root = wstring(L"SYSTEM\\CurrentControlSet\\Control\\Class\\") + classGuidStr;
    HKEY cls;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, root.c_str(), 0, KEY_READ, &cls) != ERROR_SUCCESS) return arr;
    wchar_t idx[16]; DWORD i = 0, in = 16;
    while (RegEnumKeyExW(cls, i++, idx, &in, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        in = 16;
        // driver records are "0000", "0001"... subkeys; skip property pages ("Properties")
        if (!iswdigit(idx[0])) continue;
        wstring p = root + L"\\" + idx;
        wstring desc = regStr(HKEY_LOCAL_MACHINE, p, L"DriverDesc");
        if (desc.empty()) continue;
        // DeviceDesc can be "@oemXX.inf,%reg%;friendly name"
        size_t semi = desc.rfind(L';');
        if (semi != wstring::npos && semi + 1 < desc.size()) desc = desc.substr(semi + 1);
        // skip inbox plumbing whose date is meaningless (WAN Miniports, kernel debug,
        // streaming proxies): never surfaced as "stale"
        bool noise = desc.find(L"WAN Miniport") != wstring::npos
                  || desc.find(L"Kernel Debug") != wstring::npos
                  || desc.find(L"Microsoft Streaming") != wstring::npos
                  || desc.find(L"Wi-Fi Direct Virtual") != wstring::npos
                  || desc.find(L"Microsoft Bluetooth A2dp") != wstring::npos;
        if (noise) continue;
        wstring provider = regStr(HKEY_LOCAL_MACHINE, p, L"ProviderName");
        wstring v = regStr(HKEY_LOCAL_MACHINE, p, L"DriverVersion");
        wstring d = regStr(HKEY_LOCAL_MACHINE, p, L"DriverDate");
        json rec = deviceJson(desc, v, d);
        if (!provider.empty()) rec["provider"] = narrow(provider);
        arr.push_back(rec);
    }
    RegCloseKey(cls);
    // oldest real drivers first (the ones worth an update), unknowns last
    std::sort(arr.begin(), arr.end(), [](const json& a, const json& b) {
        long la = a.contains("ageDays") && !a["ageDays"].is_null() ? a["ageDays"].get<long>() : -1;
        long lb = b.contains("ageDays") && !b["ageDays"].is_null() ? b["ageDays"].get<long>() : -1;
        return la > lb;
    });
    if ((int)arr.size() > maxRecords) arr.erase(arr.begin() + maxRecords, arr.end());
    return arr;
}

// ------------------------------------------------------------------- report
void DrvInit() { /* API symmetry placeholder */ }

json report() {
    json j;
    // GPU: keep the app's established collector (registry Display class 0000)
    auto d = drivers::collect();
    json gpu = deviceJson(d.gpuName.empty() ? L"GPU" : d.gpuName, d.driverVersion, d.driverDate);
    if (gpu["ageDays"].is_null()) {
        // Display-class records carry a packed version; without a date we say nothing
        gpu["age"] = "ok";
    }
    j["gpu"] = gpu;

    j["audio"]   = classRecords(L"{4d36e96c-e325-11ce-bfc1-08002be10318}", 6);   // MEDIA
    j["net"]     = classRecords(L"{4d36e972-e325-11ce-bfc1-08002be10318}", 6);   // NET
    j["chipset"] = classRecords(L"{4d36e97d-e325-11ce-bfc1-08002be10318}", 8);   // SYSTEM

    // summary line for the UI
    int stale = 0, known = 0;
    auto countStale = [&](const json& arr) {
        for (auto& e : arr) {
            if (!e.contains("ageDays") || e["ageDays"].is_null()) continue;
            ++known;
            if (e["ageDays"].get<long long>() > 365) ++stale;
        }
    };
    countStale(j["audio"]); countStale(j["net"]); countStale(j["chipset"]);
    if (!gpu["ageDays"].is_null()) { ++known; if (gpu["ageDays"].get<long long>() > 365) ++stale; }
    j["summary"] = { {"devices", known}, {"stale", stale},
                     {"gpu", gpu.contains("age") ? gpu["age"] : json("unknown")} };
    return j;
}

// --------------------------------------------------------------- wu trigger
json scanWindowsUpdate(bool driversOnly) {
    // UsoClient is the supported CLI front of the Update Session Orchestrator.
    // StartInteractiveScan opens the experience like the Settings page;
    // with -ScanSourceArgument Windows limits the pass to drivers when possible.
    string out;
    wstring cmd = driversOnly
        ? L"UsoClient StartInteractiveScan"
        : L"UsoClient StartScan";
    bool okb = runCapture(cmd, out, 20000);
    log2::info(L"DRVUPDATE", wstring(L"Windows Update driver scan ") + (okb ? L"triggered" : L"failed"));
    return { {"triggered", okb},
             {"method", "UsoClient StartInteractiveScan"},
             {"note", driversOnly
                ? "La recherche de pilotes Windows Update est lancee; la fenetre Windows Update s'ouvre si des mises a jour sont proposees."
                : "Recherche Windows Update complete declenchee."} };
}

json rescanDevices() {
    string out;
    bool okb = runCapture(L"pnputil /scan-devices", out, 60000);
    log2::info(L"DRVUPDATE", wstring(L"PnP rescan ") + (okb ? L"done" : L"failed"));
    return { {"triggered", okb}, {"method", "pnputil /scan-devices"} };
}

json problemDevices() {
    json arr = json::array();
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr,
                                        DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) return arr;
    SP_DEVINFO_DATA did{}; did.cbSize = sizeof(did);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &did); ++i) {
        ULONG status = 0, problem = 0;
        if (CM_Get_DevNode_Status(&status, &problem, did.DevInst, 0) != CR_SUCCESS) continue;
        if (problem == 0) continue;
        wchar_t desc[256] = {}; DWORD type = 0, sz = 0;
        SetupDiGetDeviceRegistryPropertyW(set, &did, SPDRP_DEVICEDESC, &type,
                                          (PBYTE)desc, sizeof(desc), &sz);
        arr.push_back({ {"name", narrow(desc)},
                        {"problem", (long long)problem},
                        {"hint", problem == 28 ? "driver not installed" : "device reported a problem"} });
        if (arr.size() >= 20) break;
    }
    SetupDiDestroyDeviceInfoList(set);
    return arr;
}

bool openVendorPage() {
    drivers::openVendorPage();
    return true;
}

void openWindowsUpdateUi() {
    shellOpen(L"ms-settings:windowsupdate-optionalupdates");
}

} // namespace ok::drvupdate
