// OptimizeKit - embedded HTTP server (cpp-httplib) serving the WormGPT-style dashboard
#include "server.h"
#include "engine.h"
#include "monitor.h"
#include "httplib.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace ok::server {

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

    // ---------------- static files (web/) ----------------
    svr.set_mount_point("/", narrow(webRoot()));

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

    svr.Get("/api/open", [](const httplib::Request& req, httplib::Response& res) {
        string what = req.get_param_value("what");
        if (what == "logfolder") shellOpen(appDataDir());
        else if (what == "log") shellOpen(logPath());
        else if (what == "devmgmt") { string o; runCapture(L"devmgmt.msc", o); }
        else if (what == "dxdiag") { string o; runCapture(L"dxdiag.exe", o); }
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
        string msg = "internal error";
        try { if (ep) std::rethrow_exception(ep); } catch (const std::exception& e) { msg = e.what(); }
        res.status = 500;
        res.set_content(json({ {"error", msg} }).dump(), "application/json");
    });

    int port = (int)preferredPort;
    if (!svr.bind_to_port("127.0.0.1", port)) return -1;
    if (!svr.listen_after_bind()) return -1; // blocks & serves
    return port;
}

} // namespace ok::server
