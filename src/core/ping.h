// OptimizeKit - ICMP latency prober
#pragma once
#include "common.h"

namespace ok::ping {

struct Result {
    bool   ok = false;
    DWORD  ms = 0;      // round-trip, 0 means <1ms
    wstring status;      // human readable
};

// 4 echoes, returns the average of successful replies (WIN32 IcmpSendEcho)
Result measure(const wstring& host);

} // namespace ok::ping
