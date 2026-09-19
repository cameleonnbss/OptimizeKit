// OptimizeKit - hardware / OS snapshot
#pragma once
#include "common.h"

namespace ok::sysinfo {

struct Info {
    wstring computerName, userName;
    wstring osName, osVersion, osBuild, osArch;
    wstring cpuName;
    DWORD   cpuCores = 0;
    uint64_t ramTotal = 0, ramAvail = 0;
    wstring gpuName;
    DWORD   gpuDriverVer = 0;         // as packed HIGHWORD/LOWWORD
    wstring gpuDriverStr;
    bool    elevated = false;
    bool    gameModeOn = false;       // HKCU GameDVR_GameMode
    bool    hagsOn = false;           // hardware-accelerated GPU scheduling
    wstring powerPlan;                // active scheme name
    wstring uptime;
};

Info collect();

// Individual getters used by the dashboard tiles
wstring activePowerPlanName();
wstring activePowerPlanGuid();
bool   queryGameMode();
bool   queryHags();

} // namespace ok::sysinfo
