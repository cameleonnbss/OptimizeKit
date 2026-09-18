// OptimizeKit - game boost: process priority, power throttle, game mode
#pragma once
#include "common.h"

namespace ok::gameboost {

struct RunningProcess {
    DWORD pid;
    wstring name;
    wstring title;   // main window title (may be empty)
};

// All visible-window processes (candidates to boost)
vector<RunningProcess> listProcesses();

// Set priority class for a PID (1=idle 2=below 3=normal 4=above 5=high 6=realtime)
bool setPriority(DWORD pid, int level, wstring& err);

// Persist a game exe name as "high priority" via Image File Execution Options (PerfOptions)
bool setPersistentPriority(const wstring& exeName, int level, wstring& err);

// Kill a process by pid
bool killProcess(DWORD pid, wstring& err);

} // namespace ok::gameboost
