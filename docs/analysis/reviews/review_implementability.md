# review_implementability

**Review of the plan (`docs/PLAN.md`): implementability, phasing and verification**

1. **The `-deepCRC` determinism test can't run as written, and would pass with nothing compared.**
   - Why: the retail command-line table at 0xC35DA8..0xC35E27 has only 16 switches, none of them CRC. The handlers 0x7BA60A (liteCRC), 0x7BA649 (deepCRC) and 0x7BA6C9 (verifyClientCRC) have no pointer or call references. So `-deepCRC` is silently ignored.
   - Even when it is enabled, the trace never reaches a file. The logic-RNG functions log only if the collector `[0xDE4A30]` exists, and that collector is created only at engine init (0x63AFC1). It is hashed and cleared inside getCRC (0x6CF952→0x6CF860) and cleared after every GameLogic::update (0x62E880). GameLogic also resets `[0xDE87C7]` to 0 at 0x62E7F6.
   - Fix: replace "`-deepCRC`" with a DLL trace at `Telemetry=2`.
     - Prologue detours on 0x6D328E (`55 8b ec 8b 45 0c`, 6 bytes) and 0x6D332C (`55 8b ec 51 f3 0f 10 45 0c`, 9 bytes). Log logic frame (GL+0x40), sub, file `[ebp+0x10]`, line `[ebp+0x14]` and the result. For 0x6D332C, check that its arguments use the same layout.
     - Log once per logic call, after the calls at 0x6326C6 and 0x6326F0 (`ff 90 98 00 00 00` / `ff 92 98 00 00 00`). Route 2 leaves both sites stock in either mode. Record sub, GL+0x40, the seed `[0xDA1CA4]`, the RNG call count and getCRC.
     - Every diff must print how many lines it compared, and fail when that number is 0.

2. **The B-render guard (getCRC on every B-render) changes the thing it measures and costs too much.**
   - Why: getCRC (0x625886) calls 0x440809 (`_fpreset`/`_controlfp`), xfers every object, and clears the trace collector (0x6CF952). In 60 mode only, that changes what the next CRC hashes, so it can produce false mismatches. In big battles the cost would distort the pacing measurements and the fallback.
   - The engine already has this guard. A CRCVerification object wraps radar → GameClient::update → propagate → audio: constructor 0x6CF64E called at 0x632478, destructor 0x6CF681 called at 0x63251C. It is switched on by byte `[0xDE87C5]`, but it only reports in multiplayer (call to 0x441B7C at 0x6CF6C6).
   - Fix:
     - `Telemetry=1`: cheap check only. Compare the seed and flag RNG-prologue hits while `g_inB`, logging the caller.
     - `Telemetry=2`: set `[0xDE87C5]=1` and redirect `e8 b1 24 d7 ff` at 0x6CF6C6 to a stub. The stub logs the start CRC `[edi]` against the end CRC `esi`, tagged A or B, and returns 0.
     - Record a stock baseline first: the LW eye tower changes the CRC on stock renders. Require 60 == stock, and zero changes on B-renders.
     - Never run `Telemetry=2` while measuring speed.

3. **Determinism checks start too late and have no controls.**
   - Phases 2–4 ship with only the B-guard as a logic check. Build the harness in phase 1 and make it the exit gate of phases 1, 2, 3, 4 and 6.
   - Use this control matrix on the same input:
     - (a) no DLL vs (b) `Enabled=0` must match first. This also proves the loaded save itself replays deterministically. If two stock loads differ, the save test is invalid.
     - Then (b) vs (c) 60 mode must match.
   - Determinism runs set `Fallback=0` and log the share of ticks spent in 60 mode (require ≥99%). Otherwise the `Telemetry=2` overhead trips the fallback and the run compares 30 against 30.
   - Reword the save test: load the same save S in 30 and in 60 mode (one S made at 60, one at 30), same resolution and windowed state, no input, diff the per-tick trace.
   - Add these saves: one just before a scripted cinematic (logic reads +0x23D0, +0x23D4 and isCameraMovementFinished at 0x62E53A) and one LW battle. Skirmish replays never exercise the camera-to-logic coupling.

4. **The replay test needs a better primary design.**
   - `ForceReplay60` must bypass two conditions: Recorder+0x1C==1 and GL+0x110==3 (G5: playback starts through MSG_NEW_GAME with mode 3, which is outside {0,2,6}).
   - Stronger test that needs no override:
     - Record a skirmish in 60 mode with real input while logging the trace.
     - Play the `.rep` back at stock 30 with the trace on. The two traces must be identical.
     - Control: a stock-recorded replay played back at stock.
   - Also check whether solo replays carry CRC messages (block 0x62E765..0x62E862 sends message 0x44A with getCRC). If they do, the engine's own replay check is a free extra test.

5. **Several telemetry thresholds are wrong or can't be measured as written.**
   - Sync rate: P=`_ftol2(1000/30)`=33 ms per frame, so 30.303 × 33 = **1000 ms/s, not 990**. Replace the rate with the exact identity Δsync = 33·Δm_frame (± owed ≤ 17), and 0 while paused.
   - "Δ<0.3% vs stock" in a battle is ill-posed. Stock never catches up (`last=now` at 0x63A1F8), while the deadline pacer does. Require:
     - idle: |logic Hz − 5.0505| < 0.1%;
     - battle: 60 ≤ 5.0505 × 1.001 and ≥ stock − 0.3%.
     - Compute both from cumulative counts and QPC over the whole window. A 10 s meter has ±2% quantisation.
   - "Every gated subsystem 30.3/s" is false for conditional sites (keyboard drain 0x6324BA runs only on focus regain; LW, Shell and Palantir only when present). Use exact per-tick checks instead:
     - per tick: renders = 12, A-renders = 6, Δm_frame = 6, subs 1..6 once each;
     - each gated call = 6 per tick and equal to the baseline;
     - while paused: one gated call per two renders.
   - Count Presents separately from renders.

6. **Nothing yet proves "not 2x fast" to you in one place.**
   - Add `tools/report.py`: turns the 30 and 60 CSVs into a PASS/FAIL table, committed under `docs/verification/`.
   - Add an end-to-end test: same cinematic save, no input. The logic frame at which each scripted move finishes (vt78 flips) must be identical, and the cinematic's wall-clock end within ±198 ms.
   - Add a CTest that simulates the C0 / halt / stock-stepper loop through pause, frozen time and failed ticks, and asserts the per-tick invariants above.

7. **The site lists are too vague to implement; only 5 of about 20 allow-list sites match `8b 01 ff 50 28`** (0x632449, 0x6324BA, 0x64883E, 0x648872, 0x648891).
   - Wrong or unusual sites:
     - 0x648835 is the 3-byte `ff 50 28` in the middle of the pattern, which starts at 0x648833. A call written at 0x648835 corrupts `mov ecx,[0xDE4418]` at 0x648838.
     - 0x632486 is `8b 01 33 ff 89 7d fc ff 50 28`. EDI=0 and the exception state at `[ebp-4]` must still be set.
     - 0x6324F9 `8b 01 89 35 30 43 de 00 ff 50 28` must keep its store to 0xDE4330.
     - 0x6484C8, 0x648616, 0x49AAD4, 0x449D55 and 0x4C78CE are direct `e8` calls. 0x5039C1 is an `e9` thunk. 0x444CD4 is a function entry. 0x6484D7 is a vt6C call.
   - Missing patch contracts:
     - C0 at 0x6325CF (`ff 90 9c 00 00 00`) is never named in the plan.
     - Halt at 0x6325D5 is 7 bytes. The stub must return EAX=`[0xDE4388]` and the ZF the `je` at 0x6325DC needs.
     - Pacer at 0x63A196 (`ff 15 20 09 bd 00`) exits to 0x63A1F5 with EDI=`timeGetTime()`, EBX=0 and the x87 stack empty.
     - Present is 15 bytes at 0x522644, continuing at 0x522653 with EAX=S_OK.
     - The IDIVs `f7 3d 8c 7a dc 00`: the game clamps after the divide (0x485DE0; the hold at 0x485E8C clamps to 0), so the stub must clamp before it doubles.
   - Fix: make `tools/sites.json` the single source for both a generated `sites.gen.h` and the DESIGN.md table. Columns: id, phase, address, original span bytes, length (must end on an instruction boundary), kind, replacement bytes, stub, what is live afterwards (registers, EFLAGS, x87 depth, XMM), 30/60 values, evidence id.
   - `verify_sites.py` should also:
     - check that no branch target falls inside a span;
     - check rotwk\game.dat, aotr\zGameDats\delayfix.dat and rotwk\game820.dat;
     - list every call in 0x632409, 0x64849E and InGameUI::update and fail on any call not marked run or gate. Call-site patching lets unknown calls run by default, so "unknown systems can't run 2x" only holds if every call is classified.

8. **The patcher is missing primitives.**
   - 0x48C36A and 0x48C3B0 are `d8 88 b0 0a 00 00` (fmul `[eax+0xAB0]`). Redirecting them needs a ModRM rewrite to `d8 0d <&var>` (same 6 bytes), not a disp32 write.
   - The vtable slots need a pointer-slot primitive that checks the old pointer first: 0xBDD4EC→0x48C774, 0xBDD58C→0x48CA7F, 0xBDD5C0→0x48CBFE, 0xBDE944→0x49B799. Each pointer occurs only once in the image.
   - The 30-mode value of every redirected operand must be copied from the original at install time, so the game is bit-identical when 60 mode is off.
   - Values derived from the INI (GD+0xAB0, GD+0xDE0) must be computed after the INI loads (first C0 or the reset hook).
   - State the rule explicitly: no code bytes are written after DllMain.

9. **The stub register contract omits XMM and x87.**
   - The game uses SSE (0x63256F, 0x6D332C, camera code), and st0 is live at 0x48C36A.
   - Fix: use two classes of stub.
     - **Call-boundary stubs** (replace a call or a function entry) may call C++ after saving whatever the site table marks live.
     - **Mid-stream stubs** must be pure assembly that touches only DLL globals. If one must call C++, it saves flags, general registers and XMM0–7, and may do so only where the table says x87 depth is 0.
   - Add a host-side harness that checks every stub preserves all registers and flags.

10. **Proxy and DllMain details need pinning down.**
    - DLL loading:
      - Load the real `dinput8.dll` lazily on the first DirectInput8Create call (from 0x4985FF), never in DllMain.
      - DllMain must not load shell32 or other libraries, start threads or use COM. Get the path with `GetEnvironmentVariableW(L"APPDATA")` and `GetModuleFileNameW(hinstDLL)`, never from the working directory.
      - Don't call DisableThreadLibraryCalls with the static CRT (/MT).
      - Never restore patches in DLL_PROCESS_DETACH; worker threads are still running.
      - Write `.data` (such as the test flag `[0xDE87C5]`) only after confirming no static initializer writes it.
    - Build checks:
      - `dumpbin /dependents` must show no VCRUNTIME, MSVCP or DINPUT8, and `/headers` must show machine 14C.
      - Ninja needs the x86 environment: `build.ps1` must run `Launch-VsDevShell.ps1 -Arch x86 -HostArch amd64`, or use the "Visual Studio 18 2026" generator with `-A Win32`.
    - Build identification:
      - Define the CRC as zlib CRC32 over [VA, VA+VirtualSize) at DllMain time. This reproduces §6: `.danetta` gives 6DCD48D0, while its raw 0x8200 bytes give C0D4D052.
      - That CRC is only valid before game code runs: AotR stores runtime data in `.danetta` (`[0xED0000]`).
      - Store the full SHA-256 values, not the truncated ones.

11. **Phase 2 is too big, and its interim camera gate has no sites.**
    - The updateCameraMovements call at 0x48C09A and the inline eases in 0x48BCF2 must be gated, while the drawable pass in the same function (push 0x485329 at 0x48C738) must still run on B-renders. Otherwise units disappear on B-renders.
    - Fix:
      - Split phase 2. 2a: the speed core, with the fraction left at stock and an exact-invariant + determinism exit. 2b: fraction, C4, C5, C7 and the Present-skip.
      - Phase 1 installs every phase-2 site as a counting pass-through, so the byte patches are proven before any behaviour changes.
      - Make the phase-4 sweep a separate analysis step, 4a.
    - Phases 3, 4 and 6 have no **Check:** line. For phase 3: identical per-tick traces of the camera-finished flag, +0x23D0 and +0x23D4 in 30 vs 60; and a SendInput key-hold scroll, with Δposition equal within ±1%.

12. **The plan refers to things it never defines.**
    - "M9", the M-list, C0–C7, "route 2" and "lag" are defined only in the synthesis or the critic. Inline them in the plan.

13. **Git and install hygiene.**
    - The repo is on `main` and holds the game install; nothing ignores build output.
    - Phase 0:
      - Create a branch.
      - Ignore `/aotr60/build/`.
      - Decide whether `/rotwk/dinput8.dll` is ignored or committed.
      - `install.ps1` refuses a foreign `dinput8.dll` and keeps any backup outside `rotwk\`, because the launcher deletes unknown files there.
      - Copy ghq.py, gdis.py, the 8 agent notes and the extracted journal into `docs/analysis`, since the scratchpad is temporary.
    - Every phase checks:
      - `git status --porcelain -- rotwk aotr bfme2` shows only `?? rotwk/dinput8.dll`, and `_repo/verify-media.sh` passes. Together these prove nothing else in the game folder changed.
      - The game starts both through AotR_Launcher.exe and directly via `game.dat -mod <game folder>\aotr -win`.
    - Test delayfix and game820 detection offline (CTest maps those files) rather than swapping `rotwk\game.dat`.

**Right as written; don't change:**
- Route 2: stock stepper bytes untouched, halt at 0x6325D5, isTick detour.
- m_frame at 30 Hz. C4 leaves the +0x378 expiry on m_frame: Drawable::xfer saves it (0x67AAF1 `lea ecx,[edi+0x318]`, edi = this+0x60), while the cache keys at +0x204 and +0x244 are not saved.
- The pacer trampoline at 0x63A196 (x87 is empty there), integer QPC maths, and keeping `[0xDE4318]` and the stats updated.
- Present-skip instead of dropping B-state.
- Turning 60 mode off only in the reset hook or at a quiet boundary; staying on through multiplier > 1.
- `g_uiTick` as the single gate source; snow running every render.
- All-or-nothing verification with the build table: the CRCs match, and delayfix and unknown builds are refused.
- No GlobalData writes; config and logs in `%APPDATA%\Age of the Ring\aotr60\`.
