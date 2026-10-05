# AotR60 analysis archive

Reverse-engineering record behind `../PLAN.md`. All addresses refer to AotR's `rotwk\game.dat`
(image base 0x400000, see PLAN §7 for the build table).

| Path | Content |
|---|---|
| `SYNTHESIS.md` | First 11-agent synthesis: frame-timing architecture, 86-row inventory of frame-rate-dependent systems |
| `agents/` | Per-area notes of that run (core loop, W3D, drawables, particles, camera/UI/audio, hard-coded constants, classification of the 402 timing-global references) |
| `gaps/` | Second run: G1 clientUpdate allow-list, G2 draw path, G3 input, G4 logic mutation, G5 mode detection, G6 pacing, G7 AotR hooks, G8 delayfix, G9 bootstrap, plus `CRITIC.md` |
| `reviews/` | Adversarial reviews of the plan (logic identity, speed/pacing, implementability) and the camera-feasibility / consistency checks |
| `generals_timing_map.md` | Map of C&C Generals Zero Hour source (EA open source) frame-timing code, used as a structural reference |
| `data/` | `timing_refs.tsv` (all code references to the timing globals at 0xD9F608..0xD9F62C), static-initializer tables |
| `tools/` | `ghq.py` (Ghidra dump lookup), `gdis.py` (capstone disassembly), table builders, `DumpAllParallel.java` + `analyze.cmd` (headless Ghidra dump) |

## Regenerating the Ghidra dump

The analysis used a local Ghidra project. To regenerate the text dump that
`ghq.py` reads (`decompiled.c`, `listing.asm`, `functions.tsv`, `callgraph.tsv`, `strings.tsv`, `floatrefs.tsv`), run
`tools/analyze.cmd <game.dat> <outDir> <projectDir> <projectName>` (Ghidra 11.3.2, JDK 21) and point `ghq.py`'s `D`
variable at `<outDir>`. `gdis.py` expects the binary at `ghw/bin/game.dat` relative to the working directory.
