#include "sysinfo.h"
#include <psapi.h>
#include <dxgi.h>
#include <powrprof.h>
#include <tuple>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "powrprof.lib")

namespace ok::sysinfo {

static wstring regStr(HKEY root, const wstring& sub, const wstring& value) {
    wchar_t buf[512]; DWORD size = sizeof(buf); DWORD type = 0;
    if (RegGetValueW(root, sub.c_str(), value.c_str(), RRF_RT_REG_SZ, &type, buf, &size) == ERROR_SUCCESS)
        return buf;
    return L"";
}
static bool regBool(HKEY root, const wstring& sub, const wstring& value) {
    DWORD d = 0, size = sizeof(d);
    if (RegGetValueW(root, sub.c_str(), value.c_str(), RRF_RT_REG_DWORD, nullptr, &d, &size) == ERROR_SUCCESS)
        return d != 0;
    return false;
}

wstring activePowerPlanName() {
    GUID* scheme = nullptr;
    if (PowerGetActiveScheme(nullptr, &scheme) == ERROR_SUCCESS && scheme) {
        wchar_t name[128] = {};
        DWORD cb = sizeof(name);
        if (PowerReadFriendlyName(nullptr, scheme, nullptr, nullptr, (UCHAR*)name, &cb) == ERROR_SUCCESS) {
            LocalFree(scheme);
            return name;
        }
        LocalFree(scheme);
    }
    return L"Unknown";
}

bool queryGameMode() {
    return regBool(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"AutoGameModeEnabled");
}
bool queryHags() {
    return regBool(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers", L"HwSchMode");
}

Info collect() {
    Info i;
    i.elevated = ok::isAdmin();
    i.userName = ok::userName();

    wchar_t cn[256]; DWORD n = 256;
    GetComputerNameW(cn, &n);
    i.computerName = cn;

    OSVERSIONINFOW vi{}; vi.dwOSVersionInfoSize = sizeof(vi);
    // RtlGetVersion bypasses the manifest shims
    using fnT = LONG(WINAPI*)(OSVERSIONINFOW*);
    fnT rtl = (fnT)(void*)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    if (rtl) rtl(&vi);
    i.osVersion = L"Windows " + std::to_wstring(vi.dwMajorVersion) + L"." + std::to_wstring(vi.dwMinorVersion);
    i.osBuild   = std::to_wstring(vi.dwBuildNumber);
    i.osName    = (vi.dwBuildNumber >= 22000) ? L"Windows 11" : L"Windows 10";
    i.osArch    = ok::is64bit() ? L"64-bit" : L"32-bit";

    // CPU
    {
        HKEY k;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &k) == ERROR_SUCCESS) {
            wchar_t buf[256]; DWORD sz = sizeof(buf);
            if (RegQueryValueExW(k, L"ProcessorNameString", nullptr, nullptr, (LPBYTE)buf, &sz) == ERROR_SUCCESS)
                i.cpuName = buf;
            RegCloseKey(k);
        }
        SYSTEM_INFO si{};
        GetNativeSystemInfo(&si);
        i.cpuCores = si.dwNumberOfProcessors;
    }

    // RAM
    {
        MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);
        i.ramTotal = ms.ullTotalPhys;
        i.ramAvail = ms.ullAvailPhys;
    }

    // GPU via DXGI
    {
        HRESULT (WINAPI *pCreate)(const IID&, void**);
        HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
        if (dxgi) {
            pCreate = (HRESULT (WINAPI*)(const IID&, void**))GetProcAddress(dxgi, "CreateDXGIFactory1");
            if (pCreate) {
                IDXGIFactory1* factory = nullptr;
                IID factoryIID = { 0x770aae78, 0xf416, 0x4d03, {0xa4, 0x8c, 0x99, 0x9b, 0x99, 0x3e, 0x3a, 0xd5} }; // IDXGIFactory1
                IID adapterIID = { 0x29038f61, 0x3839, 0x4626, {0xb3, 0x13, 0x1a, 0xdc, 0x45, 0x79, 0x74, 0x33} }; // IDXGIAdapter1
                if (SUCCEEDED(pCreate(factoryIID, (void**)&factory)) && factory) {                        IDXGIAdapter1* adapter = nullptr;
                    if (factory->EnumAdapters1(0, &adapter) != DXGI_ERROR_NOT_FOUND && adapter) {
                        DXGI_ADAPTER_DESC1 desc{};
                        if (SUCCEEDED(adapter->GetDesc1(&desc))) {
                            i.gpuName = desc.Description;
                            HKEY k;
                            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                                L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\0000",
                                0, KEY_READ, &k) == ERROR_SUCCESS) {
                                wchar_t sbuf[128]; DWORD sz = sizeof(sbuf);
                                if (RegQueryValueExW(k, L"DriverVersion", nullptr, nullptr, (LPBYTE)sbuf, &sz) == ERROR_SUCCESS)
                                    i.gpuDriverStr = sbuf;
                                RegCloseKey(k);
                            }
                        }
                        adapter->Release();
                    }
                    factory->Release();
                }
            }
            FreeLibrary(dxgi);
        }
    }

    i.gameModeOn = queryGameMode();
    i.hagsOn     = queryHags();
    i.powerPlan  = activePowerPlanName();

    DWORD up = GetTickCount64() / 1000;
    i.uptime = fmtDuration(up);
    return i;
}

} // namespace ok::sysinfo
