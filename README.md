<div align="center">

<img src="assets/icon256.png" width="88" alt="OptimizeKit"/>

# ⚡ OptimizeKit

**Windows Gaming Control Center — one exe, the full dashboard in a real desktop window (no browser), every tweak reversible.**

[![Release](https://img.shields.io/github/v/release/cameleonnbss/OptimizeKit?style=flat-square&color=ff3d57)](../../releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078d4?style=flat-square)](https://github.com/cameleonnbss/OptimizeKit)
[![Language](https://img.shields.io/badge/C%2B%2B-20-00599c?style=flat-square&logo=c%2B%2B&logoColor=white)](https://isocpp.org)
[![License](https://img.shields.io/badge/license-MIT-3dd68c?style=flat-square)](LICENSE)
[![Size](https://img.shields.io/badge/exe-~6.6%20MB%20static-ff9800?style=flat-square)](#-download)

**App-window dashboard (web UI, no browser)** · **Gaming score** · **77 reversible tweaks** · **Standalone CLI (no exe needed)** · **Firmware panel (SecureBoot/TPM/VT)** · **Driver auto-update engine** · **5 tuning centers** · **300-title game database + 106 covers** · **Network center** · **AnTuTu-style benchmark** · **79-tool catalog** · **Ctrl+K palette** · **12 themes** · **EN/FR**

`C++20 / Win32 / Direct2D` · `zero dependencies` · `no install` · `no driver, no injection, no Edge`

[⬇️ **Download v2.8.0**](../../releases/tag/v2.8.0) · [What's new](#-whats-new-in-280) · [Quick start](#-quick-start) · [Screenshots](#-screenshots) · [Safety](#%EF%B8%8F-safety-first) · [Français](README.fr.md)

</div>

---

## 🆕 What's new in 2.8.0

| | |
|---|---|
| **The web dashboard IS the app window** | Double-click → the embedded dashboard opens in a real desktop frame — **pixel-identical to the browser dashboard** (themes, Ctrl+K palette, game library), with no Edge window, no tabs, no address bar. If the WebView2 runtime is missing, the app silently falls back to the native Direct2D window instead of opening a browser. `--native` forces D2D. |
| **Better app frame** | The window title follows the dashboard tab (Firmware, Tweaks…), F5 / Ctrl+R reload works, right-click copy enabled. |
| **Standalone CLI launchers** | `OptimizeKit.bat` is a full module menu and `OptimizeKit-cli.bat` runs everywhere — **no exe required**: everything goes through the PowerShell engine (71 tweaks, profiles, network, cleanup, firmware, drivers, restore-all), in admin and non-admin menus. |
| **24 new PowerShell tweaks — 71 total** | The engine matches the C++ catalog: Widgets, Location, Services-to-Manual + svchost tuning, Delivery Optimization, Consumer features, Store search, End-task on taskbar, **WPBT block**, Razer block, Notifications, IPv4/IPv6/Teredo, disk cleanup + WinSxS trim, hibernation off, verbose BSoD, long paths, Game Mode (Win11), Edge & Brave debloat, UTC clock, restore point — each with apply, restore and live status. |
| **Firmware report in the engine** | `-Firmware` / menu 7: BIOS, Secure Boot, TPM, VT-x, hypervisor, pending reboot — strictly read-only. |
| **Fresh screenshots** | Every dashboard view re-captured from the live app with real machine data. |

### Still from 2.7.0

| | |
|---|---|
| **Firmware & platform panel (native + `--firmware`)** | BIOS vendor/version, motherboard, boot mode, **Secure Boot**, **TPM**, VT-x/SVM + hypervisor, modern standby vs S3, HPET, WPBT, dynamic tick / TSC, kernel dump level, pending reboot |
| **Driver auto-update engine (native + `--drvupdate`)** | Real driver ages from the driver store, problem devices, **Windows Update driver scan trigger**, PnP rescan, vendor pages |
| **28 C++ tweaks — 77 total** | Aligned on WinUtil's current catalog, all reversible |

### Still from 2.6.0

| | |
|---|---|
| **Process Reducer** | Live CPU/RAM table — eco mode (below-normal + EcoQoS), end, sweep, full restore |
| **Security scan** | Antivirus, firewall per profile, every listening port + owner, autostart persistence, ransomware patterns — one-click fixes |
| **DiskScope tools** | Duplicate finder, space hogs, folder sizes |
| **Real cover art** | Steam/Epic box art for the top-played titles |

### Still from 2.5.0 & 2.4.0

Game database (~300 titles, every store + every drive detection), batch icons, Game Library with genre-aware sets, 5 tuning centers, Ctrl+K palette, 12 full themes, ⚡ BOOST pill.

Full detail in [CHANGELOG.md](CHANGELOG.md).

## 📸 Screenshots

All shots captured from the live app (v2.8, real machine data, no mockups).

| Dashboard | Tweaks |
|---|---|
| ![Dashboard](docs/screenshots/dashboard.png) | ![Tweaks](docs/screenshots/tweaks.png) |

| Gaming Center | Games — real icons |
|---|---|
| ![Gaming Center](docs/screenshots/gaming.png) | ![Games](docs/screenshots/games.png) |

| Network Center | Storage |
|---|---|
| ![Network](docs/screenshots/network.png) | ![Storage](docs/screenshots/storage.png) |

| Tools — 79-launcher catalog | Themes — 12 packs |
|---|---|
| ![Tools](docs/screenshots/tools.png) | ![Themes](docs/screenshots/themes.png) |

## 📦 Download

| Release | Link |
|---|---|
| **v2.8.0 (current)** | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.8.0 |
| v2.7.0 | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.7.0 |
| v2.6.0 | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.6.0 |
| All releases | https://github.com/cameleonnbss/OptimizeKit/releases |
| Changelog | [CHANGELOG.md](CHANGELOG.md) |

`OptimizeKit.exe` is fully **static** (MinGW-w64, ~6.6 MB): no runtime, no DLLs, no install, no browser component. An optional `web/` folder next to the exe unlocks the full web dashboard when you launch with `--app`.

## ⚡ Quick start

| You want | Do this |
|---|---|
| **The app** (default) | Double-click `dist\OptimizeKit.exe` — the dashboard opens in its own desktop window, no browser |
| **The full CLI, no exe** | Run `OptimizeKit.bat` (module menu) or `OptimizeKit-cli.bat` (numbered menus, admin + user) |
| Direct PowerShell engine | `powershell -File PowerShell\OptimizeKit.ps1` or `OptimizeKit.bat -Status` |
| Native Direct2D window | `OptimizeKit.exe --native` |
| Headless web dashboard | `OptimizeKit.exe --web 8765` (loopback only) |

The default window **is the app**: the exact same liquid-glass interface as the browser dashboard — themes, Ctrl+K palette, game library, firmware panel, driver engine, benchmark, 77 tweaks — rendered inside a dedicated desktop frame with a dark title bar and dynamic window title.

> 🛡️ **Safety**: every tweak has a one-click **restore to Windows default** (web UI + native UI + CLI + PowerShell `-RestoreAll`), and `.reg` backups live in `%LOCALAPPDATA%\OptimizeKit\`.

## 🎮 What's inside (app window = web dashboard)

- **Dashboard** — live machine state, gaming-relevant flags, quick profiles, one-click benchmark
- **Tweaks** — all **77 tweaks** with instant apply/restore, filters, admin badges, impact stars
- **Gaming Center** — one-click profiles + 12 quick switches + process priority/kill table
- **Firmware (BIOS guide)** — BIOS identity, Secure Boot, TPM, VT, standby mode, HPET/WPBT/tick state, pending reboot (read-only; the app never writes firmware)
- **Storage** — drives with NVMe/SATA bus detection, safe cleanup targets, duplicate finder, largest files
- **Network** — TCP autotuning / RSS / MTU, profiles, latency monitor with custom targets
- **Security Scan** — antivirus, firewall, listening ports + owners, autostart persistence, one-click fixes
- **Process Reducer** — live CPU/RAM table, eco mode (EcoQoS), end, sweep, full restore
- **Games / Library** — every-store detection with real cover art, batch icons, boost
- **Tools** — 79 Windows tool launchers; **Themes** — 12 full packs + custom accent; **Ctrl+K** palette

Without the exe, `OptimizeKit.bat` / `OptimizeKit-cli.bat` expose the same feature set through the PowerShell engine: 71 tweaks with status, gaming/privacy/debloat/full profiles, network center, junk cleanup, firmware report, driver info, restore-all — in admin and non-admin menus.

## 🖥️ CLI

`OptimizeKit-cli.bat` gives numbered menus (user or admin). Direct flags:

```
OptimizeKit.exe                    web dashboard in a desktop window (default)
OptimizeKit.exe --native           native Direct2D dashboard window
OptimizeKit.exe --web [port]       serve the web dashboard, no window
OptimizeKit.exe --profile gaming|privacy|full|clean
OptimizeKit.exe --apply <tweak-id>
OptimizeKit.exe --restore <tweak-id>
OptimizeKit.exe --list             list all tweak ids
OptimizeKit.exe --clean            junk cleanup
OptimizeKit.exe --info             system summary
OptimizeKit.exe --firmware         BIOS / SecureBoot / TPM / kernel options report
OptimizeKit.exe --drvupdate        driver age report + Windows Update scan
OptimizeKit.exe --ping <host>      latency test

OptimizeKit.bat                    full module menu, PowerShell engine (no exe needed)
OptimizeKit.bat -Status            tweak states, read-only
OptimizeKit.bat -Tweaks            pick-a-number apply / r<N> restore
OptimizeKit.bat -Network           latency + DNS benchmark
OptimizeKit.bat -Cleanup           junk cleanup
OptimizeKit.bat -Firmware          BIOS / SecureBoot / TPM (read-only)
OptimizeKit.bat -Drivers           GPU/driver info + vendor pages
OptimizeKit.bat -Profile gaming|privacy|debloat|full
OptimizeKit.bat -Silent            gaming profile, no prompts
OptimizeKit.bat -RestoreAll        back to Windows defaults
OptimizeKit-cli.bat /exe           same menus through the C++ CLI (if built)
```

## 🎛️ Firmware, kernel & drivers — "closer to the metal"

OptimizeKit is plain Win32/C++ and behaves like a system tool:

- **Firmware inventory** reads the SMBIOS identity (WMI), the UEFI Secure Boot state machine, the TPM (Win32_Tpm namespace with a PnP fallback for non-elevated sessions), virtualization state including the running hypervisor, modern-standby vs S3 policy, and the boot options `bcdedit` manages (dynamic tick, platform tick, platform clock, TSC sync). It is strictly **read-only** — firmware changes stay a human job, guided by the BIOS cards.
- **Kernel-visible tweaks**: WPBT execution block (stops vendor firmware from launching code at boot), HPET policy, dynamic tick, MSI mode for GPU/NIC, interrupt affinity, `Win32PrioritySeparation`, MMCSS gaming task, svchost split threshold — all applied through documented registry/service paths, all reversible.
- **Driver updates**: the app reports the real age of every installed driver (from `Control\Class` records — same source as Device Manager), lists devices with problem codes, then triggers **Windows Update's own driver scan** (`UsoClient`, the supported orchestrator) or opens the vendor page. Nothing is downloaded or installed behind your back.

## 🛡️ Safety first

- **Every tweak is reversible** — switch OFF restores the exact Windows default value.
- **Registry backups** before any change: `%LOCALAPPDATA%\OptimizeKit\backup_*.reg`.
- **Honest descriptions** — no magic FPS promises; the RAM trim page even explains why "empty standby" is not "extra RAM"; the firmware panel says "unknown" instead of inventing values.
- **Gaming Mode** snapshots your power plan and restores everything on exit.
- **Nothing runs at boot**, no service is installed; the exe only acts when you ask.
- Source curated from [Chris Titus Tech's WinUtil](https://github.com/ChrisTitusTech/winutil) (MIT), Microsoft documentation and the PC-gaming community — every tweak shows its origin.

## 🏗️ Build from source

```
git clone https://github.com/cameleonnbss/OptimizeKit
cd OptimizeKit
build.bat          rem MinGW-w64 g++ 13+ (winlibs / MSYS2)
```

Output: `dist\OptimizeKit.exe` + `dist\web\`. Or use the provided CMakeLists with any MinGW toolchain. Regenerate the icon set with `python tools/make_icon.py` (Pillow).

## 📄 License

MIT — see [LICENSE](LICENSE). Tweaks curated from WinUtil (MIT), Microsoft docs and community knowledge.

<div align="center">
<b>If OptimizeKit saved you time, a ⭐ on the repo helps a lot.</b>
</div>
