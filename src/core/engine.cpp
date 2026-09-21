#include "engine.h"
#include <fstream>
#include <cstdio>
#include <shlobj.h>
#include <algorithm>
#include <cmath>

namespace ok::engine {

using json = nlohmann::json;

json loadConfig() {
    json j;
    bool fromDisk = false;
    std::ifstream f(configPath().c_str());
    if (f) {
        try { f >> j; fromDisk = j.is_object(); } catch (...) { j = json::object(); }
    }
    if (!fromDisk) j = json::object();
    if (!j.contains("ping_targets") || !j["ping_targets"].is_array() || j["ping_targets"].empty()) {
        j["ping_targets"] = json::array({
            { {"name", "Cloudflare"}, {"host", "1.1.1.1"} },
            { {"name", "Google"},     {"host", "8.8.8.8"} },
            { {"name", "Quad9"},      {"host", "9.9.9.9"} },
        });
        saveConfig(j); // persist defaults so the file exists on first run
    }
    if (!j.contains("applied_tweaks")) j["applied_tweaks"] = json::object();
    return j;
}

bool saveConfig(const json& j) {
    try {
        std::ofstream f(configPath().c_str(), std::ios::trunc);
        f << j.dump(2);
        return true;
    } catch (...) { return false; }
}

RunReport runProfile(const string& name) {
    RunReport rep;
    log::section(L"PROFILE: " + widen(name));

    json sel = tweaks::profilePreset(name);
    rep.applied = tweaks::applyMany(sel, rep.errors);
    rep.failed = (int)rep.errors.size();

    // cleanup is part of every profile
    rep.freedBytes = cleaner::purge();
    cleaner::emptyRecycleBin();

    // remember what was applied
    json cfg = loadConfig();
    json& applied = cfg["applied_tweaks"];
    for (auto it = sel.begin(); it != sel.end(); ++it)
        if (it.value().get<bool>()) applied[it.key()] = true;
    saveConfig(cfg);

    wchar_t buf[128];
    swprintf(buf, 128, L"%d tweaks applied, %d failed, %s freed",
             rep.applied, rep.failed, fmtBytes(rep.freedBytes).c_str());
    rep.summary = buf;
    log::section(L"PROFILE DONE: " + wstring(buf));
    return rep;
}

// ------------------------------------------------------------- Smart Optimize
// Every tweak description starts with a [tag] hint (see tweaks.cpp), which we
// reuse as its category instead of duplicating the whole catalog here.
static string smartTag(const wstring& desc) {
    if (desc.size() > 2 && desc[0] == L'[') {
        auto e = desc.find(L']');
        if (e != wstring::npos && e <= 16)
            return narrow(wstring(desc.begin() + 1, desc.begin() + (ptrdiff_t)e));
    }
    return "general";
}

// How much one category is worth for a given goal. Deliberately simple and
// transparent: no invented numbers, just an explicit priority table.
static int smartWeight(const string& tag, const string& goal) {
    if (goal == "privacy") {
        if (tag == "privacy") return 5;
        if (tag == "security") return 3;
        return 1;
    }
    if (goal == "latency") {
        if (tag == "latency" || tag == "input") return 5;
        if (tag == "network") return 4;
        if (tag == "gaming" || tag == "power") return 2;
        return 1;
    }
    if (goal == "balanced") return 2;
    // "gaming" (default)
    if (tag == "latency" || tag == "input") return 4;
    if (tag == "gaming" || tag == "network") return 3;
    if (tag == "power" || tag == "disk") return 2;
    return 1;
}

json smartPlan(const string& goal) {
    const bool admin = isAdmin();
    const wstring plan = sysinfo::activePowerPlanName();
    const bool hags = sysinfo::queryHags();
    const bool gm   = sysinfo::queryGameMode();

    struct Row { string id, tag; wstring name, desc; int impact; bool tadmin, applied, needAdmin; double w; };
    vector<Row> rows;
    double wApplied = 0.0, wTotal = 0.0;
    int appliedN = 0, adminN = 0;

    for (auto& t : tweaks::catalog()) {
        auto st = tweaks::checkState(t.id);
        string tag = smartTag(t.desc);
        double w = (double)smartWeight(tag, goal) * (double)t.impact;
        // Live-context boost: on THIS machine these are the ones actually costing
        // performance, so surface them above the generic catalog order.
        if (!st.applied && goal != "privacy") {
            if (t.id == "power_ultimate" && plan.find(L"Ultimate") == wstring::npos
                                           && plan.find(L"High performance") == wstring::npos) w += 4;
            if (t.id == "hags_on"   && !hags) w += 3;
            if (t.id == "game_mode" && !gm)   w += 2;
        }
        bool needAdmin = t.admin && !admin;
        if (needAdmin) adminN++;
        wTotal += w;
        if (st.applied) { wApplied += w; appliedN++; }
        rows.push_back({ t.id, tag, t.name, t.desc, t.impact, t.admin, st.applied, needAdmin, w });
    }

    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        if (a.applied != b.applied) return !a.applied;   // what still needs doing comes first
        if (a.w != b.w) return a.w > b.w;                // then by relevance for the goal
        if (a.impact != b.impact) return a.impact > b.impact;
        return a.id < b.id;
    });

    int total = (int)rows.size();
    int pending = total - appliedN;
    int score = wTotal > 0.0 ? (int)std::lround(wApplied / wTotal * 100.0) : 0;

    string headline;
    if (pending == 0) headline = "Everything this machine can still gain is already optimized.";
    else headline = std::to_string(pending) + (pending == 1 ? " optimization" : " optimizations") + " available for this goal";
    if (adminN > 0 && !admin) headline += " — " + std::to_string(adminN) + " need administrator";

    json items = json::array();
    for (auto& r : rows) {
        string why = r.applied ? "Already active - the Windows default is overridden and backed up."
                   : r.needAdmin ? "Requires elevation - relaunch from OptimizeKit.bat (option 1, admin)."
                   : "Ready to apply - snapshotted first, reversible in one click.";
        items.push_back({
            {"id", r.id}, {"name", narrow(r.name)}, {"desc", narrow(r.desc)},
            {"why", why}, {"impact", r.impact}, {"admin", r.tadmin},
            {"applied", r.applied}, {"needAdmin", r.needAdmin},
            {"category", r.tag}, {"weight", (int)std::lround(r.w)},
        });
    }

    json out;
    out["goal"] = goal;
    out["score"] = score;
    out["applied"] = appliedN;
    out["total"] = total;
    out["pending"] = pending;
    out["adminMissing"] = adminN;
    out["admin"] = admin;
    out["headline"] = headline;
    out["items"] = items;
    return out;
}

json pingTargets() {
    json cfg = loadConfig();
    json out = json::array();
    if (!cfg.contains("ping_targets") || !cfg["ping_targets"].is_array()) return out;
    for (auto& t : cfg["ping_targets"]) {
        auto r = ping::measure(widen(t.value("host", "1.1.1.1")));
        out.push_back({
            {"name", t.value("name", "?")},
            {"host", t.value("host", "?")},
            {"ok",   r.ok},
            {"ms",   r.ok ? (int)r.ms : -1},
        });
    }
    return out;
}

bool addPingTarget(const wstring& name, const wstring& host) {
    json cfg = loadConfig();
    for (auto& t : cfg["ping_targets"])
        if (widen(t.value("host", "")) == host) return false;
    cfg["ping_targets"].push_back({ {"name", narrow(name)}, {"host", narrow(host)} });
    return saveConfig(cfg);
}

bool removePingTarget(const wstring& name) {
    json cfg = loadConfig();
    json arr = json::array();
    for (auto& t : cfg["ping_targets"])
        if (widen(t.value("name", "")) != name) arr.push_back(t);
    cfg["ping_targets"] = arr;
    return saveConfig(cfg);
}

} // namespace ok::engine
