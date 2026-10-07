# root_causes
All four causes are in stock W3DView code. The camera code does not depend on frame rate, so cause 4 is the only one AotR60 can make worse. Ranked by confidence.

1. HIGH. Scroll speed depends on the camera's absolute altitude, not its height above the ground. The code is CONFIRMED; the numbers come from a model (INFERRED).
- scrollBy 0x48C774 moves the look-at point by |scrollAmount| * zoom * 0.25 * ScrollSpeedScalar. The instructions are 0x48C959 `fld [ebx+0x3C]` (zoom) and 0x48C95C `fmul [0xBD1904]` (0.25). There is no slope, height-above-ground (HAG) or frame-time term.
- zoom = eye altitude / offz, where offz = view+0x23F0 = 300. It settles toward (HAGd + T)/300 (0x48C2AE..0x48C2C0), where HAGd is the desired height above ground and T the terrain height.
- So the on-screen pan speed goes as eyeAltitude / HAG. That makes the pan slower over low ground and faster over high ground, in either scroll direction:
  - Settled: speed goes as (T + HAG)/HAG. A plateau 200 units up pans 1.69x faster than the valley floor. Coming down from it gives a 41 % drop that persists even after you stop and start again.
  - During a fast pan with the altitude frozen (cause 2), speed goes as E/(E - T). After a 150-unit descent it is 0.67x; after 300 units, 0.5x.
  - When zoomed in (HAGd 120) the ratio is bigger, e.g. (120 + T)/120.
- This is the most likely meaning of "going down it slows down".

2. HIGH. While you scroll, the eye altitude is frozen but the look-at point keeps re-aiming at the terrain (code CONFIRMED).
- The height adjust is skipped while |m_scrollAmount| (view+0x23FC) >= ScrollAmountCutoff (gamedata.ini:11275, = 50, copied to view+0x2404 at 0x48BCD8). The test is 0x48C309..0x48C31E: `fcompi; ja 0x48C35D`. The only exceptions are HAG < min height 120 (0x48C320..0x48C335) and EnforceMaxCameraHeight, which is No (gamedata.ini:11276).
- A ScrollFactor of 50 (Options.ini) gives H = V = 1.0 (0x91FEBA). So keyboard, edge scroll after its 250 ms ramp, and right-mouse drags over about 49 px all put |delta| at 100-250, over the cutoff.
- Meanwhile groundLevel gL (view+0x2408) is set from the camera height grid every tick (0x48C1EE -> 0x70FE22, stored at 0x48C232).
- The eye sits 1.3032*(E - gL) behind the look-at point (cot 37.5 deg; builder 0x502858). So eye speed = v*(1 + 1.303*s), where s is the grid slope along the camera's forward direction.
- A slope that falls away from the camera slows the pan in BOTH directions: screen-up going downhill, and screen-down (toward the camera) going uphill. At s = -0.3 the factor is 0.61x; at -0.5, 0.35x; at -0.6, 0.22x. A rerun of the model panning toward the camera up a 0.6 slope gave 0.71x screen flow.
- Slopes facing the camera speed the pan up instead (up to 1.78x).
- The grid is a max envelope of the terrain, dilated to slope 0.6 (0x710107). So these ramps reach up to dH/0.6 out over ground that looks flat, which makes them hard to tie to visible hills.
- This explains "either when going down or up". Which one you notice depends on whether you pan toward or away from the camera.

3. MEDIUM. The min-height floor causes judder on big climbs (code CONFIRMED; how tall AotR hills are is INFERRED).
- On a climb more than about 180 above the starting height (at HAGd 300; almost at once at HAGd 120), HAG drops below 120. One adjust step then raises zoom by 30 % of the gap: zoom goes 1.0 -> 1.187 in one tick, the eye jumps about 55 units back, and the flow drops 33 % for that frame.
- This repeats every 3-4 ticks for the rest of the climb, with backward eye steps of 5-33 units. It reads as "going up it slows/stutters".

4. LOW-MEDIUM, INFERRED. Render hitches stall the pan. This is the only route by which AotR60 could make things worse.
- The scroll step is a fixed amount per A-render with no dt. Any late A-render therefore freezes the camera for its whole length.
- The session CSV supports a link to panning, though it cannot prove it at 10 s resolution:
  - A-render stalls of 25-163 ms with no logic time: 3.9 per window while the camera moves vs 1.3 when it does not.
  - holds_over25 peaks in the windows with panning.
  - Pacing debt of 414 ms at t = 31 s.
- A camera 500 above a valley at 4K sees roughly 2.8x the ground area, so it plausibly renders more slowly. 60 mode renders twice per stock step.

Minor effects:
- At the start of a pan, gL snaps from TerrainLogic height up to the grid value, a one-off jerk.
- After you stop, the 0.3/tick settle produces a visible zoom-in or zoom-out.
- Ruled out (CONFIRMED): the 5000 clamp, MinCameraHeight/MaxCameraHeight (commented out, gamedata.ini:11270-11271), clip planes and FOV (fixed: near 10, far GD+0x950*1800, hfov view+0x6C), and AotR60 cuts.
- Map caveat: per-map keys can override cameraMinHeight, cameraMaxHeight, cameraScrollSpeedScalar and cameraMapHeightSmoothnessScalar (0x50136C..0x501454). If smoothness is 0 there is no grid; gL is then not re-aimed and cause 2's slope term disappears, but cause 1 remains.

# stock_or_aotr60
Causes 1-3 are stock behaviour and occur at stock 30 FPS. This is CONFIRMED from the code paths; the in-game A/B has not been run yet.

Why AotR60 does not cause or amplify them (CONFIRMED):
- Every camera integrator runs only in the stock A-render code: scrollBy 0x48C774 (from the LookAt translator 0x83B8A4 and InGameUI 0x6A23CA), the settle/freeze 0x48C2F3..0x48C399, and the gL re-aim 0x48C1EE..0x48C236.
- tools/sites.json has only S1 0x48BD1B, S2 0x48C701 and S2E 0x48C765 inside W3DView::update, and nothing in the scroll or settle code.
- Presentation shows A = halfway(M_{k-1}, M_k) and B = M_k (src/camera.cpp SceneOpen_A, src/camera_math.cpp InterpolateCameraHalfway). That splits each 30 Hz step into two equal halves (model error 9e-13), so the speed profile is exactly stock's, shifted by a constant 16.7 ms.
- Cut rules cannot trip while panning: per-tick eye moves are 5-76 units against the 1500 limit, and the view plane is constant.
- Telemetry agrees: 6 cuts in 2347 A-swaps (log line at 17:31:13), and none in the pan windows at t = 42 / 52.6 / 296 s. aShake 0, no-history 1.

Where AotR60 could matter:
- Perception (INFERRED): the motion is smoother at 60 FPS, so a 0.35-1.8x speed change is easier to see, but its size is identical.
- Render hitches (cause 4): unproven either way.

Quick user test, about 2 minutes. Use the KEYBOARD (fixed |delta| 250) on one hill or cliff edge, at the same wheel zoom each time:
1. Ctrl+Shift+F11 for stock 30 FPS. Hold Up across the hill and back down, then hold Down back over it. Repeat in 60 mode, and once with Ctrl+Shift+F10 (smoothing off).
2. What to look for (stock signature):
   a) On the slope facing away from the camera, the pan is slow both going screen-up (downhill) and screen-down (uphill). On the camera-facing slope it is fast.
   b) After descending into low ground the pan stays slower. Release, then press again: still slower than on the plateau. When you release over low ground the camera visibly zooms in; over high ground it zooms out. That is cause 1.
   c) Rhythmic jolts or zoom-out steps while climbing tall terrain. That is cause 3.
   d) A tiny right-mouse drag (under about 49 px) has no altitude freeze, but is still slower over low ground (cause 1).
3. If F11, F10 and 60 mode all look the same (only smoothness differs), it is stock.
4. If only 60 mode shows stop-and-go hitches while panning (camera freezes, then continues), that is cause 4. Check the aotr60.log stall lines and the holds_over34 / render_a_max_ms columns of aotr60_rates.csv for that 10 s window.

# fix
No change is needed in AotR60's 60 FPS presentation path.

Changing only the presented picture cannot fix this. The presented camera may differ from M_k by at most half a tick. A terrain-normalised picture would differ from the camera used for picking, selection and the cursor by hundreds of units.

What works is an OPT-IN, client-input-side camera tweak, default OFF, that also applies at 30 FPS. Config key: `uniformScroll` in src/config.h, next to cameraInterpolation. It has two runtime patches. Do NOT edit gamedata.ini: an INI edit changes the INI CRC, while a runtime patch does not.

(N) Normalise the scroll step by height above ground. Stock speed is unchanged at terrain height 0 (or at the reference height); this removes causes 1 and 3's speed effect.
- Site 0x48C959, 9 bytes `D9 43 3C D8 0D 04 19 BD 00` (fld [ebx+0x3C]; fmul [0xBD1904]). Entry state: EBX = view, st0 = ScrollSpeedScalar.
- Replace with `jmp CAVE_SCROLLNORM` plus 4 NOPs. The cave, in src/stubs/stubs.asm, in the same style as CAVE_S2:
```
CAVE_SCROLLNORM:
  pushfd / pushad / save XMM0-7
  push ebx
  call ScrollZoomFactor        ; cdecl float(view) -> st0 ; st1 = scalar stays
  add esp,4
  restore XMM0-7 / popad / popfd
  fmul dword ptr ds:[0BD1904h] ; displaced 0.25
  jmp 0048C962h
```
- The C side, in src/camera.cpp:
```
extern "C" float __cdecl ScrollZoomFactor(const uint8_t* view) {
  float zoom = Field<float>(view, 0x3C);
  if (!g_cfg.uniformScroll) return zoom;
  uint8_t* ui = *reinterpret_cast<uint8_t**>(0xDE4830);
  if (!ui || !Field<uint8_t>(ui, 0x7F8)) return zoom;  // only user scrolling (InGameUI::isScrolling)
  float offz = Field<float>(view, 0x23F0);             // 300
  float hag  = Field<float>(view, 0x50);               // eyeZ - terrain, written 0x48C252..0x48C266 (last tick)
  if (!(offz > 1.f) || !(hag == hag)) return zoom;
  hag = Clamp(hag, 0.25f * offz, 4.f * offz);
  return (hag + g_scrollRefHeight) / offz;             // ref 0 => stock speed at T = 0
}
```
- Add a sites.json entry and regenerate with gen_sites.py / verify_sites.py.
- g_scrollRefHeight: default 0. Optionally capture view+0x54 on the first A-render after map load, so speed at the player's base matches stock (INFERRED as preferable).
- view+0x50 is used rather than gL, so it also works on maps without a camera height grid (where gL is stale). It is one tick stale; the clamp covers jumps after minimap clicks.
- Effect: on-screen pan speed becomes about constant whatever the altitude, both settled and during the freeze.

(F) Keep terrain following while fast-scrolling. This removes cause 2's height drift, the downhill zoom-out and the uphill sinking, and with them the floor jolts.
- Patch 0x48C31E `77 3D` (ja 0x48C35D) to `EB 3D` (jmp 0x48C35D), only when uniformScroll is on.
- The camera then settles toward (HAGd + T)/offz at CameraAdjustSpeed 0.3 per tick while scrolling, just as when not scrolling.
- Alternative: write FLT_MAX to view+0x2404 after 0x48BCD8. Writing GD+0xAAC later does nothing, because it is copied at view init.

N + F together:
- Flat ground pans at 1.00x everywhere (model: plateau 1.69 -> 1.00), with no zoom drift and no floor snaps.
- What remains is the unavoidable slope foreshortening, 1 + 1.303*s along the view direction (0.64x / 1.32x at |s| = 0.3). It lasts only while crossing a grid ramp, which is dH/0.6 of travel.
- Removing that too would need step / (1 + cot*s), which diverges near s = -0.77. Not recommended.
- N alone gives constant screen speed but keeps the visible zoom drift and snaps. F alone keeps the 1.69x altitude factor.

Safety, stated precisely:
- Neither patch touches the logic step, the RNG, the save format or game speed.
- Both change the real camera state (pos, zoom, gL), which logic can read in some paths: camera-area script conditions and camera position in saves. The effect is equivalent to the player scrolling at a different speed, the same class of change as Options.ini ScrollFactor (0x91FEBA) or the mouse wheel. It is deterministic for the same input and safe for single player.
- Both are gated on InGameUI isScrolling (InGameUI+0x7F8). F's branch is inside the isScrolling block that starts at 0x48C2FB. So scripted, cinematic and follow cameras are untouched: the 0x452 messages, waypoint +0x23CC, freeze flag +0x23D0, multiplier +0x23D4 and finished flag vt78. That isScrolling is never set by script paths is INFERRED; verify that the setter 0x69B28F has only UI callers.
- Risk: this deliberately breaks the AotR60 invariant "camera bit-exact stock" (PLAN line 193, sections 1.2 / 1.6). It must therefore stay off by default, be excluded from compare_traces.py stock-equivalence runs (run those with it off), and be documented in the README as a feel change, not part of the 60 FPS feature.
- Other risks: F makes the camera bob with the terrain, which Generals deliberately froze. N reduces map-traversal speed when HAG is small and raises it when HAG is large.

If telemetry shows cause 4 (pan stalls only in 60 mode), that is a pacing / render-cost item for split present and the pacer, not a camera change. Scroll must stay one fixed step per A-render, as in stock.

# telemetry
Add a read-only camera trace captured in S2_RecordMkAndOpen (src/camera.cpp:215). It runs on every A-render with viewB4 = view+0xB4. It writes into a ring buffer and is flushed by the telemetry thread to %APPDATA%\Age of the Ring\aotr60\aotr60_camtrace.csv during a 15 s capture after a hotkey, or automatically while isScrolling.

Per-row fields:
| Field | Source |
|---|---|
| renderId, QPC time | (timing) |
| pos x / y | viewB4-0xA8 / -0xA4 |
| zoom | viewB4-0x78 |
| HAGd | viewB4-0x74 |
| HAG | viewB4-0x64 |
| T | viewB4-0x60 |
| gL | viewB4+0x2354 |
| \|scrollAmount\| | viewB4+0x2348 / +0x234C |
| cutoff | viewB4+0x2350 |
| isScrolling | [[0xDE4830]+0x7F8] |
| eye x / y / z | r.xf translation, cam+0x24 / +0x34 / +0x44 |
| A-render ms; A and B present timestamps | pacer |
| uniformScroll factor applied | when the opt-in feature is on |

Offline you can derive:
- world speed |dpos|/dt and eye speed;
- screen-centre flow, proportional to |deye|*sin(37.5 deg)/(eyeZ - gL);
- frozen ticks (isScrolling && |SA| >= cutoff);
- floor snaps (a frozen tick where zoom rose);
- scroll stalls (A-render interval > 50 ms while isScrolling).

Add 10 s CSV columns: scroll_ticks, scroll_frozen_ticks, scroll_floor_snaps, scroll_hag_min / mean / max, scroll_stall_ms, scroll_stalls_over50.

Expected readings:
- Stock causes: flow tracks eyeZ/HAG and 1 + 1.303*s; in the cut columns, cuts and shake-held stay flat during pans; the readings are identical with F11 vs 60 mode.
- Cause 4: scroll_stalls_over50 > 0 that correlate with high HAG / large view area, and appear only or mostly in 60 mode.
- Existing counters already confirm the presentation is not involved: aCuts 6 / aSwaps 2347, aNoHistory 1, aShake 0.

Scratch evidence:
- <analysis workspace>/camera_elev\stock_terrain\sim.py
- camera_elev\stock_verify\rescue.py
- camera_elev\model\pan_model.py and run.py
- camera_elev\adv\backward.py and slowcheck.py
- camera_elev\verify\sim_verify.py