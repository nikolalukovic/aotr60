# Uniform scroll: slope term (design and verification)

The final code is `src/camera_math.cpp` (`SlopeScrollFactor`) and `src/camera.cpp` (`ScrollSlopeFactor`); the unit tests in `tests/test_camera_math.cpp` use the verified values below.

## Design

### Summary

I designed the slope compensation for UniformScroll and tested it in an extended 2-D model. Two things are confirmed outside the model: the C++ helper matches the Python model to within 1e-6, and the proposed unit tests pass when built for x86 with MSVC in scratch. Nothing in the repo or the game was changed.

**The formula.** The derivation holds exactly for the screen centre once the look-at point is on the camera grid (INFERRED algebra on the CONFIRMED builder formula). Each step moves the screen-centre ground by sin(p)·(dFwd + k·dgL)/(E−gL), where k = |cameraOffset.xy|/cameraOffset.z = cot(pitch).
- Changes in eye height move the eye along the centre ray, so the 0.3/tick settle cancels out. k is therefore cot(p) exactly: 1.3032 at 37.5°, 1.351 on the 58 maps with pitch 36.5°.
- The step multiplier g solves g·clamp(1 + k·chord(g), lo, hi) = 1. The chord is the grid slope over the compensated forward step, not the slope at a point. The scroll vector is weighted in screen space, so a pure sideways step gets g = 1.
- Parameters: lo 0.6, hi 1.4 (exact for |s| ≤ 0.307, at most 1.67x faster or 0.71x slower), fade 0.15, 12 bisection steps plus one polish step. This costs about 16 calls to the read-only grid sampler at 0x70FE22 per scroll tick.
- Camera-facing slopes are compensated too, but only up to the 0.71x limit (hi 1.4). Full compensation (hi 2) slows the pan to 0.41–0.61x when zoomed in towards a cliff, where the look-at point floats above low ground.

**A related fix I recommend.** The height-above-ground input to the existing scroll factor should be zoom·view+0x23F0 − view+0x54, not view+0x50. The value at view+0x50 is stored before the settle step (0x48C252), so it is one step stale. That staleness causes a 6–38 % speed overshoot after ramps.

**Model results** (keyboard speed, flow at screen centre, 1.0 = same speed as on flat ground). The before column is the current uncommitted tree (N+F).

| Case | Before | After |
|---|---|---|
| Grade 0.15 ramps | 0.80–0.84 / 1.09–1.19 | 0.93–1.01 |
| Grade 0.3 ramps | 0.61–0.68 / 1.17–1.38 | 0.89–1.02 |
| Grade 0.6 falling away | 0.26–0.35 | 0.42–0.55 (capped) |
| Grade 0.6 facing the camera | 1.35–1.76 | 1.01–1.24 |
| Plateaus and valleys, grade 0.3 (min–max) | 0.60–1.40 | 0.94–1.05 |
| Plateaus and valleys, grade 0.6 (min–max) | 0.21–1.82 | 0.36–1.25 |

- **Smoothness:** the tick-to-tick change in speed drops from 0.1–0.4 to 0.00–0.02 on ramps up to grade 0.3.
- **Sideways and diagonal:** sideways along a ramp contour stays exactly 1.0. Diagonal goes from 0.85–1.24 to 0.92–1.01, and a camera rotated 30° from 0.68–1.29 to 0.94–1.02.
- **Simpler variant rejected:** using the slope at a point (sample half-distance d = 20, 40 or 80) overshoots at ramp ends, by up to 2–3x with fast scrolls. Without the screen-space weighting, sideways scrolling along a contour runs 1.64–2x fast.
- **30/60 presentation:** the 60 FPS mode gives the same ratios. The A and B half-frames agree to within 0.01–0.03.
- **Real maps:** on 16 AotR multiplayer maps, 12.3 % of the camera grid is steeper than |0.3| along the forward direction and 6.4 % is at the 0.55–0.6 grid limit.

**Optional phase 2 (my inference; the binary function was not disassembled).** Using the real terrain height under the centre ray via TerrainLogic::getGroundHeight improves cliffs and low zoom. Falling cliffs at HAGd 120 go from 0.26 to 0.45, and grade-0.3 rises at HAGd 50 from 0.89 to 0.99.

**Trade-off to test in game.** The exact screen centre speeds up the bottom of the screen. On a −0.3 slope the lower half moves at about 1.3x and the upper half at about 0.7x, and the average over the whole screen is 1.27–1.35. Steep slopes stay partly slow (0.42–0.55 at grade 0.6) because of the 1.67x cap.

### Binary details

All addresses are in game.dat (base 0x400000) and were checked by disassembly this session.

**Grid sampler 0x70FE22: CONFIRMED.**
- Calling convention: `float __thiscall height(grid, float x, float y)`. ECX = grid object = view+0x2458 (the caller does `lea ecx, [ebx+0x23A4]` with EBX = view+0xB4).
- Arguments: x at [ebp+8], y at [ebp+0xC], pushed as pos.x = view+0x0C and pos.y = view+0x10 (0x48C1D1..0x48C1E3). It ends with `ret 8`, so the callee pops both arguments, and the result comes back in st0.
- Valid flag: [grid+0x1C] = view+0x2474. If it is 0, the sampler returns 0.0 via `fld [0xC1B594]`. Every caller (0x489570, 0x48BCF2, 0x48CDBD, 0x48D26B) also checks view+0x2474 first.
- Grid layout:
  - +0x00: float* heights, stored row-major as [iy*w + ix]
  - +0x0C: w (int)
  - +0x10: h (int)
  - +0x14: cell size (40); the sampler multiplies by 1/cell
  - +0x18: border (int); fx = (x + border·10)/cell, so the grid origin is at −border·10
- It calls msvcr71 floor through the import at 0xBD0580 (identified from the import table) and converts with fistp.
- Index clamping: the indices are clamped to [0, w−1] and [0, h−1].
  - Past the upper edge (ix > w−2 or iy > h−2) it returns the nearest cell value without interpolation.
  - Past the lower edge the index is clamped to 0, but the fraction is taken from the unclamped floor. Outside the map it therefore interpolates between cells 0 and 1 in a sawtooth with a 40-unit period. The proposed code clamps sample points to [x0, x0+(w−1)·cell−0.01] to avoid this.
- Interpolation is on triangles split along the main diagonal:
  - if fy > fx: (h00−h01)(1−fy) + (h11−h01)·fx + h01
  - otherwise: (h00−h10)(1−fx) + (h11−h10)·fy + h10
- Read-only: it writes only its own argument slots and locals, and uses XMM0–5 and at most two x87 stack slots.

**Height block in W3DView::update: CONFIRMED.**
- 0x48C1EE samples the grid at pos. While scrolling and not scripted, gL = min(Tc, 5000) is stored at view+0x2408 (0x48C232).
- 0x48C24D stores Tc at view+0x54. 0x48C252..0x48C266 stores view+0x50 = zoom·view+0x23F0 − Tc. This is computed BEFORE the settle step.
- Settle (0x48C2AE..0x48C399): desired = (view+0x40 + view+0x54)/view+0x23F0, then zoom += (desired − zoom)·GD+0xAB0 (0.3) if |adj| ≥ 1e-4.
- With the cutoff at view+0x2404 raised, `ja 0x48C35D` at 0x48C31E is always taken. So with F the settle runs on every scrolling tick. View+0x50 is therefore always one settle step stale relative to the zoom used by the next scrollBy.

**Camera offset: CONFIRMED.**
- buildCameraTransform 0x489F96 copies view+0x23E8/0x23EC/0x23F0 (ECX[0x8FA..0x8FC]) as cameraOffset, gL from view+0x2408 (ECX[0x902]) and pos from view+0x0C..0x14.
- The offset setup at 0x5011D5 has two branches:
  - Default: z = maxH, y = −z/tan(pitch), x = −tan(yaw)·y.
  - Other: offset = L·(0, −cos p, sin p), with L = maxH·1.64267 = maxH/sin(37.5°).
- So k = |off.xy|/off.z = cot(effective pitch) in both branches. The eye-behind distance k·(E−gL) comes from the earlier review's reading of 0x502858 (CONFIRMED there). The camera's own extra pitch (view+0x2C) and FX pitch (view+0x70) are assumed to be at their defaults (INFERRED).

**Forward vector in scrollBy 0x48C774: CONFIRMED.**
- 0x488F7E(centre pixel) returns the eye position (0x53B370) and eye + ray·zfar ([cam+0xF0]).
- At 0x48C87B..0x48C8DD scrollBy forms p2 − p1, zeroes z ([ebp−0x2C] = 0) and normalises x, y with the fast inverse sqrt at 0x441C56 (magic 0xBE6EB508 plus one Newton step, error about 1e-3). This happens only when |f|² ≠ 0.
- At the CAM_SCROLLNORM site 0x48C959:
  - [ebp−0x34] = fx, [ebp−0x30] = fy (unit horizontal forward)
  - [ebp−0x20] = −fx, [ebp−0x1C] = −fy, [ebp−0x18] = fy, [ebp−0x14] = −fx
  - [ebp−8] = (float)(int)(width/height), integer division at 0x48C820
  - ESI = scroll delta (float[2], loaded at 0x48C7A6 and not touched before 0x48C9B9)
  - EBX = view, st0 = scroll speed scalar (vtbl+0x10 of view+0x24C8)
- Step built at 0x48C962..0x48C9AC, with S = scalar·factor·0.25:
  - dX = (d.x·fy − fx·d.y)·S
  - dY = (−d.x·fx·aspect − fy·d.y)·S
  - That is, pos += d.x·R − d.y·f with R = (fy, −fx), when aspect = 1.
  - Quirk: on a 21:9 screen aspect is 2 and only scales the x-input's y component (it has no effect at yaw 0). The proposed code reproduces this exactly.
- The forward vector could also be derived from the view angle (builder vtbl+0x100), but that is not needed.

**Real terrain height (for the optional depth term).** 0x484FAB calls TheTerrainLogic [0xDE4690] vtbl+0x18 as thiscall(x, y, Coord3D* normal = 0), returning st0 with a callee-cleaned stack (CONFIRMED call pattern). That it is pure is INFERRED; the target was not disassembled.

**Interaction with the settle step (INFERRED from the model).**
- On a steady slope the eye height lags by 2.33·|s|·step, which raises the depth E−gL.
- With the height-above-ground fix (current eye height, not view+0x50) this is cancelled at the centre.
- The step and the lag form a loop with gain ≈ 2.33·|s|·0.25·off·g/offz. For the keyboard this is 0.06–0.11. For a right-mouse drag (off 600) on s = −0.6 with g = 1.67 it is about 0.65: the world step grows about 2.9x on purpose to cover the extra depth. It is bounded by the ramp length and the 4·maxHeight clamp, and no oscillation appeared.

### Formula

**Inputs per scrollBy (A-render, single player, while isScrolling, UniformScroll on, grid valid byte view+0x2474 != 0).**
- f = (fx, fy) at [ebp−0x34]/[ebp−0x30]; skip if |f|² is outside (0.9, 1.1).
- d = delta (ESI), aspect = [ebp−8].
- off = view+0x23E8..F0, so k = |off.xy|/off.z = cot(pitch) and sinp = off.z/|off|.

**N factor (changed).**
- hag = zoom(view+0x3C)·view+0x23F0 − view+0x54. This is the zoom after the settle step, instead of view+0x50.
- factorN = UniformScrollFactor(hag, view+0x40, view+0x23F0, ref), as before: (clamp(hag, 0.5·HAGd, 4·offz) + ref)/offz.

**World step before the slope term.** With S = scalar·factorN·0.25:
- sx = (d.x·fy − fx·d.y)·S
- sy = (−d.x·fx·aspect − fy·d.y)·S
- δf = sx·fx + sy·fy
- δr = sx·fy − sy·fx

**Slope term g.**
- G(x, y) = sampler 0x70FE22 on view+0x2458, with x, y clamped to [−10·border, −10·border + (w−1)·cell − 0.01].
- craw(g) = 1 + k·(G(pos + g·δf·f) − G(pos))/(g·δf). This is the chord over the compensated forward displacement.
- c(g) = clamp(craw(g), lo = 0.6, hi = 1.4).
- Solve g²·(δr² + (sinp·δf)²·c(g)²) = δr² + (sinp·δf)² for g in [1/hi, 1/lo] = [0.714, 1.667]:
  - 12 bisection steps;
  - then one polish step g = sqrt((wr + wf)/(wr + wf·c²)), kept only if it stays inside the final bracket (this makes flat ground exactly 1).
- Fade: if craw(g) < 0.15, g = 1 + (g − 1)·max(craw, 0)/0.15. This protects grids steeper than the view ray (s < −0.767 reverses the image).
- If |δf| ≤ 1e-3 (pure sideways), any NaN, or no grid: g = 1.

**Result.** factor = factorN·g replaces zoom in |d|·factor·0.25·scalar. In the cave it is still multiplied by 0.25 and the scalar.

**Why it works.**
- Screen-centre flow ∝ sin(p)·(δf + k·ΔgL)/(E − gL), so g·(1 + k·s) = 1 restores flat-ground speed. With pure forward motion this reduces to g = 1/clamp(1 + k·s, 0.6, 1.4).
- Sideways: the lateral image term does not depend on the slope, so the screen-space weighting gives g = 1 for sideways steps and a speed-preserving blend for diagonals.
- Feedback: g depends only on the current pos and step (no loop inside one tick). The only cross-tick loop runs through the eye-height lag, which is stable for every input speed tested.

**Parameter choice.**
- k: cot(pitch) from the offset vector. A screen average instead of the centre would favour about 1.0 for s < 0 and 1.25 for s > 0; centre-exact is the stated target.
- Sample distance: none. The chord over the actual step replaces a fixed d of 20/40/80, which overshot by 15–25 % at ramp ends with the keyboard and 2–3x with fast drags.
- lo 0.6 / hi 1.4: exact for |s| ≤ 0.307 in both directions.
  - lo 0.5 raised the 15-point screen average on grade −0.6 slopes to 1.46–2.0.
  - hi 2.0 dropped low-zoom approaches to cliffs to 0.41–0.61.
- Camera-facing slopes: compensated (hi 1.4). Before, they ran 1.35–1.76x.
- Optional depth term (phase 2): hag_real = E − T_hit, where T_hit is real terrain at the centre-ray hit, found by fixed point x = (gL − T(pos + x·f))·k over 3 iterations using TerrainLogic vtbl+0x18. Clamp to [hag, 2·hag].

### Model results

**Model and files.**
- 2-D model: <analysis workspace>/camera_elev\slope\model\
  - cam2d.py: exact replica of the 0x70FE22 sampler and the 0x710107 grid, F settle, N factor, builder, vectorised raycast against real terrain, 30/60 presentation, slope_comp_factor (mirror of the C++).
  - cases.py, run_final.py, pool_final.py.
- Outputs (metrics relative to flat ground with the same input):
  - pf_main.txt: 128 scenarios.
  - pf_rec.txt: the recommended lo 0.6 / hi 1.4.
  - pf_hi.txt: hi and k variants.
  - pf_speed.txt: drag speeds 250 / 600.
  - pf_m60.txt: 60 FPS presentation.
  - pf_side.txt: sideways, diagonal, camera rotated 30°.
  - plane.py: analytic infinite slope.
  - mapslope_out.txt: real-map grid slopes.
  - ts_final*.txt: tick-by-tick time series.
  - Older exploratory runs: ramps_v2_*.txt, misc_*.txt.
- Setup: AotR offz 540, pitch 37.5°, keyboard off 100 (25 world units per tick at zoom 1), grid smoothness 1 unless stated. "ctr" is the screen-centre flow, "scr" the mean over 15 screen points.

**Recommended settings ("rec", lo 0.6, hi 1.4, current-HAG fix), keyboard, zoom heights 540 / 120 / 50.**

| Case | N+F (current tree) | rec | rec + depth term |
|---|---|---|---|
| Grade 0.15, falling | 0.80–0.84 | 0.99–1.01 | 0.99–1.01 |
| Grade 0.15, facing | 1.09–1.19 | 0.93–1.00 | 0.99–1.01 |
| Grade 0.3, falling | 0.61–0.68 | 0.98–1.02, min–max 1.00–1.02 | same |
| Grade 0.3, facing | 1.17–1.38 | 0.89–1.00 | 0.99–1.01 |
| Grade 0.6, falling | 0.26–0.35 | 0.42–0.55 (capped at 1.67x) | same |
| Grade 0.6, facing | 1.35–1.76 | 1.01–1.24 | 1.14–1.26 |
| Cliff 1.2 on smoothness 1 (0.6 grid ramp floating over low ground), falling | 0.08–0.22 | 0.12–0.35 | 0.23–0.46 |
| Same cliff, approached from below | 0.79–1.61 | 0.59–1.15 | 0.70–1.19 |
| Plateau / valley, grade 0.6 (min–max) | 0.21–1.82 | 0.36–1.25 | |
| Plateau / valley, grade 0.3 (min–max) | 0.60–1.40 | 0.94–1.05 | |

- With grade 0.3 at the earlier hi 2.0 / lo 0.5 setting (pf_main, final'), the facing case is 0.89–1.00. That matches rec because grade 0.3 is inside both clamp ranges.
- Grade 1.2 on a smoothness-2 grid is broken in stock geometry: the eye ends up behind or under the slope and the image reverses. Every variant gives 0.3–40x noise there; the fade keeps g = 1.

**Screen points away from the centre** (centre-exact, −0.3 slope).
- Lower row (−540 px) 1.25–1.33, upper row (+540 px) 0.65–0.71, screen average 1.27–1.35. Before: 0.79 / 0.42 / 0.80–0.85.
- On a +0.3 slope: lower 0.83–0.88, upper 1.11–1.19, average 0.86–0.92.
- Steady-state analysis on an infinite slope: the screen-average k is about 1.0 for s < 0 and 1.25 for s > 0, against 1.30 at the centre.

**Sideways, diagonal and rotated camera** (zoom 540 / 120).
- Sideways along the contour of a falling ramp: g = 1, flow 1.00. The unweighted local variant gave 1.64–2.0.
- Sideways across a side ramp: 0.95–1.06, the same as before (only vertical bob).
- Diagonal up-right and down-left: 0.92–1.01, before 0.85–1.24.
- Camera rotated 30° over grade 0.3: 0.94–1.02, before 0.68–1.29. Over grade 0.6: falling 0.72–0.73 (capped), facing 0.91–0.97.

**Smoothness, overshoot and oscillation.**
- Tick-to-tick change in centre flow on grades up to 0.3: chord 0.00–0.02, N+F 0.1–0.4.
- The local central difference (d = 20/40/80, lo 0.5, hi 2) jumped 0.1–0.3 and reached 0.76–1.2 at ramp ends with the keyboard. At off 250/600 it reached 2.2–3.0 (1.46–2.98). The chord stayed at 1.00–1.13.
- Time series (ts_final2.txt): g is constant on a ramp (1.64 at s = −0.3, 0.56 at +0.6 with hi 2), with no oscillation.
- The current-HAG fix cuts the overshoot after a ramp from 1.19 to 1.06 (N alone). With the slope term at off 600 and zoom 120 it goes from 1.27–1.38 to 1.10–1.17.

**Fast drags** (off 600).
- Grade 0.3 falling, forward 1.07–1.12, back 0.91–0.93. Before 0.58–0.70.
- Grade 0.6 facing 1.05–1.15, before 1.27–1.50.
- One extreme case (grade 0.6 falling, zoom 120, off 600): a single bottom-corner pixel flow spikes to 44x for one frame (6x in N+F). This is the near-eye geometry, amplified by the boost.

**30/60 presentation (A halfway, B = M_k).** Ratios match 30 FPS (e.g. falling 0.3: 1.01/0.99; facing 0.6: 0.97–0.99). The two half-frames agree to within 0.01–0.03, and no cuts were triggered.

**Real maps** (16 AotR multiplayer maps; forward slope of the dilated grid at camera yaw 0).
- |s| ≥ 0.15: 10.9 % falling / 12.0 % facing of the area.
- ≥ 0.3: 5.7 % / 6.6 %. ≥ 0.55: 2.8 % / 3.6 %.
- Ramp cells where the grid floats more than 30 units above the terrain: 0–65 % depending on the map (Rivendell, Amon Hen and High Pass are high).
- Map settings: 517 of 604 maps have max camera height 540; 4 maps use smoothness 2.0 and 2 use 1.3.

**C++ check.** The helper built with MSVC x86 matches the Python reference to 1e-6 on 17 cases. Proposed tests: 5/5 pass. camera_patch.cpp compiles at /W4 against stand-ins for the repo helpers.

### Risks

1. **Off-centre speed (main tuning risk, INFERRED from the model).** Making the centre exact on falling slopes speeds up the bottom of the screen: about 1.3x on the lower half and a screen average of 1.27–1.35 at s = −0.3. On steep −0.6 slopes the upper half stays nearly still whatever we do. If the user finds falling slopes too fast, use k·0.8 (centre about 0.9, screen average about 1.2) or set lo to 0.67. Facing slopes are under-corrected on purpose (hi 1.4).
2. **Cliff "envelopes" (where the camera grid floats above low ground).**
   - When zoomed in (HAGd 50–120) and approaching a cliff from below, the visible ground is farther away than the grid, so stock was already near 1.0 there and the slope term makes it 0.59–0.90 (hi 2 would be 0.41–0.62).
   - The optional depth term (TerrainLogic getGroundHeight; its purity is INFERRED, not disassembled) fixes most of this and the falling-cliff case. Verify that function before using it.
3. **Steep grids.** Smoothness-2/1.3 maps (6 maps) and rotated cameras give directional slopes past −0.65. There the stock camera reverses or clips and the fade drops g back to 1. These remain stock-bad.
4. **Frame coupling.** The cave reads scrollBy's locals [ebp−0x34]/[ebp−0x30]/[ebp−8] and ESI, which ties it to this game.dat. Add verify bytes in sites.json for 0x48C8CF, 0x48C8D7 and 0x48C856. The |f|² and aspect guards return g = 1 on garbage.
5. **The height-above-ground change** (view+0x54 with the post-settle zoom instead of view+0x50) slightly changes the existing UniformScroll behaviour: no more stale-settle overshoot. view+0x54 is written in the same block as view+0x50 (0x48C24D) and also holds the fallback height on maps without a grid, but it is just as stale after a minimap jump. The existing clamp covers that.
6. **Camera state.** The real camera state changes (pos only; this is player scroll speed, the same class as the ScrollFactor or mouse-wheel settings). This is single player only, gated on isScrolling. No logic, RNG or save-format effect. Camera positions in saves and script camera-area conditions see a different, deterministic pan. It must stay excluded from compare_traces stock-equivalence runs, as with N+F.
7. **Feedback through the eye-height lag.** World step = nominal/(1 − G), with G ≈ 2.33·|s|·0.25·off·g/offz. Keyboard G ≤ 0.11; a right-mouse drag of about 600 at s = −0.6 gives G ≈ 0.65 (step ×2.9). This is bounded by grid-ramp length (dH/0.6) and the 4·maxHeight clamp, but a much faster drag on a long steep ramp could approach G = 1. If needed, cap g·factor per tick.
8. **Sampler edge.** Below the low edge the sampler is sawtooth-periodic; mitigated by clamping sample points to the grid's interior. Sample points stay within the step (up to about 150 units) of pos.
9. **Model limits.** The terrain is a 1-D ramp, extruded, at yaw 0 except one 30° case. The grid cell = max over 4x4 samples matches the earlier review (the alignment of the max window inside the builder is INFERRED). The camera's own extra pitch (view+0x2C) and FX pitch are assumed at defaults. View+0xA8 (zoom FX multiplier in getZoom) is assumed 1. The keyboard magnitude (off 100) comes from the earlier review. The "flow" metric is per-frame displacement of the real ground under each pixel; it differs from perceived speed at steps that are large relative to the height.
10. **Performance.** About 16 sampler calls per scroll tick; negligible.

## Independent verification

### Verdict

The design holds up with corrections. I re-disassembled every binary detail it relies on and all of them check out. An independent model, which runs the compiled C++ helper in the loop, confirms that on ramps up to grade 0.3 the formula brings the centre of the screen back to flat-ground speed (0.94-1.02) with no oscillation and no overshoot at the ramp ends.

Corrections needed:
- **Steep slopes stay slower than the design says.** On grade 0.6 slopes falling away from the camera the result is 0.36x of flat speed, not the claimed 0.42-0.55. The current tree gives 0.22x there, not 0.26-0.35.
- **Lower `lo` from 0.6 to 0.5.** This allows up to 2x speed-up instead of 1.67x.
- **Cap the sampled height at 5000**, the same limit the game applies to the look-at height.
- **Add a sanity check on the grid border value.**
- **Update the unit-test values** to match lo 0.5.

The proposed fix to the height-above-ground input (current zoom times max height, minus view+0x54, instead of view+0x50) is confirmed. Without it the slope term speeds up fast drags too much: 1.30-2.36x instead of 1.14-1.54x.

Nothing in the repo, the game or %APPDATA% was changed. My scratch files are in <analysis workspace>/camera_elev\slope\verify\: vsim.py, drive*.py, edge.py, tv.py, ssf.dll and the v_*.txt outputs.

### Corrections

**A. Binary details, re-checked by disassembly. All CONFIRMED unless marked otherwise.**

1. **Grid sampler 0x70FE22.**
   - Signature: `float __thiscall(grid, float x, float y)`. ECX is the grid; the caller passes `lea ecx, [view+0xB4+0x23A4]`, which is view+0x2458.
   - The arguments are pushed as x = view+0x0C into [esp] and y = view+0x10 into [esp+4] (0x48C1D1..0x48C1E3).
   - It ends with `ret 8` (callee pops both arguments) and returns the result in st0.
   - If the byte at grid+0x1C (= view+0x2474) is 0, it returns 0.0 via `fld [0xC1B594]`.
   - Index calculation: fx = (x + 10·[grid+0x18]) · (1/[grid+0x14]). It calls the floor import at 0xBD0580 and converts with fistp.
   - Edge behaviour:
     - The fraction is computed from the floor value before the index is clamped, so outside the low edge the result repeats every 40 units (sawtooth).
     - The index is clamped to [0, w-1] / [0, h-1].
     - When ix > w-2 or iy > h-2 it returns the nearest cell without interpolation.
   - Triangle interpolation is exactly as the design states. The function only writes its own argument slots and locals.
2. **Height block and settle step.**
   - 0x48C24D writes view+0x54. 0x48C252..0x48C266 computes view+0x50 = view+0x3C · view+0x23F0 − view+0x54, before the settle step at 0x48C35D..0x48C399.
   - The settle step is: desired = (view+0x40 + view+0x54) / view+0x23F0, then zoom += (desired − zoom) · GD+0xAB0 when |adj| ≥ 1e-4.
   - The scroll cutoff view+0x2404 is read at 0x48C314, and `ja 0x48C35D` jumps to the settle step.
   - So view+0x50 is one settle step stale. The height-above-ground fix is correct.
   - Minor correction to the design: 0x484FAB is called as cdecl. The caller pops 8 bytes (`pop ecx; pop ecx` at 0x48C24B).
3. **Camera offset setup 0x5011D5.**
   - Branch 1: offset = (0, −L·trig, L·trig) with L = maxH · 1.64267.
   - Default branch: z = maxH, y = −z/tan(p), x = −tan([esi+0x10])·y.
   - So k = |off.xy| / off.z is the cotangent of the effective pitch in both branches, as the design says.
4. **scrollBy 0x48C774.**
   - ESI = delta, loaded at 0x48C7A6 and untouched until 0x48C9B9. EBX = view.
   - Aspect at [ebp−8] comes from an integer division at 0x48C820 (vtbl+0x3C width / vtbl+0x44 height).
   - The forward vector is f = p2 − p1 from 0x488F7E, written by `fstp` to [ebp−0x34] / [ebp−0x30] (bytes D9 5D CC at 0x48C8CF, D9 5D D0 at 0x48C8D7). It is normalised only when |f|² ≠ 0.
   - Bytes at 0x48C856 are F3 0F 11 4D F8. Bytes at 0x48C959 are D9 43 3C D8 0D 04 19 BD 00.
   - The step is dX = (d.x·fy − fx·d.y)·S and dY = (−d.x·fx·aspect − fy·d.y)·S, which matches the C++.
5. **Not re-verified:** the 0x502858 eye geometry (I relied on the earlier review), the TerrainLogic vtbl+0x18 used by the optional phase 2, and the real-map slope statistics.

**B. Numbers corrected. Everything else in the design was confirmed within ±0.03.**
- Grade 0.6 falling, centre flow on the ramp:
  - Current tree (N+F): 0.21-0.24, not 0.26-0.35.
  - Design setting (lo 0.6): 0.35-0.41, not 0.42-0.55. The analytic value is 1.667 · 0.218 = 0.363.
- Camera rotated 30° over a grade 0.6 falling slope: 0.54-0.57, not 0.72-0.73. The analytic value is 1.667 · (1 − 1.303 · 0.52) = 0.54.
- **The design's diagonal numbers (0.92-1.01) only apply to ramps along the camera's forward direction.** Diagonal scrolling over a ramp that slopes sideways stays at 0.85-1.10, unchanged from the current tree. The term samples only along the forward direction; sideways slope shows up only as vertical drift.
- Pure sideways scrolling across a side ramp: 0.93-1.07, also unchanged.

**C. Parameter change: lo 0.6 → 0.5 (maximum speed-up 2x, exact for slopes from −0.384 upward).**
- The design picked 0.6 partly on the mean over the whole screen. That mean is dominated by the fast bottom rows.
- Flow increases steadily down the screen, so the speed of the middle row equals the speed of the centre point. That makes the centre the right target, and the cap should only limit the bottom of the screen.
- Keyboard scrolling, before → after (0.6 → 0.5):
  - grade −0.45: 0.70 → 0.86
  - grade −0.6: 0.36 → 0.45
  - 9-point screen median at −0.45: 0.90-0.94 → 1.08-1.12
  - low row at −0.45: 1.12-1.23 → 1.36-1.49
  - overshoot after the ramp at keyboard and edge speeds (100-250): unchanged at 1.02-1.13
- Cost at fast drags (right-mouse drag delta can reach about 2000 at 4K): peak after the ramp at drag speed 1000 goes from 1.22-1.37 to 1.33-1.51. At speed 2000 and HAGd 120 it goes from 1.87 to 3.02.
- For comparison, the current tree with only the height fix already gives 1.18-1.67 there. That is the settle zoom-in after a fast descent, not the slope term.
- I tested a cap on the loop gain (G = 0.25·|d|·scalar·g·2.33·|s|/offz ≤ 0.35 or 0.5) and rejected it. It removed the boost on drags (flow on the ramp fell back to 0.2) and barely reduced the overshoot.
- The design's "no oscillation" holds. G exceeds 1 for drags of 770 or more on −0.6 slopes, but the step grows together with the depth, so screen-centre flow on the ramp stays at 0.36-1.2.
- I made the cap a config value: `UniformScrollSlopeMaxBoost`, default 2.0, which gives lo = 1/boost.

**D. Code fixes.**
- The sampler wrapper returns min(h, 5000), the same clamp the game applies to the look-at height (0x48C213).
- It rejects a border outside 0..100000 or more than 1e6 grid cells.
- Unit-test values change for lo 0.5:
  - fall 0.45 / 0.6 / 0.65 → 2.0
  - fall 0.7 → 1.584948 (fading)
  - ramp-end chords → 1.156384, 1.390999, 0.736707
  - diagonals → 1.097705, 0.891022
- Unchanged values: flat and sideways 1, fall 0.3 → 1.641948 forward and back, facing 0.3 → 0.718924, facing 0.6 / 1.2 → 1/1.4, fall 1.2 → 1.
- The helper's maths, signs, bisection, polish step, fade, screen weighting, frame offsets and cave are correct as designed.
- The weighted residual g²(wr + wf·c²) is monotonic in g whenever 1 + k·(local slope) > 0. The equation therefore has a single root and bisection is exact.

**E. Edge cases checked in the helper.**
- NaN anywhere: returns 1.
- |forward step| ≤ 1e-3: returns 1.
- A sudden drop longer than the step: craw < fade, so it returns 1.
- A sudden rise: capped at 1/hi.
- Huge or tiny steps: same g.
- Grid invalid or no grid: returns 1.
- Map edges: sample points are clamped inside the interpolated area. pos itself is taken unclamped by the game.
- A minimap jump leaves the look-at height stale for one tick. h0 is sampled fresh at pos, so that one-off jump is never compensated.

### Final formula

**When it applies.** Single player, UniformScroll on, InGameUI isScrolling (ui+0x7F8), view+0x2474 ≠ 0, called at CAM_SCROLLNORM 0x48C959.

**Inputs.**
- f = ([ebp−0x34], [ebp−0x30]); skip if |f|² is outside (0.9, 1.1).
- aspect = [ebp−8]; d = ESI[0..1]; scalar = st0.
- off = view+0x23E8..F0, so k = |off.xy| / off.z and sinp = off.z / |off|.

**1. Height above ground (fixed).**
- hag = view+0x3C · view+0x23F0 − view+0x54. This uses the zoom after the settle step instead of view+0x50.
- factorN = (clamp(hag, 0.5·HAGd (or 1), 4·offz) + ref) / offz.

**2. World step before the slope term.**
- S = scalar · factorN · 0.25
- sx = (d.x·fy − fx·d.y)·S
- sy = (−d.x·fx·aspect − fy·d.y)·S
- δf = sx·fx + sy·fy
- δr = sx·fy − sy·fx

**3. Slope term g.**
- G(x, y) = min(0x70FE22(view+0x2458, clampX(x), clampY(y)), 5000). Sample points are clamped to [−10·border, −10·border + (n−1)·cell − 0.01].
- h0 = G(pos), with pos = view+0x0C/0x10.
- craw(g) = 1 + k·(G(pos + g·δf·f) − h0) / (g·δf). This is the chord over the compensated forward step.
- c(g) = clamp(craw(g), lo, hi), with lo = 1/MaxBoost = 0.5 and hi = 1.4.
- Solve g²·(δr² + (sinp·δf)²·c(g)²) = δr² + (sinp·δf)² for g in [1/hi, 1/lo] = [0.714, 2.0]:
  - 12 bisection steps;
  - then the polish step g' = sqrt((wr + wf) / (wr + wf·c²)), kept only if it stays inside the final bracket.
- Fade: if craw(g) < 0.15, g = 1 + (g − 1)·max(craw, 0)/0.15.
- g = 1 if |δf| ≤ 1e-3, on any NaN, or if the grid is invalid.

**4. Result.** factor = factorN · g. The cave then does factor · 0.25 · scalar as now.

**Pure forward or backward step:** g = 1 / clamp(1 + k·chord, 0.5, 1.4). That is exact for slopes along the camera forward from −0.384 to +0.307, at most 2x faster and at most 0.71x slower.

**Why it works.** Per step, screen-centre flow ∝ sinp·(g·δf + k·ΔgL) / (E − gL), and the sideways image term is g·δr / (E − gL). The equation above holds the screen-space speed at its flat-ground value. Because flow increases steadily down the screen, the centre is also the middle screen row.

### Model check

**How I checked it.** I wrote a new simulation in <analysis workspace>/camera_elev\slope\verify\: vsim.py, drive.py, drive2-5.py, edge.py and tv.py.
- New code: the simulation loop, the camera builder (eye = (pos, gL) − f·k·(E−gL) + z·(E−gL)), the raycast against the real terrain, projection and flow. Flow is the screen displacement of the real ground under each pixel, divided by flat ground at the same input.
- Reused from the design's model: only the World grid builder and sampler. Its triangle interpolation matches my disassembly.
- The slope factor is the design's C++ (slope_scroll.cpp) compiled as an x64 DLL (ssf.dll) and called through ctypes in the loop. Its results match the design's test values to within 1e-5.
- Update order per tick: scrollBy, then sample the grid at the new position (gL = min(Tc, 5000), view+0x54 = gL, view+0x50 computed before the settle step), then settle 0.3.

**Keyboard speed (100), centre flow on the ramp, min-max. Columns: current tree N+F → design (lo 0.6) → recommended (lo 0.5).**

| Case | N+F | lo 0.6 | lo 0.5 |
|---|---|---|---|
| Grade 0.15, falling | 0.80-0.81 | 0.98-1.01 | same |
| Grade 0.15, facing | 1.01-1.20 | 0.92-1.00 | same |
| Grade 0.3, falling | 0.60-0.62 | 0.94-1.02 | same |
| Grade 0.3, facing | 0.88-1.40 | 0.87-1.00 | same |
| Grade 0.45, falling | – | 0.69-0.71 | 0.83-0.90 |
| Grade 0.6, falling | 0.21-0.24 | 0.35-0.41 | 0.44-0.46 |
| Grade 0.6, facing | 1.39-1.82 | 1.00-1.27 | same |

- Grade 0.6 falling: the design claimed 0.26-0.35 for N+F and 0.42-0.55 for lo 0.6. Analytic values are 0.218, 0.363 and 0.436.
- The other rows were the same for HAGd 540, 120 and 50, both scroll directions, and smoothness 1 and 2.

**Other shapes and speeds.**
- Plateaus and valleys: grade 0.3 goes from 0.61-1.37 to 0.94-1.04. Grade 0.6 goes from 0.22-1.74 to 0.36-1.25.
- Tick-to-tick change in flow on grades up to 0.3: from 0.20-0.39 to 0.00-0.02. Entering a grade-0.6 ramp it is still 0.47-0.64, an unavoidable cap step.
- Overshoot after a ramp, keyboard: rec 1.00-1.03, versus N+F 1.00-1.03. Speeds 250 and 600: 1.03-1.16.
- Speed 250: grade 0.3 falling 1.01-1.06, facing 0.92-0.97.
- Speed 600: grade 0.3 falling 1.05-1.16, facing 0.88-0.96.
- **Height fix:** with the slope term and the stale view+0x50, fast drags give 1.30-2.36. With the fix they give 1.14-1.54, so the fix is required.

**Fast drags on a 900-high, grade-0.6 ramp.**
- Speed 1000: on the ramp 0.36-0.49 (lo 0.6) and 0.46-0.64 (lo 0.5). After the ramp 1.22-1.37 and 1.33-1.51. The current tree with only the height fix gives 0.20-0.24 and 1.18-1.27.
- Speed 2000: after the ramp up to 1.87 (lo 0.6) and 3.02 (lo 0.5) at HAGd 120. The height fix alone gives 1.67.
- The eye-height lag loop is stable in screen terms. The world step grows together with the depth, and the extra after the ramp is the settle zoom-in.

**Scroll direction and camera yaw.**
- Sideways along a contour: exactly 1.00.
- Sideways across a side ramp: 0.93-1.07, unchanged from N+F.
- Diagonal over a forward ramp: from 0.90-0.92 to 0.99-1.01.
- Diagonal over a side ramp: 0.85-1.10, uncorrected in both.
- Camera rotated 30°: grade 0.3 falling from 0.66-0.84 to 1.00-1.02; facing from 0.96-1.31 to 0.94-0.98. Grade 0.6 falling 0.54-0.57, not the design's 0.72.

**30/60 presentation (A halfway, B = M_k).** Same ratios as 30 FPS (grade 0.3 falling 1.00-1.01, facing 0.98-0.99), no extra tick-to-tick jumps.

**Grade 1.2 on smoothness-2 grids.** Stock geometry breaks there (the current tree reaches 277x at HAGd 120). The slope term with fade does not make it worse (213x), and at HAGd 540 the minimum rises from 0.02 to 0.58.

**Off-centre screen points, grade −0.3.** Upper row 0.64-0.68, lower row 1.25-1.35, 9-point median 1.09-1.14. Facing +0.3: upper 1.04-1.17, lower 0.77-0.88. This is the inherent trade-off of matching the centre.

**Helper edge probes.**
- Flat ground returns exactly 1.
- Slopes from −0.15 to −0.307 and up to +0.307 are exact: flow 1.000.
- −0.7 fades to 1.39 (lo 0.6) or 1.58 (lo 0.5). Slopes of −0.767 or steeper return 1.
- NaN anywhere, or a sudden drop: returns 1.
- A large or tiny step gives the same g.
