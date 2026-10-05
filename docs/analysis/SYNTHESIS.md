# 60 FPS for Age of the Ring (BFME2 RotWK game.dat): synthesis

STATUS: written while plan mode was active. The per-area agent notes are in `agents/`:
`core.md`, `w3d.md`, `drawable.md`, `particles.md`, `camera_ui_audio.md`, `hardcoded.md` and
`classify_0.md` .. `classify_3.md`.

Agent keys used below: core, w3d, drw (drawable), ptx (particles), cua (camera_ui_audio), hc (hardcoded), c0..c3 (classify chunks).
"SYN" = verified in this synthesis pass (addresses cited).

Terminology
- s = stepper sub-frame (GameEngine+0x34). Stock 1..6, proposed 1..12.
- A-frame = stepper frame with odd s (logic sub-step (s+1)/2 runs). B-frame = even s (no logic).
- A-render = the render (clientUpdate) that follows an A-frame: TheGameClient+0xC8==1 at that moment, so m_frame increments.
  B-render = render following a B-frame (m_frame unchanged).
- m_frame = TheGameClient(0xDE4388)+0x10, the client frame counter (getFrame = vt+0x7C = 0x9F9F1A).
- Model H ("half-frame hybrid", RECOMMENDED): m_frame stays 30 Hz; CLIENT_FPS, LTR, all 0xD9F6xx floats, 0xDC7A8C and
  GameEngine+0x38 stay at stock values; only per-render systems are fixed.
- Model F ("full 60"): m_frame 60 Hz, client half of the timing block rescaled. Analysed by w3d/drw/cua/c0..c3; rejected (section 2.1).
- Cadence codes: R = runs once per render (60/s at 60 FPS), M = keyed to m_frame (30/s under H), T = real time, E = event, L = logic.

---------------------------------------------------------------------------------------------------

## 1. Architecture: frame timing end to end

### 1.1 Main loop (verified by core; SYN re-read 0x6325A0..0x632708 and 0x63A196..0x63A1FE)

```
GameEngine::execute 0x639CF8  (FUN_00639FA7 is only its catch funclet)
 loop:
  Win32GameEngine::update 0x44181F
    0x441822  call 0x6325A0            ; GameEngine::update (only caller)
      0x6325A8  [0xDEF548]->vt28        ; asset/other per-frame service
      0x6325B9  ScriptEngine 0x604189 ; 0x6325C4 0x603452 -> bl (paused/halt flag)
      0x6325CF  call [eax+0x9C]         ; clientUpdate = 0x632409 (RENDER PATH, see 1.2)
      if bl:  0x6325DE GC+0xC8=0, Debug vt94, return            ; halted: no stepper
      0x6325FB  GC+0xC8 = 1
      0x632604  if s==6 && +0x40: +0x38 = CLIENT_FPS/LTR (=6), +0x40=0
      0x632622  s = s+1 -> +0x34
      0x63263A  FUN_006251A3(logicFrame*10-1+s)       ; debug command scheduler (empty in retail)
      0x632642  FUN_0063256F: +0x3C = clamp(s/+0x38,0,1)  ; interpolation fraction
      0x63264A  if s<=6: 0x6326ED vt98(s)                  ; logic sub-step s
                else (tick): rate stats every 25 ticks (timeGetTime, +0x50..+0x5C);
                     edi=s; s=1; frac; vt98(1);
                     if GC+0xC8==0: s=edi (=7, so every frame re-attempts), frac   ; logic did not advance
                     else +0x40=1
  limiter (execute tail) 0x63A196..0x63A1FE:
     edi=timeGetTime; P=_ftol2(1000/(fpsLimit[+0xC]*speedMult[0xD9F498]))  (0x63A1AD) = 33 at 30
     stats 0xDE4314/0xDE430C/0xDE4310; busy-wait Sleep(0)+timeGetTime until now-last>=P; last=now (0x63A1F8)
     enable flag 0xDE4320 (GlobalData.UseFPSLimit +0x26); off for camera time multiplier>1, isTimeFast,
     GlobalData+0xBBD fast-forward, and MP skip window (0x63A17B, clientFrame < [0xDE4308]+6); forced on in MP / speed!=1.
```
vt98 = 0x6329B0 -> TheGameLogic(0xDE412C)->vt34(s) = GameLogic::update(sub) 0x62E4E8 (+ catch-up loop 0x632A95..0x632AEE only when CLIENT_FPS/LTR<6).
Stock timing: 33 ms per frame -> 30.30 FPS, 6 frames per tick -> 5.05 logic Hz (198 ms tick).

### 1.2 Render path (clientUpdate 0x632409, once per GameEngine::update)
1. 0x632423..0x632440: if GC+0xC8 (set by the PREVIOUS frame's stepper): setFrame(getFrame()+1) -> m_frame++.
2. 0x632443 TheAptPlayer(0xDE3F0C)->vt28 = 0x624EE1 (APT, real-time accumulator).
3. 0x63248D TheRadar; 0x632498 TheGameClient->vt28 = GameClient::update 0x64849E; 0x6324A1 message stream; 0x632501 TheAudio(0xDE42FC)->vt28 (Miles update 0x461DA6).
4. GameClient::update 0x64849E (SYN read `data/decompile/gameclient_update.c`):
   LookAt per-frame 0x83B471, LivingWorld camera 0x8392A7, 0x50EB3C; per-render subsystem vt28 calls on 0xDE3B54, 0xDE3C24,
   0xDE7734, 0xDE4EFC (snow/cloud/cloud-break managers etc.), TheAnim2DCollection 0xDE4AB0, TheDisplayStringManager-area 0xDE4334,
   0xDE8B34, TheEva 0xDE3670 (call at 0x6485F1), 0xDE36E0, FUN_00782C56, 0xDE495C, 0xDF06F8, FUN_00532D6F, debug hitch GD+0xC78;
   DRAWABLE BLOCK only if m_frame changed since last time (DAT_00D9F6F8 != m_frame) and not paused/frozen/loading:
     on isTick (FUN_0063252F: s==1): 0xDE4BD0 vt18, per drawable FUN_0068D8F7/FUN_00678F54; every drawable Drawable::updateDrawable 0x675996;
     0xDE8D68 vt28; FUN_0064594B.
   then FUN_0062B385; 0xDE4AC8+4 vt28 (terrain visual -> water 0x500137); TheDisplay(0xDE4418) vt28 = W3DDisplay::update 0x4430A7
   (time-of-day 0x442E56); TheDisplay vt30 = W3DDisplay::draw 0x44B788; 0xDE4518 vt28; TheInGameUI 0xDE7890 vt28 = 0x6A1F4D
   (-> TheControlBar 0xDE7744 vt28 at 0x6A21B0 = 0x71FC08; TheAptPalantir 0xDE4A70 vt28 at 0x6A23D7; 0xDE3D6C vt28); 0xDE4830 vt28; vt90.
5. W3DDisplay::draw 0x44B788 (SYN disassembled 0x44B7F8..0x44BC5F):
   - 0x44B807 frozen = (GC+0xC8==0); d = m_frame - [0xD98C8C] (0 if frozen); 0x44B90C terrain tracks update 0x483870 (sync based);
   - fast-forward branch 0x44B8A2..0x44B8EF (script time-fast or GD+0xBBD; render only if m_frame%30==0, else updateCameraMovements + sync+=33);
   - 0x44B911: syncAcc[0xDC7580] += [0xDC7A8C](33)*d; WW3D::Sync 0x516E20 -> PrevSync 0xDD1E10 = Sync 0xDD1E0C; Sync = syncAcc;
   - camera time multiplier (TacticalView 0xDE447C vtDC) render skip 0x44B95F..0x44B98A; else 0x44B98C prev=now-30;
   - inner limiter loop 0x44B9B8..0x44B9D8 (wait >=29 ms while UseFPSLimit) -> drawFrame 0x449CF8; loop repeats only while
     frozen && camera move unfinished && isGamePaused (0x44BBFE..0x44BC20).
   - frozen flag is used ONLY for d, the fast-forward branch and that loop condition (SYN).
6. drawFrame 0x449CF8: if !GameLogic+0x125: updateViews (vtA0 -> W3DView::update 0x48BCF2 -> updateCameraMovements 0x48A953, camera
   ease/shake/scroll application, drawable pass via 0x485329 -> Drawable::draw 0x67C482 ONLY if SyncTime != PrevSyncTime (0x48C701)),
   0x479FC4; FX particle manager 0x449D40 [0xDE3744]->vt28 = 0x5F5123; tree buffer 0x449D55 -> 0x4684FB -> 0x4ED508;
   UI particles 0x6A536C; scene render, Present.

### 1.3 Logic tick (GameLogic::update(sub) 0x62E4E8; core, c1 X2; SYN re-read `data/decompile/gamelogic_update.c`)
- sub 1: logic frame++ (+0x40) if advancing, scripts, Lua, recorder, command list, network vtAC.
- sub 2: DAT_00DE4354/4360 vt28 + per-object pass. sub 3/4: update-module bucket 0 halves; sub 5: buckets 1-2, TheAI + 13 subsystems;
  sub 6: bucket 3. sub >= 7 selects no bucket (lines 224-235: start=0,end=0).
- EVERY call regardless of sub: FUN_005FF9D3, FUN_006F2364 (path-request servicing with a fixed per-call budget, line 94), FUN_00440809,
  FUN_00629DA6, QPC. => Calling GameLogic::update more often, or with sub 7..12, changes logic results.
- Frozen-time handling (lines 29-58): at sub 1, when the camera/time is frozen and the 0x1D check passes, the full sub-1 path runs
  WITHOUT frame++. So the NUMBER of failed tick attempts during frozen time affects logic. Stock: one attempt per render (30/s).
- Logic RNG is seeded separately (SYN): logic seed 0xDA1CA4 (FUN_006D328E int, 0x6D332C real, 0x6D3261), client seed 0xDA1C74
  (0x6D32E4, 0x6D33AB, RandomVariable 0x6D343B used by particles/fire), audio seed 0xDA1C8C. Core LCG FUN_006D315D(ECX=seed).

### 1.4 Interpolation (drw D01/D02, core)
- Keyframes once per tick: GameLogic::update calls FUN_00674B1F per drawable (prev matrix +0x3AC, cur +0x3DC, Catmull-Rom +0x40C..+0x430).
- Per draw: Drawable::draw 0x67C482 -> getTransformMatrix 0x67C1FB -> 0x6765B9 (matrix lerp + Catmull-Rom with fraction GE+0x3C,
  cached in +0x208 keyed by m_frame in +0x244) and instance matrix 0x67171D (cache +0x204, expiry +0x378 = m_frame + GE+0x38),
  then applyPhysicsXform 0x67BE90.
- Fraction readers (lerp, shift on s==1 or frac==1.0): 0x48E3C4, 0x4B5241, 0x4B6C21, 0x4B6F86, 0x67175A, 0x67668D, 0x67673E, 0x6767D1, 0x8A0382.

### 1.5 W3D sync clock
- 0xDC7A8C = TheW3DFrameLengthInMsec = 33 (static init 0xBC146C/0xBC1478 = 1000/CLIENT_FPS; W3DGameClient::setFrameRate 0x44BF5C
  (cvttss2si) from GameClient::init 0x646781 with [0xD9F620]).
- Consumers: 132 refs to 0xDD1E0C/0xDD1E10 (model animation 0x4BF560 via 0xDC7580 delta, UV mappers 0x581B60.., SegLine,
  terrain tracks, water UV, snow, shader time, W3D particle buffers) + FX shader params (21 x sync*CLIENT_FPS) + FX particle storage
  (4 x sync*CLIENT_FPS). Stock: 33 ms per 33.3 ms frame = 990 ms/s.
- The 3D-in-UI renderer FUN_004A89BD swaps in its own timeGetTime clock (independent).

### 1.6 Particles (ptx; SYN fixed identity)
- TheFXParticleSystemManager = 0xDE3744, created through GameEngine vt+0x8C (0x63B595, registered 0x63B5A2); W3D subclass vtable
  0xBDAC10 (ctor 0x44C6AC), base 0xBF7A00; update vt+0x28 = 0x5F5123 (both vtables, SYN). Called per draw at 0x449D40 (and stub 0x444CF2).
  Inside: optional m_frame-change gate only when GlobalData+0xD45 (Create-a-Hero) (0x5F5168/0x5F516E); per-system update if
  sys+0x98 (update while halted) OR (GC+0xC8 && !GameLogic+0x125) (0x5F519D).
- 0x624EE1 (vtables 0xBDB780/0xBFD1F0) is the APT player, NOT the particle manager (w3d W3D-08 is wrong).

---------------------------------------------------------------------------------------------------

## 2. Core patch set (model H) and network detection

### 2.1 Model decision (conflict resolution)
Conflict: core/hc/ptx want m_frame at 30 Hz (H); w3d/drw/cua/c0/c1/c2/c3 assumed m_frame at 60 Hz with rescaled client globals (F).
Decision: H. Reasons (verified):
1. GC+0xC8 has only four readers (SYN scan of listing): 0x44B807 (draw frozen -> d), 0x5F519D (particles), 0x632429 (m_frame++),
   0x6326D1 (stepper restore). Writers: 0x6325DE/0x6325FB/0x632AFC (stepper), 0x62E5D1/0x62E5EF/0x62EE3E (GameLogic). Setting it to 0 on
   B-frames therefore only holds m_frame, the sync delta and particles at 30 Hz - exactly the intent.
2. Under H about 100 getFrame users (hc: 100 call sites / 75 functions) - fades, tints, flashes, Anim2D, BirthFade, decal envelopes,
   radar, move hints, InGameUI messages, cloud/fire managers, particles, MMB click, floating text - keep stock wall-clock timing with
   zero patches. Under F every one needs a patch, plus pinning 25 FX operands, 0xDC7A8C, 0xDE87DC, the LTR/CLIENT_FPS ratio in
   0x6732DC, and static-init changes.
3. m_frame is saved (GameClient::xfer 0x647ABF, 0x647AF8) and absolute m_frame keys are saved by W3DScriptedModelDraw xfer
   0x4C52FE and Drawable fades; H keeps save files in stock units (requirement 3).
4. The smoothness objections against H (drw, w3d) are answered by C3 (sync advances every render), C4 (interp caches re-keyed to a
   render id) and C5 (physics replay). Remaining 30 Hz-stepped visuals are listed in section 6 (R7).
Consequences: keep CLIENT_FPS=30, LTR=5, 0xD9F610..0xD9F62C, 0xDC7A8C=33, GE+0x38=6, FramesPerSecondLimit=30 (the limiter hook
halves the period). The 111 "client_scale" refs from classification mostly become "keep" (section 4.3).

### 2.2 Patch list (original bytes verified by SYN dump unless noted)
All stubs read one DLL flag `g_m60` that changes ONLY when s==1 (just ticked) - see 2.3.

C0 Render-phase hook (new, SYN): 0x6325CF `ff 90 9c 00 00 00` (call [eax+0x9C]) -> `e8 <stub> 90`.
   Stub (ECX=engine, EAX=vtable): before calling clientUpdate read b=[[0xDE4388]+0xC8]; g_isARender=b; g_renderId++ (skip 0xFFFFFFFF);
   g_uiTick = running ? b : (g_uiTick^1) (alternates while paused/frozen); then call [eax+0x9C].
   g_uiTick is the gate used by all per-render fixes below ("run on A-renders, every other render while paused").

C1 Stepper (in-place variant, preferred because it keeps MP stall, rate stats and debug calls as stock; core's alternative is to
   redirect the call at 0x441822 `e8 79 0d 1f 00` to a full reimplementation of 0x6325A0):
   a. 0x632606: imm of `83 f9 06` (cmp ecx,6) 06 -> 0C in 60 mode (refresh of +0x38 at end of cycle; VALUE stays CLIENT_FPS/LTR = 6).
   b. 0x632642 `e8 28 ff ff ff`, 0x6326BE `e8 ac fe ff ff`, 0x6326E4 `e8 86 fe ff ff` (call 0x63256F) -> FracStub:
      g_m60 ? [ecx+0x3C] = min(s,12)/12.0f : jmp 0x63256F.
   c. 0x63264C: imm of `83 f8 06` (cmp eax,6 / jle) 06 -> 0C in 60 mode (tick when s>12).
   d. 0x6326EB..0x6326F5 `8b 16 50 8b ce ff 92 98 00 00 00` -> `e8 <SubStub> 90*6`; SubStub(ESI=engine, EAX=s):
      !g_m60: vt98(s). s odd: vt98((s+1)/2); g_lastStepB=0. s even: byte [[0xDE4388]+0xC8]=0; g_lastStepB=1 (no logic call).
   e. 0x6326DF..0x6326E8 `8b ce 89 7e 34 e8 86 fe ff ff` (restore after failed tick; keep the `eb 0b` at 0x6326E9) ->
      `e8 <RestoreStub> 90 90 90 90 90`; RestoreStub(ESI=engine, EDI=old s): [esi+0x34] = g_m60 ? 11 : edi; then fraction (as C1b). (Restoring 11 instead of 13 makes failed tick attempts happen every OTHER render,
      i.e. 30/s as in stock; needed because of the frozen-time behaviour in 1.3. Core proposed "7->13", which doubles attempts.)
   f. Debug id 0x632631 `6b c9 0a`: optional imm 0x14 in 60 mode (cosmetic; or pass ceil(s/2)).
   Keep: +0x38=6, CLIENT_FPS, LTR; FUN_0063252F (isTick) stays correct (uses CLIENT_FPS/LTR=6 -> s==1).
C2 Limiter: 0x63A1AD `e8 f2 2d 40 00` (call _ftol2 0xA3CFA4) -> call LimStub: eax=_ftol2(st0)=P; g_m60 ? (toggle ? P-P/2 : P/2) : P.
   P=33 -> 16/17 alternating (33 ms per pair, exactly stock). Works for any setFPS caller (script 0x7CCF20, credits 100 at 0x91B6DC).
   STRONGLY RECOMMENDED addition (deadline pacing): 0x63A1F8 `89 3d 18 43 de 00` (last=now) -> stub: last = (now-last < P+slack) ?
   last+P : now. Under H the A-frame carries a logic sub-step plus a render and the B-frame only a render; with last=now an A-frame
   overrun is never recovered and the game runs slow (section 6 R3).
C3 W3D sync per render: 0x44B911 `a1 8c 7a dc 00 0f af c6 01 05 80 75 dc 00` -> `56 e8 <SyncStub> 01 05 80 75 dc 00 90 90`
   (core). SyncStub(d) stdcall returns delta: !g_m60: 33*d. d>0: 33*d - pending; pending=0. d==0 && g_lastStepB (consume flag):
   h = 16/17 alternating; pending=h; delta=h. else 0. EDX is dead (clobbered by timeGetTime at 0x44B946).
   Result: syncAcc equals stock at every A-render and changes on every running render, so the W3DView drawable pass (0x48C701) runs
   at 60 Hz. Use the immediate 33, not [0xDC7A8C]. Fast-forward path 0x44B8DD stays stock (60 mode off there).
   (w3d's variant `(33*esi+rem)>>1` assumes m_frame at 60 Hz; it does nothing on B-renders under H - CONFLICT resolved for core's version.)
C4 Interp cache re-key (core): 0x6765D5 `8b 0d 88 43 de 00 8b 01 53 ff 50 7c` -> `53 a1 <&g_renderId> 90*6`;
   0x67173B `8b 0d 88 43 de 00 8b 01 ff 50 7c` -> `a1 <&g_renderId> 90*6`; 0x671774 `8b 0d 88 43 de 00 8b 01 83 c4 10 ff 50 7c`
   -> `83 c4 10 a1 <&g_renderId> 90*6`. Leave the expiry 0x671720..0x67172B on m_frame. Harmless in 30 mode.
C5 Client physics replay (new requirement found by SYN): calcPhysicsXform 0x67BDC0 (prologue `55 8b ec`) returns 0 when
   loco+0xAC == m_frame; applyPhysicsXform then skips the whole xform (0x67BF40 `je 0x67C1F6`). The output IS applied (0x67BF49
   `fld [ebp-0x14]`, [ebp-8]); Ghidra's "sin(0.0)" decompile is an artefact. GlobalData+0x9B0 (ShowClientPhysics) = 1 by default
   (0x642DE0). Under H every B-render would draw units without pitch/roll/bob (flicker).
   Fix: detour 0x67BDC0: r=orig(); if r: cache[loco]={out[4], m_frame}; else if g_m60 && cache[loco].frame==m_frame: copy, return 1.
   Keep the cache in a DLL-side table (do NOT enlarge the 0x58 DrawableLocoInfo allocation - Drawable::xfer 0x67A544 risk).
C6 Inner draw limiter, switched with g_m60 (camera steps are halved globally, so the paused-camera loop must render twice as often):
   0x44B98E `e2` -> `f1` (prev=now-15), 0x44B9C3 `1d` -> `0e`. Write back stock bytes when leaving 60 mode.
C7 Particle manager gate: 0x449D40 `8b 0d 44 37 de 00 8b 01 ff 50 28` (and the stub at 0x444CF2, `... ff 60 28` tail jmp) -> call only
   when g_uiTick. Normal systems are already 30 Hz under H through GC+0xC8; this also brings sys+0x98 systems (update while halted)
   to 30 Hz, including during pause. (ptx's alternative `0x5F516E 74 1f -> 90 90` freezes +0x98 systems while paused.)

### 2.3 Mode controller (when g_m60 may be on)
Evaluate only at s==1 after a successful tick (also on map load / GameEngine reset 0x635D11, where s is reset).
g_m60 = user enabled && [0xDE4468]==NULL (TheNetwork; created only by LAN/online start paths, cleared on reset 0x635DA4)
        && !FUN_00441B7C(TheGameLogic) (mode +0x110 is 1 LAN or 5 Internet) && byte[GlobalData(0xDE4364)+0xBBD]==0 (fast-forward)
        && !ScriptEngine isTimeFast (FUN_00603491, used at 0x44B897) && TacticalView(0xDE447C)->vtDC() <= 1 (time multiplier).
Optional first release: also require GameLogic+0x110 != 4 (shell map), which avoids the shell-only issues (audio shell dt, window
transitions, shell throttle). Switching writes the imm bytes of C1a/C1c/C6 (single-byte writes, done on the game thread inside the
stepper stub). Pause (+0x124) and frozen time do not switch modes. Replays: unverified (section 6).

---------------------------------------------------------------------------------------------------

## 3. Inventory of client-side systems affected by frame rate (deduplicated)

"Unpatched (H)" = behaviour with C0..C7 applied but this system untouched. Conflicts marked CONFLICT.

### 3.1 Core time base
| # | System | Key functions / sites | Mechanism | Cad. | Unpatched (H) | Fix (H) | Patch sites | Conf | Agents |
|---|---|---|---|---|---|---|---|---|---|
| 1 | Frame limiter | 0x63A196..0x63A1FE | P=_ftol2(1000/(fps*mult)), busy wait, last=now | T | stock 30 FPS | C2 16/17 + deadline pacing | 0x63A1AD, 0x63A1F8 | high | core, c0 E1 |
| 2 | Sub-frame stepper | 0x6325A0, 0x63256F, 0x63252F | s++, frac, tick at s>6 | R | limiter alone: logic 10 Hz | C1 | 0x632606, 0x632642, 0x63264C, 0x6326BE, 0x6326DF, 0x6326E4, 0x6326EB | high | core, c0, c1 X2 |
| 3 | GameLogic sub-step split | 0x62E4E8 | sub 1..6 work split; FUN_006F2364 per call | stepper | sub>6 or extra calls change logic | only 1..6 once each; failed-tick retries 30/s | C1d, C1e | high (SYN) | core, c1 X2; CONFLICT drw D01, c0 (patch 6->12 and pass s to logic) |
| 4 | Client frame counter | GC+0x10; 0x632433..0x632440; xfer 0x647ABF | +1 per render if GC+0xC8 | R->M | (with C1) 30 Hz | keep 30 Hz | C1d | high | core, hc HC01, ptx; CONFLICT w3d, drw, c0..c3, cua (assumed 60) |
| 5 | GE+0x38 frames/tick | readers 0x4CC4B9, 0x4E5F40, 0x4E6490, 0x6716DD, 0x69DBC9, 0x69DCD0, 0x85EEA1, 0x85EF14, 0x860D43, 0x8CD5FA, 0x62C16E (debug), 0x63239D (MP) | client frames per tick in m_frame units | - | correct | keep 6 | none | high | core; CONFLICT drw D03, w3d W3D-12, c0 E7, c2 X2-5 (12 under F) |
| 6 | Fraction / isTick readers | 0x63256F, 0x63252F + 9 readers (1.4) | lerp, shift at s==1 | R | correct with C1b | frac=s/12 | C1b | high | core, drw D02 |
| 7 | Drawable interp caches | 0x6765B9 (+0x244), 0x67171D (+0x204) | cache keyed by m_frame | R | units move at 30 Hz | key by g_renderId | C4 | high | core |
| 8 | W3D sync clock | 0x44B788 (0x44B911), 0x516E20 | sync += 33*d | R | B-renders: no anim step, drawable pass skipped | C3 | 0x44B911 | high | core, w3d W3D-01 (formula CONFLICT), hc HC02, drw D04 |
| 9 | Drawable pass gate | W3DView::update 0x48BCF2, 0x48C701 | draw drawables only if Sync changed | R | see 8 | needs C3 | none | high | drw D04, w3d W3D-06 |
| 10 | 0xDC7A8C frame length | 0xBC1478, 0x44BF5C, 0x646781 | 1000/CLIENT_FPS truncated | - | stays 33 under H | keep (never rescale CLIENT_FPS/0xD9F620) | none | high | w3d W3D-02, c0 E3, cua CAM-7 |
| 11 | Client physics xform | 0x67BDC0, 0x67BE90, integrators 0x670B11, 0x67B7AA, 0x670E09, 0x677026, 0x6732DC | springs per m_frame; returns 0 if m_frame unchanged -> xform skipped | M | pitch/roll/bob vanish on B-renders | C5 replay | 0x67BDC0 | high (SYN) | drw D12, c1 X5; CONFLICT w3d W3D-28 ("constant zero") |
| 12 | Draw inner limiter | 0x44B98C, 0x44B9C1 | paused camera loop 29 ms | T | paused camera moves half speed (steps halved) | C6 | 0x44B98E, 0x44B9C3 | high | core, w3d W3D-04, cua CAM-8, c0 E2; CONFLICT hc HC23 (leave) - resolved: change, because camera is halved |
| 13 | Fast-forward / time multiplier | 0x44B8C5 (%30), 0x44B95F, 0x639FEF | render skip | - | 60 mode off there | none | - | high | w3d, cua CAM-9, c0 E2 |
| 14 | Vsync | 0xDD302C=0 (0x524B5C), Set_Swap_Interval 0x522460 unused | DEFAULT interval | - | 60 Hz display: 60.00 FPS = -1% vs 60.6 | decision (section 6) | - | medium | core |
| 15 | MP multiplier / stall | 0xD9F498, 0x63239D, 0x63A17B | MP only | - | n/a | stock path | - | high | core, c0 E8, hc HC24 |
| 16 | Debug frame id | 0x6251A3 via 0x632631 | lf*10-1+s | R | ids overlap (debug only) | C1f | 0x632633 | high | core, c0 E6, hc HC25 |

### 3.2 Render / W3D
| # | System | Key functions / sites | Mechanism | Cad. | Unpatched (H) | Fix (H) | Patch sites | Conf | Agents |
|---|---|---|---|---|---|---|---|---|---|
| 17 | Model animation, crossfade | 0x4BF560, 0x4BFAF4, 0x4C72BE | delta of syncAcc ms | R | correct with C3 | none | - | high | w3d W3D-05, drw D25 |
| 18 | Other SYNC users | 0x581B60.., 0x5909A0.., 0x483870, 0x495228, 0x4943E1 (snow), 0x54CEC5, 0x5A1D60.. | ms deltas | R | correct with C3 | none | - | high | w3d W3D-06 |
| 19 | FX shader params, FX particle storage | 21 FIMUL 0x4801D5..0x48128C; IMUL 0x7B1102, 0x7B1160, 0x7B12FE, 0x7B140B | sync*CLIENT_FPS/1000 | R | correct (CLIENT_FPS stays 30) | none (F: pin all 25) | - | high | w3d W3D-07, ptx P8, c0, c2 X2-7, drw D27 |
| 20 | 2nd material pass opacity | Drawable::draw 0x67C4C7 | x0.8 per draw | R | 2x | operand -> 0.8944272 (0xBDE8D8 shared, 33 refs) | 0x67C4CB disp32 `d8 e8 bd 00` | high | w3d W3D-09, drw D11, c0 E4 |
| 21 | Water river UV | 0x500137 (via 0x4916D9) | +8.25e-5/+1.65e-4 per call | R | 2x | data: 0xBE5608=4.125e-5, 0xBE5604=8.25e-5 (single-use); 0xBE560C=16.6667 if GD+0xD45 | .rdata after VirtualProtect | high | w3d W3D-10 |
| 22 | Vapor/hilightfilter overlays | 0x4F9871, 0x4F6B8D, 0x4F81D7 | +0.6 anim frame (0xDD1B1C), UV += consts per Render | R | 2x | operands -> 0.3/0.015/0.01125/0.01 | 0x4F9904, 0x4F9A0D, 0x4F9A22, 0x4F9A34, 0x4F6C86, 0x4F8321 | medium | w3d W3D-11, hc HC19 |
| 23 | Water shore-wave tracks | 0x4FDB07 (from flush 0x4FF9A8) | +33 ms per call | R | 2x | cave 16/17 (or imm 0x10, 3% slow) | 0x4FDB4A `83 46 74 21` | high | hc HC15 |
| 24 | Trees: sway, push-aside, sink, topple | 0x449D55 -> 0x4684FB -> 0x4ED508 (0x4E5D2C, 0x4EA107); shrubs 0x4E837E (caller 0x46B0A0) | per call, normalised by +0x38 (cached +0x5BC) | R (gated only by pause/freeze; SYN) | 2x | gate call at 0x449D55 with g_isARender (exact); smooth alternative: vegScale 12 + topple caves (W3D-13) | 0x449D55 `e8 a1 e7 01 00`; shrub caller unverified | high / medium (shrubs) | w3d W3D-12/13; CONFLICT w3d "correct if +0x38=12" (F only) |
| 25 | Terrain/selection decals | 0x732926 | throb/rotation from m_frame; spiral -= accel per call | R/M | throb/rotation OK (30 Hz steps); spiral 2x | spiral x0.5 cave at 0x732A84 | 0x732A84 | high | w3d W3D-14, drw D19, c1 |
| 26 | Shadow / decal envelopes | 0x4C451D -> 0x7323CE; eval 0x50B85D | ms*0.03 -> m_frame keys | M | correct | none | - | medium | w3d W3D-15, c0 |
| 27 | BirthFadeTime colour fade | 0x4C091D, 0x4C72BE | logic-parsed count vs m_frame delta (stock quirk 6x) | M | correct | none | - | high | w3d W3D-16, drw D14, hc HC11 |
| 28 | Model transform blend +1/30 | 0x4B686D (0x4B6B63) | per m_frame change | M | correct | none (F: operand -> 0xBDC1FC) | - | high | w3d W3D-17, drw D13, hc HC12 |
| 29 | Weapon recoil | 0x4B4699, call 0x4C78CE | integrator per draw | R | 2x | gate call with g_isARender (exact, bones persist) | 0x4C78CE `e8 c6 cd fe ff` | high | w3d W3D-18, drw D15 |
| 30 | W3DLightDraw flicker/pulse | 0x4CF2B5 | +1/LTR per draw | R | 2x | cvtsi2ss operands -> DLL int 10 | 0x4CF2BF, 0x4CF3D3 (disp32 of `f3 0f 2a 05 08 f6 d9 00`) | high | w3d W3D-19, drw D18, c0 |
| 31 | Rope, tank treads, truck cab/tires, boat wake, debris | 0x4CA481, 0x4CDBD7, 0x4CBFFB (tire /+0x38 at 0x4CC4B9), 0x4D052E, 0x4B135C | per draw | R | 2x (debris cosmetic) | gate integration with g_isARender, or caves (W3D-20/D16/D17); check AotR INI usage first | 0x4CDDCB, 0x4CA59E, 0x4CC4B9, 0xD9A3A0 | medium | w3d W3D-20, drw D16/D17/D22/D23 |
| 32 | Cloud effect manager | 0x497D00 / 0x494DBA | gated on m_frame change | M | correct | none (F: >>1 at 0x497D0C) | - | high | w3d W3D-21, hc HC07 |
| 33 | Fire manager | 0x49829C | m_frame intervals | M | correct | none (F: interval x2 at 0x49830B) | - | high | w3d W3D-22 |
| 34 | Time-of-day lerp | 0x442E56 | (m_frame-start)*0.0333 | M | correct | none | - | medium | w3d W3D-24, c0 |
| 35 | Outline pass fade | 0xB53A2B (render slot 0xB53BA6) | +-1/60 per render | R | 2x | operands -> 1/120 (0xBDC1FC shared) | 0xB53A5C, 0xB53A81 | medium | hc HC20 |
| 36 | FXList LightPulse | W3DDynamicLight::On_Frame_Update 0x46D665; create 0x46D7A9 | counters per scene render | R | 2x | double +0x148/+0x14C/+0x150/+0x154 at creation | hook 0x46D7A9 | medium | ptx P10 |
| 37 | FX particle manager | 0x5F5123 at 0x449D40/0x444CF2 | normal systems need GC+0xC8; +0x98 systems ungated | R | normal 30 Hz OK; +0x98 systems 2x | C7 | 0x449D40, 0x444CF2 | high | ptx P1; CONFLICT w3d W3D-08 (named 0x624EE1 = APT) |
| 38 | Particle sim details | P2..P7, P9 (timestamps, physics, size, keyframes, lifetimes, wind, FXList InitialDelay) | per particle update | M (via 37) | correct | none | - | high | ptx |
| 39 | Particle 60 Hz look (optional) | draw vt+0x10 0x961DFC.. (vtable entries 0xC326C8, 0xC32734, 0xC32794, 0xC327F0, 0xC32854, 0xC328B4) | 30 Hz positions drawn twice | R | judder of fast sparks | optional half-velocity extrapolation wrappers | listed | medium | ptx P13 |
| 40 | Real-time render systems | letterbox 0x443939, shroud 0x472CF2/0x473AAB, VP6 video, avg FPS 0x4430BB, 3D-in-UI 0x4A89BD | timeGetTime/QPC | T | correct | none | - | high | w3d W3D-27, cua |

### 3.3 Drawable family (all inside Drawable::updateDrawable, which GameClient::update runs only when m_frame changed - SYN)
| # | System | Key functions | Mechanism | Cad. | Unpatched (H) | Fix (H) | F-only fix | Conf | Agents |
|---|---|---|---|---|---|---|---|---|---|
| 41 | Fade in/out | 0x670A50, 0x670AA2, 0x67309D, update 0x675AC3 | elapsed += m_frame delta; ~37 callers in mixed units | M | correct | none | central x2 at 0x670A87/0x670AD9/0x6730B2 OR per caller (never both) | high | drw D05, c0 E5, c2 X2-3, c3 drawable_fade/builder_fade |
| 42 | Tint envelopes, colour flash | 0x674818, 0x6748B7, 0x671C25, 0x671CBA, 0x675749 | per call | M | correct | none | 2n / sustain x2 | high | drw D06, hc HC03, c1 X3 |
| 43 | Flash period | 0x675D5B / 0xDE45D8 (=15) | m_frame % 15 | M | correct | none | 0xDE45D8=30, keep 0xDE87DC=15 | high | drw D07, c2 X2-2, hc HC04 |
| 44 | Decal opacity +0xF4, shader effect +0xE0, opacity pulse | 0x675BD0, 0x675C51/0x67093E, 0x673114/0x6760F9 | per call / m_frame delta | M | correct | none | rates x0.5, 4 operands | high | drw D08-D10, c1 |
| 45 | Sway client update | 0x8CD675 / 0x8CD55F | /+0x38 | M | correct | none | +0x38=12 | high | drw D24 |
| 46 | Anim2D icons | 0x6D7F0A | m_frame delta vs logic-parsed delay | M | correct | none | cave 0x6D7F18 | high | drw D20, hc HC10, c1 X8 |
| 47 | Icon blink, health/pips, damage icon, horde RO | 0x677C0C, 0x673B2E, 0x6740F5, 0x479822 | m_frame / logic | M/L | correct | none | 0x677C98 | high | drw D21/D26 |

### 3.4 Camera
| # | System | Key functions / sites | Mechanism | Cad. | Unpatched (H) | Fix (H) | Patch sites | Conf | Agents |
|---|---|---|---|---|---|---|---|---|---|
| 48 | Follow/lock ease | 0x48BCF2: 0x48BE98, 0x48BEA2, lerp 0x48BFE8 | factor +0.05/call, pos lerp | R | 2x | 0.025 consts + f'=1-sqrt(1-f) | 0x48BE98, 0x48BEA2 (`... 60 d7 bd 00`), 0x48BFE8 | high | cua CAM-1 |
| 49 | Tether ease | 0x48BFCB (0.01), CameraEaseFactor GD+0xDE0 | per call | R | 2x | 0.0050126; parser 0xBFF964 or post-INI | 0x48BFCB, 0xBFF964 | medium | cua CAM-2 |
| 50 | Height settle (= wheel zoom smoothing) | 0x48C36A/0x48C3B0 `d8 88 b0 0a 00 00`; CameraAdjustSpeed GD+0xAB0 | k per call | R | 2x | k'=1-sqrt(1-k) (0.1 -> 0.0513167) via parser 0xC003E4 or post-INI | 0xC003E4, 0x642FE5 | high | cua CAM-3 |
| 51 | Camera-zone zoom smoothing | 0x48C553, 0x48C5D6, 0x48C57F, 0x48C601, 0x48C589, 0x48C60B | 0.08, min +-0.01 | R | 2x | 0.0408337, +-0.005 | 6 operands | medium | cua CAM-4 |
| 52 | Legacy view shake | 0x48C126 (x0.75, sign flip) | per call | R | 2x | skip block 0x48C121..0x48C14D unless g_uiTick (exact) | detour at 0x48C10F | high | cua CAM-5, w3d W3D-25 |
| 53 | CameraShakerSystem | 0x5033E5 `d9 05 6c fc bd 00` -> 0x4655F8 | +1/30 s per camera build | R (calls/frame unverified) | 2x | operand -> 0xBDC1FC (1/60, SYN) | 0x5033E7 | medium | cua CAM-6, hc HC16 |
| 54 | Camera Target/Zoom fly transition | 0x502458 | accelerating integrator, .data 0xD9AE94..A0 | R | 2x | data 0.0075 / 0.2 / 0.0125 / 0.25 (approx) | 0xD9AEA0, 0xD9AE9C, 0xD9AE98, 0xD9AE94 | medium | hc HC17 |
| 55 | Scripted camera moves (rotate/zoom/pitch/FOV/rotate-toward, paths 1/2) | setup 0x485DB0, 0x485E5F, 0x48868B, 0x4887DA, 0x48885E, 0x48891C, 0x4889A0; steppers 0x48660E, 0x4866A7, 0x486758, 0x4867F1, 0x48A417; paths 0x489817, 0x48688A | frames=ms/33, cur++ per call; paths +33 ms per call | R | 2x | IDIV->`idiv [G33]; add eax,eax` at 0x485DD4, 0x485E7A, 0x485E9A, 0x4886B1, 0x4887F4, 0x48887A, 0x48892E, 0x4889B2; IMUL -> x16.5 at 0x485F62, 0x486279; PUSH [0xDC7A8C] -> 16/17 at 0x48A9F2, 0x48AAE5, 0x48AB06. NOT 0x48D3C5 (mode 3) under H | listed | high | w3d W3D-03, cua CAM-7, c0 |
| 56 | Mode 3 moveCameraTo, mode 4 camera object | 0x48657E (gated on m_frame change, SYN 0x48A953), 0x48AA62 | per m_frame | M | correct speed, 30 Hz steps | none (optional smoothing: x2 at 0x48D3C5 + per-render gate) | - | high (SYN) | new |
| 57 | Living World camera | 0x8392A7, LW view 0x49B799 (vt 0xBDE918), moveTo 0x6BFB88/0x6BF78E, auto-scroll 0x6BF96D, 0x7FEA48, 0x838D3A | per update | R? | 2x if per render | 0x49B7AD -> 0.25; rot via 0x49B8D9; halve step in 0x6BFB88 once; gate LW zoom | 0x49B7AD | medium | cua INP-5, c1, c2 X2-4 |

### 3.5 Input
| # | System | Key functions / sites | Mechanism | Cad. | Unpatched (H) | Fix (H) | Patch sites | Conf | Agents |
|---|---|---|---|---|---|---|---|---|---|
| 58 | Scroll (RMB/keys/edge) | LookAt 0x83B471 -> scrollBy 0x48C774 at 0x83B8A0; InGameUI 0x6A2291 (250.0 @0xDA0B24) | offset per render (350.0/100.0 shared consts) | R | 2x | halve offset before scrollBy (keep setScrollAmount); 0xDA0B24 = 125.0 | 0x83B8A0 `8d 55 f0 52 ff 50 5c`, 0xDA0B24 | high | cua INP-1, hc HC18 |
| 59 | Keyboard rotate | GD+0xC2C readers 0x6A2205/0x6A222D, 0x83B984/0x83B9B6, LW 0x839641/0x839674 | per render | R | 2x | parser x0.5 at 0xC00864 (or post-INI) | 0xC00864 | high | cua INP-2 |
| 60 | Keyboard zoom | 0x6A225E, 0x6A227B, 0x83B9DD, 0x83B9F4 -> 0x65E803/0x65E82A | step per render | R | 2x | gate the 4 calls with g_uiTick (wheel unchanged) | 4 x `ff 90 34/38 01 00 00` | high | cua INP-3 |
| 61 | Edge rotate (W3DView+0x2449 mode) | 0x83AC4A case 3 | +-0.046/0.02 per message | R | 2x | half consts | - | medium | cua INP-4 |
| 62 | Keyboard auto-repeat | 0x63F473 (Keyboard::update 0x63F667) | input frame per render | R | 2x | 0x63F499 0A->14, 0x63F4D6 0C->13 | `83 fa 0a`, `83 e8 0c` | high | cua INP-6 |
| 63 | Mouse "held" messages | 0x5EDD69 | counter per call ==5 | R | 2x | imm 5->10 at 0x5EE0BF, 0x5EE0D5, 0x5EE0E5, 0x5EE12F | `83 f8 05`.. | high | cua INP-7 |
| 64 | MMB click window | 0x83AD70 | m_frame delta < 5 | M | correct | none (F: 5->10) | - | high | cua INP-8, hc HC09 |
| 65 | Double-click, drag ms, wheel, MMB drag | 0x81FAD4, 0x83AC4A | OS events / deltas | E | correct | none | - | medium | cua INP-9 |

### 3.6 UI, Eva, audio, AotR
| # | System | Key functions / sites | Mechanism | Cad. | Unpatched (H) | Fix (H) | Patch sites | Conf | Agents |
|---|---|---|---|---|---|---|---|---|---|
| 66 | APT movie playhead | 0x624EE1 -> 0xAE18C0 | timeGetTime accumulator, clamp 60 ms | T | correct | none | - | high | cua UI-1 |
| 67 | Palantir HUD + hero bar (+ AotR hook) | call 0x6A23D7 -> 0x6D7B33 -> 0x6D7647; 0x6D577C (AotR jmp at 0x6D57AF), timers 0x6D72DF, count-up 0x6D57B4, hero bar 0x92CF64 | counters/steps per update; AotR [0xED0000] every 30 calls | R | 2x | gate the call at 0x6A23D7 with g_uiTick (covers hero bar and AotR counter; no AotR bytes touched) | 0x6A23D7 `8b 0d 70 4a de 00 8b 01 ff 50 28` | medium | cua UI-2, c1 X9, c2; CONFLICT cua ("correct if CLIENT_FPS=60", F only) |
| 68 | ControlBar cameo flash | TheControlBar 0xDE7744 update 0x71FC08 (called at 0x6A21B0), FlashButton 0x9312B9 | per render: if m_frame%10==0 count-- | R+M | H: matching m_frame seen on 2 renders -> 2x (SYN) | wrap getFrame: return m_frame|1 on B-renders | 0x71FD5D, 0x9314DE (`8b 0d 88 43 de 00 8b 01 ff 50 7c`) | high | hc HC06, c2 X2-1, cua UI-7 |
| 69 | Move hints | 0x48EDED | m_frame <= 40 | M | correct | none (F: 0x48EE29/2D/3B) | - | high | cua UI-3, hc HC05 |
| 70 | In-game message fade | 0x6A1F4D, ftol call 0x6A2012 | alpha -= per call | R | 2x | stub returns 0 unless g_uiTick | 0x6A2012 `e8 8d af 39 00` | high | cua UI-4 |
| 71 | Floating text | add 0x6A1778, update 0x69DB40 (m_frame gated), draw 0x69DC5B | | M | correct | none | - | high | cua UI-5, c1 X6, w3d W3D-26 |
| 72 | World animations rise | 0x69DEA9 | z += rise/LTR per render | R | 2x | operand -> DLL int 10 | 0x69DF12 (disp32 of 0x69DF0E) | high | cua UI-6, c1, hc HC21 |
| 73 | UI particles / particle cursor | 0x6A536C (from 0x449CF8), add 0x6A1978 | pos+=v, v*=d per render; expiry on m_frame | R | integrator 2x | run integrate block only on g_uiTick (expiry/fade OK) | in 0x6A536C | high | c1 X7, hc HC21, w3d W3D-26 |
| 74 | Placement ghost blink | 0x6A2AE5 | m_frame & 4 | M | correct | none | - | high | cua UI-8, hc HC08 |
| 75 | Radar events, pulses, icon refresh | 0x6D8E2B, 0x6D93EB, 0x6D9B4D, 0x44D664, 0x44DFBA, 0x44FDF3 | m_frame | M | correct (%6 refresh double-fires: CPU only) | none | - | high | cua UI-9, w3d W3D-23, c0, c1 |
| 76 | Window transitions | GameWindowManager::update 0x6C15FD -> handler 0x5DB60A -> 0x5DB4B1 | frame += dir per call | R (unverified) | 2x | gate the add at 0x5DB4B4 with g_uiTick | 0x5DB4B4 `8b 46 04 01 46 08` | medium | cua UI-11, hc HC22 |
| 77 | Shell throttle | 0x75E1D3 (32.333 ms @0xC2C90C) | real time | T | correct | optional 0xC2C90C=30.0 | - | high | cua UI-12 |
| 78 | InGameUI messages, text effects, selection cycle, hint & formation fades | 0x8E9DB2, 0x8EA5D8, 0x99EAC1, 0x99EB97, 0x99ECEC, 0x92BA91, 0x6A5723, 0x69E5BB | m_frame | M | correct | none (F: refs + 0x99EE33 shr 3) | - | high | c3, c1, cua UI-14 |
| 79 | Living World countdowns / eye tower | 0x6B5929/0x6BE51A, 0x7FB29C, LW manager update 0x6121C5 (vtable slot 0xBFB570) | per LW update | R? | 2x if per render | confirm caller; gate LW update with g_uiTick | - | low | c1 X11, c2 |
| 80 | Eva timers | Eva::update 0x5DD86A (call 0x6485F1) | ms -= 33.33 per call | R | 2x | disp32 at 0x5DD98A and 0x5DD99B -> &g_msPerRender (16.667 in 60 mode) | `f3 0f 5c 05 20 f6 d9 00` x2 | high | c0 |
| 81 | Eva cycle window | 0x5DCB28 | ms*0.03 vs m_frame | E | correct | none | - | high | c0 |
| 82 | Subobject fade rates | 0x8B8F40 -> 0x672823 -> draw vt+0x88 | (1/s)/CLIENT_FPS per frame | ? | unknown (integrator untraced) | trace | 0x8B8F8C, 0x8B8FBC | low | c3 |
| 83 | Create-a-Hero preview delay | 0x9C0E74 | LTR+2 renders | R | 233 -> 117 ms (cosmetic) | optional | 0x9C0F00 | medium | c3 |
| 84 | Miles audio dt / fades / delays | getElapsedMs 0x450ABC (vt+0x1B4), fade 0x452C50 (fallback 0x452C9F), update 0x461DA6 | in-game dt = m_frame delta x33.33; shell 33.33/call; dt<1 && type 2 -> 33.33 | R | in-game average OK but type-2 fallback fires on every B-render (2x); shell 2x | replace 0x450ABC in 60 mode: 16.667 per running render (0 when halted), 16.667 in shell; fallback disp32 at 0x452CA3 -> 16.667 | 0x450ABC (`55 8b ec`), 0x452CA3 | high (SYN decompile) | cua AUD-1, c0 |
| 85 | Audio one-frame thresholds | 0x452E2A, 0x45A1BA, 0x45C6F5, 0x45CCCD, 0x45CD0A, 0x45CD31, 0x45CDCE, 0x45EFE5, 0x4615BF, 0x461AF5 | "< one frame" tests | E | <= 17 ms early starts | optional -> 16.667 | listed | medium | c0 |
| 86 | AotR Palantir counter | 0xED0800 ([0xED0000], cmp 0x1E at 0xED0816) | per Palantir update | R | 2x (harmless scan) | covered by #67; never patch 0x6D57AF..0x6D57B3 | - | medium | cua, c1 |

Totals (model H): 86 rows. Need a fix: 6 core mechanisms (rows 1-3/4 via C1, 7, 8, 11, 12) + 36 other systems
(rows 20-25, 29-31, 35-37, 48-55, 57-63, 67, 68, 70, 72, 73, 76, 79, 80, 84; row 31 groups 5 draw modules) + 2 optional (39, 85)
+ 1 decision (14). Already correct under H: 41 rows. Under F, about 25 of those would need patches too.

---------------------------------------------------------------------------------------------------

## 4. Timing-global reference classification

### 4.1 Counts (function level, 402 functions in 4 chunks; verdicts were made assuming model F)
| chunk | keep | client_scale | uncertain |
|---|---|---|---|
| 0 | 71 | 28 | 2 |
| 1 | 73 | 26 | 2 |
| 2 | 88 (84 + 4 "keep with conditions") | 13 (agent count; its JSON lists 11 functions / 15 refs) | 0 |
| 3 | 88 | 9 | 2 |
| total | 320 | 76 | 6 |
Almost all "keep" refs are LTR / LTR-derived (INIT_ltr) logic uses: AI states, update-module sleep values, script timers, ModuleData
defaults, Locomotor 0.2 s factors, logic INI parsers 0x73A403/0x73A429/0x73A4B6 (shared; never change 0.005/0.2), score/time displays.
The 21 FX shader FIMULs (0x4801D5..0x48128C) and 4 FX storage IMULs are "keep 30".
Half-shape static-init globals: 435 targets; 48 have recorded uses but 46 of them are only the initializer stores (SYN). Only 0xDE45D8
(1 read, 0x675D5B) and 0xDE87DC (2 reads, 0x7BC049, 0x7C0A4A) are read by game code. (Resolves "~48 used" vs hc's "3".)

### 4.2 All client_scale refs (as classified, F semantics) - 111 refs
- CLIENT_FPS 0xD9F60C (45): 0x44D698, 0x44E02F, 0x48AA62, 0x63252F, 0x63260F, 0x632A95, 0x635DCA, 0x63CF0F (c0); 0x6731CA,
  0x673289, 0x673494, 0x676151, 0x69E5E3, 0x6A181A, 0x6A2456, 0x6A5795, 0x6A5901, 0x6B336E, 0x6B3FE3, 0x6BFA5E, 0x6D3DAE, 0x6D3DD6,
  0x6D456C, 0x6D47B4, 0x6D946F, 0x6D94A8, 0x6D9B75, 0x79CB70 (c1); 0x7BDD7A, 0x7BDD97, 0x7FB2CE, 0x7FB334, 0x7FEA4B, 0x838D60,
  0x889C3D (c2); 0x8A2D40, 0x8B8F8C, 0x8B8FBC, 0x8E9E1B, 0x8EA686, 0x8EA697, 0x8EA896, 0x99EAFC, 0x99EC8E, 0x99ECA0 (c3).
- ms/client frame 0xD9F620 (16): 0x450B0C, 0x450B50, 0x452C9F, 0x452E2A, 0x45A1BA, 0x45C6F5, 0x45CCCD, 0x45CD0A, 0x45CD31, 0x45CDCE,
  0x45EFE5, 0x4615BF, 0x461AF5, 0x5DD986, 0x5DD997, 0x646781.
- client frames/ms 0xD9F624 (20): 0x4C4F74, 0x4C4F8E, 0x4C512D, 0x4C513E, 0x5DCB6F, 0x5E0776, 0x6A183F, 0x73297A, 0x76523B,
  0x85EED9, 0x85EEF5, 0x862C2E, 0x86AB63, 0x86ABA1, 0x86AE59, 0x86AE87, 0x8A6080, 0x8A60C0, 0x8B864C, 0x92BAB3.
- s/client frame 0xD9F62C (3): 0x442E96, 0x673222, 0x732B4F.
- 0.005 logic/ms 0xD9F610 (1): 0x73A496 (Anim2D AnimationDelay parser; needs 2*ceil(ms*0.005), not 0.01).
- 0xDC7A8C (17): 0x44B8DD, 0x44B911, 0x44BF62 (store), 0x485DD4, 0x485E7A, 0x485E9A, 0x485F62, 0x486279, 0x4886B1, 0x4887F4,
  0x48887A, 0x48892E, 0x4889B2, 0x48A9F2, 0x48AAE5, 0x48AB06, 0x48D3C5.
- LTR 0xD9F608 used as a per-client-frame constant (7): 0x4CF2BB, 0x4CF3CF, 0x664FF2, 0x665031, 0x69DF0E, 0x6A1A32, 0x87D65C
  (fix 0x87D65C via imm 5->10 at 0x87D661; never change LTR).
- INIT half 0xDE45D8 (1): 0x675D5B. INIT ltr 0xDE4848 (1): 0x6A5449 (per instruction; 0x69DF5E keeps 5).
- Divisor refs inside client_scale functions that must stay 5: 0x632535, 0x632615, 0x632A9B, 0x635DD0, 0x63CF15, 0x67349C.
- Keep-with-conditions (c2): FX storage 0x7B1102/0x7B1160/0x7B12FE/0x7B140B (stay 30); cameo flash 0x7BBFC8 (stay 30; consumer fix
  row 68); flash counts 0x7BC03E/0x7BC049/0x7C0A40/0x7C0A4A (0xDE87DC must stay 15).

### 4.3 Re-interpretation under model H (what actually changes)
Change (19 refs + 1 function): 0xDC7A8C at 0x44B911 (C3), 0x485DD4, 0x485E7A, 0x485E9A, 0x4886B1, 0x4887F4, 0x48887A, 0x48892E,
0x4889B2 (x2), 0x485F62, 0x486279 (x0.5), 0x48A9F2, 0x48AAE5, 0x48AB06 (16/17); 0xD9F620 at 0x5DD986, 0x5DD997 (Eva), 0x452C9F
(+ replace 0x450ABC, refs 0x450B0C/0x450B50); LTR at 0x4CF2BB, 0x4CF3CF, 0x69DF0E (-> 10).
Covered by gating instead of ref changes: Palantir/hero bar 0x6D3DAE, 0x6D3DD6, 0x6D456C, 0x6D47B4, 0x7BDD7A, 0x7BDD97 (row 67);
Living World 0x6B336E, 0x6B3FE3, 0x6BFA5E, 0x7FB2CE, 0x7FB334, 0x7FEA4B, 0x838D60 (rows 57/79, cadence to confirm).
Optional: audio thresholds (row 85). Unknown: 0x8B8F8C/0x8B8FBC (row 82).
All other client_scale refs KEEP their stock value under H (they convert to m_frame units, which stay 30 Hz): 0x44B8DD (fast-forward,
60 mode off), 0x44BF62, 0x48D3C5 (mode 3), all 0xD9F624 and 0xD9F62C refs, all stepper/init CLIENT_FPS refs (must give 6), radar,
fades, floating text, InGameUI messages, text effects, opacity pulse, 0x673494 physics ratio, 0x664FF2/0x665031/0x6A1A32/0x87D65C,
0x675D5B, 0x6A5449, 0x73A496, 0x646781.

### 4.4 Uncertain refs (6)
| ref | function | reason | status |
|---|---|---|---|
| 0x5DFF96 (LTR) | AttachedModel FX nugget ctor 0x5DFF7A | ExpireTimer default LTR*8; consumer is draw module vt+0x8C via 0x67654E, untraced | open |
| 0x5E1CF4 (0.005) | FXList ParticleSystem nugget 0x5E1270 | InitialDelay ceil(ms*0.005) into sys+0x120 | resolved by ptx P6/P9: decremented per particle update -> keep (stock 6x quirk) |
| 0x73A496 area: 0x73A74B (parser) | INI Envelope block | evaluator not found (laser-type draw module data +0x4C) | open |
| 0x73B071 (parser) | LargeGroupAudio HandOffModeDuration | audio consumer not found | open |
| 0x8A5A5B (LTR) | SlavedUpdate weld sparks 0x8A58FA | lifetime frames*LTR in particle ticks | resolved: keep (particles 30 Hz under H) |
| 0x9C0F00 (LTR) | CreateAHero screen 0x9C0E74 | LTR+2 render-count delay | harmless; optional |

---------------------------------------------------------------------------------------------------

## 5. Systems confirmed frame-rate independent (wall-clock or logic based)
- Logic: every LTR/INIT_ltr/0.2/0.005/200 ms reference classified "keep" (AI, scripts, modules, locomotors, Living World logic timers
  x5.0 0x80F475/0x80FE19, control-group double-tap 0x83C29E, UI cooldown 0x83AB1F, drawable icons with logic expiry).
- Real time: APT movie playhead 0x624EE1/0xAE18C0; tooltips 0x5EE7A8; animated W3D cursor 0x498CBC; edge-scroll ramp (timeGetTime
  vs ScreenEdgeScrollRampTime); letterbox fade 0x443939; military subtitles 0x69CE5F; shell 32.333 ms throttle 0x75E1D3; VP6 video
  0x4909A9..0x491DEC (VideoPlayer::update is empty 0x49017B); shroud fog interpolation 0x472CF2/0x473AAB; FPS average/dynamic LOD
  0x4430BB/0x4438DA; 3D-in-UI scene clock 0x4A89BD; water list dt 0x500137 (unless GD+0xD45); MP network pacing 0x65DD0F/0x65DE5E;
  double-click/drag-ms (OS timestamps).
- W3D-clock based (correct once C3 is in): model animation and crossfades, UV mappers, SegLine/laser scroll, terrain tracks, water
  render UV, snow, shader time, W3D particle buffers, FX shader parameters, FX particle storage (sync*CLIENT_FPS, CLIENT_FPS stays 30).
- Geometry/distance based: streak draw 0x4CF884, projectile stream 0x4D0D05, laser geometry 0x4C8A6C; health bars/pips; mouse-drag
  rotate; wheel zoom steps; empty draw modules (Floor/Prop/Tree/Buff doDraw).
- Interpolation consumers (fraction/isTick based): drawable transforms, instance matrices, turret angles 0x4B6F12, build-up 0x4B51B5/
  0x4B686D, overlay alpha 0x48E0EE, counter extrapolation 0x8A0365 (with C1b/C4).
- Under model H additionally: everything in rows 26-28, 32-34, 38, 41-47, 56, 64, 69, 71, 74, 75, 78, 81 (m_frame based).

---------------------------------------------------------------------------------------------------

## 6. Open questions and risks (ranked by impact on "no 2x speed / identical logic")

R1 CRITICAL - logic call pattern. GameLogic::update must get sub 1..6 exactly once per tick and failed-tick attempts must stay at
   30/s (frozen time runs sub-1 work without frame++, lines 29-58). Any design that lets the stepper pass s>6 (drw D01, c0 "cmp eax,6
   -> 12") or retries every render breaks bit-identity. Covered by C1d/C1e; verify with M2/M3.
R2 CRITICAL - hidden logic mutation from client code. Logic must not see more renders between sub-steps. Unverified: whether any
   per-render or per-draw code calls the logic RNG (seed 0xDA1CA4) or writes object state. The commands path (message stream ->
   command list consumed at sub 1) is unaffected. Verify with M2 (RNG counter + CRC) before release.
R3 HIGH - game speed under load. Game time is still counted in rendered frames. Budget halves to 16.5 ms; A-frames carry the logic
   sub-step + render. With stock "last=now" pacing any overrun permanently slows the game (stock had a 33 ms budget per sub-step).
   Mitigate: deadline pacing (C2 addition) with bounded catch-up; automatic fallback to 30 mode at a tick boundary when the moving
   average of pair time > 33 ms. Decide how large battles should behave.
R4 HIGH - per-render systems not yet inventoried (each would run 2x): GameClient::update subsystems 0xDE3B54, 0xDE3C24, 0xDE7734,
   0xDE4EFC (cloud-break / ray effects / language filter / snow - mapping uncertain), 0xDE36E0, 0xDE8B34, 0xDE495C, 0xDF06F8,
   FUN_00782C56, FUN_00532D6F, FUN_0062B385, 0xDE4518, 0xDE4830, GameClient vt+0x90, 0xDE3D6C (called from InGameUI::update), TheRadar
   update, other message-stream translators (selection, hint, placement, window), screen filters (BW/fade/motion blur; runtime counters
   not located), WindowTransitions caller cadence, shrub update 0x4E837E caller 0x46B0A0, Floor death fade (FloorFadeRateOnObjectDeath),
   Living World update cadence (0x6121C5 / 0x6BE51A).
R5 MEDIUM - vsync. PresentationInterval DEFAULT: on a 60 Hz display Present blocks at 60.00 FPS, i.e. 5.00 Hz logic vs stock 5.05 Hz
   (-1%); stock at 30.3 FPS is CPU-paced and unaffected. Options: accept, IMMEDIATE present (tearing), or let the deadline limiter
   absorb it. Displays above 60.6 Hz match stock. Needs measurement (M8).
R6 MEDIUM - mode transitions: pause/unpause, frozen-time cinematics (paused camera loop C6), script time multiplier changes mid-tick,
   fast-forward, save/load (s is not saved, re-init on load), map end -> shell, credits (setFPS 100 -> 200 with halving), replays
   (does replay playback create TheNetwork? unknown).
R7 MEDIUM (visual, model H) - 30 Hz-stepped visuals at 60 FPS: particles (row 39 optional fix), tree sway when gated, decal rotation,
   fades/tints, mode 3/4 camera moves (row 56), recoil/rope/tread when gated. Also per-render code that tests m_frame values (modulo or
   equality) fires twice per m_frame: ControlBar flash (fixed, row 68), radar %6 refresh and InGameUI %CLIENT_FPS deselect check
   (harmless). New such sites may exist in unaudited code.
R8 MEDIUM - client physics replay (C5) correctness: cache must be invalidated on drawable destruction/address reuse (store the m_frame
   stamp and compare with loco+0xAC).
R9 LOW-MEDIUM - unresolved units: AttachedModel ExpireTimer (5DFF96), INI Envelope evaluator, HandOffModeDuration, subobject fade rate
   integrator (row 82), LightPulse creation hook details, GlobalData+0xD45 meaning (forces 33.3 ms water dt and particle m_frame gate;
   set in Create-a-Hero 0x91A7A4), CameraShaker calls per frame (row 53), GameLogic+0x125/+0x114, modes 7/9.
R10 LOW - AotR code: only the Palantir hook body (0xED0800 counter) was examined. Other AotR hooks and their cadence: 0x629D11 in
   FUN_00629D02 (called from logic 0x62A11A/0x62CE75/0x62D232 -> logic, fine), 0x638D49 in FUN_00638D22 (GameEngine::init, once),
   0x69A760 thunk to 0xED0A00 (caller 0x69AB8B, InGameUI - unaudited), 0x6D40FA/0x6D410A (Palantir), 0x6D7267 in 0x6D67EA (Palantir,
   caller 0x6D73C2), 0x5D8A64/0x5D8AF1 in 0x5D893C (callers 0x8C1C51.. logic modules), 0x8A144D, 0x9A3AE0. Core found no absolute refs
   from AotR sections to timing globals or engine objects. No proposed patch overlaps AotR bytes.
R11 LOW - saves: under H stock-compatible (m_frame, birth-fade counters, decal keys stay in 30 Hz units; stepper fields are not saved).
   Under F they would not be.
R12 LOW - multiplayer excluded by detection; MP-only code (0x63239D, 0x63A17B, 0xD9F498) untouched.

Coverage gaps (nobody examined): items in R4; Living World (War of the Ring) mode as a whole; Create-a-Hero; loading screen; shell
menus (recommend 60 mode off in shell first); credits; replay playback; APT callbacks into C++ beyond Palantir/ControlBar; legacy
ParticleSystemManager (if any besides FX); the 0xDE8D68 subsystem in the drawable block; GameClient vt+0x90.

---------------------------------------------------------------------------------------------------

## 7. Runtime verification (DLL instrumentation, no game-file changes)
M1 Wall-clock rates over 5 min, 30 vs 60 mode, idle and in battle: logic frames/s (GameLogic+0x40 deltas vs QPC; stock 5.05),
   m_frame/s (GC+0x10; must stay 30.3 under H), syncAcc 0xDC7580 ms per real second (990) and "sync changed" on every running render,
   GameEngine+0x58 rate stat.
M2 Determinism: same skirmish/replay inputs in 30 and 60 mode; per logic frame log (a) calls to FUN_006D315D with ECX==0xDA1CA4,
   (b) a hash of object positions/health (or the game CRC if found), (c) path requests serviced by FUN_006F2364. Must be identical.
   Also load a 60-mode save in 30 mode and vice versa, run N frames, compare hashes.
M3 Stepper trace: sequence of GameLogic::update(sub) calls per tick = exactly 1,2,3,4,5,6; failed-tick attempts per second during
   pause and frozen-time cinematics = stock (30/s).
M4 Frame-time histogram split by A/B frames; count of frames over budget; limiter slip; pair time vs 33 ms.
M5 Call counters per second for every per-render subsystem (vt28 calls in GameClient::update, InGameUI::update children, LW update
   0x6121C5, transitions 0x5DB60A, shrubs 0x4E837E, CameraShaker Timestep 0x4655F8, particle manager, tree buffer) to confirm the
   cadence assumptions (R4).
M6 Side-by-side capture stock-30 vs mod-60 (same scene): scripted camera move durations, follow/settle ease curves, scroll px/s,
   keyboard rotate deg/s, zoom keys, shake decay, particle lifetimes, unit animation, tree sway/topple, fades/tints/flashes, recoil,
   light flicker, water UV, Eva repeat timing, audio fade lengths (type-2 sounds while running and paused), Palantir counters/hero-bar
   flash, ControlBar flash count, message fade, window transitions, keyboard repeat, mouse hold.
M7 Client physics: with ShowClientPhysics on, confirm pitch/roll/bob is present on B-renders (C5) and spring speed equals stock.
M8 Present blocking time and achieved FPS on 60 Hz and >=120 Hz displays, fullscreen and windowed.
M9 Mode switching: pause/unpause, script time multiplier, fast-forward, Esc-menu, save/load mid-tick, map restart, return to shell;
   g_m60 changes only at s==1 and C1a/C1c/C6 bytes match the mode.
