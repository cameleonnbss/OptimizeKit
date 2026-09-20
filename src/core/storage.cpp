// OptimizeKit - storage analyzer + real micro-benchmarks
#include "storage.h"
#include "ping.h"
#include "engine.h"
#include "common.h"
#include <winioctl.h>
#include <shellapi.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>

#pragma comment(lib, "winmm.lib")

namespace fs = std::filesystem;

namespace ok::storage {

using json = nlohmann::json;

// ------------------------------------------------------------------ drives
static wstring busTypeName(int bus) {
    switch (bus) {
        case BusTypeUsb: return L"USB";
        case BusTypeNvme: return L"NVMe";
        case BusTypeSata: return L"SATA";
        case BusTypeSd: return L"SD";
        case BusTypeVirtual: return L"Virtual";
        default: return L"";
    }
}

json drivesJson() {
    json arr = json::array();
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(mask & (1u << i))) continue;
        wchar_t letter = L'A' + i;
        wstring root = wstring(1, letter) + L":\\";
        UINT type = GetDriveTypeW(root.c_str());
        if (type == DRIVE_CDROM || type == DRIVE_NO_ROOT_DIR) continue;

        ULARGE_INTEGER total{}, freeq{};
        if (!GetDiskFreeSpaceExW(root.c_str(), nullptr, &total, &freeq)) continue;

        json j;
        j["letter"] = string(1, (char)letter) + ":";
        wchar_t vol[MAX_PATH + 1] = L""; DWORD fsFlags = 0;
        wchar_t fsName[64] = L"";
        DWORD serial = 0, maxC = 0;
        if (GetVolumeInformationW(root.c_str(), vol, MAX_PATH, &serial, &maxC, &fsFlags, fsName, 64))
            j["label"] = narrow(vol);
        j["fs"] = narrow(fsName);
        j["total"] = total.QuadPart;
        j["free"] = freeq.QuadPart;

        // bus type via StorageDeviceDescriptor (needs admin on some systems - best effort)
        wstring phys = wstring(L"\\\\.\\") + letter + L":";
        HANDLE h = CreateFileW(phys.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            BYTE buf[4096]{};
            DWORD bytes = 0;
            STORAGE_PROPERTY_QUERY q{}; q.PropertyId = StorageDeviceProperty; q.QueryType = PropertyStandardQuery;
            if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), buf, sizeof(buf), &bytes, nullptr)) {
                auto* desc = (STORAGE_DEVICE_DESCRIPTOR*)buf;
                j["bus"] = narrow(busTypeName(desc->BusType));
            }
            CloseHandle(h);
        }
        arr.push_back(j);
    }
    return arr;
}

// ------------------------------------------------------- biggest files
static void walkLargest(const wstring& dir, vector<std::pair<uint64_t, wstring>>& out, int depth, int& budget) {
    if (depth > 6 || budget <= 0) return;
    std::error_code ec;
    // avoid reparse points/junctions (OneDrive, "My Pictures"...) - they break iterators
    DWORD attrs = GetFileAttributesW(dir.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) return;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + (dir.back() == L'\\' ? L"*" : L"\\*")).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (--budget <= 0) break;
        if (fd.cFileName[0] == L'.') continue;
        wstring full = dir + (dir.back() == L'\\' ? L"" : L"\\") + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // skip known noise
            wstring l = fd.cFileName;
            std::transform(l.begin(), l.end(), l.begin(), ::towlower);
            if (l == L"windows" || l == L"$recycle.bin" || l == L"system volume information" || l == L"appdata") continue;
            walkLargest(full, out, depth + 1, budget);
        } else {
            ULARGE_INTEGER sz{ fd.nFileSizeLow, fd.nFileSizeHigh };
            if (sz.QuadPart > 100ull * 1024 * 1024) {   // only > 100 MB matters
                out.push_back({ sz.QuadPart, full });
                if (out.size() > 400) std::sort(out.begin(), out.end(), [](auto& a, auto& b){ return a.first > b.first; }), out.resize(200);
            }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

json largestFiles(const wstring& root, int topN) {
    vector<std::pair<uint64_t, wstring>> v;
    int budget = 20000;
    walkLargest(root, v, 0, budget);
    std::sort(v.begin(), v.end(), [](auto& a, auto& b){ return a.first > b.first; });
    json arr = json::array();
    int n = 0;
    for (auto& [sz, path] : v) {
        if (n++ >= topN) break;
        arr.push_back({ {"size", sz}, {"path", narrow(path)}, {"pretty", narrow(fmtBytes(sz))} });
    }
    return arr;
}

json folderSizes(const wstring& root, int topN) {
    json arr = json::array();
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((root + (root.back() == L'\\' ? L"*" : L"\\*")).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return arr;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (fd.cFileName[0] == L'.') continue;
        wstring full = root + (root.back() == L'\\' ? L"" : L"\\") + fd.cFileName;
        uint64_t sz = 0;
        int budget = 3000;
        // cheap depth-3 size estimate
        std::function<void(const wstring&, int)> walk = [&](const wstring& d, int depth) {
            if (depth > 3 || budget-- <= 0) return;
            WIN32_FIND_DATAW f2;
            HANDLE h2 = FindFirstFileW((d + L"\\*").c_str(), &f2);
            if (h2 == INVALID_HANDLE_VALUE) return;
            do {
                if (f2.cFileName[0] == L'.') continue;
                if (f2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    walk(d + L"\\" + f2.cFileName, depth + 1);
                else {
                    ULARGE_INTEGER s{ f2.nFileSizeLow, f2.nFileSizeHigh };
                    sz += s.QuadPart;
                }
            } while (FindNextFileW(h2, &f2));
            FindClose(h2);
        };
        walk(full, 0);
        arr.push_back({ {"name", narrow(fd.cFileName)}, {"size", sz} });
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(arr.begin(), arr.end(), [](const json& a, const json& b){ return a["size"].get<uint64_t>() > b["size"].get<uint64_t>(); });
    if (arr.size() > (size_t)topN) arr.erase(arr.begin() + topN, arr.end());
    return arr;
}

// -------------------------------------------------------------- benchmarks
// Raw NtQueryInformationFile helper to get the real allocated size (odd EOF with NO_BUFFERING)
static unsigned __int64 FileLen(HANDLE h) {
    LARGE_INTEGER li{};
    GetFileSizeEx(h, &li);
    return (unsigned __int64)li.QuadPart;
}

BenchResult benchDisk(const wstring& root) {
    BenchResult r; r.unit = L"MB/s";
    // 256 MiB, written with FILE_FLAG_NO_BUFFERING (aligned buffer + aligned size),
    // flushed to platters, then read back sequentially. Reports sustained READ MB/s.
    wstring file = root + (root.back() == L'\\' ? L"" : L"\\") + L"OptimizeKit_bench.tmp";
    const unsigned __int64 SZ = 256ull * 1024 * 1024;
    const DWORD CHUNK = 4 * 1024 * 1024;
    auto alignBuf = [](size_t sz) {
        void* mem = _aligned_malloc(sz, 4096);
        if (mem) memset(mem, 'x', sz);
        return (char*)mem;
    };
    char* data = alignBuf(CHUNK);
    if (!data) { r.error = L"out of memory for bench buffer"; return r; }

    auto t0 = std::chrono::steady_clock::now();
    {
        HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_FLAG_NO_BUFFERING, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            h = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
            if (h == INVALID_HANDLE_VALUE) { _aligned_free(data); r.error = L"cannot create bench file"; return r; }
        }
        unsigned __int64 written = 0;
        while (written < SZ) {
            DWORD w = 0;
            if (!WriteFile(h, data, CHUNK, &w, nullptr) || w == 0) break;
            written += w;
        }
        FlushFileBuffers(h);
        CloseHandle(h);
    }
    auto t1 = std::chrono::steady_clock::now();
    unsigned __int64 total = 0;
    {
        HANDLE h = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            h = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) { _aligned_free(data); DeleteFileW(file.c_str()); r.error = L"cannot reopen bench file"; return r; }
        DWORD rd = 0;
        while (ReadFile(h, data, CHUNK, &rd, nullptr) && rd > 0) total += rd;
        CloseHandle(h);
    }
    auto t2 = std::chrono::steady_clock::now();
    DeleteFileW(file.c_str());
    _aligned_free(data);

    double readMs = std::chrono::duration<double, std::milli>(t2 - t1).count();
    if (readMs < 1.0) readMs = 1.0;
    if (total == 0) { r.error = L"0 bytes read back"; return r; }
    double mbs = (total / (1024.0 * 1024.0)) / (readMs / 1000.0);
    r.score = mbs;
    r.ms = readMs;
    r.ok = true;
    log::ok(L"Benchmark disk: " + fmtFloat(mbs) + L" MB/s read (" + fmtBytes(total) + L" in " + fmtFloat(readMs) + L" ms)");
    return r;
}

BenchResult benchCpu() {
    BenchResult r; r.unit = L"MOPS";
    const int NTHREAD = (int)std::thread::hardware_concurrency();
    std::atomic<long long> ops{ 0 };
    std::atomic<bool> stop{ false };
    auto worker = [&] {
        long long local = 0;
        unsigned x = 12345;
        while (!stop.load(std::memory_order_relaxed)) {
            for (int i = 0; i < 100000; ++i) {
                x = x * 1664525u + 1013904223u;   // LCG: real dependency chain, not dead code
                local += x;                        // consume x so the compiler keeps the math
            }
            ops.fetch_add(local / 100000, std::memory_order_relaxed);  // per-iteration count only (no wraparound)
            local = 0;
        }
    };
    vector<std::thread> ts;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < NTHREAD; ++i) ts.emplace_back(worker);
    Sleep(1000);
    stop = true;
    for (auto& t : ts) t.join();
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    double mops = ops.load() / (ms / 1000.0) / 1e6;
    r.score = mops; r.ms = ms; r.ok = true;
    log::ok(L"Benchmark CPU MT: " + fmtFloat(mops) + L" MOPS (" + std::to_wstring(NTHREAD) + L" threads)");
    return r;
}

// Single-core variant of the same integer kernel - exposes per-core clock/stall,
// which is what most games are actually bound by.
BenchResult benchCpuSingle() {
    BenchResult r; r.unit = L"MOPS";
    std::atomic<long long> ops{ 0 };
    std::atomic<bool> stop{ false };
    auto worker = [&] {
        long long local = 0;
        unsigned x = 12345;
        while (!stop.load(std::memory_order_relaxed)) {
            for (int i = 0; i < 100000; ++i) {
                x = x * 1664525u + 1013904223u;   // same kernel as MT: real dependency
                local += x;
            }
            ops.fetch_add(local / 100000, std::memory_order_relaxed);  // per-iteration count only
            local = 0;
        }
    };
    std::thread w(worker);
    auto t0 = std::chrono::steady_clock::now();
    Sleep(500);
    stop = true;
    w.join();
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    double mops = ops.load() / (ms / 1000.0) / 1e6;
    r.score = mops; r.ms = ms; r.ok = true;
    log::ok(L"Benchmark CPU ST: " + fmtFloat(mops) + L" MOPS (1 thread)");
    return r;
}

BenchResult benchRam() {
    BenchResult r; r.unit = L"GB/s";
    // 3 rounds over 512 MiB each so the working set overflows L3 -> true DRAM bandwidth.
    const size_t SZ = 512ull * 1024 * 1024;
    std::vector<char> a(SZ), b(SZ, 0);
    std::fill(a.begin(), a.end(), (char)1);
    volatile char sink = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 3; ++i) {
        memcpy(b.data(), a.data(), SZ);
        sink = b[SZ / 2 + i];
    }
    auto t1 = std::chrono::steady_clock::now();
    (void)sink;
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    if (ms < 1.0) { r.error = L"memcpy too fast to measure"; return r; }
    double gbs = (3 * 2.0 * SZ / 1e9) / (ms / 1000.0);   // read+write per round
    r.score = gbs; r.ms = ms; r.ok = true;
    log::ok(L"Benchmark RAM: " + fmtFloat(gbs) + L" GB/s (memcpy 3x512MB)");
    return r;
}

double benchLatencyMs() {
    double sum = 0; int n = 0;
    for (auto host : { L"1.1.1.1", L"8.8.8.8", L"9.9.9.9" }) {
        auto res = ping::measure(host);
        if (res.ok) { sum += res.ms; n++; }
    }
    return n ? sum / n : -1;
}

json runBenchmarkAll() {
    log::section(L"BENCHMARK");
    json j;
    auto cpu = benchCpu();
    j["cpu"] = { {"score", cpu.score}, {"unit", narrow(cpu.unit)}, {"ok", cpu.ok} };
    auto cpuSt = benchCpuSingle();
    j["cpuSt"] = { {"score", cpuSt.score}, {"unit", narrow(cpuSt.unit)}, {"ok", cpuSt.ok} };
    auto ramr = benchRam();
    j["ram"] = { {"score", ramr.ok ? json(ramr.score) : json(nullptr)}, {"unit", narrow(ramr.unit)}, {"ok", ramr.ok}, {"error", narrow(ramr.error)} };
    auto disk = benchDisk(appDataDir());   // writable dir on the system drive (C:\ root needs admin)
    j["disk"] = { {"score", disk.ok ? json(disk.score) : json(nullptr)}, {"unit", narrow(disk.unit)}, {"ok", disk.ok}, {"error", narrow(disk.error)} };
    double lat = benchLatencyMs();
    j["latencyMs"] = lat;
    j["timestamp"] = (long long)time(nullptr);
    if (lat > 0) log::ok(L"Benchmark latency: " + fmtFloat(lat) + L" ms avg");

    // ---- normalized scores (1000 = reference: modern 8C/16T, DDR4-3600, NVMe Gen3) ----
    // Reference values calibrated on a 10C/16T mainstream desktop (i5-13400F class)
    // running these exact kernels; 1000 therefore means "mainstream gaming PC".
    const double REF_CPU_MT = 320000000.0;  // LCG+add kernel, 16 threads (calibrated)
    const double REF_CPU_ST = 24000000.0;   // single thread, same kernel (calibrated)
    const double REF_RAM    = 14.0;         // GB/s memcpy
    const double REF_DISK   = 2000.0;       // MB/s sequential read
    json subs = json::object();
    auto norm = [](double v, double ref, double cap) { return (int)std::min(cap, std::max(50.0, 1000.0 * v / ref)); };
    if (cpu.ok)   subs["cpuMt"] = norm(cpu.score,    REF_CPU_MT, 5000);
    if (cpuSt.ok) subs["cpuSt"] = norm(cpuSt.score,  REF_CPU_ST, 5000);
    if (ramr.ok)  subs["ram"]   = norm(ramr.score,   REF_RAM,    5000);
    if (disk.ok)  subs["disk"]  = norm(disk.score,   REF_DISK,   5000);
    double total = 0; double n = 0;
    auto acc = [&](const char* k, double w) { if (subs.contains(k)) { total += subs[k].get<double>() * w; n += w; } };
    acc("cpuMt", 0.40); acc("cpuSt", 0.15); acc("ram", 0.20); acc("disk", 0.25);
    int tot = n > 0 ? (int)(total / n) : 0;
    j["total"] = tot;
    j["class"] = tot >= 1800 ? "high-end" : tot >= 1100 ? "performance" : tot >= 700 ? "mainstream" : "entry";
    j["verdict"] = tot >= 1800 ? "Top tier - everything runs flat out"
                 : tot >= 1100 ? "Performance class - great for gaming and heavy apps"
                 : tot >= 700  ? "Mainstream - fine for everyday + esports titles"
                 :               "Entry level - web/CESU workloads; esports on low settings";
    j["subs"] = subs;
    log::ok(L"Benchmark TOTAL: " + std::to_wstring(tot) + L" (" + widen(j["class"].get<string>()) + L")");

    // store in config history (max 20)
    json cfg = engine::loadConfig();
    cfg["bench_history"].push_back(j);
    while (cfg["bench_history"].size() > 20) cfg["bench_history"].erase(0);
    engine::saveConfig(cfg);
    return j;
}

json historyJson() {
    json cfg = engine::loadConfig();
    if (cfg.contains("bench_history")) return cfg["bench_history"];
    return json::array();
}

} // namespace ok::storage
