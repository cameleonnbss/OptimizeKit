#include "engine.h"
#include <fstream>
#include <cstdio>
#include <shlobj.h>

namespace ok::engine {

using json = nlohmann::json;

json loadConfig() {
    json j;
    std::ifstream f(configPath().c_str());
    if (f) { try { f >> j; } catch (...) { j = json::object(); } }
    if (!j.is_object()) j = json::object();
    if (!j.contains("ping_targets")) {
        j["ping_targets"] = json::array({
            { {"name", "Cloudflare"}, {"host", "1.1.1.1"} },
            { {"name", "Google"},     {"host", "8.8.8.8"} },
            { {"name", "Gateway"},    {"host", "192.168.1.1"} },
        });
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

json pingTargets() {
    json cfg = loadConfig();
    json out = json::array();
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
