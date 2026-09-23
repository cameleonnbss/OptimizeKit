// OptimizeKit - driver update & maintenance (v2.7)
// The app cannot (and should not) silently install drivers, but it can do what
// Windows Update does, from the tool that is already on the machine:
//   - trigger the USOClient driver scan (the same one Settings > Windows Update runs)
//   - report the age of GPU / audio / NIC / chipset drivers as read from the
//     driver store (setupapi), so "your GPU driver is 14 months old" is a real fact
//   - open the right vendor page (NVIDIA / AMD / Intel / Realtek) for the manual step
#pragma once
#include "common.h"
#include "json.hpp"

namespace ok::drvupdate {

using json = nlohmann::json;

// COM is initialized per-thread by the app; report() is safe to call after that.
void DrvInit();   // no-op placeholder kept for API symmetry with firmware::FwInit

// { gpu:{name, version, date, ageDays}, audio:[...], net:[...], summary }
json report();

// { triggered, method, note } — UsoClient StartInteractiveScan / ScanSourceArgument Drivers
json scanWindowsUpdate(bool driversOnly = true);

// Optional: run pnputil /scan-devices so PnP re-checks the tree (harmless, read-mostly)
json rescanDevices();

// Devices with a problem code from WMI Win32_PnPEntity (ConfigManagerErrorCode != 0)
json problemDevices();

// One-click: open the vendor download page for the current GPU (NVIDIA/AMD/Intel)
bool openVendorPage();

// Fallback action when the user wants the Windows path with a visible window
void openWindowsUpdateUi();

} // namespace ok::drvupdate
