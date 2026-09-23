// OptimizeKit - firmware & platform security inventory (read-only)
// "Closer to the kernel/BIOS" = measure what the firmware actually told Windows:
// Secure Boot, TPM, virtualization, boot mode, power profile, HPET, WPBT, PATCH/MSI,
// kernel dumps, Dynamic Tick/TSC options set via bcdedit. This module NEVER writes
// anything: firmware state is read, kernel boot options are reported, and the
// dashboard links to the BIOS guide instead of touching the machine.
#pragma once
#include "common.h"
#include "json.hpp"

namespace ok::firmware {

using json = nlohmann::json;

// Call once on a thread that owns a COM apartment before inventory()/summaryLine()
void FwInit();   // CoInitializeEx security for the WMI reads

// One-shot inventory: { secureBoot, tpm:{present,ready,version}, virtualization,
// biosVendor, biosVersion, biosDate, motherboard, bootMode, powerProfile, hpet,
// wpbt, patchGuard, msiSupported, dynamicTick, useplatformtick, dumpLevel,
// svcSplitThreshold, rebootNeeded }
json inventory();

// Human-readable summary used by the CLI (--firmware) and the dashboard header
wstring summaryLine();

} // namespace ok::firmware
