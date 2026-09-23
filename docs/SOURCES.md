# Sources — where each tweak comes from

OptimizeKit does not invent magic tweaks: every registry change is either documented by
Microsoft, or harvested from a reputable open-source project (mainly Chris Titus Tech's
**WinUtil**, MIT-licensed) or a well-known PC-gaming guide. Each entry lists the tweak id,
what it changes, and the source it was curated from.

## Gaming / FPS / latency

| Tweak id | Registry / command | Source |
|---|---|---|
| `game_mode` | `HKCU\...\GameBar\AutoGameModeEnabled=1` | Microsoft Game Mode docs, WinUtil |
| `game_dvr_off` | `GameDVR_Enabled=0`, `AppCaptureEnabled=0` + GameBar keys | WinUtil, shadercache/GeDVR guides |
| `hags_on` | `HKLM\...\GraphicsDrivers\HwSchMode=2` | Microsoft HAGS documentation |
| `mpo_off` | `HKLM\...\Dwm\OverlayTestMode=5` | Microsoft KB, "PC Gaming: Optimized" guide (24H2 stutter/flicker fix) |
| `timer_high` | `GlobalTimerResolutionRequests=1`, `bcdedit /set disabledynamictick yes` | [insovs/TimerResolution-Optimization](https://github.com/insovs/TimerResolution-Optimization) |
| `network_gaming` | `TcpAckFrequency=1`, `TCPNoDelay=1` per interface, `NetworkThrottlingIndex=0xFFFFFFFF`, `SystemResponsiveness=0`, MMCSS Games priority | Microsoft MMCSS docs + community latency guides (Nagle's algorithm) |
| `mouse_precision` | `MouseSpeed=0`, `MouseThreshold1/2=0` | MarkC mouse fix (raw 1:1 input) |
| `visual_fx_perf` | `VisualFXSetting=2`, `MinAnimate=0` | WinUtil |
| `menu_delay_0` | `HKCU\Control Panel\Desktop\MenuShowDelay=0` | WinUtil |
| `background_apps` | `GlobalUserDisabled=1` | WinUtil |
| `storage_sense` | StoragePolicy `01=1`, `2048=7` (weekly) | Microsoft Storage Sense docs |
| `search_index` | `WSearch` service → disabled | WinUtil |
| `sysmain_off` | `SysMain` (Superfetch) service → disabled | WinUtil, community (SSD-era advice) |
| `hpets_off` | `HKLM\...\Services\hpet\Start=4`, `useplatformclock false` | calypto / community latency guides |
| `power_ultimate` | `powercfg -duplicatescheme e9a42b02-...` | WinUtil (Ultimate Performance) |
| `win32_priority` | `Win32PrioritySeparation=0x26` | Windows internals / community |
| `gpu_preference` | `DirectXUserGlobalSettings=SwapEffectUpgradeEnable=1;` | Microsoft GPU preference docs |
| `fso_on` | `~ DISABLEDXMAXIMIZEDWINDOWEDMODE` compat layer | PC Gaming: Optimized guide |
| `xbox_live_off` | XblAuthManager / XblGameSave / XboxNetApiSvc / XboxGipSvc disabled | WinUtil (warning: breaks Game Pass) |

## Privacy / telemetry

| Tweak id | Registry / command | Source |
|---|---|---|
| `telemetry_off` | `AllowTelemetry=0` + DiagTrack/dmwappushservice disabled | WinUtil, Microsoft telemetry docs |
| `advertising_off` | `AdvertisingInfo\Enabled=0` | WinUtil |
| `activity_history` | `PublishUserActivities=0`, `UploadUserActivities=0` | WinUtil |
| `bing_search` | `BingSearchEnabled=0`, `CortanaConsent=0` | WinUtil |
| `tailored_experiences` | `TailoredExperiences...=0` | WinUtil |
| `telemetry_tasks` | CEIP / Compatibility Appraiser scheduled tasks disabled | WinUtil |
| `windows_copilot` | `TurnOffWindowsCopilot=1`, `ShowCopilotButton=0` | WinUtil |
| `edge_bing_blocking` | Edge `BackgroundModeEnabled=0`, `StartupBoostEnabled=0` | WinUtil |

## Debloat

| Tweak id | Action | Source |
|---|---|---|
| `bloat_uninstall` | Remove-AppxPackage for ~35 provisioned store apps | WinUtil debloat list |
| `onedrive_off` | Official OneDriveSetup `/uninstall` + link keys | WinUtil |
| `hpets_boot` | `bcdedit /set nobootuxprogress` | community boot-trim guides |

## Credits

- **Chris Titus Tech's WinUtil** — https://github.com/ChrisTitusTech/winutil (MIT) — the single
  biggest curated source for debloat & privacy tweaks. OptimizeKit's catalog mirrors its intent
  while adding gaming/latency entries and full per-tweak restore.
- **Microsoft documentation** — HAGS, MMCSS, Game Bar, MPO, Storage Sense, telemetry policies.
- **Community projects** — TimerResolution-Optimization, MarkC mouse fix, network latency guides,
  "PC Gaming: Optimized" (MPO/FSO advice).
- **nlohmann/json** — https://github.com/nlohmann/json (MIT, vendored single header).

Every tweak in OptimizeKit has a matching `restore()` implementation that puts back the
documented Windows default. Registry exports (`backup_*.reg`) are taken automatically before
the first change of each session.

## Assets inherited from Khadafi

Two things in OptimizeKit come from the Khadafi optimizer (reverse-engineered from its
shipped files), and both are cosmetic — no code, no behaviour:

| Asset | Where it lives | What it is |
|---|---|---|
| **Game covers** (400+ thumbnails) | `web/assets/gamelogos/` | Box art for the Game Library — one cover for every title of the built-in database — downscaled to 400 px by [`tools/import_gamelogos.py`](../tools/import_gamelogos.py) (Khadafi extract) and [`tools/fetch_missing_covers.py`](../tools/fetch_missing_covers.py) (official Steam CDN search + Wikipedia lead-image fallback). Third-party artwork: it is **not** embedded in the exe and ships only as a side folder. |
| **Palette + control surface** | `web/style.css` (`body[data-theme="khadafi"]` and the v2.3/v2.4 button rules) | Colour values read from Khadafi's own resource strings (`#0a0a0c`, `#ff3b4e`, `#ffb04d`, `#5865f2`, `#ff37c7`), squared letterspaced CTAs. |

The modules that mirror Khadafi's feature set (Game Library, Input Lag center, Rendering,
Background, Power, Debloat, Packs, BIOS guide) are OptimizeKit's own C++/JS implementations on
top of OptimizeKit's tweak catalog. The two things Khadafi does that OptimizeKit deliberately
refuses to copy are the in-game overlay injection and the WireGuard kernel tunnel: they would
break the "no driver, no injection, everything reversible" promise.
