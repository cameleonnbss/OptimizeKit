<div align="center">

# ⚡ OptimizeKit

**Windows Optimization Suite — one exe, liquid-glass dashboard, real tweaks.**

Gaming FPS · latency · privacy · debloat · drivers · network · full activity log

`C++20 / Win32 / Direct2D` · `PowerShell engine` · `no install · no dependencies`

[⬇️ Download v1.0 (release)](../../releases) · [Quick start](#-quick-start) · [CLI](#-cli--numbered-menus) · [Français](README.fr.md)

</div>

---

## 📦 Download

| Release | Link |
|---|---|
| **v1.0.0 (current)** | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v1.0.0 |
| All releases | https://github.com/cameleonnbss/OptimizeKit/releases |

`OptimizeKit.exe` is fully **static** (MinGW-w64, ~4 MB): no runtime, no DLLs, no install. Drop it anywhere and run.

## ⚡ Quick start

| You want | Double-click |
|---|---|
| The dashboard (no admin needed) | `OptimizeKit-user.bat` |
| **Everything** (all tweaks, UAC prompt) | `OptimizeKit-admin.bat` |
| The numbered CLI menu (choice by digits) | `OptimizeKit-cli.bat` |
| The pure PowerShell engine (no exe) | `PowerShell\OptimizeKit.ps1` |

Then pick a profile and watch the **Logs** tab: every action is written to
`%LOCALAPPDATA%\OptimizeKit\OptimizeKit.log` and every registry key is **backed up as `.reg`**
before any change.

> 🛡️ **Safety**: `Restore Windows default` is available for every single tweak (GUI + CLI),
> the PowerShell engine has `-Restore`, and `.reg` backups live in `%LOCALAPPDATA%\OptimizeKit\`.

## 🧭 The dashboard (liquid glass)

Native Direct2D UI — dark glass panels, animated background, DPI-aware, one file exe:

| Tab | What you get |
|---|---|
| **Dashboard** | Live system snapshot: OS/CPU/GPU/RAM, power plan, Game Mode, HAGS, uptime + one-click profiles |
| **Tweaks** | All 30 tweaks with ADMIN/USER badges, impact rating, per-tweak *apply*, multi-select, restore |
| **Gaming** | Gaming profile, quick latency tweaks, live process list with priority boost / kill |
| **Privacy** | Telemetry, ads, activity history, Bing, Copilot, Edge background — one click each |
| **Drivers** | GPU + driver version, vendor download pages, `dxdiag`, Windows Update driver scan |
| **Network** | ICMP latency tester (avg of 4 pings) with saved targets, add/remove, color-coded ms |
| **Logs** | The full activity log, live — everything the kit does is written here |
| **About** | Credits, sources and where your backups live |

## 🛠️ The 30 tweaks

**Gaming / FPS / latency** — Game Mode · Game DVR & Game Bar off · Hardware-accelerated GPU
scheduling · MPO off (24H2 stutter fix) · global timer resolution 0.5 ms · gaming network stack
(`TcpAckFrequency=1`, `TCPNoDelay`, `NetworkThrottlingIndex=0xFFFFFFFF`, `SystemResponsiveness=0`,
MMCSS Games priority) · mouse acceleration off (raw 1:1) · performance visual effects · instant
menus · background apps off · Storage Sense · search indexing off · SysMain off · HPET off +
`useplatformclock false` · Ultimate Performance plan · `Win32PrioritySeparation=0x26` · GPU
preference high performance · fullscreen optimizations off · Xbox services off.

**Privacy** — telemetry off (AllowTelemetry=0, DiagTrack, dmwappush) · advertising ID ·
activity history / timeline · Bing out of search · tailored experiences · CEIP scheduled tasks ·
Copilot removed · Edge background & startup boost off.

**Debloat** — MS Store bloat apps removal (Clipchamp, News, Solitaire, Teams, Xbox gems…) ·
OneDrive uninstall · boot trim.

Full annotated mapping in [`docs/SOURCES.md`](docs/SOURCES.md) — curated from
**Chris Titus Tech's WinUtil (MIT)**, Microsoft/Valve documentation and the PC-gaming community.

## 💻 CLI — numbered menus

`OptimizeKit-cli.bat` (or `OptimizeKit.exe --cli`) opens a menu adapted to your privileges:

```
-- ADMIN MODE (full power) --
  1. System information
  2. Apply GAMING profile (one click)
  3. Apply PRIVACY profile
  4. Apply FULL kit profile
  5. Select multiple tweaks (numbers, a/n/d)
  6. Apply or restore a single tweak
  7. GAME BOOST : boost a running process priority
  8. NETWORK : saved targets latency test
  9. CLEAN : junk cleanup + recycle bin
 10. DRIVERS menu
 11. Registry backup
 12. Activity log
  0. Exit
```

One-shot commands (scriptable / CI-friendly):

```bat
OptimizeKit.exe --info                 " system summary
OptimizeKit.exe --list                 " all tweak ids
OptimizeKit.exe --profile gaming       " gaming | privacy | full
OptimizeKit.exe --apply  timer_high
OptimizeKit.exe --restore timer_high
OptimizeKit.exe --clean                " junk cleanup
OptimizeKit.exe --ping 1.1.1.1
```

## 🧪 PowerShell engine (no exe required)

```powershell
powershell -ExecutionPolicy Bypass -File PowerShell\OptimizeKit.ps1 -User   # user menu
powershell -ExecutionPolicy Bypass -File PowerShell\OptimizeKit.ps1         # self-elevates, admin menu
powershell -ExecutionPolicy Bypass -File PowerShell\OptimizeKit.ps1 -Silent # apply full kit, no menus
powershell -ExecutionPolicy Bypass -File PowerShell\OptimizeKit.ps1 -Restore
```

Same tweaks, same logging (`OptimizeKit-PowerShell.log`), same registry backups, numbered menus.

## 📁 Where things are stored

| What | Where |
|---|---|
| Config (ping targets, applied tweaks) | `%LOCALAPPDATA%\OptimizeKit\config.json` |
| Activity log | `%LOCALAPPDATA%\OptimizeKit\OptimizeKit.log` |
| PowerShell log | `%LOCALAPPDATA%\OptimizeKit\OptimizeKit-PowerShell.log` |
| Registry backups (auto, before any change) | `%LOCALAPPDATA%\OptimizeKit\backup_*.reg` |

Delete the folder for a fresh start, or run `Uninstall-OptimizeKit.bat`.

## 🔨 Build from source

Requirements: Windows 10/11, [MinGW-w64](https://winlibs.com) (`g++` + `windres`) on PATH.

```bat
build.bat
```

Output: `dist\OptimizeKit.exe`. CMake also works (`cmake -B build && cmake --build build`).
CI builds every push via GitHub Actions (`.github/workflows/build.yml`).

## ⚠️ Disclaimer

Registry tweaks change your system. The kit backs up every key it touches and can restore
defaults per-tweak, but **you** stay in charge: read a tweak's description before applying it,
and reboot after HAGS / timer / power-plan changes. Not affiliated with Microsoft. Use at your own risk.

## 📜 Credits & sources

- [ChrisTitusTech/winutil](https://github.com/ChrisTitusTech/winutil) (MIT) — debloat & privacy baseline
- Microsoft docs — [HAGS](https://learn.microsoft.com/windows/win32/direct3d12/hardware-accelerated-gpu-scheduling), Game Bar/GeDVR, MPO `OverlayTestMode`
- Community — [TimerResolution-Optimization](https://github.com/insovs/TimerResolution-Optimization), MarkC mouse fix, network latency guides
- [nlohmann/json](https://github.com/nlohmann/json) (MIT, vendored single header)
- UI style inspired by [WormGPT-desktop](https://github.com/cameleonnbss/WormGPT-desktop)

## 🇫🇷 Français

README annexe en français : **[README.fr.md](README.fr.md)**

---

MIT © 2026 [cameleonnbss](https://github.com/cameleonnbss)
