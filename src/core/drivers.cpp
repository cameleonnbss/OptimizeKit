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
    HDEVINFO set = SetupDiGetClassDevsW(&GUID_DEVCLASS_DISPLAY, nullptr, nullptr, DIGCF_PRESENT);
    if (set != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA dd{}; dd.cbSize = sizeof(dd);
        if (SetupDiEnumDeviceInfo(set, 0, &dd)) {
            auto getProp = [&](DWORD prop) -> wstring {
                wchar_t b[512] = {};
                DWORD sz = sizeof(b), type = 0;
                if (SetupDiGetDeviceRegistryPropertyW(set, &dd, prop, &type, (PBYTE)b, sz, nullptr))
                    return b;
                return L"";
            };
            inf.gpuName       = getProp(SPDRP_DEVICEDESC);
            wstring drvKey    = getProp(SPDRP_DRIVER);   // e.g. "Class\{4d36e968-...}\0000"
            if (!drvKey.empty()) {
                wstring full = L"SYSTEM\\CurrentControlSet\\" + drvKey;
                HKEY k;
                if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, full.c_str(), 0, KEY_READ, &k) == ERROR_SUCCESS) {
                    wchar_t b[128] = {}; DWORD sz = sizeof(b);
                    if (RegQueryValueExW(k, L"DriverVersion", nullptr, nullptr, (LPBYTE)b, &sz) == ERROR_SUCCESS)
                        inf.driverVersion = b;
                    sz = sizeof(b);
                    if (RegQueryValueExW(k, L"DriverDate", nullptr, nullptr, (LPBYTE)b, &sz) == ERROR_SUCCESS)
                        inf.driverDate = b;
                    RegCloseKey(k);
                }
            }
        }
        SetupDiDestroyDeviceInfoList(set);
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
