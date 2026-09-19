<div align="center">

<img src="assets/icon256.png" width="88" alt="OptimizeKit"/>

# ⚡ OptimizeKit

**Windows Gaming Control Center — one exe, liquid-glass dashboard, every tweak reversible.**

[![Release](https://img.shields.io/github/v/release/cameleonnbss/OptimizeKit?style=flat-square&color=ff3d57)](../../releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078d4?style=flat-square)](https://github.com/cameleonnbss/OptimizeKit)
[![Language](https://img.shields.io/badge/C%2B%2B-20-00599c?style=flat-square&logo=c%2B%2B&logoColor=white)](https://isocpp.org)
[![License](https://img.shields.io/badge/license-MIT-3dd68c?style=flat-square)](LICENSE)
[![Size](https://img.shields.io/badge/exe-~6%20MB%20static-ff9800?style=flat-square)](#-download)

**Live monitoring** · **Gaming score** · **30 reversible tweaks** · **Game library with real icons** · **Network center** · **Benchmark** · **Full activity log**

`C++20 / Win32` · `embedded HTTP server` · `zero dependencies` · `no install`

[⬇️ **Download v2.1.0**](../../releases) · [Quick start](#-quick-start) · [Screenshots](#-screenshots) · [Safety](#%EF%B8%8F-safety-first) · [Français](README.fr.md)

</div>

---

## 📸 Screenshots

| Gaming Center | Dashboard |
|---|---|
| ![Gaming Center](docs/screenshots/gaming.png) | ![Dashboard](docs/screenshots/dashboard.png) |

| Tweaks — instant ON/OFF switches | Games — real icons |
|---|---|
| ![Tweaks](docs/screenshots/tweaks.png) | ![Games](docs/screenshots/games.png) |

| Network Center | PC Scanner |
|---|---|
| ![Network](docs/screenshots/network.png) | ![Scan](docs/screenshots/scan.png) |

| Storage | Splash screen |
|---|---|
| ![Storage](docs/screenshots/storage.png) | ![Splash](docs/screenshots/dashboard.png) |

## 📦 Download

| Release | Link |
|---|---|
| **v2.1.0 (current)** | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.1.0 |
| All releases | https://github.com/cameleonnbss/OptimizeKit/releases |

`OptimizeKit.exe` is fully **static** (MinGW-w64, ~6 MB): no runtime, no DLLs, no install. It embeds an HTTP server and the liquid-glass web dashboard (`web/` folder ships next to it).

## ⚡ Quick start

| You want | Double-click |
|---|---|
| **The menu** (everything, exe optional) | `OptimizeKit.bat` |
| **The dashboard** (web UI in a standalone window) | `OptimizeKit.bat` option 5, or `dist\OptimizeKit.exe` |
| Direct PowerShell engine (no exe) | `powershell -File PowerShell\OptimizeKit.ps1` or `OptimizeKit.bat -Status` |
| Headless web dashboard | `OptimizeKit.exe --web 8765` |

The dashboard opens on a **splash boot sequence** (detecting hardware → reading Windows state → loading tweak catalog → measuring network), then lands on the **Gaming Center** with your machine's gaming score.

> 🛡️ **Safety**: every tweak has a one-click **restore to Windows default** (web UI + CLI), the PowerShell engine has `-Restore`, and `.reg` backups live in `%LOCALAPPDATA%\OptimizeKit\`.

## 🎮 Gaming Center

The new home page scores your machine 0–100 from the real state of gaming-relevant settings, then groups every gaming switch by category:

- **FPS & rendering** — HAGS, Game Mode, fullscreen optimizations, MPO, GPU preference
- **Latency & input** — 0.5 ms timer, gaming network stack (TcpAckFrequency/NoDelay/MMCSS), mouse acceleration, menu delay, Win32PrioritySeparation
- **Background load** — Game DVR, background apps, SysMain, search indexing, Xbox Live services
- **Power & thermals** — Ultimate Performance plan, HPET

Every switch is a real toggle: click → registry snapshot → apply → animated confirmation. Click again to restore the Windows default. Category chips show `OPTIMIZED / 3/5 / STOCK` at a glance.

## 🔧 The dashboard (WormGPT-style liquid glass)

Deep black + your accent color (6 themes in the top bar, persisted), glassmorphism with moving specular sheen, mouse-following particles, Inter & Space Mono. Served by an embedded C++ HTTP server on `127.0.0.1:8765`, opened as a chromeless app window.

| View | What you get |
|---|---|
| **Dashboard** | Live CPU / RAM / GPU / disk with 60-second sparklines, network throughput, process/thread counts, top processes (2.5 s refresh) |
| **Gaming Center** | Gaming score ring, category score cards, grouped ON/OFF switches, Gaming Mode enter/exit |
| **Tweaks** | All 49 tweaks as switch cards: instant apply / instant restore, search, category filters, ADMIN/USER badges, impact bars, live count in the sidebar |
| **Games** | Steam + Epic + Riot + GOG + registry detection, real icons extracted from the executables, per-game boost (IFEO persistent priority) and Gaming Mode |
| **Scan PC** | Full system scan: junk files, tweak state, network config, power plan, startup load, drivers, games — every finding has an Apply button |
| **Optimize** | One-click profiles (SAFE / GAMING / PRIVACY / RESTORE ALL) with snapshot → apply → verify |
| **Network** | Adapter status (autotuning, RSC, RSS, DNS, MTU), profiles, **MTU slider** with presets (Ethernet/PPPoE/VPN/Jumbo), latency monitor, quick ping |
| **RAM** | Usage graph, committed/cached stats, top consumers, honest standby-list trim (with an explanation of what it really does) |
| **Storage** | Drives with NVMe/SATA bus detection, usage bars, largest files |
| **Startup** | HKCU/HKLM Run keys + startup folders, one-click disable (reversible via stash) |
| **Benchmark** | Real measured numbers: CPU MOPS, RAM GB/s, disk MB/s, network latency — with history |
| **Logs** | Color-coded activity log with live filter |
| **Settings** | Accent color, particles, kill list for Gaming Mode, DNS management — persisted in `config.json` |

## 🕹️ Game detection

Multi-provider scanner — no single-store lock-in:

| Provider | Source |
|---|---|
| **Steam** | `libraryfolders.vdf` → `appmanifest_*.acf` (all libraries, all drives) |
| **Epic** | `%PROGRAMDATA%\Epic\EpicGamesLauncher\Data\Manifests\*.item` |
| **Riot** | `HKLM\SOFTWARE\Riot Games, Inc.\*` (VALORANT, LoL…) |
| **GOG** | `HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\*` |
| **Registry** | Uninstall keys under Epic/Riot/GOG/Battle.net/Ubisoft/EA/Xbox/Rockstar dirs |

Plus per-game profiles (persistent high priority via IFEO), gaming mode per game, and real icon extraction (`SHDefExtractIconW` → PNG cache).

## 🖥️ CLI

`OptimizeKit-cli.bat` gives numbered menus (user or admin). Direct flags:

```
OptimizeKit.exe --native            native Direct2D dashboard
OptimizeKit.exe --web [port]        web dashboard without opening a browser
OptimizeKit.exe --profile gaming|privacy|full|clean
OptimizeKit.exe --apply <tweak-id>
OptimizeKit.exe --restore <tweak-id>
OptimizeKit.exe --list              list tweak ids
OptimizeKit.exe --clean             junk cleanup
OptimizeKit.exe --info              system summary
```

## 🛡️ Safety first

- **Every tweak is reversible** — switch OFF restores the exact Windows default value.
- **Registry backups** before any change: `%LOCALAPPDATA%\OptimizeKit\backup_*.reg`.
- **Honest descriptions** — no magic FPS promises; the RAM trim page even explains why "empty standby" is not "extra RAM".
- **Gaming Mode** snapshots your power plan and restores everything on exit.
- **Nothing runs at boot**, no service is installed; the exe only acts when you ask.
- Source curated from [Chris Titus Tech's WinUtil](https://github.com/ChrisTitusTech/winutil) (MIT), Microsoft documentation and the PC-gaming community — every tweak shows its origin in the CLI.

## 🏗️ Build from source

```
git clone https://github.com/cameleonnbss/OptimizeKit
cd OptimizeKit
build.bat          rem MinGW-w64 g++ 13+ (winlibs / MSYS2)
```

Output: `dist\OptimizeKit.exe` + `dist\web\`. Or use the provided CMakeLists with any MinGW toolchain.

## 📄 License

MIT — see [LICENSE](LICENSE). Tweaks curated from WinUtil (MIT), Microsoft docs and community knowledge.

<div align="center">
<b>If OptimizeKit saved you time, a ⭐ on the repo helps a lot.</b>
</div>
