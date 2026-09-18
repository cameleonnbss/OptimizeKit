// OptimizeKit - driver info & game-driver maintenance
#pragma once
#include "common.h"

namespace ok::drivers {

struct Info {
    wstring gpuName, driverVersion, driverDate;
    wstring audioHwids;          // sample hwid of the first audio device
};

Info collect();

// Open the vendor driver download page (NVIDIA / AMD / Intel) in the default browser.
void openVendorPage();

// nvWmi-free nvidia trick: run nvidia-smi if present to fetch the driver version
wstring nvidiaSmiVersion();

// Launch the legacy DirectX control panel (which exposes driver info)
void openDxdiag();

// Windows Update driver scan (admin): trigger a driver scan via USOClient
void scanWindowsUpdateDrivers();

} // namespace ok::drivers
