# AotR60 analysis archive

Reverse-engineering record behind `../PLAN.md` and the later features. All addresses refer to AotR's `rotwk\game.dat`
(image base 0x400000, see PLAN §7 for the build table).

| Path | Content |
|---|---|
| `SYNTHESIS.md` | First 11-agent synthesis: frame-timing architecture, 86-row inventory of frame-rate-dependent systems |
| `agents/` | Per-area notes of that run (core loop, W3D, drawables, particles, camera/UI/audio, hard-coded constants, classification of the 402 timing-global references) |
| `gaps/` | Second run: G1 clientUpdate allow-list, G2 draw path, G3 input, G4 logic mutation, G5 mode detection, G6 pacing, G7 AotR hooks, G8 delayfix, G9 bootstrap, plus `CRITIC.md` |
| `reviews/` | Adversarial reviews of the plan (logic identity, speed/pacing, implementability) and the camera-feasibility / consistency checks |
| `audits/` | Phase-0 audits (LOD neutrality, RainOfFire, transform-cache consumers) |
| `living_world/` | Phase 6, the Living World strategic map at 60 FPS: `plan.md` and the five area reports (armies, camera, modes, render, UI) |
| `hitching/` | Diagnosis of the regular hitch in large fullscreen battles: `synthesis.md`, area reports (timeline, engine, DLL, mitigation) and the rejected heavy-logic pacing variant (`heavy_logic_pacing.md`) |
| `split_present/` | Design of split present (showing the in-between frame inside a long logic step): `spec.md`, area reports (Direct3D/DXVK, synchronisation, logic checkpoints, simulation) and the review |
| `camera_scroll/` | Uniform camera scroll: why the stock camera slows down over low ground and slopes (`synthesis.md`, area reports) and the slope term (`slope_term.md`) |
| `main_menu/` | Why the main menu stays at 30 FPS (AotR's menus are APT/Flash animations authored at 30 fps) |
| `generals_timing_map.md` | Map of C&C Generals Zero Hour source (EA open source) frame-timing code, used as a structural reference |
| `data/` | `timing_refs.tsv` (all code references to the timing globals at 0xD9F608..0xD9F62C), static-initializer tables; `decompile/` holds the two Ghidra decompile excerpts `SYNTHESIS.md` cites by line (GameClient::update 0x64849E, GameLogic::update 0x62E4E8) |
| `tools/` | `ghq.py` (Ghidra dump lookup), `gdis.py` (capstone disassembly), table builders, `DumpAllParallel.java` + `analyze.cmd` (headless Ghidra dump) |

The notes are working records of the analysis and keep their original wording. Path placeholders in them:

- `<game folder>`: the Age of the Ring installation (the folder with `rotwk\` and `aotr\`).
- `<analysis workspace>`: the temporary folder the analysis ran in. Helper scripts, simulations, logs and dumps named
  under it were working files and are not part of the repository; the conclusions, numbers and addresses they produced
  are in the notes, and every address can be checked against a Ghidra dump (below).
- `an/`, `ghw/`: the layout of that workspace (`an/` for the lookup scripts and findings, `ghw/` for the Ghidra dump).

## Regenerating the Ghidra dump

The Ghidra dump itself is not in the repository: it is a decompilation of EA's game executable (about 125 MB of text)
and can be regenerated from your own copy of `rotwk\game.dat`:

1. Set `GHIDRA_HOME` (Ghidra 11.3.2) and `JAVA_HOME` (JDK 21).
2. In an empty working folder, copy `rotwk\game.dat` to `ghw\bin\game.dat` and `tools\ghq.py` / `tools\gdis.py` to `an\`.
3. Run `tools\analyze.cmd ghw\bin\game.dat ghw\out\game.dat <projectDir> <projectName>`. It imports and analyses the
   binary headlessly and writes `decompiled.c`, `listing.asm`, `functions.tsv`, `callgraph.tsv`, `strings.tsv` and
   `floatrefs.tsv` to `ghw\out\game.dat`.
4. From the working folder: `python an/ghq.py fn 0x6325A0` (decompiled function), `python an/ghq.py callers 0x6325A0`,
   `python an/gdis.py 0x6325A0 0x632620` (needs `pefile` and `capstone`), and so on.
