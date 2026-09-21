// OptimizeKit v2.6 - DiskScope-inspired storage intelligence.
//
// Two features imported from the DiskScope-CLI project (github.com/cameleonnbss/DiskScope-CLI,
// same author, MIT) and rebuilt natively for OptimizeKit:
//
//   * driveAndFolders() - "quick analyse": per-drive overview plus the first-level
//                         folder sizes of a drive, the way DiskScope's menu 2 does it,
//                         with per-folder file counts;
//   * cleanupTargets()  - DiskScope's SAFE cleanup list (known cache locations with
//                         their reason string) measured on the live disk;
//   * duplicates()      - the bounded duplicate finder: group by exact size, hash the
//                         head of each file, then full-hash the collisions. SHA-256 is
//                         fetched from bcrypt.dll (Vista+), no dependency.
//
// As in DiskScope, nothing is deleted by this module - the endpoint only measures;
// deletion stays with the existing /api/clean (junk purge with recycle-bin API).
#pragma once
#include "common.h"
#include "json.hpp"

namespace ok::diskscope {

using json = nlohmann::json;

// Drive overview + top first-level folders of `drive` ("C:"). Bounded walk: depth 1,
// capped folder count, cancelable by time budget.
json driveAndFolders(const wstring& drive, int topN = 20, int budgetMs = 15000);

// The SAFE cleanup targets (caches, temp, thumbs, shader caches...) measured live.
// Each row: path, reason, bytes present.
json cleanupTargets();

// Duplicate files among the biggest files under `root`. Returns groups with paths,
// sizes and the wasted space per group. Bounded by maxScan and time budget.
json duplicates(const wstring& root, int maxScan = 4000, int budgetMs = 30000);

} // namespace ok::diskscope
