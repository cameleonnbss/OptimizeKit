// OptimizeKit - RAM status + honest trim + startup manager
#include "ram.h"
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <filesystem>
#include <algorithm>

#pragma comment(lib, "psapi.lib")

// ntdll standby trim (documented call, used by Sysinternals RAMMap "Empty Standby List")
#include <winternl.h>
// MinGW's winternl.h lacks the prototype; declare it exactly as ntdll exports it:
extern "C" NTSTATUS __stdcall NtSetSystemInformation(ULONG InfoClass, PVOID Info, ULONG Length);
#ifndef SYSTEM_MEMORY_LIST_COMMAND
#define SYSTEM_MEMORY_LIST_COMMAND 80
#endif
#ifndef MemoryPurgeStandbyList
#define MemoryPurgeStandbyList 4
#endif

namespace fs = std::filesystem;

namespace ok::ram {

using json = nlohmann::json;

MemInfo collect() {
    MemInfo m;
    MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        m.total = ms.ullTotalPhys;
        m.avail = ms.ullAvailPhys;
        m.loadPercent = ms.dwMemoryLoad;
        m.committed = ms.ullTotalPageFile - ms.ullAvailPageFile;
        m.commitLimit = ms.ullTotalPageFile;
    }
    // cached estimate via GetPerformanceInfo (kernel paged + nonpaged)
    PERFORMANCE_INFORMATION pi{}; pi.cb = sizeof(pi);        if (GetPerformanceInfo(&pi, sizeof(pi))) {
            m.cached = (uint64_t)pi.KernelPaged * pi.PageSize + (uint64_t)pi.KernelNonpaged * pi.PageSize;
    }
    return m;
}

json toJson() {
    MemInfo m = collect();
    json j;
    j["total"] = m.total;
    j["avail"] = m.avail;
    j["usedPct"] = m.loadPercent;
    j["committed"] = m.committed;
    j["commitLimit"] = m.commitLimit;
    j["cached"] = m.cached;
    return j;
}

bool trimStandby(wstring& err) {
    // requires SeProfileSingleProcessPrivilege (admin). Honest UI copy explains the effect.
    ULONG cmd = MemoryPurgeStandbyList;
    NTSTATUS st = NtSetSystemInformation(SYSTEM_MEMORY_LIST_COMMAND, &cmd, sizeof(cmd));
    if (st != 0) {
        err = L"Standby trim failed (NTSTATUS 0x" + [&]{ wchar_t b[16]; swprintf(b, 16, L"%lx", st); return wstring(b); }() +
              L") — admin required";
        log::fail(err);
        return false;
    }
    log::ok(L"Standby list purged (cached memory freed for immediate reuse)");
    return true;
}

bool trimProcess(DWORD pid, wstring& err) {
    HANDLE h = OpenProcess(PROCESS_SET_QUOTA | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!h) { err = L"OpenProcess failed"; return false; }
    BOOL okb = SetProcessWorkingSetSizeEx(h, (SIZE_T)-1, (SIZE_T)-1, 0);
    CloseHandle(h);
    if (!okb) { err = L"SetProcessWorkingSetSizeEx failed"; return false; }
    log::ok(L"Working set trimmed for pid " + std::to_wstring(pid));
    return true;
}

} // namespace ok::ram

// ================================================================ startup
namespace ok::startup {

using json = nlohmann::json;

static void readRunKey(HKEY root, const wstring& rootName, vector<Entry>& out) {
    HKEY k;
    if (RegOpenKeyExW(root, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &k) != ERROR_SUCCESS) return;
    DWORD idx = 0; wchar_t name[256]; DWORD nameLen = 256;
    wchar_t val[1024]; DWORD valLen = sizeof(val); DWORD type = 0;
    while (RegEnumValueW(k, idx++, name, &nameLen, nullptr, &type, (BYTE*)val, &valLen) == ERROR_SUCCESS) {
        nameLen = 256; valLen = sizeof(val);
        if (type != REG_SZ && type != REG_EXPAND_SZ) continue;
        Entry e;
        e.name = name;
        e.command = val;
        e.location = rootName;
        out.push_back(e);
    }
    RegCloseKey(k);
}

static void readStartupFolder(const wstring& dir, const wstring& loc, vector<Entry>& out) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*.lnk").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        Entry e;
        e.name = fd.cFileName;
        e.command = dir + L"\\" + fd.cFileName;
        e.location = loc;
        out.push_back(e);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

vector<Entry> listEntries() {
    vector<Entry> out;
    readRunKey(HKEY_CURRENT_USER, L"HKCU\\...\\Run", out);
    readRunKey(HKEY_LOCAL_MACHINE, L"HKLM\\...\\Run", out);
    wchar_t* p = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &p) == S_OK) {
        readStartupFolder(p, L"Startup folder (user)", out);
        CoTaskMemFree(p);
    }
    if (SHGetKnownFolderPath(FOLDERID_CommonStartup, 0, nullptr, &p) == S_OK) {
        readStartupFolder(p, L"Startup folder (all users)", out);
        CoTaskMemFree(p);
    }
    return out;
}

json listJson() {
    json arr = json::array();
    for (auto& e : listEntries())
        arr.push_back({ {"name", narrow(e.name)}, {"command", narrow(e.command)}, {"location", narrow(e.location)} });
    return arr;
}

bool setEnabled(const wstring& location, const wstring& name, bool enable, wstring& err) {
    // Disable = move value to the RunOnce-disabled stash; Enable = move back.
    // For .lnk files: move the file to "<name>.disabled".
    if (location.rfind(L"Startup folder", 0) == 0) {
        wstring file = name;
        wstring disabled = file + L".disabled";
        if (enable) {
            if (!MoveFileW(disabled.c_str(), file.c_str())) { err = L"MoveFile failed"; return false; }
        } else {
            if (!MoveFileW(file.c_str(), disabled.c_str())) { err = L"MoveFile failed"; return false; }
        }
        log::ok(std::wstring(L"Startup entry ") + (enable ? L"enabled: " : L"disabled: ") + name);
        return true;
    }
    // Run keys
    HKEY root = (location.rfind(L"HKLM", 0) == 0) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const wchar_t* run = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
    const wchar_t* stash = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\OptimizeKitDisabledRun";
    if (enable) {
        // read from stash, write back to Run, delete from stash
        wchar_t val[1024]; DWORD sz = sizeof(val); DWORD type = 0;
        HKEY ks;
        if (RegOpenKeyExW(root, stash, 0, KEY_READ, &ks) != ERROR_SUCCESS) { err = L"Entry not in disabled stash — cannot re-enable"; return false; }
        LONG rc = RegGetValueW(ks, nullptr, name.c_str(), RRF_RT_REG_SZ, &type, val, &sz);
        RegCloseKey(ks);
        if (rc != ERROR_SUCCESS) { err = L"Entry not in disabled stash — cannot re-enable"; return false; }
        HKEY kd;
        if (RegCreateKeyExW(root, run, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &kd, nullptr) != ERROR_SUCCESS) { err = L"RegCreateKey failed"; return false; }
        RegSetValueExW(kd, name.c_str(), 0, REG_SZ, (BYTE*)val, sz);
        RegCloseKey(kd);
        if (RegOpenKeyExW(root, stash, 0, KEY_SET_VALUE, &kd) == ERROR_SUCCESS) {
            RegDeleteValueW(kd, name.c_str());
            RegCloseKey(kd);
        }
    } else {
        wchar_t val[1024]; DWORD sz = sizeof(val); DWORD type = 0;
        HKEY kr;
        if (RegOpenKeyExW(root, run, 0, KEY_READ, &kr) != ERROR_SUCCESS) { err = L"Run key missing — cannot read entry"; return false; }
        LONG rc = RegGetValueW(kr, nullptr, name.c_str(), RRF_RT_REG_SZ, &type, val, &sz);
        RegCloseKey(kr);
        if (rc != ERROR_SUCCESS) { err = L"Value missing"; return false; }
        HKEY ks;
        if (RegCreateKeyExW(root, stash, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &ks, nullptr) != ERROR_SUCCESS) {
            err = L"Administrator rights required to disable this startup entry (HKLM)";
            log::fail(L"Startup disable failed for " + name + L" : no admin");
            return false;
        }
        RegSetValueExW(ks, name.c_str(), 0, REG_SZ, (BYTE*)val, sz);
        RegCloseKey(ks);
        if (RegOpenKeyExW(root, run, 0, KEY_SET_VALUE, &kr) == ERROR_SUCCESS) {
            RegDeleteValueW(kr, name.c_str());
            RegCloseKey(kr);
        }
    }
    log::ok(std::wstring(L"Startup entry ") + (enable ? L"enabled: " : L"disabled: ") + name);
    return true;
}

} // namespace ok::startup
