# Generals ZH (SAGE ancestor) — Timing / Frame-Advance Reference Map for BFME2 RotWK 60 FPS work

Source: EA GPL release, `GeneralsMD/Code` (paths below are relative to that dir; line numbers = this checkout).
Everything here was read from the actual code. Items marked **[BFME2-hypothesis]** are inferences about how
BFME2 likely diverged and must be verified in the binary.

Legend for "Gate" column used throughout:
- **LOOP** = runs once per main-loop iteration (= once per rendered frame), no gating.
- **CLI-Δ** = runs only when `GameClient::m_frame` changed since last call ("freezeTime" hack). In Generals
  `GameClient::m_frame` is *copied from the logic frame*, so CLI-Δ == "once per logic frame".
- **LOG-Δ** = explicitly gated on `TheGameLogic->getFrame()` changing.
- **SYNC** = driven by `WW3D::Get_Sync_Time()` / `Get_Frame_Time()` deltas (ms of "W3D time").
- **RT** = real wall-clock (`timeGetTime`, `GetTickCount`, `QueryPerformanceCounter`) — already FPS-independent.
- **DRAW** = advanced inside a draw/render call (per call; may run more than once per frame, or only for on-screen objects).

---------------------------------------------------------------------------------------------------

## 0. TL;DR for the BFME2 effort

1. Generals has **one** time base: the logic frame. `LOGICFRAMES_PER_SECOND = 30` (enum, compile-time).
   Client "frame" = logic frame (`GameLogic::update` calls `TheGameClient->setFrame(now)`). W3D time is a
   synthetic counter: `syncTime += TheW3DFrameLengthInMsec (Int 33)` once per *non-frozen* client frame,
   then `WW3D::Sync(syncTime)`. **No interpolation exists anywhere** in Generals.
2. Game speed == loop rate: single-player logic runs once per `GameEngine::update()` (i.e. once per rendered
   frame) unless paused. The only limiter is a busy-wait in `GameEngine::execute` using `m_maxFPS`
   (= `GameData.ini FramesPerSecondLimit`), active only if `TheGlobalData->m_useFpsLimit`.
3. In network games Generals sets `m_useFpsLimit = false`; logic is paced by `Network::timeForNewFrame()`
   (QPC, `m_frameRate` negotiated, capped at 30 and at `m_framesPerSecondLimit`). The render loop spins free
   and the "freezeTime" hack (`lastFrame == m_frame`) makes all CLI-Δ systems advance only when logic advanced.
4. BFME2 (5 Hz logic / 30 Hz client) must have split this: a client-frame counter that ticks every render
   frame, logic every 6th, and the 0xD9F608 block replaces the Generals compile-time constants:

| BFME2 global @0xD9F608 block | Generals equivalent (GameCommon.h:72-78) | meaning |
|---|---|---|
| int 5 | `LOGICFRAMES_PER_SECOND` (was 30) | logic Hz |
| int 30 (513 refs) | new: client frames/sec (Generals used `LOGICFRAMES_PER_SECOND` for both) | client Hz |
| float 0.005 | `LOGICFRAMES_PER_MSEC_REAL` (30/1000 in Gen) | `ConvertDurationFromMsecsToFrames` factor (logic) |
| float 200.0 | `MSEC_PER_LOGICFRAME_REAL` (33.33 in Gen) | ms per logic frame |
| float 5.0 | `LOGICFRAMES_PER_SECONDS_REAL` | |
| float 0.2 | `SECONDS_PER_LOGICFRAME_REAL` | `ConvertVelocityInSecsToFrames`, accel (squared), angular vel |
| float 33.333 | `MSEC_PER_LOGICFRAME_REAL` (client flavour) / `TheW3DFrameLengthInMsec` | ms per client frame; probably fed to W3D sync |
| float 0.03 | `LOGICFRAMES_PER_MSEC_REAL` (client flavour) | client-duration INI parse factor |
| float 30.0 | `LOGICFRAMES_PER_SECONDS_REAL` (client) | |
| float 0.0333 | `SECONDS_PER_LOGICFRAME_REAL` (client) / `CameraShakerSystem.Timestep(1.0f/30.0f)` | client velocity factor |

   Because they are *writable globals* rather than immediates, almost every Generals call site that used a
   `LOGICFRAMES_PER_SECOND`-derived immediate will show up in BFME2 as a load from this block — xrefs to the
   client half (int 30 / 33.333 / 0.03 / 0.0333) are the to-do list of "per client frame" systems.
5. Systems that will run 2x speed at 60 FPS fall in three buckets (details §7):
   (a) **per-call integrators** (`x += rate` / `x *= damping` each frame): camera shake, camera height settle,
       scroll, keyboard rotate/zoom, client physics xform (pitch/roll/wobble), tread/wheel/rope, tree sway,
       water UV, particles, tint envelopes, fades, GUI transitions, audio fades, recoil, debris, screen filters;
   (b) **frame-count durations** (counts parsed from ms into frames, or raw frame INI values): particle
       lifetimes/keyframes/burst delays, light pulses, move hints (40 frames), flash periods, camera scripted
       moves (`numFrames = ms / TheW3DFrameLengthInMsec`), keyboard repeat;
   (c) **W3D sync-time** consumers (HAnim playback, UV mappers, seg-line scroll, terrain tracks, clouds, snow):
       correct automatically **iff** `WW3D::Sync` is fed with real elapsed ms (or 16.67 ms per 60-Hz frame)
       instead of a fixed 33 ms per frame.
   Real-time (RT) systems (shroud fade, mouse cursor anim, letterbox, sky scroll, water tracks, window
   slide anims, shell 30 Hz throttle, movies) are already safe.

---------------------------------------------------------------------------------------------------

## 1. Time units, constants and conversion helpers

### 1.1 `GameEngine/Include/Common/GameCommon.h:70-108`
```cpp
enum { LOGICFRAMES_PER_SECOND = 30, MSEC_PER_SECOND = 1000 };
const Real LOGICFRAMES_PER_MSEC_REAL   = 30/1000.f;   // 0.03
const Real MSEC_PER_LOGICFRAME_REAL    = 1000/30.f;   // 33.333
const Real LOGICFRAMES_PER_SECONDS_REAL= 30.f;
const Real SECONDS_PER_LOGICFRAME_REAL = 1/30.f;      // 0.0333
inline Real ConvertDurationFromMsecsToFrames(Real ms)      { return ms * LOGICFRAMES_PER_MSEC_REAL; }
inline Real ConvertVelocityInSecsToFrames(Real d)           { return d * SECONDS_PER_LOGICFRAME_REAL; }
inline Real ConvertAccelerationInSecsToFrames(Real d)       { return d * (SPF*SPF); }
inline Real ConvertAngularVelocityInDegreesPerSecToRadsPerFrame(Real d){ return d*(SPF*PI/180); }
```
Because these are `const` in Generals they are folded into immediates. In BFME2 they became globals (above).

### 1.2 INI conversion parsers — `GameEngine/Source/Common/INI/INI.cpp`
| parser | line | conversion | used by (examples) |
|---|---|---|---|
| `INI::parseDurationReal` | 1709 | ms → frames (Real) | `SuperweaponCountdownFlashDuration`, `NamedTimerCountdownFlashDuration` |
| `INI::parseDurationUnsignedInt` | 1717 | ms → ceil(frames) | `FloatingTextTimeOut`, LightPulse `IncreaseTime/DecreaseTime`, W3DLaserDraw `MaxIntensityLifetime/FadeLifetime`, TreeDraw `MoveOutwardTime/MoveInwardTime/SinkTime`, `TimeToFadeAudio`, `DefaultOcclusionDelay`, `BaseRegenDelay` |
| `INI::parseDurationUnsignedShort` | 1725 | ms → ceil(frames) | Anim2D `AnimationDelay` |
| `INI::parseVelocityReal` | 1733 | /s → /frame | `FloatingTextMoveUpSpeed`, `FloatingTextVanishRate`, W3DTankDraw `TreadAnimationRate`, Tracer `Speed`, Locomotor speeds |
| `INI::parseAccelerationReal` | 1742 | /s² → /frame² | Locomotor `Acceleration`, `Braking`, `Lift` |
| `INI::parseAngularVelocityReal` | 598 | deg/s → rad/frame | Locomotor `BounceAmount`, turn rates |

Note: Generals uses ONE set of factors for logic *and* client values. **[BFME2-hypothesis]** BFME2 has two
families (logic: 0.005/0.2; client: 0.03/0.0333). If the client family is only read at INI-load time, then
changing the client globals (30 → 60, 0.03 → 0.06, 0.0333 → 0.01667, 33.333 → 16.667) *before* INI load would
rescale every client-side duration/velocity parsed from INI — but raw frame-count INI fields (particle
`Lifetime`, `BurstDelay`, `InitialDelay`, `SystemLifetime`, keyframe frames; see §7.3) are NOT converted.

`INI::load` (INI.cpp:351) calls `setFPMode()` first ("so we have consistent Real values for GameLogic") and
hashes **raw INI text lines** into an `XferCRC` (INI.cpp:482-486) when loaded with a CRC xfer. GameData.ini is
CRC'd (GameEngine.cpp:363) → `TheGlobalData->m_iniCRC`. **Editing GameData.ini (e.g. FramesPerSecondLimit)
changes the INI CRC used for MP compatibility; patch in code instead.**

### 1.3 `setFPMode()` — `GameLogic.cpp:198`
`_fpreset(); _statusfp(); _controlfp(RC_NEAR | PC_24, _MCW_PC|_MCW_RC)`. Called at the top of
`GameLogic::update` (3580), `GameLogic::init/reset/startNewGame`, `INI::load`, LoadScreen, MapUtil, ScoreScreen,
GameClient legal-page loop. Determinism depends on x87 at 24-bit precision during logic. Any injected client
code that changes the FPU CW must restore it (D3D without FPU_PRESERVE also forces single precision).
**Anchor:** imports `_controlfp`, `_statusfp`, `_fpreset` (msvcr71) → `setFPMode` → its callers include
`GameLogic::update` (first call in the function).

### 1.4 Other hard-coded per-frame constants worth knowing
- `GameEngine.h:37 #define DEFAULT_MAX_FPS 45` (overwritten by `setFramesPerSecondLimit(TheGlobalData->m_framesPerSecondLimit)` in init).
- `W3DView.cpp:108 Int TheW3DFrameLengthInMsec = 1000/LOGICFRAMES_PER_SECOND;` (=33, *Int*). Set again by
  `W3DGameClient::setFrameRate(Real msecsPerFrame)` (W3DGameClient.h:122) from `GameClient::init`
  (GameClient.cpp:252: `setFrameRate(MSEC_PER_LOGICFRAME_REAL)` — 33.33 truncated to 33 → W3D time runs 1% slow).
- `Drawable.h:279 DRAWABLE_FRAMES_PER_FLASH = LOGICFRAMES_PER_SECOND/2` (15).
- `Keyboard.h:90 KEY_REPEAT_DELAY = 10` (input frames).
- `LookAtXlat.cpp:67 Int SCROLL_AMT = 100;` (global, writable), `edgeScrollSize = 3`, `CLICK_DURATION = 5` (client frames, line 265).
- `InGameUI.cpp:5343 FRAMES_BEFORE_EXPIRE_TO_FADE = LOGICFRAMES_PER_SECOND*1`.
- `W3DInGameUI.cpp:478` move hint lifetime `elapsed <= 40` client frames.
- `Radar.cpp:66 RADAR_QUEUE_TERRAIN_REFRESH_DELAY = LOGICFRAMES_PER_SECOND*3.0f`; `Radar.cpp:1303 framesBetweenEvents = LOGICFRAMES_PER_SECOND*10`.
- `W3DDisplay.cpp:868 START_CUMU_FRAME = LOGICFRAMES_PER_SECOND/2`; `FPS_HISTORY_SIZE = 30`, `MaximumFrameTimeCutoff = 0.5`.
- `W3DDisplay.cpp:2009 LETTER_BOX_FADE_TIME 1000.0f` (ms, RT).
- `W3DShroud.cpp:727 FOG_INTERPOLATION_RATE (255.0f/1000.0f)` (per ms, RT).

---------------------------------------------------------------------------------------------------

## 2. Main loop

### 2.1 Entry
- `Main/WinMain.cpp:880-1083 WinMain` → arg parsing, window creation, `GameMain(argc, argv)` (1067).
  WndProc: `WM_ACTIVATEAPP` → `TheGameEngine->setIsActive()` + `Reset_D3D_Device`; mouse messages are queued into
  `TheWin32Mouse->addWin32Event(msg,w,l,TheMessageTime)`. No timers.
- `GameEngine/Source/Common/GameMain.cpp`: `TheGameEngine = CreateGameEngine(); ->init(argc,argv); ->execute(); delete`.
- `CreateGameEngine()` (WinMain.cpp:1106) → `new Win32GameEngine` (W3DGameEngine actually derives; device layer).
- `GameEngine::GameEngine()` (GameEngine.cpp:183): `timeBeginPeriod(1)`; dtor `timeEndPeriod(1)`.
  **Anchor:** import `timeBeginPeriod` has very few xrefs → GameEngine ctor → vtable.

### 2.2 `GameEngine::init(int argc, char* argv[])` — GameEngine.cpp:251
Order of subsystem creation (string names are passed to `initSubsystem(..., "TheXxx")` → survive in release):
TheLocalFileSystem, TheArchiveFileSystem, **TheWritableGlobalData** (`Data\INI\Default\GameData.ini`,
`Data\INI\GameData.ini`), (Water.ini/Weather.ini via `ini.load`), TheGameText, TheScienceStore,
TheMultiplayerSettings, TheTerrainTypes, TheTerrainRoads, TheGlobalLanguageData, TheCDManager, **TheAudio**,
TheFunctionLexicon, TheModuleFactory, TheMessageStream, TheSidesList, TheCaveSystem, TheRankInfoStore,
ThePlayerTemplateStore, **TheParticleSystemManager**, TheFXListStore, TheWeaponStore,
TheObjectCreationListStore, TheLocomotorStore, TheSpecialPowerStore, TheDamageFXStore, TheArmorStore,
TheBuildAssistant, TheThingFactory, TheUpgradeCenter, **TheGameClient**, TheAI, **TheGameLogic**,
TheTeamFactory, TheCrateSystem, ThePlayerList, TheRecorder, **TheRadar**, TheVictoryConditions, TheMetaMap,
TheActionManager, TheGameStateMap, TheGameState, TheGameResultsQueue.
Then `TheSubsystemList->postProcessLoadAll(); setFramesPerSecondLimit(TheGlobalData->m_framesPerSecondLimit);`
(GameEngine.cpp:559). `TheNetwork = NULL` until a MP game starts.

### 2.3 `GameEngine::execute()` — GameEngine.cpp:789-910 (the real main loop)
```cpp
DWORD prevTime = timeGetTime();
while (!m_quitting) {
    try { update(); }                              // virtual → Win32GameEngine::update
    catch (INIException e) { RELEASE_CRASH(e.mFailureMessage or "Uncaught Exception in GameEngine::update"); }
    catch (...) { TheRecorder->cleanUpReplayFile(); RELEASE_CRASH(("Uncaught Exception in GameEngine::update")); }

    if (TheTacticalView->getTimeMultiplier() <= 1 && !TheScriptEngine->isTimeFast()) {
        // (debug/internal builds only: ::Sleep(1))
        if (!(TheGlobalData->m_TiVOFastMode && TheGameLogic->isInReplayGame())) {
            DWORD now   = timeGetTime();
            DWORD limit = (1000.0f / m_maxFPS) - 1;        // 30 fps -> 32 ms (float->DWORD via _ftol)
            while (TheGlobalData->m_useFpsLimit && (now - prevTime) < limit) {
                ::Sleep(0);
                now = timeGetTime();
            }
            prevTime = now;
        }
    }
}
```
- Busy-wait limiter, granularity 1 ms (`timeBeginPeriod(1)`), effective ≈ 30.3 FPS at limit 30.
- Bypassed by: camera time multiplier > 1, `ScriptEngine::isTimeFast()`, TiVO fast-forward in replays.
- **Anchors:** float `1000.0f` (0x447A0000) divided by an int member, `-1`, `Sleep(0)` + `timeGetTime` in a
  loop, and the release string `"Uncaught Exception in GameEngine::update"`.

### 2.4 `Win32GameEngine::update()` — GameEngineDevice/Source/Win32Device/Common/Win32GameEngine.cpp
```cpp
GameEngine::update();
if (IsIconic(ApplicationHWnd)) {             // alt-tabbed
    while (IsIconic(...)) { Sleep(5); serviceWindowsOS(); TheLAN->update();
        if (quitting || isInInternetGame || isInLanGame) break; }   // MP keeps running logic
    TheAudio->setVolume(TheAudio->getVolume(0x10), 0x10);          // wake Miles
}
serviceWindowsOS();   // PeekMessage(PM_NOREMOVE)/GetMessage/TranslateMessage/DispatchMessage; TheMessageTime = msg.time
```
**Anchor:** `PeekMessageA` xref → `serviceWindowsOS` → caller `Win32GameEngine::update`, which first calls
`GameEngine::update` and contains `Sleep(5)` + `IsIconic`.

### 2.5 `GameEngine::update()` — GameEngine.cpp:746-780 (one "frame")
```cpp
TheRadar->UPDATE();                 // Radar::update  (logic-frame based events, see §7.8)
TheAudio->UPDATE();                 // MilesAudioManager::update (per-loop fades/delays, §7.11)
TheGameClient->UPDATE();            // GameClient::update -> includes TheDisplay->DRAW() (render!)
TheMessageStream->propagateMessages();   // translators: LookAt (scroll), Selection, Command, ... -> TheCommandList
if (TheNetwork) TheNetwork->UPDATE();    // Network::update
TheCDManager->UPDATE();
if ((TheNetwork == NULL && !TheGameLogic->isGamePaused()) ||
    (TheNetwork && TheNetwork->isFrameDataReady()))
    TheGameLogic->UPDATE();         // GameLogic::update  (m_frame++ at end)
```
Notes:
- `UPDATE()`/`DRAW()` are inline wrappers for the virtual `update()`/`draw()` in release (SubsystemInterface.h;
  DUMP_PERF_STATS variant only in debug/internal). `SubsystemInterface` vtable: [0] dtor, [1] init,
  [2] postProcessLoad, [3] reset, [4] **update**, [5] **draw** (+0x10 / +0x14). BFME2 may have extra slots.
- Order: **client (incl. render) first, logic last** in the same iteration. Rendering of iteration k shows the
  logic state produced in iteration k-1.
- Single player: logic runs every iteration → game speed = loop rate = `FramesPerSecondLimit`. This is exactly
  the BFME2 symptom (there logic = every 6th client frame, see §9).
- There is **no `isGameHalted`** in Generals; only `GameLogic::isGamePaused()` (12 header refs) and the
  script/camera "time frozen" flags (`ScriptEngine::isTimeFrozenScript/Debug`, `View::isTimeFrozen` +
  `isCameraMovementFinished`). **[BFME2-hypothesis]** a halt flag is BFME2-specific.

### 2.6 Who sets the FPS limit (all `setFramesPerSecondLimit` / `m_useFpsLimit` writers)
| site | effect |
|---|---|
| GameEngine.cpp:559 (init) | `m_maxFPS = GameData FramesPerSecondLimit` |
| GameLogicDispatch.cpp:432-439 (`MSG_NEW_GAME`, 4th int arg) | `maxFPS` from skirmish "game speed" slider (SkirmishGameOptionsMenu.cpp:425-457, clamps 15..1000); sets `m_useFpsLimit = true` |
| ScriptActions.cpp:6990-7003 (`SET_FPS_LIMIT`) | script action; 0 → restore INI value; forces `m_useFpsLimit = true` |
| ScriptEngine.cpp:5280 (`ScriptEngine::reset`) | restore INI value |
| QuitMenu.cpp:223 | reads limit to restart skirmish with same speed |
| GameLOD.cpp:352/583, OptionsMenu (`FPSLimit` pref) | user toggle of `m_useFpsLimit` |
| **LANAPICallbacks.cpp:264, StagingRoomGameInfo.cpp:863, GameSpyGameInfo.cpp:566** | **MP start: `m_useFpsLimit = false`** (render unlocked, logic network-paced) |
| CommandLine.cpp:518 `-fps N`, :1048 `-noFPSLimit` (debug/internal-only table, 1149-1234) | `m_framesPerSecondLimit = N` / `m_useFpsLimit = false, limit = 30000` — likely absent in BFME2 retail |
Debug string (debug builds only): `"GameEngine::setFramesPerSecondLimit() - setting max fps to %d (TheGlobalData->m_useFpsLimit == %d)\n"`.

### 2.7 Network frame pacing — `GameEngine/Source/GameNetwork/`
- `Network::init` (Network.cpp:324): `m_runAhead = min(max(30,MIN_RUNAHEAD),MAX_FRAMES_AHEAD/2)`,
  **`m_frameRate = 30`**, `QueryPerformanceFrequency(&m_perfCountFreq)`, `m_nextFrameTime = 0`.
- `Network::update` (684): `m_frameDataReady=FALSE; GetCommandsFromCommandList(); m_conMgr->updateRunAhead(m_runAhead, m_frameRate, m_didSelfSlug, getExecutionFrame()); liteupdate();`
  then `if (AllCommandsReady(TheGameLogic->getFrame())) { handleAllCommandsReady(); if (timeForNewFrame()) { RelayCommandsToCommandList(frame); m_frameDataReady = TRUE; } }`.
- `Network::timeForNewFrame` (760): QPC; `frameDelay = freq / m_frameRate`; if minimum cushion <
  `m_runAhead * NetworkRunAheadSlack%` → `frameDelay += frameDelay/10` and `m_didSelfSlug=TRUE`;
  `if (now >= m_nextFrameTime) { if (m_nextFrameTime + 2*frameDelay < now) m_nextFrameTime = now; else m_nextFrameTime += frameDelay; return TRUE; }`.
  → **logic tick pacing in MP is wall-clock based at `m_frameRate` Hz**, independent of render rate.
- `Network::isFrameDataReady` (807): `m_frameDataReady || localStatus == NETLOCALSTATUS_LEFT`.
- `Network::processRunAheadCommand` (644): `m_runAhead`, `m_frameRate` from packet; frame grouping =
  `(1000*runAhead/frameRate)/2` ms clamped [1,500].
- `ConnectionManager::updateRunAhead` (ConnectionManager.cpp:1211): every `NetworkRunAheadMetricsTime` ms
  (timeGetTime). Packet router computes `minFps` over players from `FrameMetrics::getAverageFPS()`, keeps old
  rate if within 10%, clamps `>=5`, **`<= TheGlobalData->m_framesPerSecondLimit`**, `newRunAhead =
  maxLatency/2 * minFps * (1+slack%)` clamped [MIN_RUNAHEAD, MAX_FRAMES_AHEAD/2]; sends
  `NetRunAheadCommandMsg(runAhead, frameRate)`; slowest player gets `minFps*11/10` **capped at hard-coded 30**.
- `FrameMetrics::doPerFrameMetrics` (FrameMetrics.cpp:89): once per second samples **`TheDisplay->getAverageFPS()`**
  (render FPS from `W3DDisplay::updateAverageFPS`, QPC) into a history of `NetworkFPSHistoryLength`;
  latency history in seconds. `init()` seeds fps 30, latency 0.2.
- **Implication:** in Generals the negotiated "net FPS" = logic FPS and is derived from *render* FPS. In BFME2
  check whether `m_frameRate`/run-ahead are in logic frames (5) or client frames (30), and whether the 30 caps
  became the CLIENT_FPS global. A 60-FPS client must not raise the negotiated logic rate.
- Debug HUD string (release, wide): `L"Run Ahead: %d, Net FPS: %d, Packet arrival cushion: %d"` (W3DDisplay.cpp:1372)
  → calls `TheNetwork->getRunAhead()`, `getFrameRate()`, `getPacketArrivalCushion()` (vtable slots of Network).
- GameData fields: `NetworkFPSHistoryLength`, `NetworkLatencyHistoryLength`, `NetworkCushionHistoryLength`,
  `NetworkRunAheadMetricsTime`, `NetworkRunAheadSlack`, `NetworkKeepAliveDelay`, `NetworkDisconnectTime`,
  `NetworkPlayerTimeoutTime`, `NetworkDisconnectScreenNotifyTime`.

---------------------------------------------------------------------------------------------------

## 3. `GameLogic::update()` — GameLogic.cpp:3572-3829

```cpp
LatchRestore<Bool> inUpdateLatch(m_isInUpdate, TRUE);    // isInGameLogicUpdate()
setFPMode();
if (m_startNewGame && !TheDisplay->isMoviePlaying()) { startNewGame(FALSE); m_startNewGame = FALSE; }
UnsignedInt now = TheGameLogic->getFrame();
TheGameClient->setFrame(now);                // <<< client frame := logic frame  (GameClient::setFrame is VIRTUAL)
TheScriptEngine->UPDATE();                   // script fades (updateFades, per logic frame), camera script actions
freezeTime = (TacticalView->isTimeFrozen() && !isCameraMovementFinished()) || ScriptEngine time-frozen;
if (freezeTime) { if (!CommandList has MSG_CLEAR_GAME_DATA) return; else forceUnfreezeTime(); }
TheTerrainLogic->UPDATE();
CRC every TheGameInfo->getCRCInterval() frames (MP) / REPLAY_CRC_INTERVAL (solo) -> MSG_LOGIC_CRC
TheStatsCollector->update(); TheRecorder->UPDATE(); processCommandList(TheCommandList);
sleepy update-module heap (friend_getNextCallFrame() <= now → u->update(); setNextCallFrame(now+sleep))
TheAI->UPDATE(); TheBuildAssistant->UPDATE(); ThePartitionManager->UPDATE();
processDestroyList(); TheCommandList->reset();
TheWeaponStore->UPDATE(); TheLocomotorStore->UPDATE(); TheVictoryConditions->UPDATE();
for obj in m_objList: if (obj->isDisabled()) obj->checkDisabledStatus();
if (!m_startNewGame) m_frame++;              // <<< the only place world time advances
```
- `GameLogic::getFrame()` is **inline** (`GameLogic.h:397`, member read); `GameClient::getFrame()`/`setFrame()`
  are **virtual** (GameClient.h:99/138) → vtable calls in the binary.
- Logic does not call any particle/draw update; all of that is client.

---------------------------------------------------------------------------------------------------

## 4. `GameClient::update()` — GameClient.cpp:513-776

```cpp
GameMessage *m = TheMessageStream->appendMessage(MSG_FRAME_TICK); m->appendTimestampArgument(getFrame());
// intro: playLogoMovie("EALogoMovie"/"EALogoMovie640",5000,3000); playMovie("Sizzle"/"Sizzle640");
//        legal page "Menus/LegalPage.wnd" shown for 4000 ms: loop {TheWindowManager->update(); TheDisplay->draw(); Sleep(100);} setFPMode();
//        TheShell->showShellMap(TRUE); TheShell->showShell();
TheSnowManager->UPDATE();          // W3DSnowManager::update: m_time += WW3D::Get_Frame_Time()/1000  (SYNC)
TheAnim2DCollection->UPDATE();     // Anim2D frames advance on logic-frame deltas
TheKeyboard->UPDATE(); TheKeyboard->createStreamMessages();     // m_inputFrame++ (LOOP)
TheEva->UPDATE();
TheMouse->UPDATE(); TheMouse->createStreamMessages();
if (TheInGameUI->isCameraTrackingDrawable()) TheTacticalView->lookAt(firstSelectedDrawable pos);
if (m_playIntro || m_afterIntro) { TheDisplay->DRAW(); TheDisplay->UPDATE(); return; }
TheWindowManager->UPDATE();        // GameWindowManager::update -> TheTransitionHandler->update() (LOOP, frame counter)
TheVideoPlayer->UPDATE();
freezeTime = (view time frozen && !camera finished) || script frozen (debug/script) || TheGameLogic->isGamePaused();
static UnsignedInt lastFrame = ~0;                         // "hack to let client spin fast in network games
freezeTime = freezeTime || (lastFrame == m_frame);         //  but still do effects at the same pace. -MDC"
lastFrame = m_frame;
if (!freezeTime) {
    TheGhostObjectManager->updateOrphanedObjects(NULL,0);
    for each Drawable d: (shroud status w/ 2*LOGICFRAMES_PER_SECOND (+3 s if dead) linger) ; d->updateDrawable();   // CLI-Δ
}
if (!freezeTime) TheParticleSystemManager->setLocalPlayerIndex(i);  // update() itself moved to W3DDisplay::draw
TheTerrainVisual->UPDATE();        // W3DTerrainVisual::update -> WaterRenderObjClass::update (LOOP!)
TheDisplay->UPDATE();              // Display::update: video stream frames (Bink, RT)
TheDisplay->DRAW();                // W3DDisplay::draw  (render + W3D sync, §5)
TheDisplayStringManager->update();
TheShell->UPDATE();                // Shell::update — internally throttled to 30 Hz via timeGetTime (RT)
TheInGameUI->UPDATE();             // InGameUI::update (LOOP, §7.6)
```
**Anchors:** strings `"EALogoMovie"`, `"EALogoMovie640"`, `"Sizzle"`, `"Sizzle640"`, `"Menus/LegalPage.wnd"`;
`Sleep(100)` + `4000` + `timeGetTime` loop; first statement appends message type `MSG_FRAME_TICK`
(message name table `GameMessage::getCommandTypeAsAsciiString`, MessageStream.cpp:235-, is *not* debug-only →
string `"MSG_FRAME_TICK"` gives the enum value).
`GameClient::init` (GameClient.cpp:249): `setFrameRate(MSEC_PER_LOGICFRAME_REAL)`; `INI "Data\\INI\\DrawGroupInfo.ini"`;
translators attached with priorities: WindowTranslator 10, MetaEventTranslator 20, HotKeyTranslator 25,
PlaceEventTranslator 30, GUICommandTranslator 40, SelectionTranslator 50, **LookAtTranslator 60**,
CommandTranslator 70, HintSpyTranslator 100, GameClientMessageDispatcher 999999999.
Subsystem name strings set here: `"TheDisplayStringManager"`, `"TheKeyboard"`, `"TheAnim2DCollection"`,
`"TheMouse"`, `"TheDisplay"`, `"TheWindowManager"`, `"TheIMEManager"`, `"TheShell"`, `"TheInGameUI"`,
`"TheHotKeyManager"`, `"TheTerrainVisual"`, `"TheRayEffects"`, `"TheVideoPlayer"`, `"TheLanguageFilter"`,
`"TheEva"`, `"TheSnowManager"`; allocation tag strings `"GameClientSubsystem"` / `"GameEngineSubsystem"`.
`TheTacticalView` is created in `InGameUI::init` (InGameUI.cpp:1164: `createView(); init(); attachView();
setWidth(display w); setHeight(display h * 0.77f)` → float 0.77 = 0x3F451EB8).

---------------------------------------------------------------------------------------------------

## 5. `W3DDisplay::draw()` — GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp:1664-2005

```cpp
static UnsignedInt syncTime = 0;                       // function-static W3D clock
if (ApplicationHWnd && IsIconic(ApplicationHWnd)) return;
updateAverageFPS();      // QPC; 30-sample history; ignores >0.5 s spikes -> m_averageFPS (used by LOD + net metrics)
dynamic LOD from m_averageFPS (TheGameLODManager->findDynamicLODLevel); terrain LOD auto
(stats: gatherDebugStats() if m_debugDisplayCallback == StatDebugDisplay)
freezeTime = (view frozen && !camera finished) || script frozen || TheGameLogic->isGamePaused();
static UnsignedInt lastFrame = ~0;
freezeTime = freezeTime || (lastFrame == TheGameClient->getFrame());   // same hack as GameClient::update
lastFrame = TheGameClient->getFrame();
W3DView *primary = (W3DView*)getFirstView();
if (!freezeTime && TheScriptEngine->isTimeFast()) {          // script fast-forward: skip rendering
    primary->updateCameraMovements(); syncTime += TheW3DFrameLengthInMsec; return; }
Debug_Statistics::Begin_Statistics();
if (!m_loadScreenRender) {
    TheTerrainTracksRenderObjClassSystem->update();           // SYNC (fade by Get_Sync_Time - timeAdded)
    TheTerrainRenderObject->getShroud()->render(camera);      // -> interpolateFogLevels (RT)
}
if (!freezeTime) {
    /// @todo Decouple framerate from timestep
    // for now, use constant time steps to avoid animations running independent of framerate
    syncTime += TheW3DFrameLengthInMsec;                      // +33 per non-frozen client frame
    //	WW3D::Sync( GetTickCount() );                         // (real-time variant, commented out)
}
WW3D::Sync(syncTime);
// second limiter: "Fast & Frozen time limits the time to 33 fps."
Int minTime = 30; static Int prevTime = timeGetTime(), now; now = timeGetTime();
if (TheTacticalView->getTimeMultiplier() > 1) { static Int c=1; if (--c > 1) return; c = multiplier; }
else { now = timeGetTime(); prevTime = now - minTime; }      // first pass immediate
do {
    if (!m_loadScreenRender) { while (m_useFpsLimit && (now-prevTime) < minTime-1) now = timeGetTime(); prevTime = now; }
    if (device->TestCooperativeLevel() == D3D_OK) {
        updateViews();                       // Display::updateViews -> W3DView::updateView -> W3DView::update (§6)
        TheParticleSystemManager->update();  // particles (LOG-Δ gated, §7.3)  "LORENZEN AND WILCZYNSKI MOVED THIS"
        if (TheWaterRenderObj && m_waterType==2) TheWaterRenderObj->updateRenderTargetTextures(cam);
        TheW3DProjectedShadowManager->updateRenderTargetTextures();
    }
    if ((TheGameLogic->getFrame() % 30 == 1) || !(TiVO fast mode ...)) {
        if (!m_breakTheMovie && !m_disableRender && WW3D::Begin_Render(true,true,black, minWaterOpacity)==OK) {
            if (m_loadScreenRender) { TheInGameUI->draw(); TheMouse->draw(); WW3D::End_Render(); continue; }
            drawViews();            // W3DView::draw -> RTS3DScene::doRender (trees, water, particles, models)
            TheInGameUI->DRAW();    // W3DInGameUI::draw: move/attack hints, preDraw(floating text, world anims), postDraw
            TheMouse->DRAW();
            video buffer; copyright string; renderLetterBox(now) (RT);
            cinematic text: if (m_cinematicTextFrames) { draw; m_cinematicTextFrames--; }   // LOOP counter (logic frames!)
            debug display; WW3D::End_Render();   // WW3D::FrameCount++
        }
    }
    if (script/debug frozen || paused) freezeTime = false;
} while (freezeTime && !TheTacticalView->isCameraMovementFinished());   // frozen-time camera moves render here at ~33 fps
```
Key facts:
- **W3D time advances by a constant 33 ms per non-frozen client frame**, *not* real time.
- The inner `minTime = 30` busy loop only throttles the *frozen-time camera* loop and time-multiplier mode to
  ≈33 FPS (first pass is immediate). A 60-FPS mode must also raise/remove this limiter.
- `W3DView::update` only re-applies drawable transforms when `WW3D::Get_Frame_Time() != 0` (see §6).
- **Anchors:** `IsIconic`, `QueryPerformanceFrequency/Counter` (inlined helpers `getPerformanceCounter*`,
  W3DDisplay.cpp:377-389), `TestCooperativeLevel` vtable call (IDirect3DDevice8 +0x0C), constant `% 30 == 1`,
  `minTime = 30` (cmp 29), wide strings in `gatherDebugStats` (release branch):
  `L"FPS: %.2f, %.2fms draws: %.2f skins: %.2f sort %.2f"`, `L", FPSLock %d"`, `L"FPS: %.2f"`,
  `L"OUT: %.2f bytes/sec, %.2f packets/sec"`, `L"Run Ahead: %d, Net FPS: %d, Packet arrival cushion: %d"`,
  font name `"FixedSys"`. From `gatherDebugStats` → W3DDisplay vtable → `draw` (Display::draw slot).
- `W3DDisplay::createLightPulse(pos,color,inner,attWidth,increaseFrameTime,decayFrameTime)` (2079) →
  `W3DDynamicLight::setFrameFade` → `W3DDynamicLight::On_Frame_Update` (W3DDynamicLight.cpp:45) decrements
  `m_curIncreaseFrameCount`/`m_curDecayFrameCount` **once per scene render** (called from
  `RTS3DScene::Customized_Render`, W3DScene.cpp:1115, skipped for the water pass) → DRAW counter; durations come
  from FXList LightPulse `IncreaseTime`/`DecreaseTime` (parseDurationUnsignedInt).

---------------------------------------------------------------------------------------------------

## 6. WW3D timing model — Libraries/Source/WWVegas/WW3D2

### 6.1 Core (`ww3d.h:171-174, 328-357`, `ww3d.cpp:170-195, 1105, 1180`)
```cpp
static unsigned int SyncTime;          // "absolute synchronized frame time (ms) supplied by the application"
static unsigned int PreviousSyncTime;
static int          FrameCount;        // ++ in WW3D::End_Render (ww3d.cpp:1105)
void WW3D::Sync(unsigned int t) { PreviousSyncTime = SyncTime; SyncTime = t; }      // tiny leaf function
unsigned Get_Sync_Time()  { return SyncTime; }                         // inline -> direct global read
unsigned Get_Frame_Time() { return SyncTime - PreviousSyncTime; }      // inline -> two global reads + sub
unsigned Get_Frame_Count(){ return FrameCount; }
```
- Only caller of `WW3D::Sync` in the whole engine: `W3DDisplay::draw` (W3DDisplay.cpp:1805).
- There is **no** `Get_Logic_Frame_Time_Milliseconds`, `FractionalSyncMs`, or `Sync(bool step)` in the EA
  Generals WW3D. **[BFME2-hypothesis]** If BFME2's WW3D has those (later SAGE), they will appear as extra globals
  next to SyncTime/PreviousSyncTime and the Sync() leaf will be larger. In the binary, identify SyncTime as the
  global written by a 2-store leaf function called once from the display draw function.

### 6.2 How model animations compute frames — `animobj.cpp`
- `Animatable3DObjClass::Render` (288) / `Special_Render` (319) / `Update_Sub_Object_Transforms` (780): if
  `CurMotionMode == SINGLE_ANIM && AnimMode != ANIM_MODE_MANUAL` → `Single_Anim_Progress()`.
- `Single_Anim_Progress` (1048): `PrevFrame = Frame; Frame = Compute_Current_Frame(&dir); LastSyncTime = WW3D::Get_Sync_Time(); Set_Hierarchy_Valid(false);`
- `Compute_Current_Frame` (~949):
  `frame += Motion->Get_Frame_Rate() * frameRateMultiplier * animDirection * (Get_Sync_Time() - LastSyncTime) * 0.001f;`
  then wrap per mode (ONCE clamp, LOOP wrap, ONCE_BACKWARDS, LOOP_BACKWARDS, LOOP_PINGPONG).
  → **HAnim playback is purely SYNC-time driven.** `Set_Animation()` and `operator=` reset `LastSyncTime`.
- Embedded animation sounds: `AnimatedSoundMgrClass::Trigger_Sound(motion, PrevFrame, Frame, bone)` in
  `Update_Sub_Object_Transforms` — fires on frame crossings (fine at any rate as long as frames are correct).
- `HLodClass::Set_Animation_Frame_Rate_Multiplier` (`frameRateMultiplier`) is how SAGE scales anim speed
  (W3DModelDraw `setCurAnimDurationInMsec`, `AnimationSpeedFactorRange`, `adjustAnimSpeedToMovementSpeed`).

### 6.3 All `Get_Sync_Time` / `Get_Frame_Time` consumers (SYNC bucket — fixed by correct Sync input)
| file:line | system | use |
|---|---|---|
| animobj.cpp:92,148,213,483,964,1061 | HAnim playback | Δsync × fps × mult |
| mapper.cpp:113-1065 | UV mappers (LinearOffset, Grid, Rotate, Sine, Step, ZigZag, Random, Bump…) | `delta = Get_Sync_Time() - LastUsedSyncTime` per Calculate_Texture_Matrix |
| seglinerenderer.cpp:86,209,235,245 | SegLine UV scroll (lasers, `W3DLaserDraw ScrollRate` via `Set_UV_Offset_Rate`) | Δsync × UVOffsetDeltaPerMS |
| part_buf.cpp:113,350,2495,2514,2544,2847,2941 | W3D-native particle buffers | elapsed sync ms; pingpong `FrameCount & 1` |
| part_emt.cpp:573,626 | W3D-native emitters | `Get_Frame_Time()` |
| dazzle.cpp:785-974,1586 | lens flares/dazzles | creation_time vs sync; frame time |
| sphereobj.cpp:1111 / ringobj.cpp:1153 | sphere/ring anim objects | `Get_Frame_Time()` |
| texproject.cpp:1320 | texture projector | frame_time/1000 |
| texture.cpp (many) | texture LastAccessed / inactivation timeout | bookkeeping only |
| dx8renderer.cpp:1896 | mapper reset on material swap | |
| W3DTerrainTracks.cpp:292,409,743 | tank track marks fade (`m_maxTankTrackFadeDelay`, default 300000) | `TerrainTracksRenderObjClassSystem::update` |
| W3DShaderManager.cpp:1572,1617 | `TerrainShader2Stage::updateNoise1` cloud/noise UV slide (`m_xSlidePerSecond = -0.02`) | Δsync/1000 |
| W3DSnow.cpp:165 | snow time | `m_time += Get_Frame_Time()/1000` |
| W3DGranny.cpp:299,783 | Granny anims (unused in ZH ship) | sync seconds |
| W3DView.cpp:1391 | **gate**: `if (WW3D::Get_Frame_Time()) iterateDrawablesInRegion(drawDrawable)` | §6.4 |

### 6.4 The drawable "draw" pass (where per-frame DRAW integrators run)
`W3DView::update` (W3DView.cpp:1084) ends with:
```cpp
getAxisAlignedViewRegion(region);
if (WW3D::Get_Frame_Time())     // "make sure some time actually elapsed"
    TheGameClient->iterateDrawablesInRegion(&region, drawDrawable, this);   // drawDrawable -> Drawable::draw(view)
```
`Drawable::draw` (Drawable.cpp:2636): fades `m_secondMaterialPassOpacity *= 0.8f` (until < 0.001), early-out if
hidden/shrouded, builds transform, **`applyPhysicsXform(&mtx)`** (client suspension/wobble integration, §7.1),
then `for each DrawModule: doDrawModule(&mtx)` (W3DModelDraw & subclasses, §7.2).
→ Only drawables inside the view region are "drawn"/integrated; called once per non-frozen W3D frame.

---------------------------------------------------------------------------------------------------

## 7. Catalog of client systems advanced per frame (what breaks at 60 FPS)

### 7.1 Drawable (GameEngine/Source/GameClient/Drawable.cpp)
| function (line) | gate | advances | constants / INI |
|---|---|---|---|
| `Drawable::updateDrawable` (1168) | CLI-Δ (GameClient::update) | client update modules `(*cu)->clientUpdate()`; **fade in/out**: `++m_timeElapsedFade` vs `m_timeToFade` (frames, from `fadeOut/fadeIn(frames)` 1072/1083); **decal opacity** `m_decalOpacity += m_decalOpacityFadeRate` (`setTerrainDecalFadeTarget(target, rate)` 868); expiration `now >= m_expirationDate` (logic frame); **flash**: `if (m_flashCount>0 && TheGameClient->getFrame() % DRAWABLE_FRAMES_PER_FLASH == 0) colorFlash(); m_flashCount--`; tint status edge → `m_colorTintEnvelope->play(color, 30, 30, SUSTAIN_INDEFINITELY)` (disabled), `(150,150)` subdual, `(30,30)` frenzy; `m_colorTintEnvelope->update()`; `m_selectionFlashEnvelope->update()`; ambient sound restart | `DRAWABLE_FRAMES_PER_FLASH = LOGICFRAMES_PER_SECOND/2`; colors `DARK_GRAY_DISABLED_COLOR {-.5,-.5,-.5}`, `SUBDUAL_DAMAGE_COLOR {-.2,-.2,.8}`, `FRENZY_COLOR {.2,-.2,-.2}`, `FRENZY_COLOR_INFANTRY {0,-.7,-.7}` |
| `TintEnvelope::play/setAttackFrames/setDecayFrames/update` (5503-5600) | via updateDrawable | ATTACK: `m_currentColor += m_attackRate` (= Δ/attackFrames); DECAY: `+= m_decayRate` (=-peak/decayFrames); SUSTAIN: `--m_sustainCounter` | `FADE_RATE_EPSILON 0.001f`; pool `"TintEnvelope"` |
| `Drawable::flashAsSelected` (1359) | event | `m_selectionFlashEnvelope->play(color, 0, 4)` (4-frame decay) | GameData `SelectionFlashSaturationFactor`, `SelectionFlashHouseColor` |
| `Drawable::colorFlash(color, decayFrames, attackFrames, sustainAtPeak)` (958) | event | envelope durations in frames | |
| `Drawable::draw` (2636) | DRAW (Get_Frame_Time≠0, on-screen) | `m_secondMaterialPassOpacity *= MATERIAL_PASS_OPACITY_FADE_SCALAR (0.8f)` until `< VERY_TRANSPARENT_MATERIAL_PASS_OPACITY (0.001f)` | |
| `Drawable::applyPhysicsXform` (1388) → `calcPhysicsXform` (1414) | DRAW; returns early if `!TheGlobalData->m_showClientPhysics` or script/camera time frozen | dispatch on `locomotor->getAppearance()`: | GameData `ShowClientPhysics` |
| `calcPhysicsXformThrust` (1470) | DRAW | wobble: `m_pitch/m_yaw += WOBBLE_RATE` (or /2), flip at MIN/MAX; `m_roll += THRUST_ROLL` per call | Locomotor `ThrustRoll`, `ThrustWobbleRate`, `ThrustMinWobble`, `ThrustMaxWobble` |
| `calcPhysicsXformHoverOrWings` (1549) | DRAW | spring-damper per call: `m_pitchRate += -K*p - D*rate; m_pitch += rate*UNIFORM_AXIAL_DAMPING`, accel pitch/roll; `m_yawModulator += RUDDER_CORRECTION_RATE`, `m_pitchModulator += ELEVATOR_CORRECTION_RATE` | `PitchStiffness`, `RollStiffness`, `PitchDamping`, `RollDamping`, `UniformAxialDamping`, `ForwardVelocityPitchFactor`, `LateralVelocityRollFactor`, `ForwardAccelerationPitchFactor`, `LateralAccelerationRollFactor`, `PitchInDirectionOfZVelFactor`, `AccelerationPitchLimit`, `DecelerationPitchLimit`, `RudderCorrectionDegree/Rate`, `ElevatorCorrectionDegree/Rate` |
| `calcPhysicsXformTreads` (1661) | DRAW | same spring-damper; `m_pitchRate *= 0.5f` if >0; overlap "fake Z physics" `m_overlapZVel -= 0.2f; m_overlapZ += vel`; `GameClientRandomValueReal` rough vibration | `LEAVE_OVERLAP_PITCH_KICK = PI/128`, `OVERLAP_ROUGH_VIBRATION_FACTOR 5`, `MAX_ROUGH_VIBRATION 0.5`, `FLATTENED_OBJECT_HEIGHT 0.5`, `OVERLAP_SHRINK_FACTOR 0.8` |
| `calcPhysicsXformWheels` (1919) / `calcPhysicsXformMotorcycle` (2214) | DRAW | suspension spring-damper + `BOUNCE_ANGLE_KICK` impulses, wheel angle | `BounceAmount` (parseAngularVelocityReal), `MaxWheelExtension`, `WheelTurnAngle` … |
Inputs to physics xform are **logic per-frame** quantities (`physics->getVelocity()`, `getAcceleration()` in
dist/logic-frame), so in BFME2 the client integrator also mixes logic-unit inputs with client-rate integration.

Pools/strings: `"Drawable"`, `"DrawableLocoInfo"`, `"DrawableIconInfo"`, `"TintEnvelope"`.
Shroud linger in GameClient::update / W3DScene.cpp:626-632: `2*LOGICFRAMES_PER_SECOND` (+`3*` if dead).

### 7.2 Draw modules — GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/
| module (file:line) | gate | per-call advance | INI |
|---|---|---|---|
| `W3DModelDraw::doDrawModule` (W3DModelDraw.cpp:2040) | DRAW | `setPauseAnimation(!getShouldAnimate())` (MANUAL mode freezes HAnim); transition/idle anim switching on `isAnimationComplete`; `adjustAnimSpeedToMovementSpeed()` (1967: `desiredMs = dist/speed * MSEC_PER_LOGICFRAME_REAL` → `setCurAnimDurationInMsec` → `Set_Animation_Frame_Rate_Multiplier(naturalMs/desiredMs)`); `handleClientTurretPositioning`; `recalcBonesForClientParticleSystems`; `updateBonesForClientParticleSystems`; **`handleClientRecoil`** (2491: RECOIL `m_shift += m_recoilRate; m_recoilRate *= m_recoilDamping`; SETTLE `m_shift -= m_recoilSettle`) | `InitialRecoilSpeed`, `MaxRecoilDistance`, `RecoilDamping`, `RecoilSettleSpeed`, `AnimationSpeedFactorRange`, `AnimationMode`, `Animation`, `IdleAnimation`, `TransitionState`, `ParticlesAttachedToAnimatedBones`, `AnimationsRequirePower`; pool `"W3DModelDraw"` |
| `W3DModelDraw::setAnimationLoopDuration(numFrames)` (3772) | event (logic) | `desiredMs = ceil(numFrames * MSEC_PER_LOGICFRAME_REAL)` | logic frames → ms (BFME2: 200 ms/logic frame) |
| `W3DTankDraw::doDrawModule` (W3DTankDraw.cpp:308) | DRAW | tread UV `customUVOffset.X -= TreadAnimationRate` per call (pivot or straight) | `TreadAnimationRate` (velocity), `TreadPivotSpeedFraction`, `TreadDriveSpeedFraction` |
| `W3DTankTruckDraw::doDrawModule` (505) | DRAW | treads + wheels (same as tank/truck) | |
| `W3DTruckDraw::doDrawModule` (389) | DRAW (skips if time frozen) | `m_curCabRotation += (desired-cur)*RotationDamping`; `m_frontWheelRotation += TireRotationMultiplier*speed` (+`PowerslideRotationAddition`) | `TireRotationMultiplier`, `RotationDamping`, `CabRotationMultiplier`, `TrailerRotationMultiplier` |
| `W3DTracerDraw::doDrawModule` (115) | DRAW | `m_opacity -= m_opacity/(expDate - logicFrame)`; `Translate(m_speedInDistPerFrame)` per call | FXList Tracer `Speed` (velocity/frame), `DecayAt` |
| `W3DRopeDraw::doDrawModule` (180) | DRAW | `m_curWobblePhase += m_wobbleRate; m_curZOffset += m_curSpeed; m_curSpeed += m_accel` | |
| `W3DDebrisDraw::doDrawModule` (213) | DRAW | `++m_frames` (MIN_FINAL_FRAMES 3), HAnim state chain | |
| `W3DLaserDraw::doDrawModule` (252) | DRAW + SYNC | `line->Set_UV_Offset_Rate(Vector2(0, ScrollRate))` (SegLine, SYNC); fade via LaserUpdate (client update module) | `MaxIntensityLifetime`, `FadeLifetime` (durations), `ScrollRate` |
| `W3DTreeDraw` data (W3DTreeDraw.cpp) | — | tree pushes/topples executed in W3DTreeBuffer (§7.9) | `MoveOutwardTime`, `MoveInwardTime`, `SinkTime` (durations), `InitialVelocityPercent`, `InitialAccelPercent`, `BounceVelocityPercent`, `MinimumToppleSpeed`, `SinkDistance` |
Module names are registered as strings via `addModule(classname)` → `AsciiString(#classname)`
(W3DModuleFactory.cpp:62-80): `"W3DDefaultDraw"`, `"W3DDebrisDraw"`, `"W3DModelDraw"`, `"W3DLaserDraw"`,
`"W3DOverlordTankDraw"`, `"W3DOverlordTruckDraw"`, `"W3DOverlordAircraftDraw"`, `"W3DProjectileStreamDraw"`,
`"W3DPoliceCarDraw"`, `"W3DRopeDraw"`, `"W3DScienceModelDraw"`, `"W3DSupplyDraw"`, `"W3DDependencyModelDraw"`,
`"W3DTankDraw"`, `"W3DTruckDraw"`, `"W3DTracerDraw"`, `"W3DTankTruckDraw"`, `"W3DTreeDraw"`, `"W3DPropDraw"`.
`addModuleInternal(proc, dataproc, type, name, mask)` → the proc pointers lead to each module's
`friend_newModuleInstance` → constructor → vtable → `doDrawModule` slot.

### 7.3 Particle systems — GameEngine/Source/GameClient/System/ParticleSys.cpp
- **Driver:** `ParticleSystemManager::update` (2928) is called from **`W3DDisplay::draw`** (inside the render
  do-loop, after `updateViews()`), *not* from GameClient::update (call there is commented out, GameClient.cpp:739).
  ```cpp
  if (m_lastLogicFrameUpdate == TheGameLogic->getFrame()) return;   // LOG-Δ gate
  m_lastLogicFrameUpdate = TheGameLogic->getFrame();
  for sys in m_allParticleSystemList: if (!sys->update(m_localPlayerIndex)) sys->deleteInstance();
  ```
  **[BFME2-hypothesis]** this guard must have become a client-frame guard (5 Hz particles would look awful). Look
  for an early-return compare of a member against a frame counter at the top of the manager update.
- `ParticleSystem::update` (1849): `if (!m_useFX) return false`; `if (m_delayLeft) { --m_delayLeft; ...}`
  (InitialDelay, frames); `updateWindMotion()`; attach transform from drawable/object; emission:
  `if (m_burstDelayLeft == 0) { count = burstCount * m_countCoeff; create...; m_burstDelayLeft = BurstDelay * m_delayCoeff; } else m_burstDelayLeft--;`;
  apply gravity force; `p->update()` each particle; `if (!m_isForever && m_systemLifetimeLeft) m_systemLifetimeLeft--`.
- `Particle::update` (358): `vel += accel; vel *= m_velDamping; pos += vel + driftVel;` wind; `m_angleZ += m_angularRateZ; m_angularRateZ *= m_angularDamping;`
  `m_size += m_sizeRate; m_sizeRate *= m_sizeRateDamping;` alpha `+= m_alphaRate` with keyframes at
  `TheGameClient->getFrame() - m_createTimestamp >= key.frame`; color likewise + `m_colorScale`;
  `if (m_lifetimeLeft && --m_lifetimeLeft == 0) return false;`
  `m_createTimestamp = TheGameClient->getFrame()` (285); system `m_startTimestamp = TheGameClient->getFrame()` (1110).
- **Units:** almost every particle INI value is **raw per-frame** (no ms conversion): parsed with
  `INI::parseGameClientRandomVariable`/`parseUnsignedInt`/`parseReal`. Field table (ParticleSys.cpp:2608-2697):
  `Priority`, `IsOneShot`, `Shader`, `Type`, `ParticleName`, `AngleZ`, `AngularRateZ`, `AngularDamping`,
  `VelocityDamping`, `Gravity`, `SlaveSystem`, `SlavePosOffset`, `PerParticleAttachedSystem`, `Lifetime`,
  `SystemLifetime`, `Size`, `StartSizeRate`, `SizeRate`, `SizeRateDamping`, `Alpha1..Alpha8`, `Color1..Color8`
  (keyframe = value + frame), `ColorScale`, `BurstDelay`, `BurstCount`, `InitialDelay`, `DriftVelocity`,
  `VelocityType`, `VelOrthoX/Y/Z`, `VelSpherical`, `VelHemispherical`, `VelCylindricalRadial`,
  `VelCylindricalNormal`, `VelOutward`, `VelOutwardOther`, `VolumeType`, `VolLineStart`, `VolLineEnd`,
  `VolBoxHalfSize`, `VolSphereRadius`, `VolCylinderRadius`, `VolCylinderLength`, `IsHollow`,
  `IsGroundAligned`, `IsEmitAboveGroundOnly`, `IsParticleUpTowardsEmitter`, `WindMotion`,
  `WindAngleChangeMin/Max`, `WindPingPongStartAngleMin/Max`, `WindPingPongEndAngleMin/Max`.
  → At 60 Hz every one of these (rates, dampings, lifetimes, delays, keyframe frames) doubles in effect unless
  the particle update keeps running at 30 Hz (simplest correct option: tick particles on the 30 Hz client
  clock and render the same state twice / interpolate).
- Pools: `"ParticlePool"`, `"ParticleSystemPool"`, `"ParticleSystemTemplatePool"`; save chunk `"CHUNK_ParticleSystem"`;
  INI block `"ParticleSystem"`; subsystem name `"TheParticleSystemManager"`.
- Dynamic LOD (`GameLOD.cpp:117-122`): `MinimumFPS`, `ParticleSkipMask`, `DebrisSkipMask`, `SlowDeathScale`,
  `MinParticlePriority`, `MinParticleSkipPriority` — chosen from `m_averageFPS` (60 FPS keeps highest LOD).
- Client-update module `AnimatedParticleSysBoneClientUpdate::clientUpdate` (…Update/AnimatedParticleSysBoneClientUpdate.cpp:72): `++m_life` (CLI-Δ).

### 7.4 Camera / tactical view — GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp
`W3DView::update` (1084) — called via `Display::updateViews()` from W3DDisplay::draw, i.e. **LOOP** (runs even
when time is frozen; only drawDrawable is gated).
| item (line) | per-call advance | constants / INI |
|---|---|---|
| camera lock follow (1102-1260) | `followFactor` starts 0.05, `+= 0.05` per call to 1.0; `curpos += d*followFactor`; tether: outside `partitionCellSize²` move `ratio*0.5` per call, inside `0.01*m_lockDist`; airborne follow `m_angle += diff*0.1`; drawable cam-lock `camtran = prev + diff*0.1f` | GameData `PartitionCellSize` |
| scripted moves `updateCameraMovements` (1039) | `zoomCameraOneFrame` (3047), `pitchCameraOneFrame` (3073), `rotateCameraOneFrame` (2975): `curFrame++` vs `numFrames`; `moveAlongWaypointPath(TheW3DFrameLengthInMsec)` (3097): `elapsedTimeMilliseconds += 33` per call | setup: `numFrames = milliseconds / TheW3DFrameLengthInMsec` in `rotateCamera` (2431), `rotateCameraTowardObject` (2456, `numHoldFrames` too), `rotateCameraTowardPosition` (2484), `zoomCamera` (2528), `pitchCamera` (2544); `moveCameraAlongWaypointPath` (2844: `shutter/TheW3DFrameLengthInMsec`); `cameraModFinalPitch` & `cameraModLookToward` convert back `frames*TheW3DFrameLengthInMsec` |
| camera shake (legacy) (1296-1320) "/// @todo Make this framerate-independent" | `m_shakeOffset = I*(cos,sin)`; `m_shakeIntensity *= 0.75f`; angle sign flipped every call; stops < 0.01 | `W3DView::shake` (3248): GameData `ShakeSubtleIntensity`, `ShakeNormalIntensity`, `ShakeStrongIntensity`, `ShakeSevereIntensity`, `ShakeCineExtremeIntensity`, `ShakeCineInsaneIntensity`, `MaxShakeIntensity`, `MaxShakeRange` (FXList `ViewShake` nugget) |
| C&C3 camera shaker `CameraShakerSystem` (camerashakesystem.cpp) | `CameraShakerSystem.Timestep(1.0f/30.0f)` inside **`buildCameraTransform`** (259; call at 416) → `ElapsedTime += dt` per *camera-transform rebuild* (can be >1 per frame: every `setCameraTransform()` from scrollBy/setAngle/setZoom/update) | float `1/30` = 0x3D088889 (**[BFME2]** likely the 0.0333 global); `W3DView::Add_Camera_Shake(pos,radius,duration,power)` (3352) |
| height settle (1333-1366) | `m_zoom += (desiredZoom-m_zoom)*TheGlobalData->m_cameraAdjustSpeed` per call (exponential) — scrolling only if `m_scrollAmount.length() < m_scrollAmountCutoff` or out of bounds | GameData `CameraAdjustSpeed` (0.1), `ScrollAmountCutoff`, `MinCameraHeight`, `MaxCameraHeight`, `EnforceMaxCameraHeight`, `CameraHeight`, `CameraPitch`, `CameraYaw` |
| scrollBy (1803) | applies screen delta × `SCROLL_RESOLUTION 250.0f` | called once per `MSG_FRAME_TICK` (§7.5) |
| `isTimeFast()` | `W3DView::update` returns before drawing | |
| `TheW3DFrameLengthInMsec` (108) | Int ms per frame for all scripted camera conversions | **[BFME2]** expect float 33.333 global or CLIENT_FPS-derived |
View base: `View::zoomIn/zoomOut` (View.cpp:128/133): `setHeightAboveGround(h ∓ 10.0f)` per call.
Save chunk `"CHUNK_TacticalView"`.

### 7.5 Input / scrolling — GameEngine/Source/GameClient/MessageStream/LookAtXlat.cpp
`LookAtTranslator::translateGameMessage`, case **`MSG_FRAME_TICK`** (386-480) — one FRAME_TICK per
`GameClient::update` = **LOOP** (not gated; in Generals MP with unlocked FPS, scroll speed already varied with FPS):
```cpp
SCROLL_RMB:      offset = H/VScrollSpeedFactor * (cur - anchor); offset += H/VFactor * normalize(offset) * KeyboardScrollFactor²;
SCROLL_KEY:      offset.y ∓= VerticalScrollSpeedFactor * SCROLL_AMT(100) * KeyboardScrollFactor; (x likewise)
SCROLL_SCREENEDGE (edge 3 px): same as KEY
TheInGameUI->setScrollAmount(offset); TheTacticalView->scrollBy(&offset);
```
GameData: `HorizontalScrollSpeedFactor`, `VerticalScrollSpeedFactor`, `KeyboardScrollSpeedFactor`,
`KeyboardDefaultScrollSpeedFactor`, `RightMouseAlwaysScrolls`; InGameUI: `DrawRMBScrollAnchor`,
`MoveRMBScrollAnchor`. Also `m_lastMouseMoveFrame + LOGICFRAMES_PER_SECOND < logicFrame` idle check (146);
click vs drag: `TheGameClient->getFrame() - m_timestamp < CLICK_DURATION (5)` (265-274).
Mouse-drag rotate/pitch are delta-based (`FACTOR 0.01f * pixel delta`) → FPS-independent.
Keyboard: `Keyboard::update` `m_inputFrame++` (Keyboard.cpp:736); auto-repeat after `KEY_REPEAT_DELAY = 10`
input frames (222-237) — LOOP counter. Mouse button frames use `TheGameClient->getFrame()` (Win32Mouse.cpp:99).
Mouse double-click/drag use ms (`DragToleranceMS`, `GetDoubleClickTime`) → RT.

### 7.6 InGameUI — GameEngine/Source/GameClient/InGameUI.cpp
`InGameUI::update` (1616) is **LOOP**:
| item | timing |
|---|---|
| UI messages (1656-1686) | uses **logic frame**: `messageTimeout = m_messageDelayMS / LOGICFRAMES_PER_SECOND / 1000` (sic); fade `a -= (logicFrame - timestamp)*0.01` per call (LOOP × logic Δ) |
| military subtitle (1691-1790) | `incrementOnFrame = logicFrame + LOGICFRAMES_PER_SECOND*MilitaryCaptionDelayMS/1000`; typing every `MilitaryCaptionSpeed` logic frames; block blink every 9 logic frames; fade `*0.1` per call; sound `"MilitarySubtitlesTyping"` |
| floating text `updateFloatingText` (5054) | **LOG-Δ** (`static lastLogicFrameUpdate`); `++m_frameCount` (rise: `pos.y -= m_frameCount * m_floatingTextMoveUpSpeed` in drawFloatingText 5106); fade `(logicFrame - m_frameTimeOut) * m_floatingTextMoveVanishRate` |
| world animations `updateAndDrawWorldAnimations` (5347, called from `preDraw` 1595) | expire on logic frame; `m_worldPos.z += m_zRisePerSecond / LOGICFRAMES_PER_SECOND` **per call** (LOOP/DRAW, not gated → already FPS-dependent); fade over `FRAMES_BEFORE_EXPIRE_TO_FADE` logic frames |
| keyboard camera rotate/zoom (1855-1875) | per call: `setAngle(angle ∓ TheGlobalData->m_keyboardCameraRotateSpeed)`; `zoomIn()/zoomOut()` (±10 height) | GameData `KeyboardCameraRotateSpeed` (0.1) |
| control bar `TheControlBar->update()` | flashes on `logicFrame % (LOGICFRAMES_PER_SECOND/2)`, `TheGameClient->getFrame() % 10`, `logicFrame % LOGICFRAMES_PER_SECOND > /2` (ControlBar.cpp:1436,1473,1661) |
| placement legality check | every other client frame `TheGameClient->getFrame() & 1` (1471) |
| move hints `W3DInGameUI::drawMoveHints` (W3DInGameUI.cpp:468) | `elapsed = TheGameClient->getFrame() - hint.frame; if (elapsed <= 40)` show; hint model anim `TheGlobalData->m_moveHintName` ".%s" HAnim `ANIM_MODE_ONCE` (SYNC) | GameData `MoveHintName` |
| superweapon/named timers (postDraw 3450-3831) | logic-frame based; flash every `SuperweaponCountdownFlashDuration`/`NamedTimerCountdownFlashDuration` (parseDurationReal) |
| video in UI | Bink `isFrameReady()` (RT) |
INI (`InGameUI` block) fields: `MessageDelayMS`, `MilitaryCaptionSpeed`, `MilitaryCaptionRandomizeTyping`,
`FloatingTextTimeOut` (duration), `FloatingTextMoveUpSpeed` (velocity), `FloatingTextVanishRate` (velocity),
`SuperweaponCountdownFlashDuration`, `NamedTimerCountdownFlashDuration`, `DrawRMBScrollAnchor`,
`MoveRMBScrollAnchor`, `PopupMessageColor`, many `*RadiusCursor`. Pools: `"FloatingTextData"`,
`"SuperweaponInfo"`, `"NamedTimerInfo"`, `"PopupMessageData"`. GlobalLanguage: `MilitaryCaptionDelayMS`.

### 7.7 Client update modules (GameEngine/Source/GameClient/Drawable/Update/) — CLI-Δ via updateDrawable
- `SwayClientUpdate::clientUpdate` (101): `m_curValue += m_curDelta` per call (wraps 2π), angle from
  `ScriptEngine::getBreezeInfo()` (breeze period in frames).
- `BeaconClientUpdate` (178): radar pulse every `RadarPulseFrequency` (logic frames), duration
  `RadarPulseDuration * SECONDS_PER_LOGICFRAME_REAL`.
- `AnimatedParticleSysBoneClientUpdate` (72): `++m_life`.
- `LaserUpdate` (logic-side module but client-dirty flag consumed by W3DLaserDraw).

### 7.8 Radar — GameEngine/Source/Common/System/Radar.cpp, W3DRadar.cpp
`Radar::update` (299) from GameEngine::update (LOOP) but uses logic frame: events die at
`createFrame + LOGICFRAMES_PER_SECOND*secondsToLive` (1121), fade at `dieFrame - LOGICFRAMES_PER_SECOND*secondsBeforeDieToFade`;
terrain refresh after `RADAR_QUEUE_TERRAIN_REFRESH_DELAY`. `W3DRadar::drawSingleBeaconEvent` (371): pulse size from
`(currentLogicFrame - createFrame)` with `TIME_FROM_FULL_SIZE_TO_SMALL_SIZE = LOGICFRAMES_PER_SECOND*1.5`.
→ Logic-frame based; at 5 Hz logic in BFME2 these step at 5 Hz unless BFME2 switched to client frames.
Save chunk `"CHUNK_Radar"`, subsystem `"TheRadar"`.

### 7.9 Terrain, water, trees, shroud, sky, tracks
| system (file:line) | gate | per-call advance | notes |
|---|---|---|---|
| `WaterRenderObjClass::update` (Water/W3DWater.cpp:1217) via `W3DTerrainVisual::update` (W3DTerrainVisual.cpp:355) | **LOOP** | `m_riverVOrigin += 0.002f`; `m_riverXOffset += (Real)(0.0125*33/5000)` (=8.25e-5, 0x38AD03DA); `m_riverYOffset += 2*that` (0x392D03DA); `m_iBumpFrame++` (wrap NUM_BUMP_FRAMES); water-grid vertex physics (`WATER_DAMPENING 0.93`, gravity*3) only when logic frame changed (LOG-Δ) | `m_riverVOrigin` drives river UV, wave sin offsets everywhere (2319-3253) |
| `WaterRenderObjClass::renderSky` (2050) | RT | `timeDiff = timeGetTime() - m_LastUpdateTime`; `m_uOffset += timeDiff*uScrollPerMs*skyTexelsPerUnit` | Water.ini `UScrollPerMS`, `VScrollPerMS`, `SkyTexelsPerUnit` |
| `WaterTracksRenderSystem` (W3DWaterTracks.cpp:832) | RT | timeGetTime | |
| `W3DShroud::interpolateFogLevels` (W3DShroud.cpp:729) | RT | `maxFogChange = 0.255*ms` | called from `W3DShroud::render` in W3DDisplay::draw |
| `TerrainTracksRenderObjClassSystem::update` (W3DTerrainTracks.cpp:740) | SYNC | fade = 1 - (sync - timeAdded)/m_maxTankTrackFadeDelay | GlobalData `m_maxTankTrackFadeDelay=300000` |
| `TerrainShader2Stage::updateNoise1` (W3DShaderManager.cpp:1612) | SYNC | cloud UV `+= slidePerSecond*Δsync/1000` | `UseCloudMap`; TerrainTex.cpp:981 variant uses `GetTickCount` (RT) |
| `W3DTreeBuffer::drawTrees` (W3DTreeBuffer.cpp:1545) | **DRAW** (per terrain pass; skipped when paused/script-frozen) | sway `m_curSwayOffset[i] += m_curSwayStep[i]` (step = `NUM_SWAY_ENTRIES / breezePeriod`); push-aside `pushAside += pushAsideDelta` (`1/m_framesToMoveOutward`, `-1/m_framesToMoveInward`); toppling `updateTopplingTree` (1864); sinking `m_sinkFramesLeft--`, `z -= SinkDistance/SinkFrames` | `pushAsideTree` uses logic frame `lastFrameUpdated < 3` |
| `W3DTreeDraw` | | `m_sinkFrames = 10*LOGICFRAMES_PER_SECOND` default (W3DTreeDraw.cpp:70) | |
| `W3DSnowManager::update` (W3DSnow.cpp:163) | SYNC | `m_time += Get_Frame_Time()/1000` | |
| `W3DDynamicLight::On_Frame_Update` | DRAW (scene render) | `--m_curIncreaseFrameCount/--m_curDecayFrameCount` | FXList LightPulse durations |
| Screen filters `ScreenBWFilter::postRender` (W3DShaderManager.cpp:331, also DOT3 520) / `ScreenCrossFadeFilter::updateFadeLevel` (715) | DRAW | `m_curFadeFrame++` vs `m_fadeFrames` | `W3DView::setFadeParameters(fadeFrames, dir)`; script `doBlackWhiteMode(frames)` (ScriptActions.cpp:3790) |
| `ScreenMotionBlurFilter::postRender` (951) | LOG-Δ | `m_maxCount ± COUNT_STEP` when logic frame changed | |

### 7.10 GUI / shell / video
- `GameWindowManager::update` (GameWindowManager.cpp:243) → `TheTransitionHandler->update()`
  (`GameWindowTransitionsHandler::update`, GameWindowTransitions.cpp:403) → `TransitionGroup::update` (264):
  **`m_currentFrame += m_directionMultiplier` per call (LOOP)** → each `TransitionWindow::update(frame)`; all
  transition styles (GameWindowTransitionsStyles.cpp, enums in GameWindowTransitions.h e.g.
  `CONTROLBARARROWTRANSITION_BEGIN_FADE = 16`) are frame-indexed. INI block `"WindowTransition"`.
  BFME2's animated menus/palantir/control bar likely use this → 2x speed at 60 FPS.
- `Shell::update` (Shell.cpp:177): `shellUpdateDelay = 30` → runs screen `runUpdate`, `AnimateWindowManager::update`,
  `ShellMenuSchemeManager::update` at most every `1000/30 - 1` ms (RT throttle). Safe.
- `ProcessAnimateWindow*` (ProcessAnimateWindow.cpp): start times via `timeGetTime`; most slide styles move
  `curPos += vel; vel *= m_slowDownRatio` **per update call** (but update is inside the 30 Hz shell throttle);
  `...Timed` variants are fully RT (percentDone from timeGetTime, `m_maxDuration = 1000`).
- Movies: `Display::playLogoMovie(name, minMovieLength, minCopyrightLength)` / `Display::update` (Display.cpp:296)
  hold times via `timeGetTime`; Bink `isFrameReady()` (RT).
- `W3DDisplay` cinematic text: `m_cinematicTextFrames--` per rendered frame; set by script
  `setCinematicTextFrames(LOGICFRAMES_PER_SECOND * timeInSeconds)` (ScriptActions.cpp:2615) → LOOP counter.
- `ScriptEngine::updateFades` (ScriptEngine.cpp:5728): `m_curFadeFrame++` per **logic** update; frames from
  script params (6399-6401). `FRAMES_TO_FADE_IN_AT_START`.

### 7.11 Audio — GameEngineDevice/Source/MilesAudioDevice/MilesAudioManager.cpp
`MilesAudioManager::update` (484) is called from GameEngine::update (**LOOP**):
`AudioManager::update` (listener/zoom volume, GameAudio.cpp:313) → `setDeviceListenerPosition` →
`processRequestList` → `processPlayingList` → **`processFadingList`** (2434: `++playing->m_framesFaded` until
`AudioSettings::m_fadeAudioFrames` (INI `TimeToFadeAudio`, duration→frames)) → `processStoppedList`.
Delayed sounds: `shouldProcessRequestThisFrame` (2505: `getDelay() < MSEC_PER_LOGICFRAME_REAL`) and
`adjustRequest` (2516: `decrementDelay(MSEC_PER_LOGICFRAME_REAL)` **per loop**); looping-sound re-delay (2756).
→ at 60 FPS audio delays expire and fades complete 2x fast (unless BFME2 uses a real-time clock).
INI `AudioSettings`: `TimeToFadeAudio`, `TimeBetweenDrawableSounds`, `MicrophoneDesiredHeightAboveTerrain`,
`MicrophoneMaxPercentageBetweenGroundAndCamera`, `ZoomMinDistance`, `ZoomMaxDistance`, `ZoomSoundVolumePercentageAmount`.

### 7.12 Mouse cursor — W3DMouse.cpp / Mouse.cpp
Animated cursors: `m_currentAnimFrame += (timeGetTime() - m_lastAnimTime) * m_currentFMS` with
`m_currentFMS = cursorInfo.fps/1000` (W3DMouse.cpp:417, 503-512) → RT. INI `MouseCursor` field `"FPS"` (Mouse.cpp:83,
default 20). Tooltip delay `m_stillTime` via timeGetTime (RT). W3D-model cursors use HAnim (SYNC).

### 7.13 Anim2D / EVA
`Anim2D::tryNextFrame` (Anim2D.cpp:441): `if (logicFrame - m_lastUpdateFrame >= m_framesBetweenUpdates)`
(`AnimationDelay` parseDurationUnsignedShort) → logic-frame based. Pools `"Anim2D"`, `"Anim2DTemplate"`.
`Eva::update` uses logic frame (Eva.cpp:295).

---------------------------------------------------------------------------------------------------

## 8. Classification summary (Generals semantics → what a 60 FPS BFME2 must do)

| bucket | systems | 60-FPS action |
|---|---|---|
| RT (wall clock) | shroud fade, sky scroll, water tracks, letterbox, mouse cursor anim, tooltips, shell 30 Hz throttle, timed window anims, movie hold, Bink, net pacing (QPC), net metrics, FPS average | none |
| SYNC (W3D clock) | HAnim playback, all UV mappers, seg-line/laser scroll, W3D particle buffers/emitters, dazzles, sphere/ring objs, tank tracks fade, terrain clouds, snow, move-hint model anim | feed `WW3D::Sync` with real/fractional ms (e.g. `syncTime += 16.667` accumulated in float, or `+= 33` only on 30 Hz ticks). Keep `Get_Frame_Time()!=0` gate in mind (W3DView drawDrawable) |
| LOOP / DRAW per-call integrators | scroll (FRAME_TICK), keyboard rotate/zoom, camera height settle, camera follow, legacy shake (×0.75), shaker Timestep(1/30), scripted camera curFrame++/elapsed+=33, client physics xform, recoil, tread/wheel/rope/tracer, tree sway/push/topple/sink, water `m_riverVOrigin`/bump frame, world-anim z rise, GUI transitions, audio fades/delays, cinematic text frames, screen filter fades, light pulses, material-pass fade (×0.8), UI message/subtitle fades, keyboard repeat | either (a) keep them on a 30 Hz "client tick" and only render extra frames (needs transform interpolation for smoothness), or (b) scale each rate/constant by 30/60 (and damping factors d → √d), or (c) patch the shared client constants (CLIENT_FPS=60, 33.333→16.667, 0.0333→0.01667, 0.03→0.06) *and* fix raw frame counts |
| CLI-Δ (client-frame gated) | Drawable::updateDrawable (fades, flashes, tint envelopes, client update modules incl. sway), particle systems (LOG-Δ in Gen; client-Δ in BFME2?) | same choice as above; these are the main candidates for "run at 30 Hz" |
| LOGIC-frame based | radar events, floating text, Anim2D, superweapon timers, UI message timeouts, drawable expiration, shroud linger, ControlBar flashes, script fades, `setAnimationLoopDuration` | unaffected by render rate (coarse at 5 Hz) |

---------------------------------------------------------------------------------------------------

## 9. Determinism / MP-sync notes

- Logic/client separation is by convention: client code must never call `GameLogicRandomValue*` (client uses
  `GameClientRandomValue*`, e.g. physics xform rough vibration, camera shake angle). A 60-FPS patch that changes
  how often client code runs is sync-safe **only if no client path feeds logic**. Known client→logic read paths:
  `W3DModelDraw` logic bone queries guarded by `isValidTimeToCalcLogicStuff()` (W3DModelDraw.cpp:78:
  `TheGameLogic->isInGameLogicUpdate() || TheGameState->isInLoadGame()`), pristine bone data for projectile
  launch offsets ("DANGER WARNING READ ME" comment at W3DModelDraw.cpp:2575). `W3DTreeBuffer::pushAsideTree`
  is called from logic but only touches client tree state.
- `GameLogic::update` begins with `setFPMode()`; INI parsing also forces it — keep FPU state intact.
- Logic per-frame constants (0.005 / 200 / 5 / 0.2 in BFME2) must stay untouched; they are baked into
  INI-parsed logic values and CRC'd state.
- INI CRC hashes raw GameData.ini lines → don't change FPS via INI for MP; replays: TiVO fast mode & CRC
  intervals are logic-frame based.
- Network: `ConnectionManager::updateRunAhead` uses render FPS (`TheDisplay->getAverageFPS()`) as the player's
  capacity and caps at `m_framesPerSecondLimit` and 30. If BFME2 keeps that, a 60-FPS client could report higher
  capacity — harmless if capped, but the negotiated `m_frameRate` must keep meaning "logic/net frames per second".
- In Generals MP the client (render) loop is unlocked (`m_useFpsLimit=false`) and the freezeTime hack makes all
  CLI-Δ/SYNC systems advance only on logic ticks; LOOP systems (scroll, keyboard rotate, camera settle, water,
  GUI transitions, audio fades) already run at render rate there.

---------------------------------------------------------------------------------------------------

## 10. Anchor catalogue (strings / imports / constants / layout) for locating the BFME2 equivalents

### 10.1 Imports
| import | leads to |
|---|---|
| `timeBeginPeriod` / `timeEndPeriod` (winmm) | `GameEngine::GameEngine` / `~GameEngine` → GameEngine vtable → `execute`, `update` |
| `timeGetTime` | `GameEngine::execute` (limiter), `W3DDisplay::draw` (inner limiter), `GameClient::update` (legal page), `W3DShroud::interpolateFogLevels`, `WaterRenderObjClass::renderSky`, `ConnectionManager::updateRunAhead`, `FrameMetrics`, `Shell::update`, `W3DMouse`, `ProcessAnimateWindow*`, `Display::update/playLogoMovie`, letterbox |
| `Sleep` | execute (`Sleep(0)` in loop; debug `Sleep(1)`), Win32GameEngine::update (`Sleep(5)`), GameClient legal page (`Sleep(100)`) |
| `QueryPerformanceCounter/Frequency` | `W3DDisplay::updateAverageFPS`/`gatherDebugStats`, `Network::init/timeForNewFrame` |
| `IsIconic` | `Win32GameEngine::update`, `W3DDisplay::draw` (first statement) |
| `PeekMessageA`/`GetMessageA`/`TranslateMessage`/`DispatchMessageA` | `Win32GameEngine::serviceWindowsOS` |
| `_controlfp`/`_statusfp`/`_fpreset` | `setFPMode` → `GameLogic::update`, `INI::load` |
| `GetTickCount` | TerrainTex cloud noise (RT) |

### 10.2 Release-surviving strings
- Engine: `"Uncaught Exception in GameEngine::update"`, `"Uncaught Exception during initialization."`,
  `"ERROR:D3DFailurePrompt"`, `"ERROR:D3DFailureMessage"`, `"GameEngineSubsystem"`, `"GameClientSubsystem"`,
  `"Data\\INI\\Default\\GameData.ini"`, `"Data\\INI\\GameData.ini"`, `"Data\\INI\\Default\\Water.ini"`,
  `"Data\\INI\\Water.ini"`, `"Data\\INI\\Default\\Weather.ini"`, `"Data\\INI\\DrawGroupInfo.ini"`,
  `"Menus/BlankWindow.wnd"` (GameEngine::reset), `"lightCRC"` (XferCRC name in init).
- Subsystem names (`initSubsystem(ptr, "TheXxx", …)` / `setName("TheXxx")`, string next to the global store):
  `"TheGameLogic"`, `"TheGameClient"`, `"TheParticleSystemManager"`, `"TheRadar"`, `"TheAudio"`,
  `"TheWritableGlobalData"`, `"TheMessageStream"`, `"TheRecorder"`, `"TheDisplay"`, `"TheInGameUI"`,
  `"TheTerrainVisual"`, `"TheWindowManager"`, `"TheShell"`, `"TheMouse"`, `"TheKeyboard"`,
  `"TheAnim2DCollection"`, `"TheSnowManager"`, `"TheVideoPlayer"`, `"TheEva"`, `"TheDisplayStringManager"`.
- Save-game chunks (`addSnapshotBlock("CHUNK_…", ptr, …)`, GameState.cpp:313-329) → global pointers:
  `"CHUNK_GameLogic"`, `"CHUNK_GameClient"`, `"CHUNK_TacticalView"` (→ TheTacticalView), `"CHUNK_InGameUI"`,
  `"CHUNK_ParticleSystem"`, `"CHUNK_TerrainVisual"`, `"CHUNK_Radar"`, `"CHUNK_ScriptEngine"`, `"CHUNK_Partition"`,
  `"CHUNK_GhostObject"`, `"CHUNK_TerrainLogic"`, `"CHUNK_Players"`.
- Message names (GameMessage::getCommandTypeAsAsciiString, unconditional): `"MSG_FRAME_TICK"`, `"MSG_NEW_GAME"`,
  `"MSG_LOGIC_CRC"`, `"MSG_CLEAR_GAME_DATA"`, `"MSG_RAW_MOUSE_WHEEL"`, `"MSG_META_DEMO_INSTANT_QUIT"` …
- GameClient::update: `"EALogoMovie"`, `"EALogoMovie640"`, `"Sizzle"`, `"Sizzle640"`, `"Menus/LegalPage.wnd"`.
- InGameUI::update: `"ControlBar.wnd:MoneyDisplay"`, `"ControlBar.wnd:PowerWindow"`, `"GUI:ControlBarMoneyDisplay"`,
  `"MilitarySubtitlesTyping"`.
- W3DDisplay::gatherDebugStats (wide): `L"FPS: %.2f, %.2fms draws: %.2f skins: %.2f sort %.2f"`, `L", FPSLock %d"`,
  `L"FPS: %.2f"`, `L"IN: %.2f bytes/sec, %.2f packets/sec"`-style, `L"OUT: %.2f bytes/sec, %.2f packets/sec"`,
  `L"Run Ahead: %d, Net FPS: %d, Packet arrival cushion: %d"`; `"FixedSys"`.
- W3DInGameUI::drawMoveHints: format `"%s.%s"` with `m_moveHintName`.
- W3DLaserDraw: `"LaserUpdate"` (NAMEKEY lookup).
- Memory pools: `"Drawable"`, `"DrawableLocoInfo"`, `"DrawableIconInfo"`, `"TintEnvelope"`, `"ParticlePool"`,
  `"ParticleSystemPool"`, `"ParticleSystemTemplatePool"`, `"W3DModelDraw"` (+ every module class name),
  `"FloatingTextData"`, `"Anim2D"`, `"Anim2DTemplate"`, `"TracerFXNugget"`, `"ViewShakeFXNugget"`,
  `"LightPulseFXNugget"`.
- Debug-only (NOT expected in release): DEBUG_LOG/DEBUG_CRASH strings such as
  `"GameEngine::setFramesPerSecondLimit() - setting max fps…"`, `"Could not do WW3D::Begin_Render()!  Are we ALT-Tabbed out?\n"`,
  `"ConnectionManager::updateRunAhead - …"` (DEBUG_LOGGING only with _DEBUG/_INTERNAL; RELEASE_CRASH survives).

### 10.3 INI field names (parse tables → field offsets → xrefs give the consumers)
- **GameData** (GlobalData.cpp): `UseFPSLimit` (76), `FramesPerSecondLimit` (78), `ShowClientPhysics` (337),
  `HorizontalScrollSpeedFactor` (393), `VerticalScrollSpeedFactor` (394), `ScrollAmountCutoff` (395),
  `CameraAdjustSpeed` (396), `EnforceMaxCameraHeight` (397), `KeyboardScrollSpeedFactor` (398),
  `KeyboardDefaultScrollSpeedFactor` (399), `SelectionFlashSaturationFactor` (435), `SelectionFlashHouseColor`,
  `ShakeSubtleIntensity`…`ShakeCineInsaneIntensity` (440-445), `MaxShakeIntensity` (446), `MaxShakeRange` (447),
  `KeyboardCameraRotateSpeed` (488), `MoveHintName` (74), `CameraPitch`, `CameraYaw`, `CameraHeight`,
  `MaxCameraHeight`, `MinCameraHeight` (179-183), `DefaultOcclusionDelay` (191), `TerrainLODTargetTimeMS` (94),
  `RightMouseAlwaysScrolls` (95), `WaterType` (108), `UseCloudMap` (81), Network* (480-486), `BenchmarkTimer` (495).
  Defaults: `m_keyboardScrollFactor = 0.5`, `m_cameraAdjustSpeed = 0.1`, `m_keyboardCameraRotateSpeed = 0.1`,
  `m_maxShakeIntensity = 10`, `m_maxShakeRange = 150`, `m_useFpsLimit = FALSE`, `m_framesPerSecondLimit = 0`.
- **InGameUI**: see §7.6. **AudioSettings**: see §7.11. **Water / WaterSet**: `UScrollPerMS`, `VScrollPerMS`,
  `SkyTexelsPerUnit`, `WaterRepeatCount`, `TransparentWaterDepth`, `TransparentWaterMinOpacity`.
- **ParticleSystem**: §7.3. **Locomotor** (client physics): §7.1. **W3DModelDraw / W3DTankDraw / W3DTruckDraw /
  W3DTreeDraw / W3DLaserDraw**: §7.2. **MouseCursor**: `FPS`. **Mouse**: `DragTolerance`, `DragTolerance3D`,
  `DragToleranceMS`. **FXList**: LightPulse `IncreaseTime`, `DecreaseTime`; ViewShake `Type`; Tracer `Speed`,
  `DecayAt`, `Length`, `Width`. **DynamicGameLOD**: `MinimumFPS`, `ParticleSkipMask`, `DebrisSkipMask`,
  `SlowDeathScale`, `MinParticlePriority`, `MinParticleSkipPriority`. **Anim2D**: `AnimationDelay`.
  **BeaconClientUpdate**: `RadarPulseFrequency`, `RadarPulseDuration`.
- INI block keywords (INI.cpp theTypeTable): `"GameData"`, `"InGameUI"`, `"ParticleSystem"`, `"FXList"`,
  `"Locomotor"`, `"Mouse"`, `"MouseCursor"`, `"AudioSettings"`, `"WaterSet"`, `"WaterTransparency"`, `"Weather"`,
  `"WindowTransition"`, `"DynamicGameLOD"`, `"StaticGameLOD"`, `"Animation"`, `"DrawGroupInfo"`, `"ShellMenuScheme"`.

### 10.4 Float/int immediates (Generals values; BFME2 may load from the 0xD9F608 block instead)
| value | hex (float) | where |
|---|---|---|
| 1000.0f | 0x447A0000 | execute limiter `1000/m_maxFPS`, many ms conversions |
| 1/30 = 0.0333333 | 0x3D088889 | `CameraShakerSystem.Timestep(1.0f/30.0f)` (W3DView.cpp:416), SECONDS_PER_LOGICFRAME |
| 33.33333 | 0x42055555 | MSEC_PER_LOGICFRAME_REAL (GameClient::init setFrameRate, W3DModelDraw durations, Miles delays) |
| 0.03 | 0x3CF5C28F | LOGICFRAMES_PER_MSEC_REAL (INI duration parse) |
| 33 (int) | 0x21 | TheW3DFrameLengthInMsec initial value (data) |
| 0.002f | 0x3B03126F | `m_riverVOrigin += 0.002f` (water update) |
| 8.25e-5 | 0x38AD03DA | `m_riverXOffset +=` (water) |
| 1.65e-4 | 0x392D03DA | `m_riverYOffset +=` (water) |
| 0.75f | 0x3F400000 | camera shake damping |
| 0.8f | 0x3F4CCCCD | `MATERIAL_PASS_OPACITY_FADE_SCALAR` (Drawable::draw) |
| 0.001f | 0x3A83126F | TintEnvelope epsilon / material pass threshold |
| 0.255f | 0x3E828F5C | FOG_INTERPOLATION_RATE |
| 0.05f | 0x3D4CCCCD | camera-lock followFactor step |
| 0.1f | 0x3DCCCCCD | camera lock smoothing, cameraAdjustSpeed default |
| 0.77f | 0x3F451EB8 | tactical view height fraction (InGameUI::init) |
| 250.0f | 0x437A0000 | `SCROLL_RESOLUTION` (W3DView::scrollBy) |
| 0.5 | 0x3F000000 | updateAverageFPS spike cutoff |
| 0.93f | 0x3F6E147B | water grid dampening |
| 100 (int global) | — | `SCROLL_AMT` (LookAtXlat.cpp:67, writable .data) |
| 40 | — | move hint lifetime (W3DInGameUI) |
| 30 / 29 | — | W3DDisplay inner limiter `minTime` |
| 10 | — | KEY_REPEAT_DELAY; zoomIn/Out step 10.0f |
For 60-Hz equivalents: 16.6667 = 0x41855555, 1/60 = 0x3C888889, 0.06 = 0x3D75C28F, 60.0 = 0x42700000.

### 10.5 Structural hints
- `SubsystemInterface` vtable: `[+0x00] ~dtor, [+0x04] init, [+0x08] postProcessLoad, [+0x0C] reset, [+0x10] update, [+0x14] draw`.
  `GameEngine::update` = sequence of `call [vtbl+0x10]` on TheRadar, TheAudio, TheGameClient, then message stream,
  TheNetwork (if non-null), TheCDManager, then conditional TheGameLogic.
- `GameClient`: `setFrame` and `getFrame` are virtual; `m_frame` is a plain member; `GameClient::update` starts
  with `TheMessageStream->appendMessage(MSG_FRAME_TICK)` + `appendTimestampArgument(getFrame())`.
- `W3DView` = `View` + `SubsystemInterface` (multiple inheritance; `updateView()` → `UPDATE()` → this-adjusted update).
- `WW3D::Sync`: leaf `PreviousSyncTime = SyncTime; SyncTime = arg;`. Find SyncTime, then every reader is a SYNC consumer.
- Static locals to look for in the BFME2 equivalents: `W3DDisplay::draw` `syncTime`, `lastFrame (~0 init)`,
  `prevTime`, `timeMultiplierCounter`; `GameClient::update` `lastFrame (~0)`, `playSizzle`;
  `InGameUI::updateFloatingText` `lastLogicFrameUpdate`; `W3DView::update` `followFactor (-1 init)`;
  `WaterRenderObjClass::update` `lastLogicFrame`; `W3DShroud::interpolateFogLevels` `prevTime`.

---------------------------------------------------------------------------------------------------

## 11. Expected BFME2 divergences to verify (search plan)

1. **Client-frame counter.** Find where BFME2 increments a client frame (likely `GameClient::update` or the
   engine loop) and where logic is triggered every `CLIENT_FPS/LOGIC_FPS` (=6) client frames (look for div/mod
   by the int globals 30 and 5, or an accumulator compared with 200.0/33.333). That code replaces the Generals
   `GameEngine::update` tail `if (... ) TheGameLogic->UPDATE();`.
2. **Interpolation.** Generals has none. With 5 Hz logic BFME2 must interpolate drawable transforms between
   logic frames (fraction = clientFrameWithinLogicFrame / 6, i.e. ×0.1667 or ×33.333/200). Look in
   `Drawable::draw`/`getTransformMatrix`/`W3DModelDraw::doDrawModule` paths for a lerp using these globals —
   that fraction must be recomputed for 12 sub-frames at 60 FPS.
3. **W3D sync feed.** Check whether BFME2's display draw adds a constant (33 / 33.333 global) per client frame
   or uses real time; and whether a fractional-ms/logic-frame-time pair was added to WW3D.
4. **Freeze hack.** Whether `lastFrame == m_frame` gating survived (now on client frame) — if so, rendering more
   frames than client ticks would freeze effects on duplicate frames (useful: keep client tick at 30 Hz).
5. **Particle manager guard** (LOG-Δ in Generals) → client-frame guard.
6. **Limiter(s).** `GameEngine::execute` busy-wait (`1000/m_maxFPS - 1`) and the W3DDisplay inner `minTime=30`
   loop; plus any BFME2 additions. FramesPerSecondLimit drives game speed only because logic is tied to client
   frames — decoupling logic pacing (time accumulator, 200 ms per logic frame) from render is the core change.
7. **Network.** Does BFME2 still pace logic with `Network::timeForNewFrame` (QPC) at `m_frameRate`, and is
   `m_frameRate` in logic or client frames? Check the hard-coded 30 caps in `updateRunAhead` and the
   `FrameMetrics` 30.0 seeds (probably the CLIENT_FPS global in BFME2).
8. **CLIENT_FPS consumers (513 refs).** Classify each xref with §7/§8: INI client-duration parsers (load-time),
   per-frame rates (runtime), frame-count timers (runtime). The float block's client half (33.333, 0.03, 30.0,
   0.0333) xrefs are the concrete patch list.
