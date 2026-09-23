// OptimizeKit - embedded HTTP server (cpp-httplib) serving the WormGPT-style dashboard
#include "server.h"
#include "engine.h"
#include "monitor.h"
#include "netprofile.h"
#include "scan.h"
#include "games.h"
#include "ram.h"
#include "storage.h"
#include "reducer.h"
#include "security.h"
#include "diskscope.h"
#include "firmware.h"
#include "drvupdate.h"
#include "logging2.h"
#include "diagnostics.h"
#include "webassets.h"
#include "httplib.h"
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace ok::server {

static httplib::Server* g_svr = nullptr;   // for stop() from the UI thread

void stop() {
    if (g_svr) g_svr->stop();
}

using json = nlohmann::json;

static wstring webRoot() {
    // exe-dir/web preferred, fallback to cwd/web (dev mode)
    wstring a = exeDir() + L"\\web";
    if (fs::exists(a)) return a;
    return L"web";
}

static bool readBin(const wstring& p, string& out) {
    std::ifstream f(p.c_str(), std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static string jstr(const json& j) { return j.is_string() ? j.get<string>() : j.dump(); }

// ------------------------------------------------------------------ routes
static json jsonState() {
    auto si = sysinfo::collect();
    auto drv = drivers::collect();
    json j;
    j["os"]      = narrow(si.osName + L" build " + si.osBuild + L" (" + si.osArch + L")");
    j["cpu"]     = narrow(si.cpuName);
    j["threads"] = si.cpuCores;
    j["gpu"]     = narrow(si.gpuName.empty() ? drv.gpuName : si.gpuName);
    j["gpuDriver"] = narrow(drv.driverVersion.empty() ? wstring() : drv.driverVersion);
    j["ramTotal"] = si.ramTotal;
    j["ramAvail"] = si.ramAvail;
    j["plan"]    = narrow(sysinfo::activePowerPlanName());
    j["gameMode"] = sysinfo::queryGameMode();
    j["hags"]     = sysinfo::queryHags();
    j["uptime"]  = narrow(si.uptime);
    j["admin"]   = isAdmin();
    j["user"]    = narrow(userName());
    j["gamingMode"] = games::gamingModeActive();
    return j;
}

static json jsonTweaks() {
    json arr = json::array();
    for (auto& t : tweaks::catalog()) {
        auto st = tweaks::checkState(t.id);
        arr.push_back({
            {"id", t.id},
            {"name", narrow(t.name)},
            {"desc", narrow(t.desc)},
            {"admin", t.admin},
            {"impact", t.impact},
            {"source", narrow(t.source)},
            {"applied", st.applied},
        });
    }
    return arr;
}

static json jsonPingAll() {
    json arr = json::array();
    json cfg = engine::loadConfig();                    // named local: iterating a temporary would dangle
    if (!cfg.contains("ping_targets") || !cfg["ping_targets"].is_array()) return arr;
    for (auto& t : cfg["ping_targets"]) {
        if (!t.is_object() || !t.contains("host")) continue;
        auto r = ping::measure(widen(t.value("host", "1.1.1.1")));
        arr.push_back({ {"name", t.value("name", "?")}, {"host", t.value("host", "?")},
                        {"ok", r.ok}, {"ms", r.ok ? (int)r.ms : -1} });
    }
    return arr;
}

int serve(unsigned short preferredPort) {
    httplib::Server svr;
    g_svr = &svr;
    atexit([]() { reducer::restoreAll(); });   // never leave a machine throttled
    // One shared COM apartment for the WMI-backed modules (firmware inventory, security scan)
    drvupdate::DrvInit();
    firmware::FwInit();

    // ------------- embedded dashboard fallback (exe works with zero files on disk) -------------
    const bool diskWeb = fs::exists(webRoot());
    if (diskWeb) {
        svr.set_mount_point("/", narrow(webRoot()));
    } else {
        log2::warn(L"SERVER", L"web/ folder not found next to the exe - serving the embedded dashboard");
        svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
            auto f = webassets::find("index.html");
            res.set_content((const char*)f->data, f->len, f->mime);
        });
        for (const auto& f : webassets::kFiles) {
            const string path = string("/") + f.path;
            svr.Get(path.c_str(), [&f](const httplib::Request&, httplib::Response& res) {
                res.set_content((const char*)f.data, f.len, f.mime);
            });
        }
    }

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"ok\":true}", "application/json");
    });

    // ---------------- system / monitoring ----------------
    svr.Get("/api/state", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(jsonState().dump(), "application/json");
    });

    svr.Get("/api/monitor", [](const httplib::Request&, httplib::Response& res) {
        auto s = monitor::sampleOnce();
        json j;
        j["cpu"] = s.cpuPercent;
        j["cpuClock"] = s.cpuClockMHz;
        j["ram"] = s.ramPercent;
        j["ramUsed"] = s.ramUsed;
        j["ramTotal"] = s.ramTotal;
        j["gpu"] = s.gpuPercent;
        j["disk"] = s.diskPercent;
        j["diskRead"] = s.diskReadMBs;
        j["diskWrite"] = s.diskWriteMBs;
        j["netDown"] = s.netDownKBs;
        j["netUp"] = s.netUpKBs;
        j["uptimeSec"] = s.uptimeSec;
        j["procs"] = s.procCount;
        j["threads"] = s.threadCount;
        json h;
        h["cpu"] = monitor::cpuHistory();
        h["ram"] = monitor::ramHistory();
        h["gpu"] = monitor::gpuHistory();
        j["hist"] = h;
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/processes", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(monitor::topProcesses(14).dump(), "application/json");
    });

    // ---------------- tweaks ----------------
    svr.Get("/api/tweaks", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(jsonTweaks().dump(), "application/json");
    });

    svr.Post("/api/tweaks/apply", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; res.set_content("{\"error\":\"bad json\"}", "application/json"); return; }
        vector<wstring> errs;
        int n = 0;
        if (body.is_array()) {
            for (auto& id : body) {
                wstring e;
                if (tweaks::apply(id.get<string>(), e)) n++;
                else errs.push_back(widen(id.get<string>()) + L": " + e);
            }
        } else if (body.is_object()) {
            n = tweaks::applyMany(body, errs);
        }
        json out = { {"applied", n}, {"errors", json::array()} };
        for (auto& e : errs) out["errors"].push_back(narrow(e));
        res.set_content(out.dump(), "application/json");
    });

    svr.Post("/api/tweaks/restore", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; res.set_content("{\"error\":\"bad json\"}", "application/json"); return; }
        int n = 0; json errs = json::array();
        for (auto& idv : body.is_array() ? body : json::array()) {
            wstring e;
            if (tweaks::restore(idv.get<string>(), e)) n++;
            else errs.push_back(narrow(widen(idv.get<string>()) + L": " + e));
        }
        res.set_content(json({ {"restored", n}, {"errors", errs} }).dump(), "application/json");
    });

    // ---------------- Smart Optimize (goal-aware ranked plan) ----------------
    svr.Get("/api/smart", [](const httplib::Request& req, httplib::Response& res) {
        string goal = req.has_param("goal") ? req.get_param_value("goal") : "gaming";
        res.set_content(engine::smartPlan(goal).dump(), "application/json");
    });

    svr.Post("/api/profile", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { body = json::object(); }
        string name = body.value("name", "gaming");
        auto rep = engine::runProfile(name);
        json out = { {"summary", narrow(rep.summary)}, {"applied", rep.applied},
                     {"failed", rep.failed}, {"freedBytes", rep.freedBytes} };
        res.set_content(out.dump(), "application/json");
    });

    // ---------------- network ----------------
    svr.Get("/api/ping", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(jsonPingAll().dump(), "application/json");
    });

    svr.Post("/api/ping", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; return; }
        string host = body.value("host", "");
        if (host.empty()) { res.status = 400; return; }
        auto r = ping::measure(widen(host));
        res.set_content(json({ {"ok", r.ok}, {"ms", r.ok ? (int)r.ms : -1} }).dump(), "application/json");
    });

    // ---------------- clean / logs / processes ----------------
    svr.Post("/api/clean", [](const httplib::Request&, httplib::Response& res) {
        auto freed = cleaner::purge();
        cleaner::emptyRecycleBin();
        res.set_content(json({ {"freedBytes", freed} }).dump(), "application/json");
    });

    svr.Get("/api/logs", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(json({ {"log", narrow(log::readAll())} }).dump(), "application/json");
    });

    // ============ v2.1: diagnostics ============
    svr.Get("/api/diagnostics", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(diag::runAll().dump(), "application/json");
    });

    // ============ v2: scanner ============
    svr.Get("/api/scan", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(scan::runScan().dump(), "application/json");
    });

    // ============ v2: network center ============
    svr.Get("/api/net/status", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(netprofile::statusJson().dump(), "application/json");
    });
    svr.Post("/api/net/profile", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; return; }
        auto r = netprofile::apply(body.value("profile", "balanced"));
        json out = { {"applied", r.applied}, {"failed", r.failed}, {"errors", json::array()} };
        for (auto& e : r.errors) out["errors"].push_back(narrow(e));
        res.set_content(out.dump(), "application/json");
    });
    svr.Post("/api/net/dns", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; return; }
        wstring err;
        bool okb = netprofile::setDns(widen(body.value("primary", "1.1.1.1")), widen(body.value("secondary", "")), err);
        res.set_content(json({ {"ok", okb}, {"error", narrow(err)} }).dump(), "application/json");
    });
    svr.Post("/api/net/flush", [](const httplib::Request&, httplib::Response& res) {
        wstring err;
        bool okb = netprofile::flushDns(err);
        res.set_content(json({ {"ok", okb}, {"error", narrow(err)} }).dump(), "application/json");
    });
    svr.Post("/api/net/dns/reset", [](const httplib::Request&, httplib::Response& res) {
        wstring err;
        bool okb = netprofile::resetDnsToDhcp(err);
        res.set_content(json({ {"ok", okb}, {"error", narrow(err)} }).dump(), "application/json");
    });
    svr.Post("/api/net/mtu", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; return; }
        wstring err;
        bool okb = netprofile::setMtu(body.value("mtu", 1500), err);
        res.set_content(json({ {"ok", okb}, {"error", narrow(err)} }).dump(), "application/json");
    });

    // ============ v2: games ============
    // ?icons=1 also extracts the missing icons (a few hundred ms) so the grid fills in one pass.
    svr.Get("/api/games", [](const httplib::Request& req, httplib::Response& res) {
        auto found = games::detect();
        if (req.has_param("icons") && req.get_param_value("icons") != "0")
            games::extractMissingIcons(found, 20000);
        json arr = json::array();
        for (auto& g : found) {
            arr.push_back({
                {"id", narrow(g.id)}, {"name", narrow(g.name)},
                {"launcher", narrow(g.launcher)}, {"exe", narrow(g.exePath)},
                {"running", g.running}, {"icon", narrow(g.iconPath)},
                {"family", narrow(g.family)}, {"matched", narrow(g.matched)},
            });
        }
        res.set_content(arr.dump(), "application/json");
    });

    // the built-in game database, so the Game Library can list far more than the cover art
    svr.Get("/api/games/catalog", [](const httplib::Request&, httplib::Response& res) {
        json arr = json::array();
        for (auto& c : games::catalog())
            arr.push_back({ {"exe", narrow(c.exe)}, {"name", narrow(c.name)}, {"family", c.family} });
        res.set_content(arr.dump(), "application/json");
    });

    // batch icon extraction: {"all":true} or {"ids":["..."]}
    svr.Post("/api/games/icons", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { body = json::object(); }
        auto found = games::detect();
        vector<wstring> wanted;
        if (body.contains("ids") && body["ids"].is_array())
            for (auto& id : body["ids"]) wanted.push_back(widen(id.get<string>()));
        vector<games::Game> target;
        for (auto& g : found) {
            if (wanted.empty()) { target.push_back(g); continue; }
            for (auto& w : wanted) if (w == g.id) { target.push_back(g); break; }
        }
        int n = games::extractMissingIcons(target, 45000);
        res.set_content(json({ {"ok", true}, {"extracted", n}, {"total", (int)found.size()} }).dump(),
                        "application/json");
    });

    // launch a detected game / reveal it in Explorer
    svr.Post("/api/games/launch", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; return; }
        string id = body.value("id", "");
        wstring err;
        for (auto& g : games::detect()) {
            if (narrow(g.id) != id) continue;
            bool okb = body.value("reveal", false) ? games::revealInExplorer(g, err) : games::launch(g, err);
            res.set_content(json({ {"ok", okb}, {"error", narrow(err)} }).dump(), "application/json");
            return;
        }
        res.status = 404;
        res.set_content(json({ {"ok", false}, {"error", "game not found"} }).dump(), "application/json");
    });
    svr.Get(R"(/api/game-icon/(.*))", [](const httplib::Request& req, httplib::Response& res) {
        // req.matches[1] = game id (already [\w-]+ by our pattern); build cache path safely
        string id = req.matches[1];
        if (id.find("..") != string::npos) { res.status = 400; return; }
        wstring p = appDataDir() + L"\\game-icons\\" + widen(id) + L".png";
        std::ifstream f(p.c_str(), std::ios::binary);
        if (!f) { res.status = 404; res.set_content("{}", "application/json"); return; }
        std::ostringstream ss; ss << f.rdbuf();
        res.set_content(ss.str(), "image/png");
    });
    svr.Post("/api/games/icon", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; return; }
        for (auto& g : games::detect()) {
            if (narrow(g.id) == body.value("id", "")) {
                wstring p = games::extractIcon(g);
                res.set_content(json({ {"ok", !p.empty()} }).dump(), "application/json");
                return;
            }
        }
        res.status = 404;
    });
    svr.Post("/api/games/profile", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; return; }
        wstring err;
        bool okb;
        if (body.value("action", "save") == "apply")
            okb = games::applyProfile(widen(body.value("id", "")), err);
        else if (body.value("action", "") == "clear")
            okb = games::clearProfile(widen(body.value("id", "")), err);
        else
            okb = games::saveProfile(widen(body.value("id", "")), body.value("profile", json::object()));
        res.set_content(json({ {"ok", okb}, {"error", narrow(err)} }).dump(), "application/json");
    });

    // one click for the whole library: save + apply a high-priority profile per detected game
    svr.Post("/api/games/boost-all", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { body = json::object(); }
        const bool on = body.value("on", true);
        const int limit = body.value("limit", 40);
        int n = 0, seen = 0;
        json errs = json::array();
        for (auto& g : games::detect()) {
            if (++seen > limit) break;
            wstring err;
            if (on && g.running) continue;
            if (on) {
                games::saveProfile(g.id, json({ {"priority", "high"}, {"disable_fso", false},
                                                {"game", narrow(g.name)} }));
                if (games::applyProfile(g.id, err)) n++;
                else if (errs.size() < 6) errs.push_back(narrow(g.name + L": " + err));
            } else {
                if (games::clearProfile(g.id, err)) n++;
                else if (errs.size() < 6) errs.push_back(narrow(g.name + L": " + err));
            }
        }
        log2::info(L"GAMES", L"boost-all " + wstring(on ? L"on" : L"off") + L": " + std::to_wstring(n) + L" games");
        res.set_content(json({ {"ok", true}, {"applied", n}, {"errors", errs} }).dump(), "application/json");
    });

    // ============ v2: gaming mode ============
    svr.Post("/api/gaming/enter", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { body = json::object(); }
        wstring err;
        bool okb = games::gamingModeEnter(widen(body.value("game", "")), err);
        res.set_content(json({ {"ok", okb}, {"error", narrow(err)}, {"active", games::gamingModeActive()} }).dump(), "application/json");
    });
    svr.Post("/api/gaming/exit", [](const httplib::Request&, httplib::Response& res) {
        wstring err;
        games::gamingModeExit(err);
        res.set_content(json({ {"ok", true}, {"active", false} }).dump(), "application/json");
    });

    // ============ v2: RAM ============
    svr.Get("/api/ram", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(ram::toJson().dump(), "application/json");
    });
    svr.Post("/api/ram/trim", [](const httplib::Request&, httplib::Response& res) {
        wstring err;
        bool okb = ram::trimStandby(err);
        res.set_content(json({ {"ok", okb}, {"error", narrow(err)} }).dump(), "application/json");
    });

    // ============ v2: startup ============
    svr.Get("/api/startup", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(startup::listJson().dump(), "application/json");
    });
    svr.Post("/api/startup/toggle", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; return; }
        wstring err;
        bool okb = startup::setEnabled(widen(body.value("location", "")), widen(body.value("name", "")),
                                       body.value("enable", true), err);
        res.set_content(json({ {"ok", okb}, {"error", narrow(err)} }).dump(), "application/json");
    });

    // ============ v2: storage ============
    svr.Get("/api/storage", [](const httplib::Request&, httplib::Response& res) {
        json out;
        out["drives"] = storage::drivesJson();
        out["junkBytes"] = cleaner::measure().total;
        res.set_content(out.dump(), "application/json");
    });
    svr.Post("/api/storage/files", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; return; }
        res.set_content(storage::largestFiles(widen(body.value("root", "C:\\")), body.value("top", 15)).dump(), "application/json");
    });
    svr.Post("/api/storage/folders", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; return; }
        res.set_content(storage::folderSizes(widen(body.value("root", "C:\\")), body.value("top", 12)).dump(), "application/json");
    });

    // ============ v2.6: process reducer ============
    svr.Get("/api/reducer", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(reducer::listJson().dump(), "application/json");
    });
    svr.Get("/api/reducer/state", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(reducer::stateJson().dump(), "application/json");
    });
    svr.Post("/api/reducer/act", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; return; }
        string action = body.value("action", "");
        DWORD pid = (DWORD)body.value("pid", 0);
        wstring err;
        json out;
        if (action == "eco")          out["ok"] = reducer::eco(pid, err);
        else if (action == "boost")   out["ok"] = reducer::boost(pid, err);
        else if (action == "undo")    out["ok"] = reducer::undo(pid, err);
        else if (action == "kill")    out["ok"] = reducer::kill(pid, err);
        else if (action == "eco-all") out["ok"] = true, out["n"] = reducer::ecoAll(body.value("minCpu", 1.0), err);
        else if (action == "undo-all")out["ok"] = true, out["n"] = reducer::undoAll(err);
        else { res.status = 400; return; }
        out["error"] = narrow(err);
        res.set_content(out.dump(), "application/json");
    });

    // ============ v2.6: security scan ============
    svr.Get("/api/security/scan", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(secscan::runScan().dump(), "application/json");
    });
    svr.Post("/api/security/revoke", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; return; }
        wstring err;
        bool okb = body.value("restore", false) ? secscan::restoreEntry(widen(body.value("id", "")), err)
                                                : secscan::revokeEntry(widen(body.value("id", "")), err);
        res.set_content(json({ {"ok", okb}, {"error", narrow(err)} }).dump(), "application/json");
    });

    // ============ v2.7: firmware / platform inventory (read-only) ============
    svr.Get("/api/firmware", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(firmware::inventory().dump(), "application/json");
    });

    // ============ v2.7: driver update engine ============
    svr.Get("/api/drvupdate/report", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(drvupdate::report().dump(), "application/json");
    });
    svr.Get("/api/drvupdate/problems", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(drvupdate::problemDevices().dump(), "application/json");
    });
    svr.Post("/api/drvupdate/scan", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { body = json::object(); }
        res.set_content(drvupdate::scanWindowsUpdate(body.value("driversOnly", true)).dump(), "application/json");
    });
    svr.Post("/api/drvupdate/rescan", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(drvupdate::rescanDevices().dump(), "application/json");
    });
    svr.Post("/api/drvupdate/vendor", [](const httplib::Request&, httplib::Response& res) {
        bool okb = drvupdate::openVendorPage();
        res.set_content(json({ {"ok", okb} }).dump(), "application/json");
    });
    svr.Post("/api/drvupdate/wu-window", [](const httplib::Request&, httplib::Response& res) {
        drvupdate::openWindowsUpdateUi();
        res.set_content(json({ {"ok", true} }).dump(), "application/json");
    });

    // ============ v2.6: DiskScope storage ============
    svr.Post("/api/disk/folders", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { body = json::object(); }
        res.set_content(diskscope::driveAndFolders(widen(body.value("drive", "C:")),
                                                   body.value("top", 20), body.value("budgetMs", 15000)).dump(),
                        "application/json");
    });
    svr.Get("/api/disk/cleanup", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(diskscope::cleanupTargets().dump(), "application/json");
    });
    svr.Post("/api/disk/duplicates", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { body = json::object(); }
        res.set_content(diskscope::duplicates(widen(body.value("root", "C:\\Users")),
                                              body.value("max", 3000), body.value("budgetMs", 30000)).dump(),
                        "application/json");
    });

    // ============ v2: benchmark ============
    svr.Post("/api/bench", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(storage::runBenchmarkAll().dump(), "application/json");
    });
    svr.Get("/api/bench/history", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(storage::historyJson().dump(), "application/json");
    });

    // ============ v2: logs (streaming) ============
    svr.Get("/api/logs2", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(log2::recentJson(500).dump(), "application/json");
    });
    svr.Get("/api/logs2/stream", [](const httplib::Request& req, httplib::Response& res) {
        long long after = 0;
        try { after = std::stoll(req.get_param_value("after")); } catch (...) {}
        auto page = log2::stream(after, 20000);   // long-poll up to 20 s
        json arr = json::array();
        long long last = after;
        for (auto& r : page.items) {
            ++last;
            arr.push_back({ {"seq", last}, {"time", narrow(r.time)},
                            {"sev", narrow(log2::sevName(r.sev))}, {"cat", narrow(r.category)},
                            {"msg", narrow(r.message)} });
        }
        res.set_content(json({ {"items", arr}, {"nextSeq", last} }).dump(), "application/json");
    });

    // ============ v2: settings ============
    svr.Get("/api/settings", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(engine::loadConfig().dump(), "application/json");
    });
    svr.Post("/api/settings", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; return; }
        if (body.is_object()) {
            for (auto it = body.begin(); it != body.end(); ++it)
                settings::set(it.key(), it.value());
        }
        res.set_content(json({ {"ok", true} }).dump(), "application/json");
    });

    // ============ v2: tools launcher ============
    svr.Post("/api/tools", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) { res.status = 400; return; }
        string tool = body.value("tool", "");
        wstring c;
        // --- admin / system
        if      (tool == "taskmgr")     c = L"taskmgr.exe";
        else if (tool == "devmgmt")     c = L"devmgmt.msc";
        else if (tool == "eventvwr")    c = L"eventvwr.msc";
        else if (tool == "services")    c = L"services.msc";
        else if (tool == "resmon")      c = L"resmon.exe";
        else if (tool == "perfmon")     c = L"perfmon.exe";
        else if (tool == "msinfo32")    c = L"msinfo32.exe";
        else if (tool == "diskmgmt")    c = L"diskmgmt.msc";
        else if (tool == "regedit")     c = L"regedit.exe";
        else if (tool == "wscui")       c = L"wscui.cpl";
        else if (tool == "ncpa")        c = L"ncpa.cpl";
        else if (tool == "powercfg")    c = L"powercfg.cpl";
        else if (tool == "wt")          c = L"wt.exe";
        else if (tool == "powershell")  c = L"powershell.exe";
        else if (tool == "cmd")         c = L"cmd.exe";
        else if (tool == "cleanmgr")    c = L"cleanmgr.exe";
        else if (tool == "dfrgui")      c = L"dfrgui.exe";
        // --- control panel applets
        else if (tool == "appwiz")      c = L"appwiz.cpl";
        else if (tool == "sysdm")       c = L"sysdm.cpl";
        else if (tool == "inetcpl")     c = L"inetcpl.cpl";
        else if (tool == "timedate")    c = L"timedate.cpl";
        else if (tool == "mouse")       c = L"main.cpl";
        else if (tool == "display")     c = L"desk.cpl";
        else if (tool == "hdwwiz")      c = L"hdwwiz.cpl";
        else if (tool == "joy")         c = L"joy.cpl";
        else if (tool == "intl")        c = L"intl.cpl";
        else if (tool == "mmsys")       c = L"mmsys.cpl";
        else if (tool == "firewall")    c = L"firewall.cpl";
        else if (tool == "sysdefaultinput") c = L"control.exe";
        // --- admin consoles
        else if (tool == "compmgmt")    c = L"compmgmt.msc";
        else if (tool == "taskchd")     c = L"taskschd.msc";
        else if (tool == "secpol")      c = L"secpol.msc";
        else if (tool == "gpedit")      c = L"gpedit.msc";
        else if (tool == "lusrmgr")     c = L"lusrmgr.msc";
        else if (tool == "certmgr")     c = L"certmgr.msc";
        else if (tool == "wfmsc")       c = L"wf.msc";
        else if (tool == "tpm")         c = L"tpm.msc";
        else if (tool == "wmimgmt")     c = L"wmimgmt.msc";
        // --- system utilities
        else if (tool == "winver")      c = L"winver.exe";
        else if (tool == "dxdiag")      c = L"dxdiag.exe";
        else if (tool == "msconfig")    c = L"msconfig.exe";
        else if (tool == "rstrui")      c = L"rstrui.exe";
        else if (tool == "sysprotection") c = L"SystemPropertiesProtection.exe";
        else if (tool == "sysperf")     c = L"SystemPropertiesPerformance.exe";
        else if (tool == "sysadvanced") c = L"SystemPropertiesAdvanced.exe";
        else if (tool == "optionalfeatures") c = L"optionalfeatures.exe";
        else if (tool == "mdsched")     c = L"mdsched.exe";
        else if (tool == "verifier")    c = L"verifier.exe";
        else if (tool == "recoverydrive") c = L"RecoveryDrive.exe";
        // --- ms-settings URIs (Windows 10/11 Settings pages)
        else if (tool.rfind("ms-settings:", 0) == 0) c = widen(tool);
        if (!c.empty()) {
            ShellExecuteW(nullptr, L"open", c.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            log2::info(L"TOOLS", L"Opened " + widen(tool));
            res.set_content(json({ {"ok", true} }).dump(), "application/json");
        } else {
            res.status = 404;
        }
    });

    svr.Get("/api/open", [](const httplib::Request& req, httplib::Response& res) {
        string what = req.get_param_value("what");
        if (what == "logfolder") shellOpen(appDataDir());
        else if (what == "log") shellOpen(logPath());
        else if (what == "devmgmt") { string o; runCapture(L"devmgmt.msc", o); }
        else if (what == "dxdiag") { string o; runCapture(L"dxdiag.exe", o); }
        // command palette: jump straight into a store's own library
        else if (what == "steam") shellOpen(L"steam://open/games");
        else if (what == "epic") shellOpen(L"com.epicgames.launcher://apps");
        else if (what == "battlenet") shellOpen(L"battlenet://");
        else if (what == "gog") shellOpen(L"goggalaxy://openAppLibrary");
        else if (what == "ubisoft") shellOpen(L"uplay://");
        else if (what == "ea") shellOpen(L"origin://launch");
        else if (what == "xbox") shellOpen(L"msxbox://");
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
        string msg = "internal error";
        try { if (ep) std::rethrow_exception(ep); } catch (const std::exception& e) { msg = e.what(); }
        log2::error(L"API", widen(msg));
        res.status = 500;
        res.set_content(json({ {"error", msg} }).dump(), "application/json");
    });

    int port = (int)preferredPort;
    if (!svr.bind_to_port("127.0.0.1", port)) return -1;
    if (!svr.listen_after_bind()) return -1; // blocks & serves
    return port;
}

} // namespace ok::server
