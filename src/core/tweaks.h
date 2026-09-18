// OptimizeKit - tweak catalog & engine
#pragma once
#include "common.h"
#include "json.hpp"

namespace ok::tweaks {

using json = nlohmann::json;

struct Tweak {
    string id;          // stable id used by config.json / CLI
    wstring name;       // display name
    wstring desc;       // what it does
    bool    admin = false;  // requires elevation
    int     impact = 1;     // 1 = mild, 2 = moderate, 3 = strong (FPS / latency)
    wstring source;     // where the tweak comes from (credit)
};

struct State {
    bool supported = true;   // tweak applies to this OS
    bool applied   = false;  // current registry state matches "tweaked"
};

const vector<Tweak>& catalog();

// How many tweaks of `catalog()` require admin?
int adminCount();

// Read current machine state for one tweak (best-effort heuristic)
State checkState(const string& id);

// Apply one tweak. Creates a registry backup on first run per session.
// Returns false + reason on failure.
bool apply(const string& id, wstring& err);

// Restore Windows defaults for one tweak.
bool restore(const string& id, wstring& err);

// Apply every tweak marked true in `sel`. Returns number applied.
int applyMany(const json& sel, vector<wstring>& errors);

// Backup the registry keys we touch into %LOCALAPPDATA%\OptimizeKit\backup_*.reg
wstring backupAll();

// One-click profiles ------------------------------------------------------
// gaming : all latency/FPS tweaks
// privacy: telemetry + bloat + tracking
// full   : everything
json profilePreset(const string& name);

} // namespace ok::tweaks
