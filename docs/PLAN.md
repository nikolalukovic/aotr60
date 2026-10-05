# Plan: native 60 FPS for Age of the Ring ("AotR60")

## Context

Age of the Ring runs on the BFME2: Rise of the Witch-king SAGE engine. The executable that actually runs is
`rotwk\game.dat`, AotR's patched 2.01 build (§7).

**How the engine keeps time:**

- Each main-loop iteration renders once, then advances a sub-frame stepper by one.
- Game logic ticks every 6 iterations (5 Hz).
- The frame limiter (33 ms per iteration) is the only clock.

So the game speed equals the frame rate. Raising `FramesPerSecondLimit` makes everything run faster, which is why the
community never shipped 60 FPS.

**Goal.** Real 60 FPS rendering with:

- **game speed identical** to stock (nothing 2x or 0.5x);
- **bit-identical game logic**: same 5 Hz ticks, same call pattern, same logic-RNG calls;
- **interchangeable save games** between 30 and 60 FPS.

**Scope:** campaign, Living World (battles first, strategic map in phase 6) and skirmish. Multiplayer stays at stock 30.

**Delivery:** a `dinput8.dll` proxy that patches the game in memory. No game file is changed.

**"Fixed 60 FPS" here means** about 60.6 renders/s: 2 × the stock 30.3 frames/s, i.e. 33.000 ms per pair, exactly
stock speed. On a 60 Hz vsync display you'll see 60 Presents/s; there is an automatic fallback to 30 if the PC can't
keep up.

**Analysis behind this plan.** A headless Ghidra 11.3.2 analysis of game.dat: 69,196 functions, full decompile,
stored in a local Ghidra project. On top of it:

- about 27 analysis agents;
- a cross-check against EA's open-sourced C&C Generals engine, from which BFME2's engine descends;
- three adversarial reviews and a camera-feasibility and consistency check of this plan.

All addresses and bytes were verified against the binary.

---

## 1. Design: stock 30 Hz simulation, plus an inserted presentation-only render

### 1.1 Terms

| Term | Meaning |
|---|---|
| Bases | GE = TheGameEngine `[0xDE4324]`; GL = TheGameLogic `[0xDE412C]`; GC = TheGameClient `[0xDE4388]`; GD = GlobalData `[0xDE4364]`; TV = TheTacticalView (W3DView) `[0xDE447C]`. |
| `s` | Stepper sub-frame `GE+0x34`: 1..6 per tick, and 7, 8, … while stalled. |
| `m_frame` | Client frame counter `GC+0x10`. Saved in saves; stays 30 Hz. |
| `GC+0xC8` | "Logic advanced" flag. The next render does `m_frame++` only if it is set. |
| `g_m60` | DLL flag, "60 mode active". When 0, **every patch takes its stock path** (the 30-mode branch). |
| X-iteration | **A-render**, followed by the **halted step** (the stepper's own halted branch; no logic). |
| Y-iteration | **B-render**, followed by the **stock step** (`s++`, logic sub-step `s`). |
| `g_uiTick` | 1 on X renders, 0 on Y renders. In 30 mode it is always 1. X and Y strictly alternate, also while paused or frozen, so every A-only system runs at stock 30/s. |
| `g_inB` | True during a B-render. |
| Presentation window | On A-renders only: during the W3DView drawable pass and the scene render (§1.6). Inside it the DLL temporarily applies the half-step fraction, cache key and camera picture. |
| k | `s` at render time (k = 1..6). |

```
X: A-render (stock render k: m_frame++, client systems, input, camera step; presentation windows show "k−½") → halted step
Y: B-render (presents exactly what stock render k presented; steps nothing persistent)                    → stock step (logic)
per tick: 6 × (X+Y) = 12 renders = 198 ms = stock. Logic sees sub 1..6 exactly once per tick, in stock order.
```

### 1.2 The core invariant

**Outside the presentation windows, the A-render is stock render k.** That covers:

- the same order and the same calls;
- the same fraction `GE+0x3C` = k/6;
- the same camera stepping and inputs;
- the same `m_frame++`.

**The B-render steps nothing persistent.** It redraws stock render k's picture: fraction k/6, camera M_k, drawable pass on
the same region. It is limited to the allow-list (§1.4), where everything that might mutate state is A-only.

**So when logic runs (right after the B-render), everything it can read equals stock:**

- `m_frame`;
- camera state, flags, waypoint id and messages;
- the drawable transform cache, filled by the B-render's pass exactly like stock render k;
- the stepper fraction.

**Exceptions that legitimately differ inside a pair but are exact again at logic time (§1.5):**

- the W3D sync clock (lag phase, exact sum);
- client-physics replay (pure copy);
- visual-only render integrators (A-only, or half-steps in phase 4b).

**Smoothness comes from the A-render's presentation windows:**

- unit transforms at fraction min(1,(2k−1)/12);
- the W3D animation clock half-advanced;
- the camera picture interpolated halfway between M_{k−1} and M_k.

Units and camera therefore share the same constant 16.7 ms display delay and never jitter against each other.

### 1.3 Stepper ("route 2": stock stepper bytes untouched)

| Hook | Site (original bytes) | Contract |
|---|---|---|
| **C0** pre-render | `0x6325CF` `ff 90 9c 00 00 00` | `e8 C0Stub; nop`. Toggles X/Y (when `g_m60`), increments `g_renderId`, runs the mode controller (§1.8), sets `g_inClientUpdate` (main thread only), then calls clientUpdate. ECX=ESI=engine; EBX (halt flag) preserved. |
| **Halt** | `0x6325D5` `84 db a1 88 43 de 00` (7 bytes) | If `g_m60 && the render just finished was X`, take the stepper's halted branch `0x6325DE` (GC+0xC8=0, Debug vt94, return). Otherwise stock (ZF from `test bl,bl`, EAX=`[0xDE4388]`). Never keyed on GC+0xC8, so tick attempts stay 30/s while paused, frozen or stalled, as stock. |
| **isTick** | `0x63252F` `a1 0c f6 d9 00` (5 bytes) | Returns 0 only when `g_m60 && g_inClientUpdate && g_inB`. Stock otherwise; a logic-side caller (`0x6759BB` via `0x7779E3`) must see stock behaviour. |
| **C4** cache key | `0x6765D5` (12 bytes, contains `push ebx`), `0x67173B`, `0x671774` (11 bytes each) | Key = `2·m_frame − presentationWindowActive`. A-window transforms never leak to B, to post-draw A code or to logic; while paused the key never churns (same as stock). Expiry `+0x378` (saved) stays on `m_frame`. |

### 1.4 B-render allow-list (safe default)

**Runs on every render:**

- the draw (W3DDisplay::draw `0x44B788` → drawFrame `0x449CF8`; camera handling per §1.6);
- deferred D3D queue `0x532D6F`;
- GC vt90 (deferred delete);
- **message propagation `0x7128C3`** (empty on B-renders; on A-renders it acknowledges camera message 0x452 in the same render, as stock);
- snow `0x4943E1`: it consumes the W3D sync delta, and only its counters `0x49448D`/`0x494494` are A-only.

**A-only, gated at the call site by `g_uiTick`:**

- clientUpdate: pending game start `0x62B385` (both call sites `0x63241E`, `0x648817`, so resets and new games happen on A-renders), APT, radar, keyboard drain, audio;
- GameClient::update `0x64849E`:
  - LookAt `0x6484B2`, LW translator `0x6484BD`, gesture `0x6484C8`, LW view `0x6484D7`, `0x645750`;
  - cloud, fire, Anim2D, keyboard `0x6485CC`, ScoredKillEva, Eva, mouse `0x6485FC`, popups;
  - WindowManager plus transitions, TerrainVisual/water (span at `0x648833`), Display::update, DisplayStringManager, Shell;
- InGameUI::update (`0x6488A6` → thunk `0x48EA1F` → `0x6A1F4D`);
- the Palantir callee `0x5039C1` (also covers the LW path and AotR's per-call income sampler);
- the LW manager `0x49AAD4` and display vt+0x188 `0x444CD4`. The **LW eye tower consumes the logic RNG**, so these must stay A-only.

**Already A-only by itself:** the drawable update block (`m_frame`-gated at `0x648705`).

**Every site has its own exact byte contract** in `sites.json` (§2). Only 5 sites are plain `8b 01 ff 50 28`. For example,
radar `0x632486` keeps EDI=0 and `[ebp-4]`, and audio `0x6324F9` keeps the `0xDE4330` store.

**Coverage rule.** `verify_sites.py` lists every call in clientUpdate `0x632409`, GameClient::update and InGameUI::update and
**fails on any call not classified run or gate**. Per-iteration work outside them is listed and counted in telemetry; it now runs at 60/s, harmless but measured:

- the asset streamer `[0xDEF548]`→vt28 (`0x6325B0`);
- the script-debug bridge `0x604189`/`0x603452`;
- Debug vt94;
- the IsIconic wrapper `0x44181F`;
- the watchdog `0x631D04`.

### 1.5 What legitimately advances on B-renders, and render-internal integrators

**W3D sync clock C3, lag phase** (`0x44B911`, 14 bytes `a1 8c 7a dc 00 0f af c6 01 05 80 75 dc 00`):

- An advancing A-render adds h = 16/17 alternating and records `owed = [0xDC7A8C]·d − h`.
- The following B-render adds `owed`, only after an advancing A-render.
- So sync = stock at logic time, and it stays frozen during pause and frozen-time cinematics like stock.
- 30 mode: stock.
- Fast-forward guard S0 at `0x44B8D0` (8 bytes): that path steps the camera and adds sync; on B-renders jump to `0x44BC5F`.

**Client-physics replay C5** (detour `0x67BDC0`). `calcPhysicsXform` returns 0 when `m_frame` is unchanged, which would drop
pitch/roll/bob on B-renders. Cache its output per `{loco ptr, m_frame}` (DLL table) and replay it.

**Particles C7.** Gate the manager calls `0x449D40` and `0x444CF2` A-only, so even "update-while-halted" systems stay 30 Hz.

**Render-internal integrators.** Phase 2 makes all of them **A-only** (exact, 30 Hz steps). Phase 4b converts *linear
float* steps that are visible to exact half-steps. INC counters and multiplicative decays stay A-only.

| Effect | Site(s) |
|---|---|
| View-filter fades | INC `0x4FB599`, `0x4FB5DD`, `0x4FBC7C`, `0x4FBCC0`, `0x4F5C3A`, `0x4F5C8D`, `0x4FCBAC`, `0x4FCBF0` (A-only) |
| Overlay UV | `0x4F9904`, `0x4F9A0D`, `0x4F9A22`, `0x4F9A34`, `0x4F6C86`, `0x4F8321`, `0x4F678D`, `0x4F7EEE` |
| Water river UV | `.rdata 0xBE5608`/`0xBE5604` |
| Shore waves | `0x4FDB4A` |
| Trees | call `0x449D55` |
| Shrubs | call `0x4E83F9` |
| Decal spiral | `0x732A84` |
| Material ×0.8 per draw | `0x67C4CB` (A-only) |
| Recoil | call `0x4C78CE` |
| Light flicker | `0x4CF2BF`, `0x4CF3D3` |
| Rope/treads/truck | `0x4CDDCB`, `0x4CA59E`, `0x4CC4B9` |
| Boat wake | `0x4D05B3`, `0x4D05BD` |
| Debris | `0x4B135C` |
| Subobject fade | `0x8B8F40` |
| Outline fade | `0xB53A5C`, `0xB53A81` |
| LightPulse | creation `0x46D7A9` |
| Instance fade | `0x4D1436`, `0x4D1463` |
| Overlay fade | `0x44A033`, `0x44A12E` |
| Subtitles | `0x44A203` |
| UI particles | `0x6A53FB`; half-step form: p += v/2 every render, v *= d only on the B-render after its half-step |
| World-anim rise | `0x69DF0E` |

The motion-blur end-pan counter `0x4FD1B4` is A-only too.

### 1.6 Camera: stepped only on A-renders; the picture is interpolated

**A-render: the camera runs 100 % stock.**

- Order: LookAt input → W3DView::update (follow, scripted moves, eases, shake, settle, zone zoom, setCameraTransform) → InGameUI keyboard camera → translator camera actions during propagate.
- This uses the **stock fraction**: the presentation fraction is not active yet. Follow mode reads the fraction through `0x676711`.
- So camera state, freeze flag `+0x23D0`, multiplier `+0x23D4`, finished flag `vt78`, waypoint `+0x23CC` and 0x452 messages are **bit-exact stock**.
- No doubled counters, no ease conversions, no split steps. 30↔60 switching is safe at any pair boundary.

**Skip sites** (exact bytes and live state in `sites.json`):

- **S1, W3DView::update `0x48BD1B`** (10 bytes `c6 45 f2 00 0f 84 dd 00 00 00`), one site for the whole function.
  - On B-renders: write the cave's `mov byte[ebp-0xE],0`, then `lea edi,[ebx-0xB4]`, swap the W3D camera to M_k, and `jmp 0x48C701`.
  - This skips every camera mutation and runs the unmodified drawable pass on stock render k's camera and region.
  - On A-renders and in 30 mode: the stock path.
- **S2, `0x48C701`** (`a1 0c 1e dd 00`), the start of the drawable pass. On A-renders: record M_k (camera `[view+0x104]`: transform cam+0x18 (48 bytes) and view plane cam+0xD8..+0xF0), then open the presentation window: fraction (2k−1)/12 and C4 key `2·m_frame−1`. Close the window at the end of the pass and reopen it for the scene render.
- **Scene-render window, `0x449DAB` → `0x44A23E`.**
  - Opens at `0x449DAB` (`a1 2c 41 de 00`), after particles, trees and shroud and before shadow, water, RenderViews and RenderUI. Effective camera: A presents lerp/slerp(M_{k−1}, M_k, ½) via Set_Transform vt+0x54 `0x533550` and Set_View_Plane `0x533590`, with a shadow refit `0x47D37D` when shadows are on; B presents M_k.
  - Restores at `0x44A23E` (`8b 0d ac 3b de 00`), which every drawFrame exit passes: the real camera, view plane, `+0xFC=0`, the shadow refit and the stock fraction and key.
  - Guards end the swap before motion-blur lookAt `0x4FD254` and before any setCameraTransform `0x48B7B1`.
  - setCameraTransform and buildCameraTransform are never re-run for the swap, because they step state and fire notifications.
- **No interpolation**, i.e. M_k is presented as-is, on cuts (large delta) and while a shake is active (`view+0x128 > 0.01` or `0x4655DD`).

**Camera time multiplier M>1 (scripted fast time).**

- `0x44B964` (`ff 0d b4 8c d9 00`): the draw-skip counter decrements on A-draws only.
- A B-draw renders iff its A-draw rendered, and jumps past the 29 ms wait (also bypassing the `[edi+0x115]` check at `0x44B995`).
- Result: one drawn frame per M−1 steps with the stock 29 ms wait per drawn frame, and 60 mode stays on.

### 1.7 Pacing (60 mode only; 30 mode = stock limiter)

**Trampoline at `0x63A196`** (`ff 15 20 09 bd 00`) runs a QPC deadline pacer:

- `T = trunc(1000/(FPSlimit·netSpeedMult[0xD9F498]))/2` = 16.5 ms, so each pair takes 33.000 ms (stock: 33 ms per frame at 1 ms timer resolution).
- High-resolution waitable timer plus a final spin.
- Exits like stock: `0x63A1F5`/`0x63A21F` with EDI=`timeGetTime()`, EBX=0, x87 stack empty.
- Keeps writing the stock stats (`0xDE4318`, `0xDE4314`, `0xDE4310`, `0xDE430C`).
- Integer QPC math only. The game forces 24-bit x87 precision (`0x440809`); the DLL never touches the x87 control word or MXCSR.

**Gaps:** if the pacer did not run in the previous iteration, the deadline resets to "now" with no catch-up. That covers:

- the stock unlimited path `0x63A200` (M>1, fast-forward, UseFPSLimit off, the first 6 frames of a game);
- device-lost `Sleep(200)` `0x522696` and `Reset_Device`.

Lateness is clamped at 1.25·T and the clamped part is added to a **debt** counter.

**Present-skip:**

- Applies only when ≥1 frame late, the pacer ran in the previous iteration, and the frame is a B-render.
- Skip only its Present: the 15-byte span `0x522644`…`0x522653` returns EAX=S_OK with EBX=0.
- All state stepping still runs. This absorbs the 60 Hz-vsync deficit: FIFO would hold 60.00 instead of 60.61, 1 % slow. That costs about one skip per 1.65 s, the same hitch rate stock already has on 60 Hz.

**Fallback to 30 mode:**

- Triggers when debt exceeds 0.3 % of wall time over a sliding 5 s window, or Present-skips exceed 10 % of B-renders over 5 s.
- The switch happens at the next pair boundary; re-probe later with backoff.
- 60 mode is not enabled when fullscreen vsync is on and the display refresh is below 59 Hz.

**DXVK** (`rotwk\d3d9.dll`, `dxvk.conf`):

- leave the present interval stock (fullscreen FIFO, windowed IMMEDIATE);
- do **not** set `d3d9.maxFrameRate`, which fights the pacer;
- the test matrix includes DXVK on and off.

**Dynamic LOD.** In 60 mode the FPS average feeding LOD (`0x4430BB` → Display+0x180 → `0x4438DA`) samples A-renders only, so
AotR's `gamelod.ini` load shedding behaves like stock. Phase 0 asserts that the logic-read LOD values stay neutral:
SlowDeathScale == 1 and DebrisSkipMask == 0.

### 1.8 Mode controller

**Runs in C0.** It turns 60 mode on or off **only at a pair boundary**, i.e. before an X-iteration that follows the stock step.
Nothing is ever doubled, so this is always exact. Switching flushes owed sync, the C5 table and the camera history (M_{k−1}).

**On requires all of the following:**

- `Enabled`, known build (§7), not delayfix;
- singletons non-null;
- `TheNetwork [0xDE4468]==NULL`;
- `GL+0x110 ∈ {0 campaign/LW campaign battle, 2 skirmish/War-of-the-Ring battle, 6 tutorial}`, or (phase 6) the
  Living World map: mode 8, LW logic `[0xDE4950]+0xB4/+0xB5` set, LW view `[0xDE4958]+0x18` set and `+0x19` clear,
  no fade (`+0x14` 2/3), no battle-transition flags (`view+0x24/+0x25`, `LWL+0x176/+0x177`), empty UI-sequence queue
  `[0xDE8900]`; `LivingWorldMap=1`;
- `GL+0x114 ∉ {1,2}`;
- `GL+0x9D==0`;
- `GL+0x125` set, or the LW view active, only together with mode 8 (GL+0x125 alone is not a reliable map flag: the
  battle-return handler's GameLogic::reset clears it while the map stays shown);
- not replay playback (`[0xDE7CD8]+0x1C != 1`);
- `GD+0xBBD==0`, `GD+0xD45==0` (Create-a-Hero), not in the intro (`GD+0xAF2/0xAF3`), `GD+0x26` (UseFPSLimit) set;
- `[0xDE3B98]==NULL` (no script-debugger DLL);
- not in pacer fallback.

**Reset hook** (`0x44181A` `e9 f2 44 1f 00` → `GameEngine::reset 0x635D11`):

- Every new game, load, restart, exit and LW battle transition goes through it.
- It sets `g_m60=0` immediately; the halt reads the live flag, so the current iteration completes as a stock one.
- It zeroes owed sync, sets `g_uiTick=1` and clears the caches.
- Because `0x62B385` and APT (save/load) are A-only, resets happen on A-renders.

---

## 2. Deliverable, repo and engineering rules (git repo `<game folder>`, branch `aotr60`)

```
aotr60/
  CMakeLists.txt, CMakePresets.json        MSVC 14.51 x86 (VS 18), C++20, /MT, /arch:SSE2; generator "Visual Studio 18 2026" -A Win32
  build.ps1 / install.ps1 / uninstall.ps1  install copies dinput8.dll into rotwk\ only; refuses an unknown existing dinput8.dll
  config/aotr60.ini.default                committed template (copied to %APPDATA%\Age of the Ring\aotr60\ on first run)
  tools/sites.json                         SINGLE SOURCE for every patch: id, phase, address, original span bytes, length (instruction
                                           boundary), kind, replacement, stub, live state after (regs/EFLAGS/x87 depth/XMM), 30/60
                                           behaviour, evidence
  tools/gen_sites.py                       sites.json → src/sites.gen.h + docs/DESIGN.md site table
  tools/verify_sites.py                    spans vs rotwk\game.dat, aotr\zGameDats\delayfix.dat, rotwk\game820.dat; no branch target inside a
                                           span; coverage rule (§1.4)
  tools/report.py                          telemetry CSVs (30 vs 60) → PASS/FAIL tables in docs/verification/
  src/proxy.cpp + dinput8.def              DirectInput8Create → real SysWOW64 dinput8, loaded lazily on first call (never in DllMain)
  src/dllmain.cpp                          host check → verify every site → write all (all-or-nothing) → else pure forwarder
  src/patcher.cpp                          call/jmp redirect, span replace, imm/disp write, ModRM rewrite (d8 88 → d8 0d), pointer slot
                                           (checks old ptr), all with the 30-mode original values kept
  src/frame_ctl.cpp pacer.cpp hooks_render.cpp hooks_client.cpp hooks_camera.cpp telemetry.cpp config.cpp
  src/stubs/                               call-boundary stubs (may call C++ after saving the table's live state) and mid-stream stubs (pure asm,
                                           DLL globals only; C++ only with flags+GPRs+XMM0-7 saved where x87 depth is 0)
  tests/                                   CTest: pacer math; C3 owed rules; fraction/C4 key rules; X/Y + halt loop simulator through pause,
                                           frozen time, failed ticks, mid-pair reset (per-tick invariants); stub register/flag preservation
                                           and forced alternate-exit tests; build detection on mapped delayfix/game820 files
  docs/DESIGN.md, docs/analysis/           address map; the full analysis (synthesis, gap reports, critic, reviews, camera check, agent notes,
                                           ghq.py/gdis.py)
```

**Rules:**

- **When code is written.** All code bytes are written in DllMain; **nothing changes afterwards**. Modes only flip DLL globals, and every stub has an explicit 30-mode stock path.
- **Data.** INI-derived data is read at the first C0. No GlobalData writes, ever.
- **DllMain** must not use LoadLibrary, threads, COM, DisableThreadLibraryCalls (static CRT) or unpatch on detach. Paths come from `GetModuleFileNameW` and `%APPDATA%`.
- **Threads.** Stubs act only on the main thread (the load-screen thread draws while the main thread is inside clientUpdate).
- **Build checks:** `dumpbin` reports machine 14C, no VCRUNTIME/MSVCP/DINPUT8 dependency, and the `DirectInput8Create` export.
- **Git:**
  - `.gitignore` excludes `/aotr60/build/`;
  - commit per step;
  - commit the installed `rotwk/dinput8.dll` at milestones;
  - phase exit: `git status --porcelain -- rotwk aotr bfme2` is clean or shows only the DLL;
  - `_repo/verify-media.sh` passes;
  - the game starts both via `AotR_Launcher.exe` and directly (`game.dat -mod <game folder>\aotr -win`).
- **AotR launcher.** It reports `rotwk\dinput8.dll` as "Game files modified" on start, and accepting its PATCH deletes the DLL. The README documents this; re-run `install.ps1` afterwards.

## 3. Implementation phases (every phase speed-exact; determinism harness gates 1–4 and 6)

**0. Scaffold and passthrough.**
- Branch, .gitignore, CMake, the analysis copied into `docs/analysis`.
- `sites.json` with all sites; `verify_sites.py` passes, coverage rule included.
- Proxy, host check, config, log.
- **Audits:**
  - `RainOfFireUpdate` use in AotR INI and `.big`;
  - every consumer of the transform-cache getter `0x6765B9`;
  - LOD neutrality.
- **Check:** the game is unchanged, the keyboard works, and the log says "normal build ready" ("delayfix: known, disabled" in the offline test).

**1. Telemetry and determinism harness, no behaviour change.**

- All phase-2 sites are installed as counting pass-throughs. Their alternate exits are covered by CTest.

**Telemetry=1:**

- per-tick invariants: renders, A-renders, Δ`m_frame`, subs 1..6, calls per gated site;
- cumulative logic frames, `m_frame` and sync against QPC;
- logic-RNG call counts (`0x6D328E`, `0x6D332C`, `0x6D34A0`);
- `[0xDA1CA4]` snapshot at logic exit vs next entry, compared **against the stock baseline**, because the stock client already calls the logic RNG (e.g. the eye tower).

**Telemetry=2** (determinism runs only, never speed runs):

- a per-call logic-RNG trace (frame, sub, file `[ebp+0x10]`, line `[ebp+0x14]`, result) via prologue detours `0x6D328E` (`55 8b ec 8b 45 0c`) and `0x6D332C` (9 bytes);
- a per-logic-call record after `0x6326C6`/`0x6326F0`: sub, `GL+0x40`, seed, RNG count, `getCRC 0x625886`;
- per tick: `vt78`, `+0x23D0`, `+0x23D4`, `+0x23CC`, `TheAI+0x1C`, and the messages appended via `0x711034`;
- the engine's own client-CRC guard: `[0xDE87C5]=1`, with the report call at `0x6CF6C6` redirected to an A/B-tagged log.

Also: `tools/report.py`, the stock baselines, and the control matrix "no DLL vs `Enabled=0`" (must be identical).

**2a. Speed core and exact camera.**

- C0, halt, isTick, C3 lag and the S0 guard, C5, C7.
- The pacer with gaps, debt and Present-skip; LOD sampling on A-renders only.
- All allow-list gates and all §1.5 integrators A-only.
- Camera S1 with the B camera swap to M_k (no interpolation yet).
- The M>1 hook, mode controller and reset hook.
- The fraction and C4 stay stock, so units move in 30 Hz steps; animation is already smooth.
- **Check:**
  - per tick: 12 renders, 6 A-renders, Δ`m_frame` = 6, subs 1..6 once;
  - gated calls equal the baseline (while paused: 1 per 2 renders);
  - sync Δ = `[0xDC7A8C]`·Δ`m_frame` (± owed);
  - camera traces (`vt78`, `+0x23D0`, `+0x23D4`, `+0x23CC`, `TheAI+0x1C`, 0x452 count) identical to stock on a cinematic save;
  - SendInput key-hold scroll Δposition per second within ±1 %;
  - §4 rates;
  - determinism identical.

**2b. Smooth units.**
- S2 presentation windows: fraction (2k−1)/12 and the C4 key rule.
- **Check:** as 2a, plus a visual check of unit motion and pitch/roll.

**3. Smooth camera picture.**
- Scene-render window interpolation lerp(M_{k−1}, M_k), cut and shake rules, guards.
- **Check:** as 2a; a visual check of scroll, rotate and follow-cam smoothness; no edge pop-in during fast scroll.

**4a. Static sweep (agents)** of virtual draw targets not scanned yet:
- draw-module vt+0x2C bodies;
- render objects;
- shader `set()` and filter classes;
- gadget draws;
- the APT renderer.

New integrators go A-only. **Check:** the sweep report is committed.

**4b. Smooth effects.** Half-step conversion of the visible linear float integrators (§1.5).
- **Check:** the 30 vs 60 side-by-side; determinism unchanged.

**5. Verification and hardening.** The full §4 matrix on your display, fallback tuning, your playtest checklist.

**6. Living World strategic map at 60.** (implemented; see sites.json group note "Living World strategic map (phase 6)")
- Every LW client stepper (LW view vt6C: zoom, fades, army mover `0x6C038B`, camera fly `0x6BF78E`; translators;
  `0x645750`) stays A-only: LW logic reads its results, so nothing is half-stepped.
- The LW draw `0x49B618` is stateless per render, so B-renders redraw stock render k. A-renders present the LW camera
  (W3D camera matrix swap, no shadow refit, far plane free to follow the zoom) and the moved LW objects (render-object
  transforms, restored exactly after the scene) halfway between the last two A-renders.
- The UI-sequence runner `0x80000F` becomes A-only (GATE_CU_UISEQ); before, the LW battle exit ran clearGameData and
  the engine reset inside a B-render.
- **Check:** identical `# LW` trace lines (seeds, view state, object transforms) per LW tick at 30 and 60 for the same
  save; 6 A-renders per LW tick; lwBMismatch and lwIconRestoreBad stay 0.

**7. Optional (separate approval).**
- delayfix: 5 A/B pairs = 10 renders per 165 ms tick, isTick at s==2.
- Shell/menus.
- Replay playback at 60.

## 4. Verification: how "not twice as fast" is proven

**1. Exact per-tick invariants (Telemetry=1, every session).** Every tick in 60 mode must have:

- 12 renders, 6 A-renders, Δ`m_frame` = 6;
- GameLogic::update sub 1..6 exactly once;
- every gated subsystem called exactly as often as in the stock baseline.

Any deviation is logged as an error.

**2. Rates over 5 minutes**, from cumulative counts and QPC. Runs: idle, plus a fixed no-input battle save; fullscreen and windowed; DXVK on and off.

- **Logic:**
  - idle |logic Hz − 5.0505| < 0.1 %;
  - in the battle save, 60-mode logic Hz ≤ 5.0505 × 1.001 and ≥ stock − 0.3 %.
- **Clocks:** `m_frame` 30.30/s; sync = `[0xDC7A8C]` × `m_frame` (≈1000 ms/s).
- **Renders:** renders and Presents counted separately; debt and skip ratios logged.
- **Pause and frozen cinematics:** sync constant; tick attempts and gated calls at 30.3/s.
- **M>1 cinematic:** logic frames per wall-second equal stock.

**3. Determinism (Telemetry=2, `Fallback=0`, ≥99 % of ticks in 60 mode).** A comparison fails if either trace has 0 records.

- Control: no DLL vs `Enabled=0` on the same input, then `Enabled=0` vs 60.
- **Replay:**
  - Record a skirmish in 60 mode with real input, then play it back at stock 30. The traces must be identical.
  - Control: a stock recording played back at stock.
- **Saves:**
  - Saves made at 30 and at 60, each loaded in both modes, from the paused Esc menu (log `s` at load; it must match).
  - No input; compare from the first sub 1.
  - Include a pre-cinematic save and a Living World battle save.

**4. End-to-end.**

- On the same cinematic save, each scripted camera move finishes on the identical logic frame.
- The cinematic's wall-clock end is within ±198 ms.

**5. Your side-by-side checklist:**

- in-game timer vs stopwatch;
- unit walk and build times;
- cinematic length;
- scroll and rotate speed;
- particles, fades and B/W filters;
- Palantir income;
- trees, water.

All results are committed as `docs/verification/*.md` via `tools/report.py`.

## 5. Decisions taken by default (change any when reviewing)

1. **Pacing.** Exact stock speed, with occasional Present-skips on 60 Hz displays. `Pacing=nominal` gives an even 1000/60 cadence but makes the game 1 % slower; it is opt-in only.
2. **Scope.** Phases 2–5: campaign, LW battles, skirmish, tutorials. Phase 6: LW strategic map. Menus, replay playback, Create-a-Hero and delayfix stay at 30 unless phase 7 is approved.
3. **Input.** Polled at stock 30 Hz (stock latency); the hardware cursor moves at OS rate. The picture shows a constant 16.7 ms presentation delay for camera and units alike.
4. **Delivery.** `dinput8.dll` proxy only. Re-run `install.ps1` after an AotR update or PATCH.

## 6. Main risks and mitigations

| Risk | Mitigation |
|---|---|
| Logic reads animation state, and animation's two half-step adds can differ in float rounding from one stock add | Determinism harness on animation-heavy maps. Fallback: present animation on A without committing it (snapshot/restore). |
| Code on the allow-list touches logic | Coverage rule, RNG/seed/client-CRC guards, determinism gate per phase. |
| Unknown per-draw integrator | Phase 4a sweep, per-tick call invariants, your checklist. |
| CPU/GPU load in huge 4K battles | Deadline pacer, Present-skip, debt fallback. |
| An AotR update changes game.dat | CRC table; unknown builds stay stock 30 and are logged; `verify_sites.py` per release. |

## 7. Build identification (anything else is refused)

| Build | `.text` | `.danetta` | `.angmar` | SHA-256 |
|---|---|---|---|---|
| AotR normal (`rotwk\game.dat`) | 879A36B4 | 6DCD48D0 | 88C16692 | `cc08275d60ff8e3bfd4374c29d61304dea8336e6dd00ab8add88b1df95a705dc` |
| AotR delayfix (launcher PvP mode) | DEDA7CF9 | 63F5DCE6 | 40B06044 | `035da15c626af6cd1663d1886deea605464ec6776f2c5f1d24570c2be6d73e61` |

- CRCs are zlib CRC32 over [VA, VA+VirtualSize) of the loaded image (zero-filled beyond raw data: `.angmar` has VirtualSize 0x1000 but 0x200 raw bytes; the earlier analysis value 609926AF covered only the raw bytes), computed in DllMain before AotR writes runtime data into `.danetta`.
- Shared values: TimeDateStamp `0x460DA09E`, AddressOfEntryPoint RVA **`0x63D082`** (VA `0xA3D082`).
- Every site's original span is verified before anything is written.
