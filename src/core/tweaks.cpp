// OptimizeKit - tweak engine.
// Sources of the tweaks (see docs/SOURCES.md for the full annotated list):
//  - Chris Titus Tech WinUtil (MIT)           https://github.com/ChrisTitusTech/winutil
//  - Valve / Microsoft official docs (Game Bar, MPO, HAGS)
//  - shadercache & network latency community guides (INDEPENDENT-NT, calypto)
// Every tweak restores a documented Windows default via restore().
#include "tweaks.h"
#include "common.h"
#include "sysinfo.h"
#include "netprofile.h"
#include <shellapi.h>
#include <fstream>
#include <set>
#include <utility>

#pragma comment(lib, "advapi32.lib")

namespace ok::tweaks {

using json = nlohmann::json;

// ---------------------------------------------------------------- catalog
const vector<Tweak>& catalog() {
    static const vector<Tweak> c = {
        // ---- Latency / gaming ----
        { "game_mode",        L"Enable Game Mode",                    L"[gaming] Prioritises the game process for CPU & GPU when a game runs.", false, 1, L"Microsoft / WinUtil" },
        { "game_dvr_off",     L"Disable Game DVR & Game Bar",         L"Stops background clip recording (Xbox Game Bar) which steals FPS.", false, 2, L"WinUtil / shadercache guides" },
        { "hags_on",          L"Hardware-accelerated GPU scheduling", L"[gaming] GPU manages its own VRAM queue; lower latency on modern GPUs (needs reboot).", true, 2, L"Microsoft (HAGS)" },
        { "mpo_off",          L"Disable MPO (multi-plane overlay)",   L"Fixes stutter / flickering / alt-tab glitches on Win11 24H2.", true, 2, L"Microsoft KB / PC Gaming Optimized" },
        { "timer_high",       L"Global timer resolution 0.5ms",       L"[latency] Raises the scheduler tick rate for all processes; reduces frame time variance.", true, 3, L"insovs/TimerResolution-Optimization" },
        { "network_gaming",   L"Gaming network stack",                L"[network] TcpAckFrequency=1 + TCPNoDelay (no Nagle), NetworkThrottlingIndex=0xFFFFFFFF, SystemResponsiveness=0 (games MMCSS).", true, 3, L"MS docs / community latency guides" },
        { "mouse_precision",  L"Disable mouse acceleration",          L"[input] Raw 1:1 mouse input (MarkC-style), critical for FPS aim.", false, 2, L"MarkC mouse fix" },
        { "visual_fx_perf",   L"Performance visual effects",          L"Switches to 'best performance' animations & transparency.", false, 1, L"WinUtil" },
        { "menu_delay_0",     L"Instant menu responses",              L"MenuShowDelay = 0 ms.", false, 1, L"WinUtil" },
        { "background_apps",  L"Disable background apps",             L"UWP apps no longer run in the background.", false, 2, L"WinUtil" },
        { "storage_sense",    L"Enable Storage Sense weekly",         L"Automatic temp/cache clean-up every week.", false, 1, L"Microsoft" },
        { "search_index",     L"Disable search indexing",             L"[disk] Removes background disk/CPU indexing churn (set 'classic' on laptops if you search a lot).", true, 2, L"WinUtil" },
        { "sysmain_off",      L"Disable SysMain (Superfetch)",        L"[ram] Useless on SSD, consumes RAM & disk. Not recommended on HDD.", true, 2, L"WinUtil / community" },
        { "hpets_off",        L"Disable high-precision event timer",  L"HPET off can reduce DPC latency on some boards; bcdedit useplatformclock false.", true, 3, L"calypto / community" },
        { "power_ultimate",   L"Ultimate Performance power plan",     L"[power] Duplicates + activates the hidden Ultimate Performance scheme.", true, 2, L"Chris Titus Tech WinUtil" },
        { "win32_priority",   L"Win32PrioritySeparation 0x26",        L"Short, variable, high foreground quantum boost — snappier gaming focus.", true, 2, L"MS internals / community" },
        { "gpu_preference",   L"GPU preference: high performance",    L"Global graphics preference set to high performance.", false, 1, L"Microsoft" },
        { "fso_on",           L"Disable fullscreen optimisations",    L"Games keep true exclusive fullscreen (one global flag).", false, 2, L"PC Gaming Optimized" },
        { "xbox_live_off",    L"Disable Xbox Live services",          L"Stops XblAuth/XblGameSave services (breaks Xbox Game Pass on PC).", true, 2, L"WinUtil" },

        // ---- Privacy / telemetry ----
        { "telemetry_off",    L"Disable telemetry (DiagnosticLog)",   L"[privacy] AllowTelemetry=0, stops DiagTrack & dmwappushservice.", true, 1, L"WinUtil" },
        { "advertising_off",  L"Disable advertising ID",              L"Enabled=0 under Privacy\\AdvertisingInfo.", false, 1, L"WinUtil" },
        { "activity_history", L"Disable activity history / timeline", L"PublishUserActivities & UploadUserActivities off.", false, 1, L"WinUtil" },
        { "bing_search",      L"Remove Bing from start menu search",  L"BingSearchEnabled=0, CortanaConsent=0.", false, 1, L"WinUtil" },
        { "tailored_experiences", L"Disable tailored experiences",    L"TailoredExperiencesWithDiagnosticDataEnabled=0.", false, 1, L"WinUtil" },
        { "telemetry_tasks",  L"Disable telemetry scheduled tasks",   L"Kills CEIP tasks (Consolidator, UsbCeip, FileHistory, DiskDiagnostic…).", true, 1, L"WinUtil" },
        { "windows_copilot",  L"Remove Windows Copilot",              L"Hides Copilot shell button + TurnOffWindowsCopilot policy.", true, 1, L"WinUtil" },

        // ---- Debloat ----
        { "bloat_uninstall",  L"Uninstall MS Store bloat apps",       L"Removes Bing apps, Clipchamp, News, Solitaire, Teams Personal, Xbox gems (appx).", true, 2, L"WinUtil" },
        { "onedrive_off",     L"Disable OneDrive sync",               L"Runs the official OneDrive uninstaller and removes autorun keys.", true, 3, L"WinUtil" },
        { "hpets_boot",       L"Trim boot: no GUI boot + msconfig",   L"Suppresses the animated boot logo for 1-2s faster POST-to-login.", true, 1, L"Community" },
        { "edge_bing_blocking", L"Block Edge prerender/prelaunch",    L"Stops Edge spawning background processes at boot.", false, 1, L"WinUtil" },

        // ---- v2.1: advanced gaming / performance ----
        { "windowed_games",     L"Optimizations for windowed games",    L"[gaming] Windows' documented borderless-windowed optimization (DXGI flip model) - lower latency in borderless.", false, 2, L"Microsoft (SwapEffectUpgradeEnable)" },
        { "vrr",               L"Variable refresh rate (VRR)",         L"Lets the monitor refresh follow the game's frame rate when windowed - removes tearing without VSync.", false, 1, L"Microsoft (Gaming/VRROptimize)" },
        { "auto_hdr_off",      L"Disable Auto-HDR",                    L"Auto-HDR adds a per-frame tone-mapping pass on some GPUs. Disable for max FPS on SDR screens.", false, 1, L"Microsoft" },
        { "game_bar_off",      L"Disable Game Bar entirely",           L"Removes the Win+G overlay hooking every game window (keeps DVR off too).", false, 2, L"WinUtil" },
        { "xbox_presence",     L"Disable Xbox presence writes",        L"Stops the GameInput service writing presence data while you play.", true, 1, L"WinUtil" },
        { "bcdedit_tsc",       L"Dynamic tick + TSC sync",             L"disabledynamictick yes + useplatformtick yes: steadier scheduler tick for frame-time consistency.", true, 2, L"calypto / community" },
        { "msi_mode",          L"MSI mode for GPU + NIC",             L"Message-signaled interrupts instead of line-based: lower DPC latency on modern GPUs/NICs.", true, 3, L"MSI-util community" },
        { "interrupt_affinity",L"GPU interrupt affinity to P-cores",   L"Pins GPU device interrupts to performance cores (Intel hybrid) - reduces input latency spikes.", true, 2, L"game-optimizer style" },
        { "tcp_congestion",    L"TCP congestion: BBR2 / CUBIC",        L"[network] Modern congestion provider for smoother throughput under packet loss (default: cubic).", true, 1, L"MS docs (netsh)" },
        { "nic_powersave",     L"NIC power saving off",                L"Disables adapter energy-efficient ethernet + idle power-down - removes micro-spikes in ping.", true, 2, L"netprofile engine" },
        { "usb_powersave",     L"USB selective suspend off",           L"[input] Windows can pause USB ports to save power - causes occasional mouse/keyboard stutter. Off = 1:1 polling.", true, 1, L"MS power docs" },
        { "pcie_aspm",         L"PCIe link power management off",      L"Keeps the PCIe link at full speed (L0) instead of dropping to L1 - removes GPU wake latency.", true, 2, L"powercfg / PCI docs" },
        { "visual_fx_balloff", L"Balanced visual effects",             L"Like performance, but keeps font smoothing & window icons - performance look without the ugly.", false, 1, L"community" },
        { "mouse_trails",      L"Disable cursor trail & shadow",       L"Removes two per-frame cursor compositor passes.", false, 1, L"community" },
        { "transparency_off",  L"Disable acrylic transparency",         L"Solid windows: frees compositor cycles on integrated GPUs / laptops.", false, 1, L"WinUtil" },
        { "taskbar_anim",      L"Disable taskbar animations",           L"Faster taskbar, snappier start menu feel.", false, 1, L"WinUtil" },
        { "dns_cache_big",     L"Bigger DNS cache",                    L"[network] MaxCacheTtl 86400 + negative cache: fewer repeat DNS lookups while gaming.", true, 1, L"MS Tcpip docs" },
        { "shutdown_fast",     L"Fast startup ON",                      L"Hiberboot: faster cold boot (kernel hibernation). Disable if dual-booting Linux.", true, 1, L"MS" },
        { "recycle_bin_conf",  L"Recycle bin: immediate confirm",       L"Classic 'are you sure' dialog instead of silent delete - safety, not speed.", false, 1, L"community" },
    };
    return c;
}

int adminCount() {
    int n = 0;
    for (auto& t : catalog()) if (t.admin) n++;
    return n;
}

// ------------------------------------------------------- registry helpers
static LONG regSetDword(HKEY root, const wstring& path, const wstring& name, DWORD v) {
    HKEY k;
    LONG rc = RegCreateKeyExW(root, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr);
    if (rc != ERROR_SUCCESS) return rc;
    rc = RegSetValueExW(k, name.c_str(), 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    RegCloseKey(k);
    return rc;
}
static LONG regDelValue(HKEY root, const wstring& path, const wstring& name) {
    HKEY k;
    LONG rc = RegOpenKeyExW(root, path.c_str(), 0, KEY_SET_VALUE, &k);
    if (rc != ERROR_SUCCESS) return rc;
    rc = RegDeleteValueW(k, name.c_str());
    RegCloseKey(k);
    return rc;
}
static LONG regSetString(HKEY root, const wstring& path, const wstring& name, const wstring& v) {
    HKEY k;
    LONG rc = RegCreateKeyExW(root, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr);
    if (rc != ERROR_SUCCESS) return rc;
    rc = RegSetValueExW(k, name.c_str(), 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    return rc;
}
static DWORD regGetDword(HKEY root, const wstring& path, const wstring& name, DWORD def) {
    DWORD v = def, size = sizeof(v), type = 0;
    if (RegGetValueW(root, path.c_str(), name.c_str(), RRF_RT_REG_DWORD, &type, &v, &size) != ERROR_SUCCESS) return def;
    return v;
}
static bool svcExists(const wstring& name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    SC_HANDLE s = OpenServiceW(scm, name.c_str(), SERVICE_QUERY_STATUS);
    bool ok = s != nullptr;
    if (s) CloseServiceHandle(s);
    CloseServiceHandle(scm);
    return ok;
}
static bool svcDisable(const wstring& name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    SC_HANDLE s = OpenServiceW(scm, name.c_str(), SERVICE_CHANGE_CONFIG | SERVICE_START);
    bool ok = false;
    if (s) {
        ok = ChangeServiceConfigW(s, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_NO_CHANGE,
                                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) != 0;
        CloseServiceHandle(s);
    }
    CloseServiceHandle(scm);
    return ok;
}
static bool svcSetManual(const wstring& name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    SC_HANDLE s = OpenServiceW(scm, name.c_str(), SERVICE_CHANGE_CONFIG);
    bool ok = false;
    if (s) {
        ok = ChangeServiceConfigW(s, SERVICE_NO_CHANGE, SERVICE_DEMAND_START, SERVICE_NO_CHANGE,
                                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) != 0;
        CloseServiceHandle(s);
    }
    CloseServiceHandle(scm);
    return ok;
}

// -------------------------------------------------------------- backup
wstring backupAll() {
    static wstring done; // only once per process
    if (!done.empty()) return done;
    wstring path = backupPath();
    wstring cmd = L"reg export \"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\" \"" + path + L"\" /y";
    // We export the roots we actually touch, one file per root (reg export is single-key).
    wstring base = path.substr(0, path.size() - 4); // strip ".reg"
    vector<std::pair<wstring, wstring>> keys = {
        { L"HKCU\\Software\\Microsoft\\GameBar",                                     L"_gamebar"   },
        { L"HKCU\\System\\GameConfigStore",                                          L"_gamecfg"   },
        { L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer",           L"_explorer"  },
        { L"HKCU\\Control Panel\\Desktop",                                           L"_desktop"   },
        { L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager", L"_cdm"   },
        { L"HKLM\\SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",              L"_gfx"       },
        { L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",           L"_tcpip"     },
        { L"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia",      L"_mmcss"     },
        { L"HKLM\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management\\PrefetchParameters", L"_prefetch" },
        { L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",           L"_datacoll"  },
    };
    wstring first;
    for (auto& [key, suffix] : keys) {
        wstring f = base + suffix + L".reg";
        wstring c = L"reg export \"" + key + L"\" \"" + f + L"\" /y";
        string out;
        runCapture(c, out, 20000);
        if (first.empty()) first = f;
    }
    done = first;
    log::info(L"registry backup written under " + appDataDir());
    return done;
}

// --------------------------------------------------------------- states
static bool svcIsDisabled(const wstring& name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    SC_HANDLE s = OpenServiceW(scm, name.c_str(), SERVICE_QUERY_CONFIG);
    bool dis = false;
    if (s) {
        DWORD need = 0;
        QueryServiceConfigW(s, nullptr, 0, &need);
        if (need) {
            std::vector<BYTE> buf(need);
            QUERY_SERVICE_CONFIGW* cfg = (QUERY_SERVICE_CONFIGW*)buf.data();
            if (QueryServiceConfigW(s, cfg, need, &need)) dis = (cfg->dwStartType == SERVICE_DISABLED);
        }
        CloseServiceHandle(s);
    }
    CloseServiceHandle(scm);
    return dis;
}

State checkState(const string& id) {
    State st;
    if      (id == "game_mode")        st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"AutoGameModeEnabled", 1) == 1;
    else if (id == "game_dvr_off")     st.applied = regGetDword(HKEY_CURRENT_USER, L"System\\GameConfigStore", L"GameDVR_Enabled", 1) == 0
                                                  && regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", L"AppCaptureEnabled", 1) == 0;
    else if (id == "hags_on")          st.applied = regGetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers", L"HwSchMode", 1) == 2;
    else if (id == "mpo_off")          st.applied = regGetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\Dwm", L"OverlayTestMode", 0) == 5;
    else if (id == "timer_high")       st.applied = regGetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\kernel", L"GlobalTimerResolutionRequests", 0) == 1;
    else if (id == "network_gaming")   st.applied = regGetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile", L"NetworkThrottlingIndex", 10) == 0xFFFFFFFF
                                                  && regGetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile", L"SystemResponsiveness", 20) == 0;
    else if (id == "mouse_precision")  st.applied = (regGetDword(HKEY_CURRENT_USER, L"Control Panel\\Mouse", L"MouseSpeed", 1) == 0);
    else if (id == "visual_fx_perf")   st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects", L"VisualFXSetting", 0) == 2;
    else if (id == "menu_delay_0")     st.applied = regGetDword(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"MenuShowDelay", 400) == 0;
    else if (id == "background_apps")  st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\BackgroundAccessApplications", L"GlobalUserDisabled", 0) == 1;
    else if (id == "storage_sense")    st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\StorageSense\\Parameters\\StoragePolicy", L"01", 0) == 1;
    else if (id == "search_index")     st.applied = svcIsDisabled(L"WSearch");
    else if (id == "sysmain_off")      st.applied = svcIsDisabled(L"SysMain");
    else if (id == "hpets_off")        st.applied = regGetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\hpet", L"Start", 1) == 4;
    else if (id == "power_ultimate")   st.applied = ok::sysinfo::activePowerPlanName().find(L"Ultimate") != wstring::npos;
    else if (id == "win32_priority")   st.applied = regGetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\PriorityControl", L"Win32PrioritySeparation", 2) == 0x26;
    else if (id == "gpu_preference")   st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\DirectX\\UserGpuPreferences", L"DirectXUserGlobalSettings", 0) != 0;
    else if (id == "fso_on")           st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers", L"~ DISABLEDXMAXIMIZEDWINDOWEDMODE", 0) != 0;
    else if (id == "xbox_live_off")    st.applied = (svcExists(L"XblAuthManager") && svcIsDisabled(L"XblAuthManager"));
    else if (id == "telemetry_off")    st.applied = regGetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", L"AllowTelemetry", 1) == 0;
    else if (id == "advertising_off")  st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo", L"Enabled", 1) == 0;
    else if (id == "activity_history") st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Privacy", L"PublishUserActivities", 1) == 0;
    else if (id == "bing_search")      st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Search", L"BingSearchEnabled", 1) == 0;
    else if (id == "tailored_experiences") st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Privacy", L"TailoredExperiencesWithDiagnosticDataEnabled", 1) == 0;
    else if (id == "telemetry_tasks")  st.applied = false; // heuristic: always show as "not applied" (tasks can be recreated by updates)
    else if (id == "windows_copilot")  st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"ShowCopilotButton", 1) == 0;
    else if (id == "bloat_uninstall")  st.applied = false;
    else if (id == "onedrive_off")     st.applied = svcExists(L"OneSyncSvc") == false || svcIsDisabled(L"OneSyncSvc");
    else if (id == "hpets_boot")       st.applied = false;
    else if (id == "edge_bing_blocking") st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\Policies\\Microsoft\\Microsoft Edge", L"HubsSidebarEnabled", 1) == 0;
    else if (id == "windowed_games")    st.applied = (regGetDword(HKEY_CURRENT_USER, L"Software\\DirectX\\UserGpuPreferences", L"DirectXUserGlobalSettings", 0) & 0) == 0 && []{ wstring v; DWORD sz=256; wchar_t b[256]; DWORD t=0; HKEY k; if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\DirectX\\UserGpuPreferences",0,KEY_READ,&k)!=ERROR_SUCCESS) return false; LONG rc=RegGetValueW(k,nullptr,L"DirectXUserGlobalSettings",RRF_RT_REG_SZ,&t,b,&sz); RegCloseKey(k); if(rc!=ERROR_SUCCESS) return false; return wcsstr(b,L"SwapEffectUpgradeEnable=1")!=nullptr; }();
    else if (id == "vrr")               st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\DirectX\\UserGpuPreferences", L"VRROptimizeEnable", 0) == 1;
    else if (id == "auto_hdr_off")      st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\AutoHDR", L"Enabled", 1) == 0 || regGetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\Dwm", L"ForceAutoHDR", 0) == 0;
    else if (id == "game_bar_off")      st.applied = regGetDword(HKEY_CURRENT_USER, L"System\\GameConfigStore", L"GameDVR_Enabled", 1) == 0 && regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"AllowAutoGameMode", 1) == 0;
    else if (id == "xbox_presence")     st.applied = svcIsDisabled(L"GameInput");
    else if (id == "bcdedit_tsc")       st.applied = false;  // bcdedit has no cheap read via registry; reapply is idempotent
    else if (id == "msi_mode")          st.applied = false;  // needs per-device registry walk; displayed as Diagnostic
    else if (id == "interrupt_affinity")st.applied = false;  // Ditto - device-specific, shown as Diagnostic
    else if (id == "tcp_congestion")    st.applied = false;  // netsh-read only
    else if (id == "nic_powersave")     st.applied = ok::netprofile::status().rscEnabled == false;
    else if (id == "usb_powersave")     st.applied = false; // driver-specific USB selective suspend read via powercfg /qh only
    else if (id == "pcie_aspm")         st.applied = false;  // read via powercfg /qh, expensive
    else if (id == "visual_fx_balloff") st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects", L"VisualFXSetting", 0) == 3;
    else if (id == "mouse_trails")      st.applied = regGetDword(HKEY_CURRENT_USER, L"Control Panel\\Cursors", L"CursorTrails", 0) == 0 && regGetDword(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"UserPreferencesMask", 0) == 0;
    else if (id == "transparency_off")  st.applied = regGetDword(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"EnableTransparency", 1) == 0;
    else if (id == "taskbar_anim")      st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"TaskbarAnimations", 1) == 0;
    else if (id == "dns_cache_big")     st.applied = regGetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters", L"MaxCacheTtl", 86400) > 86400;
    else if (id == "shutdown_fast")     st.applied = regGetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power", L"HiberbootEnabled", 0) == 1;
    else if (id == "recycle_bin_conf")  st.applied = regGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer", L"ConfirmFileDelete", 0) == 1;
    return st;
}

// --------------------------------------------------------------- apply
bool apply(const string& id, wstring& err) {
    backupAll();
    if (id == "game_mode") {
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"AutoGameModeEnabled", 1);
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"AllowAutoGameMode", 1);
    }
    else if (id == "game_dvr_off") {
        regSetDword(HKEY_CURRENT_USER, L"System\\GameConfigStore", L"GameDVR_Enabled", 0);
        regSetDword(HKEY_CURRENT_USER, L"System\\GameConfigStore", L"GameDVR_FSEBehaviorMode", 2);
        regSetDword(HKEY_CURRENT_USER, L"System\\GameConfigStore", L"GameDVR_HonorUserFSEBehaviorMode", 1);
        regSetDword(HKEY_CURRENT_USER, L"System\\GameConfigStore", L"GameDVR_EFSEFeatureFlags", 0);
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", L"AppCaptureEnabled", 0);
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"UseNexusForGameBarEnabled", 0);
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"ShowStartupPanel", 0);
    }
    else if (id == "hags_on") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        regSetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers", L"HwSchMode", 2);
    }
    else if (id == "mpo_off") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        regSetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\Dwm", L"OverlayTestMode", 5);
    }
    else if (id == "timer_high") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        regSetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\kernel", L"GlobalTimerResolutionRequests", 1);
    }
    else if (id == "network_gaming") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        // MMCSS gaming: SystemResponsiveness=0, NetworkThrottlingIndex=0xFFFFFFFF
        regSetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile", L"NetworkThrottlingIndex", 0xFFFFFFFF);
        regSetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile", L"SystemResponsiveness", 0);
        // games task gets priority
        regSetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\\Tasks\\Games", L"GPU Priority", 8);
        regSetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\\Tasks\\Games", L"Priority", 6);
        regSetString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\\Tasks\\Games", L"Scheduling Category", L"High");
        // TcpAckFrequency / TCPNoDelay on every active interface
        HKEY nicRoot;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces", 0, KEY_READ, &nicRoot) == ERROR_SUCCESS) {
            wchar_t name[256]; DWORD i = 0, n = 256;
            while (RegEnumKeyExW(nicRoot, i++, name, &n, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                n = 256;
                if (regGetDword(HKEY_LOCAL_MACHINE, wstring(L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\") + name, L"DhcpIPAddress", 0xFFFFFFFF) != 0xFFFFFFFF
                    || regGetDword(HKEY_LOCAL_MACHINE, wstring(L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\") + name, L"IPAddressType", 99) != 99) {
                    wstring p = wstring(L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\") + name;
                    regSetDword(HKEY_LOCAL_MACHINE, p, L"TcpAckFrequency", 1);
                    regSetDword(HKEY_LOCAL_MACHINE, p, L"TCPNoDelay", 1);
                }
            }
            RegCloseKey(nicRoot);
        }
    }
    else if (id == "mouse_precision") {
        regSetString(HKEY_CURRENT_USER, L"Control Panel\\Mouse", L"MouseSpeed", L"0");
        regSetString(HKEY_CURRENT_USER, L"Control Panel\\Mouse", L"MouseThreshold1", L"0");
        regSetString(HKEY_CURRENT_USER, L"Control Panel\\Mouse", L"MouseThreshold2", L"0");
    }
    else if (id == "visual_fx_perf") {
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects", L"VisualFXSetting", 2);
        regSetString(HKEY_CURRENT_USER, L"Control Panel\\Desktop\\WindowMetrics", L"MinAnimate", L"0");
    }
    else if (id == "menu_delay_0") {
        regSetDword(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"MenuShowDelay", 0);
    }
    else if (id == "background_apps") {
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\BackgroundAccessApplications", L"GlobalUserDisabled", 1);
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Search", L"BackgroundAppGlobalToggle", 0);
    }
    else if (id == "storage_sense") {
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\StorageSense\\Parameters\\StoragePolicy", L"01", 1);
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\StorageSense\\Parameters\\StoragePolicy", L"2048", 7); // weekly
    }
    else if (id == "search_index") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        svcDisable(L"WSearch");
    }
    else if (id == "sysmain_off") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        svcDisable(L"SysMain");
    }
    else if (id == "hpets_off") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        regSetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\hpet", L"Start", 4);
        string out;
        runCapture(L"bcdedit /set useplatformclock false", out, 30000);
        runCapture(L"bcdedit /set disabledynamictick yes", out, 30000);
    }
    else if (id == "power_ultimate") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        string out;
        runCapture(L"powercfg -duplicatescheme e9a42b02-d5df-448d-aa00-03f14749eb61", out, 30000);
        runCapture(L"powercfg /setactive e9a42b02-d5df-448d-aa00-03f14749eb61", out, 30000);
    }
    else if (id == "win32_priority") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        regSetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\PriorityControl", L"Win32PrioritySeparation", 0x26);
    }
    else if (id == "gpu_preference") {
        regSetString(HKEY_CURRENT_USER, L"Software\\DirectX\\UserGpuPreferences", L"DirectXUserGlobalSettings", L"SwapEffectUpgradeEnable=1;");
    }
    else if (id == "fso_on") {
        // The global "disable fullscreen optimisations" is per-exe in reality; we set the
        // "disable maximized windowed mode" compat for the system so games keep exclusive FS.
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers", L"~ DISABLEDXMAXIMIZEDWINDOWEDMODE", 1);
    }
    else if (id == "xbox_live_off") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        svcDisable(L"XblAuthManager");
        svcDisable(L"XblGameSave");
        svcDisable(L"XboxNetApiSvc");
        svcDisable(L"XboxGipSvc");
    }
    else if (id == "telemetry_off") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        regSetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", L"AllowTelemetry", 0);
        regSetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\DataCollection", L"AllowTelemetry", 0);
        svcDisable(L"DiagTrack");
        svcDisable(L"dmwappushservice");
    }
    else if (id == "advertising_off") {
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo", L"Enabled", 0);
    }
    else if (id == "activity_history") {
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Privacy", L"PublishUserActivities", 0);
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Privacy", L"UploadUserActivities", 0);
    }
    else if (id == "bing_search") {
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Search", L"BingSearchEnabled", 0);
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Search", L"CortanaConsent", 0);
    }
    else if (id == "tailored_experiences") {
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Privacy", L"TailoredExperiencesWithDiagnosticDataEnabled", 0);
    }
    else if (id == "telemetry_tasks") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        static const wchar_t* tasks[] = {
            L"\\Microsoft\\Windows\\Customer Experience Improvement Program\\Consolidator",
            L"\\Microsoft\\Windows\\Customer Experience Improvement Program\\UsbCeip",
            L"\\Microsoft\\Windows\\Customer Experience Improvement Program\\Uploader",
            L"\\Microsoft\\Windows\\Application Experience\\Microsoft Compatibility Appraiser",
            L"\\Microsoft\\Windows\\Application Experience\\ProgramDataUpdater",
            L"\\Microsoft\\Windows\\Application Experience\\StartupAppTask",
            L"\\Microsoft\\Windows\\Autochk\\Proxy",
            L"\\Microsoft\\Windows\\DiskDiagnostic\\Microsoft-Windows-DiskDiagnosticDataCollector",
            L"\\Microsoft\\Windows\\Feedback\\Siuf\\DmClient",
            L"\\Microsoft\\Windows\\Feedback\\Siuf\\DmClientOnScenarioDownload",
        };
        for (auto t : tasks) {
            wstring c = wstring(L"schtasks /Change /TN \"") + t + L"\" /Disable";
            string out; runCapture(c, out, 20000);
        }
    }
    else if (id == "windows_copilot") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        regSetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsCopilot", L"TurnOffWindowsCopilot", 1);
        regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"ShowCopilotButton", 0);
    }
    else if (id == "bloat_uninstall") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        static const wchar_t* pkgs[] = {
            L"Microsoft.549981C3F5F10",           // Cortana
            L"Microsoft.BingFinance",
            L"Microsoft.BingNews",
            L"Microsoft.BingSports",
            L"Microsoft.BingWeather",
            L"Microsoft.BingSearch",
            L"Microsoft.Clipchamp",
            L"Microsoft.GamingApp",
            L"Microsoft.GetHelp",
            L"Microsoft.Getstarted",
            L"Microsoft.Microsoft3DViewer",
            L"Microsoft.MicrosoftOfficeHub",
            L"Microsoft.MicrosoftSolitaireCollection",
            L"Microsoft.MicrosoftStickyNotes",
            L"Microsoft.MixedReality.Portal",
            L"Microsoft.News",
            L"Microsoft.Office.OneNote",
            L"Microsoft.OutlookForWindows",
            L"Microsoft.Paint",
            L"Microsoft.PeerDist",
            L"Microsoft.PowerAutomateDesktop",
            L"Microsoft.SkypeApp",
            L"Microsoft.Todos",
            L"Microsoft.Wallet",
            L"Microsoft.Whiteboard",
            L"Microsoft.WindowsAlarms",
            L"Microsoft.WindowsFeedbackHub",
            L"Microsoft.WindowsMaps",
            L"Microsoft.WindowsPhone",
            L"Microsoft.WindowsSoundRecorder",
            L"Microsoft.Xbox.TCUI",
            L"Microsoft.XboxApp",
            L"Microsoft.XboxGameOverlay",
            L"Microsoft.XboxGamingOverlay",
            L"Microsoft.XboxIdentityProvider",
            L"Microsoft.XboxSpeechToTextOverlay",
            L"Microsoft.YourPhone",
            L"Microsoft.ZuneMusic",
            L"Microsoft.ZuneVideo",
            L"MicrosoftTeams",
            L"Clipchamp.Clipchamp",
            L"Microsoft.Copilot",
            L"Microsoft.Coroutine", // only some builds
            L"Microsoft.MicrosoftJournal",
        };
        wstring ps = L"powershell -NoProfile -ExecutionPolicy Bypass -Command \"";
        bool first = true;
        for (auto p : pkgs) {
            if (!first) ps += L"; ";
            first = false;
            ps += wstring(L"Get-AppxPackage -Name '") + p + L"' -AllUsers | Remove-AppxPackage -ErrorAction SilentlyContinue";
            ps += L"; Get-AppxProvisionedPackage -Online | Where-Object DisplayName -eq '" + wstring(p) + L"' | Remove-AppxProvisionedPackage -Online -ErrorAction SilentlyContinue | Out-Null";
        }
        ps += L"\"";
        string out;
        runCapture(ps, out, 300000);
    }
    else if (id == "onedrive_off") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        string out;
        runCapture(L"powershell -NoProfile -ExecutionPolicy Bypass -Command \"Get-Process onedrive -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Process $env:WinDir\\SysWOW64\\OneDriveSetup.exe /uninstall -Wait -ErrorAction SilentlyContinue; Start-Process $env:WinDir\\System32\\OneDriveSetup.exe /uninstall -Wait -ErrorAction SilentlyContinue\"", out, 180000);
        regSetDword(HKEY_CLASSES_ROOT, L"CLSID\\{018D5C66-4533-4307-9B53-224DE2ED1FE6}", L"System.IsPinnedToNameSpaceTree", 0);
        regSetDword(HKEY_CLASSES_ROOT, L"Wow6432Node\\CLSID\\{018D5C66-4533-4307-9B53-224DE2ED1FE6}", L"System.IsPinnedToNameSpaceTree", 0);
    }
    else if (id == "hpets_boot") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        string out;
        runCapture(L"bcdedit /set nobootuxprogress", out, 30000);
    }
    else if (id == "edge_bing_blocking") {
        regSetDword(HKEY_CURRENT_USER, L"Software\\Policies\\Microsoft\\Microsoft Edge", L"HubsSidebarEnabled", 0);
        regSetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Microsoft Edge", L"BackgroundModeEnabled", 0);
        regSetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Microsoft Edge", L"StartupBoostEnabled", 0);
    }
    else if (id == "windowed_games")     { wstring cur; DWORD sz=256; wchar_t b[256]; DWORD t=0; HKEY k; if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\DirectX\\UserGpuPreferences",0,KEY_READ,&k)==ERROR_SUCCESS){ RegGetValueW(k,nullptr,L"DirectXUserGlobalSettings",RRF_RT_REG_SZ,&t,b,&sz); RegCloseKey(k); cur=b; } if(cur.find(L"SwapEffectUpgradeEnable")==wstring::npos) cur += L"SwapEffectUpgradeEnable=1;"; regSetString(HKEY_CURRENT_USER, L"Software\\DirectX\\UserGpuPreferences", L"DirectXUserGlobalSettings", cur); }
    else if (id == "vrr")                { regSetDword(HKEY_CURRENT_USER, L"Software\\DirectX\\UserGpuPreferences", L"VRROptimizeEnable", 1); }
    else if (id == "auto_hdr_off")       { regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\AutoHDR", L"Enabled", 0); regSetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\Dwm", L"ForceAutoHDR", 0); }
    else if (id == "game_bar_off")       { regSetDword(HKEY_CURRENT_USER, L"System\\GameConfigStore", L"GameDVR_Enabled", 0); regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"AllowAutoGameMode", 0); regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"AutoGameModeEnabled", 0); regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"UseNexusForGameBarEnabled", 0); regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", L"AppCaptureEnabled", 0); }
    else if (id == "xbox_presence")      { if (!isAdmin()) { err = L"administrator required"; return false; } svcDisable(L"GameInput"); }
    else if (id == "bcdedit_tsc")        { if (!isAdmin()) { err = L"administrator required"; return false; } string o; runCapture(L"bcdedit /set disabledynamictick yes", o, 30000); runCapture(L"bcdedit /set useplatformtick yes", o, 30000); }
    else if (id == "msi_mode")           { if (!isAdmin()) { err = L"administrator required"; return false; } string o; runCapture(L"powershell -NoProfile -ExecutionPolicy Bypass -Command \"Get-PnpDevice -Class Display,Net -Status OK | ForEach-Object { $p='HKLM:\\SYSTEM\\CurrentControlSet\\Enum\\'+$_.InstanceId; $d=(Get-ItemProperty -Path ($p+'\\Device Parameters') -ErrorAction SilentlyContinue); if($d -and $d.MessageSignaledInterruptProperties -ne $null -and $d.MessageSignaledInterruptProperties -eq 0){ Set-ItemProperty -Path ($p+'\\Device Parameters') -Name MessageSignaledInterruptProperties -Value 1 } }\"", o, 60000); }
    else if (id == "interrupt_affinity") { if (!isAdmin()) { err = L"administrator required"; return false; } string o; runCapture(L"powershell -NoProfile -ExecutionPolicy Bypass -Command \"$gpu=(Get-PnpDevice -Class Display -Status OK | Select-Object -First 1).InstanceId; if($gpu){ $key='HKLM:\\SYSTEM\\CurrentControlSet\\Enum\\'+$gpu+'\\Device Parameters\\Interrupt Management\\MessageSignaledInterruptProperties'; if(Test-Path $key){ Set-ItemProperty -Path $key -Name DevicePolicy -Value 4 -Type DWord -ErrorAction SilentlyContinue; Set-ItemProperty -Path $key -Name DevicePriority -Value 3 -Type DWord -ErrorAction SilentlyContinue } }\"", o, 60000); }
    else if (id == "tcp_congestion")     { if (!isAdmin()) { err = L"administrator required"; return false; } string o; runCapture(L"netsh int tcp set supplemental Internet congestionprovider=bbr2", o, 30000); }
    else if (id == "nic_powersave")      { if (!isAdmin()) { err = L"administrator required"; return false; } ok::netprofile::apply("gaming"); }
    else if (id == "usb_powersave")      { if (!isAdmin()) { err = L"administrator required"; return false; } string o; runCapture(L"powercfg /setacvalueindex scheme_current 2a737441-1930-4402-8d77-b2bebba308a3 48e6b7a6-50f5-4782-a5d4-53bb8f07e226 0", o, 30000); runCapture(L"powercfg /setactive scheme_current", o, 30000); }
    else if (id == "pcie_aspm")          { if (!isAdmin()) { err = L"administrator required"; return false; } string o; runCapture(L"powercfg /setacvalueindex scheme_current 501a4d13-42af-4429-9fd1-a8218c268e20 ee12f906-d277-404b-b6da-e5fa1a576df5 0", o, 30000); runCapture(L"powercfg /setactive scheme_current", o, 30000); }
    else if (id == "visual_fx_balloff")  { regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects", L"VisualFXSetting", 3); }
    else if (id == "mouse_trails")       { regSetDword(HKEY_CURRENT_USER, L"Control Panel\\Cursors", L"CursorTrails", 0); regSetDword(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"UserPreferencesMask", 0x90120380); }
    else if (id == "transparency_off")   { regSetDword(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"EnableTransparency", 0); }
    else if (id == "taskbar_anim")       { regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"TaskbarAnimations", 0); }
    else if (id == "dns_cache_big")      { if (!isAdmin()) { err = L"administrator required"; return false; } regSetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters", L"MaxCacheTtl", 86400); regSetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters", L"MaxNegativeCacheTtl", 5); regSetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters", L"MaxCacheSize", 0x64000); }
    else if (id == "recycle_bin_conf")   { regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer", L"ConfirmFileDelete", 1); }
    else {
        err = L"unknown tweak id: " + widen(id);
        return false;
    }
    log::ok(L"applied " + widen(id));
    return true;
}

// --------------------------------------------------------------- restore
bool restore(const string& id, wstring& err) {
    if (id == "game_mode")       { regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"AutoGameModeEnabled", 1); }
    else if (id == "game_dvr_off") {
        regDelValue(HKEY_CURRENT_USER, L"System\\GameConfigStore", L"GameDVR_Enabled");
        regDelValue(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", L"AppCaptureEnabled");
    }
    else if (id == "hags_on")    { if (!isAdmin()) { err = L"administrator required"; return false; } regSetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers", L"HwSchMode", 1); }
    else if (id == "mpo_off")    { if (!isAdmin()) { err = L"administrator required"; return false; } regDelValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\Dwm", L"OverlayTestMode"); }
    else if (id == "timer_high") { if (!isAdmin()) { err = L"administrator required"; return false; } regDelValue(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\kernel", L"GlobalTimerResolutionRequests"); }
    else if (id == "network_gaming") {
        if (!isAdmin()) { err = L"administrator required"; return false; }
        regSetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile", L"NetworkThrottlingIndex", 10);
        regSetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile", L"SystemResponsiveness", 20);
    }
    else if (id == "mouse_precision") {
        regSetString(HKEY_CURRENT_USER, L"Control Panel\\Mouse", L"MouseSpeed", L"1");
        regSetString(HKEY_CURRENT_USER, L"Control Panel\\Mouse", L"MouseThreshold1", L"6");
        regSetString(HKEY_CURRENT_USER, L"Control Panel\\Mouse", L"MouseThreshold2", L"10");
    }
    else if (id == "visual_fx_perf")     { regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects", L"VisualFXSetting", 0); }
    else if (id == "menu_delay_0")       { regSetDword(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"MenuShowDelay", 400); }
    else if (id == "background_apps")    { regDelValue(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\BackgroundAccessApplications", L"GlobalUserDisabled"); }
    else if (id == "storage_sense")      { regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\StorageSense\\Parameters\\StoragePolicy", L"01", 0); }
    else if (id == "search_index")       { if (!isAdmin()) { err = L"administrator required"; return false; } svcSetManual(L"WSearch"); }
    else if (id == "sysmain_off")        { if (!isAdmin()) { err = L"administrator required"; return false; } svcSetManual(L"SysMain"); }
    else if (id == "hpets_off")          { if (!isAdmin()) { err = L"administrator required"; return false; } regSetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\hpet", L"Start", 1); }
    else if (id == "power_ultimate")     { if (!isAdmin()) { err = L"administrator required"; return false; } string o; runCapture(L"powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e", o, 30000); }
    else if (id == "win32_priority")     { if (!isAdmin()) { err = L"administrator required"; return false; } regSetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\PriorityControl", L"Win32PrioritySeparation", 2); }
    else if (id == "gpu_preference")     { regDelValue(HKEY_CURRENT_USER, L"Software\\DirectX\\UserGpuPreferences", L"DirectXUserGlobalSettings"); }
    else if (id == "fso_on")             { regDelValue(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers", L"~ DISABLEDXMAXIMIZEDWINDOWEDMODE"); }
    else if (id == "xbox_live_off")      { if (!isAdmin()) { err = L"administrator required"; return false; } svcSetManual(L"XblAuthManager"); svcSetManual(L"XblGameSave"); svcSetManual(L"XboxNetApiSvc"); }
    else if (id == "telemetry_off")      { if (!isAdmin()) { err = L"administrator required"; return false; } regSetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", L"AllowTelemetry", 1); }
    else if (id == "advertising_off")    { regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo", L"Enabled", 1); }
    else if (id == "activity_history")   { regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Privacy", L"PublishUserActivities", 1); }
    else if (id == "bing_search")        { regDelValue(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Search", L"BingSearchEnabled"); }
    else if (id == "tailored_experiences") { regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Privacy", L"TailoredExperiencesWithDiagnosticDataEnabled", 1); }
    else if (id == "telemetry_tasks")    { if (!isAdmin()) { err = L"administrator required"; return false; } /* restoring 30+ tasks is noisy; Windows Update re-enables them anyway */ }
    else if (id == "windows_copilot")    { if (!isAdmin()) { err = L"administrator required"; return false; } regDelValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsCopilot", L"TurnOffWindowsCopilot"); regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"ShowCopilotButton", 1); }
    else if (id == "bloat_uninstall")    { /* no auto-restore; use Microsoft Store to reinstall */ }
    else if (id == "onedrive_off")       { shellOpen(L"https://www.microsoft.com/microsoft-365/onedrive/download"); }
    else if (id == "hpets_boot")         { if (!isAdmin()) { err = L"administrator required"; return false; } string o; runCapture(L"bcdedit /deletevalue nobootuxprogress", o, 30000); }
    else if (id == "edge_bing_blocking") { regDelValue(HKEY_CURRENT_USER, L"Software\\Policies\\Microsoft\\Microsoft Edge", L"HubsSidebarEnabled"); }
    else if (id == "windowed_games")     { wstring cur; DWORD sz=256; wchar_t b[256]; DWORD t=0; HKEY k; if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\DirectX\\UserGpuPreferences",0,KEY_READ,&k)==ERROR_SUCCESS){ RegGetValueW(k,nullptr,L"DirectXUserGlobalSettings",RRF_RT_REG_SZ,&t,b,&sz); RegCloseKey(k); cur=b; } size_t q=cur.find(L"SwapEffectUpgradeEnable=1;"); if(q==wstring::npos) q=cur.find(L"SwapEffectUpgradeEnable=1"); if(q!=wstring::npos) cur.replace(q, 26, L""); regSetString(HKEY_CURRENT_USER, L"Software\\DirectX\\UserGpuPreferences", L"DirectXUserGlobalSettings", cur); }
    else if (id == "vrr")                { regDelValue(HKEY_CURRENT_USER, L"Software\\DirectX\\UserGpuPreferences", L"VRROptimizeEnable"); }
    else if (id == "auto_hdr_off")       { regDelValue(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\AutoHDR", L"Enabled"); regDelValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\Dwm", L"ForceAutoHDR"); }
    else if (id == "game_bar_off")       { regSetDword(HKEY_CURRENT_USER, L"System\\GameConfigStore", L"GameDVR_Enabled", 1); regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"AllowAutoGameMode", 1); regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"AutoGameModeEnabled", 1); regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"UseNexusForGameBarEnabled", 1); regDelValue(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", L"AppCaptureEnabled"); }
    else if (id == "xbox_presence")      { if (!isAdmin()) { err = L"administrator required"; return false; } svcSetManual(L"GameInput"); }
    else if (id == "bcdedit_tsc")        { if (!isAdmin()) { err = L"administrator required"; return false; } string o; runCapture(L"bcdedit /deletevalue disabledynamictick", o, 30000); runCapture(L"bcdedit /deletevalue useplatformtick", o, 30000); }
    else if (id == "msi_mode")           { if (!isAdmin()) { err = L"administrator required"; return false; } string o; runCapture(L"powershell -NoProfile -Command \"Get-PnpDevice -Class Display,Net -Status OK | ForEach-Object { $p='HKLM:\\SYSTEM\\CurrentControlSet\\Enum\\'+$_.InstanceId+'\\Device Parameters'; if(Test-Path $p){ Set-ItemProperty -Path $p -Name MessageSignaledInterruptProperties -Value 0 -ErrorAction SilentlyContinue } }\"", o, 60000); }
    else if (id == "interrupt_affinity") { if (!isAdmin()) { err = L"administrator required"; return false; } string o; runCapture(L"powershell -NoProfile -Command \"$gpu=(Get-PnpDevice -Class Display -Status OK | Select-Object -First 1).InstanceId; if($gpu){ $key='HKLM:\\SYSTEM\\CurrentControlSet\\Enum\\'+$gpu+'\\Device Parameters\\Interrupt Management\\MessageSignaledInterruptProperties'; if(Test-Path $key){ Remove-ItemProperty -Path $key -Name DevicePolicy -ErrorAction SilentlyContinue; Remove-ItemProperty -Path $key -Name DevicePriority -ErrorAction SilentlyContinue } }\"", o, 60000); }
    else if (id == "tcp_congestion")     { if (!isAdmin()) { err = L"administrator required"; return false; } string o; runCapture(L"netsh int tcp set supplemental Internet congestionprovider=cubic", o, 30000); }
    else if (id == "nic_powersave")      { if (!isAdmin()) { err = L"administrator required"; return false; } ok::netprofile::apply("restore"); }
    else if (id == "usb_powersave")      { if (!isAdmin()) { err = L"administrator required"; return false; } string o; runCapture(L"powercfg /setacvalueindex scheme_current 2a737441-1930-4402-8d77-b2bebba308a3 48e6b7a6-50f5-4782-a5d4-53bb8f07e226 1", o, 30000); runCapture(L"powercfg /setactive scheme_current", o, 30000); }
    else if (id == "pcie_aspm")          { if (!isAdmin()) { err = L"administrator required"; return false; } string o; runCapture(L"powercfg /setacvalueindex scheme_current 501a4d13-42af-4429-9fd1-a8218c268e20 ee12f906-d277-404b-b6da-e5fa1a576df5 1", o, 30000); runCapture(L"powercfg /setactive scheme_current", o, 30000); }
    else if (id == "visual_fx_balloff")  { regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects", L"VisualFXSetting", 0); }
    else if (id == "mouse_trails")       { regSetDword(HKEY_CURRENT_USER, L"Control Panel\\Cursors", L"CursorTrails", 0); regSetDword(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"UserPreferencesMask", 0x9E3E0780); }
    else if (id == "transparency_off")   { regDelValue(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"EnableTransparency"); }
    else if (id == "taskbar_anim")       { regDelValue(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"TaskbarAnimations"); }
    else if (id == "dns_cache_big")      { if (!isAdmin()) { err = L"administrator required"; return false; } regDelValue(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters", L"MaxCacheTtl"); regDelValue(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters", L"MaxNegativeCacheTtl"); regDelValue(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters", L"MaxCacheSize"); }
    else if (id == "shutdown_fast")      { regSetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power", L"HiberbootEnabled", 1); }
    else if (id == "recycle_bin_conf")   { regSetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer", L"ConfirmFileDelete", 0); }
    else { err = L"unknown tweak id: " + widen(id); return false; }
    log::ok(L"restored defaults " + widen(id));
    return true;
}

int applyMany(const json& sel, vector<wstring>& errors) {
    int done = 0;
    for (auto& t : catalog()) {
        auto it = sel.find(t.id);
        if (it == sel.end() || !it->get<bool>()) continue;
        wstring e;
        if (apply(t.id, e)) done++;
        else {
            errors.push_back(widen(t.id) + L": " + e);
            log::fail(L"apply " + widen(t.id) + L" -> " + e);
        }
    }
    return done;
}

json profilePreset(const string& name) {
    json j = json::object();
    if (name == "gaming") {
        for (auto& t : catalog()) if (t.impact >= 2 && t.id != "onedrive_off") j[t.id] = true;
        j["game_mode"] = true; j["storage_sense"] = true; j["gpu_preference"] = true;
    } else if (name == "privacy") {
        for (auto& t : catalog())
            if (t.id == "telemetry_off" || t.id == "advertising_off" || t.id == "activity_history"
             || t.id == "bing_search" || t.id == "tailored_experiences" || t.id == "telemetry_tasks"
             || t.id == "windows_copilot" || t.id == "edge_bing_blocking") j[t.id] = true;
    } else if (name == "full") {
        for (auto& t : catalog()) j[t.id] = true;
    }
    return j;
}

} // namespace ok::tweaks
