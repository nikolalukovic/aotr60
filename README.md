# AotR60 — native 60 FPS for Age of the Ring

AotR60 renders Age of the Ring (BFME2: Rise of the Witch-king engine) at 60 frames per second **without changing
game speed**. The game logic still runs exactly as in the stock game (5 logic ticks per second, 30 game frames per
second); AotR60 inserts one extra, presentation-only frame between every two stock frames and draws units and the
camera picture half a step in between (effects such as particles, fades and water still advance 30 times a second, as
in stock). Save games stay compatible both ways.

Where it is active: campaign, Living World (the strategic map and its battles), skirmish against the AI and
tutorials. Menus, multiplayer, replays and Create-a-Hero stay at the stock 30 FPS (AotR's menus are Flash animations
authored at 30 fps over a still image, so 60 FPS would only show every picture twice). On the Living World map, fades,
the switch to and from battles and other transitions run at 30 FPS for a moment.

**Version 1.0** works with **Age of the Ring 9.3.3** (AotR launcher 1.3.2): `rotwk\game.dat` with SHA-256
`cc08275d60ff8e3bfd4374c29d61304dea8336e6dd00ab8add88b1df95a705dc`. With any other `game.dat` (another AotR version,
or the launcher's PvP delay-fix mode) AotR60 stays inactive and the game runs at the stock 30 FPS. See
[CHANGELOG.md](CHANGELOG.md) for the features of each version and the
[releases](../../releases) for downloads.

## Install

1. Clone this repository **into your Age of the Ring folder** (the folder that contains `rotwk\` and `aotr\`):

   ```bat
   cd /d "D:\Games\Age of the Ring"
   git clone <URL of this repository> aotr60
   ```

2. Run `aotr60\install.cmd` (double-click it). It checks the game folder and the game version, then copies the
   prebuilt `bin\dinput8.dll` to `rotwk\dinput8.dll`. That is the only file it writes.
3. Start Age of the Ring as usual (AotR launcher). Start a skirmish or campaign mission: 60 FPS switches on
   automatically shortly after the match begins (within about half a second, or once a mission's opening fade-in has
   finished).

`aotr60\uninstall.cmd` removes it again. To update, `git pull` in the `aotr60` folder and run `install.cmd` again.

Without git: download `dinput8.dll` from the [latest release](../../releases/latest)
and copy it to `rotwk\dinput8.dll` (delete that file to uninstall). The DLL checks the game version itself, so a wrong
`game.dat` only means stock 30 FPS.

AotR60 patches the game **in memory** when it starts; no game file is modified. The patches are applied only after
the executable has been identified as the Age of the Ring build AotR60 was made for; with any other version the DLL
just forwards DirectInput and the game runs at stock 30 FPS.

**AotR launcher:** starting the game through the launcher works. The launcher reports `rotwk\dinput8.dll` as a
modified game file; do not accept its PATCH, which deletes the DLL (if it happened, or after every AotR update, run
`install.cmd` again). The launcher's PvP (delay-fix) mode replaces `rotwk\game.dat` with a different build; AotR60
recognises that build and leaves it at the stock 30 FPS (`install.cmd` warns about an unsupported `game.dat` while PvP
mode is on).

### Building from source (optional)

Needs Visual Studio 2026 with the C++ x86 tools (CMake is the one bundled with Visual Studio).

```powershell
.\build.ps1              # build and run the unit tests
.\build.ps1 -Publish     # ... and refresh bin\dinput8.dll
.\install.cmd -Build     # build from source and install that DLL
```

## In game

The game window's title bar shows the current state (in windowed mode), for example
`[AotR60: 60 FPS, smooth]` or `[AotR60: 30 FPS, <reason>]`.

| Keys (in the game window) | Effect |
|---|---|
| Ctrl+Shift+F11 | 60 FPS on / off (off at the next frame pair, on at the next logic tick, within 0.2 s) |
| Ctrl+Shift+F10 | smooth unit/camera interpolation on / off |
| Ctrl+Shift+F9 | split present (large battles, see below) on / off (no effect with `SplitPresent = 0`) |
| Ctrl+Shift+F8 | uniform camera scroll (see below) on / off |
| Ctrl+Shift+F7 | smooth resource area while placing a building (see below) on / off |

If the PC cannot hold 60 FPS for a sustained period, AotR60 falls back to 30 FPS for 30 s (doubling up to 8 min on
repeats; back to 30 s after five minutes of smooth 60 FPS play, where time at 30 FPS or in menus does not count) and
then tries again. Isolated hitches do not trigger this.

**Large battles (split present).** The game runs its logic five times a second on the main thread. In big battles one
of those steps can take 20-40 ms; nothing can be shown while it runs, so at 60 FPS the screen would hold the last frame
for the whole step (a regular hitch; a stock 30 FPS frame has room to hide it). AotR60 predicts these steps and shows
the already drawn in-between frame in the middle of the step instead: a timer thread marks the moment, and the game
thread presents the frame from cheap checkpoints inside the logic (all Direct3D calls stay on the game thread). One long
hold becomes two short ones. The logic itself is untouched. Ctrl+Shift+F9 switches it off and on while playing;
`SplitPresent = 0` switches it off for the whole session (the hotkey cannot switch it back on). It needs the
`d3d9.dll` in the game folder (DXVK); with the system `d3d9.dll` it stays off unless `SplitPresentNative = 1`.

**Uniform camera scroll.** The stock camera pans slower over low ground and faster over high ground (the scroll
step is scaled by the camera's absolute height), freezes the camera height while scrolling, and slows down on slopes
that fall away from the camera (in both scroll directions). With `UniformScroll = 1` (default, single player) the step
uses the height above the ground, the camera keeps following the terrain while scrolling, and a slope term measured
from the camera's own height grid speeds the step up on falling slopes (at most 2x) and down on facing ones (at most
0.71x), so panning keeps the same on-screen speed over hills, valleys and moderate slopes; very steep slopes are only
partly corrected. The speed where you first scroll on a map is unchanged. It applies at 30 and 60 FPS, affects only
player scrolling (not scripted cameras) and does not change game logic or speed. `UniformScroll = 0` or
Ctrl+Shift+F8 restores the stock camera; `UniformScrollSlope = 0` keeps the height part without the slope term.

**Smooth resource area.** While a resource building is being placed, the game shows the area it will take for resources
as a circle on the ground. Stock snaps that circle to the 20-unit resource grid, so it jumps from cell to cell while the
building moves freely under the cursor. With `SmoothResourceArea = 1` (default, single player) the circle stays centred
on the building, so it can sit up to one grid cell away from where the stock circle would be. What the building
actually claims, the colours of existing claims on the ground and the percentage shown are unchanged.
`SmoothResourceArea = 0` or Ctrl+Shift+F7 restores the stock circle.

## Settings

`%APPDATA%\Age of the Ring\aotr60\aotr60.ini` (created on first start; delete it to restore the defaults):

| Key | Default | Meaning |
|---|---|---|
| `Enabled` | 1 | 60 FPS in supported games (0 = stock 30 FPS; the hotkey can still switch it on) |
| `Pacing` | stock | `stock` = exact stock speed; `nominal` = perfectly even 1000/60 ms frames, game 1 % slower |
| `UnitInterpolation` | 1 | draw units half a step in between on the inserted frames |
| `CameraInterpolation` | 1 | interpolate the camera picture too (needs UnitInterpolation) |
| `UniformScroll` | 1 | same camera pan speed over high and low ground and on slopes, single player (0 = stock camera) |
| `UniformScrollSlope` | 1 | the slope part of UniformScroll |
| `SmoothResourceArea` | 1 | the resource area circle follows a building being placed smoothly, single player (0 = stock grid steps) |
| `PresentPacing` | 1 | space the presented frames evenly, half a stock frame apart (matters without vsync, and with vsync on a 120 Hz or faster display) |
| `SplitPresent` | 1 | large battles: show the in-between frame in the middle of a long logic step (0 = off; 2 = profile, 3 = stress: testing only) |
| `SplitPresentEarly` | 1 | with split present: start the frame before a predicted heavy logic step early |
| `RepayProportional` | 1 | catch up late frames faster when more time is owed (keeps exact speed in heavy battles) |
| `SplitPresentNative` | 0 | allow split present with the system d3d9.dll instead of DXVK (untested) |
| `LivingWorldMap` | 1 | 60 FPS on the Living World strategic map too (0 = the map stays at 30; battles unaffected) |
| `Fallback` | 1 | automatic fallback to 30 FPS on sustained overload |
| `Telemetry` | 0 | 1 = speed/consistency checks and frame-stall lines in `aotr60.log` plus `aotr60_rates.csv`; 2 = also a logic trace (testing) |

Logs: `aotr60.log` (and the previous session's `aotr60.log.prev`) in the same folder. Keys from older versions (for
example `AllowLivingWorldMap`) are ignored. `AllowDelayfix` in the generated file currently has no effect: the
delay-fix build always runs at the stock 30 FPS.

## Status

Tested in play (DXVK, 4K at 120 Hz, windowed and exclusive fullscreen): skirmishes including large battles,
the Living World map, save games made at 30 FPS loaded at 60 FPS and the other way round, uniform camera scroll and
the smooth resource area. The logic-trace comparison (`Telemetry = 2`, see Development) has not been run on the
current version.

Known limits:

- Menus, multiplayer, replays and Create-a-Hero run at the stock 30 FPS (see the top of this page).
- The game reads the mouse 30 times a second, as in stock, so a building being placed follows the cursor in 30 FPS
  steps (the mouse pointer itself is drawn by Windows and moves smoothly).
- The game's own long logic steps in very large battles are still visible as short holds; split present halves them.
- Uniform scroll corrects moderate slopes fully, very steep ones only partly.

## How it works (short)

See `docs/PLAN.md` for the full design and `docs/analysis/` for the reverse-engineering notes.

- Each stock iteration (render + logic sub-step) becomes an **A-render** (does all of the stock render's state
  stepping, but draws the in-between picture) followed by a halted step, and a **B-render** (redraws the stock
  render's picture without stepping anything persistent) followed by the unmodified logic step. Logic therefore runs
  at exactly the stock rate and sees exactly the stock state.
- Everything that steps client state once per drawn frame — UI updates, camera stepping, input, particle updates,
  fades, counters — runs only on A-renders. Animation-frame events (FX/Lua events) are evaluated once per pair on the
  B-render, and the W3D animation clock advances half on A and the rest on B, so every system keeps the stock
  30-per-second cadence. All patches are in `tools/sites.json`: 152 sites, each verified byte for byte against
  `game.dat` and each with a stock path; up to 148 are installed (the 4 half-step effect sites are not used yet, and
  the 13 split-present sites only with `SplitPresent` on).
- On A-renders units are drawn halfway between two stock frames (fraction (2k-1)/12 of the logic step instead of
  the stock k/6) and the camera picture halfway between the previous and current camera; B-renders show exactly the
  stock picture.
- On the Living World map the armies and the map camera are moved by the A-render's client update as in stock (the
  Living World logic reads the result). A-renders draw the map with each moved army and the camera halfway between
  their previous and current position and restore the exact positions right after drawing.
- A QPC-based pacer replaces the stock 33 ms frame limiter while 60 mode is active (two 16.5 ms halves).
- Split present (large battles): a timer thread marks the middle of a long logic step and the game thread presents
  the already drawn in-between frame from checkpoints inside the logic; all Direct3D calls stay on the game thread.
- Two stock behaviours are corrected on top, at 30 and 60 FPS, single player only and each with its own switch: the
  camera pan speed over high and low ground (uniform scroll) and the resource area circle while placing a building.

## Development

- `tools/sites.json` is the single source of every patch; `tools/verify_sites.py` re-checks all spans against
  `rotwk\game.dat`, `tools/gen_sites.py` generates `src/sites.gen.h` and `src/stubs/sites.gen.inc`.
- `src/stubs/stubs.asm` holds all stubs; `src/frame_ctl.cpp` the 30/60 mode controller; `src/pacer.cpp` the pacer;
  `src/camera.cpp` the camera presentation; `src/telemetry.cpp` the measurements.
- Unit tests: `build\Release\aotr60_tests.exe` (run by `build.ps1`).
- `Telemetry = 2` writes `aotr60_trace_<date>_<time>.txt` per session: logic frame, sub-step, logic RNG seed and
  30/60 mode per logic call, the engine's sync CRC once per logic tick (sub 1), plus one `# LW` line per Living World
  logic tick (seeds, map view state, army transforms).
  `tools/compare_traces.py` finds a game played at 60 FPS and a 30 FPS game from the same starting seed in the newest
  trace (the replay of a skirmish, or the same save loaded again with Ctrl+Shift+F11 off) and compares them tick by
  tick; `tools/compare_traces.py A B` compares the longest game of two trace files.
