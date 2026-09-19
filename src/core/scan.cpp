// OptimizeKit - full system scanner: finds what can be optimized on THIS pc.
// Every finding references a real action id (tweak id, cleaner target, netprofile toggle...)
// so "apply suggestion" never invents anything: it calls the same engine as the UI.
#include "scan.h"
#include "engine.h"
#include "tweaks.h"
#include "cleaner.h"
#include "netprofile.h"
#include "monitor.h"
#include "drivers.h"
#include "sysinfo.h"
#include "games.h"
#include "ram.h"
#include <filesystem>
#include <fstream>
#include <shellapi.h>

namespace fs = std::filesystem;

namespace ok::scan {

using json = nlohmann::json;

// -------------------------------------------------------------- one scan
static void addFinding(json& out, const wstring& id, const wstring& title,
                       const wstring& detail, const wstring& severity,
                       const string& actionType, const string& actionId,
                       const wstring& impact) {
    json f;
    f["id"]       = narrow(id);
    f["title"]    = narrow(title);
    f["detail"]   = narrow(detail);
    f["severity"] = narrow(severity);   // info | low | medium | high
    f["impact"]   = narrow(impact);
    f["action"]   = { {"type", actionType}, {"id", actionId} };
    out["findings"].push_back(f);
}

json runScan() {
    log::section(L"SYSTEM SCAN");
    json out;
    out["findings"] = json::array();
    out["startedAt"] = (long long)time(nullptr);

    auto si = sysinfo::collect();

    // ---------------- storage junk ----------------
    auto junk = cleaner::measure();
    if (junk.total > 50ull * 1024 * 1024) {
        wstring detail = fmtBytes(junk.total) + L" of junk files";
        addFinding(out, L"junk", L"Temporary files are piling up", detail, L"medium",
                   "clean", "all", L"Frees disk space");
    } else {
        addFinding(out, L"junk-ok", L"Temporary files under control",
                   fmtBytes(junk.total) + L" of junk found", L"info", "none", "", L"");
    }

    // ---------------- tweak state ----------------
    for (auto& t : tweaks::catalog()) {
        auto st = tweaks::checkState(t.id);
        if (st.applied) continue;
        if (t.id == "onedrive_off") continue;         // too intrusive to suggest blindly
        wstring sev = (t.impact >= 3) ? L"high" : (t.impact == 2 ? L"medium" : L"low");
        wstring fid = wstring(L"tw-") + widen(t.id);
        addFinding(out, fid, t.name, t.desc, sev, "tweak", t.id,
                   t.admin ? L"Admin · reversible" : L"User · reversible");
    }

    // ---------------- network ----------------
    auto ns = netprofile::status();
    if (ns.autotuning == L"disabled")
        addFinding(out, L"net-auto", L"TCP autotuning is disabled",
                   L"This throttles downloads on fast connections. Restore 'normal'.", L"medium",
                   "net", "autotuning_normal", L"Download speed");
    if (ns.rscEnabled)
        addFinding(out, L"net-rsc", L"Receive Segment Coalescing is ON",
                   L"RSC batches network packets and adds latency on some Realtek NICs. Disable it for gaming.", L"low",
                   "net", "rsc_off", L"Latency");
    auto ads = netprofile::adapters();
    for (auto& a : ads) {
        if (a.type != L"Wi-Fi") continue;
        addFinding(out, L"net-wifi", L"Wi-Fi adapter detected",
                   a.name + L" — for competitive gaming prefer Ethernet.", L"info", "none", "", L"");
    }

    // ---------------- power plan ----------------
    wstring plan = sysinfo::activePowerPlanName();
    if (plan.find(L"Ultimate") == wstring::npos && plan.find(L"High performance") == wstring::npos) {
        addFinding(out, L"power", L"Power plan is not performance-oriented",
                   L"Active plan: " + plan + L". The Ultimate Performance plan keeps CPU clocks high.", L"medium",
                   "tweak", "power_ultimate", L"CPU responsiveness");
    }

    // ---------------- game mode / HAGS ----------------
    if (!sysinfo::queryGameMode())
        addFinding(out, L"gm", L"Game Mode is disabled",
                   L"Game Mode gives the game process CPU/GPU priority while playing.", L"low",
                   "tweak", "game_mode", L"Frame stability");
    if (!sysinfo::queryHags())
        addFinding(out, L"hags", L"Hardware-accelerated GPU scheduling is OFF",
                   L"HAGS lowers latency on modern GPUs (RTX/RDNA+). Requires reboot.", L"low",
                   "tweak", "hags_on", L"GPU latency");

    // ---------------- startup ----------------
    auto su = startup::listEntries();
    if (su.size() > 10)
        addFinding(out, L"startup", std::to_wstring(su.size()) + L" startup entries",
                   L"A heavy startup list slows boot and fills RAM. Review the Startup page.", L"medium",
                   "none", "", L"Boot time, RAM");

    // ---------------- processes ----------------
    auto s = monitor::sampleOnce();
    if (s.procCount > 220)
        addFinding(out, L"procs", std::to_wstring(s.procCount) + L" processes running",
                   L"Typical range is 120-180. Check the RAM page for the biggest consumers.", L"info",
                   "none", "", L"");

    // ---------------- driver ----------------
    auto drv = drivers::collect();
    if (drv.gpuName.empty())
        addFinding(out, L"drv-gpu", L"GPU not identified",
                   L"Could not read the display driver from the registry.", L"info", "none", "", L"");
    else
        addFinding(out, L"drv-gpu-ok", L"GPU driver detected",
                   drv.gpuName + L" — driver " + drv.driverVersion, L"info", "none", "", L"");

    // ---------------- games ----------------
    auto games = games::detect();
    if (games.empty())
        addFinding(out, L"games-none", L"No games detected",
                   L"Install a game via Steam/Epic/Xbox and rescan — per-game profiles unlock here.", L"info", "none", "", L"");
    else
        addFinding(out, L"games-ok", std::to_wstring(games.size()) + L" games detected",
                   L"Open the Games page to create per-game optimization profiles.", L"info", "none", "", L"");

    // summary
    int highs = 0, meds = 0;
    for (auto& f : out["findings"]) {
        if (f["severity"] == "high") highs++;
        else if (f["severity"] == "medium") meds++;
    }
    out["high"] = highs;
    out["medium"] = meds;
    wchar_t sum[96];
    swprintf(sum, 96, L"%d findings (%d high, %d medium)", (int)out["findings"].size(), highs, meds);
    out["summary"] = narrow(sum);
    log::ok(widen(out["summary"].get<string>()));
    return out;
}

} // namespace ok::scan
