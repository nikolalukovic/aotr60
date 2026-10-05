# AotR60 — native 60 FPS for Age of the Ring

AotR60 renders Age of the Ring (BFME2: Rise of the Witch-king engine) at 60 frames per second **without changing
game speed**. The game logic still runs exactly as in the stock game (5 logic ticks per second, 30 game frames per
second); AotR60 inserts one extra, presentation-only frame between every two stock frames and draws units, the camera
and effects half a step in between. Save games stay compatible both ways.

Where it is active: campaign, Living World battles, skirmish against the AI and tutorials. Menus, multiplayer,
replays, the Living World strategic map and Create-a-Hero stay at the stock 30 FPS.

## Install / uninstall

AotR60 is a proxy `dinput8.dll` placed in `rotwk\`. Nothing else in the game folder is changed; every patch is
applied in memory when the game starts, and only after the game executable has been identified as the AotR build it
was made for (otherwise the DLL just forwards DirectInput and the game runs stock).

```powershell
.\build.ps1      # build (Visual Studio 2026 C++ x86 tools) and run the unit tests
.\install.ps1    # copy build\Release\dinput8.dll to ..\rotwk\
.\uninstall.ps1  # remove it again
```

**AotR launcher:** the launcher reports `rotwk\dinput8.dll` as a modified game file. Accepting its PATCH deletes the
DLL; run `install.ps1` again afterwards (also after every AotR update).

## In game

The game window's title bar shows the current state (in windowed mode), for example
`[AotR60: 60 FPS, smooth]` or `[AotR60: 30 FPS, <reason>]`.

| Keys (in the game window) | Effect |
|---|---|
| Ctrl+Shift+F11 | 60 FPS on / off (switches at the next frame pair) |
| Ctrl+Shift+F10 | smooth unit/camera interpolation on / off |

If the PC cannot hold 60 FPS for a sustained period, AotR60 falls back to 30 FPS for 30 s (doubling up to 8 min on
repeats) and then tries again. Isolated hitches do not trigger this.

## Settings

`%APPDATA%\Age of the Ring\aotr60\aotr60.ini` (created on first start; delete it to restore the defaults):

| Key | Default | Meaning |
|---|---|---|
| `Enabled` | 1 | 60 FPS in supported games (0 = stock 30 FPS; the hotkey can still switch it on) |
| `Pacing` | stock | `stock` = exact stock speed; `nominal` = perfectly even 1000/60 ms frames, game 1 % slower |
| `UnitInterpolation` | 1 | draw units half a step in between on the inserted frames |
| `CameraInterpolation` | 1 | interpolate the camera picture too (needs UnitInterpolation) |
| `PresentPacing` | 1 | without vsync: space the presented frames evenly |
| `Fallback` | 1 | automatic fallback to 30 FPS on sustained overload |
| `Telemetry` | 0 | 1 = speed/consistency checks in `aotr60.log` and `aotr60_rates.csv` (testing) |

Logs: `aotr60.log` (and the previous session's `aotr60.log.prev`) in the same folder.

## How it works (short)

See `docs/PLAN.md` for the full design and `docs/analysis/` for the reverse-engineering notes.

- Each stock iteration (render + logic sub-step) becomes an **A-render** (exactly the stock render) followed by a
  halted step, and a **B-render** (a presentation-only repeat) followed by the unmodified logic step. Logic therefore
  runs at exactly the stock rate and sees exactly the stock state.
- Everything that advances per drawn frame — UI updates, camera stepping, input, particle updates, animation-frame
  effects, fades, counters — runs only on A-renders (124 verified patch sites in `tools/sites.json`, each with a
  byte-exact stock path).
- On A-renders units are drawn at the half step between logic frames and the camera picture halfway between the
  previous and current camera; B-renders show exactly the stock picture.
- A QPC-based pacer replaces the stock 33 ms frame limiter while 60 mode is active (two 16.5 ms halves).

## Development

- `tools/sites.json` is the single source of every patch; `tools/verify_sites.py` re-checks all spans against
  `rotwk\game.dat`, `tools/gen_sites.py` generates `src/sites.gen.h` and `src/stubs/sites.gen.inc`.
- `src/stubs/stubs.asm` holds all stubs; `src/frame_ctl.cpp` the 30/60 mode controller; `src/pacer.cpp` the pacer;
  `src/camera.cpp` the camera presentation; `src/telemetry.cpp` the measurements.
- Unit tests: `build\Release\aotr60_tests.exe` (run by `build.ps1`).
