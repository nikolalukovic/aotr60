# Particles and client FX at 60 FPS: findings and plan (AotR game.dat)

During the analysis, helper scripts were created under
`an\findings\ptx\` (pdd.py, pscan.py, pfields.py, bytes.py, gcw.py, vcalls.py, gf.py), along with
`an\findings\refs_de3744.txt` and `an\findings\fieldscan.py`.

## 0. Globals identified (from GameEngine::init at FUN_0063ad4f)
- TheFXParticleSystemManager = [0xDE3744] (W3DFXParticleSystemManager; vtable 0xBDAC10, ctor 0x44C67D)
- TheGameClient = [0xDE4388] (W3DGameClient vtable 0xBDA6E0). vt+0x7C = getFrame = `mov eax,[ecx+0x10]` (0x9F9F1A), vt+0x38 = setFrame
- TheGameLogic = [0xDE412C] (+0x40 logic frame, +0x125 paused)
- TheGlobalData = [0xDE4364]; TheDisplay = [0xDE4418] (W3DDisplay vtable 0xBD9C28: +0x30 draw = 0x44B788, +0xBC createLightPulse = 0x443B5D)
- TheTacticalView = [0xDE447C]

## 1. Particle update cadence: one update per rendered client frame (verified)
- GameEngine vt+0x9C (0x632409) runs once per frame from the stepper at FUN_006325a0. It does:
  `if (TheGameClient+0xC8) TheGameClient->setFrame(getFrame()+1)` (0x632423..0x632440), then calls GameClient::update (0x64849E).
- GameClient::update calls TheDisplay->draw (outer 0x44B788). The outer draw calls the inner draw FUN_00449CF8 once in normal play. The inner draw calls TheFXParticleSystemManager->update() (vt+0x28 = **FUN_005F5123**) at **0x449D40..0x449D48**.
- A second call site exists in the W3DDisplay vt slot +0x188 stub at 0x444CD4..0x444CFA. It ends with `jmp [eax+0x28]` on the manager. Its caller is unresolved, possibly a loading-screen path.
- Manager update 0x5F5123:
  - It has a client-frame gate, but the gate is only active when `GlobalData+0xD45` is set (0x5F5168 cmp, **0x5F516E `74 1F` je**). That flag is set only by the Create-a-Hero screen (writers 0x91A7A4, 0x919FB6, 0x6435AF), so the gate is OFF in normal play.
  - Gate body: `f = TheGameClient->getFrame(); if (f == this+0x74) return; this+0x74 = f`. Reset FUN_005F50D4 clears +0x74.
  - For each system in list +0x4C: if `sys+0x98 != 0 || (TheGameClient+0xC8 && !TheGameLogic+0x125)` then `sys->vt+0x10(localPlayer)`. +0x98 marks an "update even when paused/halted" system.
- Result: particles advance once per W3DDisplay::draw, which is 30/s in stock. That is 6 particle updates per 5 Hz logic tick. At 60 FPS they would run 2x fast.

## 2. Simulation chain (all per update, verified)
FXParticleSystem vtable 0xBF7B48 (ctor 0x5FBE28), update vt+0x10 = **FUN_005FA2CE**:
- +0x120 InitialDelay countdown: `--` per update. When it reaches 0, +0x124 = getFrame() (0x5FA32A..0x5FA335).
- The emitter follows a drawable/object transform each update. Previous position is kept at +0x150 and current at +0x144.
- Emission FUN_005F3CCF:
  - +0x11C burstDelayLeft. When 0, emit `count = (int)(rand(BurstCount) * this+0x13C)` via storage vt+0x24, then reset to rand(BurstDelay). Otherwise `--`, or set to 1 if IsOneShot.
  - Burst period = BurstDelay+1 updates. Slave systems are chained via +0x15C.
- Per-particle placement FUN_005F4C6B spreads a burst along (curPos-prevPos)·(1-i/N).
- System modules: the chain at +0x1AC.. (FUN_005F9F6D) calls module vt+4.
- Alive check FUN_005F332E:
  - calls storage vt+0x28 (particle update loop);
  - if not forever: +0x128 SystemLifetime `--` per update;
  - the system dies when storage is empty and lifetime is 0.
- Storage modules:
  - CPU 0xC33AE8: vt+0x28 = 0x7B09A1 walks particles (next ptr +0x64) and deletes p when `!FUN_005FA0E6(p)`.
  - GPU 0xC33B48 (type 7) and TerrainFire 0xC33BA8 (type 8): vt+0x28 = FUN_007B12F4.

FXParticle (ctor FUN_005F41D5, update **FUN_005FA0E6**). Fields:
- +4 accel; +0x10 vel; +0x1C pos; +0x28 origin; +0x34 lifetime; +0x54 lifetimeLeft
- +0x58 creation client frame (0x5F426D..0x5F427A); +0x64 next; +0x88 particle id

Update: run the per-particle module chain (+0x94), then `if (+0x54 && --(+0x54)==0) die`.

Per-particle module vtables (vt+4 = update):
| module | vtable | update | per-update math |
|---|---|---|---|
| DefaultParticle PHYSICS | 0xC32B18 | 0x96AAD2 | accel.z = sys gravity; vel += accel; **vel *= VelocityDamping**; pos += vel + drift; Swirly: pos += dir·|vel|·4.0 (0xBD88C0); accel = 0 |
| DefaultParticle UPDATE | 0xC32BD0 | 0x96AF99 | size += sizeRate; **sizeRate *= SizeRateDamping**; angleZ += rateZ; **rateZ *= AngularDamping**; angleXY += rateXY; **rateXY *= AngularDampingXY** |
| DefaultParticle ALPHA | 0xC329B0 | 0x969A94 | alpha += rate (clamped 0..1); key i reached when `key.frame <= lifetime - lifetimeLeft` (update count); rate = (v_next-alpha)/(f_next-f_cur) (0x969978) |
| DefaultParticle COLOR | 0xC32A64 | 0x96A0F9 | rgb += rate; same keyframe rule (0x96A079) |
| DefaultParticle WIND | 0xC32C80 | 0x966DFF | pos += WindStrength·atten·(cos a, sin a); turbulence uses id(+0x88)+z·freq (both terms add to x, a stock bug) |
| Wind system module | 0xC32C48 | 0x9669D8 | wind angle ping-pong/circular += rate per update |
| RenderObjectParticleUpdate | 0xC32DB0 | 0x965DA7 | size xyz += rate; **rate *= damping** xyz; angle += rate; **rate *= damping** |
| ParticleLifeEvent | 0xC32D08 | 0x96B7BD | fires when `getFrame() - p+0x58 >= EventTime` (GameClient frames) |
| LifeEvent (system) | 0xC32CD8 | 0x96B8E5 | fires when `getFrame() - sys+0x124 >= EventTime` |
| ParticleTerrainCollision | 0xC32E60 | 0x96C01B | state-based (z <= terrain height), no timing |

Draw modules (vt+0x10) only copy state into W3D buffers; no per-frame state:
- Default 0x961DFC, Streak 0x9624B9 (sorted by id), Quad 0x963481, Butterfly 0x963DA0, RenderObject 0x964C00, Lightning 0x962A31, GPU 0x9658BA.

Render: W3DFXParticleSystemManager vt+0x40 = 0x44C84A. Draw modules are dispatched at 0x44CB9E and 0x44CC8C. The "SMUD" heat-haze path draws client-random jitter on every render call (cosmetic only).

## 3. INI units and parse-time conversion (verified)
- The FXParticleSystem INI is module-based (fxps*module.cpp).
- Field tables:
  - System 0xBF2E70-style list built in FUN_005F33C2
  - Physics 0xC83C20, Update 0xC83CB0, Alpha 0xC83A70, Color 0xC83B40, Wind 0xC83058
  - RenderObjectUpdate 0xC82E40, LifeEvent 0xC83DA0, TerrainCollision 0xC83E48, Lightning 0xC838C0
- Parsers are parseReal 0x42ED00, randomVar 0x73A396, uint 0x42ECB2/0x42EC5E, and keyframes 0x969878/0x969F88.
- **No particle field uses the timing globals.** All values are raw per-update rates/dampings and raw update counts: Lifetime, SystemLifetime, BurstDelay, InitialDelay, keyframe frames, EventTime.
- Exceptions, where conversion happens in FXList at spawn time rather than at INI parse:
  - FXList `ParticleSystem` nugget (field table 0xBF35A8):
    - InitialDelay(ms) → `ceil(ms·0.005 [0xD9F610])` = LOGIC frames, written into sys+0x120 (0x5E1CDF..0x5E1D25).
    - That counter is decremented per client update, so stock delays are 6x shorter than the INI ms value. This is a stock quirk and must be preserved.
    - SystemLife (raw) → sys+0x128.
  - GPU storage uses W3D sync time: `now = syncMs[0xDD1E0C] · CLIENT_FPS[0xD9F60C] · 0.001`, in 30 Hz frame units, at 0x7B1102, 0x7B1160, 0x7B12FE, 0x7B140B. CLIENT_FPS must stay 30 at these sites, or they need a constant 30, and sync time must remain real milliseconds.

## 4. Recommended design (Option A): keep particle simulation at exactly 30 Hz
Run the manager update on every other rendered frame: 6 updates per logic tick, as in stock. Render the same state twice. This makes the particle state bit-identical to stock, including the client-RNG sequence. Damping, keyframes, bursts and wind need no maths changes.

Use the even sub-frames (sub 2,4,..,12 of the 12-sub-frame tick). Emitter transforms sampled from interpolated drawables are then also bit-identical, because (float)2/12 == (float)1/6.

### A1: if the core keeps GameClient::m_frame at 30 Hz (increment every 2nd rendered frame; recommended for save compatibility too)
- Patch **0x5F516E: `74 1F` → `90 90`**. This forces the existing "client frame changed" gate, so updates happen once per 30 Hz client frame.
- No other particle patch is needed. LifeEvents, creation stamps and decal deadlines all stay in 30 Hz units.
- Caveat: systems with +0x98 (update-while-halted) will freeze while the client frame is frozen. If that matters, use the A2 counter for the gate instead.

### A2: if GameClient::m_frame becomes 60 Hz
Give the DLL a proxy object whose vtable slot +0x7C returns a 30 Hz "particle frame". That value can be a render-frame counter divided by 2, which keeps advancing while paused just as stock does.

Retarget the 4-byte address operand of each `mov ecx,[0x00DE4388]` in particle code to `&g_pParticleClockProxy`:
- 0x5F5172 (manager gate, plus NOP 0x5F516E as in A1)
- 0x5F426F (FXParticle +0x58 stamp)
- 0x5FC082 (system ctor +0x124)
- 0x5FA32C (delay-end +0x124)
- 0x96B7CF (particle LifeEvent age)
- 0x96B909 (system LifeEvent age)

The original bytes at each site are `8B 0D 88 43 DE 00`.

Alternative: hook the call at 0x449D40 (`8B 0D 44 37 DE 00 / 8B 01 / FF 50 28`) and 0x444CF2, and call through only on particle-tick frames.

Save-game caveat: +0x58, +0x124 and m_frame may be serialized (xfer around 0x5FAA2E). Mixing 30 Hz and 60 Hz frame units breaks LifeEvent ages after loading a save. A1 avoids this.

### Optional smoothing (cosmetic, no state change)
Patch the draw-module vtable +0x10 data entries to DLL wrappers:
- 0xC326C8 (Default), 0xC32734 (Streak), 0xC32794 (Quad), 0xC327F0 (Butterfly), 0xC32854 (RenderObject), 0xC328B4 (Lightning). GPU (0xC3291C) is not needed.

On in-between frames the wrapper:
1. walks `this+4` system → storage+0xA4 vt+0x20 → next ptr +0x64;
2. adds 0.5·(vel+drift) to pos;
3. calls the original;
4. subtracts the offset again.

Particle state stays untouched.

## 5. Option B (not recommended): half-step every update at 60 Hz
Every per-update term needs a transform, for exact match at even steps:
- **Linear accumulators** (alpha/colour rate, size rate with no damping, wind strength, wind angle rates, drift): halve.
- **Integer counters**: Lifetime, SystemLifetime, InitialDelay (also the FXList-converted one), keyframe frames and EventTime double. BurstDelay must become 2·BD+1, because the period is BD+1.
- **Exponential terms** (`x+=r; r*=d`: SizeRateDamping, AngularDamping, AngularDampingXY, RenderObject damping xyz): d' = √d and r0' = r0/(1+√d). The initial rates are drawn at module construction, so every module ctor needs patching.
- **Physics** (`v=d(v+g); p+=v+w`): e=√d, h = g·√d/(2(1+√d)), w' = w/2. The initial velocity must be remapped to u0 = (1-e)/e·[v0·d/(1-d) − g·d²/(1-d)² + h·e²/(1-e)²]. For d=1: h=g/4 and u0 = v0/2+g/8.
- **Swirly** (|v|-proportional) and turbulence cannot be matched exactly.

This needs dozens of patches across ctors and updates, and the client RNG sequence would diverge.

## 6. FXList nuggets (keyword table 0xBF2898)
| nugget | fields / units | runtime consumer | 60 FPS note |
|---|---|---|---|
| ParticleSystem / ParticleSysBone / FXListAtBonePos / CursorParticleSystem | see §3 | particle system | covered by Option A |
| LightPulse (0xBF2D50) | IncreaseTime/DecreaseTime via 0x73A429 = ceil(ms·0.005) LOGIC frames | W3DDisplay::createLightPulse 0x443B5D → light setup 0x46D7A9 (+0x14C/+0x154 inc, +0x148/+0x150 dec) | decremented per render by W3DDynamicLight On_Frame_Update **0x46D665** (vtable 0xBDBFD8 slot +0x34), called from RTS3DScene render 0x46FF57. Stock pulses are 6x shorter than the INI ms. At 60 FPS, double the counters in 0x46D7A9 or skip odd frames in 0x46D665 |
| DynamicDecal (0xBF2E70) | Opacity*Time, StartingDelay, Lifetime (ms) × 0.03 [0xD9F624] at 0x5E0776.. → client frames | deadlines = getFrame()+n in 0x7323CE | fine if m_frame stays 30 Hz; otherwise needs 0.06 or a 30 Hz clock |
| ViewShake (0xBF3974) | Type | TheTacticalView vt+0x1AC (W3DView legacy shake, per-frame damping) | camera domain |
| CameraShakerVolume (0xBF31C0) | Radius, Duration_Seconds, Amplitude_Degrees (raw) | camera shaker | camera domain (Timestep 1/30) |
| AttachedModel (0xBF3298) | ExpireTimer raw uint, default LTR·8 (ctor 0x5DFF7A) | draw module vt+0x8C (0x67654E) | drawable domain; units unverified |
| TintDrawable (0xBF33F8) | Pre/Post/SustainedColorTime raw uint (default 2000), Frequency, Amplitude | Drawable tint 0x671A73 | drawable domain |
| BuffNugget (0xBF3038) | BuffLifeTime via 0x73A429 (logic frames) | logic | unaffected |
| CullingInfo | TrackingSeconds × LTR (0x5DF4AD) | logic frames | unaffected |
| Sound / EvaEvent / Laser / RayEffect / TerrainScorch | names/offsets only | audio / laser drawable | other domains |

## 7. Cross-domain observations (for the core and other agents)
- W3DDisplay outer draw 0x44B788:
  - W3D sync time is `[0xDC7580] += [0xDC7A8C](33) × (clientFrame − last)` at 0x44B8DD and 0x44B911, then set via 0x516E20 into [0xDD1E0C].
  - There is a **second limiter**: a 29 ms busy-wait at **0x44B9C1 `cmp ecx,0x1D`**, gated by GlobalData+0x26.
  - A fast-forward path renders only when `getFrame()%30==0`.
- Eva (≈0x5DD982): each element is decremented by 33.33 ms [0xD9F620] per client frame.
- 0x5DCB28: sound repeat uses ms×0.03 vs client frame.
- Decal pulse 0x732926 uses 0.03 / 0.0333 × client frame.
- W3D-native emitters (ParticleBufferClass/ParticleEmitter 0x5A07A0..0x5AE4xx) run on W3D sync time.
- AotR hooks at 0x5D8A64/0x5D8AF1 sit in FUN_005D893C (damage-type/"FORCE" code). They are not particle code and none of the proposed sites overlap AotR bytes.
