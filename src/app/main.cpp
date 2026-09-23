// OptimizeKit - entry point.
// No arguments -> the NATIVE Direct2D liquid-glass dashboard in a real Win32
//                window. No Edge, no browser, no port, no extra process: one
//                exe, one window, one loopback listener for the UI data only.
// --app         -> (v2.6 behaviour) WebView2 frame window hosting the web
//                dashboard; falls back to msedge --app, then the default browser.
// With arguments (from OptimizeKit-cli.bat) -> attaches to the parent console and runs the CLI.
#include "core/common.h"
#include "core/engine.h"
#include "core/firmware.h"
#include "core/drvupdate.h"
#include "ui/ui.h"
#include "ui/webframe.h"
#include "app/cli.h"
#include "server/server.h"
#include <shellapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <fcntl.h>
#include <io.h>
#include <cstdio>

using namespace ok;
using json = nlohmann::json;

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
        L"OptimizeKit v2.7 - Windows Gaming & Performance Control Center\n"
        L"usage:\n"
        L"  OptimizeKit.exe                 native Direct2D window (default, no browser)\n"
        L"  OptimizeKit.exe --app           WebView2 frame window (web dashboard)\n"
        L"  OptimizeKit.exe --web [port]    serve the dashboard without opening a window\n"
        L"  OptimizeKit.exe --cli           numbered CLI menu (user or admin)\n"
        L"  OptimizeKit.exe --profile gaming|privacy|full|clean\n"
        L"  OptimizeKit.exe --apply <tweak-id>\n"
        L"  OptimizeKit.exe --restore <tweak-id>\n"
        L"  OptimizeKit.exe --list          list tweak ids\n"
        L"  OptimizeKit.exe --clean         junk cleanup\n"
        L"  OptimizeKit.exe --info          system summary\n"
        L"  OptimizeKit.exe --firmware      BIOS / SecureBoot / TPM / kernel report\n"
        L"  OptimizeKit.exe --drvupdate     driver age report + Windows Update scan\n"
        L"  OptimizeKit.exe --ping <host>   latency test\n";
}

static string g_probeBuf;

static bool openAppWindow(const wstring& url) {
    // msedge --app gives the WormGPT-like chromeless window when available
    if (runCapture(L"reg query HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\msedge.exe /ve", g_probeBuf, 8000)) {
        ShellExecuteW(nullptr, L"open",
            L"msedge.exe",
            (L"--app=" + url + L" --window-size=1280,860").c_str(), nullptr, SW_SHOWNORMAL);
        return true;
    }
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return true;
}

// probe 127.0.0.1:port..port+20 until one accepts a TCP connection (server ready)
static int probeServer(int port) {
    WSADATA wd; WSAStartup(MAKEWORD(2, 2), &wd);
    for (int attempt = 0; attempt < 60; ++attempt) {
        for (int p = port; p < port + 20; ++p) {
            SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
            if (s == INVALID_SOCKET) continue;
            sockaddr_in a{};
            a.sin_family = AF_INET;
            a.sin_port = htons((u_short)p);
            a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            if (connect(s, (sockaddr*)&a, sizeof(a)) == 0) { closesocket(s); return p; }
            closesocket(s);
        }
        Sleep(100);
    }
    return -1;
}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    auto args = getArgs();

    if (args.empty() || args[0] == L"--native") {
        // v2.7 default: the NATIVE Direct2D dashboard. One exe, one Win32 window,
        // zero browser components - the dashboard is drawn by Direct2D/DirectWrite.
        return ui::runDashboard();
    }

    if (args[0] == L"--app") {
        // legacy v2.6 behaviour kept for users who prefer the web surface:
        // a WebView2 frame hosting the embedded dashboard.
        std::thread srv([] { ok::server::serve(8765); });
        int port = probeServer(8765);
        if (port > 0) {
            wchar_t url[64]; swprintf(url, 64, L"http://127.0.0.1:%d", port);
            if (webframe::runWindow(wstring(url)) != 0)
                openAppWindow(url); // WebView2 unavailable -> browser app window
        }
        ok::server::stop();
        srv.join();
        return 0;
    }

    if (args[0] == L"--web") {
        unsigned short port = 8765;
        if (args.size() > 1) port = (unsigned short)_wtoi(args[1].c_str());
        return ok::server::serve(port) < 0 ? 2 : 0;
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
    else if (a1 == L"--firmware") {
        firmware::FwInit();
        json f = firmware::inventory();
        std::wcout << L"BIOS        : " << widen(f.value("biosVendor", std::string("?")))
                   << L"  " << widen(f.value("biosVersion", std::string("?")))
                   << L"  (" << widen(f.value("biosDate", std::string("?"))) << L")\n";
        std::wcout << L"Board       : " << widen(f.value("motherboard", std::string("?"))) << L"\n";
        std::wcout << L"Boot mode   : " << widen(f.value("bootMode", std::string("?")))
                   << L"   SecureBoot: " << widen(f.value("secureBoot", std::string("?"))) << L"\n";
        std::wcout << L"TPM         : " << widen(f["tpm"].value("version", std::string("n/a")))
                   << L"  (" << (f["tpm"].value("present", false) ? L"present" : L"absent") << L")\n";
        std::wcout << L"VT-x / SVM  : " << widen(f.value("virtualization", std::string("?")))
                   << L"   Hypervisor running: " << (f.value("hypervisorRunning", false) ? L"yes" : L"no") << L"\n";
        std::wcout << L"HPET        : " << widen(f.value("hpet", std::string("?")))
                   << L"   WPBT: " << widen(f.value("wpbt", std::string("?")))
                   << L"   dynamicTick: " << widen(f.value("dynamicTick", std::string("?"))) << L"\n";
        std::wcout << L"Standby     : " << widen(f.value("modernStandby", std::string("?")))
                   << L"   PatchGuard: " << widen(f.value("patchGuard", std::string("?"))) << L"\n";
        std::wcout << L"Reboot pending: " << (f.value("rebootNeeded", false) ? L"yes" : L"no") << L"\n";
    }
    else if (a1 == L"--drvupdate") {
        drvupdate::DrvInit();
        json r = drvupdate::report();
        auto printDev = [](const json& d) {
            wstring line = L"  " + widen(d.value("name", std::string("?")))
                + L"  v" + widen(d.value("version", std::string("?")))
                + L"  [" + widen(d.value("date", std::string("?"))) + L"]";
            if (d.contains("ageDays") && !d["ageDays"].is_null())
                line += L"  " + std::to_wstring(d["ageDays"].get<long long>()) + L" days (" + widen(d.value("age", std::string("?"))) + L")";
            std::wcout << line << L"\n";
        };
        std::wcout << L"GPU:\n";    printDev(r["gpu"]);
        std::wcout << L"Audio:\n"; for (auto& d : r["audio"]) printDev(d);
        std::wcout << L"Network:\n"; for (auto& d : r["net"]) printDev(d);
        auto probs = drvupdate::problemDevices();
        std::wcout << L"Devices with a problem: " << probs.size() << L"\n";
        for (auto& d : probs)
            std::wcout << L"  [!] " << widen(d.value("name", std::string("?")))
                       << L" (code " << d.value("problem", 0) << L")\n";
        std::wcout << L"Triggering the Windows Update driver scan...\n";
        drvupdate::scanWindowsUpdate(true);
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
