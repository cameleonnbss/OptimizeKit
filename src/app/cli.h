// OptimizeKit - numbered CLI
#pragma once
#include "common.h"

namespace ok::cli {

// runs the user (non-admin) or the full admin menu
int runUserMenu();
int runAdminMenu();

inline int run(bool admin) { return admin ? runAdminMenu() : runUserMenu(); }

} // namespace ok::cli
