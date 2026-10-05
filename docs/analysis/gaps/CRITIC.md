# Completeness critic (gap workflow)

COMPLETENESS CRITIC REPORT: model H + B-render allow-list. Read-only; no files written.

Verified in this pass (gdis/ghq): 0x44A23E..0x44A26B, 0x44BBF0..0x44BC26, 0x44B946..0x44B9EA, 0x62E4E8 head, 0x6329B0..0x632B10, 0x6325A0..0x632708, 0x4943E1, 0x5A1D60→0x5A2060→vtable 0xBECC6C / ParticleBuffer 0x5A5E60 @0xBED2A4, 0x49AAB8/0x6C0E4D/0x6121F2/0x7FB2BB, 0x839635..0x8396A0, 0x49B8D9, 0x6A1F4D/0x6A21DF/0x6A23CD/0x6A24C3, View::xfer 0x65ED04, 0x8AF912, 0x8B5738, 0x8C3B89, 0x85F28A, 0x918BA0, 0x65D3F5, isTick callers, slot-0x188 call sites.

## 1. CONTRADICTIONS, and which side the code supports

- **K1. Paused-camera do-while: G2 is right, G6-10 is wrong.**
  - drawFrame sets bl=1 only if !0x441E23 && !0x603418 && !isGamePaused (0x44A23E..0x44A26B).
  - The loop exits on `test al,al; je` at 0x44BBFA, and it needs isGamePaused at 0x44BC19. Both cannot hold, so the loop never repeats.
  - The 29 ms wait at 0x44B9B8 only bites when TV vtDC>1. Otherwise prev=now-30 (`add esi,-0x1e` at 0x44B98C).
  - So C6 matters only while the time multiplier is >1 (see risk 7). (high)
- **K2. Failed tick attempts: G4/G5 are right, synthesis 1.3 is wrong.**
  - Frozen sub-1 takes `C8=0; goto LAB_0062ee54` before FUN_005FF9D3/FUN_006F2364. Only 0x604152 (debug-DLL bridge) and TheNetwork vtAC run.
  - However, vt98(1) calls [0xDE4950]->vt28 (LivingWorldLogic::update) after every non-paused attempt (0x632A85..0x632A92). Attempts must therefore stay at 30/s. (high)
- **K3. Stock vsync: G6 (-1%) over G9 (no change).**
  - G9 says stock is snapped to 30.0 FPS. G6 says stock at 30.3 FPS never fills the FIFO queue, so it is CPU-paced at 30.30.
  - On a 60 Hz FIFO display, 60 mode is held to 60.00 FPS, which is 1% slow. That violates "speed unchanged". (medium, needs M8)
- **K4. C3 sync clock while paused or frozen: bug confirmed; lag phase recommended.**
  - The tick path calls vt98 directly at 0x6326C6 and bypasses SubStub 0x6326EB.
  - With C1, the pause/freeze cycle is s=12 (B, arms), then s=13 (fails), then 11. The armed flag adds h on every other render. Sync runs at about 50% while paused or frozen; G8's "25%" is wrong, G2's unbounded drift is right.
  - Recommendation: lag phase (the G4-21 option). The A-render adds h; the next B-render adds 33-h, but only if the previous render was an A-render that advanced.
  - With lag, the B-render reproduces stock render k exactly: m_frame k, frac 2k/12 = k/6, sync S_k, camera cur'=2k. Logic runs right after the B-render in the same iteration, so it sees stock sync.
  - With lead, animation runs about half a frame ahead of interpolated positions, and logic sees S+h. (high)
- **K5. Debug vt94 (0x43B050): G4-08 "halted path only" is wrong.** It runs on every stepper frame (0x6326F6..0x6326FE) as well as on the halted path (0x6325E5). Under route 2 it stays one call per iteration. Harmless. (high)
- **K6. Stepper: G8 route 2 is better than synthesis C1.**
  - A-frames run the unmodified stepper. B-frames go through the halted path at 0x6325DE. isTick gets a 5-byte detour at 0x63252F. C0 writes GE+0x3C.
  - Removes the C1a/c/e/f patches and the C1e frac 11/12 stall bug, and works for delayfix with s_tick=2.
  - Precondition holds: all 10 isTick callers are render-side (0x461EBD, 0x48E2D3, 0x4B51F4, 0x4B6BE6, 0x4ED6AB, 0x64871C, 0x6487F3, 0x6759BB, 0x83BA12, 0x8A036E). (high)
- **K7. Turning 60 mode off: G2/G6 over G5.**
  - G5 wants off "at any frame with s conversion" and whenever TV+0x23D4>1. G2/G6 say never switch while a camera move is in flight.
  - The lag model doubles N and hold via IDIVs (0x485DD4 and others). A switch mid-move leaves N'=2N stepping at 30 Hz, so freeze or move lasts 2x. Logic reads that at 0x62E528/0x62E53A, so logic diverges.
  - The multiplier is itself set by a camera path (0x48A69A), so it is always mid-move. Keep 60 on and keep C6 (0x44B9C3 1d→0e, or an exact 14/15 stub).
  - Otherwise rescale cur/N/hold on the switch. (high)
- **K8. Input camera: the G1, G3 and G4-16 schemes are mutually exclusive.**
  - G1: LookAt, the IGUI camera block and the LW translator run every render with halved constants. G3: tick-only, with vtable proxies and a stash. G4-16: InGameUI skipped entirely, which gives 30 Hz key-camera judder.
  - Combining G1 and G3 halves twice.
  - G1 already halves twice on one path (verified). LW key rotate pushes GD+0xC2C*±15 (0x839641/0x839674, ±15 at 0xC53AFC/0xBDC6CC) into LW vt2C, which multiplies rot by [0xD99B64] at 0x49B8D9. G1 halves both, so LW key rotate runs at 0.5x.
  - G1's "0x49B7AD 0.5→0.25" must redirect the operand, because 0xBD869C is shared. G1 also mutates GlobalData+0xC2C, which G2's rule forbids. (high)
- **K9. Living World: requirement wins over G5's v1 exclusion, and G4's logic-RNG finding confirms the stakes.**
  - The requirement puts Living World in scope; G5 v1 excludes mode 8 / GL+0x125.
  - Verified render-path logic-RNG chain: 0x6484D9 → vt6C 0x49AAB8 → call 0x6C0E4D at 0x49AAD4 → 0x6C0E7A [0xDE3C08]->vt28 → call 0x7FB3F0 at 0x6121F2 (eye tower) → call 0x6D328E at 0x7FB2BB.
  - This needs a g_uiTick gate. Synthesis row 79's gate on 0x6BE51A is wrong: that is LW logic at sub 1. (high)
- **K10. Corrections the synthesis needs:**
  - GL+0x125 means "LW strategic map shown" (G5), not "loading".
  - 0x635D11 never resets s (G5 and core agree; synthesis 2.3 is wrong).
  - TheInGameUI is 0xDE4830; TheShell is 0xDE7890 (G1/G4; consistent with the vt10C deselect at 0x6A24B0).
- **K11. Minor B-policy splits, benign either way:** 0x62B385 (G1 run / G4 skip), GC vt90 0x645F65 (G1 run / G4 skip), gesture state machine 0x50EB3C (G1 run / G3 skip → skip, the safe default).
- **K12. AotR Palantir counter: G7 is right.** Row 86's "harmless scan" is wrong: [0xED0000] is the RPM sampler (cmp 0x1E at 0xED0816). Gate the callee at 0x5039C1; the caller gate at 0x6A23D7 misses the LW path at 0x61226F.
- **K13. Saves: verified safe.** View::xfer 0x65ED04 (W3DView vtable 0xBDD490+0xC) writes only base View fields up to +0xB0. W3DView move counters (+0x1AC.., +0x20C.., +0x23D0/+0x23D4) are not saved, so a doubled N can never reach a save file. (high)

## 2. Paths still unexamined that could run 2x fast or touch logic

- **U1. The draw is default-allow.**
  - G2 scanned 0x440000-0x516000 plus the direct-call closure. Not scanned: virtual targets, i.e. draw-module vt+0x2C bodies, render-object Render/On_Frame_Update, shader set(), screen-filter classes, gadget draw callbacks under WindowManager draw 0x48F871, and APT renderer internals of 0x46203E.
  - Known integrators already lie outside the scanned range (0xB53A2B, 0x732926, 0x6A536C, 0x69DEA9, 0x65CF05, 0x6603BA), so more are likely.
- **U2. Display vt+0x188 = 0x444CD4** (LW manager update plus particle tail at 0x444CF2). The only pointer is at 0xBD9DB0. No `[reg+0x188]` call site loads TheDisplay (grep). Caller unknown; it can run the eye-tower RNG and Palantir.
- **U3. Draws outside clientUpdate, where C0 state is stale:**
  - movie loop 0x65D3F5 (vt18C 0x4475E4, fade 0x65C97A +0.05/call, keyboard 0x65D581);
  - LOD benchmark 0x4436DD;
  - load-screen thread 0x65CE28;
  - intro branch 0x6488BD.
- **U4. LW strategic map per-render work:**
  - 0x49AAB8 tail: zoom velocity 0x49AB21/0x49AB3E, fade, angle inertia;
  - moveTo 0x6BF78E/0x6BFB88;
  - state machines 0x645750;
  - LW draw vt20 0x49B618;
  - APT embedded 'LivingWorldMap'/'BinkGameWindow' vt14 via 0x814218;
  - where army movement lives (LW logic or the manager).
- **U5. Logic reading drawable animation state** (anim frame or anim-complete) is not proven absent. The float anim frame accumulates h+(33-h) per pair, which can differ in ULPs from a single 33 step.
- **U6. Logic reading camera pose.**
  - RainOfFireUpdate 0x8AF912 calls [0xDE447C]->vt118 (= 0x48B465, copies view+0xC..+0x14) inside its logic-RNG block (0x6D332C).
  - AotR loose INI has it only commented out (system.ini:8776); .big contents not checked.
  - The other candidates (0x8B5738 EmotionTracker, 0x8C3B89, 0x85F28A) call vt+0x30 = 0x918BA0 `ret 0xc`, a no-op.
  - Still open: camera-message handling in 0x779A3D/0x77CC8A.
- **U7.** Linearity of water vt4(dt) (0x500137 lists); shore waves 0x4FDB4A; fly transition 0x502458 (used in AotR?); subtitle scroller units 0x6603BA.
- **U8.** Per-iteration work doubles in both stepper variants: [0xDEF548]->vt28 (0xA37E50), 0x604189/0x603452, serviceWindowsOS, Debug vt94. Believed harmless, not measured.
- **U9.** Client-to-logic writes via input/UI (ClickReactionBehavior 0x85C75E feed) are still open (G4).

## 3. Allow-list verdict and contents

**Verdict:** the principle is sound at the call sites in clientUpdate and GameClient::update. A skipped system sees exactly the stock call sequence: one call per pair, isTick on 1 of 6, about 33 ms apart. It needs these refinements:
- **a. One gate signal.** g_uiTick = isARender ? 1 : (prevWasA ? 0 : !prevTick). This keeps strict alternation through pause, freeze and LW, and gives exactly one gated render between consecutive tick attempts, so eye-tower and LW-logic RNG interleave as in stock. Never mix in g_isARender (it would freeze recoil and similar while paused).
- **b. Never skip a consumer of a global that allowed renders advance.**
  - Sync−PrevSync has one such consumer outside the draw: snow 0x4943E1 (verified).
  - 0x5A1D60 is reached from ParticleBuffer On_Frame_Update inside the scene render, not from the C7-gated 0x5F5123.
  - Run snow every render and gate only 0x49448D and 0x494494.
- **c. Rules for allowed code:** no m_frame modulo/equality triggers; no logic RNG; no CRC-covered writes; no extra network-range messages (each one creates an AIGroup, TheAI+0x1C++).
- **d. Couplings:** camera stepping on B requires propagate 0x6324A1 on B. Input skipped on B requires the camera to replay stale input as half-steps (or use split proxies).
- **e. Inside the draw the principle inverts.** The draw is the allowed work, so a per-instruction deny-list of integrators is mandatory there.

**Run every render:**
- clientUpdate: 0x62B385 (×2); m_frame++ at 0x632423..0x632440; propagate 0x7128C3; 0x80000F (but gate its keyboard drain 0x6324B4..0x6324BC).
- GameClient::update: LookAt 0x83B471 and LW translator 0x8392A7 (one input scheme only, see K8); snow (inner gates); 0x532D6F; freeze calculation; drawable block (self-gated at 0x648705); DRAW vt30 0x44B788; 0x645DAD; GC vt90; the IGUI camera block 0x6A21DF..0x6A23CD (or a POST-window split). TerrainVisual 0x648835 only once the water UV operands are halved; skip it in phase 1.

**Fixes inside the draw:**
- C3 (lag), C4, C5, C7 on g_uiTick; gate the tree call 0x449D55.
- Camera lag model:
  - N'=2N via the IDIVs, with N clamped ≥1 first;
  - hold ×2;
  - instant moves step twice (0x488854, 0x488911, 0x488995, 0x488A19);
  - IMUL at 0x485F62/0x486279;
  - 16/17 dt at 0x48A9F2/0x48AAE5/0x48AB06.
- Camera integrators: follow 0x48BEA2/0x48BFE8; tether 0x48BFCB/0x48BFA3; settle 0x48C36A/0x48C3B0; zone zoom 0x48C553 and related, including compares 0x48C56A/0x48C5F0; shake 0x48C10A; shaker 0x5033E5; +0x138 decay 0x48A256; rolling average 0x486B3D.
- View-filter INC sites: 0x4FB599, 0x4FB5DD, 0x4FBC7C, 0x4FBCC0, 0x4F5C3A, 0x4F5C8D, 0x4FCBAC, 0x4FCBF0.
- UV 0x4F678D/0x4F7EEE plus row 22; instance fade 0x4D1436/0x4D1463; flash 0x44A033/0x44A12E; subtitles 0x44A203; UI particles 0x6A53FB; rise 0x69DF0E; rows 20, 23, 25, 29-31, 35, 36.

**Skip on B (g_uiTick):**
- APT 0x632449, Radar 0x632486, keyboard drain 0x6324BA, Audio 0x6324F9;
- gesture state machine 0x6484C8;
- LW view vt6C 0x6484D7 (phase 1; phase 2 gates 0x49AAD4 only), 0x645750;
- Cloud, Fire, Anim2D; Keyboard 0x6485CC; ScoredKillEva; Eva; Mouse 0x6485FC;
- popups 0x648616, WindowManager 0x64863A, Display::update 0x64883E, DisplayStringManager 0x648872, Shell 0x648891;
- the rest of InGameUI, plus a Palantir callee gate at 0x5039C1 and gating of 0x444CD4.

**Made unnecessary by this list:** rows 61-63, 67, 68, 70, 76, 79, 80, 84-86.

## 4. Top 10 risks: "never 2x, logic identical, saves compatible"

1. **Logic call pattern and build variant.** Use route 2. Refuse unknown builds by CRC (.text/.danetta/.angmar) plus the discriminator bytes 0x632535/0x632A9B/0xECA400/0xED0816; delayfix stays at 30 in v1.
   - Verify (M3): hook GameLogic vt34 and 0x6BE50E. Expect exactly 1..6 per tick, and 30.3 attempts/s while paused, frozen, and on the LW map.
2. **Hidden logic RNG or CRC writes from B-run code.** The eye tower is verified; virtual paths are not proven.
   - Mitigate: gate 0x49AAD4/0x6484D7 and 0x444CD4. In release builds, snapshot [0xDA1CA4] around each B-render, and hook the prologues of 0x6D328E/0x6D332C/0x6D34A0 while g_inB to log and restore.
   - Verify (M2): diff -deepCRC logs (hook 0x6CF86E) between 30 and 60, and call getCRC 0x625886 before and after each B-render.
3. **Camera-to-logic coupling.** Covers freeze vtD8/vt78, the CAMERA_MOVEMENT_FINISHED condition at 0x7EB965, 0x452 messages (+0x23CC, AIGroup ids), and RainOfFire reading camera pose.
   - Mitigate: lag model with exact integer doubling; propagate on B; no drops and no mode switches while vtD8 || !vt78 || +0x23C8/+0x2354 ≠ 0 || a filter fade is active.
   - Verify: replay a scripted cinematic from one save in 30 and 60, logging GameLogic+0x40, vt78, +0x23D0, +0x23CC and TheAI+0x1C per tick.
4. **Sync phase, and logic reading animation state.** Adopt C3-lag (K4).
   - Verify (M1): sync advances 990 ms/s while running and 0 while paused or frozen. Run the CRC harness on animation-heavy maps (catches U5).
5. **Unknown per-draw integrators (U1, U3).**
   - Mitigate: extend the const-step/counter scan to the virtual draw targets. Stubs fall back to stock unless inside a C0-bracketed clientUpdate on the main thread.
   - Verify (M5): per-function call-rate profiler across the draw closure in 30 vs 60; any function whose global/object stores double is suspect. Then M6 capture diffs.
6. **Pacing.** Up to -1% on 60 Hz FIFO, plus overrun slip.
   - Mitigate: QPC deadline pacer with T=P/2 (33.000 ms per pair) and bounded lateness.
   - Absorb the FIFO deficit by skipping Present (hook 0x522644, return S_OK) rather than dropping B-render state. That keeps the lag-model camera and split integrators exact without the n-aware rewrite G6 needs. (suggestion, medium)
   - Add a hard fallback to 30 mode at safe boundaries.
   - Verify: logic Hz = 5.050 ±0.1% over 10 minutes on 60/120/144 Hz, fullscreen and windowed (M1/M8).
7. **Mode switching with doubled state in flight (K7).**
   - Mitigate: switch on only at a pair boundary. Switch off only via the reset hook 0x44181A or when nothing is in flight. Keep 60 on through multiplier >1 with C6. Use hysteresis.
   - Verify (M9): SET_VISUAL_SPEED_MULTIPLIER cinematics, Esc during a camera move, save/load mid-move.
8. **Input applied twice or not at all (K8).** Pick one scheme and drop G1's GD+0xC2C mutation.
   - Verify (M6): scroll px/s, key rotate deg/s on both the tactical view and the LW map, zoom per key hold, edge/MMB rotate, auto-repeat 367 ms then 30/s, held click at 133 ms, all equal to stock.
9. **Half-speed and double-fire via the allow-list itself.** A skipped Sync-delta consumer runs at half speed (snow); m_frame-modulo triggers in allowed code fire twice; mixing g_isARender with g_uiTick.
   - Mitigate: rules b and c, one gate source.
   - Verify: snow time / real time = 1.0; M5 counts per m_frame for every allowed function that tests getFrame().
10. **Living World map is in scope but under-audited (K9, U4, K12).**
    - Mitigate: phase 1 gates all of vt6C; single-halving LW camera fixes; Palantir callee gate 0x5039C1.
    - Verify on the LW map: 0x6121C5, 0x7FB3F0, 0x6D577C and 0x49AAB8 each run 30.3/s; logic-RNG calls per LW tick are identical in 30 and 60; the AotR RPM matches stock.

Saves (requirement 3): no blocker found. m_frame and fades stay in stock units, View::xfer excludes the doubled counters (K13), and no DLL state is serialised. Residual: never mutate GlobalData or Options-backed values (prefer operand redirects).
