// OptimizeKit - game library: launcher scan, per-game profiles, icon cache, gaming mode
#pragma once
#include "common.h"
#include "json.hpp"

namespace ok::games {

using json = nlohmann::json;

struct Game {
    wstring id;          // stable id (exe name lowercased)
    wstring name;        // display name
    wstring exePath;     // full path
    wstring launcher;    // Steam / Epic / Xbox / Riot / GOG / Battle.net / Ubisoft / EA / itch.io / Standalone
    wstring iconPath;    // cached PNG path ("" if none) - served as /api/game-icon/<id>
    wstring family;      // genre family from the built-in database ("" when unknown)
    wstring matched;     // canonical database title when the exe/folder name was recognised
    bool running = false;
};

// Scan launcher libraries + common install dirs. Fast (registry + a few dir listings).
vector<Game> detect();

// Extract the game's real icon from its exe into the icon cache (PNG, 96px).
// Returns the relative cache path or "" on failure. Cheap after first extraction.
wstring extractIcon(const Game& g);

// Extract icons for every game that has none yet, stopping after `budgetMs` (0 = no limit).
// Returns how many new PNGs were written.
int extractMissingIcons(vector<Game>& games, int budgetMs = 0);

// Launch the game / reveal its exe in Explorer.
bool launch(const Game& g, wstring& err);
bool revealInExplorer(const Game& g, wstring& err);

// Built-in game database rows (the Game Library module merges these with its cover art).
struct CatalogEntry { wstring exe; wstring name; string family; };
vector<CatalogEntry> catalog();

// Per-game profile stored in config.json under "game_profiles".
// Actions are REAL engine actions: persistent priority, GPU preference, FSO flag.
json getProfile(const wstring& gameId);           // {} if none
bool saveProfile(const wstring& gameId, const json& p);

// Apply / clear the per-game profile (called from UI and from the launcher watcher).
bool applyProfile(const wstring& gameId, wstring& err);
bool clearProfile(const wstring& gameId, wstring& err);

// ---------------- Gaming Mode (snapshot & restore) ----------------
// Snapshot: power plan, priority-boosted PIDs, killed user processes (only listed ones
// are killed - the user picks them in Settings > Gaming). Everything is restored.
struct GamingSnapshot { std::wstring powerPlanBefore; bool active = false; };

// Enter gaming mode for `gameId` (or "" for generic). Returns false + err on failure.
bool gamingModeEnter(const wstring& gameId, wstring& err);
bool gamingModeExit(wstring& err);
bool gamingModeActive();

} // namespace ok::games
