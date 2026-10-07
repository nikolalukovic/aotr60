# AREA shellmap

## SUMMARY

AotR has no 3D shell map. ShellMapOn = No in every GameData source, so the main menu runs with GL+0x110 = 9 (no game). It is a purely 2D shell: APT movies, a static Display background image (MappedImage ShellMapLowLOD = InstallLoad.tga), window transitions and a hardware .cur cursor. None of the 3D presentation machinery needs to change for the shell, and none of it is bypassed. The camera stays static, so S1/S2/SCENE_OPEN reduce to no-ops. There are no drawables, so C4, C5 and C7 do nothing. C3 runs the same W3DDisplay::draw code as in battles. Split present already stays off in mode 9 through its own StateGate. The pacer works unchanged: GE+0xC is 30 in the shell, giving 33 ms pairs. The fallback policy does not trip on isolated 50-110 ms menu hitches. Two things must be blocked: the credits, which set the FPS limit to 100 and Shell+0x5D=1, and mode 4, which AotR never uses. The key quantitative finding: all 19 AotR APT movies surveyed have MillisecondsPerFrame = 33, and every shell stepper is A-only. So in 60 mode the B-render presents a pixel-identical copy of the A-render, and the content on screen still changes every 33 ms. At 120 Hz FIFO the menu would look exactly like stock 30 FPS unless the APT/UI area finds animation that can be evaluated per render. My area needs no new patch sites. If the menu is enabled anyway, only a mode-controller predicate in frame_ctl.cpp is needed.

## DETAILS

Scratch files: <analysis workspace>/menu\shellmap_pacing\ holds w3dview_update.c, w3ddisplay_draw.c and aptmainmenu_fscmd.c (decompiles).

**1. There is no 3D shell map (CONFIRMED)**
- **INI sources.** Every GameData source sets `ShellMapOn = No`:
  - <game folder>\aotr\data\ini\gamedata.ini:11133
  - aotr\data\ini\object\gamedata.ini:3
  - rotwk\#aotr_patch202.big data\ini\gamedata.ini
  - rotwk\INI.big data\ini\gamedata.ini
  - The only other copy, INI.big default\gamedata.ini (39 bytes), has no ShellMapOn line.
- **Parse table.** Entry 0xC00510 = {"ShellMapOn" 0xC027E4, bool parser 0x42E558, GD+0xAF0}. Entry 0xC00500 = ShellMapName → GD+0xAEC.
- **Writers of GD+0xAF0.**
  - Ctor 0x64308E sets it to 1.
  - Every other writer only clears it or keeps it: 0x601F05 (the LOD apply keeps it only if it is already set and +0xAF1 == 0), 0x7B9ED0 (-noshellmap), 0x7BA988, and the -file paths 0x63C9B0, 0x63CBFC and 0x63CC65.
- **Shell::showShellMap 0x75DE01.**
  - With AF0 == 0 it sets Shell+0x52 = 1 and never sends MSG_NEW_GAME(4); the new-game branch needs AF0 != 0 at 0x75DE76.
  - It clears the Display image layers (0x65CFF6).
  - It sends MSG_CLEAR_GAME_DATA only if the mode is already 4.
- **Shell::showShell 0x75E43C.** With AF0 == 0 and an empty screen stack (Shell+0x4C == 0) it pushes MainMenu.apt, then sets Shell+0x5C = 1. hideShell 0x75DBB9 sets +0x5C = 0.
- **Shell::update 0x75E1D3.** Screen updates are throttled by timeGetTime. When +0x52 && !+0x53 && !+0x5D it:
  - sets the static MappedImage ShellMapLowLOD (handcreatedmappedimages.ini:15, InstallLoad.tga) as Display image layer 0 via 0x65C42C (slot display+0x68);
  - sets [0xDE4418]+0x114 = 1;
  - starts the transition FadeInGameMovie_NoAudio.
- **Runtime evidence.** In the menu, aotr60.log stall lines show logic "frame 0": the 17:25:04-17:25:12 and 17:31:27-17:31:32 entries, i.e. log lines 11-16 and 760-762. With a shell map, GL+0x40 would advance. GL+0x110 = 9 comes from the ctor 0x6301C9 and from clearGameData 0x7793C5 (../gaps/G5_modes.md:12, :138).
- **Other 3D candidates.**
  - MainMenu.apt only imports View3D (from GameWindowGadgets) and uses BinkMovie only for the credits (movieCredits, AptMainMenu::RenderCredits).
  - SpellStore is the only other APT movie that references View3D.
  - The LivingWorldMap APT window (MpGameSetup) is the LW area's case. frame_ctl.cpp:113-125 already keeps it at 30 ("Living World map transition").

**2. The draw path in the shell is the battle path; nothing bypasses the presentation hooks (CONFIRMED by decompile)**

drawFrame 0x449CF8 runs in this order:
1. updateViews vtA0. It runs because GL+0x125 == 0, so W3DView::update 0x48BCF2 runs with S1 at 0x48BD1B and S2 at 0x48C701.
2. Scene window 0x449DAB → 0x44A23E.
3. RenderViews vt98.
4. Display image layers 0x65D03B → 0x65C529. This draw is idempotent: no state writes.
5. Overlay fade 0x65CF05, which is already A-only (INT_OVL_FADE).
6. RenderUI: APT [0xDE3F0C] vt30 between the two [0xDE4830] vt30 calls.
7. UI particles 0x6A536C (0x6A53FB A-only).
8. [0xDE36E0] vt30 (mouse).
9. Transition handler [0xDE3654] vt30.

There is no separate view object, shell camera or letterbox path:
- **Drawable pass.** The pass at 0x48C701 exits when the drawable list is empty ([0xDD1E0C] == [0xDD1E10]), so C4 and C5 never run.
- **Static camera.**
  - The B swap is a no-op because BeginSwap compares memory first (camera.cpp:92-95).
  - The A interpolation takes the "camera did not move" exit (camera.cpp:294).
  - Shake-hold and cut rules never engage.
- **Load screen.** SceneOpen_A skips load-screen renders via the GD+0xAF6 == 1 check (camera.cpp:271).
- **C3.** W3DDisplay::draw 0x44B788 adds sync = [0xDC7A8C]·Δm_frame only when GC+0xC8 is set. This is the same as in battles: m_frame does advance in the menu because the stepper sets GC+0xC8 = 1 at 0x6325FB and s cycles 1..6, as the log shows subs 2/4/5/6. Nothing visible in the shell reads the sync clock.
- **M>1 wait.** The 29 ms busy-wait at 0x44B98x is inactive because TV vtDC < 2.

**3. Pacing (CONFIRMED)**
- **FPS limit.** GE+0xC in the shell = GD+0x28 = 30 (gamedata.ini:11138).
  - GameEngine::init 0x63C841 sets it.
  - The ScriptEngine reset at 0x6096AC/0x6096B1 restores GD+0x28 after any map SET_FPS_LIMIT.
  - StockFrameMs (pacer.cpp:191) therefore gives 33 ms pairs and 16.5 ms halves, as in battles.
- **Stock limiter.** FUN_00639FEF 0x63A17B..0x63A1F3 is the same limiter used in battles. Its unlimited cases (UseFPSLimit off, TV vtDC ≥ 2, RunAppFast, m_frame < [0xDE4308]+6 after 0x63239D) all reach the pacer as gaps.
- **Credits** (AptMainMenu::Credits registered at 0x91CDB3 → 0x91B5E9):
  - Start sets Shell+0x5D = 1 (0x91B6CE) and the FPS limit to 100 (push 0x64 / call vt48 at 0x91B6DA/0x91B6DC).
  - Exit 0x91B6FD clears +0x5D (0x91B796) and restores GD+0x28 (0x91B7AA).
  - The credits scroll update is the credits manager [0xDEBF50] vt28, called from AptMainMenu::update 0x91C2FC (state 4, call at 0x91C643). RenderCredits 0x91B1B8 → 0x9C6765 runs per render.
  - Stock credits therefore already render at 100 FPS. In 60 mode the pacer would target 10 ms pairs (200 renders/s) and would likely fall back.
- **Fallback with light load** (pacer.cpp late handling; pacer_policy.cpp:5-8).
  - Menu hitches seen in the log are 56-108 ms renders.
  - Lateness up to 250 ms is carried up to 3×16.5 = 49.5 ms; the excess counts as lost time. A 108 ms render loses about 42 ms, i.e. 0.84 % of a 5 s window.
  - The policy needs >5 % lost time for 3 windows (slow), >50 % late iterations or >25 % lost for 2 windows (overloaded), or <50 % on-time iterations for 2 windows (sustained). An idle menu is about 100 % on time, so it never falls back.
  - Stalls over 250 ms are gaps: the 5.2 s first render at startup and the 15.5 s skirmish load, both in the log.
- **Split present** is gated at split_present.cpp:611-613: a mode not in {0,2,6} returns kGateLwMap. So SpTryDefer never defers in the shell and SpDeferLikely returns false, which also disables the early release. Nothing needs to change.
- **Present path, fullscreen FIFO 120 Hz** (log: "display: vsync (presentation interval 0x0), fullscreen, refresh 120 Hz").
  - At 16.5 ms spacing the swap queue never fills, so the B Present-skip, which needs presentsSinceBlock < 2, never fires.
  - Present pacing spaces the frames: 1.98 vblanks per Present, with one 1-vblank Present every ~50 frames (~0.83 s).
- **Light-load B-render.** The B-render only redraws: APT render is idempotent (area_ui.md), the image layers are idempotent, and all UI steppers are A-only (GATE_CU_APT, GATE_GC_WM incl. transitions, GATE_GC_SHELL, GATE_CU_UISEQ, INT_OVL_FADE, UI particles).

**4. What 60 FPS can show in the shell (key result)**
- **APT frame rate (CONFIRMED values; field layout INFERRED from the OpenSAGE APT format).** Every APT movie has a 33 ms frame. The movie header (word 9 at the .const-given offset) holds MillisecondsPerFrame:
  - MainMenu.apt: header at 0x500, value 0x21;
  - SkyrimMenu.apt: 0x21;
  - all 18 other APT movies in aotr\AOTR*.big: 33.
- **Cursor (INFERRED).** It is a hardware cursor: mouse.ini uses .cur textures, loaded with LoadCursorFromFileA (0x441506) and set with SetCursor (0x4997D8). It already moves at the OS rate.
- **Video and music (per G5).** The video update 0x65C1EA runs inside the A-only GameClient::update span, and audio is A-only.
- **Result.** In 60 mode every B-render is a pixel copy of its A-render, so the visible content cadence stays 30.3 Hz and input latency is unchanged. Enabling the menu changes the Present rate and the title text, not what the user sees. Real 60 Hz motion would need UI animation that can be evaluated per render. APT timelines are discrete 30 fps keyframes and cannot be half-stepped, so any such source has to come from the APT/UI area (for example ActionScript setInterval-driven fades, if intervals fire on their own clock).

**5. What must change to enable the shell (C++ only, no patch sites)**

In frame_ctl.cpp GameModeBlockReason (:113-120), allow mode 9 only when all of these hold:
- TheShell [0xDE7890] is non-null, Shell+0x5C (shell shown) is set and Shell+0x4C > 0;
- Shell+0x5D == 0 (not in the credits);
- GE+0xC == 30, or more generally GE+0xC == GD+0x28 == 30;
- GL+0x125 == 0 and the LW view [0xDE4958]+0x18 is clear (the LW preview stays with the LW area);
- GD+0xAF6 != 1 (not the load screen);
- GD+0xAF2/0xAF3 clear (already in BlockReason :191-192);
- GD+0xD45 == 0 and mode != 7 (Create-a-Hero);
- TheNetwork is NULL, plus the LAN/online lobby objects are absent (modes/UI area to name them).

Mode 4 stays blocked.

Everything else stays as is:
- StartBlockReason (:216-229): GC+0x10 ≥ 8, GL+0x9C/0xA8 and TV+0x23D4 behave as in battles (INFERRED that they are clear in the menu);
- camera.cpp, c5_physics, split_present.cpp, pacer.cpp and pacer_policy.cpp;
- the reset hook. Menu-to-game starts go through the A-only pending-game path (0x62B385) or a sub-1 logic reset, and both reach GameEngine::reset 0x44181A → OnEngineReset, which leaves 60 mode before the load. Game-to-menu goes through the same reset, and the menu re-enters 60 at the next pair boundary once m_frame ≥ 8 (about 0.27 s).

## RISKS

1. No visible benefit (CONFIRMED by data; APT field layout INFERRED).
   - The shell has no 3D content, and all APT movies run at 33 ms per frame.
   - In 60 mode the B-render repeats the A-render, so the user will see exactly the stock 30 Hz menu with twice the Presents.
   - Promising "smooth 60 FPS main menu" would mislead the user unless the APT/UI area finds content that can be evaluated per render.

2. Credits.
   - 0x91B5E9 sets the FPS limit to 100 and Shell+0x5D = 1. Without a block, the pacer would target 10 ms pairs (200 renders/s) at 4K, probably trip the fallback and start a 30 s backoff that carries into the next battle.
   - Block on Shell+0x5D and on GE+0xC != 30. The credits already run at 100 FPS in stock.

3. Fallback state is global.
   - A fallback earned in the menu (for example during APT screen loads) would also block 60 in the next battle for 30 s to 8 min.
   - Isolated 50-110 ms menu hitches do not trip it, but repeated sub-screen loads within 10-15 s could (sustained/slow windows).
   - Option: reset the fallback when leaving the shell, or exclude shell windows from the policy.

4. Mode-9 predicate breadth.
   - Mode 9 also covers the intro, the load screen (GD+0xAF6), the score screen, LAN/online lobbies (TheNetwork is still NULL there) and the LW preview inside MpGameSetup.
   - The predicate must name each of them, or multiplayer UIs would run under the A/B scheme, contrary to the SP-only constraint.
   - The lobby objects to test are not identified here (modes/UI area).

5. Mode 4 stays unaudited.
   - It is never reached with AotR's INI.
   - A user who edits ShellMapOn = Yes would get a real GameLogic shell map with scripted cameras. The battle camera code would apply, but that path was not audited. Keep mode 4 blocked.

6. Telemetry (INFERRED).
   - The tick invariants assume logic ticks. In mode 9 GL+0x40 stays 0, so verify that Telemetry=1 does not log false tick errors or skew the 60-mode rate CSV while in the menu.

7. Present cadence at 120 Hz FIFO.
   - One Present per ~0.83 s is shown for one vblank instead of two. This is invisible in the menu because A and B are identical, the same as in battles.

## RECOMMENDATION

My area needs no new patch sites and no change to camera.cpp, C3/C4/C5/C7, split_present.cpp or the pacer: there is no 3D shell map, the 3D presentation hooks are inert in the shell, and split present is already gated off for mode 9.

**Recommended answer to the user:** the AotR main menu stays at 30 FPS by default. Its content is 30 fps APT movies plus a static background and a hardware cursor, so 60 FPS rendering would produce byte-identical duplicate frames and no visible improvement. The exception is if the APT/UI area finds menu animation that can genuinely be evaluated per render.

**If the user still wants the 60 FPS status in the menu,** add an opt-in `MainMenu=1` allowance to GameModeBlockReason (frame_ctl.cpp:113) for GL+0x110 == 9 with:
- TheShell [0xDE7890] non-null, Shell+0x5C set, Shell+0x4C > 0;
- Shell+0x5D == 0 (credits) and GE+0xC == 30;
- GL+0x125 == 0 and the LW view +0x18 clear;
- GD+0xAF6 != 1, intro flags clear, not Create-a-Hero;
- no network and no LAN/online lobby objects.

Mode 4 and the LW preview stay at 30. Also consider resetting the pacer fallback when a game starts, so a menu fallback never carries into a battle.

Verification:
- Telemetry=1 shows no tick errors in the menu.
- Credits stay at 30 mode with the stock 100 FPS.
- Menu → skirmish and skirmish → menu log a reset-hook "60 FPS off" before the load.
- No fallback is logged during menu navigation.

## VERIFIER
Mostly correct. The central conclusion holds and my checks make it stronger: AotR has no 3D shell map, the shell is mode 9, every menu stepper is A-only, and APT content steps at 30 Hz or slower. So enabling 60 mode in the menu doubles the Presents and changes nothing the user can see.

Four corrections:
1. The cursor citation is partly wrong. SetCursor at 0x4997D8 is only SetCursor(NULL) for redraw modes 1-3. The Windows-cursor path is 0x4413E3 SetCursor(table[+0x601C + id*8]). Mode 0 (RM_WINDOWS) is the default because GD+0x9C6 = 1 (GD ctor 0x642E38, Mouse::init 0x5EE519..0x5EE529).
2. The APT survey was incomplete. It missed the 81 base-game movies in <game folder>\rotwk\apt\*.big, among them MpGameSetup, Background, MenuFrameAndBg and libSnow. I checked them: 79 run at 33 ms per frame, MenuFrameAndBg at 83 ms (12 fps) and libSnow at 40 ms (25 fps). Nothing runs faster than 30 fps, so the conclusion gets stronger, but the claim "all 19 movies surveyed" does not cover what the menu actually loads.
3. "APT timelines cannot be half-stepped" overstates it. They cannot be stepped at half frames, but the display-list matrices and colour transforms of persistent characters could be interpolated at render time, the way units already are. That is the only route to a menu that really looks 60 FPS. It is unexplored and large: APT render reverse-engineering, plus handling for cuts and removed characters.
4. Menu-to-game: the reset fires before the load on the paths I checked; the Living World campaign entry is the one that needs checking. G5:194 lists clearGameData callers for skirmish (0x9286D7), campaign (0x5EAB8F), tutorials (0x91B825), CaH (0x9C4FEA) and loadGame. All of them reach GameEngine::reset in the render phase before MSG_NEW_GAME is processed. The analysis's "sub-1 logic reset" alternative is unproven. The Living World campaign entry from the menu (0x6B5286: mode 8 set directly, then startNewGame(0)) has no proven reset-hook crossing.

All sites, bytes and values I re-checked match: INI lines, parse table, showShellMap / showShell / Shell::update, the credits bytes, GE+0xC init, the stepper bytes, the frame_ctl / camera / split_present line numbers, and the pacer arithmetic.

No patch sites are needed if the goal is only the "60 FPS" status. The right answer to the user is still: the main menu stays at 30 by default, because 60 would show identical duplicate frames.

- [confirmed] ShellMapOn = No in every GameData source (aotr gamedata.ini:11133, object/gamedata.ini:3, #aotr_patch202.big, INI.big)
  Raw scan of every .big/.ini/.inc under aotr and rotwk finds only these 4 hits, all 'No'. Caveat: RefPack-compressed .big entries cannot be seen by a raw scan. Parse table checked: 0xC00510 = {0xC027E4 'ShellMapOn', 0x42E558, 0, 0xAF0} and 0xC00500 = {'ShellMapName', 0x42EE5E, 0, 0xAEC}. Ctor sets GD+0xAF0 = 1 at 0x64308E ('mov byte [esi+0xaf0],1').

- [confirmed] showShellMap 0x75DE01 with AF0==0 sets Shell+0x52=1, never sends MSG_NEW_GAME(4), and sends MSG_CLEAR_GAME_DATA only in mode 4
  Decompile: +0x52 = (param_2 && AF0==0). The new-game branch needs AF0 != 0 (cmp at 0x75DE76). The else path calls 0x65CFF6 and 0x75D9B1, then sends vt48(0x1D) only if GL+0x110 == 4. Minor: +0x52 is set only when param_2 (show) is true, and the whole block needs GD+0xABC empty and GL non-null.

- [confirmed] showShell 0x75E43C pushes MainMenu.apt when AF0==0 and Shell+0x4C==0, then sets Shell+0x5C=1
  Decompile matches. Omitted detail: if the env var _EA_RTS_HEADLESS or GD+0xAC5 is set, it pushes LanLobbyMenu.wnd instead.

- [confirmed] Shell::update 0x75E1D3: timeGetTime-throttled screen updates; with +0x52 && !+0x53 && !+0x5D it sets ShellMapLowLOD (InstallLoad.tga) as image layer 0, [0xDE4418]+0x114=1, FadeInGameMovie_NoAudio
  Decompile and handcreatedmappedimages.ini:15-21 match. Not mentioned by the analysis: the throttle constant is 32.333 ms at 0xC2C90C (comiss at 0x75E210), compared against an integer timeGetTime delta. So a delta of 32 ms skips the screen runUpdate, the animate-window-manager vt28 and the scheme update for that render, with no catch-up. [0xDE4418] is TheDisplay: the W3DMouse draw uses its vt40/vt44 as width/height.

- [confirmed] Menu runs at GL+0x110 = 9 with logic frame 0
  aotr60.log lines 10-16 and 760-762: 'stall 30 -' entries show 'frame 0' with subs 1/2/4/5/6 cycling, plus the 'game mode (menu or multiplayer)' line. Mode 9 is set by the ctor and by clearGameData (../gaps/G5_modes.md:12, :116). Mode 4 needs AF0 != 0.

- [confirmed] drawFrame 0x449CF8 order: updateViews vtA0 (GL+0x125==0), scene, RenderViews vt98, image layers 0x65D03B, overlay fade 0x65CF05, RenderUI (APT [0xDE3F0C] vt30 between [0xDE4830] vt30), UI particles 0x6A536C, mouse [0xDE36E0] vt30
  Decompile matches. Not mentioned: when TheDisplay+0x38 != 0 (Display video buffer), the overlay fade is skipped and a video-draw tail runs (layers 0/1, video draw via vt104, layer 2, overlay fade). A menu predicate should also require Display+0x38 == 0. That meaning is INFERRED.

- [refuted] Cursor is a hardware cursor: LoadCursorFromFileA at 0x441506 and SetCursor at 0x4997D8
  The conclusion is right but the citation is wrong. 0x4997D8 (W3DMouse setCursor, vtbl 0xBDE64C) calls SetCursor(NULL) for modes 1/2/3; mode 3 is a D3D hardware cursor via device vt28/vt2C/vt30. The Win32 path is 0x4413E3 SetCursor(hcursor). The mode is chosen at Mouse::init 0x5EE519..0x5EE529: GD+0x9C6 != 0 gives 0 (RM_WINDOWS), otherwise 1 (W3D). GD+0x9C6 = 1 in the GD ctor 0x642E38, and no INI parse entry writes it. So a Windows cursor at OS rate is CONFIRMED statically; runtime is INFERRED. W3DMouse::draw 0x498CBC re-applies the same cursor every draw, which is harmless. The tooltip linger counter is already A-only in sites.json; the tooltip delay is timeGetTime-based.

- [confirmed] All 19 AotR APT movies have MillisecondsPerFrame = 33 (header word 9; MainMenu header at 0x500 = 0x21)
  MainMenu.apt @0x500 reads {9, 0x09876543, 17 frames, ..., 0x400x0x300, 0x21}; SkyrimMenu likewise 0x21. My parse of the 19 AotR APTs inside .big archives also gives 33 for all. But the survey misses rotwk\apt\*.big (81 base movies: MpGameSetup, Background, MenuFrameAndBg, GuiFX, ScoreScreen, ...). There 79 are 33 ms, MenuFrameAndBg is 83 ms and libSnow is 40 ms. Nothing is faster than 30 fps, so the 'no visible benefit' conclusion gets stronger. MainMenu.const also holds setInterval/clearInterval/vFadeLogoInTimer; they are stepped by the A-only APT update, so at most 30 Hz.

- [uncertain] APT timelines are discrete 30 fps keyframes and cannot be half-stepped, so no 60 Hz menu motion is possible without APT/UI help
  Half-stepping the timeline is indeed impossible, because ActionScript and frame actions run per frame. But render-time interpolation of display-list matrices and colour transforms between APT frames k-1 and k on the A-render is conceptually possible, the same idea as the unit presentation windows: tweens are baked as per-frame matrices. It is unexplored, needs the APT render internals reversed, and needs rules for cuts and removed characters. It is the only path to a menu that actually looks 60 FPS.

- [confirmed] Credits: Shell+0x5D=1 at 0x91B6CE, FPS limit 100 via push 0x64 / call vt48 at 0x91B6DA/0x91B6DC; exit clears +0x5D at 0x91B796 and restores GD+0x28 at 0x91B7AA
  Disassembly matches exactly; AptMainMenu+0x288 = 4 at 0x91B6BF. Update 0x91C2FC is AptMainMenu vtbl 0xC7CDE0 slot +0x14, and the call at 0x91C643 into [0xDEBF50] vt28 is confirmed. Who calls vt+0x14 (APT update or Shell screen update, both A-only) I did not trace, so 'credits scroll update is A-only' is INFERRED. A pacer at 100 FPS would use 5 ms halves (10 ms pairs, 200 renders/s); block on +0x5D and on GE+0xC != 30.

- [confirmed] GE+0xC in the shell = GD+0x28 = 30 (gamedata.ini:11138); ScriptEngine reset 0x6096AC/0x6096B1 restores it; StockFrameMs gives 33 ms pairs
  0x63C83A push [ecx+0x28] / 0x63C841 call vt48; 0x6096AC/0x6096B1 identical; gamedata.ini:11138 'FramesPerSecondLimit = 30'. pacer.cpp:191-201 truncates 1000/30 to 33.

- [confirmed] Halt/stepper: non-halted step sets GC+0xC8=1 at 0x6325FB
  0x6325F8 xor ebx,ebx / inc ebx / 0x6325FB mov [eax+0xc8],bl. The halted branch 0x6325DE writes 0 and jumps to 0x632705, as in PLAN 1.3.

- [confirmed] Camera hooks inert in shell (camera.cpp:92-95 BeginSwap no-op, :294 'did not move', :271 AF6 skip)
  Line numbers and logic match the source. The drawable-list-empty exit at 0x48C701 was not re-checked (INFERRED).

- [confirmed] Split present stays off in mode 9 (split_present.cpp:611-613)
  mode not in {0,2,6}, or GL+0x125, returns kGateLwMap.

- [confirmed] Fallback: a 108 ms render loses ~42 ms; idle menu never trips the policy
  pacer.cpp: late = 108 - 16.5 = 91.5 and clamp = 3*16.5 = 49.5, so 42 ms of debt; more than 250 ms is a gap. Policy (pacer_policy.cpp:5-19) has two conditions the analysis left out: a skipping condition (skipRatio > 10% for 2 windows), and sustained also needs lostRatio > 0.003. The fallback state is global (PacerFallbackActive), so the carry-over into a battle is real.

- [confirmed] frame_ctl.cpp references (GameModeBlockReason :113-120, intro :191-192, StartBlockReason :216-229)
  Line numbers match. Any mode-9 allowance must go inside the first branch (mode != 8 && !viewActive && !GL+0x125), which already keeps the LW preview at 30 ('Living World map transition').

- [uncertain] Menu-to-game starts leave 60 mode via the reset hook before the load (A-only 0x62B385 path or a sub-1 logic reset)
  G5:194 shows skirmish 0x9286D7, campaign 0x5EAB8F, tutorials 0x91B825, CaH 0x9C4FEA and loadGame calling clearGameData, which reaches GameEngine::reset in the render phase before MSG_NEW_GAME is processed in logic sub 1 (the 15.5 s skirmish load is logic). That path is safe. Not proven: Living World campaign entry from the menu (0x6B5286 sets mode 8 directly, then startNewGame(0)), restart, and a reset arriving inside a Y logic step. The 0x62B385 route is the post-load fade-in, not the reset.

- [confirmed] Present cadence at 120 Hz FIFO: 1.98 vblanks/Present, one short Present every ~0.83 s, invisible
  60.6 Presents/s x 2 = 121.2 vblanks needed vs 120 available, so about 1.2 short Presents/s. Stock 30 has the same 1% slip (one 3-vblank frame every ~0.83 s), so there is no change.

## MISSED
1. Incomplete APT survey. rotwk\apt\*.big (81 movies, including MpGameSetup, Background, MenuFrameAndBg, ScoreScreen, Online*/Lan*) was not surveyed. My parse: 79 at 33 ms, MenuFrameAndBg 83 ms, libSnow 40 ms. No menu content steps faster than 30 Hz.

2. Shell::update throttle. The threshold is 32.333 ms (0xC2C90C), compared against an integer timeGetTime delta, so a measured delta of 32 ms drops the update with no catch-up. With QPC-paced A-renders at 33.000 ms, plus pacer repayment (deadline -= pay) and early release, A-to-A spacing can measure ≤32 ms. Each such pair drops the Shell screen runUpdate, the animate-window-manager update and the scheme-manager update, which could show as window-animation stutter. INFERRED. Add a telemetry counter for skipped Shell updates before enabling.

3. Display video path. When TheDisplay+0x38 != 0, drawFrame takes a separate video-draw tail. The menu predicate should require it to be 0 (the meaning of the field is INFERRED).

4. SP-only enforcement. TheNetwork is NULL in LAN and online lobbies (G5: it is created only at game start), and GL+0x114 is not set there. The predicate therefore needs named lobby singletons (TheLAN / GameSpy objects, still unidentified). A safer design is an allow-list of top-of-stack Shell screen names: MainMenu, Options, SaveLoad, Skirmish setup and CampaignMenu allowed; MpGameSetup/LW preview, Lan*, Online*, CaH, LoadScreen, ScoreScreen and credits blocked. A block-list is not safe enough.

5. Living World campaign entry from a 60 FPS menu. 0x6B5286 sets mode 8 directly and calls startNewGame(0). It is not proven to pass the reset hook before the load. If it does not, the load-screen loop would run while g_m60 = 1. Needs a trace or runtime test.

6. Unverified: whether the score screen is mode 9 or still 0/2 (if 0/2, it may already be eligible today), and whether StartBlockReason's GC+0x10 ≥ 8, TV+0x23D4 and GL+0x9C/0xA8 are all clear in the menu (INFERRED, never measured).

7. The pacer fallback is global. Reset it on the shell→game transition, or exclude shell windows from the policy.

8. Cost with no benefit. The B-render redraws the full 4K UI, so GPU and power use roughly double for identical frames.

9. Only option for real visible 60 Hz menu motion: render-time interpolation of the APT display list (character matrices and colour transforms between frames k-1 and k on the A-render, real transforms kept for hit-testing). Large and unexplored. It needs APT render reverse-engineering and rules for cuts and removed characters.

10. Verification plan if enabled anyway:
- Hash or compare A vs B back buffers in a debug build to prove they are identical.
- Telemetry=1 in the menu to check tick invariants with GL+0x40 frozen at 0.
- The skipped-Shell-update counter from point 2.
- Credits stay in 30 mode.
- Every menu→game path logs '60 FPS off (game reset)' before the load-screen render: skirmish, campaign, LW campaign, tutorial, load and CaH.