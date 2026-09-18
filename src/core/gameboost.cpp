#include "gameboost.h"
#include <tlhelp32.h>

#pragma comment(lib, "advapi32.lib")

namespace ok::gameboost {

vector<RunningProcess> listProcesses() {
    vector<RunningProcess> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            // The UI/CLI filter; we list everything here.
            out.push_back({ pe.th32ProcessID, pe.szExeFile, L"" });
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return out;
}

bool setPriority(DWORD pid, int level, wstring& err) {
    DWORD cls = NORMAL_PRIORITY_CLASS;
    switch (level) {
        case 1: cls = IDLE_PRIORITY_CLASS; break;
        case 2: cls = BELOW_NORMAL_PRIORITY_CLASS; break;
        case 3: cls = NORMAL_PRIORITY_CLASS; break;
        case 4: cls = ABOVE_NORMAL_PRIORITY_CLASS; break;
        case 5: cls = HIGH_PRIORITY_CLASS; break;
        case 6: cls = REALTIME_PRIORITY_CLASS; break;
        default: err = L"bad level"; return false;
    }
    HANDLE h = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (!h) { err = L"OpenProcess failed (try running as admin)"; return false; }
    BOOL ok = SetPriorityClass(h, cls);
    CloseHandle(h);
    if (!ok) { err = L"SetPriorityClass failed"; return false; }
    log::ok(L"priority of PID " + std::to_wstring(pid) + L" set to level " + std::to_wstring(level));
    return true;
}

bool setPersistentPriority(const wstring& exeName, int level, wstring& err) {
    // Map level to the 6-bit PerfOptions encoding used by IFEO
    static const DWORD perfopts[] = { 0, 0, 0, 0, 0x28, 0x38, 0x50 }; // normal, above, high, realtime
    if (level < 4 || level > 6) { err = L"only above/normal/high/realtime persist"; return false; }
    wstring key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\" + exeName;
    HKEY k;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) {
        err = L"cannot write IFEO (admin required)";
        return false;
    }
    DWORD v = perfopts[level];
    RegSetValueExW(k, L"PerfOptions", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    RegCloseKey(k);
    log::ok(L"persistent priority stored for " + exeName);
    return true;
}

bool killProcess(DWORD pid, wstring& err) {
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) { err = L"OpenProcess failed"; return false; }
    BOOL ok = TerminateProcess(h, 1);
    CloseHandle(h);
    if (!ok) { err = L"TerminateProcess failed"; return false; }
    log::ok(L"killed PID " + std::to_wstring(pid));
    return true;
}

} // namespace ok::gameboost
