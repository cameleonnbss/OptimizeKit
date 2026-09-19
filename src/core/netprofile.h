// OptimizeKit - network optimization profiles (every change rollback-able)
#pragma once
#include "common.h"
#include "json.hpp"

namespace ok::netprofile {

using json = nlohmann::json;

// Adapter info for the UI
struct Adapter {
    wstring name;        // friendly name
    wstring description; // model
    wstring type;        // Ethernet / Wi-Fi
    wstring speed;       // link speed string
    bool powerSaving = false;  // NIC power saving currently enabled?
};

struct Status {
    wstring autotuning;   // normal | disabled | experimental ...
    bool rscEnabled = false;
    bool rssEnabled = false;
    wstring dns;          // primary DNS of the active adapter
    int mtu = 0;
};

vector<Adapter> adapters();
Status status();
json statusJson();

// Apply a profile by name: "balanced" | "gaming" | "low_latency" | "download" | "restore"
// Every action is logged (severity/category). Returns number of changes + errors.
struct Result { int applied = 0; int failed = 0; vector<wstring> errors; };
Result apply(const string& profile);

// Individual toggles used by the UI cards (all reversible via restoreAll())
bool setNicPowerSaving(bool off, wstring& err);   // off=true => disable power saving (latency)
bool setRsc(bool enabled, wstring& err);
bool setAutotuning(const wstring& level, wstring& err); // normal|disabled|experimental|highlyrestricted
bool setMtu(int mtu, wstring& err);
bool flushDns(wstring& err);
bool setDns(const wstring& primary, const wstring& secondary, wstring& err);

} // namespace ok::netprofile
