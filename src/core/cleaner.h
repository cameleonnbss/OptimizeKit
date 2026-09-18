// OptimizeKit - disk & junk cleaner
#pragma once
#include "common.h"

namespace ok::cleaner {

struct Entry {
    wstring label;      // display name
    wstring path;       // directory to purge
    uint64_t bytes = 0; // measured
};

struct Report {
    vector<Entry> entries;
    uint64_t total = 0;
};

// Measure junk in the well-known temp/cache locations (no deletion).
Report measure();

// Delete every file older than 0 days in those locations. Returns freed bytes.
uint64_t purge();

// Empty the recycle bin for all drives (SHERB_NOCONFIRM etc).
void emptyRecycleBin();

// Windows component cleanup (DISM /StartComponentCleanup) - admin only.
bool componentCleanup(wstring& err);

} // namespace ok::cleaner
