// OptimizeKit - logging v2: severity, category, streaming (ring buffer + waiters),
// plus a tiny persistent settings store on top of config.json.
#pragma once
#include "common.h"
#include "json.hpp"
#include <functional>

namespace ok::log2 {

using json = nlohmann::json;

enum class Severity { Trace, Debug, Info, Success, Warning, Error, Critical };

// One log record (also stored in the in-memory ring for the UI stream)
struct Record {
    std::wstring time;        // "21:42:01"
    Severity sev;
    std::wstring category;    // ENGINE / NETWORK / GAMING / CLEANER / SCAN / SYSTEM / API...
    std::wstring message;
};

// Write a full record (also appended to the file and the ring)
void write(Severity s, const std::wstring& category, const std::wstring& msg);

// Sugar
void info(const std::wstring& cat, const std::wstring& msg);
void success(const std::wstring& cat, const std::wstring& msg);
void warn(const std::wstring& cat, const std::wstring& msg);
void error(const std::wstring& cat, const std::wstring& msg);

// Streaming API for the UI: returns records newer than `afterSeq`.
// Blocks up to `timeoutMs` if none are available (long-poll), returns immediately when any.
struct StreamPage { std::vector<Record> items; long long nextSeq = 0; };
StreamPage stream(long long afterSeq, unsigned timeoutMs);

// Whole ring as JSON (page reload)
json recentJson(int maxLines);

// severity -> "INFO"/"SUCCESS"/... string
const wchar_t* sevName(Severity s);

} // namespace ok::log2

// --------------------------------------------------------------- settings
namespace ok::settings {

using json = nlohmann::json;

// Access the live config.json object (thread-safe get/set of keys).
json get(const std::string& key, const json& def = json());
void set(const std::string& key, const json& value);   // persists immediately

} // namespace ok::settings
