# G1_clientupdate

## Summary

G1 result: I enumerated every call in clientUpdate (0x632409) and GameClient::update (0x64849E) and verified each by decompile/disassembly. The allow-list principle holds up, with three refinements.

(1) The gate must use g_uiTick, not g_isARender, so skipped systems still run every other render while paused or frozen.

(2) Some per-render work has to stay on every render:
- the draw;
- the camera-input integrators: LookAt 0x83B471, the LW translator 0x8392A7, and the keyboard-camera block 0x6A21DF..0x6A23CD inside InGameUI::update. All of them need halved steps.
- frequency-independent servicing: pending-game 0x62B385, the deferred-D3D queue 0x532D6F, deferred drawable deletion GC vt90 0x645F65, the focus/input lock 0x80000F, and message propagation 0x7128C3;
- TheSnowManager 0x4943E1. This is a NEW hazard class: it integrates (Sync-PrevSync), so skipping it on B makes snow run at half speed under C3. Only its weather-transition counter (0x49405A) and its lightning rand() chance (0x492C35) must be gated.

(3) Everything else is skipped on B at call-site gates (5-byte `8b 01 ff 50 28` -> call GateVt28): APT, Radar, Audio, Keyboard/Mouse (input "Option II"), Eva, ScoredKillEva, Cloud/Fire/Anim2D, popup pumps, WindowManager+transitions, Display::update (video+ToD), DisplayStringManager, Shell, the rest of InGameUI (ControlBar, Palantir, BannerUI, messages), and the LW view update. That keeps stock 30 Hz cadence exactly and makes the synthesis patches for rows 61-63, 67, 68, 70, 76, 79, 80, 84, 85 and 86 unnecessary.

The drawable block is already m_frame-gated (0x648705 `cmp [0xD9F6F8],m_frame`), so it is automatically A-only.

No update->draw consume/clear coupling was found among the skipped systems. Proof: stock already draws without them while paused (drawables, particles, LW). APT advances only via a real-time accumulator, so it often does not advance in stock either. The radar flag (+0x11) is set-only, and the video surface persists.

Order: 0x62B385 runs first, then m_frame++ at 0x632433..0x632440 (if GC+0xC8). Only after that do APT, Radar, GameClient::update and the draw run. InGameUI::update (keyboard camera) runs AFTER the draw.

Corrections to the synthesis:
- 0xDE7890 is TheShell (update 0x75E1D3), not TheInGameUI. TheInGameUI is 0xDE4830 (vt28 thunk 0x48EA1F -> 0x6A1F4D).
- 0xDE4334 is TheKeyboard. TheDisplayStringManager is 0xDE4518.
- 0xDE8B34 is TheScoredKillEvaAnnouncerController.
- The LW manager update is confirmed per render via 0x49AAB8 -> 0x6C0E4D -> 0x6121C5.

New per-draw integrator: the screen flash 0x65CF05 (it is set only by the intro sequence, so low priority).

Built-in verification aid: GameLogic::getCRC 0x625886, already used by the debug guard 0x6CF64E/0x6CF681 ("GameLogic changed outside of GameLogic::update").

## Design notes

1. VERDICT ON THE ALLOW-LIST PRINCIPLE: sound, and recommended. A system skipped on B-renders sees exactly the stock call sequence: one call per logic sub-step, isTick true on exactly one call per tick (s==1 only on the A-render), and about 33 ms of wall clock between calls. So every per-call counter and timer inside it keeps stock speed with ZERO patches.
- Gate on g_uiTick (A-render while running, alternating while paused or frozen), not on g_isARender. Otherwise the pause menu, APT, audio and InGameUI would freeze during pause.
- Three classes must stay on B-renders:
  (a) the render and the camera integrators driven by input. These need frame-rate-correct steps: LookAt 0x83B471, the LW translator 0x8392A7, the InGameUI keyboard-camera block 0x6A21DF..0x6A23CD, and W3DView::update inside the draw.
  (b) frequency-independent servicing and event work: 0x62B385 x2, 0x532D6F deferred-D3D queue, GC vt90 deferred delete, 0x80000F input lock, 0x7128C3 propagate, 0x645DAD.
  (c) NEW CLASS: consumers of draw-advanced deltas. TheSnowManager 0x4943E1 integrates (Sync - PrevSync). WW3D::Sync (0x516E20, in the draw) moves PrevSync every render under C3, so skipping snow on B loses half the time; run it on B but gate its per-call parts 0x49405A/0x492C35. It is the only PrevSync reader outside the draw path (refs: 0x48BCF2, 0x4943E1, 0x4A89BD, 0x4BF560, 0x516E20, 0x5A1D60). Rule for future B-skips: a skipped system must not compute deltas from globals that allowed per-render code advances; per-object 'last seen' values are fine.

2. UPDATE->DRAW COUPLING CHECK. None of the skipped systems produce per-render data that the draw consumes or clears.
- Proof by pause: stock draws without the drawable block and without particle per-system updates while paused, and the LW view and translator also early-out while paused.
- APT: the accumulator 0xAE18C0 often advances 0 frames per call even in stock, so the draw 0x46203E cannot depend on a per-call update.
- Radar +0x11 is set by the update and only read by the draw at 0x44FE19.
- The video surface persists.
- InGameUI draw 0x48EA29 only reads persistent state.
- DisplayStringManager frees only renderers unused for 60 m_frames, and the draw re-creates them.

3. INPUT, OPTION II (recommended, phase 1). Keyboard/Mouse update+createStreamMessages (and the extra keyboard call 0x6324BC) run only on g_uiTick, so input is sampled at stock 30 Hz.
- Removes rows 61, 62 and 63 and any translator audit.
- The cursor stays smooth: redraw mode 0 is the OS cursor (GD+0x9C6 defaults to 1 at 0x642E38; 0x5EE519 selects mode 0).
- Camera translators run on B with halved steps on stale input, which is an exact split-step: per-pair total equals stock.
- Never split Mouse::update from createStreamMessages (events consumed by update would be lost).
- Option I (input every render) is possible later. It needs rows 61-63 plus an audit of every MSG_RAW_MOUSE_POSITION handler for per-message integrators.

4. PATCHES MADE UNNECESSARY by B-skip:
- row 80 Eva (0x5DD98A, 0x5DD99B);
- rows 84/85 audio (0x450ABC replacement, 0x452CA3, thresholds);
- row 76 window transitions (0x5DB4B4);
- row 67 Palantir gate (it becomes implicit) and row 86 AotR counter;
- row 68 ControlBar getFrame wrap (0x71FD5D, 0x9314DE);
- row 70 message fade (0x6A2012);
- row 79 LW manager (cadence now confirmed: GameClient::update 0x6484D9 -> 0x49AAB8 -> 0x6C0E4D -> 0x6121C5);
- rows 61-63 (with Option II).
Still needed: C0-C7; rows 58/59/60 (applied on every render, because LookAt and the InGameUI camera block run on B); row 57 LW; all in-draw rows 20-25, 29-31, 35-37, 48-55, 72, 73.

5. IMPLEMENTATION: per-call-site stubs rather than a GameClient::update rewrite, so A-renders execute the original code unchanged.
- Most sites have the contiguous 5-byte pattern `8b 01 ff 50 28` -> `e8 GateVt28`. GateVt28 = `cmp byte[g_uiTick],0 / je .r / mov eax,[ecx] / jmp [eax+0x28] / .r: ret`. Same pattern for vt3C, vt40, vt6C, vt134, vt138 and a GateCall(T) for direct calls.
- Special sites: radar 0x632486 (10 bytes, keep `xor edi,edi; mov [ebp-4],edi`) and audio 0x6324F9 (11 bytes, keep the [0xDE4330] store).
- InGameUI split reuses original code: entry 0x6A1F5F jumps to 0x6A21DF and exit 0x6A23CD jumps to the epilogue 0x6A24C3 when !g_uiTick. The block reads only ESI, EDI (temp), GD and its own freshly written locals [ebp-0x24..-0x18].
- Mode switching must also halve and restore GD+0xC2C and 0xDA0B24 (and the water constants) atomically with g_m60.

6. ORDER (for C0 and for reasoning): clientUpdate =
- 0x62B385 (pending game, before m_frame++);
- m_frame++ at 0x632433..0x632440 iff GC+0xC8;
- APT;
- MP stall;
- CRC-guard begin;
- Radar;
- GameClient::update (LookAt -> LW translator -> gesture SM -> LW view -> Snow/Cloud/CloudBreak/Fire -> Anim2D -> Keyboard -> ScoredKillEva -> Eva -> Mouse -> popups -> WM -> Video -> DX queue -> freeze calc -> drawables (m_frame-gated) -> 0x62B385 -> TerrainVisual -> Display::update -> particle local player -> DRAW -> DisplayStringManager -> Shell -> 0x645DAD -> InGameUI::update -> deferred delete);
- propagate;
- input lock;
- Audio;
- Network;
- CRC-guard end.
So m_frame IS incremented before GameClient::update and the draw. The A/B latch must be taken before 0x632409. Camera changes made by InGameUI::update (after the draw) show on the next render. The commands generated by A-render propagation are consumed at the next sub-1, as in stock.

7. CORRECTIONS TO THE SYNTHESIS (section 1.2/R4):
- 0xDE7890 = TheShell (vtbl 0xC2C894, update 0x75E1D3), NOT TheInGameUI. TheInGameUI = 0xDE4830 (W3DInGameUI vtbl 0xBDD9B0, vt28 thunk 0x48EA1F -> 0x6A1F4D). The ControlBar (0x6A21B0), Palantir (0x6A23D7) and BannerUI (0x6A23EA, 0xDE3D6C) calls live in TheInGameUI::update.
- 0xDE4334 = TheKeyboard (vt28 0x4985CB -> 0x63F667, vt3C 0x63F19A); TheDisplayStringManager = 0xDE4518 (update 0x48FD6A).
- 0xDE8B34 = TheScoredKillEvaAnnouncerController (0x825A5C).
- 0xDE3B54/0xDE3C24/0xDE7734/0xDE4EFC = Snow (0x4943E1) / Cloud (0x497D00) / CloudBreak (empty 0x63F3BF) / Fire (0x49829C).
- 0xDE495C = WindowManager (0x6C15FD, which also drives TheTransitionHandler 0x5DB60A).
- 0xDF06F8 = VideoPlayer (ret).
- FUN_00782C56 = popup/message-box pumps; FUN_00532D6F = deferred D3D queue; GC vt90 = deferred drawable deletion; 0x50EB3C = mouse-gesture state machine; 0xDE4BD0 = object created by GameLogic vt3C (likely GhostObjectManager, updateOrphanedObjects(0,0) on isTick).

8. LOGIC SAFETY:
- No B-run function calls GameLogic::update or the logic RNG (0xDA1CA4) in the paths read.
- CRT rand() users are client/W3D/audio only (0xB25D60 snow, 0x600ADE weather setup, W3D lib, game-setup 0x801AAF/0x644CC2).
- B-render use of the client RNG or CRT rand therefore cannot change logic (medium confidence; confirm with the getCRC check).
- Built-in instrument for M2/R2: GameLogic::getCRC 0x625886, already wrapped around clientUpdate by the debug guard 0x6CF64E/0x6CF681 (flag 0xDE87C5). In test builds call it before and after every B-render and assert equality.

9. PERFORMANCE BONUS (R3): B-renders skip APT, audio, input, UI, the radar, the drawable block and particle updates, so B-frames are much cheaper than A-frames. Deadline pacing (C2) should let B-frames absorb A-frame overruns.

## Items

### CU-01 — GameLogic pending start-new-game / load transition  [run_on_B_as_is, high]
- **Site:** 0x63241E call 0x62B385 (ECX=TheGameLogic); second call 0x648817
- **What:** Returns at once unless GameLogic+0x9C is set (decompile: `if (*(char*)(this+0x9c)=='\0') goto end`). When set, starts or transitions the game with fades and Sleep loops, and calls vt5C serviceWindowsOS.
- **Cadence:** event (flag-gated), called twice per render in stock
- **Reason:** Event-driven and idempotent when the flag is clear. Stock already calls it from client code on any render, so a B-render call behaves the same as a stock call.
- **Risk if wrong:** Gating it only delays a game start by one render. No speed or logic effect.

### CU-02 — Client frame counter m_frame (GC+0x10)  [n/a, high]
- **Site:** 0x632423..0x632440 (`mov al,[ecx+0xC8]; test; je; call [esi+0x7C]; inc eax; push eax; call [esi+0x38]`)
- **What:** m_frame++ only if GC+0xC8 was set by the previous stepper call. This runs BEFORE APT, Radar, GameClient::update and the draw, but AFTER 0x62B385.
- **Cadence:** per render if C8 (30 Hz under model H)
- **Reason:** Core mechanism. B-frames clear C8, so a B-render leaves m_frame unchanged. The C0 hook at 0x6325CF must latch C8 (A/B, g_uiTick) before this read.
- **Risk if wrong:** If the A/B latch were taken after 0x632429, every gate below would be misclassified.

### CU-03 — APT player (shell, Palantir HUD, BannerUI movies, embedded APT game windows)  [skip_on_B, medium]
- **Site:** 0x63244B TheAptPlayer(0xDE3F0C) vt28 = 0x624EE1 (vtbl 0xBDB780)
- **What:** dt = timeGetTime delta, clamped to 60 ms (min 34 ms once after load via +0x328), fed into the 0xAE3150 fixed-step accumulator 0xAE18C0 (advances while acc >= movie frame ms). Also 0x814218 calls vt14 on every embedded APT game window in map 0xDE8A28 ('BinkGameWindow', 'LivingWorldMap'), 0x624514, and the mouse-hover handler 0x624C68.
- **Cadence:** T (real time) for APT frames; per call for the embedded-window vt14 and hover
- **Reason:** APT frames are discrete and real-time based, so 60 Hz calls add nothing visible. Skipping keeps the per-call work (embedded Bink/LW windows of unknown cadence, hover) at stock 30 Hz. The draw (vt30 0x46203E) renders the persistent display list; in stock the accumulator often advances 0 frames in a call, so the draw cannot depend on a per-call update.
- **Risk if wrong:** If run on B: an embedded window that steps per call would run 2x. If skipped wrongly: none, since time accumulates.
- **Fix @ 0x632449:** e8 <GateVt28>. Stub: cmp byte[g_uiTick],0; je ret; mov eax,[ecx]; jmp [eax+0x28]; ret (orig: 8b 01 ff 50 28 (mov eax,[ecx]; call [eax+0x28]))

### CU-04 — MP stall/skip helper  [n/a, high]
- **Site:** 0x632450 call 0x63239D
- **What:** Returns 0 when TheNetwork(0xDE4468)==NULL. Otherwise it may skip the client update (0xDE4304++, 0xDE4308=frame).
- **Cadence:** per render
- **Reason:** Multiplayer only; 60 mode is off in MP.
- **Risk if wrong:** none in SP

### CU-05 — Debug logic-CRC guard around the client update  [run_on_B_as_is, high]
- **Site:** 0x632478 FUN_006CF64E / 0x63251C FUN_006CF681
- **What:** If debug flag 0xDE87C5 is set (no writers found, so off in retail), takes GameLogic::getCRC 0x625886 before and after the client update. In MP it reports 'GameLogic changed outside of GameLogic::update' (UTF-16 0xC18480).
- **Cadence:** per render (when the flag is on)
- **Reason:** Debug only. Calling 0x625886 from the DLL before/after each B-render is a ready-made R2 check (client code mutating logic).
- **Risk if wrong:** none

### CU-06 — TheRadar  [skip_on_B, high]
- **Site:** 0x63248D [[0xDE4AB8]+4] vt28 = Radar::update 0x6D8E2B
- **What:** Sets radar+0x11=1 ('updated since reset'; W3DRadar draw 0x44FE19 only reads it, never clears it). Kills radar events when getFrame() > dieFrame. Calls vt10(TheTerrainLogic) when logicFrame - [+0x145C] > LTR*3.
- **Cadence:** M/L (idempotent within one m_frame)
- **Reason:** No per-render effect; the draw reads only the persistent flag. %6 icon refresh happens in the draw.
- **Risk if wrong:** none either way
- **Fix @ 0x632486:** 33 ff 89 7d fc e8 <GateVt28> (keep xor edi,edi and mov [ebp-4],edi) (orig: 8b 01 33 ff 89 7d fc ff 50 28)

### CU-07 — MessageStream::propagateMessages  [run_on_B_as_is, high]
- **Site:** 0x6324A1 call 0x7128C3 (ECX=TheMessageStream 0xDE6398)
- **What:** For each translator and each queued message, calls translate. Then appends the remainder to TheCommandList (0x711034) and clears. No per-call work when the stream is empty.
- **Cadence:** E (per message)
- **Reason:** Purely event-driven. With input devices skipped on B (Option II) the stream is empty on B-renders. Anything that a B-run component appends is handled promptly, as stock would.
- **Risk if wrong:** If Option I (input on B) is chosen, per-position-message integrators in translators (e.g. edge rotate 0x83AC4A case 3) run 2x and need the row 61 fixes plus an audit.

### CU-08 — UI-sequence input lock (queue 0xDE8900, intro/transition sequences)  [run_on_B_as_is, high]
- **Site:** 0x6324A6 call 0x80000F; 0x6324D2/0x6324DC InGameUI 0x69B5C7; 0x6324EE Mouse 0x5EDCF7
- **What:** 0x7FFFAC evaluates a timeGetTime-based sequence and returns flags&5. When bit0 changes vs [0xDE4330], it toggles InGameUI input-enable (+0x15 via vt148) and Mouse input-enable (+0x4F9D via vt70).
- **Cadence:** T + E
- **Reason:** Real-time, event-driven, and empty in game.
- **Risk if wrong:** none

### CU-09 — Keyboard::update 0x63F667 (second call during UI sequences)  [skip_on_B, high]
- **Site:** 0x6324BC TheKeyboard(0xDE4334) vt28 (extra call when 0x80000F bit0)
- **What:** Input frame++, buffered key read, auto-repeat (row 62).
- **Cadence:** per call
- **Reason:** Part of the input group (Option II).
- **Risk if wrong:** Only active during sequences; 2x auto-repeat there.
- **Fix @ 0x6324BA:** e8 <GateVt28> (orig: 8b 01 ff 50 28)

### CU-10 — AudioManager::update  [skip_on_B, high]
- **Site:** 0x632501 TheAudio(0xDE42FC) vt28 = 0x461DA6 (Miles vtbl 0xBDB550)
- **What:** dt = vt1B4 getElapsedMs 0x450ABC (m_frame delta x 33.33, with a type-2 fallback when dt<1), fed to 0x4519FC(dt) (twice when not paused). Also isTick -> +0x6A9, request processing, the fade/stream lists 0x45D7BA..0x45AE1E, and 0x4518CE x3.
- **Cadence:** per render, with dt in m_frame units
- **Reason:** Once per A-render, dt = 1 m_frame and the fallback never fires, which is exactly stock. This removes the row 84 patch (replacing 0x450ABC, 0x452CA3) and the row 85 thresholds. Requests queued during a B-render wait at most 16.7 ms. The listener updates at 30 Hz as in stock.
- **Risk if wrong:** If run on B unpatched: type-2 fallback fires 2x, giving 2x fades/delays.
- **Fix @ 0x6324F9:** 89 35 30 43 de 00 e8 <GateVt28> (keep the [0xDE4330]=esi store) (orig: 8b 01 89 35 30 43 de 00 ff 50 28)

### CU-11 — Network  [n/a, high]
- **Site:** 0x632512 TheNetwork vt3C(0)
- **What:** MP network update
- **Cadence:** per render
- **Reason:** MP only (TheNetwork NULL in SP)
- **Risk if wrong:** none

### G-01 — Camera input per-frame (RMB/key/edge scroll, key rotate, key zoom, replay camera)  [run_on_B_with_fix, high]
- **Site:** 0x6484B2 call 0x83B471 (ECX=[0xDE8CB0] LookAtTranslator)
- **What:** Computes the scroll offset (RMB drag, keys x350/100, edge with real-time ramp) and calls InGameUI vtB4 setScrollAmount and TacticalView vt5C scrollBy (or LW view vt2C). Key rotate +0x154/+0x155 via setAngle(getAngle ± GD+0xC2C). Key zoom +0x156/+0x157 via vt134/vt138. When GD+0xB70 && isTick (0x63252F) it appends replay-camera message 0x447.
- **Cadence:** per render (R); replay message once per tick
- **Reason:** Applies the camera directly, so A-only would give 30 Hz camera judder. Under Option II the translator/mouse state is stale on B, so a halved step on both renders replays the A input exactly and the per-pair total equals stock. The replay message stays A-only via isTick (s even on B).
- **Risk if wrong:** Unfixed: 2x scroll/rotate/zoom speed. A-only: judder.
- **Fix @ 0x83B8A0:** call stub that multiplies [ebp-0x10],[ebp-0xC] by 0.5 in 60 mode, then pushes and calls vt5C (synthesis row 58) (orig: 8d 55 f0 52 ff 50 5c (lea edx,[ebp-0x10]; push edx; call [eax+0x5C]))
- **Fix @ 0x83B9DB / 0x83B9F2:** e8 <GateVt134>/<GateVt138> 90 90 90 (zoom keys only on g_uiTick; the wheel path is unaffected; height settle smooths per render) (orig: 8b 01 ff 90 34 01 00 00 / 8b 01 ff 90 38 01 00 00)
- **Fix @ GD+0xC2C (KeyboardCameraRotateSpeed; parser 0xC00864):** x0.5 while in 60 mode. Shared by LookAt 0x83B984/0x83B9B6, InGameUI 0x6A2205/0x6A222D and LW 0x839641/0x839674, all of which run every render. Restore when leaving 60 mode. (orig: INI value)

### G-02 — Living World map camera input per-frame  [run_on_B_with_fix, medium]
- **Site:** 0x6484BD call 0x8392A7 (ECX=[0xDE8CAC] LW translator)
- **What:** Only when TheLivingWorldLogic exists and the game is not paused. Computes pan (RMB/key x[0xC53B00]/edge) and rotation, calls LW view vt2C 0x49B799(offset, rot) (pos += 0.5*offset*zoom). Key rotate GD+0xC2C*±15. Key zoom vt54(±1) per call.
- **Cadence:** per render (R)
- **Reason:** Camera smoothness in LW. Same split-step argument as G-01.
- **Risk if wrong:** 2x LW pan/rotate/zoom if unfixed
- **Fix @ 0x49B7AD (movss [0xBD869C]=0.5) and 0x49B8D9:** 0.25 / halve the rotation (synthesis row 57). vt2C is also called from LookAt when the LW view is active; that caller also runs every render, so the fix is consistent. (orig: operands of LW vt2C)
- **Fix @ end of 0x8392A7: call [eax+0x54] (LW zoom ±1):** gate with g_uiTick (orig: per-call zoom step)

### G-03 — Mouse-button gesture state machine  [run_on_B_as_is, high]
- **Site:** 0x6484C8 call 0x50EB3C (ECX=[0xDE8CC8], translator created at 0x83E25D)
- **What:** Calls [[this+8]]->vt4. The states (vtbls 0xC53D80/8C/98/A4/B0) switch only when TheMouse button state +0x4F24/+0x4F30 changes. One exit path appends a message (0x83DE6B).
- **Cadence:** per render, state-change driven
- **Reason:** No counters; it only reacts to mouse state, which changes only on A under Option II.
- **Risk if wrong:** none

### G-04 — Living World view update -> 0x6C0E4D -> TheLivingWorldManager(0xDE3C08) vt28 0x6121C5 (+TheAptPalantir vt28 inside), LW objects 0x6C0BD2/0x6C038B, LW camera fly 0x6BF78E  [split_step, medium]
- **Site:** 0x6484D9 [0xDE4958] vt6C = 0x49AAB8 (LW view, vtbl 0xBDE918)
- **What:** Returns at once if paused. Calls 0x6C0E4D, which only acts if view+0x18 (LW active). Then per call: fade +=/-= [0xDCB840] (mode 2/3), and angle += vel; vel *= [LWMgr+0x1DC] (inertia damping).
- **Cadence:** per render (R); confirms synthesis row 79 (cadence was 'R?')
- **Reason:** The LW manager, object and Palantir updates are per-call timers and must keep 30 Hz. The camera fade, inertia and fly are camera visuals. Phase 1: gate the whole vt6C call with g_uiTick (exact speed; LW camera inertia/fly in 30 Hz steps; pan stays smooth via G-02). Phase 2: gate only 0x6C0E64/0x6C0E6D/0x6C0E78 and convert the in-function integrators (fade step/2, damping sqrt, halve fly step).
- **Risk if wrong:** Ungated: LW campaign timers, countdowns and Palantir run 2x.
- **Fix @ 0x6484D7:** e8 <GateVt6C> (phase 1) (orig: 8b 01 ff 50 6c)
- **Fix @ 0x6C0E78 (phase 2):** e8 <GateVt28>; also gate the calls at 0x6C0E66 (0x6C0BD2) and 0x6C0E6D (0x6C038B) (orig: 8b 01 ff 50 28 (LWManager vt28))

### G-05 — LW UI state machines (0x83881D->0x8385A3, 0x838D1C->0x838C8A)  [skip_on_B, medium]
- **Site:** 0x6484E2 call 0x645750 (ECX=GC+0x13C)
- **What:** State machines keyed on TheLivingWorldLogic(0xDE4950) state; they call sub-object vt0/vt4 steps.
- **Cadence:** per render, state driven
- **Reason:** Safe default: no visual need, and the sub-object step cadence is unknown.
- **Risk if wrong:** Minimal (≤16 ms latency).
- **Fix @ 0x6484E2:** e8 <GateCall_0x645750> (orig: e8 69 d2 ff ff (call 0x645750))

### G-06 — Intro/startup sequence registration (functors 0x645B8D, 0x64838D, 0x645BDF, 0x64576D pushed via 0x8000AA into 0xDE8900)  [n/a, high]
- **Site:** 0x6484E7..0x64857C (DAT_00D9F6FC one-shot)
- **What:** Runs once at the first client update.
- **Cadence:** once
- **Reason:** Startup only
- **Risk if wrong:** none

### G-07 — Snow / weather manager  [split_step, high]
- **Site:** 0x648591 TheSnowManager(0xDE3B54) vt28 = 0x4943E1 (vtbl 0xBDE06C)
- **What:** time += (Sync 0xDD1E0C - PrevSync 0xDD1E10)*0.001 and wraps it; the wind/scroll offsets +0x94/+0x98/+0x9C get += dtime*k. Then 0x49405A: weather transition, counter +0x58 -= 1 per call, density 0x493563. Then 0x492C35: lightning chance per call using CRT rand() via 0xB25D60, and a flash counter +0x54-- per call.
- **Cadence:** time part: W3D-clock delta per call. 0x49405A/0x492C35: per call.
- **Reason:** NEW HAZARD CLASS: this system reads PrevSync, which the draw (WW3D::Sync 0x516E20) refreshes every render. If skipped on B, the B-step of sync is lost and snow moves at HALF speed under C3; if run whole, transitions and lightning run 2x. It is the only PrevSync reader outside the draw (refs: 0x48BCF2, 0x4943E1, 0x4A89BD, 0x4BF560, 0x516E20, 0x5A1D60).
- **Risk if wrong:** Half-speed snow (if skipped), or 2x lightning and weather transitions (if fully run).
- **Fix @ 0x49448D:** e8 <GateCall_0x49405A> (only on g_uiTick; ECX=esi preserved) (orig: e8 c8 fb ff ff (call 0x49405A))
- **Fix @ 0x494494:** e8 <GateCall_0x492C35> (orig: e8 9c e7 ff ff (call 0x492C35))

### G-08 — Cloud shadow effect manager  [skip_on_B, high]
- **Site:** 0x6485A0 TheCloudEffectManager(0xDE3C24) vt28 = 0x497D00
- **What:** Acts only when getFrame() != +0x100 (last m_frame); global-weather state sync.
- **Cadence:** M
- **Reason:** It is a no-op on B anyway (m_frame unchanged); gating saves time.
- **Risk if wrong:** none
- **Fix @ 0x64859E:** e8 <GateVt28> (orig: 8b 01 ff 50 28)

### G-09 — Cloud-break effects  [n/a, high]
- **Site:** 0x6485AF TheCloudBreakEffectManager(0xDE7734) vt28
- **What:** vt28 = 0x63F3BF = `ret` (vtbl 0xBDE1FC)
- **Cadence:** none
- **Reason:** Empty
- **Risk if wrong:** none

### G-10 — Ambient fire/burn manager  [skip_on_B, high]
- **Site:** 0x6485BE TheFireManager(0xDE4EFC) vt28 = 0x49829C
- **What:** When getFrame() > next (+0x64): next = m_frame + client-RNG interval (0x6D343B), and spawns fire FX at positions using client RNG 0x6D33AB.
- **Cadence:** M interval (idempotent within one m_frame)
- **Reason:** No-op on B; only the client RNG is involved.
- **Risk if wrong:** none
- **Fix @ 0x6485BC:** e8 <GateVt28> (orig: 8b 01 ff 50 28)

### G-11 — 2D icon animations  [skip_on_B, high]
- **Site:** 0x6485C9 TheAnim2DCollection(0xDE4AB0) vt28 = 0x6D82BD
- **What:** For each animation not flagged (+0x10&1), calls 0x6D7F0A (m_frame delta vs logic-parsed delay).
- **Cadence:** M
- **Reason:** Idempotent within one m_frame; icons are drawn from the persistent frame index.
- **Risk if wrong:** none
- **Fix @ 0x6485C7:** e8 <GateVt28> (orig: 8b 01 ff 50 28)

### G-12 — Keyboard input  [skip_on_B, high]
- **Site:** 0x6485D8 TheKeyboard(0xDE4334) vt28 = 0x63F667 (thunk 0x4985CB); 0x6485E3 vt3C = 0x63F19A createStreamMessages
- **What:** Input frame +0xE1C++, buffered DirectInput read, auto-repeat (first repeat after 10 frames, then every frame: row 62). Messages go to TheMessageStream.
- **Cadence:** per render (R)
- **Reason:** Option II (input sampled at 30 Hz, stock). No fixes needed for auto-repeat. Camera key flags persist (held-key state), so B-render camera code replays them.
- **Risk if wrong:** If run on B: 2x auto-repeat (needs 0x63F499 0A->14, 0x63F4D6 0C->13).
- **Fix @ 0x6485D6:** e8 <GateVt28> (orig: 8b 01 ff 50 28)
- **Fix @ 0x6485E1:** e8 <GateVt3C> (orig: 8b 01 ff 50 3c)

### G-13 — TheScoredKillEvaAnnouncerController (name string 0xBFEA70, registered 0x63C710)  [skip_on_B, medium]
- **Site:** 0x6485EE [0xDE8B34] vt28 = 0x825A5C (vtbl 0xC51604)
- **What:** For each entry, 0x825995 compares the local player's kill tallies and fires an Eva event (0x5DD9EE) when the threshold is reached.
- **Cadence:** per render, logic-data driven
- **Reason:** Event detection only; announcements may come at most 16.7 ms later.
- **Risk if wrong:** none
- **Fix @ 0x6485EC:** e8 <GateVt28> (orig: 8b 01 ff 50 28)

### G-14 — Eva announcer  [skip_on_B, high]
- **Site:** 0x6485F9 TheEva(0xDE3670) vt28 = 0x5DD86A (slot 0xBF2758)
- **What:** Per call, every cooldown pair: subss [0xD9F620]=33.33 at 0x5DD986 and 0x5DD997.
- **Cadence:** per render, fixed 33.33 ms per call
- **Reason:** Gives stock-exact timing once per A-render. Removes the synthesis row 80 patch (0x5DD98A/0x5DD99B disp32).
- **Risk if wrong:** If run on B: 2x Eva cooldowns
- **Fix @ 0x6485F7:** e8 <GateVt28> (orig: 8b 01 ff 50 28)

### G-15 — Mouse input  [skip_on_B, high]
- **Site:** 0x648608 TheMouse(0xDE36E0) vt28 = 0x44136B -> 0x5ED573; 0x648613 vt40 = 0x5EDD69 createStreamMessages
- **What:** update: inputFrame +0x4F94++, then updateMouseData 0x5ED3EA (consumes the WndProc event ring, updates pos +0x4F0C/+0x4F10). createStreamMessages: raw position msg every call (0x5EE17B); held counters +0x5000/+0x5004 ++ and fire at 5 (0x5EE0BD..0x5EE12F).
- **Cadence:** per render (R)
- **Reason:** Option II: no held-counter or edge-rotate fixes needed. The cursor stays smooth: the Mouse ctor 0x5EE519 selects redraw mode 0 (OS cursor) when GD+0x9C6 != 0, and the GlobalData ctor sets GD+0x9C6=1 (0x642E38). Only in-game feedback (drag box, ghost, hover) updates at 30 Hz, as in stock. Never split update from createStreamMessages: events read by update but not turned into messages would be lost.
- **Risk if wrong:** If AotR forces a W3D/poly cursor (mode 1/2) the cursor moves at 30 Hz. If run on B unfixed: 2x held-click, 2x edge rotate.
- **Fix @ 0x648606:** e8 <GateVt28> (orig: 8b 01 ff 50 28)
- **Fix @ 0x648611:** e8 <GateVt40> (orig: 8b 01 ff 50 40)

### G-16 — Popup / message-box UI pumps (0xDE8A9C MessageBox, 0xDEBA1C via 0x9534A0->0x9532F3; 0xDE9ED0/0xDEA110 vt14)  [skip_on_B, medium]
- **Site:** 0x648616 call 0x782C56
- **What:** State machines that show or close APT message boxes.
- **Cadence:** per render, state driven
- **Reason:** No visual need; safe default.
- **Risk if wrong:** none
- **Fix @ 0x648616:** e8 <GateCall_0x782C56> (orig: e8 3b a6 13 00 (call 0x782C56))

### G-17 — GameWindowManager::update  [skip_on_B, high]
- **Site:** 0x648642 TheWindowManager(0xDE495C) vt28 = 0x6C15FD
- **What:** processDestroyList 0x6C1478 (windows were already unlinked when destroyed), then TheTransitionHandler(0xDE3654) vt28 = 0x5DB60A (window transitions: frame += dir per call, row 76).
- **Cadence:** per render (R)
- **Reason:** Transitions keep stock speed (30 Hz steps), removing the row 76 patch (0x5DB4B4). The deferred destroy runs one render later, which is harmless.
- **Risk if wrong:** If run on B: 2x GUI transitions
- **Fix @ 0x648640:** e8 <GateVt28> (orig: 8b 01 ff 50 28)

### G-18 — VideoPlayer  [n/a, high]
- **Site:** 0x64864D TheVideoPlayer(0xDF06F8) vt28 = 0x49017B -> jmp 0xA88AC0 (ret)
- **What:** Empty
- **Cadence:** none
- **Reason:** No-op
- **Risk if wrong:** none

### G-19 — Deferred DirectX work queue (vector 0xDD83E8..0xDD83EC, taken under the DX mutex 0x51EEC0 'A thread held onto DirectX...')  [run_on_B_as_is, high]
- **Site:** 0x648650 call 0x532D6F
- **What:** Executes and clears queued operations (vt8) posted by other threads. Also called from loading code.
- **Cadence:** E (queue)
- **Reason:** Resource servicing; frequency-independent. Running every render frees and creates D3D resources promptly.
- **Risk if wrong:** none

### G-20 — Debug hitch + freeze computation + local player index  [run_on_B_as_is, high]
- **Site:** 0x64865A..0x648690 (GD+0xC78) and 0x64869D..0x6486E9
- **What:** Busy wait when debug hitch > 0. freeze = (TacticalView vtD8 && !vt78) || ScriptEngine 0x441E23/0x603418 || isGamePaused 0x90F92C.
- **Cadence:** per render; pure queries
- **Reason:** Pure; needed to compute the gate of the drawable block.
- **Risk if wrong:** none

### G-21 — Drawable client updates: GhostObjectManager (0xDE4BD0 created by GameLogic vt3C), shroud status (isTick), Drawable::updateDrawable, TheAnimationSoundModuleManager, 0x64594B (objects following Drawable::getInterpolatedPosition 0x676711)  [skip_on_B, high]
- **Site:** 0x648705..0x64880C drawable block: 0x64875A [0xDE4BD0] vt18(0,0); 0x648786 0x68D8F7; 0x6487CC 0x678F54; 0x6487D6 0x675996; 0x6487EA [0xDE8D68] vt28 0x83F321; 0x64880C 0x64594B
- **What:** Runs only if !freeze && static 0xD9F6F8 != m_frame && !GameLogic+0x125.
- **Cadence:** M (once per m_frame)
- **Reason:** Already gated by m_frame: B-renders skip it automatically, with no patch needed. Paused-state proof: stock draws drawables without this block.
- **Risk if wrong:** none (no patch)

### G-22 — W3DTerrainVisual::update -> water 0x500137, envelope evaluator 0x50B85D via 0xDD1D18->vt4  [run_on_B_with_fix, medium]
- **Site:** 0x648835 [[0xDE4AC8]+4] vt28 = 0x4916D9 (TerrainVisual, vtbls 0xBDDFC8/0xBDDF90)
- **What:** Water: dt = timeGetTime delta (own static 0xDD1CA0; fixed [0xBE560C] if GD+0xD45) passed to each water object vt4(dt) (lists +0x108/+0x118). River UV += [0xBE5608], [0xBE5604] per call (row 21). Envelopes are a pure function of m_frame.
- **Cadence:** T for the dt lists, per call for the UV
- **Reason:** Water animation is a large smooth visual. The dt lists are real-time correct; only the per-call UV constants need halving. Phase-1 fallback: skip_on_B (exact, water steps at 30 Hz).
- **Risk if wrong:** Unfixed on B: 2x river UV scroll.
- **Fix @ 0xBE5608 / 0xBE5604 (.rdata, single use per W3D-10):** 4.125e-5 / 8.25e-5 in 60 mode (VirtualProtect); restore on exit (orig: 8.25e-5 / 1.65e-4)

### G-23 — Display::update 0x65C1EA (video stream service) + time-of-day 0x442E56  [skip_on_B, medium]
- **Site:** 0x648840 TheDisplay(0xDE4418) vt28 = W3DDisplay::update 0x4430A7; loading path 0x64884B call 0x65C1EA
- **What:** Video: stream vt18(surface) -> 'frame ready' flags; 0x65C0A1 computes the rect; at end of stream it loops or stops with a per-call hold counter +0x110 (shared with the screen flash) and timeGetTime holds. Time-of-day lerp is a pure function of m_frame (0x442E56).
- **Cadence:** T (video) / per call (+0x110) / M
- **Reason:** The video surface persists and is drawn every render by drawFrame (Display+0x38 path). Video is serviced at 30 Hz, as in stock.
- **Risk if wrong:** If run on B: the end-of-stream hold +0x110 counts 2x (minor).
- **Fix @ 0x64883E:** e8 <GateVt28> (orig: 8b 01 ff 50 28)
- **Fix @ 0x64884B (only if 60 mode can be active while GameLogic+0x125):** e8 <GateCall_0x65C1EA> (orig: e8 9a 39 01 00)

### G-24 — FX particle manager local player  [n/a, high]
- **Site:** 0x64885E [0xDE3744]+0x64 = localPlayerIndex
- **What:** Store, only when the block ran (local_5==0)
- **Cadence:** M
- **Reason:** Already A-only
- **Risk if wrong:** none

### G-25 — THE RENDER (core of the allow-list)  [run_on_B_with_fix, high]
- **Site:** 0x648869 TheDisplay vt30 = W3DDisplay::draw 0x44B788 -> drawFrame 0x449CF8
- **What:** Sync clock (C3), updateViews (camera 0x48BCF2), FX particles 0x5F5123 (C7), tree buffer 0x4684FB, shroud (real time), shadow and water RTs, RenderViews, screen overlays 0x65D03B (pure draw), screen flash 0x65CF05, RenderUI (InGameUI draw 0x48EA29 incl. world anim preDraw 0x69DEA9, floating text; APT draw 0x46203E), UI particles 0x6A536C, mouse draw 0x498CBC, Present.
- **Cadence:** per render
- **Reason:** Must run every render. Needs C3/C6/C7 and the in-draw integrator fixes from the synthesis (rows 20-25, 29-31, 35-37, 48-55, 72, 73). NEW: 0x65CF05 is a per-draw integrator (hold +0x110-- then alpha +0x120 -= rate +0x11C, both per draw); its only setter found is 0x65C476, called only from intro functor 0x645BDF.
- **Risk if wrong:** If skipped: no 60 FPS. If unfixed: the listed per-draw integrators run 2x.
- **Fix @ 0x65CF05 (decrement blocks):** Low priority (shell only). If 60 mode is ever enabled in the shell, gate the decrement with g_uiTick but always draw the overlay (0x65C5D3). (orig: +0x110 decrement / +0x120 -= +0x11C per call)

### G-26 — W3DDisplayStringManager::update  [skip_on_B, high]
- **Site:** 0x648874 TheDisplayStringManager(0xDE4518) vt28 = 0x48FD6A (vtbl 0xBDDD48)
- **What:** Scans 10 strings per call and frees renderers not drawn for more than 60 m_frames (string+500 = draw frame).
- **Cadence:** per call, m_frame expiry
- **Reason:** The draw recreates freed renderers on demand; expiry is in m_frame units.
- **Risk if wrong:** none
- **Fix @ 0x648872:** e8 <GateVt28> (orig: 8b 01 ff 50 28)

### G-27 — Shell (screen stack, shell-map LOD)  [skip_on_B, high]
- **Site:** 0x648893 TheShell(0xDE7890, vtbl 0xC2C894) vt28 = 0x75E1D3 (only if !GameLogic+0x125 || shell+0x5C)
- **What:** Screen updates throttled by real time at 32.33 ms (0xC2C90C), shell-map low-LOD handling, 0x75D9CA/0x75DF26.
- **Cadence:** T-throttled + per call
- **Reason:** Safe default; in game it only services shell screens.
- **Risk if wrong:** none
- **Fix @ 0x648891:** e8 <GateVt28> (orig: 8b 01 ff 50 28)

### G-28 — Pending display-mode/resolution change  [run_on_B_as_is, high]
- **Site:** 0x6488A1 call 0x645DAD (if GC+0xC9)
- **What:** Deletes the shell, applies the resolution, re-inits
- **Cadence:** event
- **Reason:** Event
- **Risk if wrong:** none

### G-29 — InGameUI::update and children: UI messages fade (ftol 0x6A2012), military subtitles 0x69CE5F, text effects +0x7F4 vt28 0x8E9DB2, money display, floating text 0x69DB40, ControlBar 0xDE7744 vt28 0x71FC08, vt1D0 0x69BAA8, list +0x18 vt8, KEYBOARD CAMERA BLOCK 0x6A21DF..0x6A23CD (rotate vt100/vtFC ± GD+0xC2C, zoom vt134/vt138, scroll 250.0@0xDA0B24 -> vt5C), vtD4 0x6A5723, TheAptPalantir 0xDE4A70 vt28 (AotR counter 0xED0800), TheBannerUI 0xDE3D6C vt28 0x6164D7, 0x822A35/0x82326D, +0x53C vtBC/vtDC, 0x6A1BB7, 0x69B1C3, 0x8EB9B4, deselect-on-shroud when getFrame()%CLIENT_FPS==0  [split_step, medium]
- **Site:** 0x6488AE TheInGameUI(0xDE4830) vt28 = 0x48EA1F -> 0x6A1F4D (W3DInGameUI vtbl 0xBDD9B0)
- **What:** Mixed: per-call integrators (message fade, Palantir, ControlBar flash), m_frame-pure UI state, and the per-render camera-key application. It runs AFTER the draw.
- **Cadence:** per render (R)
- **Reason:** Full update on g_uiTick: the stock cadence makes rows 67, 68, 70 and 86 unnecessary. On other renders only the camera-key block runs, reusing the original code with halved constants, so key scrolling and rotation stay smooth and per-pair totals equal stock. Draw 0x48EA29 consumes nothing produced only by update.
- **Risk if wrong:** Whole update on B: 2x Palantir/ControlBar/message fades. Whole skip on B: key-scroll/rotate camera judders at 30 Hz.
- **Fix @ 0x6A1F5F:** e8 <IGUI_Entry> 90 90 90. IGUI_Entry: xor edi,edi; if !g_uiTick {add esp,4; jmp 0x6A21DF}; cmp dword[esi+0x5C4],edi; ret (flags survive ret) (orig: 33 ff 39 be c4 05 00 00 (xor edi,edi; cmp [esi+0x5C4],edi))
- **Fix @ 0x6A23CD:** e8 <IGUI_Exit> 90*5. IGUI_Exit: if !g_uiTick {add esp,4; jmp 0x6A24C3 (epilogue)}; mov eax,[esi]; mov ecx,esi; jmp [eax+0xD4] (orig: 8b 06 8b ce ff 90 d4 00 00 00 (mov eax,[esi]; mov ecx,esi; call [eax+0xD4]))
- **Fix @ 0x6A225C / 0x6A2279:** e8 <GateVt134>/<GateVt138> 90 90 90 (zoom keys only on g_uiTick) (orig: 8b 01 ff 90 34 01 00 00 / 8b 01 ff 90 38 01 00 00)
- **Fix @ 0xDA0B24 (.data float, only user 0x6A2291):** 125.0 in 60 mode; restore on exit (orig: 250.0)

### G-30 — Deferred drawable deletion (destroyDrawable 0x6464F6 pushes into GC+0xE4)  [run_on_B_as_is, high]
- **Site:** 0x6488B5 GameClient vt90 = 0x645F65
- **What:** Deletes the queued drawables (vt1C(0) + free) and clears the list (0x6459CB).
- **Cadence:** E (queue)
- **Reason:** Frequency-independent cleanup; preserves the stock invariant 'flush at end of every client update'.
- **Risk if wrong:** none (if skipped: memory freed one render later)

### G-31 — Intro movie path  [n/a, high]
- **Site:** 0x6488BD..0x6488D0 (GD+0xAF2/0xAF3 intro branch: Display vt30 + vt28)
- **What:** Draw + display update only
- **Cadence:** shell only
- **Reason:** 60 mode is off in the shell
- **Risk if wrong:** none

### X-01 — Asset streaming / OS message pump  [not_on_B_path, medium]
- **Site:** Outside clientUpdate: GameEngine::update 0x6325A8 [0xDEF548] vt28 (asset streaming), 0x6325B9/0x6325C4 script-debug hooks; Win32GameEngine::update tail vt5C serviceWindowsOS
- **What:** Runs every loop iteration, before or after clientUpdate.
- **Cadence:** per loop iteration (60/s in 60 mode)
- **Reason:** Not part of clientUpdate. More frequent streaming and message pumping is harmless, and their only effect is lower latency.
- **Risk if wrong:** none known

## Open questions

- Option I (input every render) needs an audit of all message translators (priorities 0..0x3B9AC9FF attached in GameClient::init 0x646955..0x646C91) for per-MSG_RAW_MOUSE_POSITION integrators. Option II avoids this; decide whether 30 Hz drag-box/placement-ghost/hover feedback is acceptable.
- Does AotR (INI/options/command line) ever set GD+0x9C6=0 (W3D cursor, redraw mode 1) or call setRedrawMode 0x499A56 with mode 1/2? If so, Option II gives a 30 Hz cursor. Fix: a B-render-only cursor position refresh (write GetCursorPos into Mouse+0x4F0C/+0x4F10 before the draw), or Option I.
- Water objects in water+0x108/+0x118 lists (vt4(dt) from 0x500137): are they all linear in real dt (no per-call exponential steps)? Needed before enabling G-22 run_on_B_with_fix; otherwise use skip_on_B.
- LW split (G-04 phase 2): per-call behaviour of 0x6C0BD2 (0x7FC09D per object), 0x6C038B (vt1C per object), 0x6BF78E camera fly, and the angle damping [LWManager+0x1DC]. Phase 1 gates all of vt6C. Also: are LW strategic army movements simulated in TheLivingWorldManager::update (per render) rather than TheLivingWorldLogic (sub-1)? If so, ungated stock 60 FPS would already double the LW campaign speed.
- APT embedded game windows in map 0xDE8A28 ('BinkGameWindow', 'LivingWorldMap'), vt14 called per APT update via 0x814218: cadence unknown. B-skip keeps them stock; verify the LW map window does not need per-render camera updates for smoothness in LW mode.
- InGameUI split jumps (0x6A1F5F -> 0x6A21DF and 0x6A23CD -> 0x6A24C3): verify at runtime that the stack depth equals the normal path (all intermediate calls are balanced cdecl/thiscall) and that no register other than ESI is live into 0x6A21DF.
- GameLogic+0x125 semantics (loading/scene-loading?). It selects 0x65C1EA instead of TerrainVisual+Display update and disables drawFrame updateViews. Should 60 mode (and the gates) be forced off while it is set?
- Screen flash 0x65CF05 (per-draw hold and fade integrator): only setter found is 0x65C476 from intro functor 0x645BDF. Confirm there is no script or FX path that sets Display+0x110/+0x11C/+0x120 in game (Display+0x110 is also decremented in Display::update 0x65C1EA at end of video).
- 0x62B385 starting a new game on a B-render leaves s even. Confirm the C1 mode controller and stepper re-sync on map load. Core says GameEngine reset 0x635D11 does not reset +0x34, which conflicts with synthesis 2.3.
- FUN_0064594B (in the m_frame-gated block) feeds objects (0x7644EC, probably 3D-sound or effect followers) with Drawable::getInterpolatedPosition 0x676711. Under 60 FPS these update at 30 Hz while drawables interpolate at 60 Hz. Identify them (0x764374/0x764442/0x82A4A8) and check whether any are visual and would visibly lag.
- Runtime confirmation (M5): call counters per second for every gated site, and snow time/real time = 1.0 in 60 mode with the split.
