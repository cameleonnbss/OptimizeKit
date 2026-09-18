// OptimizeKit - live performance monitoring (PDH counters + process scan)
#pragma once
#include "common.h"
#include "json.hpp"

namespace ok::monitor {

using json = nlohmann::json;

struct Sample {
    float cpuPercent = 0;        // total CPU load
    float cpuClockMHz = 0;       // avg current frequency
    float ramPercent = 0;
    uint64_t ramUsed = 0, ramTotal = 0;
    float gpuPercent = 0;        // GPU engine utilization (utilization type)
    float vramPercent = 0;
    float diskPercent = 0;       // % of max disk time
    float diskReadMBs = 0, diskWriteMBs = 0;
    float netDownKBs = 0, netUpKBs = 0;
    DWORD uptimeSec = 0;
    int   procCount = 0, threadCount = 0;
};

// Take one sample (blocks ~ nothing; counters normalized between successive calls).
// First call of a session returns zeros for rate counters (PDH needs two samples).
Sample sampleOnce();

// History ring (CPU & RAM, one value per second, max 120 points)
std::vector<float> cpuHistory();
std::vector<float> ramHistory();
std::vector<float> gpuHistory();

// Top processes by CPU% (name, pid, cpu, ramMB), sorted desc, max `n`
json topProcesses(int n);

} // namespace ok::monitor
