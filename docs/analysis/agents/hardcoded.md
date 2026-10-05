# Hard-coded per-frame constants sweep (AotR game.dat) - notes

Plan mode was switched on during the task, so the requested file
`an\findings\hardcoded.md` was NOT written. These notes are its intended content
(copy to hardcoded.md once writing is allowed). Scratch dumps used for the analysis are under
`an\findings\hc_tmp\` (cf\*.c = every function calling GameClient::getFrame).

## 0. Identities established (verified)
- TheGameClient = *0xDE4388 (string "TheGameClient" @0xBFED74, pushed with 0xDE4388 at 0x63BDF5). TheGameLogic = *0xDE412C, TheAudio = *0xDE42FC, TheGlobalData = *0xDE4364, TheTacticalView = *0xDE447C, engine = *0xDE4324 (+0x38 framesPerTick, +0x3C interp fraction). *0xDC62C0 = Debug object (Debug::PreStaticInit).
- GameClient::getFrame = vtable+0x7C -> 0x9F9F1A `mov eax,[ecx+0x10]`. Client frame counter = TheGameClient+0x10.
- Increment: engine vt+0x9C = 0x632409 (called once per rendered frame from 0x6325A0): `if (TheGameClient+0xC8) setFrame(getFrame()+1)` at 0x632433..0x632440, then TheGameClient vt+0x28 = GameClient::update 0x64849E.
- GameClient::update freeze gate: `if (DAT_00d9f6f8 == m_frame) skip drawables` -> Drawable::updateDrawable (0x675996) runs once per m_frame change.
- 100 getFrame call sites in 75 functions (list in hc_tmp\cf).
- GameData field offsets (table 0xBFF580..0xC01210): +0x26 UseFPSLimit, +0x28 FramesPerSecondLimit, +0xA9C HorizontalScrollSpeedFactor, +0xAA0 VerticalScrollSpeedFactor, +0xAA4 ScreenEdgeScrollSpeedFactor, +0xAA8 ScreenEdgeScrollRampTime, +0xAF8 KeyboardScrollSpeedFactor, +0xC2C KeyboardCameraRotateSpeed, +0x10 MoveHintName, +0xDA4/+0xDB0/+0xDB4 ParticleCursor*.

## 1. Core design observation
Under the planned model (12 sub-frames, every rendered frame increments m_frame) the client
frame counter runs at 60/s, so every client-frame-count timer below runs 2x. Two options:
- A: keep m_frame at 30 Hz (increment at 0x632433 only on every 2nd 60-Hz frame). Fixes all
  m_frame-count timers at once (drawable fades/tints/flashes, weather, ControlBar, move hints,
  Anim2D, BirthFade, radar, particles when m_frame-gated, W3D sync delta). Side effects:
  drawable/particle/W3D-anim state steps at 30 Hz; W3D sync should then be fed per render frame
  with fractional ms (0x44B911) for smooth animation; interpolation fraction (+0x3C) is
  independent and still 12-step. Per-render systems (section 3) still need halving.
- B: m_frame at 60 Hz -> patch every item in section 2 (plus particle/INI frame counts, owned by others).

## 2. m_frame (client-frame) count timers with hard-coded counts
| site | system | constant | at 60 (B) | fix |
|---|---|---|---|---|
| 0x675D5B DIV [0xDE45D8] | Drawable flash period (half global =15) | 15 | 2x | 0xDE45D8=30 |
| 0x675D75 push 4 | colorFlash decay | 4 frames | 2x | 8 |
| 0x675DC8/E0E/E78 | tint envelopes play(color,30,30,sustain) | 30,30,300,9999 | 2x | 60,60,600 |
| 0x7BC049, 0x7C0A4A IDIV [0xDE87DC] | script flash count = LTR*n/15 | 15 | count ok only if kept 15 | keep 0xDE87DC=15 if CLIENT_FPS=60 at static init |
| 0x48EE27/0x48EE2C/0x48EE39 | move hints lifetime | 0x28/0x29 | 0.68 s | 0x50/0x51 |
| 0x71FD68, 0x9314E9 push 0xA | ControlBar button flash countdown %10 | 10 | 2x | 0x14 |
| 0x6A2FEE,0x6A3031,0x6A3059,0x6A3086,0x6A3A99 test al,4 | placement ghost blink | &4 | 2x | &8 |
| 0x83AD70 cmp eax,5 | middle-click duration | 5 | half window | 0xA |
| 0x4B6B63 addss [0xBDFC6C] | W3DModelDraw transform blend | 1/30 per frame | 2x | operand -> 0xBDC1FC (1/60) |
| 0x497D00 / 0x494DBA | weather fade +-1 (0..255), lightning counts & per-frame probability | 1/frame | 2x | gate to 30 Hz |
| 0x4951FF push 0xA | scene light refresh %10 | 10 | cost only | optional 0x14 |
| 0x4D1DFE push 0xA | geometry refresh %10 | 10 | cost only | optional |
| 0x44FDF3 %6 | radar unit layer refresh | 6 | 10 Hz (cost) | optional 12 |
| 0x48FD6A 0x3C | DisplayString cache eviction | 60 | evicts at 1 s | harmless |
| 0x677C0C &10 | number glyph blink | | 2x | cosmetic |
| 0x6D7F1B | Anim2D delay: parsed by 0x73A46F with LOGIC 0.005 (ms->logic frames) but compared to client frames | ceil(ms*0.005) | 2x | cave after ftol at 0x73A4A9: store 2*value (0x73A46F is only used by Anim2D AnimationDelay, table 0xC195C0) |
| 0x4C09E0 / 0x4C72BE | W3DModelDraw BirthFadeTime (+0x150, parser 0x73A429 logic ms*0.005) consumed as client frames | | 2x | double at 0x4C09E0 store / 0x4C0A99 arg |
| literal fadeIn/fadeOut(frames) args | 0x858332(10), 0x8583C9(30), 0x7954A8(138), 0x79B05E/0x79B09D(10), 0x8AEE86/0x8B1513/0x8B2392(10), 0x664FF2/0x665031([LTR]=5); colorFlash 0x871B1C/0x871B63/0x8A029B(4), 0x81E22E(8) | | 2x | double immediates |
| 0x67561E (-0.03), 0x8889E9 (+0.03), 0x860E93 (-0.2 @0xC2E198) | Drawable +0xE0 rate per client frame (0x67093E setter) | | 2x | halve rates (redirect operands; 0xBDC540/0xBDC538 are shared) |

Global-based m_frame users (follow the 0xD9F6xx block): 0x442E56 (0.0333), 0x450ABC (33.333),
0x44DFBA (FPS*2 pulse), 0x44D664 (FPS*1.5 beacon), 0x48A953 mode 4 (30.0/FPS), 0x6A1F4D (%FPS),
0x6D9B4D (FPS*10), 0x8EA5D8/0x8E9DB2 (FPS*15, FPS fade), 0x732926, 0x5DCB28/0x7651B1 (0.03),
Drawable fade callers using 0.03 (0x85EBEE, 0x86A943, 0x86ABDC, 0x8A5D00, 0x8B8464, 0x862B07).
MP only: 0x63A17B `m_frame < DAT_00de4308+6` limiter bypass; 0x63239D render-skip/framesPerTick++.

## 3. Per-render-call systems (NOT gated by m_frame) - 2x at 60 under both options
| site | system | step | fix |
|---|---|---|---|
| 0x4FDB4A `add [esi+0x74],0x21` (83 46 74 21) | WaterTracksRenderSystem::update (Generals "lock to 30 fps" 33 ms), from flush 0x4FF9A8 | +33 ms/call | imm 0x10/0x11 alternate or cave |
| 0x5033E5 `fld [0xBDFC6C]` (D9 05 6C FC BD 00) | CameraShakerSystem::Timestep(1/30) in buildCameraTransform 0x502858 (Timestep=0x4655F8) | 1/30 s/call | operand -> 0xBDC1FC |
| 0x502458 (.data 0xD9AE94=0.5,0xD9AE98=0.05,0xD9AE9C=0.4,0xD9AEA0=0.03) | camera "Target"/"Zoom" transition accelerating integrator per buildCameraTransform | v+=a, p+=v | caps/2, accel/4 (write .data) |
| 0x8392A7 & 0x83B471 (called from GameClient::update) | camera scroll (RMB, key 350.0@0xC53B00 / 100.0@0xBD88D8, edge, rotate ±15 @0xBDC6CC/0xC53AFC * KeyboardCameraRotateSpeed) | per call | halve the scroll offset/angle at the call into TacticalView vt+0x2C / InGameUI vt+0xB4; do NOT patch 350.0 (also used by AI 0x9E896E) or 100.0/15.0 (shared) |
| 0x4F9871, 0x4F6B8D, 0x4F81D7 (vtables 0xBE5228.., "shaders\\hilightfilter", ExVapor01/02.tga) | shader anim frame +=0.6 (0xBDAD70, wrap 13) and UV +0.03/-0.0225/+0.02 per call | per render | private halved constants (operands) |
| 0xB53A5C/0xB53A81 [0xBDC1FC] | outline pass (unlitnormalextrusion) fade ±1/60 per render | | private 1/120 |
| 0x6A536C (called from render 0x449CF8), creator 0x6A1978 | particle cursor trail: pos+=v, v*=damp per render; expiry m_frame+LTR*x; fade DAT_00de4848(=LTR) frames | | sqrt damping & scaled v, or gate 30 Hz |
| 0x69DEA9 | world animations z += zRise/LTR per call | | /2 |
| 0x5DB4B1 `add [esi+8],eax` (via 0x5DB60A handler vt+0x10) | window transitions frame index per call (styles e.g. 0x75F4B5 16 slide + 6 fade) | 1 frame/call | step every 2nd call |
| 0x44B98C (83 C6 E2) / 0x44B9C1 (83 F9 1D) | W3DDisplay::draw inner limiter for frozen-time camera loop (30/29 ms) | ~34 FPS | keep; change to 0xF/0x10 only if camera steps halved |
| 0x44B8C5 (6A 1E) | fast-forward render 1 of 30 m_frames | | leave |

## 4. Not time-relevant / logic (checked)
LTR*30/LTR*15 IMULs (0x5DC714, 0x73FEF4, 0x8F085A, 0x94AA73, 0x89A79B), 0x639E7E (logicFrame+30 string),
0x80B088 (delta*1/30*LTR UI conversion), 0x4430BB avg-FPS 30-sample QPC history, 0x4FCDE3 (fade gated
on logic frame), HSV 0x46DC7B (1/60 = hue/60), "Avg frame time" debug 0x65C1EA/0x65D3F5/0x65D604,
0x765E00 (segment count), 15.0/30.0/60.0/2/3 rdata users = geometry/angles/ctor defaults, PUSH 0x21 =
33 control-bar buttons, 0x4735AB (terrain lighting), 0x4EA107 (epsilon).
Half globals: 435 targets, only 3 code reads (0x675D5B, 0x7BC049, 0x7C0A4A); raw .text scan found no others.

## 5. Patch-site safety
None of the sites above overlaps AotR hooks (0x5D8A64, 0x5D8AF1, 0x629D11, 0x638D49, 0x69A760,
0x6D40FA, 0x6D410A, 0x6D57AF, 0x6D7267, 0x8A144D, 0x9A3AE0). Shared rdata constants (0xBDC540,
0xBDC6CC, 0xBD88D8, 0xC53B00, 0xBDC1FC, 0xBDFC6C) must not be rewritten; redirect instruction operands.
