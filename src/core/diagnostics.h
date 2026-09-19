// OptimizeKit - honest diagnostics (all values are measured, never invented)
#pragma once
#include "common.h"
#include "json.hpp"

namespace ok::diag {
using json = nlohmann::json;

// One-shot full diagnostics:
// { clockMHz, cpuTemp:{available}, dpc:{dpcPercent,isrPercent,verdict},
//   network:{avgMs,jitterMs,lossPercent,minMs,maxMs,verdict},
//   disk:{avgLatencyMs,verdict}, game:{running,process,pid} }
json runAll();

} // namespace ok::diag
