// OptimizeKit - RAM status + honest memory trim, and the startup manager
#pragma once
#include "common.h"
#include "json.hpp"

namespace ok::ram {

using json = nlohmann::json;

struct MemInfo {
    uint64_t total = 0, avail = 0;
    DWORD loadPercent = 0;
    uint64_t committed = 0, commitLimit = 0;
    uint64_t cached = 0;      // standby cache (approximation via perf counters)
    int slots = 0;
    wstring speed;            // e.g. "3200 MT/s"
    wstring form;             // DIMM / SODIMM
};

MemInfo collect();
json toJson();

// Standby list trim via NtSetSystemInformation (EmptyWorkingSet is the honest variant).
// This is NOT magic: Windows reuses standby memory instantly when needed; trimming helps
// mostly after heavy apps exited (game mode end). Explained in the UI.
bool trimStandby(wstring& err);

// Reduce the working set of a single process (safe, pages reload on demand)
bool trimProcess(DWORD pid, wstring& err);

} // namespace ok::ram

// ------------------------------------------------------------- startup
namespace ok::startup {

using json = nlohmann::json;

struct Entry {
    wstring name;
    wstring command;
    wstring location;     // HKCU Run / HKLM Run / Startup folder / Scheduled Task
    bool enabled = true;
};

vector<Entry> listEntries();          // HKCU+HKLM Run, Startup folders, approved startup
json listJson();
bool setEnabled(const wstring& location, const wstring& name, bool enable, wstring& err);

} // namespace ok::startup
