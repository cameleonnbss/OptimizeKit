// OptimizeKit - storage analyzer + real micro-benchmarks (no fabricated numbers)
#pragma once
#include "common.h"
#include "json.hpp"

namespace ok::storage {

using json = nlohmann::json;

// Drives: letter, label, total/free, type (SSD/HDD/USB via BusType), health%
json drivesJson();

// Largest files under a root (top N by size, shallow-ish walk with limits)
json largestFiles(const wstring& root, int topN);

// Folder sizes for the top level of a root (dashboard pie)
json folderSizes(const wstring& root, int topN);

// ---------------- benchmarks (real measured values) ----------------
struct BenchResult {
    double score = 0;          // meaningful unit per test
    wstring unit;
    double ms = 0;             // wall time of the test
    bool ok = false;
    wstring error;
};

// Sequential disk read: reads a 256 MB file it just wrote, reports MB/s
BenchResult benchDisk(const wstring& root);
// CPU: multi-threaded integer ops for ~800 ms, reports MOPS (million ops/s)
BenchResult benchCpu();
// RAM: 512 MB copy loop, reports GB/s
BenchResult benchRam();
// Latency summary (4 pings x 3 targets, avg)
double benchLatencyMs();

json runBenchmarkAll();   // {cpu:{...},disk:{...},ram:{...},latencyMs:12.3,timestamp}
json historyJson();       // past runs from config["bench_history"]

} // namespace ok::storage
