// OptimizeKit - full system scanner (JSON findings -> real action ids)
#pragma once
#include "common.h"
#include "json.hpp"

namespace ok::scan {
using json = nlohmann::json;

// Scans storage junk, tweak state, network config, power plan, game mode/HAGS,
// startup load, processes, drivers and detected games. Returns
// { findings: [ {id,title,detail,severity,impact,action:{type,id}} ], summary }.
json runScan();

} // namespace ok::scan
