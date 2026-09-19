// OptimizeKit - logging v2 implementation
#include "logging2.h"
#include "engine.h"
#include <mutex>
#include <condition_variable>
#include <deque>
#include <ctime>
#include <cstdio>

namespace ok::log2 {

using json = nlohmann::json;

static std::mutex g_mu;
static std::condition_variable g_cv;
static std::deque<std::pair<long long, Record>> g_ring;
static long long g_seq = 0;
constexpr size_t RING_MAX = 800;

static const wchar_t* sevFileTag(Severity s) {
    switch (s) {
        case Severity::Trace:    return L"TRACE   ";
        case Severity::Debug:    return L"DEBUG   ";
        case Severity::Info:     return L"INFO    ";
        case Severity::Success:  return L"SUCCESS ";
        case Severity::Warning:  return L"WARNING ";
        case Severity::Error:    return L"ERROR   ";
        case Severity::Critical: return L"CRITICAL";
    }
    return L"INFO    ";
}

const wchar_t* sevName(Severity s) {
    switch (s) {
        case Severity::Trace:    return L"TRACE";
        case Severity::Debug:    return L"DEBUG";
        case Severity::Info:     return L"INFO";
        case Severity::Success:  return L"SUCCESS";
        case Severity::Warning:  return L"WARNING";
        case Severity::Error:    return L"ERROR";
        case Severity::Critical: return L"CRITICAL";
    }
    return L"INFO";
}

static void push(long long seq, const Record& r) {
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_ring.push_back({ seq, r });
        while (g_ring.size() > RING_MAX) g_ring.pop_front();
    }
    g_cv.notify_all();
}

void write(Severity s, const std::wstring& category, const std::wstring& msg) {
    // file line: "[2026-09-19 14:03:22] SUCCESS  [NETWORK] message"
    time_t now = time(nullptr);
    struct tm tmv{};
    localtime_s(&tmv, &now);
    wchar_t tbuf[32];
    wcsftime(tbuf, 32, L"%Y-%m-%d %H:%M:%S", &tmv);

    wchar_t tonly[16];
    wcsftime(tonly, 16, L"%H:%M:%S", &tmv);

    Record r;
    r.time = tonly;
    r.sev = s;
    r.category = category;
    r.message = msg;

    // to the classic file via the existing logger (keeps .log readers happy)
    std::wstring line = std::wstring(L"[") + tbuf + L"] " + sevFileTag(s) + L"[" + category + L"] " + msg;
    log::line(line);

    long long seq;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        seq = ++g_seq;
    }
    push(seq, r);
}

void info(const std::wstring& c, const std::wstring& m)    { write(Severity::Info, c, m); }
void success(const std::wstring& c, const std::wstring& m) { write(Severity::Success, c, m); }
void warn(const std::wstring& c, const std::wstring& m)    { write(Severity::Warning, c, m); }
void error(const std::wstring& c, const std::wstring& m)   { write(Severity::Error, c, m); }

StreamPage stream(long long afterSeq, unsigned timeoutMs) {
    StreamPage page;
    std::unique_lock<std::mutex> lk(g_mu);
    bool got = g_cv.wait_for(lk, std::chrono::milliseconds(timeoutMs), [&] {
        return !g_ring.empty() && g_ring.back().first > afterSeq;
    });
    if (got) {
        for (auto& [seq, r] : g_ring) {
            if (seq > afterSeq) {
                page.items.push_back(r);
                page.nextSeq = seq;
            }
        }
    }
    if (!page.nextSeq) page.nextSeq = afterSeq;
    return page;
}

json recentJson(int maxLines) {
    json arr = json::array();
    std::lock_guard<std::mutex> lk(g_mu);
    int start = (int)std::max<long long>(0, (long long)g_ring.size() - maxLines);
    long long last = 0;
    int i = 0;
    for (auto& [seq, r] : g_ring) {
        if (i++ < start) continue;
        last = seq;
        arr.push_back({
            {"seq", seq},
            {"time", narrow(r.time)},
            {"sev", narrow(sevName(r.sev))},
            {"cat", narrow(r.category)},
            {"msg", narrow(r.message)},
        });
    }
    return { {"items", arr}, {"nextSeq", last} };
}

} // namespace ok::log2

// ================================================================ settings
namespace ok::settings {

using json = nlohmann::json;

static std::mutex g_cfgMu;

json get(const std::string& key, const json& def) {
    std::lock_guard<std::mutex> lk(g_cfgMu);
    json cfg = engine::loadConfig();
    if (cfg.contains(key)) return cfg[key];
    return def;
}

void set(const std::string& key, const json& value) {
    std::lock_guard<std::mutex> lk(g_cfgMu);
    json cfg = engine::loadConfig();
    cfg[key] = value;
    engine::saveConfig(cfg);
    log2::info(L"SETTINGS", widen("Set " + key + " = " + value.dump()));
}

} // namespace ok::settings
