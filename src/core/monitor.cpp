#include "monitor.h"
#include <pdh.h>
#include <pdhmsg.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <mutex>
#include <deque>
#include <map>
#include <algorithm>
#include <cmath>
#include <thread>

#pragma comment(lib, "pdh.lib")

namespace ok::monitor {

using json = nlohmann::json;

// ---------------------------------------------------------------- state
namespace {
struct Counters {
    // CPU
    PDH_HQUERY  q = nullptr;
    PDH_HCOUNTER cpuTotal = nullptr;
    PDH_HCOUNTER cpuClock = nullptr;
    // GPU (engine utilization, type 18446744073709551615 = utilization)
    PDH_HCOUNTER gpuUtil = nullptr;
    // Disk
    PDH_HCOUNTER diskTime = nullptr;
    PDH_HCOUNTER diskRead = nullptr;
    PDH_HCOUNTER diskWrite = nullptr;
    // Network (all interfaces summed)
    PDH_HCOUNTER netIn = nullptr;
    PDH_HCOUNTER netOut = nullptr;
    bool firstSampleDone = false;
    std::deque<float> cpuHist, ramHist, gpuHist;
    ULONGLONG lastHistTick = 0;
    CRITICAL_SECTION cs;
    bool csInit = false;
};

Counters g;

void ensureInit() {
    if (g.q) return;
    if (PdhOpenQueryW(nullptr, 0, &g.q) != ERROR_SUCCESS) return;

    PdhAddEnglishCounterW(g.q, L"\\Processor Information(_Total)\\% Processor Time", 0, &g.cpuTotal);
    PdhAddEnglishCounterW(g.q, L"\\Processor Information(_Total)\\% Processor Performance", 0, &g.cpuClock);
    PdhAddEnglishCounterW(g.q, L"\\GPU Engine(*)\\Utilization Percentage", 0, &g.gpuUtil);
    PdhAddEnglishCounterW(g.q, L"\\PhysicalDisk(_Total)\\% Disk Time", 0, &g.diskTime);
    PdhAddEnglishCounterW(g.q, L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", 0, &g.diskRead);
    PdhAddEnglishCounterW(g.q, L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", 0, &g.diskWrite);
    PdhAddEnglishCounterW(g.q, L"\\Network Interface(*)\\Bytes Received/sec", 0, &g.netIn);
    PdhAddEnglishCounterW(g.q, L"\\Network Interface(*)\\Bytes Sent/sec", 0, &g.netOut);

    if (!g.csInit) { InitializeCriticalSection(&g.cs); g.csInit = true; }
}

double counterDouble(PDH_HCOUNTER c) {
    if (!c) return 0;
    PDH_FMT_COUNTERVALUE v{};
    if (PdhGetFormattedCounterValue(c, PDH_FMT_DOUBLE, nullptr, &v) != ERROR_SUCCESS) return 0;
    return v.doubleValue;
}

float gpuUtilPercent() {
    // GPU Engine has many instances; utilization is per-engine 0..100.
    // PdhGetFormattedCounterArray over all instances would be heavy; a plain
    // formatted value on the wildcard sums usable for a rough load indicator.
    if (!g.gpuUtil) return 0;
    DWORD bufSize = 0, itemCount = 0;
    if (PdhGetFormattedCounterArrayW(g.gpuUtil, PDH_FMT_DOUBLE, &bufSize, &itemCount, nullptr) == PDH_MORE_DATA && bufSize) {
        std::vector<BYTE> buf(bufSize);
        if (PdhGetFormattedCounterArrayW(g.gpuUtil, PDH_FMT_DOUBLE, &bufSize, &itemCount, (PDH_FMT_COUNTERVALUE_ITEM_W*)buf.data()) == ERROR_SUCCESS) {
            auto* items = (PDH_FMT_COUNTERVALUE_ITEM_W*)buf.data();
            double sum = 0, maxv = 0;
            for (DWORD i = 0; i < itemCount; ++i) {
                double v = items[i].FmtValue.doubleValue;
                if (v > 0.5) { sum += v; maxv = (v > maxv) ? v : maxv; }
            }
            // engines overlap; use max(engines) + partial sum credit, clamp 0..100
            double est = maxv + (sum - maxv) * 0.15;
            if (est > 100) est = 100;
            return (float)est;
        }
    }
    return 0;
}

void pushHistory(float cpu, float ram, float gpu) {
    ULONGLONG now = GetTickCount64();
    if (now - g.lastHistTick < 1000) return;
    g.lastHistTick = now;
    EnterCriticalSection(&g.cs);
    g.cpuHist.push_back(cpu); g.ramHist.push_back(ram); g.gpuHist.push_back(gpu);
    while (g.cpuHist.size() > 120) g.cpuHist.pop_front();
    while (g.ramHist.size() > 120) g.ramHist.pop_front();
    while (g.gpuHist.size() > 120) g.gpuHist.pop_front();
    LeaveCriticalSection(&g.cs);
}
} // anonymous namespace

Sample sampleOnce() {
    ensureInit();
    Sample s;
    if (!g.q) return s;

    PdhCollectQueryData(g.q);

    // non-rate values are valid on every sample
    MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        s.ramTotal = ms.ullTotalPhys;
        s.ramUsed = ms.ullTotalPhys - ms.ullAvailPhys;
        s.ramPercent = (float)ms.dwMemoryLoad;
    }
    s.uptimeSec = (DWORD)(GetTickCount64() / 1000);
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        if (Process32FirstW(snap, &pe)) {
            do { s.procCount++; s.threadCount += pe.cntThreads; } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }

    if (!g.firstSampleDone) { g.firstSampleDone = true; return s; } // rate counters need 2 samples

    s.cpuPercent = (float)counterDouble(g.cpuTotal);
    double perf = counterDouble(g.cpuClock); // % of nominal frequency
    if (perf > 0) {
        // approximate current frequency from base clock
        LARGE_INTEGER freq{}; QueryPerformanceFrequency(&freq);
        (void)freq;
        s.cpuClockMHz = (float)(perf); // interpreted by UI as "% of base clock"
    }
    s.gpuPercent = gpuUtilPercent();

    s.diskPercent  = (float)counterDouble(g.diskTime);
    s.diskReadMBs  = (float)(counterDouble(g.diskRead)  / (1024.0 * 1024.0));
    s.diskWriteMBs = (float)(counterDouble(g.diskWrite) / (1024.0 * 1024.0));
    s.netDownKBs   = (float)(counterDouble(g.netIn)  / 1024.0);
    s.netUpKBs     = (float)(counterDouble(g.netOut) / 1024.0);

    pushHistory(s.cpuPercent, s.ramPercent, s.gpuPercent);
    return s;
}

std::vector<float> cpuHistory() { EnterCriticalSection(&g.cs); auto v = std::vector<float>(g.cpuHist.begin(), g.cpuHist.end()); LeaveCriticalSection(&g.cs); return v; }
std::vector<float> ramHistory() { EnterCriticalSection(&g.cs); auto v = std::vector<float>(g.ramHist.begin(), g.ramHist.end()); LeaveCriticalSection(&g.cs); return v; }
std::vector<float> gpuHistory() { EnterCriticalSection(&g.cs); auto v = std::vector<float>(g.gpuHist.begin(), g.gpuHist.end()); LeaveCriticalSection(&g.cs); return v; }

json topProcesses(int n) {
    json arr = json::array();
    // CPU per process via GetProcessTimes delta over 250ms
    struct Row { wstring name; DWORD pid; double cpu; uint64_t ramMB; };
    static std::map<DWORD, ULONGLONG> lastProcTime;
    static ULONGLONG lastTick = 0;

    static CRITICAL_SECTION pcs; static bool pcsInit = false;
    if (!pcsInit) { InitializeCriticalSection(&pcs); pcsInit = true; }

    ULONGLONG now = GetTickCount64();
    const bool firstScan = (lastTick == 0);
    double elapsed = (double)(now - lastTick);
    if (elapsed < 100) elapsed = 100;
    double numCpu = (double)std::thread::hardware_concurrency(); if (!numCpu) numCpu = 8;

    std::map<DWORD, ULONGLONG> cur;
    std::vector<Row> rows;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                if (h) {
                    FILETIME ct{}, et{}, kt{}, ut{};
                    if (GetProcessTimes(h, &ct, &et, &kt, &ut)) {
                        ULONGLONG t = ((ULONGLONG)kt.dwHighDateTime << 32 | kt.dwLowDateTime)
                                    + ((ULONGLONG)ut.dwHighDateTime << 32 | ut.dwLowDateTime);
                        cur[pe.th32ProcessID] = t;
                        ULONGLONG prev = 0;
                        EnterCriticalSection(&pcs);
                        auto it = lastProcTime.find(pe.th32ProcessID);
                        if (it != lastProcTime.end()) prev = it->second;
                        LeaveCriticalSection(&pcs);
                        double cpuPct = firstScan ? 0.0 : (double)(t - prev) / elapsed / 10.0 / numCpu * 100.0; // 100ns units
                        if (cpuPct < 0 || cpuPct > 100) cpuPct = (cpuPct < 0 ? 0 : 100);
                        PROCESS_MEMORY_COUNTERS pmc{}; pmc.cb = sizeof(pmc);
                        uint64_t ramMB = 0;
                        if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc))) ramMB = pmc.WorkingSetSize / (1024ull * 1024ull);
                        rows.push_back({ pe.szExeFile, pe.th32ProcessID, cpuPct, ramMB });
                    }
                    CloseHandle(h);
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }

    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.cpu > b.cpu; });
    int count = 0;
    for (auto& r : rows) {
        if (r.cpu < 0.05 && count >= 12) break;
        arr.push_back({ {"name", narrow(r.name)}, {"pid", r.pid},
                        {"cpu", (r.cpu < 10 ? (int)std::lround(r.cpu * 10) / 10.0 : std::lround(r.cpu))},
                        {"ramMB", r.ramMB} });
        if (++count >= n) break;
    }
    EnterCriticalSection(&pcs);
    lastProcTime = cur;
    lastTick = now;
    LeaveCriticalSection(&pcs);
    return arr;
}

} // namespace ok::monitor
