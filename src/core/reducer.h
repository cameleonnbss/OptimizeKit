// OptimizeKit v2.6 - Process Reducer.
//
// Cut background CPU and load on a gaming machine, without killing anything by default.
// Three reversible levers per process:
//   * priority       - Eco sets the priority class BELOW_NORMAL (restored to what it was);
//   * power throttle - PROCESS_POWER_THROTTLING_EXECUTION_SPEED asks Windows to run the
//                      process on efficiency cores when it would otherwise burn performance
//                      cores (EcoQoS - the real lever, Win10 20H1+ / Win11);
//   * kill           - explicit, per-PID, never part of the bulk actions.
//
// A snapshot of everything the reducer touches is kept so "Undo all" restores the exact
// previous state, even after the UI was closed in between. restoreAll() also runs on
// server shutdown so an OptimizeKit session never leaves a machine throttled.
#pragma once
#include "common.h"
#include "json.hpp"

namespace ok::reducer {

using json = nlohmann::json;

// Live view of the candidates: every process that is NOT the dashboard itself and not
// a system-critical one, with CPU% (delta-based) and RAM, sorted by cost. Page 1 of the
// Process Reducer view.
json listJson();

// Actions (all reversible unless stated):
//   eco   - BELOW_NORMAL priority + EcoQoS power throttling on that PID
//   undo  - restore this PID's original priority class + clear throttling
//   boost - ABOVE_NORMAL priority (for the thing that matters right now)
//   kill  - terminate one PID (explicit user action only, never bulk)
bool eco(DWORD pid, wstring& err);
bool undo(DWORD pid, wstring& err);
bool boost(DWORD pid, wstring& err);
bool kill(DWORD pid, wstring& err);

// Bulk actions over every candidate above `minCpuPct` (or all, when 0).
// Returns how many processes were touched.
int ecoAll(double minCpuPct, wstring& err);
int undoAll(wstring& err);

// Restore every process touched this session (also called on server shutdown).
void restoreAll();

// Session summary for the UI: how many processes reduced, original classes kept.
json stateJson();

} // namespace ok::reducer
