// OptimizeKit v2.6 - Security Center: full local security scan.
//
// What it actually checks (real evidence, never invented):
//   1. Defender          - real-time protection, signature age (WMI SecurityCenter2)
//   2. Firewall          - per-profile enable state (INetFwPolicy2)
//   3. Listening ports   - every TCP/UDP endpoint + owning process; flags
//                          0.0.0.0/[::] listeners and port ranges commonly used by
//                          RATs / bind shells (4444, 5555, 1337, 31337...)
//   4. Established conns - every outbound connection: remote IP, port, process
//   5. Persistence       - Run/RunOnce keys (user + machine), Startup folders,
//                          scheduled tasks, services with a writable-path warning,
//                          IFEO debuggers (classic malware hijack)
//   6. Disk artifacts    - startup-folder .scr/.bat/.vbs/.ps1 droppers, recent
//                          executables in %TEMP% (ransomware/stager behaviour),
//                          files with double extensions (photo.jpg.exe),
//                          Rasautou / regsvr32 scriptlet launches in Run keys
//   7. Crypto-ransomware smoke - shadow-copy deletions are the classic pre-encryption
//                          step; the scan checks vssadmin delete-event evidence in the
//                          Security log when readable, and flags writable startup
//                          folders (an actual encryption-impact surface)
//   8. UAC               - is UAC enabled, is the current shell elevated
//
// Everything is READ-ONLY. The scan never deletes, never quarantines: it reports, and
// the only write actions it offers are "open Defender" and "revoke one Run-key entry"
// (the latter stashes the value in config.json first, so it restores).
#pragma once
#include "common.h"
#include "json.hpp"

namespace ok::secscan {

using json = nlohmann::json;

// Run the full scan. Takes a few seconds (the TCP table and the temp walk dominate).
json runScan();

// Revoke one startup entry found by the scan (path form: "HKCU\\...\\Run|ValueName"
// or a full file path for Startup-folder items). The value is stashed in config.json
// under "security_stash" so it can be put back.
bool revokeEntry(const wstring& id, wstring& err);
// Put a previously revoked entry back.
bool restoreEntry(const wstring& id, wstring& err);

} // namespace ok::secscan
