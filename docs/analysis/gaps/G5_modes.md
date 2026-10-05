# G5_modes

## Summary

G5 (mode detection and 30/60 switching), checked against game.dat (sha1 0a0613..., byte-identical to <game folder>\rotwk\game.dat). All addresses were verified by disassembly or decompile in this pass.

(1) GameLogic+0x110 mode. All writers: ctor 0x6301C9 (=9), clearGameData 0x7793C5 (=9), setGameMode 0x77948E (store at 0x7794B9), LW handler 0x779CC9/0x779CD6, FUN_0062602B 0x62603C..0x626052, and the save xfer 0x82BC04 (restores the saved mode on load). Values:
- 0 = campaign mission (0x5EAC5F); also SP Living World battles (0x612339) and skirmish-menu starts on a non-MP map.
- 1 = LAN (0x6494C3, plus LW-LAN).
- 2 = skirmish (0x928807), WotR battles that have TheSkirmishGameInfo 0xDE8930, and `-file` with an MP map.
- 3 = replay playback (0x77F7D8).
- 4 = shell map (0x75DEE2).
- 5 = Internet (0x904654, plus LW-online).
- 6 = tutorials Basic/Advanced (0x91BAAA) and the CaH test map (0x9C50D4).
- 7 = Create-a-Hero (0x91A06C).
- 8 = Living World strategic map, SP only (0x6B52BB, 0x626052).
- 9 = no game (initial value / after clearGameData).
GameLogic+0x114 is the LW type: 0 = SP LW, 1 = LW-LAN, 2 = LW-online, 3 = not LW.

(2) TheNetwork 0xDE4468:
- Created only by 0x65E168, which is called from 0x648D11/0x6492D0 (LAN) and 0x9038E5/0x9044F9 (online).
- GameEngine::reset deletes it only when MP and not LW-MP (0x635D64..0x635DA4), so in LW-MP it survives between battles.
- Replays do not create it.

(3) Replays: TheRecorder 0xDE7CD8. Mode is +0x1C (getMode 0x7B0F25): 0 = record, 1 = playback (set at 0x77F698), 2 = none (0x77D7DC). +0xED4 holds the original mode of the replayed game. MSG_NEW_GAME is sent with mode 3 and the replay's FPS, TheGameInfo = recorder+0x24. Playback commands are fed by TheRecorder->vt28 at sub 1 inside GameLogic::update. Fast-forward GD+0xBBD is toggled only in mode 3, in the render phase (0x820571).

(4) Living World strategic map: it runs on the normal GameEngine::update stepper.
- GameLogic+0x125 means "LW map shown / tactical logic suspended". It is set by LW-client setActive (0x6C0069, via vt+0x28 0x49D565) and by loading save types 4/6 (0x6DED03). It is NOT a "save is loading" flag (that was wrong in earlier notes).
- While it is set, GameLogic::update only dispatches commands at sub 1 and reports success (0x62E5AC..0x62E5F6).
- TheLivingWorldLogic::update 0x6BE50E runs once per sub-1 attempt (0x632A85..0x632A92).
- Per-render LW work: LW camera 0x8392A7, LW client vt6C at 0x6484D9, LW draw vt20 at 0x449F45.

(5) Corrections to the earlier synthesis:
- GameEngine+0x34 (s) is never reset by GameEngine::reset 0x635D11. Its only writers are the stepper and init 0x63CF0C (s=0).
- Loads usually happen in the render phase (0x818456, 0x6E047A), i.e. mid-tick, and s then continues exactly as in stock.
- Failed tick attempts (frozen camera time or pause) never touch GameLogic state. But TheLivingWorldLogic::update still runs on every non-paused sub-1 attempt, so the "restore to 11" stub (30 attempts per second) is still right.

(6) Every game teardown (new game, restart, quit to shell, load, LW battle start/end, replay start) goes through GameEngine::reset: Win32GameEngine vtable slot 0xBD8504 -> thunk 0x44181A -> 0x635D11. That thunk is the reliable hook point for forcing 30 mode.

(7) delayfix.dat (the launcher's "PvP mode" file) changes the stepper itself (0x632535 and 0x632A9B: two GameLogic sub-steps in one frame), so 60 mode must refuse to run on it.

(8) Command line. The launcher (a PyInstaller/PyQt6 app; launch_game was read in memory) starts rotwk\lotrbfme2ep1.exe with: `-mod <aotr dir>`, `-win` or `-fullscreen`, `-xpos 0 -ypos 0`, `-xres W -yres H`, and optionally `-scriptdebug2`.
- `-scriptdebug2` together with `-win` loads DebugWindowLite.dll (0x6056E5). That DLL is present in the rotwk folder, and it turns on the script-debugger halt (0x603452), debug freeze (0x60342F) and RunAppFast (0x603491) paths.
- `-noshellmap` (GD+0xAF0=0 at 0x7B9ED0) and `-file` / `-resumeGame` change the startup path.
- RotWK has no `-quickstart` option.

Recommended controller: a hook at 0x6325CF that runs every frame before clientUpdate, plus the reset hook at 0x44181A.
- Switch to 60 only when s==1, TheGameClient+0xC8==1 (the previous frame ticked successfully) and the safety check passes. At that point the A/B frame pair is complete, so no conversion is needed.
- Switch to 30 at any frame where the check fails, and always in the reset hook, converting s30 = (s60+1)>>1.
- Switch these immediate bytes:
  - 0x63264C: 06 / 0C (required)
  - 0x632606: 06 / 0C (has no effect in single-player; switch it for symmetry)
  - 0x44B98E: E2 / F1
  - 0x44B9C3: 1D / 0E

  Also recompute GameEngine+0x3C (the interpolation fraction) and clear the DLL's own state (pending sync, B-frame flag, limiter toggle, physics cache).
- First release: allow only modes {0,2,6}. Keep 30 FPS in replays, the shell, Create-a-Hero, MP, the LW strategic map, and while the camera time multiplier is above 1.

## Design notes

RECOMMENDED SAFETY CHECK (Safe60, read-only, run once per frame inside the hook at 0x6325CF before clientUpdate):
- User has 60 FPS enabled.
- The binary passed the original-byte check and is not delayfix.dat (the 0x632535 / 0x632A9B signatures).
- The pointers [0xDE4324], [0xDE412C], [0xDE4388], [0xDE4364] and [0xDE447C] are non-NULL.
- [0xDE4468] == NULL (TheNetwork).
- mode = [GL+0x110] is in {0, 2, 6}.
- [GL+0x114] is not 1 and not 2.
- byte [GL+0x9D] == 0.
- byte [GL+0x125] == 0 (not on the Living World map).
- Recorder [0xDE7CD8] is NULL, or [R+0x1C] != 1 (no replay playback).
- byte [GD+0xBBD] == 0 (no fast-forward).
- byte [GD+0xD45] == 0 (not Create-a-Hero).
- byte [GD+0xAF2] == 0 and byte [GD+0xAF3] == 0 (not in the intro).
- byte [GD+0x26] != 0 (UseFPSLimit on).
- [0xDE3B98] == NULL (no script-debugger DLL).
- int [TV+0x23D4] <= 1 (camera time multiplier).
- Optional: [TV+0x23D0] == 0, TV->vt78() != 0, letterbox [[0xDE4418]+0xD8] == 0. Use these if the scripted-camera fixes are not in v1.
- Optional, cosmetic: [GL+0x9C] == 0 and [GL+0xA8] == 0.

SWITCHING RULES:
- Turn ON only before a render, when s == 1 and [GC+0xC8] == 1. That means the previous frame ticked successfully, and in 60 mode the last render was a B-render, so the A/B pair is complete. Set the fraction to 1/12 and write the 60-mode bytes from G5-15. No s conversion is needed.
- Turn OFF on any frame where Safe60 fails, and always in the ResetHook at thunk 0x44181A:
  - s = (s+1) >> 1 (11 becomes 6, 12 becomes 6, 1 stays 1);
  - fraction = clamp(s/6) via 0x63256F;
  - write the stock bytes;
  - zero C3 pending only after it has been flushed. SyncStub must always subtract pending when d > 0, whatever the mode.
  - clear g_lastStepB and the limiter toggle;
  - flush split-step integrators;
  - invalidate the C5 and interpolation caches.
- Turning ON at any s with s60 = 2*s30 would also preserve logic, but it is not recommended.
- Where transitions are caught: logic-phase changes (scripts, MSG_NEW_GAME, MSG_CLEAR_GAME_DATA 0x1D, LW messages, freeze, setFPS) all happen inside GameLogic::update(sub 1) or the LW update, so the next pre-render check sees them at s == 1. Render-phase changes (UI loads and restarts, replay start, camera time multiplier, BBD, TheNetwork creation) are caught by the reset hook or by the next pre-render check with conversion. An optional post-render check makes those immediate.

CONTRADICTIONS WITH THE SYNTHESIS / CORE NOTES:
1. GameLogic+0x125 is not "a save game is loading".
   - It is "Living World strategic map shown, tactical logic suspended". It is set at 0x6C0069 from the LW client's setActive, and at 0x6DED03 after loading save types 4/6.
   - It is cleared by GameLogic init (0x62D194) and reset (0x62D285).
   - Likewise, FUN_0065C1EA is Display::update (video only), used while the LW map is shown. It is not a loading routine.
2. Synthesis 2.3 says "GameEngine reset 0x635D11, where s is reset". That is wrong: 0x635D11 never writes +0x34. The only non-stepper writer is 0x63CF0C (init, s=0). Loads happen mid-tick and s carries on.
3. Section 1.3 says "the number of failed tick attempts during frozen time affects logic". Partly wrong:
   - Failed sub-1 attempts during camera freeze or pause never touch GameLogic state. Pause skips the GameLogic call entirely. Freeze returns at 0x62E5EF after a pure containsMessageOfType(0x1D) query.
   - The "full sub-1 path without frame++" is the !runnable && !frozen case, and that counts as a successful tick (GC+0xC8 = 1 at 0x62EE3E).
   - Still keep RestoreStub = 11: step(1) calls TheLivingWorldLogic::update on every non-paused attempt (0x632A85..0x632A92). During LW battles that only runs TheShell::update; on the LW map it runs full LW logic.
4. Core's FUN_00441B60 "modes 7, 9" now have meanings: 7 = Create-a-Hero, 9 = no game (initial / after clearGameData). Mode 6 is tutorials and the CaH test map, not GAME_NONE. Mode 8 is the SP Living World strategic map.
5. The synthesis predicate uses a blacklist ("!= 1, 5, optional != 4"). It should be a whitelist: as written it lets 60 mode run in replays, Create-a-Hero, the LW map and mode 9.
6. Byte 0x632606 (the +0x38 refresh, C1a) has no effect in SP. Only 0x63264C is essential in the stepper.

OTHER NOTES:
- The launcher's PvP mode installs delayfix.dat, which changes the stepper. Refuse to run on it.
- DXVK is installed (rotwk\d3d9.dll and dxvk.conf; the launcher has a DXVK toggle). Vsync / presentInterval behaviour (synthesis R5) therefore depends on dxvk.conf (d3d9.presentInterval, d3d9.maxFrameRate) and needs measuring under DXVK.
- AotR's extra sections only read TheGameLogic+0x40 (0xECA025) and TheLivingWorldLogic (0xED0989). There are no writes to mode or pause flags, and no overlap with the controller hooks (0x6325CF, 0x44181A).
- Hysteresis: require the check to pass for at least one full tick before turning 60 on again. This stops 30/60 flapping when a camera path's time multiplier hovers around 1, or at cinematic boundaries.

## Items

### G5-01 — mode detection / GameLogic mode  [n/a, high]
- **Site:** [[0xDE412C]+0x110] (int). Writers: 0x6301C9 (W3DGameLogic ctor 0x630023, =9), 0x7793C5 (clearGameData 0x7792BC, =9), 0x7794B9 (setGameMode FUN_0077948E, from MSG_NEW_GAME arg0 at 0x779DE5 or direct 1/5/8 from 0x6B52BB/0x6B533A), 0x779CC9/0x779CD6 (LW-MP path in logicMessageDispatcher), 0x62603C/0x626047/0x626052 (FUN_0062602B: +0x114 0->8, 1->1, 2->5), 0x82BC04 (GameStateMap xfer: restore saved mode)
- **What:** Game mode enum. MSG_NEW_GAME (0x1E) senders and mode arg: 0x5EAC5F campaign mission (0; EBX=0 at 0x5EABAD); 0x612339 LW battle (1/5 if LW-MP, 2 if TheSkirmishGameInfo 0xDE8930, else 0; asm 0x6123E4..0x61241E); 0x63CA19 startup -file (2 if map metadata +0x24 multiplayer else 0; .rep -> replay at 0x63CBD6); 0x6494C3 LAN start (1); 0x75DEE2 Shell::showShellMap (4); 0x77F7D8 replay playbackFile (3); 0x904654 online start (5); 0x91A06C Create-a-Hero map (7); 0x91BAAA tutorial Basic/Advanced (6); 0x922246 restart (same mode as before, saved at 0x9220DE); 0x928807 skirmish menu (2 if map multiplayer-flagged else 0); 0x9C50D4 CaH test map (6). Value 8 is only set directly (0x6B52BB FUN_006B5286 = LW SP strategic map, followed by startNewGame(0)). Helpers: isInMultiplayerGame 0x441B7C (1|5), isInInteractiveGame 0x441B60 (not 4,7,9), isInShellGame 0x484A85 (4|7), isSinglePlayerish 0x6253BF (0|6 or replay of 0|6), 0x62541E (MP incl. replays of 1/5), 0x625456 (MP or skirmish incl. replays).
- **Cadence:** event: set in logic phase (MSG_NEW_GAME processed in GameLogic::update sub 1 -> 0x779DE5; LW messages at sub 1) or by clearGameData (render or logic phase); read any time
- **Reason:** Detection predicate only. Whitelist recommended: allow 60 only for 0 (campaign incl. SP LW battles), 2 (skirmish incl. WotR battles), 6 (tutorial); exclude 1,5 (MP), 3 (replay), 4 (shell), 7 (CaH), 8 (LW strategic map, until its per-render systems are audited), 9 (no game/intro/menus without shell map).
- **Risk if wrong:** Blacklisting (only !=1,!=5,!=4) would enable 60 in replays (mid-tick fast-forward toggles), Create-a-Hero (GD+0xD45 code paths) and the LW map (unaudited per-render LW camera/client update -> 2x LW camera/animation).

### G5-02 — mode detection / network  [n/a, high]
- **Site:** [0xDE4468] TheNetwork; creator 0x65E168 (stores at 0x65E1B0) called from 0x648D11 (FUN_00648CF1 LAN), 0x6492D0 (FUN_006492B0 LAN game start, also sets GL+0x9D=1 at 0x6494D6), 0x9038E5 (FUN_00903887 online), 0x9044F9 (FUN_00904498 online start, GL+0x9D=1 at 0x904687); cleared 0x635DA4 (GameEngine::reset, only if 0x441B7C && !0x610A21), 0x779F0A, 0x63C922, 0x63D0C5, 0x648E1E, 0x6493DD/0x649545, 0x649989, 0x90372A/0x90381E, 0x903994, 0x9045A8/0x9047A4
- **What:** Non-NULL only for LAN/online sessions (including LW-MP, where GameEngine::reset keeps it between battles because FUN_00610A21 = GL+0x114 in {1,2}). Replays never create it (playbackFile 0x77F66B has no network creation). Used by step(1) 0x6329D9..0x632A5D, MP stall 0x63239D, limiter MP branch 0x63A03B..0x63A169.
- **Cadence:** event (render phase: lobby/menu callbacks; only reachable from shell/LW menus)
- **Reason:** Primary MP predicate: g_m60 requires [0xDE4468]==NULL. Also check GL+0x9D==0 (MP start wait, 0x625130) and GL+0x114 not in {1,2} (0x610A21) as cheap redundancy.
- **Risk if wrong:** 60 mode in MP would desync timing-dependent MP code that reads s (0x63239D reads +0x34, 0x63A06E 'cmp [esi+0x34],1') and change the network frame cadence.

### G5-03 — mode detection / replay  [n/a, high]
- **Site:** TheRecorder [0xDE7CD8]: mode +0x1C (getMode 0x7B0F25): 0=RECORD (startRecording 0x77EA03 sets 0 at 0x77EA20), 1=PLAYBACK (0x77F698 in playbackFile 0x77F66B), 2=NONE (0x77D7DC). +0xED4 = original game mode of the replay (read from file). Playback entry points: replay menu AptSaveLoad 0x81848C, restart-of-replay 0x9220DE, startup '-file x.rep' 0x63CBD6
- **What:** playbackFile: clearGameData (unless +0xED0), Recorder+0x1C=1, TheGameInfo 0xDE892C = recorder+0x24, MSG_NEW_GAME(3, difficulty, x, [fps]) - the 4th arg sets GameEngine+0xC (FPS limit) and forces UseFPSLimit at 0x779DB2..0x779DD7. No TheNetwork. Commands injected by TheRecorder->vt28 at sub 1 inside GameLogic::update (after 0x62E8A8). Fast-forward GD+0xBBD toggled only when mode==3 by CommandXlat meta message 0x9C, write at 0x820571 (render phase, any s).
- **Cadence:** event; fast-forward toggle in render phase (mid-tick)
- **Reason:** Replays are logic-deterministic, but v1 should exclude them (mode 3 or Recorder+0x1C==1) because GD+0xBBD flips mid-tick and switches W3DDisplay::draw into the render-skip branch 0x44B8A2..0x44B8EF and the limiter off (0x63A02D). Can be enabled later with immediate off on BBD.
- **Risk if wrong:** Fast-forward under 60 mode runs half as fast for up to one tick and B-renders take the non-FF path (cosmetic); a missed BBD check would keep the halving limiter active during FF.

### G5-04 — mode detection / shell, intro, menus  [n/a, high]
- **Site:** GD = [0xDE4364]: +0xAF0 shellMapOn (ctor 0x64308E =1; cleared by -noshellmap handler 0x7B9ED0 and -file 0x63C9B0/0x63CBFC), +0xAF1 (=1 by -noshellmap), +0xAF2 playIntro (ctor 0x64309B =1), +0xAF3 afterIntro; Shell::showShellMap FUN_0075DE01 (MSG 0x1D at 0x75DF1A if mode!=4/9 then MSG_NEW_GAME(4) at 0x75DEE2, map from GD+0xAEC); GameClient::update intro branch 0x648620/0x64862D
- **What:** Main menu with shell map = mode 4, a real GameLogic game driven by the same stepper (scripts, camera paths). With -noshellmap, menus run with mode 9 (no game). During intro movies (GD+0xAF2 || GD+0xAF3) GameClient::update only calls TheDisplay draw/update (0x648620..0x648640 branch) while the stepper still runs on mode 9 (not runnable -> sub-1 path without frame++).
- **Cadence:** per render (shell UI, TheShell::update with real-time throttle 0x75E1D3), logic per tick (shell map)
- **Reason:** Exclude mode 4 and 9 in v1; shell per-render systems (window transitions 0x5DB60A, Shell update, APT callbacks, audio shell dt) are not audited. Mode 9 also covers intro and menus without shell map.
- **Risk if wrong:** Menu animations / transitions / shell audio fades could run 2x; credits screen sets FPS 100 (0x91B6DC) and restores GD+0x28 at 0x91B7AA.

### G5-05 — loading screens  [n/a, high]
- **Site:** GL=[0xDE412C]: +0xA8 map-loading (set 0x62A1E8 in FUN_0062A11A, cleared 0x62F9D3 in FUN_0062F91A, both inside startNewGame 0x6314CD); +0x9C post-load transition (set 0x63150C in startNewGame, cleared 0x62B42F in FUN_0062B385); +0x44 started (0x6315D6); runnable = FUN_00625130 (+0x44 && !+0xA8 && !+0x9D); load screen FUN_00758084 (timeGetTime/Sleep loop)
- **What:** Map loads run entirely inside one frame: MSG_NEW_GAME is processed in GameLogic::update(sub 1) (0x779DE5 setGameMode, 0x779DEE startNewGame) or in the render phase via loadGame (0x6DF4D1). The load screen pumps its own real-time loop; the stepper does not run. After return, FUN_0062B385 (called from clientUpdate 0x63241E and GameClient::update) performs the fade-in with blocking Sleep(1/5) loops while GL+0x9C.
- **Cadence:** event (blocking, real time)
- **Reason:** No per-load switching needed: every load path is preceded by GameEngine::reset (G5-12), so the reset hook forces 30 mode before the load starts. Optional: require GL+0x9C==0 and GL+0xA8==0 before re-enabling 60 (cosmetic: avoids enabling during the fade frame).
- **Risk if wrong:** If 60 were active during load-screen draws, C3/C7 stubs would run with stale A/B state (visual only).

### G5-06 — Living World strategic map  [n/a, high]
- **Site:** Mode 8 set at 0x6B52BB (FUN_006B5286 via FUN_006BE09E, MSG 0x1F handler in logicMessageDispatcher) and 0x626052 (FUN_0062602B after a battle); GL+0x125 set at 0x6C0069 (LW client setActive 0x6BFF7B, reached through W3D LW client vtable 0xBDE918 slot +0x28 = 0x49D565) and 0x6DED03 (loadGame tail for save types 4/6); cleared at GameLogic init/reset 0x62D194/0x62D285; LW client [0xDE4958] +0x18 active, +0x19 suspended (set via vt+0x50 0x49BE23); TheLivingWorldLogic [0xDE4950] (vtable 0xC14574) +0xB4 active, +0xB5 running (FUN_00441E4A; +0xB5=0 when a battle starts 0x6BF133, =1 after 0x6BF207)
- **What:** The LW map runs on the normal main loop + stepper. With GL+0x125 set: GameLogic::update only calls FUN_005FF9D3 every sub and at sub 1 dispatches the command list (FUN_00625804) and sets GC+0xC8=1 (0x62E5AC..0x62E5D1), subs 2..6 do nothing else. step(1) then calls TheLivingWorldLogic::update 0x6BE50E (0x632A8A..0x632A92) which runs LW logic (+0x100 LW frame++) and TheShell->vt28. Render side per render: LW camera 0x8392A7 (GameClient::update, gated on LW active&&running && !paused), LW client update vt6C 0x49AAB8 at 0x6484D9 + FUN_00645750, display video update 0x65C1EA instead of terrain/W3DDisplay update (0x648821), drawFrame skips updateViews (0x449D29) and draws the LW scene via vt20 0x49B618 at 0x449F45 instead of RenderViews; particles gated (0x5F51AD); FF branch disabled (0x44B8B1).
- **Cadence:** LW logic: per tick (A-frame s=1); LW camera/client update/draw: per render
- **Reason:** v1: disable 60 when mode==8 or byte[GL+0x125]!=0 (strategic map stays stock 30). v2 (LW map at 60): LW logic needs nothing (tick cadence preserved by model H); LW draw vt20 run on B as is; LW camera 0x8392A7/0x49B799 needs halving (run_on_B_with_fix, cua INP-5); LW client update vt6C 0x49AAB8 skip_on_B until audited.
- **Risk if wrong:** Enabling 60 on the LW map without fixes gives 2x LW camera scroll/zoom and possibly 2x LW army/marker animations from vt6C.
- **Fix @ 0x6484D9:** v2 only: gate with g_isARender||!g_m60 (skip on B-renders) until audited (orig: ff 50 6c (call [eax+0x6C], LW client update))

### G5-07 — Living World battles  [n/a, high]
- **Site:** FUN_00612274 (LW battle start, called from LW message handler 0x6BE6A6 at 0x6BF12E): clearGameData then MSG_NEW_GAME at 0x612339 with mode chosen at 0x6123E4..0x61241E; return to map: FUN_0062602B (0x62603C..0x626052)
- **What:** SP LW campaign battles run as mode 0, WotR battles started from the skirmish menu (TheSkirmishGameInfo 0xDE8930 present) as mode 2, LW-MP as 1/5. During a battle TheLivingWorldLogic stays active (+0xB4) but not running (+0xB5=0), so its per-sub-1-attempt update only calls TheShell->vt28 (real-time throttled UI). Battle end returns to mode 8 / GL+0x125=1 in the logic phase (sub 1).
- **Cadence:** event (logic phase sub 1)
- **Reason:** LW battles are normal games: allowed (mode 0/2 with GL+0x114==0). The transition into a battle passes clearGameData -> GameEngine::reset (reset hook = off); return to the map is caught by the pre-render check at s==1 (mode 8 / GL+0x125).
- **Risk if wrong:** If LW-map exclusion relied only on mode 8, a save loaded with GL+0x125 set (type 4/6, 0x6DED03) could leave 60 on; check both mode and GL+0x125.

### G5-08 — pause / Esc menu  [n/a, high]
- **Site:** GL+0x124 isGamePaused (FUN_0090F92C); setter setGamePaused 0x625AF1 (ignored when 0x441B7C MP or 0x610A21 LW-MP; pauses audio); callers incl. Esc/options FUN_008E8843 (pauses unless MP or mode 6, check at 0x8E8901), QuitMenu 0x921904/0x921B0F (unpause on close), SaveLoad 0x8182CF/0x81874E, 0x822BE3/0x8232ED, Display::playMovie 0x65D3F5, exit 0x625E36, campaign/LW start 0x5EAB8F/0x612274; step(1) check 0x6329CD..0x6329E5, skip at 0x632A74 -> GC+0xC8=0 at 0x632AFC
- **What:** When paused, step(1) calls neither GameLogic::update nor TheLivingWorldLogic::update; the stepper restores s (stock 7) and every render has GC+0xC8==0. Tutorials (mode 6) do not pause on Esc. Movies (playMovie 0x65D3F5) pause and run their own blocking loop.
- **Cadence:** event (render phase UI)
- **Reason:** No mode switch needed: pause is logic-free in both modes. In 60 mode RestoreStub (s=11) gives B-frame / failed attempt alternation; per-render UI gates must use the paused toggle (g_uiTick alternates) because GC+0xC8 stays 0. The paused-camera inner loop in W3DDisplay::draw needs C6 bytes (G5-15).
- **Risk if wrong:** Switching on pause would cause needless 30/60 flapping on every Esc; gating UI only on GC+0xC8 would freeze allowed UI animations while paused.

### G5-09 — cinematics / frozen time  [n/a, high]
- **Site:** GameLogic::update sub 1: 0x62E520..0x62E554 frozen = (TacticalView vtD8 0x48B5D3 = W3DView+0x23D0 && !vt78 0x486352 isCameraMovementFinished) || FUN_0060342F (debug freeze); runnable FUN_00625130 at 0x62E56A; frame++ only at 0x62E577; frozen path: TheCommandList(0xDE639C)->vt44(0x1D) = containsMessageOfType 0x710E2D (pure query); none -> GC+0xC8=0 at 0x62E5EF and return; W3DView+0x23D0 set 0x48B5CB (CAMERA_MOD_FREEZE_TIME), cleared 0x48A6C3/0x4868C5/0x4868F6/0x48986C/0x4898B3/0x48A452
- **What:** Camera freeze-time: logic does not run (failed attempts are GameLogic-neutral; only a pending MSG_CLEAR_GAME_DATA lets sub 1 proceed). Correction to earlier synthesis: the 'full sub-1 path without frame++' happens when !runnable && !frozen (game not started / loading / MP wait), and that path ends with GC+0xC8=1 (0x62EE3E) i.e. counts as a successful tick. However step(1) still calls TheLivingWorldLogic::update after GameLogic::update(1) on every non-paused attempt (0x632A85..0x632A92), so attempts per second must stay stock (30/s) - keep RestoreStub=11.
- **Cadence:** per render while frozen (failed tick attempts); camera moves per render
- **Reason:** No switch required for logic identity. Camera moves during frozen time are per-render presentation (allow-list, need row-55 fixes). Safe-default option if camera fixes are not ready: predicate !byte[TV+0x23D0] && TV->vt78()!=0 (no scripted camera move) -> off at the s==1 pre-render check (freeze is set by scripts at sub 1).
- **Risk if wrong:** With 13 instead of 11, LW update/Shell update and debug polling run 60/s during freeze; without camera fixes, cinematic camera moves run 2x and desync from real-time audio/subtitles.

### G5-10 — script FREEZE_TIME  [n/a, high]
- **Site:** ScriptEngine [0xDE3BAC]+0x1A5D4 freezeByScript: set 0x60341F (from FREEZE_TIME action 0x7BC991), cleared 0x603427 (0x7BC99C), 0x60806B, 0x60984A; read FUN_00603418 by 0x449CF8, 0x48BCF2, 0x4CBFFB, 0x4CDBD7, 0x4E837E, 0x4ED508, 0x64849E, 0x67BE90
- **What:** Client-visual freeze only (drawables, view update, trees, treads, physics xform, drawFrame skip); NOT read by GameLogic::update or step(), so logic keeps ticking.
- **Cadence:** event (logic phase sub 1, script actions)
- **Reason:** No switch needed; frozen visuals are identical in both modes.
- **Risk if wrong:** None for logic; assuming it freezes logic would mislead the frozen-time analysis.

### G5-11 — camera time multiplier  [n/a, high]
- **Site:** TheTacticalView [0xDE447C] (W3DView vtable 0xBDD490): vtDC 0x48B5DA returns +0x23D4 time multiplier; setter 0x48B5E1 (SET_VISUAL_SPEED_MULTIPLIER), camera-path lerp 0x48A69A (FUN_0048A417 called from updateCameraMovements 0x48A953, render phase), 0x486232, 0x486BBC, reset 0x48B2B4; consumers: execute limiter 0x63A001..0x63A00A (limiter off if >1), W3DDisplay::draw render skip 0x44B959..0x44B98A (counter 0xD98CB4)
- **What:** When >1 the game renders 1 of N draws and the outer limiter is disabled, i.e. a fast-forward driven by camera paths/scripts. Value can change on any render (render phase, mid-tick).
- **Cadence:** per render (camera path) or event (script, logic phase)
- **Reason:** Predicate: *(int*)([0xDE447C]+0x23D4) <= 1 (direct field read; TV may be NULL early). Off on the next pre-render check with s conversion; back on at the next successful tick.
- **Risk if wrong:** Staying in 60 mode while >1: stepper needs 12 frames per tick so the fast-forward runs at half speed for that period, and the halving limiter/sync stubs interact with the render-skip branch (cosmetic, no logic change).

### G5-12 — game teardown (new game, restart, exit to shell, load)  [n/a, high]
- **Site:** GameEngine::reset 0x635D11 reached via Win32GameEngine vtable 0xBD84E0 slot +0x24 (0xBD8504 = 0x44181A) -> thunk 0x44181A 'e9 f2 44 1f 00' (jmp 0x635D11). Callers through the vtable: clearGameData 0x7793C2, loadGame 0x6DEB9A, AptSaveLoad 0x8184AA, 0x91C6C4, 0x401B5E. clearGameData 0x7792BC callers include restart 0x9220DE, campaign 0x5EAB8F, skirmish 0x9286D7, LW battle 0x612274, tutorials 0x91B825, CaH 0x9C4FEA, replay 0x77F66B, loadGame 0x6DEAxx/0x6DF53B, MSG_CLEAR_GAME_DATA (0x1D) handler
- **What:** Every teardown/new-session path calls GameEngine::reset: resets +0x38=6, +0x40, +0x44=6.0, +0x48, +0x4C..+0x5C (0x635DC7..0x635E10) and deletes TheNetwork if MP (not LW-MP). It does NOT touch +0x34 (s). Only non-stepper writer of s is GameEngine::init tail 0x63CF0C (s=0); stepper writers 0x632625, 0x6326BB, 0x6326E1, 0x632AC0. Immediately after reset clearGameData sets mode 9 (0x7793C5).
- **Cadence:** event: render phase (UI: restart, load, start buttons) or logic phase at sub 1 (MSG_CLEAR_GAME_DATA queued by exit 0x625E36 / shell 0x75DF1A); in the logic-phase case s==1
- **Reason:** Best single hook for 'force 30 mode': ResetHook does off-with-conversion (s=(s+1)>>1), restores bytes, flushes C3 pending, clears B-frame flag/limiter toggle, invalidates C5 physics cache and interpolation caches (drawable addresses and m_frame values repeat after load), then jmp 0x635D11.
- **Risk if wrong:** Without it, a load in the render phase can complete without the per-frame check ever seeing mode 9, leaving stale DLL caches (C5 cache hit on a reused loco pointer with a restored m_frame).
- **Fix @ 0x44181A:** e9 <ResetHook>; ResetHook(ECX=engine): if g_m60 -> Off60(convert); InvalidateCaches(); jmp 0x635D11 (tail jump, ECX and stack untouched) (orig: e9 f2 44 1f 00 (jmp 0x635D11))

### G5-13 — save / load  [n/a, high]
- **Site:** loadGame 0x6DF4D1 (-> 0x6DEA25, tail 0x6DEC46). Callers: AptSaveLoad close 0x818456 (render phase), 0x6E047A (FUN_006E03F3 from UI 0x91C2FC, render phase), logicMessageDispatcher 0x779C9F (LW-MP load, sub 1), GameEngine::execute 0x639E2A (-resumeGame, before main loop). Save type +0x48 in {1,4,6} -> clearGameData first; GameStateMap xfer 0x82B8E4 restores mode (0x82BC04) and calls startNewGame(1) unless mode 9 / type 4/6; type 4/6 -> GL+0x125=1 (0x6DED03)
- **What:** Saves happen from the APT save menu while paused (render phase); nothing stepper-related is saved (GameEngine has no xfer; GameClient::xfer 0x647ABF saves m_frame, 30 Hz units under model H). Loads usually happen mid-tick (render phase); s continues afterwards exactly as stock (the remaining sub-steps of the interrupted tick run on the loaded world, then a tick).
- **Cadence:** event
- **Reason:** Load is handled by the reset hook (0x6DEB9A always calls GameEngine::reset): off with conversion, so post-load behaviour equals stock 30 mode at s30=ceil(s60/2). Re-enable at the next successful tick if the loaded mode passes. Save needs no action; files stay interchangeable.
- **Risk if wrong:** Switching modes mid-tick without conversion would make the 30-mode stepper see s in 7..12 and tick early, skipping sub-steps (logic divergence).

### G5-14 — mode controller  [run_on_B_as_is, high]
- **Site:** C0 render-phase hook at 0x6325CF 'ff 90 9c 00 00 00' (call [eax+0x9C] = clientUpdate 0x632409); ESI/ECX = TheGameEngine [0xDE4324]; s=[esi+0x34], frac=[esi+0x3C]; GC=[0xDE4388] +0xC8 frameAdvanced, +0x10 m_frame; EBX(BL)=halt flag from 0x603452 must be preserved
- **What:** Runs once per main-loop iteration, before the render. At this point GC+0xC8 and s are the previous frame's stepper results: (s==1 && GC+0xC8==1) <=> the previous frame ticked successfully, and in 60 mode the last render was a B-render, so the A/B pair is complete.
- **Cadence:** per render (every loop iteration, incl. paused/halted)
- **Reason:** Controller: (1) if g_m60 && !Safe() -> Off60: s=(s+1)>>1; frac = clamp(s/6) (call 0x63256F with ECX=engine); write bytes of G5-15 to stock; pending sync flush rule; clear B-flag/limiter toggle; flush split-step halves. (2) else if !g_m60 && Safe() && s==1 && byte[GC+0xC8]==1 (&& optional GL+0x9C==0, hysteresis >=1 tick) -> On60: s stays 1, frac=1/12, bytes to 60 values, reset limiter toggle, invalidate caches. Then call [eax+0x9C]. Optional post-render check (after clientUpdate, before the stepper) for immediate off on render-phase triggers. Logic-phase triggers (scripts, MSG_NEW_GAME/0x1D, LW messages, setFPS) always land at s==1 and are pair-aligned here.
- **Risk if wrong:** Turning on at s!=1 without conversion skips or repeats sub-steps; turning off mid-pair without flushing split-step integrators loses half a step once (visual only).
- **Fix @ 0x6325CF:** e8 <C0Stub> 90; C0Stub: pushad; Controller(); popad; call [eax+0x9C]; [optional pushad; PostCheck(); popad]; ret (preserve EBX) (orig: ff 90 9c 00 00 00)

### G5-15 — mode switch bytes  [n/a, high]
- **Site:** 0x632606 (imm8 of 'cmp ecx,6' at 0x632604), 0x63264C (imm8 of 'cmp eax,6' at 0x63264A, jle 0x6326EB), 0x44B98E (imm8 of 'add esi,-0x1e' at 0x44B98C), 0x44B9C3 (imm8 of 'cmp ecx,0x1d' at 0x44B9C1); originals verified identical in game.dat, delayfix.dat, game820.dat
- **What:** Bytes written by On60/Off60 on the game thread (pages made RWX once at install; FlushInstructionCache after write). All other model-H stubs (C1b frac, C1d sub, C1e restore, C2 limiter, C3 sync, C4 keys, C5 physics, C7 particles, allow-list gates) read g_m60 at runtime and need no byte switching.
- **Cadence:** event (mode switch)
- **Reason:** 0x63264C 06/0C is required (tick at s>12). 0x632606 06/0C only affects the +0x38 refresh at end of cycle; +0x38 only changes in MP (0x63239D), so it has no effect in SP - switch for symmetry. 0x44B98E E2/F1 and 0x44B9C3 1D/0E (or alternating 0E/0F) are the paused-camera inner limiter. Data: s conversion and fraction recompute (G5-14).
- **Risk if wrong:** 0x63264C left at 0C in 30 mode: 12 frames per tick at 30 FPS = half game speed; left at 06 in 60 mode: 6 frames per tick at 60 FPS = double speed.
- **Fix @ 0x63264C:** 0C in 60 mode, 06 in 30 mode (orig: 06)
- **Fix @ 0x632606:** 0C in 60 mode, 06 in 30 mode (optional, no SP effect) (orig: 06)
- **Fix @ 0x44B98E:** f1 in 60 mode (prev = now-15) (orig: e2)
- **Fix @ 0x44B9C3:** 0e (or alternating 0e/0f) in 60 mode (orig: 1d)

### G5-16 — script debugger / fast-forward  [n/a, high]
- **Site:** Script debugger: DLL handle [0xDE3B98], loaded at 0x6056E5 (LoadLibrary 'DebugWindowLite.dll' 0xBF9CCC if 0xDE87BA else 'DebugWindow.dll' 0xBF9CBC) only if GD+0x2C (windowed) && GD+0x9C1 (scriptDebug) (checks 0x6056C7, 0x6056D0); halt FUN_00603452 (stepper skipped at 0x6325D5 -> 0x6325DE), debug freeze FUN_0060342F (sub-1 frozen 0x62E547), isTimeFast FUN_00603491 (GD+0xBBD when not paused, or DLL 'RunAppFast')
- **What:** The launcher option 'script debug' passes -scriptdebug2 (handler 0x7BA1D4: GD+0x9C1=1, GD+0x9C6=1, 0xDE87BA=1); combined with -win, DebugWindowLite.dll (present in <game folder>\rotwk) gets loaded and these hooks become live: halting (no stepper, renders continue), debug freeze, RunAppFast (limiter off at 0x63A012, FF render branch at 0x44B897).
- **Cadence:** per frame polling (DLL calls)
- **Reason:** Predicate: [0xDE3B98]==NULL (disable 60 entirely when the script debugger DLL is loaded) plus byte[GD+0xBBD]==0; avoids calling GetProcAddress-based 0x603491 from the DLL every frame.
- **Risk if wrong:** RunAppFast or halt/resume toggling mid-tick with 60 on gives FF at half speed and odd pacing (no logic change).

### G5-17 — command line / launcher  [n/a, medium]
- **Site:** Command line table 0xC35DA8 (16 entries {name, handler}), matcher FUN_007BA7E1 (_strnicmp, length must match), called from FUN_007BAA44 in GameEngine::init 0x63AD4F; WinMain parser FUN_004027F7 (-win/-fullscreen -> 0xDC3C68, -xpos/-ypos -> 0xD8AFE0/0xD8AFE4, -automatch)
- **What:** Entries: -noshellmap 0x7B9EC7 (GD+0xAF0=0, +0xAF1=1), -mod 0x7BADB9, -noaudio 0x7BA024, -xres 0x7BA104, -yres 0x7BA131, -win 0x7B9FC6 (GD+0x2C=1), -scriptDebug2 0x7BA1D4, -scriptDebugLite 0x7BA1FB, -fullVersion, -preferLocalFiles, -Watchdog, -noWatchdog, -rif, -file 0x7BACA4 (GD+0xABC; .map -> new game mode 2/0 at 0x63CA19, .rep -> replay 0x63CBD6; clearGameData then quits the app, 0x7793F0..), -resumeGame 0x7BACE3 (GD+0xAC4=1; load at 0x639E2A), -randomSeed 0x7BA795 (GD+0x1228). No -quickstart in RotWK; '-startPaused' is a MemoryPool/debug string. AotR_Launcher.exe (PyInstaller, views.home_view.launch_game) runs rotwk_exec (lotrbfme2ep1.exe, lcf 'RUN = . game.dat') with: -mod <aotr>, -win or -fullscreen, -xpos 0 -ypos 0, -xres W -yres H, optional -scriptdebug2.
- **Cadence:** once at startup
- **Reason:** Relevant for G5: -scriptdebug2(+-win) -> G5-16; -noshellmap -> menus in mode 9; -file/-resumeGame -> direct start into mode 0/2/3 (controller handles via predicate); -win affects present/vsync behaviour (DXVK d3d9.dll + dxvk.conf present in rotwk). No flag changes FPS limit (that comes from GameData UseFPSLimit GD+0x26 / FramesPerSecondLimit GD+0x28).
- **Risk if wrong:** Ignoring -scriptdebug2 would let debug halt/fast paths run under 60 mode.

### G5-18 — binary identification (launcher PvP mode)  [n/a, high]
- **Site:** delayfix.dat vs game.dat: 0x632535 'f7 3d 08 f6 d9 00' (idiv [LTR]) vs 'f7 3d 00 a4 ec 00' (idiv [0xECA400]=8) in isTick 0x63252F; 0x632A9B 'f7 3d 08 f6 d9 00 8b' vs 'b8 02 00 00 00 eb 19' in step() catch-up loop; other diffs 0x6E5A10, 0x8ED4BF, 0x8ED544, 0x920B79, 0x9A3AE0, AotR sections
- **What:** The launcher's PvP mode installs delayfix.dat as game.dat. It forces step(1) to also call GameLogic::update(2) in the same frame (catch-up loop) and changes isTick to s==2, i.e. a different stepper cadence.
- **Cadence:** once (DLL init)
- **Reason:** DLL must verify the original bytes of every patch site and the two delayfix signatures; refuse 60 mode on delayfix.dat (it is MP-oriented and the stepper patches assume stock cadence).
- **Risk if wrong:** Applying model-H stepper patches over the delayfix catch-up path would call sub-steps out of order and change logic speed.

### G5-19 — minimize / focus  [n/a, high]
- **Site:** Win32GameEngine::update 0x44181F: after GameEngine::update, while IsIconic(0xDC3C64): Sleep(5), serviceWindowsOS, TheLAN (0xDE4394) update; breaks for quit or mode 5/1
- **What:** In SP the main loop (stepper and render) stops while minimized; game time freezes. MP keeps running.
- **Cadence:** event
- **Reason:** No mode switch. The limiter deadline-pacing stub must reset 'last' after a large gap (bounded catch-up), otherwise the game would fast-forward after restore.
- **Risk if wrong:** Unbounded deadline catch-up runs many frames unthrottled after restore (game speed burst).

### G5-20 — Create-a-Hero  [n/a, high]
- **Site:** GD+0xD45 Create-a-Hero flag: set 0x91A7A4 (FUN_0091A6BF), restored on exit 0x919FB6 (FUN_00919EDE from CaH+0x435), ctor 0x6435AF; readers incl. particle gate 0x5F5168, water dt 0x50017F, GameLogic 0x62ECE7; CaH map mode 7 via 0x91A06C; CaH test map mode 6 via 0x9C50D4
- **What:** CaH runs a dedicated map in mode 7 (or test map in mode 6) and switches several client systems into special timing paths via GD+0xD45.
- **Cadence:** event
- **Reason:** Predicate: mode!=7 and byte[GD+0xD45]==0 (covers the CaH test map that uses mode 6).
- **Risk if wrong:** CaH preview paths (forced 33.3 ms water dt, particle m_frame gate) would mix with 60-mode stubs.

### G5-21 — FPS limit changes  [n/a, medium]
- **Site:** FPS limit GameEngine+0xC: setFramesPerSecondLimit vt+0x48 0x66F0FD; writers GameEngine::init 0x63C841, ScriptEngine reset 0x6096B1, MSG_NEW_GAME 4th arg 0x779DCF (and UseFPSLimit=1 at 0x779DD7; restart passes current FPS from vt4C at 0x9220DE), script SET_FPS_LIMIT 0x7CCF20 (+GD+0x26=1 at 0x7CCF28), credits 0x91B6DC (100) / 0x91B7AA (restore GD+0x28)
- **What:** Mission scripts can change the FPS limit (which in stock changes game speed). Under model H the C2 limiter stub halves P=_ftol2(1000/fps) for any fps, so game speed per tick stays stock; render rate becomes 2*fps.
- **Cadence:** event (logic phase sub 1 for scripts and MSG_NEW_GAME)
- **Reason:** No switch required for speed correctness. Optional predicate GE+0xC == GD+0x28 (30) if 'exactly 60 FPS' is preferred over 'double the scripted rate'. Require GD+0x26 (UseFPSLimit) != 0.
- **Risk if wrong:** None for logic; only render rate differs from 60 during scripted FPS changes.

### G5-22 — cinematic letterbox (optional predicate)  [n/a, medium]
- **Site:** TheDisplay [0xDE4418] letterbox: +0xD8 enabled flag, +0xD4 fade value (FUN_00443939, called from drawFrame); script actions CAMERA_LETTERBOX_BEGIN/END (strings 0xC45168/0xC450FC in 0x7D5270 table)
- **What:** Letterbox mode marks scripted cinematics where logic usually keeps running but the camera is script-driven (per-render camera moves).
- **Cadence:** event (scripts, logic phase) + per-render fade
- **Reason:** Optional safe default if scripted-camera fixes (synthesis row 55) are not in the first release: also require byte[[0xDE4418]+0xD8]==0 and TV->vt78()!=0 (no camera movement). Changes land at s==1 (scripts), so switching is pair-aligned.
- **Risk if wrong:** Without camera fixes and without this predicate, cinematic camera pans/zooms run 2x fast at 60 FPS.

## Open questions

- Living World strategic map at 60 FPS (the user lists LW as in scope): is the LW client update vt6C 0x49AAB8 (+FUN_00645750) frame-rate dependent? It is called per render at 0x6484D9. Also the LW render vt20 0x49B618, the LW camera 0x8392A7 -> LW view vt2C 0x49B799, and TheShell::update in the GameClient path when TheShell[0x17]. These need an audit before mode 8 / GL+0x125 can be allowed.
- Shell map (mode 4) and menus: not audited (window transitions 0x5DB60A, TheShell::update, shell audio dt, APT callbacks, credits setFPS 100). It stays at 30 FPS in v1.
- Replays (mode 3): logic is deterministic, so 60 could be allowed if GD+0xBBD (written at 0x820571 in the render phase) triggers an immediate switch-off. Is this wanted?
- Cinematics: will the first release include the scripted-camera fixes (synthesis row 55)? If not, the optional predicates are needed: TV+0x23D0 freeze, TV->vt78 camera moving, and letterbox [[0xDE4418]+0xD8].
- Exact meaning of the LW client's +0x19 (set via vt+0x50 0x49BE23). It gates both GL+0x125 and the LW draw path. Is it set during LW battle loading or preview?
- Is GameClient m_frame reset to 0 when a new game starts? This decides the length of the 'first 6 client frames unlimited' window (0x63A17B, [0xDE4308] reset at 0x77945F / 0x6315FB) under model H. Logic-neutral either way.
- The exact composition of launcher arguments was taken from the 3.12 code-object constants, not from a bytecode disassembly. The order and the conditional -win/-fullscreen handling have medium confidence.
- Runtime check M9: log s, g_m60, mode, GL+0x124/+0x125, TV+0x23D4 and the reset-hook events across pause, Esc, save, load (render-phase and LW), restart, quit to shell, LW battle enter and exit, and cinematics. Confirm that every switch happens at s==1, or uses the conversion inside the reset hook.
