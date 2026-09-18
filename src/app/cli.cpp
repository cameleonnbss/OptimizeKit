// OptimizeKit - numbered CLI (choice by digits), used by the .bat launchers
#include "cli.h"
#include "engine.h"
#include <iostream>
#include <map>
#include <set>
#include <cstdlib>
#include <fcntl.h>
#include <io.h>

namespace ok::cli {

// ---------- console colors ----------
static HANDLE hCon() { return GetStdHandle(STD_OUTPUT_HANDLE); }
static void setColor(WORD attr) { SetConsoleTextAttribute(hCon(), attr); }
static const WORD A_DEF = 7, A_ACC = 11 /*cyan*/, A_OK = 10 /*green*/, A_WARN = 14, A_ERR = 12, A_DIM = 8, A_MAG = 13;

static void title(const wstring& s) {
    setColor(A_ACC);
    std::wcout << L"\n== " << s << L" ==" << std::endl;
    setColor(A_DEF);
}
static void line(const wstring& s) { std::wcout << s << std::endl; }
static void ok(const wstring& s)   { setColor(A_OK);   std::wcout << L"  [OK] "   << s << std::endl; setColor(A_DEF); }
static void err(const wstring& s)  { setColor(A_ERR);  std::wcout << L"  [!!] "   << s << std::endl; setColor(A_DEF); }
static void dim(const wstring& s)  { setColor(A_DIM);  std::wcout << L"  "        << s << std::endl; setColor(A_DEF); }

static int ask(const wstring& prompt, int lo, int hi) {
    for (;;) {
        setColor(A_WARN);
        std::wcout << prompt << L" [" << lo << L"-" << hi << L"]: ";
        setColor(A_DEF);
        wstring s;
        if (!(std::wcin >> s)) return lo;
        int v = _wtoi(s.c_str());
        if (v >= lo && v <= hi) return v;
    }
}

static void pauseEnter() {
    setColor(A_DIM);
    std::wcout << L"\n  Press ENTER to continue...";
    setColor(A_DEF);
    std::wcin.ignore();
    std::wstring dummy; std::getline(std::wcin, dummy);
}

static void printHeader() {
    system("cls");
    setColor(A_MAG);
    std::wcout << L"\n  ██████╗ ██████╗ ████████╗██╗███╗   ███╗██╗███████╗███████╗██╗  ██╗██╗████████╗\n";
    std::wcout << L"  ██╔══██╗██╔══██╗╚══██╔══╝██║████╗ ████║██║██╔════╝██╔════╝╚██╗██╔╝██║╚══██╔══╝\n";
    std::wcout << L"  ██████╔╝██████╔╝   ██║   ██║██╔████╔██║██║█████╗  █████╗   ╚███╔╝ ██║   ██║\n";
    std::wcout << L"  ██╔═══╝ ██╔══██╗   ██║   ██║██║╚██╔╝██║██║██╔══╝  ██╔══╝   ██╔██╗ ██║   ██║\n";
    std::wcout << L"  ██║     ██║  ██║   ██║   ██║██║ ╚═╝ ██║██║███████╗███████╗██╔╝ ██╗██║   ██║\n";
    std::wcout << L"  ╚═╝     ╚═╝  ╚═╝   ╚═╝   ╚═╝╚═╝     ╚═╝╚═╝╚══════╝╚══════╝╚═╝  ╚═╝╚═╝   ╚═╝\n";
    setColor(A_DEF);
    setColor(A_DIM);
    std::wcout << L"  Windows Optimization Suite v1.0 — native C++ engine + PowerShell\n";
    std::wcout << L"  Tweaks curated from Chris Titus Tech WinUtil (MIT) & PC-gaming community\n";
    setColor(A_DEF);
    if (isAdmin()) { setColor(A_OK); std::wcout << L"  [ running as ADMINISTRATOR — all tweaks available ]"  << std::endl; }
    else            { setColor(A_WARN); std::wcout << L"  [ standard user — " << tweaks::adminCount() << L" admin tweaks locked (use OptimizeKit-admin.bat) ]" << std::endl; }
    setColor(A_DEF);
}

// ---------------------------------------------------------- actions
static void doProfile(const string& name) {
    title(L"Running profile: " + widen(name));
    auto rep = engine::runProfile(name);
    for (auto& e : rep.errors) err(e);
    ok(rep.summary);
    dim(L"Log: " + logPath());
}

static void doBackup() {
    title(L"Registry backup");
    wstring f = tweaks::backupAll();
    ok(L"backups written under " + appDataDir());
    dim(L"first file: " + f);
}

static void listTweaksTable(bool onlyAdmin) {
    const auto& cat = tweaks::catalog();
    int i = 1;
    for (auto& t : cat) {
        if (onlyAdmin && !t.admin) continue;
        auto st = tweaks::checkState(t.id);
        std::wcout << L"  " << (i < 10 ? L" " : L"") << i << L". " << t.name << L"\n";
        dim(L"      " + t.desc);
        wstring tag = t.admin ? L"admin" : L"user ";
        setColor(t.admin ? A_WARN : A_OK);
        std::wcout << L"      [" << tag << L"]";
        setColor(A_DIM);
        std::wcout << L"  impact ";
        for (int k = 0; k < t.impact; ++k) std::wcout << L"*";
        std::wcout << L"  - " << t.source << std::endl;
        setColor(A_DEF);
        ++i;
    }
}

static void doApplyOne() {
    title(L"Apply a single tweak");
    listTweaksTable(false);
    const auto& cat = tweaks::catalog();
    int v = ask(L"  Number to apply (0=cancel)", 0, (int)cat.size());
    if (v == 0) return;
    wstring e;
    if (tweaks::apply(cat[v - 1].id, e)) ok(cat[v - 1].name + L" applied");
    else err(e);
}

static void doRestoreOne() {
    title(L"Restore Windows default");
    listTweaksTable(false);
    const auto& cat = tweaks::catalog();
    int v = ask(L"  Number to restore (0=cancel)", 0, (int)cat.size());
    if (v == 0) return;
    wstring e;
    if (tweaks::restore(cat[v - 1].id, e)) ok(cat[v - 1].name + L" restored");
    else err(e);
}

static void doSelectMulti() {
    title(L"Select tweaks (numbers, comma-separated; a=all / n=none / d=done)");
    const auto& cat = tweaks::catalog();
    using json = nlohmann::json;
    json selObj = json::object();
    std::map<string, bool> sel;
    std::set<int> chosen;
    for (;;) {
        int i = 1;
        for (auto& t : cat) {
            setColor(sel[t.id] ? A_OK : A_DIM);
            std::wcout << L"  " << i << L". " << (sel[t.id] ? L"[x] " : L"[ ] ") << t.name << std::endl;
            setColor(A_DEF);
            ++i;
        }
        std::wcout << L"  Toggle which number (or a / n / d): ";
        wstring s;
        if (!(std::wcin >> s)) return;
        if (s == L"d" || s == L"D") break;
        if (s == L"a" || s == L"A") { for (auto& t : cat) sel[t.id] = true; continue; }
        if (s == L"n" || s == L"N") { for (auto& t : cat) sel[t.id] = false; continue; }
        int v = _wtoi(s.c_str());
        if (v >= 1 && v <= (int)cat.size()) sel[cat[v - 1].id] = !sel[cat[v - 1].id];
    }
    json j = selObj;
    for (auto& t : cat) if (sel[t.id]) j[t.id] = true;
    std::vector<wstring> errs;
    int n = tweaks::applyMany(j, errs);
    for (auto& e : errs) err(e);
    ok(std::to_wstring(n) + L" tweaks applied");
}

static void doPingMenu() {
    title(L"Network latency test");
    auto rows = engine::pingTargets();
    for (auto& r : rows) {
        wstring name = widen(r["name"].get<string>());
        wstring host = widen(r["host"].get<string>());
        bool okr = r["ok"].get<bool>();
        int ms = r["ms"].get<int>();
        setColor(okr ? (ms < 30 ? A_OK : ms < 80 ? A_WARN : A_ERR) : A_ERR);
        std::wcout << L"  " << name << L" (" << host << L") : "
                   << (okr ? std::to_wstring(ms) + L" ms" : L"timeout") << std::endl;
        setColor(A_DEF);
    }
    dim(L"Add more targets from the GUI (Network tab) or config.json");
}

static void doPingInput() {
    title(L"Quick latency test");
    std::wcin.ignore();
    std::wcout << L"  Host or IP to ping: ";
    wstring host;
    std::getline(std::wcin, host);
    if (host.empty()) return;
    auto r = ping::measure(host);
    if (r.ok) ok(host + L" = " + std::to_wstring(r.ms) + L" ms (" + r.status + L")");
    else err(host + L": " + r.status);
}

static void doGameBoost() {
    title(L"Running processes — set priority");
    auto procs = gameboost::listProcesses();
    int i = 1;
    for (auto& p : procs) {
        if (p.pid <= 4) continue;
        std::wcout << L"  " << (i < 10 ? L" " : L"") << i << L". " << p.name << L"  (PID " << p.pid << L")" << std::endl;
        ++i;
        if (i > 60) break;
    }
    int v = ask(L"  Process number (0=cancel)", 0, (int)i);
    if (v == 0) return;
    int lvl = ask(L"  Priority 4=Above normal 5=High 6=Realtime", 4, 6);
    wstring e;
    if (gameboost::setPriority(procs[v - 1].pid, lvl, e)) ok(procs[v - 1].name + L" boosted");
    else err(e);
}

static void doClean() {
    title(L"Junk cleanup");
    auto rep = cleaner::measure();
    for (auto& e : rep.entries)
        dim(e.label + L" : " + fmtBytes(e.bytes));
    setColor(A_WARN);
    std::wcout << L"\n  Total reclaimable: " << fmtBytes(rep.total) << std::endl;
    setColor(A_DEF);
    int v = ask(L"  1=Clean now  2=Cancel", 1, 2);
    if (v == 2) return;
    uint64_t freed = cleaner::purge();
    cleaner::emptyRecycleBin();
    ok(fmtBytes(freed) + L" freed");
}

static void doInfo() {
    title(L"System information");
    auto si = sysinfo::collect();
    line(L"  Computer : " + si.computerName + L" (" + si.userName + L")");
    line(L"  OS       : " + si.osName + L" build " + si.osBuild + L" " + si.osArch);
    line(L"  CPU      : " + si.cpuName + L" [" + std::to_wstring(si.cpuCores) + L" threads]");
    line(L"  RAM      : " + fmtBytes(si.ramTotal) + L" (" + fmtBytes(si.ramAvail) + L" free)");
    line(L"  GPU      : " + si.gpuName);
    auto d = drivers::collect();
    line(L"  Driver   : " + (d.driverVersion.empty() ? L"unknown" : d.driverVersion));
    line(L"  Power    : " + sysinfo::activePowerPlanName());
    line(wstring(L"  GameMode : ") + (sysinfo::queryGameMode() ? L"On" : L"Off"));
    line(wstring(L"  HAGS     : ") + (sysinfo::queryHags() ? L"On" : L"Off"));
    line(L"  Uptime   : " + si.uptime);
}

static void doDriversMenu() {
    title(L"Drivers");
    line(L"  1. Open GPU vendor driver page");
    line(L"  2. Open dxdiag");
    line(L"  3. Trigger Windows Update driver scan (admin)");
    line(L"  4. NVIDIA page");
    line(L"  5. AMD page");
    line(L"  6. Intel page");
    line(L"  0. Back");
    int v = ask(L"  Choice", 0, 6);
    switch (v) {
        case 1: drivers::openVendorPage(); break;
        case 2: drivers::openDxdiag(); break;
        case 3: drivers::scanWindowsUpdateDrivers(); break;
        case 4: shellOpen(L"https://www.nvidia.com/Download/index.aspx"); break;
        case 5: shellOpen(L"https://www.amd.com/en/support"); break;
        case 6: shellOpen(L"https://www.intel.com/content/www/us/en/download-center/home.html"); break;
        default: break;
    }
}

static void doLogs() {
    title(L"Log tail");
    wstring all = log::readAll();
    if (all.size() > 2400) all = L"…" + all.substr(all.size() - 2400);
    setColor(A_DIM);
    std::wcout << all << std::endl;
    setColor(A_DEF);
    dim(L"Full log: " + logPath());
}

// ---------------------------------------------------------- main menus
int runUserMenu() {
    log::init();
    for (;;) {
        printHeader();
        std::wcout << L"\n  -- USER MODE (no admin required) --\n" << std::endl;
        std::wcout << L"  1. System information" << std::endl;
        std::wcout << L"  2. List available tweaks" << std::endl;
        std::wcout << L"  3. Apply a single tweak" << std::endl;
        std::wcout << L"  4. Restore Windows default for a tweak" << std::endl;
        std::wcout << L"  5. Quick latency test (ping any host)" << std::endl;
        std::wcout << L"  6. Activity log" << std::endl;
        std::wcout << L"  7. Registry backup (recommended first)" << std::endl;
        std::wcout << L"  0. Exit\n" << std::endl;
        int v = ask(L"  Choice", 0, 7);
        switch (v) {
            case 1: doInfo(); pauseEnter(); break;
            case 2: listTweaksTable(false); pauseEnter(); break;
            case 3: doApplyOne(); pauseEnter(); break;
            case 4: doRestoreOne(); pauseEnter(); break;
            case 5: doPingInput(); pauseEnter(); break;
            case 6: doLogs(); pauseEnter(); break;
            case 7: doBackup(); pauseEnter(); break;
            case 0: return 0;
        }
    }
}

int runAdminMenu() {
    log::init();
    for (;;) {
        printHeader();
        std::wcout << L"\n  -- ADMIN MODE (full power) --\n" << std::endl;
        std::wcout << L"  1. System information" << std::endl;
        std::wcout << L"  2. Apply GAMING profile (one click)" << std::endl;
        std::wcout << L"  3. Apply PRIVACY profile" << std::endl;
        std::wcout << L"  4. Apply FULL kit profile" << std::endl;
        std::wcout << L"  5. Select multiple tweaks (numbers, a/n/d)" << std::endl;
        std::wcout << L"  6. Apply or restore a single tweak" << std::endl;
        std::wcout << L"  7. GAME BOOST : boost a running process priority" << std::endl;
        std::wcout << L"  8. NETWORK : saved targets latency test" << std::endl;
        std::wcout << L"  9. CLEAN : junk cleanup + recycle bin" << std::endl;
        std::wcout << L" 10. DRIVERS menu" << std::endl;
        std::wcout << L" 11. Registry backup" << std::endl;
        std::wcout << L" 12. Activity log" << std::endl;
        std::wcout << L"  0. Exit\n" << std::endl;
        int v = ask(L"  Choice", 0, 12);
        switch (v) {
            case 1: doInfo(); pauseEnter(); break;
            case 2: doProfile("gaming");  pauseEnter(); break;
            case 3: doProfile("privacy"); pauseEnter(); break;
            case 4: doProfile("full");    pauseEnter(); break;
            case 5: doSelectMulti(); pauseEnter(); break;
            case 6: {
                int w = ask(L"  1=Apply  2=Restore  0=back", 0, 2);
                if (w == 1) doApplyOne();
                if (w == 2) doRestoreOne();
                pauseEnter(); break;
            }
            case 7: doGameBoost(); pauseEnter(); break;
            case 8: doPingMenu(); pauseEnter(); break;
            case 9: doClean(); pauseEnter(); break;
            case 10: doDriversMenu(); pauseEnter(); break;
            case 11: doBackup(); pauseEnter(); break;
            case 12: doLogs(); pauseEnter(); break;
            case 0: return 0;
        }
    }
}

} // namespace ok::cli
