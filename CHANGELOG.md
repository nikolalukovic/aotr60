# Changelog

## 1.0 — 2026-10-05

**Works with Age of the Ring 9.3.3** (AotR launcher 1.3.2), `rotwk\game.dat` SHA-256
`cc08275d60ff8e3bfd4374c29d61304dea8336e6dd00ab8add88b1df95a705dc`. Any other `game.dat` (another AotR version, or the
launcher's PvP delay-fix mode) is recognised as unsupported and runs at the stock 30 FPS.

First release.

### 60 FPS at stock game speed
- Renders at 60 FPS without changing game speed: the game logic runs exactly as in the stock game (5 logic ticks and
  30 game frames per second). AotR60 inserts one presentation-only frame between every two stock frames.
- Smooth units and camera: on the inserted frames, units and the camera picture are drawn halfway between two stock
  frames. Effects (particles, fades, water) still advance 30 times a second, as in stock.
- Active in the campaign, Living World (the strategic map and its battles), skirmish against the AI and tutorials.
  Menus, multiplayer, replays and Create-a-Hero stay at the stock 30 FPS.
- Save games are compatible both ways: a game saved at 30 FPS loads at 60 FPS and the other way round.
- Exact pacing: a high-resolution pacer replaces the stock frame limiter (two 16.5 ms halves per stock 33 ms frame).
- Automatic fallback to 30 FPS when the PC cannot hold 60 for a sustained period, with a retry after 30 s (doubling up
  to 8 min on repeats).

### Large battles
- Split present: when a logic step takes long in a big battle, the already drawn in-between frame is shown in the
  middle of the step, so one long hold becomes two short ones. Needs a DXVK `d3d9.dll` in `rotwk\`.

### Fixes to stock behaviour (single player, at 30 and 60 FPS, each switchable)
- Uniform camera scroll: the camera pans at the same speed over high and low ground and on moderate slopes (stock: slower
  over low ground and on slopes falling away from the camera), and keeps following the terrain while scrolling.
- Smooth resource area: the circle showing a resource building's area while you place it moves smoothly with the
  building (stock: it jumps in 20-unit grid steps).

### Controls and tools
- Hotkeys in the game window: Ctrl+Shift+F11 60 FPS, F10 interpolation, F9 split present, F8 uniform scroll, F7 smooth
  resource area. The window title shows the current state.
- Settings in `%APPDATA%\Age of the Ring\aotr60\aotr60.ini`; log in `aotr60.log`. `Telemetry = 1` / `2` add speed and
  consistency checks and a logic trace for determinism comparisons (`tools/compare_traces.py`).

### Install
- Writes only `rotwk\dinput8.dll` (`install.cmd`, or copy the release's `dinput8.dll` there). The game is patched in
  memory at start-up after the executable has been identified; every patch site (152) is verified byte for byte, and
  the game files are not modified.
