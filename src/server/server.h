// OptimizeKit - embedded HTTP server serving the WormGPT-style web dashboard
#pragma once
#include "common.h"

namespace ok::server {

// Start serving on 127.0.0.1:port (blocks until the server stops).
// Returns the chosen port if it started.
int serve(unsigned short preferredPort);

} // namespace ok::server
