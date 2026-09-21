// OptimizeKit - native frame window hosting the dashboard via WebView2
#pragma once
#include "common.h"

namespace ok::webframe {

// true when WebView2Loader.dll ships next to the exe AND an Evergreen
// WebView2 runtime is installed (preinstalled on Windows 10/11).
bool available();

// Open a real desktop window (own title bar, app icon, dark frame) hosting
// the dashboard at the given loopback URL. Blocks until the window closes.
// Returns 0 on success, nonzero when the window could not be created
// (caller should fall back to the browser).
int runWindow(const wstring& url);

} // namespace ok::webframe
