# AotR 60 FPS: drawables and client objects (notes)

These notes were meant to go to `an\findings\drawable.md`. Plan mode started partway through the work and blocks writes outside this file, so they live here instead. Helper scripts written before plan mode are in `an\findings\drw\`: pe.py, follow.py, callctx.py, field.py and modvt.py, plus decompile dumps.

Terms used below:
- **E** = TheGameEngine (`[0xDE4324]`). Stepper fields: E+0x34 = sub-frame, E+0x38 = frames per tick, E+0x3C = interpolation fraction.
- **GC** = TheGameClient (`[0xDE4388]`). `GC->vtbl+0x7C` is getFrame (m_frame at GC+0x10). **GL** = TheGameLogic (`[0xDE412C]`), with the logic frame at GL+0x40.
- **AUTO**: correct at 60 FPS with no extra work once the core model is in place (E+0x38 = 12, fraction = sub/12).
- **BLOCK**: reads the client half of the timing block (0xD9F60C int 30, 0xD9F624 = 0.03, 0xD9F62C = 0.0333) at runtime. It is correct only if those globals are switched to 60-FPS values.
- **FIXED**: a hard-coded per-frame step or count. It needs its own patch.

## 0. Frame order and cadence (verified)

- Engine vtbl (0xBD84E0) `+0x9C` = 0x632409 runs every rendered frame:
  - `GL->FUN_0062b385`.
  - If GC+0xC8 is set: `GC->setFrame(getFrame()+1)` (0x632423..0x632440). The **client frame therefore counts rendered frames**.
  - Then `GC->update` = FUN_0064849e (0x632498).
- Then the stepper does `sub++`, sets the fraction (FUN_0063256f), and calls `vtbl+0x98` = 0x6329b0 (GameLogic `vtbl+0x34`(sub); logic runs at sub==1).
- GameClient::update (FUN_0064849e) does the following:
  - Freeze check: `DAT_00d9f6f8 == m_frame` skips the drawable work.
  - `local_6 = FUN_0063252f()` is true on the first sub-frame after a logic tick.
  - For every drawable (first = `GC vtbl+0x44`, next = Drawable+0x104): on the first sub-frame it runs FUN_0068d8f7 and FUN_00678f54 (shroud and visibility). On **every frame** it runs `Drawable::updateDrawable` = FUN_00675996.
- Drawable::draw = FUN_0067c482 is reached from the W3DView::update callback 0x485329, pushed at 0x48c738.
  - That call is gated by `if (SyncTime[0xDD1E0C] - PrevSyncTime[0xDD1E10])` (0x48c701..0x48c70c, `je` skip). This is the Generals `Get_Frame_Time()!=0` gate.
  - It is also called for attached sub-drawables from W3DScriptedModelDraw::doDrawModule (0x4c77f4).
  - If the W3D sync does not advance on some rendered frame, no drawable is re-posed on that frame. **The W3D clock fix must advance SyncTime on every rendered frame.**
- W3D sync (FUN_0044b788) adds `DAT_00dc7580 += DC7A8C * (clientFrame delta)`.
- **Do not hold GC m_frame at 30 Hz.** Three things key on the client frame:
  - The interpolated-transform cache (FUN_006765b9, 0x6765de, cache tag Drawable+0x244).
  - The instance-matrix cache (FUN_0067171d, +0x204).
  - The client-physics once-per-frame gate (FUN_0067bdc0, 0x67be04).

  The W3D sync advance is also keyed on client-frame deltas. Halving the counter would freeze interpolation on alternate frames and blank the physics xform. Keep the client frame at 60/s and fix systems one at a time, as listed below.

## 1. Interpolation (AUTO)

- Keyframes: GameLogic::update(sub) (FUN_0062e4e8), when sub==1 and after logic runs, loops over objects and calls `drawable->FUN_00674b1f(0)`. That function sets:
  - prev matrix +0x3AC = object prev transform (obj+0x158 / +8)
  - cur matrix +0x3DC = object transform
  - Catmull-Rom points: p0 +0x40C = obj prev pos (+0x18C), p1 +0x418 = prev translation, p2 +0x424 = obj pos, p3 +0x430 = obj predicted pos (+0x198)
  - +0x444 = 1, and cache +0x244 = -1
- Consumers of E+0x3C:
  - FUN_006765b9 (getTransformMatrix): matrix lerp FUN_00b27c80(prev, cur, t) plus D3DXVec3CatmullRom(t). It snaps if obj+0x3A4 < logicFrame-2.
  - FUN_00676711 (Catmull-Rom position) and FUN_0067679b (linear position).
  - FUN_0067171d: instance-matrix blend (+0x170 to +0x1A0). Its window `+0x378 = getFrame + E+0x38` is set in FUN_006714c0 at 0x6716dd. Used by SwayClientUpdate.
  - FUN_004b51b5: construction-progress animation frame, `t*(1/buildFrames) + pct*0.01`.
  - FUN_004b686d: build-up height offset.
  - FUN_004b6f12: turret/barrel. It shifts prev/cur when fraction == 1.0 and lerps otherwise. At 12 sub-frames, 1.0 is reached at sub 12, so this still works.
  - FUN_0048e0ee: screen overlay alpha.
  - FUN_008a0365: int counter extrapolation.
- No hard-coded 6 appears in any drawable path. The only 6-assumptions are in the stepper (0x632604, 0x63264a), FUN_0063252f's `<6` branch, and 0x632aa3. These are already known.
- Consumers of E+0x38 (AUTO):
  - SwayClientUpdate FUN_008cd55f (phase step = 2π·rand / (E+0x38 · breezePeriodLogicFrames))
  - W3DTruckDraw wheels (0x4cc4b9)
  - FUN_006714c0
  - Fade-from-logic-frames call sites 0x85eea1, 0x85ef14, 0x860d43
  - InGameUI floating text 0x69dbc9, 0x69dcd0
- GameLogic benchmark FUN_0062c159 also reads E+0x38 (QPC perf test). Flag it to the logic agent.

## 2. Drawable::updateDrawable (FUN_00675996), per rendered frame

| system | fields | mechanism | at 60 | fix |
|---|---|---|---|---|
| fade in/out | +0x128 state, +0x12C elapsed, +0x130 total, +0x37C last client frame, +0xB0 opacity | elapsed += getFrame delta (0x675ac3..0x675aff). Setters: fadeOut FUN_00670a50, fadeIn FUN_00670aa2, FUN_0067309d | 2x for most callers | See §3 |
| decal opacity fade | +0xE4 enable, +0xF4 value, +0xF8 rate | +0xF4 += +0xF8 per call. Rate 0.03 set in the ctor at 0x679b64 (`movss xmm0,[0xBDC540]`) | 2x | Repoint the operand to 0.015. 0xBDC540 is shared by 10+ sites, so do not edit the constant |
| shader-effect progress | +0xA8 type, +0xDC rate, +0xE0 progress; drawModule0 vtbl+0x50/+0x58 | `progress += max(1, delta)*rate` (0x675c51..0x675ca9). Setter FUN_0067093e. Callers: 0x67561e (-0.03 @0xBDC538), 0x860f5d (-0.2 @0xC2E198), 0x8889e9 (+0.03), virtual FUN_00870d4e (param) | 2x | Replace FUN_0067093e so it stores rate*0.5 |
| flash | +0x168 count, +0x16C color | `if (getFrame % DAT_00de45d8 == 0) colorFlash(c, 4, 0, 0)` at 0x675d5b. DAT_00de45d8 = CLIENT_FPS/2 from static init 0xbc32b1 | 2x | Set DAT_00de45d8 = 30 after CRT init, or rely on CLIENT_FPS=60 before CRT |
| status tint | +0x118/+0x11C bits, TintEnvelope +0x68 | play(color, 30, 30, sustain -2/300/9999) at 0x675dc6, 0x675e0e, 0x675e78. Custom tint at 0x675ec1 | 2x | Fix TintEnvelope centrally (§4) |
| tint updates | +0x68, +0x64 | FUN_006748b7 per call | 2x | §4 |
| opacity pulse (stealth-like) | +0xB4..+0xD4 | FUN_00673114: blend counter +0xD4 counts down from `CLIENT_FPS*period`; phase += (0.0333/period)·π. Setter FUN_006760f9 (period in s, clamped 0.1..60) | BLOCK | If the block is unchanged, repoint 0x6731ca, 0x673289, 0x676151 (`FILD [0xD9F60C]`) to int 60 and 0x673222 (`MOVSS [0xD9F62C]`) to 1/60 |
| client update modules | +0x150 list | `if (!m->vtbl2C()) m->vtbl30()`, or all of them when +0x441 is set | Sway: AUTO. APSBone: m_life++ (harmless). Beacon: logic frames. RadarMarker: position only | none |
| expiration | +0x350 | Compared to logic frame | OK | none |

## 3. Fade durations: call-site units

The fade timer counts client frames. Callers pass:

- **Immediates (30-Hz frames):**
  - 0x7954a8 `push 0x8a`
  - 0x79b05e and 0x79b09d `push 0xa`
  - 0x858330 `push 0xa` (fadeOut)
  - 0x8583c9 `push 0x1e`
  - 0x8aee86, 0x8b1513, 0x8b2392 `push 0xa`
- **LTR used as frames:**
  - 0x664ff2 and 0x665031 `push [0xD9F608]` (5 frames)
  - 0x87d65c `LTR*5`
- **Logic-frame INI values used as client frames:**
  - BirthFadeTime: W3DScriptedModelDraw data+0x150, parser FUN_0073a429 = ceil(ms·0.005). Used at 0x4c0a99.
  - GameData BuilderFadeInTime +0x11E0 at 0x88d79e, and BuilderFadeOutTime +0x11DC at 0x88ddd4.
  - Stock fades therefore last 1/6 of the INI time.
- **BLOCK:**
  - FILD CLIENT_FPS·s at 0x798af0, 0x889c3d, 0x8a2d40
  - 0.03·ms at 0x862c2e, 0x86ab63, 0x86aba1, 0x86ae59, 0x86ae87, 0x8a6080, 0x8a60c0, 0x8b864c
- **AUTO (E+0x38·logicFrames):** 0x85eea1, 0x85ef14, 0x860d43
- **Unknown (passed through):** 0x5f170a, 0x5f1727, 0x5f178a (`[EBX+0x78]`) and 0x79cb98, 0x79cbc8, 0x79cbf8 (param)

Recommended fix (F2): run the fade clock at 30 Hz. Replace the 11-byte getFrame pattern `8B 0D 88 43 DE 00 8B 01 FF 50 7C` with `call helper_frame_half; nop×6`, where the helper calls getFrame and then does `shr eax,1`. Apply it at:

- 0x670a8d (fadeOut)
- 0x670adf (fadeIn)
- 0x6730cc (FUN_0067309d)
- 0x675ac3 (update)

After that:
- Immediates, LTR, logic-INI and unknown callers are correct.
- The AUTO callers (0x85eea1, 0x85ef14, 0x860d43) need a constant 6 in place of E+0x38.
- The BLOCK callers need patching only if the block is switched to 60.

## 4. TintEnvelope (pool "TintEnvelope", object size 0x50, vtable 0xC11078)

- `play` FUN_00674818(color, attack, decay, sustain) calls FUN_00671c25 setAttackFrames(n) (rate = Δ/n) and FUN_00671cba setDecayFrames(n) (rate = -peak/n).
- `update` FUN_006748b7 (one caller, updateDrawable) has states:
  - 1: attack, rate per call
  - 3: sustain, `--counter`
  - 2: decay
- Users:
  - colorFlash FUN_00675749
  - flashAsSelected FUN_00678fa7: play(c, 0, 4, 1) on +0x64
  - FUN_006758c3
  - status tints (§2)
  - colorFlash callers 0x81e234 (8), 0x871b24, 0x871b6b, 0x8a02a3 (4, 4, 15), 0x6757cf (sustain -2)
- Fix:
  - In FUN_00671c25 and FUN_00671cba use 2n (keep n=0 → 1 → 2).
  - In FUN_00674818 double param_5 when it is > 0. Negative values mean "forever" and stay as they are.
  - With this, every frame-count caller is correct.

## 5. Drawable::draw (FUN_0067c482)

- +0x358 (second material pass / "detected" flash) `*= 0.8` per draw at 0x67c4c7 (`F3 0F 59 05 D8 E8 BD 00`). Repoint it to sqrt(0.8) = 0.894427 (0x3F64F92E). 0xBDE8D8 is shared, so do not edit the constant.
- FUN_0067c1fb gives the transform: the interpolated xform (FUN_006765b9), times the instance matrix (FUN_0067171d), then applyPhysicsXform FUN_0067be90.

## 6. Client physics xform (pitch/roll/wobble/suspension)

- FUN_0067be90 builds a zeroed local {pitch, roll, yaw, z} struct and calls calcPhysicsXform FUN_0067bdc0.
  - FUN_0067bdc0 returns 0 if locomotor(obj+0x260 → +0x1F0)+0xAC == getFrame (0x67be04); otherwise it stores getFrame.
  - It dispatches on locomotor appearance (+0x74): 1/8 → FUN_00677026, 2/3 → FUN_00670b11, 4 → FUN_0067b7aa, 5 → FUN_00670e09, 9 → FUN_006732dc.
  - The whole path is gated by GameData+0x9B0 (ShowClientPhysics) and pause checks.
- State lives in DrawableLocoInfo (Drawable+0x13C, 0x58 bytes, ctor FUN_00670392). Spring-damper steps run once per client frame:
  - e.g. FUN_00670b11: `rate += -k·x - d·rate; x += rate·uniformDamping`
  - FUN_00670e09: `x = x*0.8 + accel·k`
  - FUN_006732dc: wobble phases +0x34/+0x38 += loco+0x120/+0x118 per frame
- FUN_006732dc also scales per-tick displacement (p2−p1) by LTR/CLIENT_FPS (0x67349c / 0x673494).
- At 60 FPS every oscillation runs 2x fast. Fix:
  - Integrate at 30 Hz and replay the cached output on the other frames. Hook FUN_0067bdc0: on even client frames call the original and store *out; on odd frames set loco+0xAC = frame, copy the cache to out, return 1.
  - Cache storage: enlarge the alloc `push 0x58` → `push 0x6C` at 0x670b28, 0x6732f8, 0x677042, 0x67a8e0 (Drawable::xfer), 0x67b7c6, and use +0x58..+0x6B.
  - If CLIENT_FPS is set to 60, repoint 0x673494 to int 30 so the 30-Hz step stays valid.

## 7. Draw modules

The module vtables were found through FUN_00464ac1 → factory → ctor. doDrawModule is at vtbl+0x2C.

| module | vtbl | doDrawModule | per-call stepping | at 60 | fix |
|---|---|---|---|---|---|
| W3DScriptedModelDraw (also Horde, Quadruped, Supply, Sail) | 0xBDFFE8 | 0x4c72be | see rows below | | |
| (same) transform blend | | FUN_004b686d | +0x2C0 += 1/30 (`.rdata 0xBDFC6C`) at 0x4b6b63, once per client frame | 2x | Repoint the operand to 1/60. 0xBDFC6C is shared with 0x5033e5, 0x765e07, 0x80b088 |
| (same) birth colour fade | | | +0x9C..+0x9F: elapsed += getFrame delta (0x4c76f6); total = BirthFadeTime in logic frames | 2x | getFrame>>1 at 0x4c0a8e and 0x4c76f6, as in F2 |
| (same) recoil | | FUN_004b4699, last call | shift += rate; rate *= RecoilDamping(+0x5C @0x4b47c1); settle -= RecoilSettleSpeed(+0x60 @0x4b4785). InitialRecoilSpeed +0x54. Speeds parsed by FUN_0073a4b6 (×0.2) | 2x | Data fix-up: d' = √d, settle' = settle/2, init' = init/(1+√d). Field tables: 0xBE1320/0xBE1340/0xBE1350. Or call FUN_004b4699 on even frames only |
| (same) turret, construction, build-up | | | fraction | AUTO | |
| (same) anim state machine | | FUN_004bfaf4/FUN_004bf560 | W3D sync-ms deltas; FUN_004b67d4 uses 200 ms/logic | correct if the W3D sync is in real ms | |
| (same) terrain decal | | FUN_00732926 | see §8 | | |
| W3DTankDraw | 0xBE2A80 | 0x4cdbd7 | tread UV `frac(uv - TreadAnimationRate)` per draw (0x4cddcb), pivot FUN_004cd7bc. Rate = INI·0.2 (tbl 0xBE2988, parser 0x73a4b6) | 2x | Swap the parser to store ·0.1 |
| W3DTruckDraw | 0xBE24E8 | 0x4cbffb | wheels: AUTO (E+0x38). Cab/trailer `x += (target-x)·RotationDamping(+0x1E4)` per draw | cab 2x | Parser swap at tbl 0xBE2448: r' = 1-√(1-r) |
| W3DLightDraw | 0xBE36D8 | 0x4cf2b5 | `t += 1/LTR` per draw (0x4cf2bb); flicker countdown = LTR/rate (0x4cf3cf), -1 per draw | 2x | Repoint both cvtsi2ss operands from [0xD9F608] to an int 10 constant |
| W3DRopeDraw | 0xBE1CC0 | 0x4ca481 | wobble += rate; z += speed; speed += accel per draw | 2x | Halve, or gate to 30 Hz. Rarely used |
| W3DBoatWakeModelDraw | 0xBE3D50 | 0x4d052e | ratio slew 0.005 per draw (global 0xD9A3A0) | 2x response | Repoint to 0.0025 |
| W3DDebrisDraw | 0xBDF700 | 0x4b135c | +0x3C counter (<4) | negligible | |
| W3DLaserDraw | 0xBE1938 | 0x4c8a6c | geometry only | OK | |
| W3DFloorDraw | 0xBE3378 | 0x4cebca | death fade sent to the floor manager (0xDC78EC → FUN_004e5a5b, which uses E+0x38) | other agent | |
| Default/Tree/Prop/Buff | | 0x9f3a3c (ret) | | n/a | |
| W3DHordeModelDraw | 0xBDC810 | +0x18 0x47a043; FUN_00479822 | shared render object freshness by client frame | OK | |
| ProjStream / Tornado / Streak | | 0x4d0d05 / 0x4d0fcb / 0x4cfc54 | No integrator found. Tornado calls FUN_00732926 | | |

## 8. Selection and terrain decals (FUN_00732926; fields Texture, Style, OpacityMin/Max, OpacityThrobTime +0x14, RotationsPerMinute +0x2C, MaxRadius, MinRadius, SpiralAcceleration +0x30; table 0xC24278)

- Throb: period = max(1, ceil(0.03·ms)) client frames (0x73297a) and phase = getFrame % period. BLOCK.
- Rotation: angle = getFrame · (0.0333·deg2rad·RPM·60) (0x732b4f). BLOCK.
- Spiral: value -= SpiralAcceleration per draw (0x732a84). FIXED. Halve it.

## 9. Icons and UI on drawables (FUN_00679a04 passes)

- Anim2D frame advance FUN_006d7f0a: `getFrame - last >= delay` (0x6d7f18/0x6d7f1b). The delay comes from AnimationDelay, parsed ms → **logic** frames (FUN_0073a46f, tbl 0xC195C0), but it is compared against client frames. At 60 FPS animations run 2x fast.
  - Fix: replace the 6 bytes `2B 46 08 3B 46 18` with `call cave; nop`, where cave = `sub eax,[esi+8]; shr eax,1; cmp eax,[esi+0x18]; ret`. `ret` keeps the flags, so the following `jb` still works.
- Blinking group/number text FUN_00677c0c: `((getFrame + n) & 10)` at 0x677ca3. Use frame>>1 at 0x677c98 (11-byte pattern).
- Damage icon FUN_006740f5 uses logic frames (DAT_00de45f0 = LTR*3). OK.
- Health bars and pips FUN_00673b2e and FUN_00677994: no timing found.

## 10. Other notes

- No timeGetTime or GetTickCount calls in the Drawable or draw-module code.
- AotR sections .danetta/.angmar make no direct E8 calls to the fade, tint, progress or pulse setters.
- None of the patch sites above overlap AotR's patched bytes.
- Hazard for setting CLIENT_FPS=60 globally: the shader time callbacks at 0x48017a..0x481235 compute `syncTime·0.001·[0xD9F60C]`, so their time would double. Keep 30 there.
- FUN_004d1acd refreshes every 10 client frames (0x4d1dfb). At 60 FPS this is just twice as often (performance only).
- Drawable::xfer FUN_0067a544 persists some cosmetic client state, including DrawableLocoInfo. This has no effect on logic compatibility.
