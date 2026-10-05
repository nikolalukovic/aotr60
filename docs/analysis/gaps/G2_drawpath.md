# G2_drawpath

## Summary

G2: draw path (W3DDisplay::draw 0x44B788, drawFrame 0x449CF8, W3DView::update 0x48BCF2 and its camera calls, the drawable pass, scene render and UI draw). I checked everything reachable statically from these roots: 1489 functions plus the virtual draw roots. I also ran a const-step and counter scan over the W3D client code (0x440000-0x516000), then confirmed each hit by hand. All patch-site bytes are identical in game.dat, delayfix.dat and game820.dat.

New per-render integrators that no synthesis row covers:
(1) Three view-filter fade frame counters that increment on every render: BW 0xDD1BEC, mask 0xDD1A40 and the monochrome-zoom fallback 0xDD1BFC. They are set by W3DView vt+0xC0 0x4857FB.
(2) The rotate-toward height-offset decay view+0x138 inside buildCameraTransform (0x48A256..0x48A2D3).
(3) The waypoint-path angle smoothing by RollingAverage (1/R per call, 0x486B3D).
(4) An instant camera move (ms<=1) steps at setup (0x488854/0x488911/0x488995/0x488A19). Under N'=2N it would not finish in the same logic frame, and that is visible to logic.
(5) Two more 0.05 per-call UV scrolls in the hilight/vapor overlays (0x4F678D, 0x4F7EEE), an extension of row 22.
(6) A buffered-model instance fade of +-1/12 per draw (0x4D1417).
(7) The display overlay fade 0x65CF05 and the subtitle line scroller 0x6603BA, both called from drawFrame.

Camera integrators, each with an exact patch site: follow ramp/lerp, tether, eight IDIV setups + hold, IMUL back-conversion, ms paths, rolling average, mode 3/4, legacy shake, CameraShaker, height settle, zone zoom, +0x138 decay, fly transition and Living World.

CameraShaker cadence is resolved: setCameraTransform 0x48B7B1 calls buildCameraTransform twice (when view+0x241C==0), so Timestep(1/30) runs at least twice per camera rebuild. The 1/60 operand still gives exact parity per pair.

Two corrections to the synthesis:
(a) C3 as specified advances the W3D sync on every B-render during script-frozen time. Stock keeps sync frozen then. Drawables would animate at half speed in frozen-time cinematics and the sync accumulator would drift without bound.
(b) The 'paused-camera' do-while in W3DDisplay::draw can never repeat in retail. drawFrame returns 0 whenever GameLogic+0x124 is set, and the loop needs that flag set. C6 matters only for the camera time-multiplier wait.

Recommended camera model: a 'lag' split-step. N'=2N, cur starts at 0 as in stock, instant moves step twice, and ms paths get 16/17 per render. With this, the camera state at every A-frame (logic sample point) is bit-equal to stock for counters, and equal up to float ULPs for ms paths. That includes isCameraMovementFinished, isTimeFrozen (+0x23D0) and the time multiplier (+0x23D4), which logic reads at 0x62E53A, at 0x7EB96D (script condition case 9) and at 0x77CC8A. The display trails the stock time base by half a frame.

## Design notes

Evaluation of the B-render allow-list principle for the draw path:

1. The draw cannot be gated wholesale; it is itself the B-render work. Inside the draw, 'safe default' must be applied per integrator instruction, not per call.
   - Several per-render consumers use the global W3D frame delta (SyncTime - PrevSyncTime). Skipping one of them on B loses the B half of the time instead of preserving cadence. Readers of PrevSync: snow 0x4943E1, particle emitter 0x5A1D60, model animation first frame 0x4BF5FF, the drawable gate 0x48C701 and 3D-in-UI 0x4A89BD.
   - Rule: never skip-on-B a function that consumes Sync-PrevSync. Gate only the counter/integrator instructions inside it.
   - Example outside G2: snow 0x4943E1 (GameClient::update) also runs per-call weather countdowns at 0x49408D ([+0x58]) and 0x492C66 ([+0x54]). Gate those, never the call.
   - The allow-list approach stays sound at the GameClient::update subsystem level (G1).

2. Gate signal. Every skip_on_B / split_step above uses g_uiTick and must alternate in all states. Rule: tick = isARender ? 1 : (prevWasARender ? 0 : !prevTick).
   - In script-frozen time every render has GC+0xC8==0 (C1e alternates s=12/13 with failed ticks). Stock steps these render-cadence integrators every render (30/s), and the toggle reproduces that.
   - Camera path dt (16/17) uses its own per-render toggle, independent of A/B. Any two consecutive renders sum to 33.

3. C3 needs the correction in G2-D03: a B advance only after an advancing A, and pending==0. Otherwise W3D time runs during frozen cinematics.

4. Camera model: the 'lag' split-step (N'=2N, hold x2, cur starts at 0, instant moves step twice, 16/17 ms path dt).
   - It is the only cheap scheme that keeps the camera state bit-equal to stock at A-frames, where logic reads isCameraMovementFinished / +0x23D0 / +0x23D4.
   - The orchestrator asked for exact equivalence at A-render sample points. That is the 'lead' variant (cur'=1 at setup). It makes the A-render presentation time-exact, but logic then sees a half-step-ahead pose and time multiplier. Recommend lag unless M2 shows logic never reads camera pose.
   - Mode 3/4 stay M-cadence (exact, 30 Hz steps), with optional smoothing.
   - Exponential integrators (follow, tether, settle, zone zoom, rolling average) are made exact per pair for static targets via f' = 1-sqrt(1-f), with ramps incremented only on tick renders. Prefer operand redirects to DLL variables of the same instruction length: settle 0x48C36A/0x48C3B0 'd8 0d', zone zoom, shaker 0x5033E5, path dt pushes, follow ramp 0x48BEA2. Mode switching then becomes a pure data write: no code-byte toggling and no GlobalData mutation.

5. Mode-switch constraint. IDIV-doubled steppers, doubled holds and in-flight split paths assume one mode for their whole lifetime. Add to the g_m60 controller (synthesis 2.3): switch only when TacticalView vt+0x78 reports finished, +0x23C8/+0x2354 == 0, no view-filter fade is active (0xDD1BE4/0xDD1A38/0xDD1BF4 == 0) and the time multiplier <= 1. Otherwise defer to the next s==1.

6. Corrections to the synthesis:
   - The W3DDisplay::draw do-while cannot repeat in retail, because drawFrame returns 0 whenever GameLogic+0x124 is set. C6 only matters for the camera time-multiplier wait.
   - CameraShaker calls per frame (row 53, R9) are resolved: at least 2 builds per setCameraTransform. The 1/60 operand is still exact per pair.
   - Row 51 must also redirect the compare thresholds 0x48C56A/0x48C5F0.
   - Row 22 misses 0x4F678D/0x4F7EEE.
   - Row 55's IDIV->x2 must clamp N >= 1 before doubling, and needs the instant-move double step (0x488854/0x488911/0x488995/0x488A19). Without it, an instant move does not finish in the same logic frame, which is a logic-visible difference.

7. All patch sites listed have identical bytes in game.dat, delayfix.dat and game820.dat. None overlaps the AotR hooks (0x5D8A64, 0x5D8AF1, 0x629D11, 0x638D49, 0x69A760, 0x6D40FA, 0x6D410A, 0x6D57AF, 0x6D7267, 0x8A144D, 0x9A3AE0).

8. Shared constants that must never be edited in place: 0xBDD760 (0.05: follow ramp, overlays, movie fade 0x65C97A), 0xBE5600 (0.01), 0xBDD74C, 0xBD869C, 0xBDFC6C, 0xBD88C8.

Process note: one accidental stray file (a Python REPL error log from a malformed heredoc, outside the scratchpad) was created during this run and deleted immediately. No other files were written.

## Items

### G2-D01 — W3DDisplay::draw prologue  [run_on_B_as_is, high]
- **Site:** 0x44B7B5 call 0x4430BB (avg FPS, QPC, 30-sample ring 0xDC7590); 0x44B7BC call 0x4438DA -> 0x601F94/0x6020E7 (dynamic LOD from display+0x180)
- **What:** Updates the average FPS from QPC intervals over the last 30 renders and selects the dynamic LOD level from it (setter is idempotent when unchanged).
- **Cadence:** per render (real time).
- **Reason:** Real-time measurement; the window becomes 0.5 s at 60 FPS; the LOD is chosen from real FPS, so it is >= stock at a true 60 FPS. No frame-count state.
- **Risk if wrong:** None (client LOD only). Skipping on B would halve the sample rate only.

### G2-D02 — W3DDisplay::draw debug/screenshot  [n/a, high]
- **Site:** 0x44B7C1..0x44B7FD (display+0x30 debug callback: 0x447B4C/0x44ABB2/0x448B51/0x44A2C7); panorama/screenshot counters 0xDC7568 (0x44BBDE, 0x44BC4B), 0xDC76FC (0x44BBD8), 0xD98CAC (0x4474CD); terrain LOD benchmark 0x4436DD (only when GD+0x50==8, one-shot)
- **What:** Debug stats display, panorama screenshot slices and a one-time terrain LOD benchmark (calls updateViews in a loop).
- **Cadence:** event / debug
- **Reason:** Not active in normal play. The benchmark is a one-shot at startup and identical in both modes.
- **Risk if wrong:** None for gameplay.

### G2-D03 — W3D sync clock  [split_step, high]
- **Site:** 0x44B911 (C3 SyncStub) mov eax,[0xDC7A8C]; imul eax,esi; add [0xDC7580],eax -> WW3D::Sync 0x516E20
- **What:** Advances syncAcc by 33*d, where d = m_frame delta and is 0 when GC+0xC8==0 (frozen). Sync feeds model animation 0x4BF560, the drawable-pass gate 0x48C701, particle emitters 0x5A1D60 (global delta), snow 0x4943E1 (global delta) and UV/shader time.
- **Cadence:** per render; stock: only renders following an advancing frame add time.
- **Reason:** C3's lead split (B borrows 16/17 from the next A) is correct while running. As written (d==0 && g_lastStepB) it also fires on every B during script-frozen time (C1e makes frames alternate s=12 B / s=13 failed tick). Stock adds 0 there, so syncAcc would drift unboundedly and drawables would animate during frozen cinematics. Required condition: allow a B advance only if the previous render was an A-render with d>0 and pending==0. A freeze can then be entered with at most one outstanding 16/17 ms, which is repaid by 'delta = 33*d - pending' on the next advancing A.
- **Risk if wrong:** Without the condition: animations/UV/water/shader time keep running at half speed during frozen-time cinematics and stay offset afterwards (visual only, but obvious). Skipping on B entirely would freeze the drawable pass on B-renders (units drawn at 30 Hz).
- **Fix @ 0x44B911:** As C3 (56 e8 <SyncStub> 01 05 80 75 dc 00 90 90). SyncStub: d>0 -> 33*d - pending, pending=0, g_prevAdv=1. d==0 && g_prevAdv && pending==0 && previous render was an A-render -> h=16/17 alt, pending=h, g_prevAdv=0. Otherwise 0. (orig: a1 8c 7a dc 00 0f af c6 01 05 80 75 dc 00)

### G2-D04 — W3DDisplay::draw fast-forward  [not_on_B_path, high]
- **Site:** 0x44B8A2..0x44B8EF (fast-forward/isTimeFast: render only if m_frame%30==0, else updateCameraMovements + sync += [0xDC7A8C])
- **What:** Skips the render and steps the camera plus sync once per call.
- **Cadence:** per call while fast-forward is active
- **Reason:** The mode controller (synthesis 2.3) turns 60 mode off for GD+0xBBD / isTimeFast. In the switch window B-frames are 'frozen' (GC+0xC8=0), so this branch is not taken on B.
- **Risk if wrong:** Transient only (until the next s==1).

### G2-D05 — W3DDisplay::draw time-multiplier limiter  [run_on_B_as_is, high]
- **Site:** 0x44B946..0x44B98F (camera time-multiplier skip counter 0xD98CB4, prev=now-30); 0x44B9B8..0x44B9DD inner wait (cmp 0x1D); do-while tail 0x44BBFE..0x44BC20
- **What:** With multiplier N>=2, draws 1 of N calls and waits >=29 ms since the last drawn frame. The do-while repeats only if frozen && !cameraFinished && GameLogic+0x124. drawFrame returns 0 whenever GameLogic+0x124 (0x44A23E..0x44A262), so the repeat is dead in retail.
- **Cadence:** per draw call
- **Reason:** 60 mode is turned off when TacticalView vtDC (+0x23D4) > 1. During the switch window, C6 (0x44B98E e2->f1, 0x44B9C3 1d->0e) keeps N engine frames per ~29 ms of wall time equal to stock. C6's synthesis rationale (paused camera loop) is wrong: only the time-multiplier wait uses it.
- **Risk if wrong:** Without C6, half game speed while multiplier>1 and 60 mode is still on (<= 12 frames).
- **Fix @ 0x44B98E:** f1 in 60 mode (C6) (orig: e2)
- **Fix @ 0x44B9C3:** 0e in 60 mode (C6) (orig: 1d)

### G2-D06 — W3D terrain tracks  [run_on_B_as_is, high]
- **Site:** 0x44B90C call 0x483870 (terrain tracks fade/expire)
- **What:** alpha = 1 - (SyncTime - stamp)/fadeTime, monotone min; it removes edges at alpha 0. Reads 0xDD1E0C before this render's Sync.
- **Cadence:** per render, sync-based
- **Reason:** A stateless function of absolute SyncTime (counters only remove expired edges). Correct once C3 advances sync on B.
- **Risk if wrong:** Skipping on B only delays edge expiry by one render.

### G2-D07 — drawFrame view update / horde batching  [run_on_B_as_is, high]
- **Site:** drawFrame 0x449D24..0x449D3B: if !GameLogic+0x125 {display vtA0 = 0x65BF2F updateViews; 0x479FC4 -> 0x479DB1 horde batch build (0xDC79F0)}; 0x44BC5A 0x4791E4 release
- **What:** Updates views (W3DView::update via thunk 0x48546A -> 0x48BCF2) and rebuilds horde render batches each frame.
- **Cadence:** per render (skipped while Living World map active, GameLogic+0x125)
- **Reason:** Batch build is stateless (rebuilt each frame, refcounts only). W3DView::update is the camera allow-list (see G2-Cxx).
- **Risk if wrong:** Skipping updateViews on B would freeze the camera and the drawable pass at 30 Hz.

### G2-D08 — scene render / Present  [run_on_B_as_is, medium]
- **Site:** drawFrame: 0x449D8C 0x472EF4 / 0x449DA6 0x473CAD (shroud; timeGetTime 0x472CF2/0x473AAB); 0x449DF5 0x47D5C9 shadow map; 0x449E5D 0x47F25A water reflection; 0x449FE0 display vt98 -> view vt178 -> W3DView::draw 0x4876C6 (filter pre/post 0x474FAA/0x474FDC, WW3D::Render 0x51CCD0, VB ring 0xDC79C0 %50 at 0x4752F3); 0x44A228 End_Render 0x516DA0 (FrameCount 0xDD1E20 ++)
- **What:** Renders the scene and passes. FrameCount readers only choose double-buffer halves (0x5AAB18, 0x5AAE95, 0x5ADCE1, 0x5AE4CA, 0x5AE7A4, 0x5AE8D0: &1). 0x5459C0 compares with 0xDD9084, which is never written (dead).
- **Cadence:** per render
- **Reason:** Real-time (shroud) or stateless per-frame work. WW3D library render objects use Get_Frame_Time/Sync (fixed by C3).
- **Risk if wrong:** None found.

### G2-D09 — W3DView screen filters (script CAMERA_BW_MODE_*, fades)  [skip_on_B, high]
- **Site:** View-filter fade counters: BW 0xDD1BEC INC at 0x4FB599, 0x4FB5DD (0x4FB56F, shader BW class vt 0xBE533C slot5, called from postRender 0x4FB35F via vt+0x14 at 0x4FB383) and 0x4FBC7C, 0x4FBCC0 (0x4FBC57, non-shader BW vt 0xBE5358); mask filter 0xDD1A40 INC at 0x4F5C3A, 0x4F5C8D (0x4F5C29, called from preRender 0x4F5CEA, class vt 0xBE51DC); monochrome-zoom fallback 0xDD1BFC INC at 0x4FCBAC, 0x4FCBF0 (0x4FCB87, vt 0xBE53CC). Setter W3DView vt+0xC0 0x4857FB (frames -> 0xDD1BE8 and 0xDD1A3C, dir -> 0xDD1BE4/0xDD1A38) and 0x484E9E (0xDD1BF8=12).
- **What:** Fade value = counter/frames, with counter++ per render. At the end it calls TacticalView vt+0xB4(0)/vt+0xBC(0) (filter off).
- **Cadence:** per render while a filter is active (also while paused).
- **Reason:** Frame-count fade with 30 Hz units from script. Gating the INC with g_uiTick gives the stock count per pair (exact, 30 Hz steps) and survives mode switches. Shader monochrome-zoom 0x4FBF86/0x4FC28E/0x4FC884 steps only on a logic-frame change (L) and stays as is. Counter 0xDD1AB4 in 0x4F9278 is dead (frames/dir never written).
- **Risk if wrong:** Unpatched: BW/mask fades 2x fast and the filter turns off early (visual). No logic effect: isCameraMovementFinished's filter early-out tests filter 2 only (0x486352).
- **Fix @ 0x4FB599, 0x4FB5DD, 0x4FBC7C, 0x4FBCC0:** e8 <IncIfTick(0xDD1BEC)> 90; stub: if (!g_m60 || g_uiTick) inc dword [0xDD1BEC]; preserve EAX (cmp eax follows) (orig: ff 05 ec 1b dd 00)
- **Fix @ 0x4F5C3A, 0x4F5C8D:** e8 <IncIfTick(0xDD1A40)> 90 (orig: ff 05 40 1a dd 00)
- **Fix @ 0x4FCBAC, 0x4FCBF0:** e8 <IncIfTick(0xDD1BFC)> 90 (orig: ff 05 fc 1b dd 00)

### G2-D10 — hilightfilter / vapor overlay shaders (row 22 extension)  [run_on_B_with_fix, medium]
- **Site:** 0x4F678D (in 0x4F669F, called 0x4F76B1 inside 0x4F6B8D) and 0x4F7EEE (in 0x4F7E00, called 0x4F8DAB inside 0x4F81D7): movss xmm0,[0xBDD760]=0.05; UV += 0.05 into 0xDD1A50/54 and 0xDD1AB8/BC
- **What:** Per-call texture scroll of 0.05 on two globals each, in addition to the row 22 operands.
- **Cadence:** per shader set() call = per render while drawn
- **Reason:** Linear per call, so halving the step is exact per pair. 0xBDD760 is shared (follow ramp, movie fade 0x65C97A), so redirect the operand only.
- **Risk if wrong:** 2x scroll speed of the vapor/hilight overlay.
- **Fix @ 0x4F678D:** f3 0f 10 05 <&g_uv005> (0.05 / 0.025) (orig: f3 0f 10 05 60 d7 bd 00)
- **Fix @ 0x4F7EEE:** f3 0f 10 05 <&g_uv005> (orig: f3 0f 10 05 60 d7 bd 00)

### G2-D11 — buffered model instances in the terrain render  [run_on_B_with_fix, medium]
- **Site:** 0x4D1417 fade state machine (state +8, alpha +0xC, +-[0xBD88C8]=1/12), called 0x4D2088 from 0x4D1ACD <- 0x4D20C6 <- terrain 'RenderBuffs' 0x46B3E4 <- 0x470F44; class vt 0xBE42F0 (ctor 0x4D12FA, built from a Drawable)
- **What:** Instance alpha fades in/out by 1/12 per draw call.
- **Cadence:** per render (per terrain buffer draw)
- **Reason:** Linear ramp with clamp: a 1/24 step gives the same state at pair ends (2*(1/24) == 1/12 exactly in binary). Alternative exact gate: call stub at 0x4D2088 that skips on !g_uiTick.
- **Risk if wrong:** Fades 2x fast (0.2 s instead of 0.4 s).
- **Fix @ 0x4D1436:** f3 0f 5c 05 <&g_instFade> (1/12 or 1/24) (orig: f3 0f 5c 05 c8 88 bd 00)
- **Fix @ 0x4D1463:** f3 0f 58 05 <&g_instFade> (orig: f3 0f 58 05 c8 88 bd 00)

### G2-D12 — Display image/movie overlay fade  [skip_on_B, medium]
- **Site:** drawFrame 0x44A033 and 0x44A12E call 0x65CF05 (display+0x110 hold counter -1/call clamp 45; +0x120 alpha -= +0x11C = 1/frames)
- **What:** Fades the logo/overlay image set by 0x65C476 (startup 0x645BDF, shell 0x75E1D3).
- **Cadence:** per render
- **Reason:** Frame-count fade. It is normally only used outside gameplay, where 60 mode should be off; gating keeps it stock if it is ever used in-game.
- **Risk if wrong:** 2x fast overlay fade (cosmetic).
- **Fix @ 0x44A033:** call stub: if (!g_m60 || g_uiTick) jmp 0x65CF05; else ret (ECX preserved) (orig: e8 cd 2e 21 00)
- **Fix @ 0x44A12E:** same stub (orig: e8 d2 2d 21 00)

### G2-D13 — subtitle/text line scroller  [skip_on_B, medium]
- **Site:** drawFrame 0x44A203 call 0x6604D9 -> 0x66045B -> 0x6603BA (list 0xDE4484; entries created by audio 0x450BF0 -> 0x660539)
- **What:** Per call: delay +0x44 -= +0x48; at <=0 it reveals the next line (+0x3C++) and resets the delay to +0x30.
- **Cadence:** per render (when the InGameUI+0x810 flag is clear and display+0x140 is set)
- **Reason:** Per-call countdown in 30 Hz units, so gating with g_uiTick is exact.
- **Risk if wrong:** Subtitle lines advance 2x fast.
- **Fix @ 0x44A203:** call stub: if (!g_m60 || g_uiTick) jmp 0x6604D9; else ret (orig: e8 d1 62 21 00)

### G2-D14 — UI particles / particle cursor (row 73 refined)  [split_step, high]
- **Site:** drawFrame 0x44A0B3 call 0x6A536C; integrate block 0x6A53FB..0x6A5436 (p.x+=v.x; v.x*=d; p.y+=v.y; v.y*=d), flags from TEST at 0x6A53F7 used by JZ 0x6A5437
- **What:** Euler step with velocity damping per render. Expiry and fade use m_frame (M). The random size option calls client RNG 0x6D343B (client seed 0xDA1C74).
- **Cadence:** per render (integrates also while paused)
- **Reason:** p += v/2 on every render and v *= d once per pair (on the !g_uiTick render). The pair total is p+v, v*d, matching one stock render exactly up to float rounding.
- **Risk if wrong:** 2x speed and 2x damping of cursor trails.
- **Fix @ 0x6A53FB:** e8 <UiPartStub> + 55 nops; stub pushfd/popfd; ESI=particle; 30 mode: stock math; 60 mode: p += 0.5*v; if (!g_uiTick) v *= d (orig: f3 0f 10 46 04 f3 0f 58 46 14 ... f3 0f 11 46 18 (60 bytes to 0x6A5436))

### G2-D15 — world animations rise (row 72 verified)  [run_on_B_with_fix, high]
- **Site:** InGameUI draw 0x48EA29 -> vt150 0x6A3B59 -> 0x69DEA9; 0x69DF0E cvtsi2ss xmm0,[0xD9F608 LTR]; z += rise/LTR (only when not paused)
- **What:** Linear rise per draw call.
- **Cadence:** per render, not paused
- **Reason:** Linear, so divisor 10 in 60 mode is exact per pair. Never change LTR itself.
- **Risk if wrong:** 2x rise speed.
- **Fix @ 0x69DF0E:** f3 0f 2a 05 <&g_ltrRise> (int 5 / 10) (orig: f3 0f 2a 05 08 f6 d9 00)

### G2-D16 — UI draw  [run_on_B_as_is, medium]
- **Site:** InGameUI draw 0x48EA29: preDraw 0x6A3B59 {placement ghost 0x6A2AE5 (m_frame&4), mouse decal 0x69B0F8 -> 0x732926 (row 25), floating text draw 0x69DC5B, world anim 0x69DEA9}; drag box 0x48ECF4; per view move hints 0x48EDED, 0x48EAD8; postDraw 0x6A0BC0 (blinks keyed to GameLogic+0x40 at 0x6A0F..); vt158 0x69B7BE subtitles; WindowManager draw 0x48F871
- **What:** Pure drawing or m_frame / logic-frame / real-time keyed effects.
- **Cadence:** per render
- **Reason:** Under model H, m_frame and logic frames keep stock units. Only integrators are row 25 (spiral) and row 72 (G2-D15).
- **Risk if wrong:** None beyond rows 25/72.

### G2-D17 — overlays  [run_on_B_as_is, medium]
- **Site:** W3DMouse draw 0x498CBC (anim cursor timeGetTime 0x498D99 area); APT draw 0x46203E (0x4A89BD own clock, 0x62212F); transitions draw 0x5DB725 -> 0x5DB2CF (state advanced only in update 0x5DB4B1, row 76); Living World view draw 0x49B618 (lerps by +0x198/+0x134); letterbox 0x443939
- **What:** Draw-only or real-time.
- **Cadence:** per render
- **Reason:** No frame-count state in the draw functions.
- **Risk if wrong:** None found.

### G2-D18 — drawable pass  [run_on_B_with_fix, high]
- **Site:** W3DView::update 0x48C701..0x48C762 (iterate drawables, callback 0x485329 -> Drawable::draw 0x67C482 -> 0x67C1FB -> 0x6765B9/0x67171D/0x67BE90 -> draw modules vt+0x2C)
- **What:** Gated on SyncTime!=PrevSyncTime. Per-draw integrators are only those of rows 20, 25, 29-31. Transform caches use C4 and physics uses C5. applyPhysicsXform is skipped while time frozen && camera moving (0x67BED1..0x67BEF0).
- **Cadence:** per render when sync advanced
- **Reason:** Needs C3 (B sync advance) plus C4/C5 and the row fixes. No further integrators found in Drawable::draw (0x67C482 decompiled).
- **Risk if wrong:** Without C3, drawables are posed at 30 Hz.

### G2-C01 — camera bookkeeping  [run_on_B_as_is, high]
- **Site:** W3DView::update (ECX=ebx=view+0xB4) 0x48BD25..0x48BDFD and setCameraTransform 0x48B7B1 terrain block (terrain vt+0x218 updateCenter, 0x47D37D/0x47B3F9); prev pos 0x48C035 (+0x23F4) used by 0x4858DC (motion-blur scroll delta); 0x48C6FB terrain vt+0x22C(0); FUN_00489364 bounds; GD+0x9C1 -> +0x24C4 = max(logic frame) in 0x48A953
- **What:** Re-centers terrain and lights on the camera, stores the previous position for the motion-blur delta, and recomputes camera bounds.
- **Cadence:** per render
- **Reason:** Stateless functions of the current camera. The motion-blur delta becomes per-60 Hz frame (shorter trails per frame, same rate).
- **Risk if wrong:** None.

### G2-C02 — camera follow/lock ease (row 48 refined)  [run_on_B_with_fix, high]
- **Site:** follow factor static 0xD99638: init 0x48BE98 (0.05), ramp 0x48BEA2 (+0.05 clamp 1, store 0x48BEB5/0x48BEC2), position lerp 0x48BFE8..0x48C001 (lock type !=1)
- **What:** ff ramps by 0.05 per call up to 1. pos += (target - pos)*ff, where target = the interpolated drawable position 0x676711 or the object position. Once ff==1 it is stateless (pos=target).
- **Cadence:** per render (also while paused)
- **Reason:** Exact per pair for a static target: increment ff only on g_uiTick renders, and use g = 1 - sqrt(1 - ff) on both renders, so (1-g)^2 = 1-ff. Only the 0.67 s ramp-in is affected. After that the lerp is identity and runs per render at 60 Hz.
- **Risk if wrong:** Ramp-in and smoothing 2x fast (visual).
- **Fix @ 0x48BEA2:** f3 0f 58 0d <&g_ffInc>; g_ffInc = 0.05 in 30 mode; 60 mode: 0.05 on g_uiTick renders, 0.0 otherwise (C0 hook updates it) (orig: f3 0f 58 0d 60 d7 bd 00)
- **Fix @ 0x48BFE8:** e8 <FollowLerpStub> 90 90 90; stub: if g_m60 {t=min(xmm1,1); xmm1 = 1 - sqrtss(1-t)}; movaps xmm0,xmm1; mulss xmm0,[ebp-0x20]; ret (only xmm0/xmm1 touched; ebp is the caller frame) (orig: 0f 28 c1 f3 0f 59 45 e0)

### G2-C03 — camera tether ease (row 49 refined)  [run_on_B_with_fix, medium]
- **Site:** tether (lock type view+0x60==1): inside radius GD+0xD4: factor = view+0x64 * 0.01 at 0x48BFC6/0x48BFCB; outside: factor = GD+0xDE0 (CameraEaseFactor) * (1 - r^2/d^2) at 0x48BF8B..0x48BFA7
- **What:** Per-call exponential approach.
- **Cadence:** per render
- **Reason:** Exact per pair inside (constant factor); approximate outside (distance-dependent factor). Convert at the instruction instead of editing GD+0xDE0 or the shared 0xBE5600.
- **Risk if wrong:** 2x tether pull.
- **Fix @ 0x48BFCB:** e8 <TetherInStub> 90 90 90: mulss xmm0,[0xBE5600]; if g_m60 xmm0 = (xmm0<1) ? 1-sqrt(1-xmm0) : xmm0 (save/restore temp on the stack) (orig: f3 0f 59 05 00 56 be 00)
- **Fix @ 0x48BFA3:** e8 <TetherOutStub> 90 90: mulss xmm0,xmm1; conv(xmm0) if g_m60; movaps xmm1,xmm0 (orig: f3 0f 59 c1 0f 28 c8)

### G2-C04 — scripted camera moves (row 55 refined)  [split_step, high]
- **Site:** updateCameraMovements 0x48A953 counter steppers: 0x48660E (+0x20C/+0x208 -> +0x3C), 0x4866A7 (+0x230/+0x22C -> +0x6C), 0x486758 (+0x1E4/+0x1E0 -> +0x70), 0x4867F1 (+0x25C/+0x258 -> +0x30), 0x48A417 (rotate +0x1B0/+0x1AC, hold +0x1BC; also writes time multiplier +0x23D4, clears isTimeFrozen +0x23D0). Setup IDIVs (ms/[0xDC7A8C]): vt+0xCC 0x485DD4 (N), vt+0xD0 0x485E7A (hold) and 0x485E9A (N), vt+0xD4 0x4886B1, vt+0xEC 0x4887F4, vt+0xF0 0x48887A, vt+0xF4 0x48892E, vt+0xF8 0x4889B2; cur reset to 0 at setup (e.g. 0x485E00, 0x485EB4, 0x488784, 0x48880C)
- **What:** cur++ per call; value = lerp(start, end, ease(cur/N)); finished when cur >= N (+hold).
- **Cadence:** per render while not paused / not Living World (+0x125)
- **Reason:** Lag model: N'=2N and hold'=2*hold, with cur starting at 0 as in stock. At each A-frame cur'=2k, and (float)2k/(float)2N == (float)k/N exactly, so state, finish flags, the +0x23D0 freeze clear and the +0x23D4 time multiplier are bit-equal to stock when logic reads them. Readers: GameLogic sub-1 frozen check 0x62E53A, script condition CAMERA_MOVEMENT_FINISHED 0x7EB96D, 0x77CC8A. The display trails by half a frame. N must be clamped to >=1 before doubling (stock clamps after the IDIV).
- **Risk if wrong:** Unpatched: all scripted rotate/zoom/pitch moves 2x fast and freeze-time cinematics end early (logic-visible). Lead model (cur'=1 at start): A-render exact but logic sees a half-step-ahead pose and time multiplier.
- **Fix @ 0x485DD4, 0x485E9A, 0x4886B1, 0x4887F4, 0x48887A, 0x48892E, 0x4889B2:** e8 <IdivN> 90; IdivN: idiv dword [g_33]; if g_m60 {eax=max(eax,1); add eax,eax}; ret (EDX:EAX from the caller's cdq; ECX preserved) (orig: f7 3d 8c 7a dc 00)
- **Fix @ 0x485E7A:** e8 <IdivHold> 90; idiv dword [g_33]; if g_m60 add eax,eax (orig: f7 3d 8c 7a dc 00)

### G2-C05 — scripted camera moves (new finding)  [run_on_B_with_fix, high]
- **Site:** instant-move setup steps (only when ms<=1, cmp edi,1 / jne): 0x488854 call 0x48660E, 0x488911 call 0x4866A7, 0x488995 call 0x486758, 0x488A19 call 0x4867F1
- **What:** For ms<=1 the setup applies the move immediately, so N=1 finishes inside the script action.
- **Cadence:** event (script action, logic frame)
- **Reason:** With N'=2 a single call leaves the move unfinished until the next render. A script testing CAMERA_MOVEMENT_FINISHED later in the same logic frame would then diverge. Calling the stepper twice in 60 mode restores instant completion.
- **Risk if wrong:** Logic divergence (script branch timing) for instant camera mods.
- **Fix @ 0x488854:** e8 <Twice_48660E>: push ecx; call 0x48660E; pop ecx; if g_m60 jmp 0x48660E; ret (orig: e8 b5 dd ff ff)
- **Fix @ 0x488911:** e8 <Twice_4866A7> (orig: e8 91 dd ff ff)
- **Fix @ 0x488995:** e8 <Twice_486758> (orig: e8 be dd ff ff)
- **Fix @ 0x488A19:** e8 <Twice_4867F1> (orig: e8 d3 dd ff ff)

### G2-C06 — scripted camera mods (final zoom/pitch)  [run_on_B_with_fix, high]
- **Site:** remaining-time back conversion (hold - cur + N)*[0xDC7A8C]: 0x485F62 (vt+0x7C 0x485F0F) and 0x486279 (vt+0x88 0x486253)
- **What:** Converts remaining frames to ms for chained camera mods.
- **Cadence:** event (script)
- **Reason:** Under N'=2N the remaining count is in half frames: ms = r'*33/2. r' is even at logic time, so the result is exact.
- **Risk if wrong:** Chained mods would last 2x long.
- **Fix @ 0x485F62, 0x486279:** e8 <ImulMs> 90 90; imul eax,eax,33; if g_m60 sar eax,1; ret (orig: 0f af 05 8c 7a dc 00)

### G2-C07 — camera paths (row 55 refined)  [split_step, high]
- **Site:** ms-based paths: 0x48A9F2 push [0xDC7A8C] -> 0x489817(dt,1) (+0x23C8 look-at spline), 0x48AAE5 -> 0x489817(dt,0) (mode 2), 0x48AB06 -> 0x48688A(dt) (mode 1 waypoint path); spline walker 0x713242
- **What:** elapsed += dt; dist += D*(ease(e/T) - ease((e-dt)/T)), which telescopes; position = spline(dist); the spline walker is driven by distance.
- **Cadence:** per render
- **Reason:** Pass dt = 16/17 alternating on every render. Any two consecutive renders then sum to 33 regardless of A/B phase (also in frozen time), so elapsed at every A-frame equals stock and finish visibility matches. Distance agrees up to float ULPs.
- **Risk if wrong:** Unpatched: path cinematics 2x fast and logic-visible early finish.
- **Fix @ 0x48A9F2, 0x48AAE5, 0x48AB06:** ff 35 <&g_camDt> (data only); g_camDt = 33 in 30 mode; in 60 mode toggled 16/17 by the C0 hook on every render (orig: ff 35 8c 7a dc 00)

### G2-C08 — camera path rolling average (new)  [run_on_B_with_fix, medium]
- **Site:** 0x48688A waypoint path angle smoothing: factor 1/R with R = view+0x2A8 (0x486A5B; set by script vt+0x80 0x48623E CAMERA_MOD_ROLLING_AVERAGE), last segment factor += (1-factor)*progress; applied at 0x486B3D (angle += delta*factor)
- **What:** Per-call exponential angle smoothing toward the path heading.
- **Cadence:** per render
- **Reason:** R=1 (default) gives factor 1, which is stateless. For R>1 use f' = 1 - sqrt(1-f), applied after the last-segment adjustment. Exact only for a constant heading.
- **Risk if wrong:** Heading converges 2x fast when R>1.
- **Fix @ 0x486B3D:** e8 <RollAvgStub> 90 90 90 90; f=[ebp-4]; if g_m60 && f<1 f=1-sqrt(1-f); xmm0 = xmm0*f + [esi] (orig: f3 0f 59 45 fc f3 0f 58 06)

### G2-C09 — camera mode 3 (row 56)  [run_on_B_as_is, high]
- **Site:** mode 3 moveCameraTo: setup vt+0xC8 0x48D26B (IDIV 0x48D3C5 -> +0x19C, +0x2358 = m_frame); stepper 0x48657E (matrix lerp, camera vt+0x54) gated by m_frame change at 0x48AAAF..0x48AACD
- **What:** Steps once per m_frame change.
- **Cadence:** M (30 Hz under H)
- **Reason:** Exact stock state and speed. On B, buildCameraTransform copies the existing camera transform (0x489F96 mode-3 branch). Optional 60 Hz smoothing (lag model): remove the m_frame gate (0x48AABC 'lea edi,[esi+0x2358]; cmp ebp,[edi]; je' -> step every render) and IDIV x2 at 0x48D3C5.
- **Risk if wrong:** Applying the IDIV x2 without removing the gate makes moves 2x long.

### G2-C10 — camera mode 4  [run_on_B_as_is, high]
- **Site:** mode 4 camera animation object 0x48AA25..0x48AAAD: dt = (30.0/[CLIENT_FPS]) * (m_frame - +0x2360) passed to obj vt+4
- **What:** Advances an animation object by the m_frame delta.
- **Cadence:** M (dt 0 on B-renders)
- **Reason:** Correct speed and exact state (30 Hz steps). Optional smoothing would need a DLL half-frame counter instead of +0x2360; keep +0x2360 stock.
- **Risk if wrong:** Changing CLIENT_FPS at 0x48AA62 alone would halve the speed.

### G2-C11 — camera shake (row 52 exact site)  [skip_on_B, high]
- **Site:** legacy view shake (W3DView::shake 0x486D86): intensity view+0x128 (ebx+0x74), dir +0x120/+0x124, offsets +0x118/+0x11C; decay+flip 0x48C121..0x48C148 gated at 0x48C10A..0x48C11F (panorama flags GD+0xEA6/0xEA7)
- **What:** offset = I*dir; then I *= 0.75 and dir = -dir (15 Hz wobble).
- **Cadence:** per render (also while paused)
- **Reason:** Run the decay/flip only on g_uiTick renders. The A-render then shows exactly stock render k's offset, and the B-render shows the next value. The offset application runs on both renders.
- **Risk if wrong:** 2x decay and 2x oscillation frequency.
- **Fix @ 0x48C10A:** e9 <ShakeCave> + 18 nops; cave: if (g_m60 && !g_uiTick) jmp 0x48C14D; mov eax,[0xDE4364]; cmp byte [eax+0xEA6],0; je 0x48C121; cmp byte [eax+0xEA7],0; je 0x48C14D; jmp 0x48C121 (orig: a1 64 43 de 00 80 b8 a6 0e 00 00 00 74 09 80 b8 a7 0e 00 00 00 74 2c)

### G2-C12 — CameraShaker (row 53 resolved)  [run_on_B_with_fix, high]
- **Site:** CameraShakerSystem (0xDC78D4) Timestep 0x4655F8 at 0x5033F5 with fld [0xBDFC6C]=1/30 at 0x5033E5, inside 0x502858 called from buildCameraTransform 0x48A372; setCameraTransform 0x48B7B1 builds twice when view+0x241C==0; active shakers force a rebuild each render via 0x4655DD (0x48C179)
- **What:** elapsed += 1/30 per build, so at least 2 per setCameraTransform (plus 2 per scrollBy/setter call). Evaluation 0x4652D7 uses sin(elapsed) and CRT rand() via 0xB25D60 (client only).
- **Cadence:** per build (>=2 per render while active)
- **Reason:** 60-mode renders make the same number of setCameraTransform calls per render as stock, so 1/60 per build gives the stock elapsed per pair (up to float ULPs). CRT rand is not used by per-tick logic (only setup 0x801AAF, 0x644CC2).
- **Risk if wrong:** Shakes end 2x early.
- **Fix @ 0x5033E5:** d9 05 <&g_shakerDt> (1/30 or 1/60; never edit the shared 0xBDFC6C) (orig: d9 05 6c fc bd 00)

### G2-C13 — camera height settle (row 50 refined)  [run_on_B_with_fix, high]
- **Site:** height settle / wheel-zoom smoothing: 0x48C36A and 0x48C3B0 fmul [eax+0xAB0] (GD CameraAdjustSpeed); cutoff |step|<1e-4 at 0x48C37B/0x48C3C1
- **What:** z += (target - z)*k per call.
- **Cadence:** per render
- **Reason:** k' = 1 - sqrt(1-k) is exact per pair for a static target. Redirect the operand instead of writing GD+0xAB0 (no GlobalData mutation; INI/options keep working).
- **Risk if wrong:** 2x settle speed. The residual at the 1e-4 cutoff doubles (negligible).
- **Fix @ 0x48C36A, 0x48C3B0:** d8 0d <&g_kSettle> (same 6 bytes); the DLL refreshes g_kSettle = g_m60 ? 1-sqrt(1-GD[0xAB0]) : GD[0xAB0] at every s==1 mode evaluation (orig: d8 88 b0 0a 00 00)

### G2-C14 — camera zone zoom (row 51 refined)  [run_on_B_with_fix, medium]
- **Site:** camera-zone zoom scale 0x48C49B..0x48C6D1 (targets view+0x9C/+0xA4 from 0xDE4B40 area, currents +0xA0/+0xA8): step = 0.08*d (0x48C553, 0x48C5D6) with min |step| 0.01 (compare 0x48C56A/0x48C5F0, values 0x48C57F/0x48C589/0x48C601/0x48C60B); snap |d|<0.01 (0x48C537/0x48C5B2); factor = min(f1,f2)
- **What:** Exponential approach with a linear minimum step.
- **Cadence:** per render
- **Reason:** 0.08 -> 0.0408337 (exact for the exponential part; conv is monotone, so min() commutes). The min-step regime 0.01 -> 0.005 is exact only if the compare thresholds change too (row 51 omitted them; otherwise steps are discontinuous between 0.005 and 0.01). Keep the snap thresholds.
- **Risk if wrong:** 2x zoom-scale transition speed.
- **Fix @ 0x48C553, 0x48C5D6:** d8 0d <&g_zoneK> (0.08 / 0.0408337) (orig: d8 0d 50 d7 bd 00)
- **Fix @ 0x48C56A, 0x48C5F0:** d9 05 <&g_zoneMin> (0.01 / 0.005) (orig: d9 05 00 56 be 00)
- **Fix @ 0x48C57F:** f3 0f 10 05 <&g_zoneMin> (orig: f3 0f 10 05 00 56 be 00)
- **Fix @ 0x48C601:** f3 0f 10 0d <&g_zoneMin> (orig: f3 0f 10 0d 00 56 be 00)
- **Fix @ 0x48C589:** f3 0f 10 05 <&g_zoneMinNeg> (-0.01 / -0.005) (orig: f3 0f 10 05 4c d7 bd 00)
- **Fix @ 0x48C60B:** f3 0f 10 0d <&g_zoneMinNeg> (orig: f3 0f 10 0d 4c d7 bd 00)

### G2-C15 — rotate-toward-object height offset (new)  [skip_on_B, medium]
- **Site:** buildCameraTransform 0x489F96: view+0x138 decay 0x48A256..0x48A2CB (when rotate-toward +0x1DC==0): |v|<3 -> 0, else v -= max(0.1v, 3) toward 0 ([0xBDD42C]=3, [0xBD83D4]=0.1). +0x138 is set by 0x48A417 as (cur/N)*+0x1D8
- **What:** Per-build decay of the look-at height offset after a rotate-toward move.
- **Cadence:** per build (2 per setCameraTransform; only while the camera rebuilds)
- **Reason:** Gate the decay on g_uiTick. A-renders do the same number of builds as stock renders, so the state is exact per pair. One register (xmm1=3) serves as both snap band and min step, so operand halving would need a cave anyway.
- **Risk if wrong:** Offset relaxes 2x fast (visual).
- **Fix @ 0x48A256:** e9 <Dec138Cave> 90 90 90 90; cave: cmp byte [ebx+0x1DC],0; jne 0x48A2DB; if (g_m60 && !g_uiTick) jmp 0x48A2D3; jmp 0x48A25F (xmm0=v and xmm2=0 untouched) (orig: 80 bb dc 01 00 00 00 75 7c)

### G2-C16 — special-mode fly transition (row 54)  [run_on_B_with_fix, medium]
- **Site:** 0x502458 'Target'/'Zoom' transition (called per build from 0x502858 at 0x5032xx): pos 0xDD1CBC += vel 0xDD1CB8, vel += 0.05 while < 0.5 (0x50257E..0x50259C); pos 0xDD1CC4 += vel 0xDD1CC0, vel += 0.03 while < 0.4 (0x5026F3..0x502718)
- **What:** Accelerating integrator; its outputs override camera position/target on every build.
- **Cadence:** per build
- **Reason:** The call cannot be skipped on B: the outputs are written every build, so skipping would flicker. Use the row 54 data change (approximate). The exact alternative gates only the two integrator blocks on g_uiTick (caves at 0x50257B and 0x5026F0) and gives 30 Hz steps.
- **Risk if wrong:** 2x transition speed.

### G2-C17 — Living World view (row 57 refined, outside the draw)  [run_on_B_with_fix, medium]
- **Site:** Living World camera (GameClient::update 0x8392A7 -> LW view vt+0x2C 0x49B799, two calls per update: scroll+rotate and keyboard rotate); scroll factor 0x49B7AD movss [0xBD869C]=0.5; rotate 0x49B8D9 movss [0xD99B64] (runtime global written at 0x49CE48); LW moveTo setup 0x6BFB88 (step 1/frames -> +0x7C)
- **What:** Scroll and rotate are applied per call, plus a frames-based move-to.
- **Cadence:** per render (GameClient::update)
- **Reason:** Linear per call, so halving is exact per pair. 0xBD869C (0.5) is shared, so redirect the operand. The rotate operand is a runtime variable, so use a stub. The moveTo stepper and its caller cadence (0x6BF96D <- 0x6BAEB6/0x6BFC41/0x90498E) are unverified.
- **Risk if wrong:** 2x LW scroll/rotate.
- **Fix @ 0x49B7AD:** f3 0f 10 15 <&g_lwHalf> (0.5 / 0.25) (orig: f3 0f 10 15 9c 86 bd 00)
- **Fix @ 0x49B8D9:** e8 <LwRotStub> 90 90 90; xmm0 = [0xD99B64] * (g_m60 ? 0.5 : 1) (orig: f3 0f 10 05 64 9b d9 00)

### G2-C18 — camera -> logic coupling  [split_step, high]
- **Site:** logic-visible camera outputs: TacticalView vt+0x78 0x486352 (isCameraMovementFinished; early true if filter +0x110==2 && mode 7..12), vt+0xD8 0x48B5D3 (+0x23D0 isTimeFrozen), vt+0xDC 0x48B5DA (+0x23D4 time multiplier). Readers: 0x62E53A (GameLogic sub-1 freeze), 0x7EB96D (script cond case 9), 0x77CC8A (logic), 0x44BC0C, 0x6486AF, 0x67BEEB, 0x4CC048, 0x4CDBFA
- **What:** Camera move completion gates logic frame advance and script conditions.
- **Cadence:** read at A-frames (logic) and per render
- **Reason:** Requires the lag model (G2-C04/C05/C06/C07) so these values are stock at every A-frame. No mode switch may happen while a move is active (see design notes).
- **Risk if wrong:** Changed failed-tick counts during frozen cinematics, which breaks bit-identity.

## Open questions

- Does any per-tick logic read camera pose (position/angle/zoom) rather than only the finished/frozen/multiplier flags? Candidates referencing TacticalView from logic-side code: 0x8AF912 (vt+0x118), 0x9457FE (vt+0x160 worldToScreen), 0x8B5738 (vt+0x30), 0x8C3B89, 0x85F28A, 0x779A3D. If yes, the exponential camera integrators (follow, tether, settle, zone zoom, rolling average, user scroll) can only be bit-exact at logic sample points by snapshotting the view object on B-renders and restoring it afterwards. Verify with M2 and a frozen-time cinematic replay.
- Identity and pass count of the instanced-model buffer with fade 0x4D1417 (class vt 0xBE42F0, render 0x4D1ACD via terrain RenderBuffs 0x46B3E4). Is it drawn once per render or also in reflection/shadow passes? The fix stays per pair either way.
- Semantics of the subtitle line scroller 0x6603BA (audio 0x450BF0 -> 0x660539): which unit is +0x48 decremented in, and is it visible in campaign speech?
- Display overlay 0x65CF05 and movie fade-in 0x65C97A (+0.05 per call, reached via the virtual 0x65CC2A/0x65D3F5): is either used in-game (e.g. Living World/campaign videos) while 60 mode could be on?
- Living World camera: cadence of the LW moveTo stepper (setup 0x6BFB88, step via 0x6BF96D <- 0x6BAEB6/0x6BFC41/0x90498E) and LW zoom (vt+0x54) are not traced.
- Snow manager update 0x4943E1 (GameClient::update scope): the weather countdowns 0x49408D/0x492C66 and the rand() in 0x492C43 run per call. G1 should gate the instructions only; the call consumes the sync delta.
- Exact equivalence of the fly transition 0x502458 needs caves at 0x50257B/0x5026F0. Is the 'Target'/'Zoom' special camera mode (vt+0x1CC flag) used in AotR at all?
- Mode-switch handling while a scripted camera move or view-filter fade is in flight: defer the switch (recommended), or rescale active cur/N/hold?
- Dynamic LOD (0x601F94) at 60 FPS: confirm the GameLOD MinimumFPS thresholds do not make the LOD oscillate when the frame time hovers near 16-33 ms (real time only, no logic effect).
