<div align="center">

# ⚡ OptimizeKit

**Windows Optimization Suite — one exe, WormGPT-style liquid-glass dashboard, live monitoring.**

Live monitoring (CPU / RAM / GPU / disk / network) · gaming FPS · latency · privacy · debloat · drivers · full activity log

`C++20 / Win32 / embedded HTTP server` · `WormGPT-style UI (DarkGPT theme)` · `PowerShell engine` · `no install · no dependencies`

[⬇️ Download v1.1.0 (release)](../../releases) · [Quick start](#-quick-start) · [CLI](#-cli--numbered-menus) · [Français](README.fr.md)

</div>

---

## 📦 Download

| Release | Link |
|---|---|
| **v1.1.0 (current)** | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v1.1.0 |
| All releases | https://github.com/cameleonnbss/OptimizeKit/releases |

`OptimizeKit.exe` is fully **static** (MinGW-w64, ~5 MB): no runtime, no DLLs, no install. It embeds an HTTP server and the DarkGPT-style web dashboard (`web/` folder ships next to it).

## ⚡ Quick start

| You want | Double-click |
|---|---|
| **The dashboard** (web UI in a standalone window) | `OptimizeKit-user.bat` |
| **Everything** (all tweaks, UAC prompt) | `OptimizeKit-admin.bat` |
| The numbered CLI menu (choice by digits) | `OptimizeKit-cli.bat` |
| The pure PowerShell engine (no exe) | `PowerShell\OptimizeKit.ps1` |
| The old native Direct2D window | `OptimizeKit.exe --native` |

Then pick a profile and watch the **Logs** view: every action is written to
`%LOCALAPPDATA%\OptimizeKit\OptimizeKit.log` and every registry key is **backed up as `.reg`**
before any change.

> 🛡️ **Safety**: `Restore Windows default` is available for every single tweak (web UI + CLI),
> the PowerShell engine has `-Restore`, and `.reg` backups live in `%LOCALAPPDATA%\OptimizeKit\`.

## 🖥️ The dashboard (WormGPT-style liquid glass)

The UI reuses the **DarkGPT design system** from WormGPT-desktop (cameleonnbss): deep black +
exclusive `#ff3d57` red accent, glassmorphism with moving specular sheen, animated red particles
following the mouse, background grid + noise + vignette, Inter & Space Mono fonts. Served by an
embedded C++ HTTP server on `127.0.0.1:8765` and opened as a chromeless app window.

| View | What you get |
|---|---|
| **Dashboard** | **Live monitoring**: CPU / RAM / GPU usage with 60-second sparkline graphs, disk & network throughput, process/thread counts, top processes by CPU (2.5 s refresh), full system snapshot, one-click profiles |
| **Tweaks** | All 30 tweaks with ADMIN/USER badges, impact rating, filters, multi-select apply/restore |
| **Gaming** | Gaming profile + 8 quick latency tweaks (DVR, network, timer, HAGS, MPO, power, mouse, FSO) |
| **Privacy** | Privacy profile + 8 quick tweaks (telemetry, ads, activity, Bing, Copilot, Edge…) |
| **Drivers** | GPU + driver version (auto-detected), vendor pages, dxdiag, Device Manager |
| **Network** | ICMP latency tester with saved targets + quick ping any host |
| **Logs** | Full activity log, color-coded, live |
| **About** | Credits and backup locations |

## 📊 Live monitoring

- **CPU %** (per-core aggregated, PDH `Processor Information`) + % of base clock
- **RAM %** + used/total bytes (`GlobalMemoryStatusEx`)
- **GPU %** (PDH `GPU Engine` utilization, all engines merged) — works on NVIDIA / AMD / Intel
- **Disk** % busy + read/write MB/s, **Network** down/up KB-s
- **Top processes** by CPU with RAM and PID (250 ms delta sampling)
- **60-second rolling history** rendered as glowing sparklines, 1 s polling

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
OptimizeKit.exe --web 8765             " serve the dashboard headless
```

### HTTP API (localhost only)

The embedded server also exposes a JSON API you can script against:
`GET /api/state` · `GET /api/monitor` · `GET /api/processes` · `GET/POST /api/tweaks` ·
`POST /api/tweaks/apply` · `POST /api/tweaks/restore` · `POST /api/profile` ·
`GET/POST /api/ping` · `POST /api/clean` · `GET /api/logs`.

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
