# W3D / render-path timing audit (AotR game.dat) - notes

NOTE: plan mode became active mid-task, so the requested file
`an/findings/w3d.md` could NOT be written. These notes are its intended content.
(Helper scripts written before plan mode: an/findings/{ptrscan,dd,vt,modvt,fieldfind,fieldtab}.py)

## 0. Key verified facts (all addresses read in the binary)

- Globals: TheGameClient=0xDE4388, TheGameLogic=0xDE412C, TheDisplay=0xDE4418, TheTacticalView=0xDE447C,
  TheGameEngine=0xDE4324, TheTerrainVisual=0xDE4AC8, TheInGameUI=0xDE4830, TheAptPlayer=0xDE3F0C,
  TheFXParticleSystemManager=0xDE3744, TheSnowManager=0xDE3B54, TheCloudEffectManager=0xDE3C24,
  TheCloudBreakEffectManager=0xDE7734, TheFireManager=0xDE4EFC, TheGlobalWeatherSystem=0xDE772C.
- TheGameClient vtable 0xBDA6E0: +0x38 setFrame (0x993D2E: mov [ecx+0x10],arg), +0x7C getFrame (0x9F9F1A: mov eax,[ecx+0x10]),
  +0xCC setFrameRate (0x44BF5C: cvttss2si eax,[esp+4]; mov [0xDC7A8C],eax).
- Client frame m_frame (+0x10) is incremented once per engine client update FUN_00632409 (0x632433..0x632440:
  getFrame()+1 -> setFrame) when TheGameClient+0xC8 (advance flag set by stepper) != 0. So m_frame = rendered (non-paused) frames.
- GameClient::update = FUN_0064849E (per rendered frame): freeze hack `DAT_00d9f6f8 == m_frame`; calls TheTerrainVisual(+4 vtbl)+0x28,
  TheDisplay+0x28 (update 0x4430A7), TheDisplay+0x30 (draw 0x44B788), weather managers +0x28, InGameUI +0x28.
- W3DDisplay vtable 0xBD9C28: +0x28 update 0x4430A7, +0x30 draw 0x44B788.
- WW3D::Sync = FUN_00516E20 (SyncTime 0xDD1E0C, PrevSyncTime 0xDD1E10). FrameCount 0xDD1E20 ++ in End_Render 0x516DA0.
- W3DDisplay::draw sync accumulator static = 0xDC7580.
  normal path 0x44B911: `mov eax,[0xDC7A8C]; imul eax,esi; add [0xDC7580],eax; push [0xDC7580]; call Sync`
  where esi = m_frame - lastFrame(0xD98C8C) (0 when frozen).  Fast-forward path 0x44B8D8: `+= [0xDC7A8C]` per call when
  (scriptTimeFast||FUN_00603491) && m_frame % 30 != 0 (render skipped).
- 0xDC7A8C (TheW3DFrameLengthInMsec): static init 0xBC1466..0xBC1478 = 1000/CLIENT_FPS(0xD9F60C);
  GameClient::init 0x646781 pushes float [0xD9F620] -> setFrameRate -> (int)33.333 = 33.
- Engine object (stepper param) +0x34 sub, +0x38 framesPerTick (=CLIENT_FPS/LTR), +0x3C interp fraction.

## 1. Time-base classes found in render code
A SYNC (W3D clock): correct iff sync advances avg 16.5 ms per 60-Hz frame.
B client-frame (m_frame) consumers normalised by client constants (0xD9F60C int 30, 0xD9F624 0.03, 0xD9F62C 1/30):
  self-correct only if m_frame runs at 60/s AND the client half of the block is rescaled; otherwise 2x.
C client-frame consumers with hard-coded/logic-derived counts: need individual patches.
D per-call (per draw / per Render) integrators: 2x at 60 -> halve / sqrt damping / gate to 30 Hz.
E normalised by TheGameEngine+0x38 (framesPerTick): self-correct if +0x38==12 (vegetation reads it at buffer init).
F real time: safe.
G sync x CLIENT_FPS (FX shader/FX storage "time in frames"): break if CLIENT_FPS global is changed -> pin to 30.

## 2. Systems (see structured output for details and patch sites)
- Sync clock 0x44B788 (A root). Patch 0x44B911/0x44B916 and 0x44B8DD with half-ms accumulator.
- 0xDC7A8C pin to 33 (setter 0x44BF5C, static init 0xBC1478).
- Camera (0xDC7A8C users): IDIV 0x485DD4,0x485E7A,0x485E9A,0x4886B1,0x4887F4,0x48887A,0x48892E,0x4889B2,0x48D3C5 (frames x2);
  IMUL 0x485F62,0x486279 (x0.5); PUSH dt 0x48A9F2,0x48AAE5,0x48AB06 (16/17); mode4 0x48AA62 (B); shake 0x48C126 x0.75 + sign flip (D).
- Inner limiter 0x44B9C1 cmp 0x1D, 0x44B98C add -0x1E; fast-forward 0x44B8C5 push 0x1E.
- Model anim/crossfade W3DScriptedModelDraw 0x4BF560 (delta of 0xDC7580) (A).
- SYNC consumers: mappers 0x581B60..0x586DD0, segline 0x5909A0/0x590A70/0x591420/0xB571A0..0xB57950, terrain tracks 0x483870,
  water render UV 0x495228, snow 0x4943E1, shader time 0x54CEC5, W3D part_buf/emitters 0x5A1D60..0x5AE3F0, W3DView gate 0x48C701.
- G sites: 0x48017A..0x481235 (21 fns, FIMUL [0xD9F60C]), 0x7B1102,0x7B1160,0x7B12FE,0x7B140B (IMUL [0xD9F60C]).
- FX particle manager update 0x624EE1: RT dt clamp<=60 ms (>=34 ms when +0x328) (F).
- Drawable::draw 0x67C4C7 material-pass opacity x0.8 per draw (D).
- Water river offsets 0x5001F1/0x50020C (D) (+ fixed 33.33 dt at 0x500186 when GlobalData+0xD45).
- Water/overlay render objs per Render(): 0x4F9871 (0x4F9904 +0.6 frame, 0x4F9A0D/0x4F9A22/0x4F9A34 UV), 0x4F6B8D (0x4F6C86), 0x4F81D7 (0x4F8321) (D).
- Vegetation: sway 0x4E5D2C, shrub push 0x4E837E, sink count 0x4EA0E8 (E); sink depth 0x4ED63F, topple 0x4E9F17/0x4E9F28 + 0x4EA107 (D).
- Decal throb/rotation/spiral 0x732926: 0x73297A (B), 0x732B4F (B), spiral 0x732A84 (D).
- Shadow/decal opacity envelopes: setup 0x7323CE from 0x4C4F74/0x4C4F8E/0x4C512D/0x4C513E (B), evaluated 0x50B85D.
- BirthFadeTime 0x4C76EE..0x4C7780 (C, total from 0x4C0A77, logic-frame count).
- Transform blend +0x2C0 += 1/30 per client frame 0x4B6B63 (C).
- Recoil 0x4B4699 (D) called 0x4C78CE. W3DLightDraw 0x4CF2B5 (0x4CF2BB/0x4CF3CF) (D). Rope 0x4CA481 (D). Tank tread 0x4CDDCB (D).
- Cloud effect manager 0x497D00/0x494DBA (C). Fire manager 0x49829C interval 0x49830B (C).
- Radar pulse 0x44D664 (0x44D698), 0x44DFBA (0x44E02F) (B). Time-of-day lerp 0x442E56 (0x442E96) (B).
- Floating text 0x6A536C (D + C; UI domain).
- RT safe: letterbox 0x443939/0x443C46, shroud 0x472CF2/0x473AAB, Bink 0x4909A9.., avg FPS 0x4430BB, UI-scene clock 0x4A89BD.
- Not affected: physics xform 0x67BE90 (stubbed), streak 0x4CF884 (distance based), projectile stream 0x4D0D05, laser geometry 0x4C8A6C.
- No AotR (.danetta/.angmar) code references any timing global; no patch site overlaps AotR hooks.
