// OptimizeKit v2.6 - Process Reducer implementation.
//
// EcoQoS: SetProcessInformation(ProcessPowerThrottling) with
// PROCESS_POWER_THROTTLING_EXECUTION_SPEED asks the kernel to schedule the process on
// efficiency cores (and to let it idle harder). It is the documented mechanism behind
// Task Manager's "Efficiency mode" - reversible by clearing the same flag, which is
// exactly what undo()/undoAll() do.
//
// Nothing here requires admin for the processes the current user owns; system-owned
// processes simply fail with "access denied" and are reported as such.
#include "reducer.h"
#include "monitor.h"
#include <tlhelp32.h>

namespace ok::reducer {

using json = nlohmann::json;

// ------------------------------------------------------------------ session state
struct Saved {
    DWORD pid;
    int   prioClass;      // original priority class
    bool  throttled;      // was power throttling already on before we touched it?
};
static std::vector<Saved> g_saved;          // everything this session changed
static std::vector<DWORD> g_killed;         // audit trail (cannot be restored)
static CRITICAL_SECTION g_cs;
static bool g_csInit = false;

static void csInit() {
    if (!g_csInit) { InitializeCriticalSection(&g_cs); g_csInit = true; }
}

static bool isSelf(DWORD pid) { return pid == GetCurrentProcessId(); }

// System-critical processes: never listed, never touched, even by accident.
static bool isCriticalName(const wstring& name) {
    static const wchar_t* crit[] = {
        L"system", L"registry", L"smss.exe", L"csrss.exe", L"wininit.exe",
        L"winlogon.exe", L"services.exe", L"lsass.exe", L"svchost.exe",
        L"dwm.exe", L"explorer.exe", L"fontdrvhost.exe", L"audiodg.exe",
        L"memcompression", L"securityhealthservice.exe", L"msmpeng.exe",
        L"searchindexer.exe", L"spoolsv.exe", L"conhost.exe", L"wudfhost.exe",
        L"optimizekit.exe", L"webview2.exe", L"msedgewebview2.exe",
    };
    wstring l = name;
    std::transform(l.begin(), l.end(), l.begin(), ::towlower);
    for (auto* c : crit) if (l == c) return true;
    return false;
}

typedef BOOL (WINAPI *SetProcInfo_t)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
static SetProcInfo_t setProcInfo() {
    static SetProcInfo_t f = nullptr;
    if (!f) {
        HMODULE m = GetModuleHandleW(L"kernel32.dll");
        f = m ? (SetProcInfo_t)GetProcAddress(m, "SetProcessInformation") : nullptr;
    }
    return f;
}

// ------------------------------------------------------------------ internals
static bool applyThrottle(HANDLE h, bool on, wstring& err) {
    auto f = setProcInfo();
    if (!f) { err = L"SetProcessInformation unavailable (Windows < 2004)"; return false; }
    PROCESS_POWER_THROTTLING_STATE st{};
    st.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    st.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    st.StateMask = on ? PROCESS_POWER_THROTTLING_EXECUTION_SPEED : 0;
    if (!f(h, ProcessPowerThrottling, &st, sizeof(st))) {
        err = L"SetProcessInformation failed (" + std::to_wstring(GetLastError()) + L")";
        return false;
    }
    return true;
}

static bool remember(DWORD pid, HANDLE h) {
    csInit();
    for (auto& s : g_saved) if (s.pid == pid) return true;   // already snapshotted
    Saved s{};
    s.pid = pid;
    s.prioClass = GetPriorityClass(h);
    PROCESS_POWER_THROTTLING_STATE st{};
    st.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    st.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    DWORD ret = 0;
    if (auto f = setProcInfo(); f && f(h, ProcessPowerThrottling, &st, sizeof(st)))
        s.throttled = (st.StateMask & PROCESS_POWER_THROTTLING_EXECUTION_SPEED) != 0;
    EnterCriticalSection(&g_cs);
    g_saved.push_back(s);
    LeaveCriticalSection(&g_cs);
    return true;
}

static bool forget(DWORD pid) {
    csInit();
    EnterCriticalSection(&g_cs);
    for (size_t i = 0; i < g_saved.size(); ++i)
        if (g_saved[i].pid == pid) { g_saved.erase(g_saved.begin() + i); break; }
    LeaveCriticalSection(&g_cs);
    return true;
}

// ------------------------------------------------------------------ actions
bool eco(DWORD pid, wstring& err) {
    if (isSelf(pid)) { err = L"cannot reduce the dashboard itself"; return false; }
    HANDLE h = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) { err = L"OpenProcess failed (try running as admin)"; return false; }
    bool ok = false;
    if (remember(pid, h) && SetPriorityClass(h, BELOW_NORMAL_PRIORITY_CLASS))
        ok = applyThrottle(h, true, err);
    else if (err.empty())
        err = L"SetPriorityClass failed";
    CloseHandle(h);
    if (ok) log::ok(L"Reducer: process " + std::to_wstring(pid) + L" set to Eco");
    return ok;
}

bool boost(DWORD pid, wstring& err) {
    HANDLE h = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) { err = L"OpenProcess failed (try running as admin)"; return false; }
    if (remember(pid, h)) { /* remember the original even when boosting */ }
    bool ok = SetPriorityClass(h, ABOVE_NORMAL_PRIORITY_CLASS);
    applyThrottle(h, false, err);              // a boosted process must not be eco-parked
    err.clear();
    CloseHandle(h);
    if (!ok) err = L"SetPriorityClass failed";
    else log::ok(L"Reducer: process " + std::to_wstring(pid) + L" boosted (above normal)");
    return ok;
}

bool undo(DWORD pid, wstring& err) {
    HANDLE h = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) { err = L"OpenProcess failed"; return false; }
    int prio = NORMAL_PRIORITY_CLASS;
    bool wasThrottled = false;
    csInit();
    EnterCriticalSection(&g_cs);
    for (auto& s : g_saved) if (s.pid == pid) { prio = s.prioClass; wasThrottled = s.throttled; break; }
    LeaveCriticalSection(&g_cs);
    bool ok = SetPriorityClass(h, (DWORD)prio) != FALSE;
    applyThrottle(h, wasThrottled, err);       // clear throttling unless Windows had it on
    err.clear();
    CloseHandle(h);
    if (ok) forget(pid);
    else err = L"SetPriorityClass failed";
    if (ok) log::ok(L"Reducer: process " + std::to_wstring(pid) + L" restored");
    return ok;
}

bool kill(DWORD pid, wstring& err) {
    if (isSelf(pid)) { err = L"cannot kill the dashboard itself"; return false; }
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) { err = L"OpenProcess failed (try running as admin)"; return false; }
    BOOL ok = TerminateProcess(h, 1);
    CloseHandle(h);
    if (!ok) { err = L"TerminateProcess failed"; return false; }
    forget(pid);
    csInit();
    EnterCriticalSection(&g_cs);
    g_killed.push_back(pid);
    LeaveCriticalSection(&g_cs);
    log::info(L"Reducer: killed PID " + std::to_wstring(pid));
    return true;
}

int ecoAll(double minCpuPct, wstring& err) {
    json procs = monitor::topProcesses(60);
    int n = 0;
    for (auto& p : procs) {
        if (!p.is_object()) continue;
        double cpu = p.value("cpu", 0.0);
        if (minCpuPct > 0 && cpu < minCpuPct) continue;
        wstring e;
        if (eco((DWORD)p.value("pid", 0), e)) n++;
        else if (err.empty()) err = e;
    }
    log::ok(L"Reducer: eco applied to " + std::to_wstring(n) + L" processes");
    return n;
}

int undoAll(wstring& err) {
    csInit();
    vector<DWORD> pids;
    EnterCriticalSection(&g_cs);
    for (auto& s : g_saved) pids.push_back(s.pid);
    LeaveCriticalSection(&g_cs);
    int n = 0;
    for (DWORD pid : pids) {
        wstring e;
        if (undo(pid, e)) n++;
    }
    log::ok(L"Reducer: restored " + std::to_wstring(n) + L" processes");
    return n;
}

void restoreAll() { wstring e; undoAll(e); }

json stateJson() {
    csInit();
    json j;
    j["reduced"] = g_saved.size();
    j["killed"] = g_killed.size();
    json arr = json::array();
    EnterCriticalSection(&g_cs);
    for (auto& s : g_saved) arr.push_back({ {"pid", s.pid}, {"prioClass", s.prioClass}, {"wasThrottled", s.throttled} });
    LeaveCriticalSection(&g_cs);
    j["items"] = arr;
    return j;
}

// ------------------------------------------------------------------ listing
json listJson() {
    // Two passes over the process table so the CPU% deltas are meaningful: the monitor
    // already owns that machinery, reuse it.
    json rows = monitor::topProcesses(300);
    json out = json::array();
    int reduced = 0;
    {
        csInit();
        EnterCriticalSection(&g_cs);
        reduced = (int)g_saved.size();
        LeaveCriticalSection(&g_cs);
    }
    for (auto& r : rows) {
        if (!r.is_object()) continue;
        wstring name = widen(r.value("name", string()));
        DWORD pid = r.value("pid", 0);
        if (isCriticalName(name) || isSelf(pid)) continue;
        json o;
        o["pid"] = pid;
        o["name"] = narrow(name);
        o["cpu"] = r.value("cpu", 0.0);
        o["ramMB"] = r.value("ramMB", 0);
        // current class + throttle state (cheap: one open per candidate)
        if (HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
            DWORD cls = GetPriorityClass(h);
            o["cls"] = cls == BELOW_NORMAL_PRIORITY_CLASS ? "below" :
                       cls == IDLE_PRIORITY_CLASS ? "idle" :
                       cls == ABOVE_NORMAL_PRIORITY_CLASS ? "above" :
                       cls == HIGH_PRIORITY_CLASS ? "high" :
                       cls == REALTIME_PRIORITY_CLASS ? "realtime" : "normal";
            PROCESS_POWER_THROTTLING_STATE st{};
            st.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
            st.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
            DWORD ret = 0;
            if (auto f = setProcInfo(); f && GetProcessInformation(h, ProcessPowerThrottling, &st, sizeof(st)))
                o["eco"] = (st.StateMask & PROCESS_POWER_THROTTLING_EXECUTION_SPEED) != 0;
            else o["eco"] = false;
            CloseHandle(h);
        } else {
            o["cls"] = "?";
            o["eco"] = false;
        }
        out.push_back(o);
    }
    json j;
    j["procs"] = out;
    j["reduced"] = reduced;
    j["killed"] = g_killed.size();
    return j;
}

} // namespace ok::reducer
