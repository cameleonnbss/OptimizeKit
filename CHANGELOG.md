# Changelog

All notable changes to OptimizeKit are documented here.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

[2.4.0]: https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.4.0
[2.3.0]: https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.3.0
[2.2.0]: https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.2.0
[2.1.0]: https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.1.0
[2.0.0]: https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.0.0
[1.1.0]: https://github.com/cameleonnbss/OptimizeKit/releases/tag/v1.1.0
[1.0.0]: https://github.com/cameleonnbss/OptimizeKit/releases/tag/v1.0.0
