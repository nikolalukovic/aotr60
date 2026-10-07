# AREA transitions

## SUMMARY

I traced the menu transitions. The main menu can safely run at 60 FPS: every route from the menu into a match, a load, Create-a-Hero, a replay or the Living World already passes the engine-reset hook. That hook switches 60 off before any loading starts, and no game-logic state can leak into the next match.

AotR has no 3D shell map (ShellMapOn = No), so the whole menu runs as game mode 9 (no game).

Switching on needs only a new shell rule in the mode controller. It keeps 30 FPS in the LAN lobby, online screens, credits (the stock credits run at 100 FPS) and the War-of-the-Ring map preview. Plus one new backstop hook at the start of a new game (0x6314CD).

Several fixes are needed so that menu-side 60 mode cannot disable 60 in a later match or blur the picture:
- scope the performance fallback and the sticky g_unknownPath flag to the menu;
- reset the camera history after a device reset;
- include the resolution in the display check;
- keep menu segments out of the determinism trace, which would otherwise give a false PASS.

Full report: transitions_report.md

## DETAILS

BASE FACTS
F1 CONFIRMED: no shell map. ShellMapOn = No in <game folder>\aotr\data\ini\gamedata.ini:11133 and data\ini\object\gamedata.ini:3. Shell::showShellMap 0x75DE01 starts the map only when GD+0xAF0 != 0 (checks at 0x75DE2D and 0x75DE76). So the menu and all its sub-screens run in mode 9.

F2 CONFIRMED: the stepper keeps running in mode 9.
- GameLogic::update 0x62E4E8 takes the !runnable full path, with no frame++.
- The stepper sets GC+0xC8 at 0x6325FB, so m_frame advances 30/s and the start rule (s==1 && GC+0xC8) is met once per tick.

F3 CONFIRMED: every reset zeroes m_frame.
- Chain: GameEngine::reset 0x635D83 -> SubsystemList [0xDE3380] resetAll 0x5B4783 (calls vt+0x24 of each subsystem) -> GameClient::reset 0x64782E -> m_frame = 0 at 0x64784B.
- [0xDE4308] is also zeroed, at 0x77945F and 0x6315FB.
- The runtime log agrees: "60 FPS on ... m_frame 10" after more than 150 menu renders.

F4 CONFIRMED (frame_ctl.cpp OnEngineReset / LeaveSixty / OnPostRender): a reset can happen anywhere and the result is still exact.
- Inside an A-render: g_forceHalt becomes 0, so that iteration is stock render k + stock step.
- Inside the Y stock step: the pair is already complete.
- Inside a B-render: the result is B + stock step.

F5: StartBlockReason's m_frame<8 rule is what stops 60 from restarting between a reset and the load.
- A queued MSG_NEW_GAME or 0x1F is processed at the next sub 1, at most 6 steps after the reset.
- So the threshold must stay at 7 or higher.

TRANSITIONS
- Boot -> intro/logo movies -> menu:
  - The intro is a UI sequence: 0x648482 sets GD+0xAF3, 0x64838D plays the NewLine/Tolkien logos via Display vt10C, and 0x6457EA clears GD+0xAF3.
  - The existing intro rule blocks 60 here; the menu goes to 60 at the first s==1 tick with m_frame>=8.
- Skirmish 0x9286D7 (CONFIRMED): clearGameData 0x9286F0 (A-render, APT callback), InitRandom 0x928736, MSG_NEW_GAME 0x928807. War of the Ring: a second clearGameData 0x92879B, then MSG 0x1F 0x9287B5.
- Campaign 0x5EAB8F (CONFIRMED): clearGameData 0x5EABE8, InitRandom 0x5EAC4A, MSG_NEW_GAME 0x5EAC5A.
- Living World 0x91B825 (CONFIRMED): clearGameData 0x91B87E, then MSG 0x1F 0x91B8A9 -> 0x779D36 -> 0x6BE09E -> 0x6B5286. That last call does setGameMode(8) + startNewGame at 0x6B5347 without its own reset; this is why the backstop hook below exists.
- Create-a-Hero 0x91A018 (CONFIRMED):
  - It appends MSG 0x1D (0x91A043) and MSG_NEW_GAME(7) (0x91A06C). The reset then runs inside the Y logic step, so the pair is complete.
  - InitRandom(0) at 0x91A05E runs on an A-render while 60 is on; telemetry will count it as a "seed change in 60 mode". This is expected.
  - The Create-a-Hero map itself stays blocked (mode 7, GD+0xD45).
- Load from the menu: AptSaveLoad 0x818456 / 0x8184AA, then reset, then loadGame 0x6DEB9A (reset again), then startNewGame(1) at 0x82BEED. The reset happens mid-A-render. The stepper phase at load time can be any k, as in stock.
- Replay: playbackFile 0x77F66B resets; playback (mode 3) stays blocked.
- Quit to menu from a game: 0x625E36 queues 0x1D, the reset runs in the logic step, and the menu goes back to 60 at least 8 frames later.
- Mode-9 gap during a Living World battle start: 60 cannot start here because m_frame stays below 8.
- Options -> resolution / display mode (in an A-render):
  - Reset_Device 0x522000 sets g_gapReset, so the pacer treats the next iteration as a gap; no mode change.
  - Gap: DisplayBlockReason ignores resolution-only changes, so a stale refresh rate could stay cached (fix R3).
  - Gap: the camera history could blend view planes across the reset for one frame (fix R2).
- Alt-tab / minimise in exclusive fullscreen:
  - While minimised, the loop at 0x44181F parks between iterations, so A/B parity survives.
  - Lateness over 250 ms counts as a gap (pacer.cpp:240).
  - On return, the Present stub sets g_gapDevLost on DEVICELOST; the engine then calls TestCooperativeLevel, followed by Reset_Device(1) at 0x52268F or Sleep(200) at 0x522696.
- War-of-the-Ring map preview in MpGameSetup:
  - The window ctor 0x975659 makes the LW view suspended + active; its dtor 0x9753D3 deactivates it (CONFIRMED).
  - The current LW branch already blocks it, because the view is active while mode != 8.
  - It is INFERRED safe to allow later, but it would only show 30 Hz motion twice, because lw_present requires !suspended.
- LAN and online: TheLAN [0xDE4394] (set 0x847F3E, cleared 0x8471E8) and the AptOnline singleton [0xDEA36C] (set 0x91E1D6, cleared 0x91DC4C). Both block.
- Credits: start sets Shell+0x5D at 0x91B6CE and FPS 100 at 0x91B6DC; end restores GD+0x28 at 0x91B7AA. Block while GE+0xC != GD+0x28.
- Quit to desktop:
  - WW3D::Shutdown 0x517AA0 (SP_SHUTDOWN) is reached only from the W3DDisplay dtor 0x449861, the init-failure path 0x4466B4 and WinMain 0x402D27. It is never reached by a resolution change, so split present stays enabled.
  - Split present never defers in mode 9 (StateGate kGateLwMap).

PREDICATE (frame_ctl.cpp)
- In the first branch of GameModeBlockReason: modes 0/2/6 -> allowed; mode 9 -> ShellBlockReason(); anything else -> blocked.
- ShellBlockReason blocks when:
  - the new INI key Menus = 0;
  - [0xDE4394] != NULL ("LAN lobby");
  - [0xDEA36C] != NULL ("online");
  - GE+0xC != GD+0x28 ("credits").
- In the LW branch, give mode 9 with an active+suspended view its own reason: "Living World map preview".
- All other existing BlockReason and StartBlockReason checks stay.
- No UI-sequence block is needed: the runner 0x80000F is A-only, and the intro is covered by the AF2/AF3 flags.

LEAK AUDIT
Game state:
- m_frame, [0xDE4308] and GE+0x38..0x5C are reset by the engine.
- s (GE+0x34) is not reset, same as stock.
- The sync clock is flushed exactly (FlushOwedSync).
- Logic RNG: new games reseed (callers of 0x6D3261). Tactical loads apparently do not restore the seed (INFERRED; only 0x6D320A, the CRC path, xfers it). Menu B-renders must therefore consume none (check V3).
- Messages added after propagation (UI-sequence runner 0x6324A6 and later; propagation runs at 0x6324A1) reach logic one step earlier than stock. For menu transitions this has no effect on determinism. Noted for the in-game owners.

DLL state:
- Cleared by LeaveSixty / reset: everything except the pacer fallback (g_fallbackUntil, backoff count) and g_unknownPath (sticky). Fixes R5 and R6 scope those.
- Split present's g_disabled cannot be reached from the menu.

CODE CHANGES
- H1 NEWGAME_GUARD: new site (see the table below).
- R2: the GAP_RESET stub increments g_devResetGen; C0 calls CameraReset() and resets the LW present history at the next X boundary.
- R3: add BackBufferWidth/Height (0xDD2FF8 / 0xDD2FFC) to the display signature.
- R4 (optional): block 60 while the device is lost.
- R5: the pacer fallback must not be triggered by, or carry over from, the menu.
- R6: a g_unknownPath bit set in the menu is cleared by a reset.
- R7 telemetry:
  - Skip TraceLogicCall in modes 9 and 4, and have compare_traces ignore segments whose frames never exceed 30. Otherwise a 60-FPS menu segment can pair with a 30-FPS one of equal seed and report a false PASS.
  - Keep separate per-tick site sums for the menu.
  - Add counters for resets inside a B-render, seed changes on B-renders in mode 9, and guard hits.
- T1 (rule for the APT and menu owners): anything that resets, loads, starts a game, changes FPS or the device, or appends logic messages must run on A-renders only.

## RISKS

- INFERRED: tactical saves do not restore the logic RNG seed (only the CRC path 0x6D320A references it), so a load from the menu continues the RNG state the menu left. Menu B-renders must therefore consume no logic RNG. APT rendering on B-renders was found idempotent in the phase-6 report, but this needs the telemetry check V3.
- INFERRED: leaving the Living World to the menu deactivates the LW view (its reset). If it does not, the menu stays at 30 with the reason "Living World map preview/transition"; check V4 will show it.
- INFERRED: [0xDEA36C] (AptOnline) and [0xDE4394] (TheLAN) are non-NULL only while their screens exist. 0x9131DB also creates TheLAN (loopback 127.0.0.1) but has no static reference, so it is probably dead code. If it does run in single player, the menu shows the reason "LAN lobby".
- Menu B-renders show APT frames that do not change (APT update is A-only), so the menu is not smoother until the APT/shell area changes that. If APT update is later run on B-renders, button callbacks (reset, load, new game, FPS change, LAN/online creation, message appends) must stay on A-renders (rule T1).
- The m_frame<8 start threshold is now load-bearing for transitions; lowering it below 7 would allow a short 60 window between a reset and the load. The NEWGAME_GUARD hook also covers the load itself.
- Without R5, a fallback triggered in the menu (rapid screen loads producing 50-250 ms stalls three windows in a row) would hold the next match at 30 for 30 s or more, with a doubled backoff. Without R6, a menu-only display vt+0x188 call path would disable 60 for the rest of the session.
- Without R7a, the determinism tool can pair two menu segments of equal seed (one at 60, one at 30) and print a false PASS.
- Resolution changes in exclusive fullscreen: without R3, a mode whose default refresh is below 59 Hz goes undetected. This is unlikely on the 120 Hz test display.
- Device-lost state (not minimised): each render sleeps 200 ms (0x522696), which is two sleeps per pair in 60 mode. This is harmless in the menu; R4 is optional.
- Messages appended after propagation (0x6324A1), for example by the UI-sequence runner, reach logic one step earlier than stock in 60 mode. This does not matter for menu transitions; it is flagged for the in-game owners.

## RECOMMENDATION

Allow 60 FPS in mode 9 through a ShellBlockReason: Menus=1, TheLAN==NULL, AptOnline==NULL, GE+0xC==GD+0x28. Keep the War-of-the-Ring map preview, mode 4, mode 7, replays and multiplayer at 30, and keep all existing BlockReason / StartBlockReason checks (m_frame<8 stays).

Add these changes:
- the NEWGAME_GUARD backstop at 0x6314CD;
- the device-reset camera flush via GAP_RESET (R2);
- the resolution in the display signature (R3);
- the menu-scoped pacer fallback and g_unknownPath (R5, R6);
- the telemetry and trace filtering and counters (R7).

Then run checks V1 to V7: each menu-to-game route logs "60 FPS off (game reset)" before the load and 0 guard hits; in mode 9, 0 logic-seed changes on B-renders and 0 tick-invariant errors; a resolution change and 10 alt-tabs at 4K without a fallback; and a replay determinism PASS after time spent in the menu at 60.

Treat the War-of-the-Ring preview as a later opt-in once the preview's LW render is shown to be stateless at runtime.

## SITE {
 "id": "NEWGAME_GUARD",
 "address": "0x6314CD",
 "length": 5,
 "original_hex": "b8 67 45 b8 00",
 "original_asm": "0x6314cd: b8 67 45 b8 00  mov eax, 0xb84567  ; GameLogic::startNewGame(bool loadingSaveGame) SEH prologue, followed by 0x6314d2: e8 19 ba 40 00 call 0xa3cef0 (_EH_prolog)",
 "kind": "func_detour",
 "replacement_hex": "e9 <rel32:NEWGAME_GUARD_CAVE>",
 "stub": "NEWGAME_GUARD_CAVE (all modes):\n    cmp byte ptr [g_m60], 0\n    je  stock                      ; 30 mode: nothing to do (no C++ call)\n    SAVE_ALL\n    call AotR60_OnNewGameGuard     ; cdecl(): if !OnMainThread() return; ++g_newGameGuardHits; Log WARN 'new game without engine reset'; g_fs.Reset(); LeaveSixty(\"new game without engine reset\"); SplitPresentReset(kCancelEngineReset); g_lodChecked=nullptr (no '# reset' trace marker)\n    RESTORE_ALL\nstock:\n    mov eax, 0B84567h              ; displaced\n    jmp 0x6314D2                   ; call _EH_prolog with the stock stack",
 "live_after": "At 0x6314D2: EAX=0xB84567 (the EH handler that _EH_prolog consumes), ECX=this (TheGameLogic), [esp]=return address to the caller (0x6B534C, 0x779DF3 or 0x82BEF2), [esp+4]=loadingSaveGame. EBX/ESI/EDI/EBP as at entry. EFLAGS dead (_EH_prolog does not read them). x87 stack empty and XMM caller-saved at a call boundary. SAVE_ALL preserves everything anyway.",
 "branch_into_span_check": "Interior 0x6314CE..0x6314D1: an unaligned rel8/rel32 sweep of the executable sections found 0 hits, and an absolute-pointer scan found 0 hits. Same result in game.dat, aotr\\zGameDats\\delayfix.dat and rotwk\\game820.dat, which have identical bytes at 0x6314CD. The entry is reached only by calls at 0x6B5347, 0x779DEE and 0x82BEED. Script: <analysis workspace>\\menu\\transitions\\span_check.py.",
 "purpose": "Backstop: 60 mode can never be active while a new game or save load starts. It covers startNewGame callers that have no reset of their own: the LW map start 0x6B5347 via MSG 0x1F, and any future path. Expected hit count 0, because all checked menu paths call clearGameData first. If the RESET hook already ran, it is a no-op."
}

## SITE {
 "id": "GAP_RESET (existing site, stub change only)",
 "address": "0x522000",
 "length": 6,
 "original_hex": "64 a1 00 00 00 00",
 "original_asm": "0x522000: 64 a1 00 00 00 00  mov eax, dword ptr fs:[0]  ; DX8Wrapper::Reset_Device SEH prologue",
 "kind": "func_detour",
 "replacement_hex": "e9 <rel32:GAP_RESET_CAVE> 90",
 "stub": "GAP_RESET_CAVE: mov byte ptr [g_gapReset],1; lock inc dword ptr [g_devResetGen] (NEW); [existing split-present cancel]; mov eax, fs:[0]; jmp 0x522006. In C++, OnPreRender compares g_devResetGen with its last seen value. When the coming iteration is X (or in 30 mode), it calls CameraReset() and resets the LW present history.",
 "live_after": "Unchanged from sites.json: at 0x522006 EAX=fs:[0], all other GPRs and ESP as at entry. EFLAGS are dead (first consumer is the cmp at 0x52201B), so the flag change from lock inc is allowed.",
 "branch_into_span_check": "Unchanged (existing verified site): interior 0x522001..0x522005 has no hits.",
 "purpose": "After a resolution, display-mode or device-lost reset, the A-render must not blend camera transforms or view planes recorded before the reset (the aspect may change). Applies in the menu and in paused games."
}

## VERIFIER
The core safety argument holds up. Every menu-to-game route I checked goes through GameEngine::reset (thunk 0x44181A) before any loading starts. A reset at any point keeps logic exact. AotR has no shell map, so the menu runs as mode 9. The new NEWGAME_GUARD site checks out byte for byte in all three builds (game.dat, delayfix.dat, game820.dat).

The analysis does have factual errors:
- **Dead-code claim refuted.** 0x9131DB, which creates the loopback TheLAN, is called from 0x91336F, so it is not dead code.
- **Load row mislabelled.** 0x818456 is the call to loadGame, not a reset. 0x8184AA is the reset on the replay-failure path.
- **Campaign entry wrong.** The menu's campaign buttons go 0x91BE05 -> 0x91B1D2: clearGameData, then MSG 0x20. 0x5EAB8F is only the second step of that route.
- **Fix R2 not needed, and timed wrong.** The camera interpolation already refuses to blend when the aspect, near or far plane changes. The proposed C0-time flush would also act one pair too late.

The summary also overclaims. APT update is A-only and APT movies advance on a fixed step, so the recommended predicate change on its own makes the menu present 60 times per second with only 30 distinct pictures. That is safe, but it does not give the user a smoother menu. Real smoothness needs work in the APT/shell area. Smaller gaps: the transition table leaves out the tutorial, score screens and UI-sequence-driven transitions. B-render message propagation also runs translators on UI-sequence and keyboard messages, which the B-only-draws assumption does not cover.

Verdict: the transitions part is safe to build, with corrections. NEWGAME_GUARD is fine. R3, R5, R6 and R7a are valid. Drop R2 or move it to the scene-render window.

- [confirmed] F1: no shell map; ShellMapOn=No; showShellMap starts the map only when GD+0xAF0 != 0; the menu is mode 9
  aotr\data\ini\gamedata.ini:11133 and data\ini\object\gamedata.ini:3 both say No. No AotR .big file contains 'ShellMapOn', so the loose INI wins. In the INI parse table, the entry at 0xC00510 maps 'ShellMapOn' (0xC027E4) to offset 0xAF0, and PlayIntro maps to 0xAF2. The checks `cmp byte [eax+0xaf0],bl` sit at 0x75DE2D and 0x75DE76. Runtime only proves 'mode not in {0,2,6}': the log prints the generic reason and never the value 9. Suggest logging GL+0x110 in the reason.

- [confirmed] F2: in mode 9 the stepper sets GC+0xC8 every iteration and m_frame advances 30/s; the start rule is met once per tick
  0x6325F8..0x6325FB `xor ebx,ebx; inc ebx; mov [eax+0xc8],bl` runs on every non-halted iteration. GameLogic::update 0x62E4E8 clears C8 only on the frozen branch at 0x62E5EF, when message 0x1D is not pending. A runnable check (0x625130) gates frame++ at 0x62E577. The menu log lines read 'sub 2/4/5/6, frame 0', so s cycles while the logic frame stays 0.

- [confirmed] F3: every reset zeroes m_frame (0x64784B) and [0xDE4308] (0x77945F, 0x6315FB)
  0x647849 `xor ebx,ebx` is followed by 0x64784B `mov [esi+0x10],ebx`. A pointer to 0x64782E sits at vtable slot 0xC048BC (+0x24). resetAll 0x5B4783 loops `call [eax+0x24]`. The stores at 0x77945F and 0x6315FB match. One correction: the cited log line 'm_frame 10' comes from the game start at render 181, not from a menu segment. It is consistent with the claim but weak evidence.

- [confirmed] F4: a reset inside an A-render, the Y step or a B-render all stay logic-exact
  OnEngineReset -> LeaveSixty sets g_m60=0 and g_forceHalt=0. OnPostRender recomputes g_forceHalt as g_m60 && ... = 0 (frame_ctl.cpp:457), so the iteration becomes a stock one. Resets from APT (0x63244B) run before the draw, so C3 runs stock, and FlushOwedSync covers the rest. One gloss: after a reset inside a B-render, g_uiTick becomes 1, so the A-only tail of that render (UISEQ 0x6324A6, keyboard drain 0x6324BC, audio 0x6324F9) runs a second time in that pair. Logic stays exact, but the result is not simply 'B + stock step'.

- [confirmed] F5: m_frame<8 keeps 60 off between a reset and the new game (the next sub 1 comes within 6 steps)
  s is not reset. After a reset in render k, the steps are k+1..6 and then sub 1, which is at most 6 steps later. At that C0, m_frame is at most 6. The limiter is unlimited while m_frame < [0xDE4308]+6 (0x63A17B..0x63A188). The 'load-bearing' wording is overstated, though. Multi-hop routes (campaign: MSG 0x20 -> 0x5EAB8F -> MSG_NEW_GAME) hold only because each hop resets again (0x5EABE8). A UI sequence with wall-clock steps after a reset is not bounded by 6 steps. The reset hook and NEWGAME_GUARD keep things exact anyway.

- [confirmed] Skirmish 0x9286D7 / LW 0x91B825 / Create-a-Hero 0x91A018 sequences
  Skirmish: clearGameData 0x9286F0, then InitRandom 0x928736. LW: clearGameData 0x91B87E, LW view vt28(1) at 0x91B89C, MSG 0x1F at 0x91B8A9, setGameMode(8) at 0x6B533A, startNewGame(0) at 0x6B5347. 0x6BE110 calls 0x6B5286, with no reset in that chain. CaH: MSG 0x1D 0x91A043, InitRandom(0) 0x91A05E, MSG 0x1E 0x91A06C + arg 7. Missed point: the LW and WotR paths activate the LW view inside the A-render (0x91B89C, 0x9287AA), so the existing LW branch blocks at once, independent of m_frame.

- [refuted] Campaign: 0x5EAB8F is the menu-to-campaign route
  0x5EAB8F has no menu caller. Its callers are the campaign-manager functions 0x5EADC3, 0x5EAE37, 0x5EC4EC and 0x5EC5D1. The main-menu campaign buttons (0x91BE05, 0x91BE64, 0x91BEFA; strings GOOD_CAMPAIGN / ANGMAR_CAMPAIGN) call 0x91B1D2. That function runs UI calls, clearGameData at 0x91B231 (reset in the A-render), then appends MSG 0x20 at 0x91B256. The logic later reaches 0x5EAB8F, which does clearGameData again plus MSG_NEW_GAME. Safety is unchanged, but the table names the wrong entry and leaves out the second hop.

- [refuted] Load from the menu: AptSaveLoad 0x818456 / 0x8184AA, then reset, then loadGame
  0x818456 is `call 0x6df4d1` (loadGame) itself. 0x8184AA is GE vt24 (reset) after clearGameData 0x81849D, on the path where playbackFile 0x81848C returned 0, i.e. a failed replay. loadGame does its own clearGameData at 0x6DEAC7 and GE vt24 at 0x6DEB9A before the xfer. The conclusion (reset before load) holds; the address labels are wrong.

- [refuted] TheLAN [0xDE4394] and AptOnline [0xDEA36C] set/clear addresses; 0x9131DB has no static reference and is probably dead code
  The set and clear sites are confirmed: 0x847F3E, 0x8471E8, 0x91E1D6 (factory 0x6D2CA4, registered at 0x91E702) and 0x91DC4C. The dead-code claim is wrong. A raw E8 scan finds `call 0x9131db` at 0x91336F, inside 0x913338. That function runs clearGameData 0x913349, then creates the loopback LAN if [0xDB670C]==0 && [0xDEA020]!=0, else InitRandom + MSG_NEW_GAME. It is called at 0x9133ED from the legacy WND MapSelectMenu code. Whether it is reachable in AotR is unproven. If it is reached, the outcome is the safe one (blocked, reason 'LAN lobby').

- [confirmed] Credits: FPS 100 at 0x91B6DC, restore at 0x91B7AA; block while GE+0xC != GD+0x28
  GE vt+0x48 resolves to 0x66F0FD `mov [ecx+0xc],eax`, and pacer.cpp:193 reads GE+0xC. Only three call sites use [0xDE4324]->vt48: 0x779DCF (MSG_NEW_GAME, optional arg3 1..1000) and the two credits sites. Caveat: an MSG_NEW_GAME with a non-default FPS leaves GE+0xC changed after quitting to the menu. The rule then pins the menu at 30, which is safe but needs a clear reason string.

- [confirmed] NEWGAME_GUARD site 0x6314CD: bytes, boundaries, live_after, branch-into-span
  `b8 67 45 b8 00` is followed by `e8` _EH_prolog at 0x6314D2. game.dat md5 6ffe859d... matches rotwk\game.dat. span_check reports 0 interior hits in game.dat, delayfix.dat and game820.dat, with E8 callers 0x6B5347, 0x779DEE and 0x82BEED only. The MSG_NEW_GAME handler (0x779D50..0x779DEE) does no clearGameData of its own, which justifies the backstop. 'Expected 0 hits' is plausible: every SP menu sender I checked clears first, including the tutorial at 0x91B967. It is unproven for in-game LW returns.

- [refuted] R2: without a device-reset camera flush, the A-render blends view planes across a resolution change
  InterpolateCameraHalfway (camera_math.cpp:70-79) already returns false, which counts as a cut, when aspect, near or far differ or the extents change. In mode 9 there is no 3D scene. The proposed check also runs too late: the Options callback is APT, which runs inside an A-render after that iteration's C0. If any blend did happen, it would be in that same A-render, while R2's flush runs only at the next X C0. If the fix is kept, it belongs at S2 or the scene window.

- [confirmed] R3: the display signature ignores resolution
  The signature (frame_ctl.cpp:67) covers only the interval, windowed flag, refresh (0xDD3028) and hwnd. 0xDD2FF8 and 0xDD2FFC are BackBufferWidth and BackBufferHeight of the same D3DPRESENT_PARAMETERS block, consistent with +0x1C hwnd at 0xDD3014. Impact on a 120 Hz display is low.

- [confirmed] R5/R6: the fallback and g_unknownPath persist across reset and mode changes
  g_fallbackUntil is global. PacerOnModeChange clears the window but not FallbackPolicy's streak counters or fallbacks_. Vt188Detour does `lock or g_unknownPath`, and nothing ever clears it. Severity is low: a fallback needs 2-3 bad 5 s windows (pacer_policy.cpp), and today's menu render stalls (56-105 ms, plus one 5.2 s gap counted as a gap) would rarely trigger it.

- [confirmed] R7a: a menu segment can produce a false determinism PASS
  In mode 9, GL+0x40 stays 0, so a menu segment produces only the keys (0,1..6). compare_logic keys on (frame,sub). In 2-argument mode main() picks the longest segment, so a long menu session at 60 compared with one at 30 checks 6 keys and passes.

- [confirmed] Split present never defers in mode 9; alt-tab parks between iterations; device-lost path
  split_present.cpp:611-613 returns kGateLwMap for modes other than 0/2/6. 0x441822 calls the stepper before the IsIconic/Sleep(5) loop. 0x52267B calls TestCooperativeLevel (vt+0xC); 0x52268F calls Reset_Device(1); 0x522696 calls Sleep(200). The 250 ms gap rule is at pacer.cpp:240 (`late > g_freq/4`).

- [uncertain] Summary: the main menu can safely run at 60 FPS with just the new shell rule
  It is safe as far as transitions go. But APT update is A-only (GATE_CU_APT) and APT movies advance on a fixed step (../living_world/area_ui.md:6), so B-renders repeat A's picture. The menu would be labelled 60 FPS while showing 30 distinct frames per second, at twice the GPU cost at 4K. The user's goal of a smooth menu is not met until the APT/shell area adds presentation-only half-steps. Also, B-render APT idempotence was shown only for the LW HUD movies, not for MainMenu.apt and the shell movies.

- [confirmed] Messages appended after propagation reach logic one step early (noted for in-game owners)
  clientUpdate call order: APT 0x63244B, GameClient::update 0x632498, propagate 0x6324A1, UISEQ 0x6324A6, keyboard drain 0x6324BC. The analysis misses a consequence: the B-render's propagate is not empty. It runs translators on UISEQ and keyboard messages from the A-render, so translator side effects run on B-renders, which contradicts PLAN §1.4 and the analysis's assumption that B-renders do no A-only work.

## MISSED
Missing for a safe and useful 60 FPS main menu:

**Coverage gaps**
1. **Visible smoothness.** Nothing in this analysis makes the menu look smoother. APT is A-only and APT movies use a fixed step, so the shell area must add presentation half-steps or interpolation, or 60 shows duplicate frames.
2. **APT idempotence in menu movies.** It has not been checked for MainMenu.apt and the other shell movies (3D elements, real-time clocks, the AptPlayer+0x328 pending path). Menu video advance is INFERRED A-only (Display::update) but not verified. The software or animated cursor draw is also unverified.
3. **Transition table is incomplete.** Missing rows:
   - tutorial (0x91B967 clearGameData, 0x91BAAA MSG_NEW_GAME mode 6, no InitRandom);
   - the menu-side campaign entry 0x91BE05 -> 0x91B1D2 (MSG 0x20 hop);
   - post-game score/results screens, campaign end-of-mission movies and LW results, which return to mode 9 from a game;
   - main-menu UI sequences pushed by the FS-command handler 0x91C2FC and 0x91C108 (0x8000AA).

**Predicate hardening**
4. In mode 9, also block while the UI-sequence queue [0xDE8900] is non-null. It costs one check and removes reliance on m_frame<8 for wall-clock or multi-hop transitions. The analysis said this was not needed.

**Stale state that pins the menu at 30 (safe, but needs clear reasons)**
5. StartBlockReason inputs can persist into the menu: TV+0x23D4>1 after quitting mid-cinematic, GL+0x9C/0xA8 after quitting during a fade-in, a non-default GE+0xC from an MSG_NEW_GAME arg3, or a persistent LW view after an LW quit.

**B-render translators**
6. Propagate on B handles messages appended after propagation (UISEQ, keyboard drain). Translator side effects, such as meta-key actions or anything that resets, loads or starts a game, can therefore run on a B-render. This needs an audit or a counter (T1 does not cover it).

**Unproven reachability**
7. 0x913338 -> 0x9131DB (loopback TheLAN through the legacy MapSelectMenu) is statically reachable. Confirm it never runs in AotR.

**Telemetry and verification**
8. Log GL+0x110 in the reason line to prove mode 9 at runtime.
9. Count NEWGAME_GUARD hits per caller (0x6B5347, 0x779DEE, 0x82BEED).
10. The guard's C++ body must check OnMainThread before LeaveSixty, as OnPreRender does.
11. Reset FallbackPolicy streak counters, not only g_fallbackUntil, at the menu/game boundary.

**R2 placement**
12. If kept, R2 must act at S2 or the scene-render window, not at C0.

**Address corrections for the report**
13. 0x818456 = loadGame call; 0x8184AA = failed-replay reset; 0x5EAB8F is a logic-side campaign starter, not a menu handler.