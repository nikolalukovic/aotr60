# Camera / input / UI / audio frame-timing findings (AotR game.dat, 60 FPS project)

Plan mode note: the task asked for notes in `an/findings/camera_ui_audio.md`; plan mode
forbids writing there, so the notes live in this file. Copy it there once plan mode is lifted.
(Scratch helpers already exist in `an/findings/cua/`: pe.py, bd.py, gdfield.py, vcall.py, reffuncs.py.)

## Definitions
- "per frame" = once per rendered frame. Client frame counter = TheGameClient(0xDE4388)->getFrame()
  (vtbl+0x7c, field +0x10), incremented once per rendered frame by GameEngine client step 0x632409
  (`call [esi+0x7c]; inc eax; call [esi+0x38]` at 0x632435..0x632440, only when GameClient+0xC8 set,
  i.e. not script-frozen). So client frame == rendered frame in stock and in the 12-sub-frame plan.
- at60 below = behaviour if the system is left unpatched under the planned core model:
  limiter 60, 0xD9F60C(CLIENT_FPS)=60 before CRT init, client float half rescaled (0xD9F620=16.667,
  0xD9F624=0.06, 0xD9F628=60, 0xD9F62C=0.016667), therefore 0xDC7A8C=16 (static init 0xBC146C =
  1000/CLIENT_FPS, and GameClient::init 0x646781 `fld [0xD9F620]` -> setFrameRate 0x44BF5C cvttss2si).

## Subsystem map (verified)
TheGlobalData 0xDE4364, TheGameLogic 0xDE412C (+0x40 logic frame, +0x124 paused), TheGameClient 0xDE4388,
TheTacticalView 0xDE447C (W3DView vtbl 0xBDD490; SubsystemInterface vtbl 0xBDD454, update 0x48BCF2),
TheInGameUI 0xDE4830 (W3DInGameUI vtbl 0xBDD9B0, update 0x6A1F4D, draw 0x48EA29), TheMouse 0xDE36E0
(vtbl 0xBDE600), TheKeyboard 0xDE4334 (vtbl 0xBDE5B0), TheWindowManager 0xDE495C (vtbl 0xBDDBB8, update
0x6C15FD), TheTransitionHandler 0xDE3654 (vtbl 0xBF1DA8), TheShell 0xDE7890 (vtbl 0xC2C894, update
0x75E1D3), TheAptPlayer 0xDE3F0C (vtbl 0xBDB780, update 0x624EE1), TheAudio 0xDE42FC (Miles vtbl 0xBDB550,
update 0x461DA6), TheRadar 0xDE4AB8 (update 0x6D8E2B), TheVideoPlayer 0xDF06F8 (update = no-op 0xA88AC0),
ControlBar 0xDE7744 (vtbl 0xC23300, update 0x71FC08), Living-World view 0xDE4958 (vtbl 0xBDE918),
TheScriptEngine 0xDE3BAC, TheMessageStream 0xDE6398, CameraShakerSystem 0xDC78D4.
Per-frame call order: GameEngine::update 0x6325A0 -> client step 0x632409 -> TheAptPlayer->update,
TheRadar->update, GameClient::update 0x64849E [LookAt scroll 0x83B471, LW scroll 0x8392A7, keyboard,
mouse, WindowManager, ..., Display draw (W3DDisplay::draw 0x44B788 -> draw pass 0x449CF8 -> W3DView::update),
Shell, InGameUI::update], MessageStream, TheAudio->update.

## Findings (address, mechanism, fix)

### Camera
1. W3DView follow/lock ease 0x48BCF2: static followFactor 0xD99638 (init -1): first frame 0.05
   (0x48BE98 movss [0xBDD760]), then += 0.05/frame (0x48BEA2 addss [0xBDD760]) clamp 1.0; pos +=
   (target-pos)*ff per frame. 2x at 60. Fix: new const 0.025 for both operands + replace lerp factor by
   1-sqrt(1-ff) (detour after the factor is in xmm1 at 0x48BFE8).
2. Tether mode (lock type 1): inside cell -> factor lockDist(-0x50)*0.01 (0x48BFCB mulss [0xBE5600]);
   outside -> CameraEaseFactor(GameData+0xDE0, 0x48BF9B)*(1-r^2/d^2). Per frame exp. 2x. Fix f->1-sqrt(1-f)
   (0.01 -> 0.0050126 via new const; CameraEaseFactor via parse hook GameData table 0xBFF960).
3. Height settle CameraAdjustSpeed (GameData+0xAB0, INI entry 0xC003E0 parser 0x42ED00, default 0.1 set at
   0x642FE5): 0x48C36A / 0x48C3B0 `fmul [eax+0xAB0]`, per frame exponential. Also the visible smoothing
   of mouse-wheel zoom. 2x. Fix: store k' = 1-sqrt(1-k) (custom parser + default 0.0513167).
4. Camera-zone zoom-scale smoothing (View+0x9C..+0xA8 toward area values when FUN_006E4DED(pos) hits
   W3DView+0x2408 area): step=(t-c)*0.08 (0x48C553, 0x48C5D6 fmul [0xBDD750]) with min step +-0.01
   (0x48C57F/0x48C601 [0xBE5600], 0x48C589/0x48C60B [0xBDD74C]). 2x. Fix: 0.0408337 and +-0.005 consts.
5. Legacy view shake (FXList ViewShake -> W3DView::shake 0x486D86, Shake*Intensity/MaxShake*):
   offset = I*(cos,sin); I *= 0.75 (0x48C126 mulss [0xBDC628]) and direction negated every frame
   (0x48C133..0x48C148); decay skipped only in panorama-screenshot mode (GameData+0xEA6/0xEA7 set by
   W3DDisplay::draw 0x44BA05). 2x decay and 2x oscillation. Fix: run block 0x48C121..0x48C14D only on
   even 60-Hz frames (keeps 15 Hz wobble), or const 0.8660254 (decay only).
6. CameraShakerSystem::Timestep 0x4655F8 (ElapsedTime += dt, delete at Duration) called from
   0x502858 with hard-coded 1/30: 0x5033E5 `fld [0xBDFC6C]`, executed in buildCameraTransform 0x489F96
   (via setCameraTransform 0x48B7B1, i.e. every camera rebuild ~ every frame). Script CAMERA_ADD_SHAKER_AT.
   2x. Fix: change operand to 0xD9F62C (rescaled 1/60) or a new 1/60 const (bytes D9 05 imm32).
7. Scripted camera moves use int 0xDC7A8C (ms per client frame) - NOT animation playback:
   setup numFrames = ms/0xDC7A8C at 0x485DD4 (rotateCamera vtbl+0xCC), 0x485E7A/0x485E9A
   (rotateTowardObject +0xD0), 0x4886B1 (+0xD4), 0x4887F4 (+0xEC), 0x48887A (zoom +0xF0), 0x48892E
   (pitch +0xF4), 0x4889B2 (+0xF8), 0x48D3C5 (moveCameraTo +0xC8, mode 3); back-conversion frames*ms
   at 0x485F62 (+0x7C), 0x486279 (+0x88); per-frame steppers in updateCameraMovements 0x48A953:
   curFrame++ (0x48660E,0x4866A7,0x486758,0x4867F1,0x48A417), mode 3 per client-frame change 0x48657E,
   waypoint/spline paths elapsed += 0xDC7A8C ms (PUSH [0xDC7A8C] at 0x48A9F2, 0x48AAE5, 0x48AB06 ->
   0x48688A, 0x489817). Mode 4 (camera animation object) advances (30.0/CLIENT_FPS)*dClientFrames
   (0x48AA62/0x48AA69) - auto-correct.
   Stock: 33 ms/frame (1% long). Plan (0xDC7A8C=16): every move +3.9% longer than stock.
   If 0xDC7A8C stays 33: 2x fast. Exact fix: keep a camera-only value: redirect the 8 IDIVs to
   "ms/33*2" (detour: idiv [G33]; add eax,eax) and the 3 PUSHes to a global alternating 16/17 each frame.
8. W3DDisplay::draw inner limiter (time-multiplier + paused-camera loop): prev=now-30 (0x44B98C,
   imm 0xE2 at 0x44B98E), wait while (now-prev) < 29 (0x44B9C1, imm 0x1D at 0x44B9C3). Camera time
   multiplier (W3DView+0x23D4, get vtbl+0xDC 0x48B5DA) renders 1 of N frames, outer limiter off (0x639FEF),
   so game speed = N client frames per >=29 ms. With 12 sub-frames: half speed. Fix: 0xE2->0xF1, 0x1D->0x0E.
9. Time-fast (script fast-forward) renders only when clientFrame % 30 == 0 (0x44B8C5 push 0x1E):
   cosmetic, 2 renders/s at 60. Optional 0x1E->0x3C.
10. Letterbox 0x443939: fade = (now-start)*0.001, now=timeGetTime from draw loop: real time, OK.

### Input
11. Scroll (LookAtTranslator per-frame 0x83B471, start of GameClient::update): RMB
    offset=H/V*(cur-anchor)+H/V*KSF^2*norm; keyboard +-KSF*H/V*100; edge: KSF*ScreenEdgeScrollSpeedFactor
    *H/V*ramp% (ramp from timeGetTime vs ScreenEdgeScrollRampTime ms = RT); then
    InGameUI->setScrollAmount (vtbl+0xB4 0x69B2F9) and TacticalView->scrollBy (vtbl+0x5C 0x48C774, call
    0x83B8A4). InGameUI::update keyboard scroll 0x6A1F4D: KSF*H*[0xDA0B24 = 250.0, only user] -> scrollBy
    0x6A23CA. scrollBy moves position directly. 2x. H/V factors are rewritten from Options.ini ScrollFactor
    (0x641DED, 0x91FEC2) so do not patch GameData. Fix: halve the vector passed at 0x83B8A0..0x83B8A6
    (7 bytes `lea edx,[ebp-0x10]; push edx; call [eax+0x5C]` -> call stub) and set 0xDA0B24=125.0.
    Leave setScrollAmount unscaled (ScrollAmountCutoff semantics).
12. Keyboard rotate: angle +-= KeyboardCameraRotateSpeed (GameData+0xC2C, INI entry 0xC00860) per frame at
    0x6A2205/0x6A222D (InGameUI), 0x83B984/0x83B9B6 (LookAt), 0x839641/0x839674 (*+-15, Living World).
    Only readers. Fix: parse hook storing value*0.5 (or redirect 6 operands).
13. Keyboard zoom: zoomIn 0x65E803 = setZoom(z*0.96-1), zoomOut 0x65E82A = setZoom(z*1.05+1), called per
    frame from 0x6A225E/0x6A227B (InGameUI) and 0x83B9DD/0x83B9F4 (LookAt +0x156/+0x157). Mouse wheel
    (MSG_RAW_MOUSE_WHEEL=0x13 in 0x83AC4A) calls the same functions per notch (event, correct).
    Fix: gate the four per-frame call sites to even frames (do not change the constants, wheel shares them).
14. Special camera mode (W3DView+0x2449 via vtbl+0x1CC/0x1D0): screen-edge rotate +-0.046 rad
    ([0xC53D5C]) and pitch +-0.02 per MSG_RAW_MOUSE_POSITION; Mouse::createStreamMessages 0x5EDD69 emits
    type 3 every frame (0x5EE17B). 2x. Fix: scale by 0.5 in 0x83AC4A case 3 (consts) or gate.
15. Living World (War of the Ring) view 0x8392A7 -> LW vtbl+0x2C 0x49B799: pos += 0.5*offset*zoom
    (0x49B7AD movss [0xBD869C]) and angle += [0xD99B64]*rot (0x49B8D9; 0xD99B64 written by 0x49CE48).
    Per frame. 2x. Fix: 0x49B7AD operand -> 0.25 const, 0x49B8D9 operand -> const 0.5*value or patch store.
16. Keyboard auto-repeat (Keyboard::update 0x63F667, input frame +0xE1C++ per frame; 0x63F473):
    first repeat after >10 frames (0x63F497 cmp edx,0xA), then stamp=now-12 => repeat every frame.
    2x. Fix: imm 0x0A->0x14 at 0x63F499, 0x0C->0x13 at 0x63F4D6.
17. Mouse "button held" messages (types 9 / 0x11): counters +0x5000/+0x5004 start at button-down
    (0x5EDEA9), ++ per frame, fire at 5. 83 ms at 60. Fix: imm 5->10 at 0x5EE0BF, 0x5EE0D5, 0x5EE0E5, 0x5EE12F.
18. MMB click (camera reset vtbl+0xC8) if MMB up within 5 client frames: 0x83AD70 cmp eax,5 -> 10 (0x83AD72).
19. Correct already: double-click (OS WM_*DBLCLK), DragToleranceMS (0x81FAD4, 0x81FC37, 0x83C86A compare
    Windows message times), drag px tolerances, MMB-drag rotate/pitch (delta based), mouse wheel steps.

### UI / APT
20. APT movies: AptPlayer::update 0x624EE1 dt=timeGetTime delta clamped <=60 ms (>=34 ms once after a
    movie load, flag +0x328 set by 0x622911/0x622959); FUN_00AE18C0 fixed-step accumulator per movie
    (frame ms = movie header +0x24). Real time: correct at any FPS.
21. Palantir (AptPalantir::update 0x6D7647 via subsystem thunk 0x6D7B33, vtbl 0xC18E30): button flash
    counters this+0x12C set to CLIENT_FPS*sec (0x6D455F), --/call (0x6D72DF); resource popups
    CLIENT_FPS*5 / *6 (0x6D3DAC/0x6D3DCE) decremented per call in 0x6D57B4; radar events die at
    getFrame()+CLIENT_FPS*sec (0x6D93EB, 0x6D9B4D). All auto with CLIENT_FPS=60.
    AotR code: 0x6D57AF jmp -> .danetta 0xED0B00 -> 0xED0800 has a per-call counter [0xED0000],
    work every 30 calls (0xED0816 cmp 0x1E) = 1/s stock, 2/s at 60 (harmless; would be 0x3C, AotR bytes -
    leave to AotR). Community "Palantir breaks at high FPS": not reproduced from code; most likely via
    CLIENT_FPS-scaled counters vs. FramesPerSecondLimit/delay-fix combos - unverified.
22. Move hints W3DInGameUI 0x48EDED: visible while getFrame()-hint.frame <= 40 (0x48EE27 cmp 0x28,
    0x48EE2C push 0x29, 0x48EE39 cmp 0x28). 0.68 s at 60. Fix 0x28->0x51 (0x48EE29, 0x48EE3B), 0x29->0x52 (0x48EE2D).
23. UI messages (InGameUI::update 0x6A1FD3..0x6A2064): timeout in logic frames (MessageDelayMS/LTR/1000),
    then alpha -= int((logicFrame-ts)*0.01) per frame (0x6A200C, ftol 0x6A2012). 2x fade. Fix: replace
    call at 0x6A2012 with stub returning 0 on odd frames.
24. Floating text: create 0x6A1804: timeout = clientFrame + TimeOut_ms*0.03 ([0xD9F624]) or CLIENT_FPS/3;
    update 0x69DB40 (once per client frame): count++, alpha -= int(VanishRate/framesPerTick*(f-timeout));
    draw 0x69DC5B: rise = count*MoveUpSpeed/framesPerTick (DAT_00DE4324+0x38). Rise/timeout auto;
    vanish quadratic -> ~2x faster (fix: extra 0.5 on the 0x69DBC9 term).
25. World animations 0x69DEA9: z += zRisePerSecond / LTR per preDraw call (0x69DF0E cvtsi2ss [0xD9F608]).
    2x. Fix: operand -> int 10.
26. ControlBar button flash 0x71FC08: counter-- when getFrame()%10==0 (0x71FD68 push 0xA). 2x. Fix 0x14.
27. Build-placement ghost blink (0x6A2AE5): color toggles on getFrame()&4 (0x6A2FEE, 0x6A3031, 0x6A3059,
    0x6A3086, 0x6A3A99 `test al,4`). 2x. Fix imm 4->8. (&1 legality check at 0x6A393D: CPU only.)
28. Radar: Radar::update 0x6D8E2B kills events when getFrame() > dieFrame (auto); W3DRadar draw 0x44FDF3
    refreshes unit icons when getFrame()%6==0 (0x45011C push 6) -> 10 Hz at 60 (harmless; 12 restores).
29. RT, correct: military subtitles 0x69CE5F/0x69B7BE (timeGetTime), tooltips (Mouse 0x5EE7A8,
    TooltipDelayTime ms), animated cursor (W3DMouse 0x498CBC timeGetTime*fps), shell update throttle
    0x75E1D3 (>=32.33 ms [0xC2C90C], only ref; at 60 fires every 2nd frame if frames are >=16.2 ms;
    suggest 30.0f for robustness), VP6 video (timeGetTime in 0x4909A9.. 0x491DEC).
30. Window transitions: TransitionGroup update 0x5DB4B1 frame(+8) += dir(+4) per call, from
    WindowManager::update 0x6C1602 every frame (WindowTransitions.ini). 2x. Fix: gate 0x6C1602 call to
    even frames.
31. Formation preview fade 0x6A5723 uses CLIENT_FPS*0.5 frames: auto.

### Audio
32. Miles update 0x461DA6: dt = vtbl+0x1B4 0x450ABC = (getFrame()-last)*[0xD9F620] in game, or [0xD9F620]
    per update in shell (+0x678!=0); stored at +0x90 and used by fades 0x452C50, envelopes 0x4519FC
    (dt*0.001 s), delays (thresholds "< 33.33" at 0x452E2A, 0x45CD31, 0x461AF5...). Auto when 0xD9F620=16.667;
    2x if 0xD9F620 is left at 33.33.

### Cross-cutting (other agents)
- W3D shader time 0x48017A..0x481235: t = SyncTime(0xDD1E0C)*0.001*CLIENT_FPS -> doubles with CLIENT_FPS=60.
- GameEngine 0x63A181 `add ecx,6` (limiter bypass for 6 client frames after skip) -> 12.

## Verification plan
- Static: re-read each patch site bytes with bd.py before applying; confirm no overlap with AotR hooks.
- Runtime (later, by user): compare stock 30 vs patched 60 with stopwatch: edge/RMB/keyboard scroll speed
  over map width, keyboard rotate 360 deg time, scripted camera intro length, move-hint duration,
  shake decay, audio fades, key repeat.
