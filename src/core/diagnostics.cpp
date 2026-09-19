// OptimizeKit - honest diagnostics module.
// Everything here is MEASURED on this PC right now. Where a value cannot be
// measured without a kernel driver, the JSON says "available": false and the
// UI shows "Diagnostic" instead of inventing a number.
#include "diagnostics.h"
#include "common.h"
#include "ping.h"
#include <pdh.h>
#include <pdhmsg.h>
#include <tlhelp32.h>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "pdh.lib")

namespace ok::diag {

using json = nlohmann::json;

// ---------------------------------------------------------------- cpu clock
// Effective clock = base clock x "% Processor Performance" (same source as
// Task Manager). PDH, no kernel driver needed.
static double effectiveClockMHz() {
    static PDH_HQUERY q = nullptr;
    static PDH_HCOUNTER c = nullptr;
    if (!q) {
        if (PdhOpenQueryW(nullptr, 0, &q) != ERROR_SUCCESS) return 0;
        if (PdhAddEnglishCounterW(q, L"\\Processor Information(_Total)\\% Processor Performance", 0, &c) != ERROR_SUCCESS) { PdhCloseQuery(q); q = nullptr; return 0; }
        PdhCollectQueryData(q);
    }
    Sleep(300);
    if (PdhCollectQueryData(q) != ERROR_SUCCESS) return 0;
    PDH_FMT_COUNTERVALUE v;
    if (PdhGetFormattedCounterValue(c, PDH_FMT_DOUBLE, nullptr, &v) != ERROR_SUCCESS) return 0;
    DWORD baseMHz = 0; DWORD sz = sizeof(baseMHz);
    RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"~MHz", RRF_RT_REG_DWORD, nullptr, &baseMHz, &sz);
    if (!baseMHz) return 0;
    return baseMHz * v.doubleValue / 100.0;
}

// ---------------------------------------------------------------- cpu temp
// Userland Windows does not expose CPU temperature without a vendor/kernel
// driver. We report that honestly instead of inventing a value.
static json cpuTemp() {
    json j;
    j["available"] = false;
    j["celsius"] = nullptr;
    j["why"] = "CPU temperature needs a vendor driver (MSI Afterburner, HWiNFO). Windows does not expose it to userland.";
    return j;
}

// ------------------------------------------------------------- dpc / isr
// % DPC time + % Interrupt time via PDH (2 s sample). Percentage of CPU time
// stolen by kernel deferred procedure calls - the honest, no-driver metric.
static json dpcIsr() {
    json j;
    PDH_HQUERY q = nullptr;
    PDH_HCOUNTER cd = nullptr, ci = nullptr;
    if (PdhOpenQueryW(nullptr, 0, &q) != ERROR_SUCCESS) { j["available"] = false; return j; }
    PdhAddEnglishCounterW(q, L"\\Processor(_Total)\\% DPC Time", 0, &cd);
    PdhAddEnglishCounterW(q, L"\\Processor(_Total)\\% Interrupt Time", 0, &ci);
    PdhCollectQueryData(q);
    Sleep(2000);
    PdhCollectQueryData(q);
    PDH_FMT_COUNTERVALUE vd{}, vi{};
    double dp = 0, ip = 0;
    if (PdhGetFormattedCounterValue(cd, PDH_FMT_DOUBLE, nullptr, &vd) == ERROR_SUCCESS) dp = vd.doubleValue;
    if (PdhGetFormattedCounterValue(ci, PDH_FMT_DOUBLE, nullptr, &vi) == ERROR_SUCCESS) ip = vi.doubleValue;
    PdhCloseQuery(q);
    j["available"] = true;
    j["dpcPercent"] = std::round(dp * 10) / 10.0;
    j["isrPercent"] = std::round(ip * 10) / 10.0;
    const char* verdict;
    if (dp > 15.0 || ip > 10.0) verdict = "high - a driver is burning CPU in kernel mode (run LatencyMon to find it)";
    else if (dp > 5.0)          verdict = "moderate - some DPC load, usually audio or network drivers";
    else                        verdict = "healthy";
    j["verdict"] = verdict;
    return j;
}

// ------------------------------------------------------------- network qos
// 20 real pings to 1.1.1.1: min/max/avg + RFC3550-style jitter + loss.
static json netQuality() {
    json j;
    std::vector<double> ms;
    int sent = 20, lost = 0;
    for (int i = 0; i < sent; ++i) {
        auto r = ping::measure(L"1.1.1.1");
        if (r.ok) ms.push_back(r.ms); else lost++;
        Sleep(60);
    }
    j["target"] = "1.1.1.1";
    j["sent"] = sent;
    if (ms.empty()) { j["available"] = false; j["lossPercent"] = 100.0; return j; }
    double sum = 0; for (double m : ms) sum += m;
    double avg = sum / ms.size();
    double jit = 0;
    for (size_t i = 1; i < ms.size(); ++i) jit += std::fabs(ms[i] - ms[i - 1]);
    jit = ms.size() > 1 ? jit / (ms.size() - 1) : 0;
    double mn = *std::min_element(ms.begin(), ms.end());
    double mx = *std::max_element(ms.begin(), ms.end());
    j["available"] = true;
    j["received"] = (int)ms.size();
    double lossPct = lost * 100.0 / sent;
    j["lossPercent"] = std::round(lossPct * 10) / 10.0;
    j["minMs"] = std::round(mn * 10) / 10.0;
    j["maxMs"] = std::round(mx * 10) / 10.0;
    j["avgMs"] = std::round(avg * 10) / 10.0;
    j["jitterMs"] = std::round(jit * 10) / 10.0;
    const char* verdict = "excellent - esport ready";
    if (lossPct > 1.0 || jit > 5.0)      verdict = "unstable - not usable for competitive play";
    else if (lossPct > 0.5 || jit > 2.0) verdict = "acceptable - jitter visible in fast games";
    else if (jit > 1.0)                  verdict = "good";
    j["verdict"] = verdict;
    return j;
}

// ------------------------------------------------------------- disk latency
// Real 4K sequential-read latency over a 1 MB unbuffered scratch file.
static json diskLatency() {
    json j;
    j["available"] = false;
    wstring file = appDataDir() + L"\\ok_diag.tmp";
    HANDLE h = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) return j;
    char* buf = (char*)_aligned_malloc(4096, 4096);
    if (!buf) { CloseHandle(h); return j; }
    memset(buf, 0xAB, 4096);
    DWORD w = 0;
    for (int i = 0; i < 256; ++i) WriteFile(h, buf, 4096, &w, nullptr);
    FlushFileBuffers(h);
    auto t0 = std::chrono::steady_clock::now();
    int reads = 0;
    for (int i = 0; i < 40; ++i) {
        LARGE_INTEGER off; off.QuadPart = (LONGLONG)(i * 4096) % (256 * 4096);
        SetFilePointerEx(h, off, nullptr, FILE_BEGIN);
        DWORD r = 0;
        if (ReadFile(h, buf, 4096, &r, nullptr) && r) reads++;
    }
    auto t1 = std::chrono::steady_clock::now();
    CloseHandle(h);
    _aligned_free(buf);
    if (reads < 10) return j;
    double totalMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    j["available"] = true;
    j["avgLatencyMs"] = std::round(totalMs / reads * 1000.0) / 1000.0;
    const char* verdict;
    double per = totalMs / reads;
    if (per < 0.5)  verdict = "NVMe-class";
    else if (per < 2.0)  verdict = "SATA SSD";
    else if (per < 15.0) verdict = "HDD (mechanical)";
    else                 verdict = "very slow storage";
    j["verdict"] = verdict;
    return j;
}

// ------------------------------------------------------------- game running
// Heuristic scan of running processes for known game exe names.
static json gameRunning() {
    json j;
    j["running"] = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return j;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    static const wchar_t* hints[] = {
        L"valorant", L"rdr2", L"cs2", L"fortniteclient", L"cyberpunk", L"eldenring",
        L"gta5", L"fivem", L"rainbowsix", L"pubg", L"overwatch", L"apex", L"hogwarts",
        L"starfield", L"minecraft", L"robloxplayer", L"league of legends", L"aoe2de",
        L"rocketleague", L"thefinals", L"marvelrivals", L"nightmare",
    };
    if (Process32FirstW(snap, &pe)) {
        do {
            wstring n = pe.szExeFile;
            std::transform(n.begin(), n.end(), n.begin(), ::towlower);
            for (auto hint : hints) {
                if (n.find(hint) != wstring::npos) {
                    j["running"] = true;
                    j["process"] = narrow(pe.szExeFile);
                    j["pid"] = (int)pe.th32ProcessID;
                    break;
                }
            }
            if (j["running"].get<bool>()) break;
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return j;
}

json runAll() {
    log::section(L"DIAGNOSTICS");
    json j;
    j["timestamp"] = (long long)time(nullptr);
    j["clockMHz"] = std::round(effectiveClockMHz());
    j["cpuTemp"] = cpuTemp();
    j["dpc"] = dpcIsr();
    j["network"] = netQuality();
    j["disk"] = diskLatency();
    j["game"] = gameRunning();
    log::ok(L"diagnostics complete");
    return j;
}

} // namespace ok::diag
