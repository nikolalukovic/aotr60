# AotR 60 FPS: core loop and engine clock

Plan mode was switched on partway through this task, so this file is the only one I may write.

Everything below comes from reading game.dat code: Ghidra dumps, raw disassembly via gdis/capstone, and
dword scans of the image. "VERIFIED" means I read the code. "INFERENCE" means a likely explanation that I did not prove.

---------------------------------------------------------------------------------------------------
## 0. Summary and corrections to the planned core model

* **Logic work is spread over the six sub-frames of each logic tick. It does not all run at the tick (VERIFIED).**
  `GameLogic::update(int sub)` (FUN_0062e4e8, W3DGameLogic vtable 0xBD8590 +0x34) does different work for each sub value:
  - sub==1: `m_frame++` (+0x40), script engine, Lua, recorder, command processing, etc.
  - sub==2: DAT_00de4354/4360 updates, then a per-object pass (FUN_006260e1).
  - sub 3,4: sleepy update-module bucket 0 (sub 3 runs the first half, sub 4 the second half).
  - sub 5: buckets 1..2, then TheAI plus 13 more logic subsystems.
  - sub 6: bucket 3.
  - every call: FUN_005ff9d3, FUN_006f2364 (QPC), setFPMode (FUN_00440809), FUN_00629da6.
  The engine's own catch-up loop (0x632a95, used when CLIENT_FPS/LTR < 6) keeps passing 1..6 even when
  logic runs faster.
  ⇒ With 12 client sub-frames, **logic must still receive sub = 1..6 exactly once each per tick, in order.**
  Call `step((s12+1)/2)` only on odd s12 (1,3,5,7,9,11). On even s12 ("B-frames") do not call logic at all.
  Passing 1..12, or calling any sub value twice, changes logic and breaks save and replay compatibility.
* Do **not** change CLIENT_FPS (0xD9F60C), LTR or `GameEngine+0x38`. +0x38 means *client frames per logic tick*,
  in the units of TheGameClient's frame counter. Logic modules (0x85ebee, 0x860b39, 0x8cd55f), drawables (0x6714c0)
  and InGameUI floating text (0x69db40, 0x69dc5b) all read it with that meaning.
* Recommended model, the **"half-frame hybrid"**:
  - Rendering and GameEngine::update run at 60 Hz.
  - The stepper counter `GameEngine+0x34` runs 1..12 and the fraction is s12/12.
  - TheGameClient's frame counter (+0x10, which is saved in save games) stays at 30 Hz. To do that, set
    `TheGameClient+0xC8 = 0` on B-frames; the engine then increments the counter only after A-frames.
  - W3D sync time advances 16 or 17 ms per rendered frame.
  - The two Drawable interpolation caches must be keyed on a 60 Hz render id. Today they are keyed on the
    client frame, so without this fix units would still move at 30 Hz.
* Limiter: stock uses `_ftol2(1000/30) = 33` ms, which is 30.30 FPS and 5.05 logic Hz. At 60 FPS use
  alternating 16 and 17 ms waits. A plain 16 ms wait would run the game 3.1% fast.
* Correction to the task brief: FUN_00639fa7 is not GameEngine::update. It is the catch funclet of
  **GameEngine::execute (0x639CF8)**. The limiter sits in execute's loop tail, which Ghidra split off as FUN_00639fef.
  GameEngine::update is 0x6325A0, called from Win32GameEngine::update 0x44181F.

---------------------------------------------------------------------------------------------------
## a) Structure

### Objects (VERIFIED from the initSubsystem calls in GameEngine::init, 0x63AD4F)
| global | object | vtable |
|---|---|---|
| 0xDE4324 | TheGameEngine (Win32GameEngine) | 0xBD84E0 (base GameEngine 0xBFE260) |
| 0xDE412C | TheGameLogic (W3DGameLogic, 0x2A8 bytes) | 0xBD8590, Snapshot 0xBD8580 |
| 0xDE4388 | TheGameClient (W3DGameClient, 0x140 bytes) | 0xBDA6E0, Snapshot 0xBDA6CC |
| 0xDE4418 | TheDisplay (W3DDisplay) | 0xBD9C28 |
| 0xDE3744 | TheFXParticleSystemManager (W3D) | 0xBDAC10 (base 0xBF7A00) |
| 0xDE3F0C TheAptPlayer · 0xDE4AB8 TheRadar · 0xDE6398 TheMessageStream · 0xDE42FC TheAudio · 0xDE3BAC TheScriptEngine · 0xDE4950 TheLivingWorldLogic · 0xDE4364 TheWritableGlobalData · 0xDE447C TheTacticalView · 0xDE4468 TheNetwork (NULL unless LAN or online) · 0xDE4830 TheInGameUI · 0xDE495C TheWindowManager · 0xDE7890 TheShell · 0xDE4334 TheKeyboard · 0xDE36E0 TheMouse · 0xDE4AC8 TheTerrainVisual · 0xDE4518 TheDisplayStringManager · 0xDC62C0 the EA Debug object · 0xDEF548 the 'asst' asset/streaming manager (update is 0xA37E50) |

### GameEngine vtable (base 0xBFE260; Win32 0xBD84E0) and fields
- +0x24 reset (0x635D11): resets +0x38=6, +0x44=6.0, +0x48, +0x40, +0x4C..+0x5C. It does **not** reset +0x34.
- +0x28 update (0x6325A0; Win32 0x44181F). +0x38 init (0x63AD4F). +0x3C execute (0x639CF8).
- +0x48 setFramesPerSecondLimit(int) (0x66F0FD writes [this+0xC]). +0x4C get (0x5F2D61). +0x50 setQuitting. +0x54 getQuitting.
- +0x5C serviceWindowsOS (Win32 0x441A7D). +0x70 createGameLogic (0x4418DD). +0x74 createGameClient (0x4416BE). +0x8C createParticleSystemManager (0x441757).
- **+0x98 step(int sub) (0x6329B0). +0x9C clientUpdate() (0x632409).** Neither function has a Ghidra function boundary, so read them with gdis.
- Fields:
  - +0xC maxFPS.
  - +0x10 quitting.
  - **+0x34 sub-frame, +0x38 frames-per-tick (ctor 1, init and reset 6), +0x3C fraction, +0x40 ticked flag**.
  - +0x44 max stretched frames-per-tick (float, MP).
  - +0x48.
  - +0x50 tick count, +0x54 timeGetTime stamp, +0x58 measured client frames per second (float), +0x5C sum of +0x38.
  - +0x60.
- Constructor at 0x63A49C: sets everything to 0 except +0x38=1 and +0x44=1.0, then calls timeBeginPeriod(1).

### Main loop (VERIFIED)
```
GameEngine::execute 0x639CF8 (vt+0x3C):
  [first time: 0xDE431C = timeGetTime]; [optional startup map load]
  loop @0x639EC6: if (quitting [esi+0x10]) exit
     perf("") ; this->update() [vt+0x28 = Win32GameEngine::update 0x44181F]
     LIMITER 0x639FEF..0x63A21F (see c)
     if (DAT_00DE42F8) FUN_00631D04()       // per-frame service object, created when 0xDE87B9 is set
Win32GameEngine::update 0x44181F:
  call 0x6325A0 (GameEngine::update)  @0x441822 (e8 79 0d 1f 00)
  while minimized: Sleep(5); serviceWindowsOS; TheLAN(0xDE4394) update; break if quitting or mode==5/1 (MP keeps running)
  jmp serviceWindowsOS (vt+0x5C)
GameEngine::update 0x6325A0:
  1 TheAssetMgr(0xDEF548)->vt28            (threaded asset streaming)
  2 ScriptEngine debug-DLL hooks FUN_00604189 / FUN_00603452 (DAT_00DE3B98 HMODULE; NULL in retail, so no-ops, return false)
  3 this->vt9C()  = clientUpdate 0x632409:
       GameLogic FUN_0062B385 (start-new-game / load transition; only acts if GameLogic+0x9C)
       if (TheGameClient+0xC8) TheGameClient->setFrame(getFrame()+1)    // CLIENT FRAME COUNTER (+0x10)
       TheAptPlayer->update (vt28)
       if (FUN_0063239D()) {0xDE4304++; 0xDE4308=clientFrame; return}   // MP-only stall/skip
       TheRadar(+4)->update ; TheGameClient->update (0x64849E, includes RENDER)
       TheMessageStream propagate (FUN_007128C3) ; focus handling (FUN_0080000F, InGameUI 0x69B5C7, Mouse 0x5EDCF7)
       TheAudio->update ; TheNetwork->vt3C(0)
  4 if ext-pause: TheGameClient+0xC8=0 ; Debug->vt94 ; return
  5 STEPPER: TheGameClient+0xC8=1; [if sub==6 && +0x40: +0x38=FPS/LTR, +0x40=0]; sub++;
       Debug->vt90(logicFrame*10-1+sub) [FUN_006251A3]; fraction=FUN_0063256F
       if sub<=6: step(sub)
       else: [every 25 ticks measure rate]; old=sub; sub=1; fraction; step(1);
             if (!TheGameClient+0xC8) {sub=old; fraction}   // logic did not advance (pause or net wait)
             else +0x40=1
  6 Debug->vt94 (log flush)
step(sub) 0x6329B0:
  if sub==1: ok = !GameLogic->isGamePaused (+0x124); MP: TheNetwork checks/frame-data;
             if mode==5 FUN_0082753A; if !ok -> TheGameClient+0xC8=0, return
  TheGameLogic->vt34(sub)  = GameLogic::update(sub) 0x62E4E8
  if sub==1: TheLivingWorldLogic(0xDE4950)->vt28
  catch-up loop 0x632A95..0x632AEE only if CLIENT_FPS/LTR < 6
GameClient::update 0x64849E (W3DGameClient vt28 -> jmp 0x64849E):
  FUN_0083B471 (camera scroll), FUN_008392A7, FUN_0050EB3C; DAT_00DE4958 vt6C
  Snow, Cloud, CloudBreak, Fire managers; Anim2D; Keyboard upd+msgs; ScoredKillEva; Eva; Mouse upd+msgs; FUN_00782C56
  WindowManager; VideoPlayer; FUN_00532D6F; debug hitch (GlobalData+0xC78)
  freeze = (TacticalView frozen && !camDone) || FUN_00441E23 || FUN_00603418 || isGamePaused
  skip = freeze || (static lastFrame 0xD9F6F8 == clientFrame)                 // runs once per CLIENT FRAME
  if !skip && !GameLogic+0x125: lastFrame=frame; isTick=FUN_0063252F (sub==1)
       if isTick: DAT_00DE4BD0->vt18
       for each drawable: if isTick && d+0xFC {FUN_0068D8F7; FUN_00678F54 (history shift)}; FUN_00675996(d)
       TheAnimationSoundModuleManager->update; FUN_0064594B
  FUN_0062B385 ; TerrainVisual update ; TheDisplay->update (vt28)   [or FUN_0065C1EA while loading]
  particle mgr local player ; **TheDisplay->vt30 = W3DDisplay::draw 0x44B788** ; DisplayStringManager ; Shell ; InGameUI ; vt90
W3DDisplay::draw 0x44B788:
  iconic -> return; FPS stats/LOD (0x4430BB, 0x4438DA, debug display cb)
  frozen = (TheGameClient+0xC8 == 0)
  d = clientFrame - lastDrawFrame(0xD98C8C)  (0 if frozen)
  fast-forward branch (not frozen && (isTimeFast || GlobalData+0xBBD) && !loading && frame%30): camera update 0x48A953, sync+=33, NO render
  else: sync(0xDC7580) += 33*d ; WW3D::Sync(sync) (FUN_00516E20)
        frozen-camera inner loop: { busy-wait 29 ms ; drawFrame FUN_00449CF8 } while (frozen && !camDone && isGamePaused)
  drawFrame 0x449CF8: updateViews (vtA0, camera) + FUN_00479FC4 ; **TheFXParticleSystemManager->update (0x5F5123)** ;
        shadow/water RT ; RenderViews ; RenderUI (InGameUI draw, TheAptPlayer->vt30 draw) ; mouse ; Present
```
### Cadence (stock)
| system | cadence |
|---|---|
| GameEngine::update, clientUpdate, GameClient::update, W3DDisplay::draw, APT update, radar, message stream/input, audio, camera (updateViews), particle mgr update (gated by +0xC8) | **per rendered frame** |
| drawable client update FUN_00675996, anim-sound mgr | per client frame (lastFrame hack 0xD9F6F8) |
| drawable interpolation history shift, isTickFrame users (0x48E0EE, 0x4B51B5, 0x4B686D, 0x8A0365, 0x461DBD, 0x4ED508, 0x83B471) | once per logic tick (first frame after the tick) |
| GameLogic::update(sub) | every client frame, sub 1..6 |
| logic frame++ / commands / scripts | once per tick (sub==1) |
| W3D sync time | +33 per client frame |

---------------------------------------------------------------------------------------------------
## b) Sub-frame, fraction, frames-per-tick and their readers
- step(sub) is described in (a). GameLogic::update stores sub in GameLogic+0x17C. That field has no reader outside GameLogic (searched).
- **FUN_006251A3(x) = Debug(0xDC62C0)->vt90(x) (0x43AFC0).** It runs debug commands queued for "frame" ≤ x (queue
  count at +0x9E0C; empty in retail). The x = logicFrame*10-1+sub ids are debug frame numbers only.
  With 12 sub-frames, pass ceil(s12/2) so the ids stay inside the 10-per-tick range. GameLogic::update calls it
  again with frame*10 at the end of sub 1.
- **Accessors:**
  - FUN_0063252F isTickFrame: returns `+0x34 == 1` when FPS/LTR ≥ 6, otherwise `+0x34 == 6/n`.
  - FUN_0063256F fraction = clamp(+0x34/+0x38, 0, 1). Its only caller is the stepper.
  - 0x63255F sets +0x38 = FPS/LTR. It has no direct callers.
  - FUN_0063239D is the MP stall helper; it is reached inline from clientUpdate.
- isTickFrame callers: 0x461DBD, 0x48E0EE, 0x4B51B5, 0x4B686D, 0x4ED508, 0x64849E (x2), 0x675996, 0x83B471, 0x8A0365.
  Pattern: when tick frame, shift prev←cur; then lerp(prev, cur, fraction). This works with any number of
  sub-frames, provided sub==1 happens on exactly one rendered frame per tick.
- **+0x3C (fraction) readers:**
  - 0x48E3C4 (W3DView script value lerp)
  - 0x4B5241, 0x4B6C21, 0x4B6F86 (W3D draw modules)
  - 0x67175A (FUN_0067171D, Drawable transform lerp, cached per client frame +0x204)
  - 0x67668D (FUN_006765B9, Drawable::getInterpolatedTransform, catmull-rom, cached per client frame +0x244)
  - 0x67673E (FUN_00676711), 0x6767D1 (FUN_0067679B)
  - 0x8A0382
  All read it as a continuous lerp parameter. None compare it to multiples of 1/6.
- **+0x38 readers:**
  - 0x4CC4BE (FUN_004CBFFB, W3D tank/tread-type draw module: `step = X/+0x38` per draw, a per-render integrator)
  - 0x4E5F40, 0x4E648F (cache (float)+0x38 at construction or reset)
  - 0x62C16E (debug benchmark tool)
  - 0x6716E2 (drawable interpolation expiry = clientFrame + +0x38)
  - 0x69DBC9, 0x69DCD0 (floating-text fade and rise per client frame)
  - 0x85EEA1, 0x85EF14, 0x860D43 (logic modules: drawable timer = logicFrames × +0x38)
  - 0x8CD5FA
  All except 0x4CC4BE use client-frame units, so +0x38 must stay 6 under the hybrid.
- +0x58 reader: 0x819ECC (stats or log print only).
- +0x34 readers outside the stepper: FUN_0063252F; FUN_0063239D and execute 0x63A06E (both MP-only).

---------------------------------------------------------------------------------------------------
## c) Limiter
- INI parse table (GameData): 0xBFF600 {"UseFPSLimit", parseBool 0x42E558, 0, **+0x26**};
  0xBFF630 {"FramesPerSecondLimit", parseInt 0x42EC5E, 0, **+0x28**}.
  GlobalData ctor (0x6429AD) defaults are 0 and 0. AotR gamedata.ini sets `UseFPSLimit = Yes`, `FramesPerSecondLimit = 30`.
- setFPS (GameEngine+0xC) writers:
  - GameEngine::init 0x63C841 sets GlobalData+0x28.
  - ScriptEngine reset 0x6096B1 sets GlobalData+0x28.
  - MSG_NEW_GAME 0x779DCF sets the arg if it is in [1,1000], otherwise GlobalData+0x28, and forces UseFPSLimit=1.
  - script SET_FPS_LIMIT 0x7CCF20.
  - Credits screen 0x91B6DC sets 100 and restores to GlobalData+0x28 at 0x91B7AA.
- Speed multiplier 0xD9F498: it is written **only** in the execute loop (0x63A080, 0x63A09D, 0x63A14A). It is 1.0 unless
  TheNetwork is present. MP adaptive formulas: (10-min(n,10))*0.1+0.5+old)/2 or (min(n*0.1+0.7,1)+old)/2.
  Readers: 0x448CDA (stats), 0x6323B2 (MP stall).
- Flag 0xDE4320 = limiter on. It is set to:
  - GlobalData.UseFPSLimit when TacticalView time multiplier ≤ 1 and !isTimeFast (0x603491)
  - 0 if GlobalData+0xBBD (fast-forward)
  - 1 for MP or when speed ≠ 1.0
  - 0 if `0xDE4308 + 6 > clientFrame`. 0xDE4308 holds the client frame of the last MP stall skip. It is reset to 0 at
    0x6315FB and 0x77945F, so the first 6 client frames of a game run unlimited. The unit is client frames, which
    stays valid under the hybrid.
- **Exact code:**
```
0x63A196 ff 15 20 09 bd 00   call timeGetTime
0x63A19C db 46 0c            fild [esi+0xC]           ; maxFPS
0x63A19F 8b f8               mov edi,eax
0x63A1A1 d8 0d 98 f4 d9 00   fmul [0xD9F498]
0x63A1A7 d8 3d 88 43 bd 00   fdivr [0xBD4388]=1000.0f
0x63A1AD e8 f2 2d 40 00      call 0xA3CFA4 (_ftol2, truncates) -> 33
0x63A1B2 8b f0               mov esi,eax              ; frameMs
... 0xDE4314=elapsed, 0xDE4310=sleep, 0xDE430C+=sleep
0x63A1DC 53 / ff 15 b4 03 bd 00 / ff 15 20 09 bd 00 ... jb 0x63A1DC   ; Sleep(0)+timeGetTime busy-wait while (now-last) < frameMs
0x63A1F8 89 3d 18 43 de 00   mov [0xDE4318],edi       ; last = now
```
- Second limiter, in W3DDisplay::draw:
  - 0x44B98C `83 c6 e2` sets prev = now-30, so the first pass is immediate.
  - 0x44B9C1 `83 f9 1d` (cmp 29) / `7d 14` busy-waits while UseFPSLimit && now-prev < 29.
  - It only matters inside the paused + camera-moving loop (≈34 FPS). For 60 FPS the threshold should alternate 14 and 15.
- Other waits:
  - minimized Sleep(5) loop
  - load and transition fades in FUN_0062B385 (Sleep 1 or 5 while the display fade is < 100)
  - debug hitch at GlobalData+0xC78
  - credits forced to 100 FPS
  - IPC startup wait 0x63F6BF
- **Vsync:** D3D9 PresentationInterval 0xDD302C is set to EBP=0 (D3DPRESENT_INTERVAL_DEFAULT, so vsync is on) at 0x524B5C.
  Set_Swap_Interval (0x522460) has no callers.
  - Stock pacing is CPU-limited (30.3 FPS).
  - At 60.6 FPS on a 60 Hz display, Present blocks and pacing becomes 60.00 FPS (logic 5.00 Hz, 1% slower than stock).
  - On displays faster than 60.6 Hz it matches stock exactly.

---------------------------------------------------------------------------------------------------
## d) W3D clock and real time
- 0xDC7A8C TheW3DFrameLengthInMsec (int):
  - static init 0xBC146C = 1000/CLIENT_FPS = 33
  - W3DGameClient::setFrameRate 0x44BF5C (vt+0xCC): `cvttss2si` of the arg, called from GameClient::init 0x646792
    with *(float*)0xD9F620 = 33.33, giving 33
  - FUN_0044BF2A in the brief is a different function; the writer is 0x44BF5C, right after it
- Readers:
  - 0x44B8DD, 0x44B911 (sync)
  - W3DView camera moves FUN_00485DB0, 0x485E5F, 0x485F0F, 0x486253, 0x48868B, 0x48885E, 0x48D26B
    (numFrames = ms / 33; one step per updateCameraMovements per draw)
  - 0x48A953 (pushes it as an argument)
- Display sync 0xDC7580:
  - writers: 0x44B8E4, 0x44B919, reset at 0x442DD5 (with 0xD98C8C = -1)
  - readers: 0x4B32FD, 0x4BF15E, 0x4BF560, 0x4BFAF4, 0x4C04A3
- WW3D::Sync = FUN_00516E20 (Prev 0xDD1E10 ← Sync 0xDD1E0C ← t). Callers:
  - W3DDisplay::draw (0x44B8E9, 0x44B925)
  - FUN_004A89BD: a separate preview renderer. It swaps in its own timeGetTime clock (clamped to 100 ms) and then
    restores the old one, so it is real-time based and FPS-independent.
  - 83 functions read SyncTime: W3DView 0x48xxxx, WW3D2 lib 0x58xxxx–0x5Axxxx, 0xB57xxx, 0x7B10B4, …
  - 6 functions read PrevSync (frame time): 0x48BCF2, 0x4943E1, 0x4A89BD, 0x4BF560, 0x5A1D60.
  - All of these get correct speed if Sync advances by real elapsed ms; 16/17 per 60 Hz frame works.
- Real-time API users (timeGetTime, GetTickCount, QPC) in game code:
  - 0x440210, 0x442927, 0x443C46, 0x448B51 (stats), 0x44B788, 0x472CF2, 0x473AAB, 0x47F544, 0x4909A9, 0x490B52,
    0x490BBF, 0x491285, 0x491B47, 0x498CBC, 0x4997D8, 0x49F7BC, 0x4A8989, 0x4A89BD, 0x4E013B, 0x4FDB07, 0x500137,
    0x5B86A0, 0x5EDC1F, 0x5EE384, 0x5EE7A8, 0x5F5123 (QPC stats only), 0x5FA2CE, 0x60CC67, 0x612274, 0x624B0C,
    0x624EE1, 0x625C6F/92/DE, 0x62C159, 0x6325A0, 0x635CF5, 0x639CF8, 0x639FEF, 0x63CEF1, 0x63F6BF, 0x644CC2,
    0x645BDF, 0x64849E, 0x648F31, 0x64A02C (LAN), 0x65C1EA, 0x65C67E, 0x65D86D…0x65DE5E (network),
    0x69CE5F, 0x69FF85, 0x6F2364 (QPC inside GameLogic::update), 0x727081, 0x7298A3, 0x758084 (load screen),
    0x75E1D3, 0x784B58, 0x79120E, 0x7FFFAC, 0x801AAF, 0x807848, 0x808A20, 0x808E1F, 0x826B4B, 0x82753A,
    0x83A9BD, 0x83B471 (camera scroll), 0x83EBD4, 0x8431B8, 0x84392C, 0x84BF0D…0x84F288 (net/LAN),
    0x8D3338…0x8D9749, 0x8EAEE0, 0x8EAF73; plus 0x9xxxxx GameSpy and 0xA9xxxx Miles audio.
  - Systems driven by real time are FPS-independent by construction.
  - The exceptions use real time as a throttle that sets the frame rate: the execute limiter, the frozen-camera
    loop, the stepper rate stats and the debug hitch.
  - I did not classify each one individually. The other agents own those systems.

---------------------------------------------------------------------------------------------------
## e) Mode detection, pause, Xfer
- Network game: `*(void**)0xDE4468 != NULL` (TheNetwork). It is created only by the LAN or online start paths
  0x648D11, 0x6492D0, 0x9038E5, 0x9044F9 and deleted on reset 0x635DA4.
  Also `GameLogic::isInMultiplayerGame` = FUN_00441B7C: mode `[TheGameLogic+0x110]` ∈ {1 LAN, 5 Internet}.
  FUN_00441B60: mode ∉ {4 shell, 7, 9}.
  Use: mode60 allowed iff TheNetwork == NULL && !FUN_00441B7C. Replays are fine because they are deterministic.
- Pause:
  - `byte [TheGameLogic+0x124]` (FUN_0090F92C isGamePaused; setter GameLogic::setGamePaused 0x625AF1, which also pauses audio).
  - Script/time freeze is TacticalView vtD8 && !vt78, or FUN_0060342F.
  - +0x125 is set while a save game is loading (0x6DED03).
  - "Logic advanced" flag is `TheGameClient+0xC8`.
    - Writers: stepper 0x6325DE/0x6325FB; GameLogic::update 0x62E5D1, 0x62E5EF, 0x62EE3E; step 0x632AFC.
    - Readers: clientUpdate 0x632429, W3DDisplay::draw 0x44B807, ParticleSystemManager::update 0x5F519D, stepper 0x6326D1.
  - Runnable = FUN_00625130: +0x44 started && !+0xA8 && !+0x9D. +0x9D is set at MP start.
  - Fast-forward: GlobalData+0xBBD / isTimeFast 0x603491. Camera time multiplier: TacticalView vtDC.
- Xfer:
  - GameEngine has no Snapshot interface and no xfer code touches +0x34..+0x5C, so the stepper is **not saved**.
  - GameLogic::xfer 0x6308F3 does not xfer +0x17C.
  - **GameClient::xfer 0x647ABF xfers TheGameClient+0x10, the client frame counter** (`lea ecx,[edi+4]` with edi=GameClient+0xC, vt78 at 0x647AFE).
  - So keep the counter at 30 Hz, or save games from the two modes will hold client frame values on different scales.

---------------------------------------------------------------------------------------------------
## f) Core patch set (hybrid). The DLL keeps globals mode60, s12 (stored in +0x34), renderId, halfToggle and prevKind.
None of these sites overlap AotR patches. I scanned .danetta, .angmar and the other extra sections for absolute
references to the timing globals and engine objects and found none.

P1 Stepper. Hook the call at **0x441822 `e8 79 0d 1f 00`** (call 0x6325A0) and point it to `GE_update60(this)`. In 30 mode, or when MP, call the original.
```
renderId++ (skip 0xFFFFFFFF)
TheAssetMgr->vt28; FUN_00604189(SE); ext=FUN_00603452(SE); e->vt9C()        // unchanged order
if ext {GC+0xC8=0; Debug->vt94; return}
GC+0xC8=1; s=e->sub+1                                  // e->sub(+0x34) is the 12-counter
if s<=12: e->sub=s; e->frac=s/12; dbg(logicFrame*10-1+(s+1)/2)
          if (s&1) e->vt98((s+1)/2)  /* logic sub 2..6 */  else GC+0xC8=0   /* B-frame */
else:     stats block as stock (every 25 ticks, +0x5C += +0x38 (=6))
          old=s; e->sub=1; e->frac=1/12; dbg; e->vt98(1)
          if (!GC+0xC8) {e->sub=old; e->frac=min(1,old/12)} else e->flag40=1
prevKind = A-ok | A-fail | B ; Debug->vt94
```
- Keep +0x38 at 6. Skip the stock refresh at 0x632604, or only do it when s==12 and flag40 is set.
- Switch between 30 and 60 mode only when +0x34==1. That value means "just ticked" in both scales.
  For the paused state, map 7↔13.
- Equivalent in-place byte sites, if you patch the code instead of hooking:
  - 0x632604 `83 f9 06` (refresh at end of cycle)
  - 0x632631 `6b c9 0a` / 0x632634 (debug id)
  - 0x632642 & 0x6326BE & 0x6326E4 `e8 .. call 0x63256F` (fraction = sub/+0x38)
  - **0x63264A `83 f8 06` + 0x63264D `0f 8e 98 00 00 00` (tick when sub>6)**
  - 0x6326ED-0x6326F0 `push eax; call [edx+0x98]`. This must pass (sub+1)/2 and only on odd sub. Even sub must store 0 to GC+0xC8 and must not call it.
  - 0x6325FB `88 98 c8 00 00 00` (flag=1).
P2 Limiter. Redirect **0x63A1AD `e8 f2 2d 40 00`** (call _ftol2) to a hook that calls _ftol2 and, when mode60, returns
   floor(P/2) and P-floor(P/2) on alternate frames. For P=33 that is 16 and 17, giving exactly the stock pair period.
   Optional: deadline-based catch-up so frames over 16 ms do not slow the game; B-frame renders may be dropped when behind.
P3 W3D sync. Replace **0x44B911..0x44B91E** (`a1 8c 7a dc 00` / `0f af c6` / `01 05 80 75 dc 00`, 14 bytes) with
   `56 e8 <Hook_SyncDelta(d)> 01 05 80 75 dc 00 90 90`. Hook rules, in 60 mode:
   - d>0: delta = 33*d - pendingHalf; pendingHalf = 0.
   - d==0 and the previous stepper frame was B: delta = h (16 or 17 alternating); pendingHalf = h.
   - otherwise (paused): delta = 0.
   - 30 mode: delta = d*[0xDC7A8C].
   Use the immediate 33 so other agents can change 0xDC7A8C for camera move frame counts.
   Disable mode60 during fast-forward or a time multiplier > 1, so the 0x44B8D8 branch stays stock.
P4 Interpolation cache keys. Load the 60 Hz renderId instead of TheGameClient->getFrame():
   - 0x6765D5 `8b 0d 88 43 de 00 8b 01 53 ff 50 7c` → `53 a1 <&renderId> 90×6`
   - 0x67173B `8b 0d 88 43 de 00 8b 01 ff 50 7c` → `a1 <&renderId> 90×6`
   - 0x671774 `8b 0d 88 43 de 00 8b 01 83 c4 10 ff 50 7c` → `83 c4 10 a1 <&renderId> 90×6`
   Leave the expiry compare at 0x671720..0x67172B, which uses the real client frame.
   The caches are invalidated with -1 at 0x67170B, 0x674611, 0x674B2C, 0x679CCE and 0x679CE6.
P5 Frozen-camera loop. In 60 mode, 0x44B9C1 `83 f9 1d` should alternate 0x0E and 0x0F, and 0x44B98C `83 c6 e2` should become -15.
   Only needed if camera steps per draw are halved by the camera agent.
P6 Do not touch: 0xD9F608, 0xD9F60C, the client float block, +0x38, the 0x632A95 catch-up path, or 0x63A181 `add ecx,6` (client-frame units).

Every other hard-coded "6 sub-frames" or 1/6 assumption:
- 0x63253C `6a 06` (isTickFrame, n<6 path)
- 0x632AA3 `83 f9 06`, 0x632AAF `6a 06`, 0x632AB5 `6b c0 06` (catch-up)
- GameLogic::update sub dispatch 0x62E4E8 (1..6 semantics)
- reset and init writes of +0x38 = FPS/LTR (0x635DCA, 0x63CF0F); ctor +0x38=1 (0x63A4DE)
- 0x6716E2 (+6 client frames)
- 0x44B8C5 `6a 1e` (fast-forward renders on frame%30)
- debug id `imul 0xA` at 0x632631 and 0x62EDxx (frame*10)
- per-render integrator FUN_004CBFFB (step/+0x38, runs at 2x)

Consequences that other agents must handle (anything that runs per GameEngine::update and is not gated by client frame or +0xC8):
- APT update, radar, message-stream translators (scroll and camera), audio, the GameClient::update subsystems
  (snow/cloud/fire, Anim2D, keyboard/mouse, Eva, WindowManager, video, terrain visual, Display::update, Shell, InGameUI)
- W3DDisplay drawFrame: camera via updateViews, render-time counters
- The particle manager is gated by +0xC8, so with B-frame +0xC8=0 it stays at 30 Hz with correct speed.
  To run it at 60 Hz, gate it differently and halve its per-update steps.
- Mechanism option: the DLL can skip any of these calls on renders that follow a B-frame. That keeps the exact stock cadence.
- Risk: the game speed is still tied to the frame rate. Frames over 16.5 ms slow the game down. Mitigation: catch-up, or drop B-frame renders.
