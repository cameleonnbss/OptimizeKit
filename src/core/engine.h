// OptimizeKit - engine: ties together tweaks, cleaner, logging and the JSON config
#pragma once
#include "common.h"
#include "json.hpp"
#include "tweaks.h"
#include "cleaner.h"
#include "sysinfo.h"
#include "ping.h"
#include "drivers.h"
#include "gameboost.h"

namespace ok::engine {

using json = nlohmann::json;

// Load %LOCALAPPDATA%\OptimizeKit\config.json (or a default skeleton)
json loadConfig();
bool   saveConfig(const json& j);

// Run a named profile: "gaming" | "privacy" | "full" | "clean"
// Applies tweaks + cleanup, logs everything, returns summary text (for GUI + CLI)
struct RunReport {
    int    applied = 0;
    int    failed = 0;
    uint64_t freedBytes = 0;
    vector<wstring> errors;
    wstring summary;
};
RunReport runProfile(const string& name);

// Ping helpers used by the dashboard network tile
json pingTargets();     // configured targets -> [{name, host, ms, ok}]
bool addPingTarget(const wstring& name, const wstring& host);
bool removePingTarget(const wstring& name);

} // namespace ok::engine
