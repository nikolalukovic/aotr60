# AotR60 phase 6: Living World strategic map at 60 FPS (consolidated plan)

Nothing was created, changed or deleted in the repo or the game folders. The only file I wrote is the scratch file `<analysis workspace>/lw_tmp\lead\sites.json`, which holds the full JSON of the section 1 site list (a working file, not in the repo; the sites that were implemented are in tools/sites.json).

**What I re-checked myself against game.dat:**
- The site bytes at 0x449F43, 0x449D63, 0x449DBD, 0x49B77E, 0x49B78F and 0x6C0E6B.
- Vtable slots: 0xBDE938 holds 0x49B618; 0xC1459C holds 0x6BE50E; 0xC4E2E0 has slot +0x34 = 0x9A7ED6 and slot +0x3C = 0x9A7EE0.
- The disassembly of 0x6C0E4D..0x6C0E8C and 0x632A74..0x632A95.

**Hand-off gaps:**
- The lw-armies area came through truncated: its verifier verdicts are missing.
- Two of the five areas are missing entirely. The scratch folders `lw_tmp/ui` and `v_tel.py` suggest they were the LW UI (0x645750) audit and a telemetry audit.
- Anything that rests only on those areas is marked as unverified below.

## 0. Starting position (what the areas proved)
- **Logic side needs no new gates.** Every writer of LW logic-visible client state already sits on an A-only path:
  - GATE_GC_LOOKAT 0x6484B2;
  - GATE_GC_LWXLAT 0x6484BD (0x8392A7 → vt2C 0x49B799 / vt54);
  - GATE_GC_LWVIEW 0x6484D7, the whole of vt6C 0x49AAB8: zoom 0x49AB21, damping 0x49AB3E, fades 0x49ABB4/0x49ABFD, and through 0x49AAD4/GATE_LWM → 0x6C0E4D the army/icon mover 0x6C038B, the LWM vt28 (eye tower, consumes the logic RNG) and moveTo 0x6BF78E;
  - GATE_GC_LWUI 0x645750;
  - GATE_GC_DISPUPD_LW.
  - LW logic reads this state: zoom via vt60 at 0x6B96F8, the fade-done flags via vt8C/vt90 at 0x6B5BDE/0x6B5BF7, the target +0x110 via vt70 at 0x6BAF63, and the army moving flag and waypoints (lw-armies).
  - **Rule: these gates stay A-only for good.** The old phase-6 idea of running the vt6C tail on B is dropped, because it would double zoom, fade and army speed and change LW turn timing.
- **The LW draw is stateless per render.** That covers vt20 0x49B618, 0x49B4A5, 0x49AF11, LW scene vt5C 0x49C3DF, the army lines 0x49C384/0x49C215 and the embedded window 0x975441 → vt24 0x49BEFD.
  - A B-render therefore reproduces stock render k with no new gates.
  - Sync-clock animation splits correctly, because the stepper sets GC+0xC8=1 on every non-halted step (0x6325F8).
- **What is still missing is presentation.** Without it, A and B show the same LW picture, so camera and army motion step at 30 Hz while the screen updates 60 times per second.

## 1. Final site list (7 new sites; the full JSON was a working file, not in the repo; the sites that were implemented are in tools/sites.json)

| id | addr | kind | status |
|---|---|---|---|
| LW6_CAM_REC | 0x49B77E | call_gate (`call 0x49B4A5`) | confirmed. B path changed to verify-only, following the verifier of the "B needs no swap" finding |
| LW6_CAM_SCENE_END | 0x49B78F | call_gate (`call 0x518000`) | confirmed |
| LW_ICON_SNAP | 0x6C0E6B | call_gate (`mov ecx,esi; call 0x6C038B`, 7 bytes → e8+2 NOP) | verifier verdict missing from hand-off; lead re-checked bytes, in-function branches and vtable slots |
| LW_SCENE_PRESENT | 0x449F43 | call_gate (`mov eax,[ecx]; call [eax+0x20]`) | span and liveness confirmed (lw-render verifier); uses the lw-armies Open/Close stub |
| TEL_LW_TICK | 0xC1459C | ptr_slot (vt28 of TheLivingWorldLogic) | lead-proposed. The dword 0x6BE50E occurs only at 0xC1459C; plain `ret`, no args |
| LW_B_SKIP_SHROUD | 0x449D63 | jmp_detour, optional | confirmed (corrected text) |
| LW_B_SKIP_SHADOW_WATER | 0x449DBD | jmp_detour, optional | needs_changes → corrected (counter only on the skip path) |

**Dedupe and drop decisions:**
- **lw-render's LW_SCENE_PRESENT variant is dropped.** It wrote the halfway pos/angle/zoom into view +0xF8/+0x11C/+0x134 and re-ran 0x49B4A5 at SceneRestore. Its replacement is the camera-matrix swap (LW6_CAM_REC/LW6_CAM_SCENE_END), because:
  - writing view fields leaves halfway-derived +0x110..+0x118, +0x188 and +0x18C..+0x194 behind if the following draw is skipped (0x449D1E), and the defeat handler 0x6BAEB6 reads +0x110;
  - it costs an extra 0x49B4A5 call;
  - the "interpolate the matrix, never the fields" finding was confirmed.
- **Address 0x449F43 is reused by the lw-armies icon Open/Close stub.** The call order on an A-render is:
  1. icon Open;
  2. vt20, which itself runs: 0x49B4A5 → LW6_CAM_REC (record M_k, swap to the halfway camera) → 0x518000 under LW6_CAM_SCENE_END (render, then end the camera swap);
  3. icon Close;
  4. return to 0x449F48.
- **Not added:** a twin pair for the embedded APT LW window (vt24 0x49BEFD: 0x49C168/0x49C179). It stays exact at 30 Hz camera steps; revisit only if smooth wheel zoom in the preview window is wanted.
- **The optional sites are not added on day one.** Add them only if telemetry shows the B-render cost on the LW map matters at 4K. The shroud, shadow and water passes run on every LW render in stock today, on the last loaded map.
- **Repo conventions:** game-address jumps go through `T_xxxxxx` slots; counters use RUNCNT/SKIPCNT with generated IDX_ (stubs.asm already runs RUNCNT on 30-mode paths where flags are dead). Run the generator so `sites.gen.h` picks up the 7 ids and the 4 new T_ targets: T_6C038B, T_49B4A5, T_518000, T_449D6B/T_449DC5/T_449E08/T_449E70 (T_449DAB already exists).

## 2. Mode predicate change (src/frame_ctl.cpp)
In `BlockReason()`, replace lines 103-106 and delete the separate block at 114-116:
```cpp
uint32_t mode = Field<uint32_t>(gl, 0x110);
bool lwMap = Field<uint8_t>(gl, 0x125) != 0;
if (lwMap || mode == 8) {
    if (!g_cfg.lwMap)            return "Living World strategic map (LivingWorldMap=0)";
    if (!lwMap || mode != 8)     return "Living World map transition";   // mode 8 set at 0x6B52BB before setActive; 0x125 alone after a type-4/6 load before the mode is restored
    uint8_t* lwc = Ptr(0xDE4958);  // LW client/view
    uint8_t* lwl = Ptr(0xDE4950);  // TheLivingWorldLogic
    if (!lwc || !lwl)            return "Living World map not ready";
    if (!Field<uint8_t>(lwc, 0x18)) return "Living World map not active";
    // +0x19 (suspended, embedded APT window) is allowed: stateless draw, camera stays 30 Hz-stepped but exact
} else if (mode != 0 && mode != 2 && mode != 6) {
    return "game mode (menu or multiplayer)";
}
```
Leave the GL+0x114 multiplayer check, TheNetwork, GL+0x9D and the rest as they are. Mode 8 is SP only: 0x62602B maps +0x114 0→8, while LW-MP is mode 1/5 with +0x114 1/2.

**Supporting changes:**
- Add ini key `LivingWorldMap` (default 1; 0 keeps the strategic map at stock 30). Plumb it as `g_cfg.lwMap`, and add it to README's config table.
- **StartBlockReason:** keep it as is. If the in-game test shows the title stuck on "camera time multiplier" or "first frames of a game" on the LW map, skip the TV+0x23D4 check when GL+0x125 is set. That field belongs to the tactical view, which the LW map does not use. GC+0x10 advancing on the LW map is not proven.
- Update docs: ../gaps/G5_modes.md G5-06 reason → phase 6 state; PLAN §1.8 list. In sites.json, replace the GATE_LWM risk note (2) with: "vt6C stays fully A-only in phase 6; the tail has no angle inertia; 0x6C038B (army mover) is logic-visible". Update the GATE_GC_LWVIEW note to match.

## 3. DLL code to add or change

### 3a. LW camera presentation (src/camera.cpp, camera_math.*)
- **Data:**
  - `struct LwCamRec { uint8_t* view; uint8_t* cam; float xf[12]; uint32_t vp[7]; int32_t st; uint8_t act, sus; uint32_t renderId; bool valid; } g_lwRec[2];`
  - `g_lwStats {lwSwapA, lwNoHist, lwCut, lwBExact, lwBNoRec, lwBMismatch, lwEnds}`
  - `float LwCamCutDist = 1500` (eye translation; the LW extent is 4200x3450).
- **Record point (after `call 0x49B4A5` inside vt20):**
  - A-render: shift [1]→[0] when renderId changed, then record cam+0x18 (48 bytes) and cam+0xD8 (28 bytes: extents, aspect +0xE8, znear +0xEC, zfar +0xF0), plus view +0x14, +0x18 and +0x19.
  - Interpolate only when all of these hold: g_featCamInterp; no swap active; [0] valid, renderId−2, same cam and view; st, act and sus unchanged; not identical.
  - Then run `InterpolateCameraHalfway` with LW limits, and on success `BeginSwap(cam, mid.xf, mid.vp, false)`.
- **Swap point:** `LwCamSceneEnd` → `CamSwapEnd()` right after WW3D::Render 0x518000.
- **Backstops:** SCENE_RESTORE 0x44A271 (already calls CamSwapEnd) and `CloseWindowsSafetyNet` at C0.
- **B-render: no swap.** B rebuilds exactly M_k from the unchanged state. Only compare it against g_lwRec[1] (lwBExact / lwBMismatch), and log the view fields once on a mismatch.
- **`BeginSwap(cam, xf, vp, bool refit)`:** tactical callers pass `RefitWanted()`; LW passes `false`, so `g_save.refit` stays false. Extra guard: never refit when `cam != TacticalCamera()`. Stock never fits the shadow manager to the LW camera; 0x47D37D is called only from 0x48B7B1/0x48BCF2 with view+0x104.
- **`CameraCutLimits.allowFarChange`** (LW only):
  - compare only vp[4] (aspect) and vp[5] (znear, constant 10) byte-exactly;
  - set `mid.vp[6] = max(a,b)`, because zfar = 1.5·dist + 2000 changes on every zoom frame;
  - keep the 5% extent rule, the 45° rotation limit and the distance limit.
  - Unit tests in tests/test_camera_math.cpp:
    1. a zoom pair with zfar 2600 vs 2750 interpolates;
    2. the same pair with the flag off cuts;
    3. a pair with a different znear or aspect cuts.
- **Cuts:**
  - **Cut (present M_k):** history not contiguous; a change in +0x14, +0x18 or +0x19; aspect or extent >5%; eye distance > LwCamCutDist; rotation > 45°. Instant setters behind these:
    - 0x6BF878, reached from battle-exit 0x6B33FB and from DelayedSplineCamera 0x7FEE5B at 5 Hz;
    - setSuspended vt50 0x49BE23;
    - deactivation 0x6BF8BB.
  - **Interpolated:** moveTo flights, scroll, rotate, zoom inertia and the +0x198 fade blend.
- **`SceneOpen_A`:** after the load-screen check, `if (gl && Field<uint8_t>(gl,0x125)) return;` before `OpenWindow`. This keeps GE+0x3C stock on the LW map and stops the bogus aNoHistory count.
- **`CameraReset()`** also clears `g_lwRec[0/1].valid` and `g_lwSnapRender=0`.
- **Telemetry:** print the lwStats line next to the camera stats.
- **Accepted, visual only:** on A-renders the LM_SunRays height (+0x188 m[11] = dist·2·pitch) and the zoom alphas (+0x18C/+0x190/+0x194) stay at S_k. Optional later: lerp m[11] around 0x518000.

### 3b. LW army / icon smoothing (new src/lw_present.cpp)
- **`LwIconSnapshot(view)`** runs from LW_ICON_SNAP on a 60-mode A-render, main thread, with g_featPresent, just before 0x6C038B. It is read-only:
  - walk the view+0x98 hash map (buckets [+0x9C,+0xA0), node→next at +0, object at +8);
  - for objects whose vt+0x34 = 0x9A7ED6 and vt+0x3C = 0x9A7EE0, take the icons from the vector [obj+0x2C,+0x30) and the ROs [icon+8]/[icon+0x14], skipping those with RO+0x7C != 0;
  - store `{RO, RO+0x18[12]}` in `g_lwSnap[8192]` plus a hash index;
  - guards: buckets ≤ 65536, chain ≤ 4096, icons ≤ 64; overflow sets `g_lwSnapRender=0`;
  - set `g_lwSnapRender=g_renderId`.
- **`LwPresentOpen()`**, from LW_SCENE_PRESENT on an A-render:
  - re-walk the live hash map;
  - for each RO whose transform changed since the snapshot and moved ≤ 400 units: save the current 48 bytes, write the translation as (snap+cur)/2 (and the 3x3 too if max |Δ| ≤ 0.25), and call `RO->vt54(&P)`.
- **`LwPresentClose()`:**
  - restore in reverse order through vt54;
  - at Telemetry ≥ 1, memcmp against the saved bytes → `lwIconRestoreBad` (must stay 0).
- **Backstops:**
  - `SceneRestore()` calls Close when `g_lwRestoreN != 0` (`lwIconLateClose`);
  - the STUB_SCENE_RESTORE fast path adds `cmp dword ptr [g_lwRestoreN],0 / jne work`;
  - `CloseWindowsSafetyNet` logs if `g_lwRestoreN != 0` at C0.
- **Result:** A shows armies halfway between stock k−1 and k, B shows stock k. Picking, the next 0x6C038B, saves and LW logic only ever see restored transforms. Colour and alpha pulses and fades stay at 30 Hz steps.

### 3c. LW telemetry (src/telemetry.cpp, tools/compare_traces.py)
- **TEL_LW_TICK wrapper:**
  - `++lwTicks`; then seedIn, call 0x6BE50E, seedOut;
  - at Telemetry=2, write a line: `LW <lwframe=[lwl+0x100]> <seedIn> <seedOut> <poseHash> <iconHash> <aRendersSinceLastLwTick> <30|60>`.
  - poseHash covers view +0xF8..+0x100, +0x11C, +0x134, +0x138, +0x198, +0x24/+0x25, +0x110..+0x118 and +0x14/+0x18/+0x19.
  - iconHash is an order-independent sum of FNV-1a hashes of RO+0x18 per icon RO.
- **Expected `seedOutside`:** on the LW map it is nonzero in both 30 and 60, because LWM vt28 and the LW logic use the RNG outside GameLogic::update. It is not an error there. Compare per LW tick instead.
- **compare_traces.py:**
  - parse the `LW` lines;
  - align by lwframe;
  - ignore the last column (30/60);
  - report the first differing tick and field.
- **Rates CSV:** add the run/skip counts for GATE_LWM, GATE_GC_LWVIEW, GATE_GC_LWXLAT, GATE_GC_LWUI, GATE_GC_DISPUPD_LW, LW_ICON_SNAP, LW_SCENE_PRESENT and LW6_CAM_*, plus lwTicks/s.

## 4. Implementation order
1. Telemetry first: TEL_LW_TICK and the compare_traces support, with the LW block still on. Record stock (Enabled=0) baselines on the LW map.
2. Make the camera_math change and add its unit tests; add the BeginSwap refit parameter; add the SceneOpen_A early return.
3. Add LW6_CAM_REC and LW6_CAM_SCENE_END, then lift BlockReason behind `LivingWorldMap`.
4. Add LW_ICON_SNAP, LW_SCENE_PRESENT and the SCENE_RESTORE fast-path change.
5. Measure; add the two optional B-skip sites only if the B-render cost on the LW map is significant.
6. Update the docs, sites.json group_notes and G5-06.

## 5. Risks (honest list)
- **No in-game verification yet.** Everything is static analysis.
- **Static-proof gaps:**
  - LW-only GameWindow/APT draw callbacks inside the shared InGameUI/WindowManager draw block on the LW map. The two missing areas may have covered this.
  - Where the banner-scale and army-selection fade integrators live (mapinfo +0xCC, +0x1DC..+0x1F0). They are unreachable from the draw roots, so they are presumably A-only, but not pinned.
  - Virtual calls inside the LW draw were not resolved for the shadow/water skip.
- **Post-draw event callbacks may run in A's post-draw section.** These are the fade callback 0x8004C9 → vt4C, the AptMapPreview teardown 0x9769E8 and setSuspended. If they do, B shows the post-event picture one render early. That is visual only; watch lwBMismatch.
- **Battle↔map transitions with 60 on.** Battle entry passes GameEngine::reset (forced 30). Whether return-to-map (0x62602B in logic sub 1, 0x625E20 → 0x6B33FB) also passes reset is not proven. History invalidation (renderId contiguity plus the +0x14/+0x18/+0x19 cut) protects the picture either way, but the mode log must be checked.
- **Icon swap via Set_Transform:**
  - HLOD propagation (0x574FC0 → vt208) and re-registration in the LW scene list (stock does the same for moving armies);
  - exact restore is asserted, not proven;
  - teleports under 400 units show one halfway frame.
- **LW turbo/fast-forward** (moveTo ×10 via 0x63F122/GD+0x88, army steps up to about 200 units): still A-only and exact, but more cuts and bigger halfway steps.
- **Performance:** 60 renders/s at 4K on the LW map with tactical render-to-texture passes running on every render. If the pacer falls back, the title shows "performance fallback"; then consider the optional sites.
- **The exception-unwind path** (drawFrame throwing) skips Close and Restore, the same exposure as the existing tactical swap. The C0 safety net logs it.

## 6. In-game test checklist (for you)
Setup: `%APPDATA%\Age of the Ring\aotr60\aotr60.ini` → `Enabled=1`, `LivingWorldMap=1`, `Telemetry=2`. Launch as usual and load a Living World campaign save that is on the strategic map.

1. **Mode.** The title shows `[AotR60: 60 FPS, smooth]` on the map. If it shows `30 FPS, <reason>`, tell me the reason.
2. **Camera.** Scroll with the keys and screen edges, rotate, mouse-wheel zoom fully in and fully out (watch for the horizon or far edge popping), double-click a region to fly to it, numpad 5. It should be smooth, with no jitter or back-and-forth. Ctrl+Shift+F10 (smoothing off) must give the same speed with 30 Hz steps. Ctrl+Shift+F11 (30↔60) during a fly-to must not change how long it takes.
3. **Armies.** Order a move, then End Turn and watch AI armies march. They should be smooth at 60, with the same duration as at 30, no flicker or teleport-backs, banners/selection rings/path lines correct, and the turn ending at the same moment.
4. **Transitions.** Zoom-out fades, the region/army preview window (embedded map), the Esc menu, starting a battle from the map and returning (camera not stuck, no jump frame), and eye-tower/Palantir events.
5. **Saves.** Save on the map at 60 and load at 30 (and the other way round). Saves must load identically.
6. **Determinism (Telemetry=2):**
   - **Control first.** Load the same LW save twice at 30 FPS. After loading, do not touch mouse or keyboard for about 2 minutes, then quit. Keep both `aotr60_trace.txt` files. If `compare_traces.py` reports any LW-line difference between these two stock runs, an idle LW run is not reproducible and this test cannot be used (tell me).
   - Then repeat with 60 on: load at 60 and stay idle for the same time. The LW lines (lwframe, seeds, poseHash, iconHash) must match the 30 run tick for tick.
   - **In-run checks I will read from aotr60.log/rates.csv:**
     - LW ticks about 5.05/s;
     - `aRendersSinceLastLwTick` = 6 on every LW line in both modes;
     - GATE_LWM / GATE_GC_LWVIEW / GATE_GC_LWXLAT runs about 30.3/s with B-skips about 30.3/s;
     - lwBMismatch = 0, lwIconRestoreBad = 0, lwIconLateClose = 0, no safety-net swap lines;
     - lwSwapA well above zero while moving, and lwCut only at the transitions.
7. **Report back** anything that looks different from 30 FPS stock apart from smoothness.