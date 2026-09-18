#include "drivers.h"
#include "sysinfo.h"
#include <shellapi.h>
#include <setupapi.h>
#include <devguid.h>
#include <ctype.h>

#pragma comment(lib, "setupapi.lib")

namespace ok::drivers {

Info collect() {
    Info inf;
    // Enumerate the Display class instances directly from the registry
    // (0000, 0001, ...) — more reliable than setupapi for driver metadata.
    const wstring cls = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\";
    for (DWORD i = 0; i < 16; ++i) {
        wchar_t sub[16]; swprintf(sub, 16, L"%04llu", (unsigned long long)i);
        HKEY k;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, (cls + sub).c_str(), 0, KEY_READ, &k) != ERROR_SUCCESS) break;
        wchar_t desc[256] = {}; DWORD sz = sizeof(desc);
        if (RegQueryValueExW(k, L"DriverDesc", nullptr, nullptr, (LPBYTE)desc, &sz) == ERROR_SUCCESS && desc[0]) {
            inf.gpuName = desc;
            sz = sizeof(desc);
            if (RegQueryValueExW(k, L"DriverVersion", nullptr, nullptr, (LPBYTE)desc, &sz) == ERROR_SUCCESS)
                inf.driverVersion = desc;
            wchar_t date[64] = {}; sz = sizeof(date);
            if (RegQueryValueExW(k, L"DriverDate", nullptr, nullptr, (LPBYTE)date, &sz) == ERROR_SUCCESS)
                inf.driverDate = date;
            RegCloseKey(k);
            break;
        }
        RegCloseKey(k);
    }
    return inf;
}

static wstring lower(wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }

void openVendorPage() {
    wstring g = lower(sysinfo::collect().gpuName);
    if (g.find(L"nvidia") != wstring::npos || g.find(L"geforce") != wstring::npos
        || g.find(L"rtx") != wstring::npos || g.find(L"gtx") != wstring::npos)
        shellOpen(L"https://www.nvidia.com/Download/index.aspx");
    else if (g.find(L"amd") != wstring::npos || g.find(L"radeon") != wstring::npos)
        shellOpen(L"https://www.amd.com/en/support");
    else if (g.find(L"intel") != wstring::npos || g.find(L"iris") != wstring::npos
        || g.find(L"arc") != wstring::npos || g.find(L"uhd") != wstring::npos)
        shellOpen(L"https://www.intel.com/content/www/us/en/download-center/home.html");
    else
        shellOpen(L"https://www.google.com/search?q=" + sysinfo::collect().gpuName + L"+driver+download");
}

wstring nvidiaSmiVersion() {
    string out;
    if (runCapture(L"nvidia-smi --query-gpu=driver_version --format=csv,noheader", out, 15000)) {
        wstring v = widen(out);
        while (!v.empty() && (v.back() == L'\n' || v.back() == L'\r' || v.back() == L' ')) v.pop_back();
        return v;
    }
    return L"";
}

void openDxdiag() {
    ShellExecuteW(nullptr, L"open", L"dxdiag.exe", nullptr, nullptr, SW_SHOWNORMAL);
}

void scanWindowsUpdateDrivers() {
    string out;
    runCapture(L"UsoClient StartInteractiveScan", out, 20000);
    log::ok(L"Windows Update driver scan requested");
}

} // namespace ok::drivers
