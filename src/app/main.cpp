// OptimizeKit - GUI-subsystem entry point.
// No arguments -> liquid glass dashboard.
// With arguments (from OptimizeKit-cli.bat) -> attaches to the parent console and runs the CLI.
#include "core/common.h"
#include "core/engine.h"
#include "ui/ui.h"
#include "app/cli.h"
#include <shellapi.h>
#include <iostream>
#include <fcntl.h>
#include <io.h>
#include <cstdio>

using namespace ok;

static void attachParentConsole() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    // stdout redirected to a pipe/file (PowerShell, CI, `> file`) -> the MinGW CRT
    // already wired fd 1/2 to the inherited handles: leave everything untouched.
    if (h && h != INVALID_HANDLE_VALUE && !GetConsoleMode(h, &mode)) return;
    // launched from a real console (cmd / .bat): the GUI CRT needs manual wiring
    if (!h || h == INVALID_HANDLE_VALUE) {
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) return; // double-clicked: no console, give up silently
    }
    FILE* f1 = nullptr; FILE* f2 = nullptr; FILE* f3 = nullptr;
    if (freopen_s(&f1, "CONOUT$", "w", stdout) == 0) _setmode(_fileno(stdout), _O_U16TEXT);
    if (freopen_s(&f2, "CONOUT$", "w", stderr) == 0) _setmode(_fileno(stderr), _O_U16TEXT);
    if (freopen_s(&f3, "CONIN$",  "r", stdin)  == 0) _setmode(_fileno(stdin),  _O_U16TEXT);
}

static std::vector<wstring> getArgs() {
    std::vector<wstring> out;
    int n = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &n);
    if (argv) {
        for (int i = 1; i < n; ++i) out.push_back(argv[i]);
        LocalFree(argv);
    }
    return out;
}

static void printHelp() {
    std::wcout <<
        L"OptimizeKit v1.0 - Windows Optimization Suite\n"
        L"usage:\n"
        L"  OptimizeKit.exe                 open the liquid-glass dashboard\n"
        L"  OptimizeKit.exe --cli           numbered CLI menu (user or admin)\n"
        L"  OptimizeKit.exe --profile gaming|privacy|full|clean\n"
        L"  OptimizeKit.exe --apply <tweak-id>\n"
        L"  OptimizeKit.exe --restore <tweak-id>\n"
        L"  OptimizeKit.exe --list          list tweak ids\n"
        L"  OptimizeKit.exe --clean         junk cleanup\n"
        L"  OptimizeKit.exe --info          system summary\n"
        L"  OptimizeKit.exe --ping <host>   latency test\n";
}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    auto args = getArgs();

    if (args.empty()) {
        return ui::runDashboard();
    }

    attachParentConsole();
    const wstring& a1 = args[0];
    wstring a2 = args.size() > 1 ? args[1] : L"";

    int ret = 0;
    if (a1 == L"--help" || a1 == L"-h" || a1 == L"/?") {
        printHelp();
    }
    else if (a1 == L"--cli") {
        ret = cli::run(isAdmin());
    }
    else if (a1 == L"--list") {
        for (auto& t : tweaks::catalog()) {
            wchar_t line[200];
            swprintf(line, 200, L"%-22ls %-6ls impact=%d  %ls\n",
                     widen(t.id).c_str(), t.admin ? L"admin" : L"user", t.impact, t.name.c_str());
            std::wcout << line;
        }
    }
    else if (a1 == L"--info") {
        auto si = sysinfo::collect();
        auto d = drivers::collect();
        std::wcout << L"OS     : " << si.osName << L" build " << si.osBuild << L" (" << si.osArch << L")\n";
        std::wcout << L"CPU    : " << si.cpuName << L" [" << si.cpuCores << L" threads]\n";
        std::wcout << L"GPU    : " << (si.gpuName.empty() ? d.gpuName : si.gpuName) << L"\n";
        std::wcout << L"Driver : " << (d.driverVersion.empty() ? L"unknown" : d.driverVersion) << L"\n";
        std::wcout << L"RAM    : " << fmtBytes(si.ramTotal) << L" total / " << fmtBytes(si.ramAvail) << L" avail\n";
        std::wcout << L"Plan   : " << sysinfo::activePowerPlanName() << L"\n";
        std::wcout << L"Admin  : " << (isAdmin() ? L"yes" : L"no") << L"\n";
    }
    else if (a1 == L"--ping") {
        auto r = ping::measure(a2.empty() ? L"1.1.1.1" : a2);
        if (r.ok) std::wcout << a2 << L" : " << r.ms << L" ms\n";
        else { std::wcout << a2 << L" : " << r.status << L"\n"; ret = 2; }
    }
    else if (a1 == L"--clean") {
        auto freed = cleaner::purge();
        cleaner::emptyRecycleBin();
        std::wcout << L"freed " << fmtBytes(freed) << L"\n";
    }
    else if (a1 == L"--apply" && !a2.empty()) {
        wstring e;
        bool r = tweaks::apply(narrow(a2), e);
        std::wcout << (r ? L"applied " : L"FAILED ") << a2 << (r ? L"" : L" : " + e) << L"\n";
        ret = r ? 0 : 1;
    }
    else if (a1 == L"--restore" && !a2.empty()) {
        wstring e;
        bool r = tweaks::restore(narrow(a2), e);
        std::wcout << (r ? L"restored " : L"FAILED ") << a2 << (r ? L"" : L" : " + e) << L"\n";
        ret = r ? 0 : 1;
    }
    else if (a1 == L"--profile") {
        if (a2.empty()) a2 = L"gaming";
        auto rep = engine::runProfile(narrow(a2));
        std::wcout << rep.summary << L"\n";
        for (auto& e : rep.errors) std::wcout << L"  [!] " << e << L"\n";
        ret = rep.failed ? 1 : 0;
    }
    else {
        printHelp();
        ret = 1;
    }

    // give the .bat time to show the output when it ends with pause
    fflush(stdout);
    return ret;
}
