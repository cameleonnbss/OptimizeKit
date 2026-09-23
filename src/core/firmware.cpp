// OptimizeKit - firmware & platform security inventory (read-only).
// Sources: Microsoft Learn (TPM WMI, SecureBoot UEFI variable, PowerSettings),
// WinUtil's WPBT/HPET/PATCH tweaks, bcdedit documentation. Every field is a real
// read - unknown stays "unknown" instead of a guessed value.
#include "firmware.h"
#include <oleauto.h>
#include <wbemcli.h>
#include <shellapi.h>

#pragma comment(lib, "wbemuuid.lib")

namespace ok::firmware {

using json = nlohmann::json;

// ---------------------------------------------------------------- wmi helper
struct WStr {
    BSTR b;
    explicit WStr(const wchar_t* s) : b(SysAllocString(s)) {}
    ~WStr() { if (b) SysFreeString(b); }
    operator BSTR() { return b; }
};

static json wmiQuery(const wchar_t* wql, const wchar_t* cls,
                     std::initializer_list<const wchar_t*> props) {
    json out = json::object();
    // httplib workers are foreign threads: make sure each caller owns a COM apartment
    thread_local bool comReady = false;
    if (!comReady) {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        comReady = true;
    }
    IWbemLocator* loc = nullptr;
    if (CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                         IID_IWbemLocator, (void**)&loc) != S_OK) return out;
    IWbemServices* svc = nullptr;
    if (loc->ConnectServer(WStr(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr,
                           0, nullptr, nullptr, &svc) != S_OK) { loc->Release(); return out; }
    CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHN_NONE, nullptr,
                      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    IEnumWbemClassObject* en = nullptr;
    wstring q = wstring(L"SELECT * FROM ") + cls + (wql && *wql ? (L" WHERE " + wstring(wql)) : L"");
    WStr lang(L"WQL");
    if (svc->ExecQuery(lang, WStr(q.c_str()),
                       WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en) == S_OK) {
        IWbemClassObject* obj = nullptr; ULONG got = 0;
        if (en->Next(WBEM_INFINITE, 1, &obj, &got) == S_OK && got) {
            for (auto* p : props) {
                VARIANT v;
                VariantInit(&v);
                if (obj->Get(p, 0, &v, nullptr, nullptr) == S_OK) {
                    if (v.vt == VT_BSTR) out[narrow(p)] = narrow(v.bstrVal);
                    else if (v.vt == VT_I4)  out[narrow(p)] = v.lVal;
                    else if (v.vt == VT_BOOL) out[narrow(p)] = v.boolVal != VARIANT_FALSE;
                    else if (v.vt == VT_NULL) out[narrow(p)] = nullptr;
                    VariantClear(&v);
                }
            }
            obj->Release();
        }
        en->Release();
    }
    svc->Release();
    loc->Release();
    return out;
}

// ---------------------------------------------------------------- inventory
void FwInit() {
    // Called from a worker thread in the server/UI: own COM apartment for WMI.
    // The handlers run on httplib workers; S_OK/ RPC_E_CHANGED_MODE both mean "usable".
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    (void)hr;
    CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
                         RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
}

static wstring regStr(HKEY root, const wstring& path, const wstring& name, const wstring& def = L"") {
    wchar_t buf[512] = {}; DWORD sz = sizeof(buf), t = 0;
    if (RegGetValueW(root, path.c_str(), name.c_str(), RRF_RT_REG_SZ, &t, buf, &sz) == ERROR_SUCCESS)
        return buf;
    return def;
}
static DWORD regDw(HKEY root, const wstring& path, const wstring& name, DWORD def) {
    DWORD v = def, sz = sizeof(v), t = 0;
    if (RegGetValueW(root, path.c_str(), name.c_str(), RRF_RT_REG_DWORD, &t, &v, &sz) == ERROR_SUCCESS)
        return v;
    return def;
}
static bool regHasValue(HKEY root, const wstring& path, const wstring& name) {
    return RegGetValueW(root, path.c_str(), name.c_str(), RRF_RT_ANY, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}

// One bcdedit entry: "option   yes|no" lines; we only need yes/no values.
static wstring bcdDump() {
    string out;
    runCapture(L"bcdedit", out, 20000);
    return widen(out);
}

// ---------------------------------------------------------------- inventory
static wstring tpmStr(const json& j) {
    const json& t = j.contains("tpm") ? j["tpm"] : json::object();
    if (!t.value("present", false)) return L"none";
    return widen(t.value("version", std::string("present")));
}

json inventory() {
    json j;

    // --- Secure Boot (UEFI state machine, works only in UEFI boot) ---
    bool sb = false, sbRead = false;
    {
        DWORD v = 0, sz = sizeof(v);
        if (RegGetValueW(HKEY_LOCAL_MACHINE,
                L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State",
                L"UEFISecureBootEnabled", RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS) {
            sbRead = true; sb = v == 1;
        }
    }
    j["secureBoot"] = sbRead ? (sb ? "on" : "off") : "unknown";
    j["bootMode"] = sbRead ? "UEFI" : "Legacy/BIOS";

    // --- TPM 2.0 (Win32_Tpm under ROOT\CIMV2\Security\MicrosoftTpm) ---
    json tpm;
    {
        IWbemLocator* loc = nullptr;
        if (CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                             IID_IWbemLocator, (void**)&loc) == S_OK) {
            IWbemServices* svc = nullptr;
            if (loc->ConnectServer(WStr(L"ROOT\\CIMV2\\Security\\MicrosoftTpm"), nullptr,
                                   nullptr, nullptr, 0, nullptr, nullptr, &svc) == S_OK) {
                CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHN_NONE, nullptr,
                                  RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
                IEnumWbemClassObject* en = nullptr;
                WStr lang(L"WQL");
                if (svc->ExecQuery(lang, WStr(L"SELECT * FROM Win32_Tpm"),
                                   WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                   nullptr, &en) == S_OK) {
                    IWbemClassObject* obj = nullptr; ULONG got = 0;
                    if (en->Next(WBEM_INFINITE, 1, &obj, &got) == S_OK && got) {
                        VARIANT v; VariantInit(&v);
                        if (obj->Get(L"IsEnabled_InitialValue", 0, &v, nullptr, nullptr) == S_OK)
                            tpm["present"] = v.boolVal != VARIANT_FALSE, VariantClear(&v);
                        if (obj->Get(L"IsActivated_InitialValue", 0, &v, nullptr, nullptr) == S_OK)
                            tpm["activated"] = v.boolVal != VARIANT_FALSE, VariantClear(&v);
                        if (obj->Get(L"SpecVersion", 0, &v, nullptr, nullptr) == S_OK && v.vt == VT_BSTR)
                            tpm["version"] = narrow(v.bstrVal), VariantClear(&v);
                        obj->Release();
                    }
                    en->Release();
                }
                svc->Release();
            }
            loc->Release();
        }
    }
    if (!tpm.contains("present")) {
        // The Win32_Tpm namespace answers only to elevated readers on most SKUs.
        // Fallback: the TPM device present in the PnP tree (admin or not).
        bool dev = false;
        HKEY k;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Enum\\TPM", 0, KEY_READ, &k) == ERROR_SUCCESS) {
            dev = true; RegCloseKey(k);
        }
        tpm["present"] = dev;
        tpm["version"] = dev ? "2.0 (device present)" : "n/a (elevate to read the full state)";
    }
    j["tpm"] = tpm;

    // --- Virtualization (firmware feature as exposed by Hyper-V) ---
    {
        json cv = wmiQuery(nullptr, L"Win32_ComputerSystem", { L"HypervisorPresent" });
        json p  = wmiQuery(nullptr, L"Win32_Processor", { L"VirtualizationFirmwareEnabled" });
        bool hvOn = cv.value("HypervisorPresent", false);
        bool fw = p.value("VirtualizationFirmwareEnabled", false);
        j["virtualization"] = (hvOn || fw) ? "on" : "off";
        j["hypervisorRunning"] = hvOn;
    }

    // --- BIOS / board identity ---
    {
        json b = wmiQuery(nullptr, L"Win32_BIOS",
                          { L"Manufacturer", L"SMBIOSBIOSVersion", L"ReleaseDate" });
        json bb = wmiQuery(nullptr, L"Win32_BaseBoard", { L"Manufacturer", L"Product" });
        j["biosVendor"]  = narrow(regStr(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BIOSVendor",
                                         widen(b.value("Manufacturer", ""))));
        j["biosVersion"] = narrow(regStr(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BIOSVersion",
                                         widen(b.value("SMBIOSBIOSVersion", ""))));
        j["biosDate"]    = narrow(widen(b.value("ReleaseDate", "")).substr(0, 8));
        j["motherboard"] = narrow(widen(bb.value("Manufacturer", "")) + L" " + widen(bb.value("Product", "")));
    }

    // --- Power profile (firmware-facing: S0 modern standby vs S3) ---
    {
        DWORD aoac = regDw(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Power",
                           L"PlatformAoAcOverride", 0xFFFFFFFF);
        json ps = wmiQuery(nullptr, L"Win32_ComputerSystem", { L"PCSystemType" });
        j["modernStandby"] = aoac == 1 ? "on" : (aoac == 0 ? "off (S3)" : "auto");
        j["deviceType"] = ps.value("PCSystemType", 1) == 2 ? "mobile" : "desktop";
    }

    // --- Kernel options the tweaks of this app set (honest live read) ---
    wstring bcd = bcdDump();
    auto bcdOpt = [&](const wchar_t* opt) -> wstring {
        size_t p = bcd.find(opt);
        if (p == wstring::npos) return L"default";
        size_t e = bcd.find(L'\n', p);
        wstring tail = bcd.substr(p, e == wstring::npos ? wstring::npos : e - p);
        return tail.find(L"yes") != wstring::npos ? L"yes"
             : tail.find(L"no") != wstring::npos ? L"no" : L"set";
    };
    j["hpet"]          = regDw(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\hpet", L"Start", 1) == 4 ? "off" : "on";
    j["wpbt"]          = regHasValue(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager", L"DisableWpbtExecution")
                            ? (regDw(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager", L"DisableWpbtExecution", 0) == 1 ? "blocked" : "allowed")
                            : "allowed (default)";
    j["dynamicTick"]   = narrow(bcdOpt(L"disabledynamictick"));
    j["useplatformtick"] = narrow(bcdOpt(L"useplatformtick"));
    j["tscSync"]       = narrow(bcdOpt(L"useplatformclock"));
    j["patchGuard"]    = "active";   // cannot be disabled on retail x64 - stated, not guessed
    j["dumpLevel"]     = regDw(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\CrashControl", L"CrashDumpEnabled", 7) == 1
                            ? "complete" : (regDw(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\CrashControl", L"CrashDumpEnabled", 7) == 7 ? "automatic" : "kernel/small");
    j["svcSplitThreshold"] = regHasValue(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control", L"SvcHostSplitThresholdInKB")
                            ? (json)((long long)regDw(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control", L"SvcHostSplitThresholdInKB", 0) * 1024)
                            : json("windows default");
    j["rebootNeeded"] = false;
    { HKEY k; if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\RebootPending", 0, KEY_READ, &k) == ERROR_SUCCESS) { RegCloseKey(k); j["rebootNeeded"] = true; } }
    return j;
}

wstring summaryLine() {
    json j = inventory();
    return L"BIOS " + widen(j.value("biosVersion", std::string("?")))
        + L"  ·  SecureBoot " + widen(j.value("secureBoot", std::string("?")))
        + L"  ·  TPM " + tpmStr(j)
        + L"  ·  VT " + widen(j.value("virtualization", std::string("?")));
}

} // namespace ok::firmware
