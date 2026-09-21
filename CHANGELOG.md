# Changelog

All notable changes to OptimizeKit are documented here.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.6.0] — 2026-09-21

The "know your machine" release: a process reducer, a full local security scan,
a DiskScope-powered storage view, real Steam/Epic cover art in the Game Library
and a polished, animated dashboard.

### Added

- **Process Reducer** — live CPU/RAM table of everything running, with per-process
  eco mode (below-normal priority + power throttling), one-click end, an "eco all
  background" sweep and a full state restore. Endpoints: `/api/reducer`,
  `/api/reducer/act`.
- **Security scan** — evidence-based local audit: active antivirus + signature age
  (SecurityCenter2), firewall per profile (INetFwPolicy2), every listening port
  with its owning process, established outbound connections with remote IPs,
  autostart persistence entries and ransomware-pattern file checks. Each finding
  ships a one-click fix or open-action. Endpoints: `/api/security/scan`,
  `/api/security/revoke`.
- **DiskScope storage tools** — duplicate finder, space-hog cleanup targets and
  folder size analysis, built with assets and concepts from the
  [DiskScope-CLI](https://github.com/cameleonnbss/DiskScope-CLI) project.
  Endpoints: `/api/disk/folders`, `/api/disk/cleanup`, `/api/disk/duplicates`.
- **Real cover art** — the Game Library now pulls box art for the top-played Steam
  and Epic titles straight from the store CDNs, so most tiles show the actual
  jacket instead of a placeholder.
- **Dashboard polish** — accent-color flyout in the top-left is fully functional,
  view transitions and hover states animated throughout.

### Fixed

- Firewall COM query used a wrong interface IID and always fell back to "unknown".
- Process reducer reported killed counts but the UI read a different field.

## [2.5.0] — 2026-09-21

The "every game, every drive" release: a built-in database of known PC titles, a
detection pass that no longer depends on a store being installed, batch icon
extraction and a lot more buttons to act on the result.

### Added

- **Built-in game database** (`src/core/gamedb.h`) — ~300 known PC titles with their
  executable stems, alternative exe/folder aliases and a genre family. It names a
  detected game, tells the Game Library what set fits it, and recognises titles
  installed outside any store. `GET /api/games/catalog` serves it to the dashboard.
- **Detection beyond the stores** — six new scans: Blizzard/Battle.net,
  Ubisoft Connect, the EA app, itch.io, per-user Uninstall keys, an Xbox/Game Pass
  path, and a survey of the usual game folders on **every fixed drive**
  (`\Games`, `\Program Files (x86)`, `\XboxGames`, `\SteamLibrary\steamapps\common`…).
  Steam root discovery now reads the registry, so a Steam installed on `D:` is found.
  Recognised games get their real name and genre instead of an exe basename.
- **Batch icons** — `POST /api/games/icons` extracts every missing icon in one pass
  (45 s budget, cached as PNG); `GET /api/games?icons=1` does it inline. The Games
  view fills the whole grid with real icons instead of the first twelve.
- **Game actions** — `POST /api/games/launch` (launch, or reveal in Explorer),
  `POST /api/games/boost-all` (persistent high priority for every detected game,
  reversible). New buttons: launch, boost this game, gaming mode, extract icon,
  open folder, copy path, library preset, boost all, extract all icons, random,
  store shortcuts, plus launcher chips and a stats row.
- **Library doubles** — the cover art (106 titles) now merges with the database, so
  titles without art get a generated tile. New filters (genre, installed, cover art),
  sorts (A→Z, Z→A, genre, installed first, cover art), installed badges with the
  real extracted icon, launch/open-folder for installed titles, and a Surprise me
  button.

### Changed

- Splash and boot copy mention the new detection surface; the library paragraph is
  generated from the real counts instead of a hard-coded "106".

## [2.4.0] — 2026-09-21

The "more modules, better looks" release: 10 new views, a real theme engine, a
command palette and a working CI.

### Added

- **Game Library** — 106 titles with cover art grouped by genre, per-title
  suggested profile, progress bar of what is already active, apply/restore in one
  click. Covers are imported with `tools/import_gamelogos.py` (thumbnails only,
  never embedded in the exe).
- **Five tuning centers** — Input Lag (14 settings + on-demand DPC/ISR and jitter
  measurement), Rendering & FPS (10), Background load (15), Power & thermals (7),
  Debloat & boot (10). Each one shows catalog state plus measured cards marked
  `· LIVE`, and can apply the user-safe items or restore everything.
- **Smart Optimize** (`GET /api/smart`) — ranks the whole catalog against a goal
  (Gaming / Latency / Privacy / Balanced) from the machine's real state, with a
  weight and a "why" per item.
- **Packs** — six preconfigured bundles (Esport, Low latency, Play & stream,
  Laptop/thermals, Fast clean boot, Privacy lock-down) with live progress,
  Apply/Restore and a "What's inside" list resolving the real tweak names.
- **BIOS & firmware guide** — 12 cards marked SAFE / ADVANCED, read-only: the app
  never writes to firmware and says so.
- **Command palette** — `Ctrl+K` searches every module, tweak, tool, pack and game;
  tweaks toggle straight from the palette with their current state shown.
- **Appearance view** — 12 full themes previewed as cards (Magma, Khadafi, Carbon,
  Discord, Fusion, Acid, Ocean, Matrix, Violet, Gold, Steel, Rose) plus a per-theme
  accent memory and a custom accent picker.
- **⚡ BOOST** pill in the top bar: the competitive set in one click, reversible.
- Bento dashboard: KPI tiles (tweaks active, junk on disk, processes, uptime) and a
  two-column bottom row (top processes + machine state).
- `docs/SOURCES.md` credits and `tools/import_gamelogos.py` asset importer.

### Fixed

- **Theme engine.** A theme pack is now applied as a whole, and the accent is
  remembered per theme. Previously the 10 s state poll re-applied `ui_theme` and
  silently reverted an accent picked in the flyout, which could leave an
  inconsistent pair (`ui_theme=steel` with `ui_accent=#38bdf8` — silver surfaces,
  blue accent).
- **MinGW link error** in the WebView2 layer: `rpcndr.h` defines `MIDL_INTERFACE`
  as a plain `struct`, so `__uuidof()` had no definition for three interfaces.
  The IIDs are now declared with `__CRT_UUID_DECL` and the project links again.
- **Background particles / glitch settings** were saved but never applied. Both are
  now real, and glitch moved to its own key (`ui_glitch_text`, off by default) so
  nobody inherits an effect that never existed.
- **CI could not build**: the workflow compiled a partial source list (missing
  netprofile, scan, games, ram, storage, logging2, diagnostics, webframe), never ran
  `tools/embed_web.py`, and referenced launchers that do not exist in the repo.
- Long hardware strings no longer push the dashboard cards open.

### Changed

- Boot is roughly twice as fast: the tweak catalog read (~3 s) and the network read
  (~3 s) now run in parallel instead of in series.
- Squared, letterspaced primary buttons and a grouped sidebar with an active accent
  bar (Khadafi-grade control surface).

## [2.3.0] — 2026-09-20

### Added
- Tools mega-catalog: 79 Windows launchers in 9 searchable categories.
- Space Grotesk typography, refined glass surfaces and animations.

## [2.2.0] — 2026-09-19

### Added
- AnTuTu-style benchmark with weighted sub-scores, per-run deltas and history.
- 7 theme packs, first-run wizard offering to benchmark.

## [2.1.0] — 2026-09-19

### Added
- 49 reversible tweaks, standalone PowerShell engine, single all-in-one `.bat`.

## [2.0.0] — 2026-09-18

### Added
- Rebrand and rebuild as "Windows Gaming Control Center": embedded HTTP server,
  web dashboard, logged and reversible registry work.

## [1.1.0] — 2026-09-18

### Added
- Web dashboard with live monitoring.

## [1.0.0] — 2026-09-18

### Added
- Initial release: C++ dashboard plus PowerShell engine.

[2.5.0]: https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.5.0
[2.4.0]: https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.4.0
[2.3.0]: https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.3.0
[2.2.0]: https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.2.0
[2.1.0]: https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.1.0
[2.0.0]: https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.0.0
[1.1.0]: https://github.com/cameleonnbss/OptimizeKit/releases/tag/v1.1.0
[1.0.0]: https://github.com/cameleonnbss/OptimizeKit/releases/tag/v1.0.0
