# AREA ui

## SUMMARY

In the AotR main menu, the existing A/B design already handles nearly all per-render work. No path I traced runs twice as fast at 60. The menu has no 3D shell map: ShellMapOn = No, so the menu runs in mode 9 with no game and the logic frame stays at 0. Everything the menu shows is one of three things. The APT Flash movies are updated A-only and drawn identically on A and B. Videos are serviced A-only and play at wall-clock speed. The rest is time-based: the background image, the W3D background model drawn in the APT View3D window on the APT real-time clock, and the cursor, which is the hardware Win32 cursor. I propose one small new gate, GATE_GC_DISPMODE: a resolution change requested in Options can otherwise be applied on a B-render. I also propose one optional robustness site, SHELL_THROTTLE_60. The real work is in the mode controller: allow mode 9 under a set of conditions, block the credits screen (it raises the FPS limit to 100, which would set the pacer to 5 ms per half), and do not use m_frame for the start rule. Important for the user: every shell APT movie is authored at 33 ms per frame (30.3 fps), checked in 11 .apt files. Menu animations will therefore look the same at 60 as at 30. The visible gain is limited to the slowly rotating lens-flare and light textures of the 3D background model, plus lower latency. The menu's GPU load roughly doubles.

## DETAILS

SCOPE / METHOD: static analysis of game.dat (Ghidra dump plus capstone). Bytes were checked in rotwk\game.dat, aotr\zGameDats\delayfix.dat and rotwk\game820.dat (all three identical for the proposed spans). AotR data checked: data\ini\gamedata.ini, video.ini, mappedimages, the aotr root MainMenu.apt and SkyrimMenu.apt, rotwk\apt\*.big, aotr\*.big and rotwk\W3D.big. Helper script: <analysis workspace>/menu\shell_render\wscan.py (lists the non-stack memory writes of a function).

1. WHAT THE MENU IS (CONFIRMED)
- No shell map.
  - aotr\data\ini\gamedata.ini:11133 and data\ini\object\gamedata.ini:3 both say `ShellMapOn = No`.
  - Shell::showShellMap 0x75DE01 therefore sets Shell+0x52 = 1 (0x75DE3C) and starts no mode-4 game. GL+0x110 stays 9 (no game).
  - The test log agrees: in the menu it shows "logic 0.0 (sub 5, frame 0)" and the block reason 'game mode (menu or multiplayer)'.
- Background, two layers:
  - (a) Shell::update 0x75E24D..0x75E2A0 shows the mapped image 'ShellMapLowLOD' (AotR: InstallLoad.tga) as the Display image overlay 0x65C42C. It is drawn on every render by 0x65CF05, and its hold/alpha step is already A-only (site INT_OVL_FADE), so it does not flicker.
  - (b) MainMenu.apt calls FSCommand:SetBackground fadein (0x815507), which runs ShowFrontEndBackground 0x6230B6 in Background.apt. That movie embeds a View3D window, '_RenderObj SFE_MenuBkgrnd', '_AnimMode MANUAL'.
- Menu screens: APT (MainMenu.apt, Skirmish, CampaignMenu, Options and others). Every shell .apt has 1024x768 at 33 ms per frame; checked in 11 files, including AotR's MainMenu.apt, SkyrimMenu.apt, Skirmish, Options, CampaignMenu, MenuExport and Background.

2. clientUpdate 0x632409 / GameClient::update 0x64849E IN THE SHELL
- Every call is already classified in sites.json coverage. The shell-relevant calls are gated A-only:
  - APT update: GATE_CU_APT.
  - Window manager and transitions: GATE_GC_WM (0x6C15FD → destroy list 0x6C1478 + TheTransitionHandler vt28).
  - Shell::update: GATE_GC_SHELL.
  - Keyboard/mouse: GATE_GC_KBDSEL / GATE_GC_MOUSESEL.
  - Audio: GATE_CU_AUDIO. In the shell Miles adds a fixed [0xD9F620] per update, so it stays 30.3 updates/s.
  - Display::update (video): GATE_GC_DISPUPD.
  - Popups, UI sequence and radar are also gated.
- Ungated per-render calls in the shell, all harmless:
  - message propagation 0x6324A1 (empty on B);
  - CRC guard 0x632478/0x63251C;
  - 0x63239D (no network);
  - deferred D3D queue 0x532D6F;
  - freeze queries;
  - the draw 0x648869;
  - GC vt90 deferred delete.
- One exception: 0x6488A1 → 0x645DAD, the pending display-mode change (GC+0xC9).
  - Only two code paths set GC+0xC9: 0x6455F3 (caller 0x91EF0F, Options apply) and 0x645622 (caller 0x91B2A7, AptMainMenu revert).
  - If the flag is set after A's GameClient::update, B-render runs the change: it deletes and recreates TheShell, resets the device to the new resolution and reloads MainMenu.apt. One possible way the flag gets set that late is an APT input callback during propagate (INFERRED).
  - Fix: site GATE_GC_DISPMODE (event moved to the next A).
- Shell::update 0x75E1D3 (CONFIRMED):
  - It fires its screens only if timeGetTime − last ≥ 32.333f ([0xC2C90C], comiss at 0x75E210), and then sets last = now.
  - Stack entries are screen wrappers (vtable 0xC4FC04). Their update slot vt8 0x8129F0 calls the screen window's vt+0x14, e.g. AptMainMenu::update 0x91C2FC (state machine, credits).
  - So screen updates can never exceed ~30.9/s, whoever calls them. In 60 mode they run only on A.
  - timeGetTime has 1 ms granularity against the QPC 33.000 ms pair, so an A-to-A gap occasionally reads 32 ms and that A is skipped (slower, never faster). Stock has a similar exposure. Optional site SHELL_THROTTLE_60.

3. drawFrame 0x449CF8 IN THE SHELL (mode 9, GL+0x125=0, no LW view)
- Order of per-render calls (A and B):
  - updateViews vtA0. W3DView::update: S1 skips it on B. The drawable pass at 0x48C701 runs only if sync changed (0x48C70C); sync is frozen in the shell, so the pass is empty.
  - 0x479FC4; particle manager (C7, A-only); Begin_Render; RenderViews (empty world).
  - Video layers 0x65D03B: 0 memory writes (CONFIRMED wscan).
  - Image overlay 0x65CF05 (INT_OVL_FADE).
  - InGameUI draw 0x48EA29, three layer passes with WM+0x3C=1/0/-1. Its only writes are lazy model creation at +0xAB8/+0xABC. It calls winRepaint 0x6C1C18 → drawWindow 0x6C1B52 (window vt+0xC only; 0 writes) and the transitions draw [0xDE3654] vt30 (0x5DB725/0x5DB2CF: 0 writes).
  - APT render 0x46203E → 0x62212F (details below).
  - UI particles 0x6A536C (INT_UIPART); mouse draw 0x498CBC; letterbox 0x443939 (absolute (t−start)·0.001, idempotent); subtitles vt158 (timeGetTime); [0xDE3654] vt30; End_Render/Present (PRESENT site).
- APT render 0x46203E → 0x62212F (CONFIRMED + LW report):
  - The W3D sync is swapped to an absolute APT real-time clock: 0x4A89BD sets dcbbc4 = dcbbbc + (now − dcbbc0), clamped at a 100 ms step. 0x4A8A96 restores it.
  - The pending update at +0x328 is set only by the in-game ShowCommandInterface helper.
  - The mark/hide sweep of embedded windows (0x81424C/0x814278) is rebuilt identically each render.
- Shell render callbacks (all registrations via 0x624348/0x92B194 enumerated) are idempotent per render:
  - RenderCredits 0x91B1B8 → 0x9C6765 writes only +0x40/+0x44 = the draw rect.
  - AptTimeLine::RenderGraph 0x9257F2 recomputes the max into +0x2B0/+0x2B4.
  - AptMapPreview::Picture 0x9757C0 only drops a bad texture pointer.
  - _Content 0x92E2AC, _Message 0x8EAFC5 and RenderFactionIcon 0x6D3D5D: pure.
  - ToolTipText 0x782D52 is a pure draw; 0x782DD1 sets the flag [0xDE7D28]=1, which is otherwise only reset at init.
  - RenderImage, TimerOverlay, _RenderText and _ProgressOverlay are covered in the LW report.
- View3D background (CONFIRMED structure):
  - Callback 0x814DDC → 0x813514 re-applies the '_frame:/light' command string only when it changed → vt+8 0xB5470F reads the CAMERA bone and runs the scene render 0x518000 (0 writes).
  - SFE_MenuBkgrnd.w3d contains texture mappers: a rotate mapper (Speed=0.02 and −0.03 Hz) and sine (UAmp/UFreq) mappers. Their animation follows the APT real-time clock, so it is smoother at 60 at real-time speed.
  - Mapper internals follow the W3D source semantics (INFERRED).
- Mouse: W3DMouse draw 0x498CBC (CONFIRMED).
  - Default redraw mode 0 is the hardware cursor (G3-20: GD+0x9C6=1 at 0x642E38), so the OS moves it at full rate.
  - In mode 3 the animated-cursor frame is a float accumulator of timeGetTime delta × rate (0x498D6D..0x498DB7), i.e. real time.
  - Its per-draw setCursor(current) is idempotent.
  - The tooltip linger counter is the existing UI_TOOLTIP_LINGER site.
  - I found no per-render cursor thread state.

4. VIDEOS (CONFIRMED)
- APT-embedded VP6 movies (BinkMovie windows: credits movie, skirmish/campaign preview and flag videos *_with_alpha.vp6):
  - Window class vtable 0xC84DCC / base 0xC8CBD0.
  - Service (vt+0x14 0x9D8E00 → movie vt+0x28 0x92B5BB → stream vt+0x18 0x490BBF) runs only from AptPlayer::update 0x624F31 → 0x814218. That is A-only through GATE_CU_APT.
  - Draw (vt+0xC 0x9D8BE1 → 0x92B447 → Display vt+0x104) is pure.
  - The stream target frame is wall-clock: 0x4909A9 computes (now−start)·fps/1000. In the flag-0x44 mode it advances at most 1 frame per call, so it can never run faster than real time.
  - The fade alpha in 0x92B5BB is absolute timeGetTime.
  - The end-of-movie AS callback (_CallOnLastFrame 0x975181) fires inside the A-only service.
- Full-screen movies: Display::update is A-only and the draw at 0x44A0D0..0x44A1A1 is pure.

5. MENU ANIMATIONS / AS TIMERS
- AptPlayer::update 0x624EE1 passes dt = min(timeGetTime−last, 60 ms) to the APT runtime 0xAE3150. The runtime accumulates it and steps each movie by whole frames of movie+0x24 ms (0xAE1917..0xAE1937).
- MainMenu.const uses setInterval-driven increment fades (faderEngine, fadeSpeed, increment). These run inside the APT update, so GATE_CU_APT must stay A-only. An ungated update would risk 2x fades (INFERRED APT interval semantics).
- Because the movies are 33 ms per frame, a B-side APT update could not show extra frames anyway.

6. CREDITS (CONFIRMED)
- MainMenuToCreditsScreen 0x91B5E9 creates the CreditsManager [0xDEBF50] (vtable 0xC8B8A8), sets Shell+0x5D=1 (0x91B6CE) and calls GE vt48(100) (0x91B6DC). The exit at 0x91B796/0x91B7AA clears Shell+0x5D and restores GD+0x28.
- Credits scroll 0x9C6BB5: frame counter +0x38++, and every UpdateRate calls y += ScrollRate. It is driven by Shell::update through AptMainMenu::update case 4.
- At 60 the pacer would take P from GE+0xC = 100: T = 5 ms, 200 renders/s. The credits screen must stay at 30 (stock).

7. MODE-CONTROLLER SPEC FOR THE SHELL (replaces 'game mode (menu or multiplayer)' for mode 9 only)
- Allow 60 when all of these hold:
  - GL+0x110==9, GL+0x125==0, and LW view [0xDE4958] null or +0x18==0. This keeps the War of the Ring setup screen with its embedded LivingWorldMap preview (MpGameSetup) at 30.
  - TheNetwork [0xDE4468]==0, [0xDE8D90]==0 (LAN lobby screen instance, ctor 0x848C3D, INFERRED) and [0xDEA36C]==0 (online shell instance, 0x91D7A3, INFERRED).
  - TheShell [0xDE7890]!=0 && Shell+0x5D==0 (credits).
  - GE+0xC == GD+0x28 (no FPS override; in the shell only the credits change it).
  - GD+0xAF2/0xAF3==0 (intro), GD+0xAF6==0 (loading; set at 0x62A221, cleared at 0x628F87), GD+0xD45==0 (Create-a-Hero), GD+0xBBD==0, GD+0x26==1.
  - GL+0x9C==0 && GL+0xA8==0 (no pending start / fade), and the UI-sequence queue [0xDE8900] is empty.
  - GC+0xC9==0, only if GATE_GC_DISPMODE is not installed.
  - Optional: Display+0x38==0 (no full-screen movie).
  - Keep mode 4 (shell map) and mode 7 (CaH) blocked.
- Start rule: m_frame does not advance in mode 9 (INFERRED). GC+0xC8 is set only from logic (0x62E5D1, 0x62EE3E) and from the 0x494xxx/0x5E4512 paths, and the log shows frame 0. So StartBlockReason's 'm_frame<8' must not be used for the shell. Use ≥30 consecutive renders that satisfy the predicate.
- Telemetry tick checks must accept Δm_frame=0 in the shell, as they do while paused.
- Transitions:
  - A game start from the menu is processed in the logic step after B (the log shows the skirmish load inside logic sub 1). Loads and new games from APT callbacks run in A. Both go through GameEngine::reset, where the RESET hook drops g_m60 immediately; this is the path already used by in-game restart and load.
  - Leaving a game to the menu: the reset hook, then the shell start rule.
- Suggest an ini switch MainMenu=1.

8. NOT TRACED / OPEN
- How APT receives mouse clicks (propagate vs update) was not traced, so whether GC+0xC9 can really be set after A's check is INFERRED. The site is cheap insurance.
- The SFE_MenuBkgrnd mapper code was not disassembled.
- The LAN and online singleton meanings are INFERRED from screen create/destroy code.
- The MpGameSetup LW preview path (LW view vt24 0x49BEFD, window draw 0x975441) is left at 30.

## RISKS

- Expectation risk: shell APT movies are authored at 33 ms per frame and must stay A-only. 60 FPS in the menu will therefore not make menu animations smoother. The gain is the 3D background mapper animation and about 16 ms lower display latency. The GPU cost doubles at 4K: the B-render redraws the APT, the View3D scene and the video. Tell the user before investing in phase 7.
- Credits: at FPS limit 100 the pacer would run 5 ms halves (200 renders/s). The credits screen must be blocked (Shell+0x5D or GE+0xC != GD+0x28).
- Start rule: m_frame does not advance in mode 9 (INFERRED from the GC+0xC8 setters and the log). Reusing StartBlockReason's m_frame<8 test could block the menu forever, or let it start immediately after a reset. Use a render-count rule instead.
- Unverified singletons: [0xDE8D90] (LAN lobby) and [0xDEA36C] (online shell) are INFERRED from screen create/destroy code. A false negative would let a multiplayer lobby run at 60. Its screen updates are still throttled by Shell::update and its network pumps were not traced. Keep the TheNetwork check and confirm both pointers at runtime.
- War of the Ring setup (MpGameSetup) draws the Living World scene through the embedded LivingWorldMap window, with the LW view active and suspended. The current 'Living World map transition' block must stay for mode 9, or that screen needs its own audit.
- Camera sites run on an empty world in the shell. S1's B path jumps to 0x48C701, but the drawable pass early-outs because sync is frozen. SCENE_OPEN/RESTORE have no valid record or no visible effect. Check that the telemetry shows no WARN 'camera swap still active' and no tick-check errors (Δm_frame=0 must be accepted as in pause).
- One-shot events on B (harmless): the first draw of an APT embedded window runs its '_Init' AS function (e.g. BinkMovieInit) from the render callback 0x814BEC. That can be a B-render, so the event fires about 16 ms earlier, never twice.
- Not traced: APT mouse-click delivery (propagate vs update); the internal code of the W3D texture mappers; the full-screen campaign movie path with Display::playMovie pausing (recommend blocking while Display+0x38 != 0).

## RECOMMENDATION

1. Implement menu-60 in the mode controller only (src/frame_ctl.cpp). Add a mode-9 branch, ShellBlockReason, with the predicate from details §7:
   - mode 9, no LW view, no network/LAN/online screen;
   - not credits (Shell+0x5D==0 and GE+0xC==GD+0x28);
   - not intro/loading/Create-a-Hero/fast-forward;
   - no pending start/fade/UI sequence; no display-mode change pending.
   Use a render-count start rule (≥30 qualifying renders) instead of m_frame<8. Keep mode 4 and mode 7 blocked. Add an ini switch (MainMenu=1).
2. Keep every existing gate as it is. GATE_CU_APT, GATE_GC_SHELL, GATE_GC_WM, GATE_GC_DISPUPD, GATE_CU_AUDIO, INT_OVL_FADE, INT_UIPART and UI_TOOLTIP_LINGER already make the shell exact. Do not ungate the APT update: the movies are 33 ms per frame, so there is nothing to gain, and setInterval fades would risk running 2x.
3. Add GATE_GC_DISPMODE (0x6488A1, 5 bytes, verified) and reclassify coverage entry 0x6488A1 as a gate. SHELL_THROTTLE_60 is optional.
4. Telemetry for the first test:
   - per-tick checks must accept Δm_frame=0 in the shell;
   - log the 30/60 transitions at menu→game and game→menu (the reset hook must fire before any load);
   - confirm that [0xDE8D90] and [0xDEA36C] are 0 in the SP menus and non-zero in the LAN and online screens;
   - with the credits screen open, the log must show 'credits' and 30 FPS.
5. Before investing more, tell the user the expected visual result: same menu animation cadence (the APT movies are authored at 30.3 fps), smoother rotating background effects, lower latency, and twice the GPU load in the menu.

## SITE {
 "id": "GATE_GC_DISPMODE",
 "address": "0x6488A1",
 "length": 5,
 "original_hex": "e8 07 d5 ff ff",
 "original_asm": "0x6488a1: e8 07 d5 ff ff  call 0x645dad  ; GameClient pending display-mode change (runs only if GC+0xC9; preceded by cmp byte [ebx+0xC9],0 / je 0x6488A6 / mov ecx,ebx). ECX=EBX=TheGameClient",
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:GATE_GC_DISPMODE_STUB>",
 "stub": "GATE_GC_DISPMODE_STUB (pure asm; entered by the call at 0x6488A1 with [esp]=0x6488A6, ECX=EBX=TheGameClient):\n  cmp byte ptr [g_skipB],0 ; je .run\n  push eax ; mov eax,fs:[0x24] ; cmp eax,[g_mainTid] ; pop eax ; jne .run\n  ret                         ; 60-mode B-render on the main thread: GC+0xC9 stays set, the next A-render services it at the same site\n.run:\n  jmp dword ptr [k_645DAD]    ; =0x645DAD, tail call: same ECX, same return address 0x6488A6, no stack args (0x645DAD ends with plain RET at 0x645F64)\n30 mode / A-render / other thread: identical to stock. Reclassify coverage entry 0x6488A1 from 'run' to 'gate'.",
 "live_after": "At 0x6488A6 nothing from the span is live: ECX is reloaded (mov ecx,[0xDE4830]), EAX is reloaded at 0x6488AC (mov eax,[ecx]). EBX=TheGameClient is used at 0x6488B1 and is callee-saved, as are ESI/EDI/EBP (0x645DAD is an MSVC thiscall with EH prolog). EFLAGS dead. x87 depth 0. No XMM live.",
 "branch_into_span_check": "Interior 0x6488A2..0x6488A5: brute rel8/rel32/jcc32 decode over all executable sections plus a whole-image abs32 scan (lw_tmp/ui/brute.py): 0 hits. 0x6488A6 is the je target from 0x64888F and 0x64889D, i.e. the span end (allowed). Adjacent GATE_GC_IGUI starts at 0x6488A6; no overlap. Bytes identical in game.dat, delayfix.dat and game820.dat.",
 "purpose": "Keep the display-mode change event A-only. It recreates TheShell, resets the device to the new resolution and reloads MainMenu.apt. The request comes from Options apply 0x91EF0F→0x6455F3 or the AptMainMenu revert 0x91B2A7→0x645622. Without the gate, a request raised after A's check (e.g. during propagate 0x6324A1) is executed inside the following B-render."
}

## SITE {
 "id": "SHELL_THROTTLE_60",
 "address": "0x75E210",
 "length": 7,
 "original_hex": "0f 2f 05 0c c9 c2 00",
 "original_asm": "0x75e210: 0f 2f 05 0c c9 c2 00  comiss xmm0, dword ptr [0xc2c90c]  ; Shell::update throttle (elapsed ms as float) vs 32.333f; jb 0x75E24D at 0x75E217 skips the screen updates",
 "kind": "operand_redirect",
 "replacement_hex": "0f 2f 05 <abs32:g_shellThrottle>",
 "stub": "No code. DLL float g_shellThrottle. At install, copy the 4 bytes at 0xC2C90C (55 55 01 42 = 32.333334f) so 30 mode is bit-identical. The mode controller sets 26.0f when entering 60 mode and restores the copied value when leaving (at the pair boundary, like the other mode flips). OPTIONAL.",
 "live_after": "Same instruction semantics. XMM0 is unchanged, EFLAGS come from comiss as before, and jb 0x75E24D at 0x75E217 consumes CF. EAX/ESI/EDI/EBX as before. x87 depth 0.",
 "branch_into_span_check": "Interior 0x75E211..0x75E216: brute decode has 1 hit, rel8 at 0x75E1B3→0x75E213. It is a false positive: 0x75E1B3 is the disp8 byte of 'mov eax,[esi+0x74]' (8b 46 74) at 0x75E1B1. abs32 scan: 0 hits. listing.asm has only this one reference to 0xC2C90C. Bytes identical in all three builds.",
 "purpose": "Optional robustness. Shell::update is A-only (GATE_GC_SHELL) and sets last=now each time it fires. A-renders are 33.000 ms apart by QPC, but timeGetTime with 1 ms resolution plus work jitter can read 32 ms, and then a whole screen update is dropped (66 ms gap). A 26 ms threshold fires on every A-render. It can never exceed the A rate in steady state: a call from the LW logic path after B comes about 16.5 ms after A's fire, which is below 26. The worst case under irregular timing is 1/26 ms. Impact without the site: occasionally late AptMainMenu state-machine steps and music-restart checks. Slower only, never 2x."
}

## VERIFIER
The analysis gets the shell's structure and most addresses right. Its central conclusion holds: the existing A-only gates already cover the menu's per-render work, and only the mode controller needs real changes. It is wrong about the start rule and telemetry, and it overstates the benefit.

**Confirmed by re-checking in game.dat (byte-identical in delayfix.dat and game820.dat):**
- ShellMapOn=No and the Shell+0x52 background path.
- The APT movies are authored at 33 ms per frame, and the runtime steps them by whole frames of movie+0x24.
- AptPlayer::update clamps dt to 60 ms.
- Shell::update has a 32.333f real-time throttle.
- The credits screen sets FPS 100 and later restores it. The pacer's StockFrameMs then gives 5 ms halves.
- The setters and service path of GC+0xC9, and the bytes, boundaries and branch-into-span checks of both proposed sites. My own brute-force decode found only the known false positive at 0x75E1B3.

**Refuted:**
- The m_frame / start-rule section. The frame stepper 0x6325A0 sets GC+0xC8=1 at 0x6325FB on every step that is not halted, so GC+0x10 advances once per stock render in the menu. GameClient::reset (vt24, 0x64784B) sets it back to 0.
- The log evidence is misread. The 'frame' in the stall lines is GL+0x40 (telemetry.cpp:699), and it reads 0 in the 60-mode battle lines too.
- So the existing StartBlockReason (GC+0x10<8) and FrameState::BeginIteration (s==1 && GC+0xC8) already work in the shell. The tick checks need no Δm_frame=0 exception.
- The claimed ~16 ms latency gain. Input, the APT update (hover and click), Shell::update and the popups are all A-only, so menu responses keep the 30 Hz cadence. The hardware cursor does not depend on the frame rate anyway.

**Net effect for the user:** the visible gain is limited to the View3D background mapper animation. Even that is only plausible: whether the AotR menu actually renders that model was not checked. In exchange, the menu's GPU load roughly doubles at 4K.

- [confirmed] No 3D shell map: ShellMapOn = No, so Shell::showShellMap sets Shell+0x52=1 and the menu runs in mode 9 with no game
  aotr\data\ini\gamedata.ini:11133 and data\ini\object\gamedata.ini:3 both say 'ShellMapOn = No'. 0x75DE28..0x75DE3C: if GD+0xAF0==0 then AL=1, stored at [esi+0x52]. The mode-9 writers exist (GL init 0x6301C9, clearGameData 0x7793C5). That the menu really is mode 9 is INFERRED: the log reason 'game mode (menu or multiplayer)' fires for any mode outside {0,2,6} that is not the LW map.

- [confirmed] Every shell APT movie is authored at 33 ms per frame, so A-only APT updates look the same at 60
  MainMenu.apt movie header at 0x514 reads (97 frames, ptr 892, 1024, 768, 33). SkyrimMenu.apt at 0x1DCF0 reads (622 frames, 1024, 768, 33). The runtime adds dt to an accumulator and steps only when it reaches movie+0x24 (0xAE192A..0xAE1937). AptPlayer::update clamps dt to 60 ms (0x624F3D..0x624F52). I checked only these two files, not all 11.

- [confirmed] Shell::update throttles screen updates with timeGetTime-last >= 32.333f and then sets last=now; it is A-only via GATE_GC_SHELL
  0x75E201..0x75E217 (cvtsi2ss, comiss against [0xC2C90C]=55 55 01 42=32.3333f, jb 0x75E24D). Inside the throttle it runs the screen-stack vt8 loop, [esi+0x60] vt28 and 0x63F3BF, then last=now at 0x75E248. GATE_GC_SHELL is at 0x648891 in sites.json (g_m60/g_uiTick template). LW logic 0x6BE50E also calls Shell vt28 when LWL+0xB4 is set; the throttle still bounds the rate.

- [confirmed] GATE_GC_DISPMODE at 0x6488A1: bytes e8 07 d5 ff ff, call 0x645DAD, safe 5-byte call gate
  The bytes match in all three binaries, and the target is 0x6488A6-0x2AF9 = 0x645DAD. 0x645DAD ends with a plain RET at 0x645F64. The only writers of GC+0xC9=1 are 0x64560B (0x6455F3) and 0x645640 (0x645622), and their callers are 0x91EF0F and 0x91B2A7. My brute rel8/rel32/jcc32 and abs32 scan found no hits in 0x6488A2..0x6488A5. Two notes: (1) The proposed stub uses g_skipB plus a fs:[0x24] check, while every other GameClient gate uses the g_m60/g_uiTick template; they behave the same during clientUpdate, but the standard template should be used for consistency. (2) Adding the site changes kSiteCount, the telemetry arrays, stubs.asm and coverage.

- [uncertain] Without the gate, a GC+0xC9 request raised after A's check runs inside the B-render
  The ordering is confirmed: W3DDisplay::draw at 0x648869 and Shell::update at 0x648891 both come before the GC+0xC9 check at 0x648896, and MessageStream propagate at 0x6324A1 comes after GameClient::update. But Options apply (0x91EF0F via the tail-jump at 0x91F03E) and the revert callback 0x91B2A7 (registered at 0x91C026 in the AptMainMenu state code) most likely run from A-only code that executes before 0x6488A1: the APT update, Shell::update or the popups. Then they are serviced on the same A. The gate is cheap insurance, not a proven fix.

- [confirmed] SHELL_THROTTLE_60 at 0x75E210 (operand redirect of the 32.333f constant)
  The bytes 0f 2f 05 0c c9 c2 00 match in all three builds. 0xC2C90C has exactly one reference, the disp32 at 0x75E213. The rel8 hit from 0x75E1B3 is the disp8 of 'mov eax,[esi+0x74]' at 0x75E1B1, a false positive. However, the occasional missed fire it targets exists in stock as well, and the site makes 60 mode deviate from stock cadence. I recommend dropping it.

- [confirmed] Credits: Shell+0x5D=1 and GE vt48(100); the exit restores GD+0x28; at 60 the pacer would use 5 ms halves
  0x91B6CE writes [Shell+0x5D]=1, and 0x91B6DA/0x91B6DC pushes 0x64 and calls [GE vt+0x48]. 0x91B796 clears +0x5D, and 0x91B7A5/0x91B7AA push GD+0x28 and call vt48. pacer.cpp:193 StockFrameMs reads GE+0xC, so P=10 ms and the halves are 5 ms. In Pacing=nominal the pacer ignores GE+0xC, so the block should key on Shell+0x5D, not only on GE+0xC.

- [refuted] m_frame does not advance in mode 9; GC+0xC8 is set only from logic (0x62E5D1, 0x62EE3E) and 0x494xxx/0x5E4512; the log shows frame 0
  The frame stepper 0x6325A0 calls 0x603452 (debug-frozen query, normally false) and then sets GC+0xC8=1 at 0x6325F8..0x6325FB on every step that is not halted. clientUpdate 0x632423..0x632440 then increments GC+0x10 on the next render. In 60 mode the HALT site 0x6325D5 forces the halted branch on X, which keeps the rate at 30/s. The 'frame' in the log's stall lines is logic+0x40 (telemetry.cpp:699), not m_frame, and it also reads 0 in the 60-mode battle lines. The log shows logic subs 1..6 running in the menu, so the stepper is active there.

- [refuted] The start rule must not use m_frame<8 (it could block the menu forever); use >=30 qualifying renders instead
  GameClient::reset (vtbl 0xC04898 slot 0x24 = 0x64782E) writes GC+0x10=0 at 0x64784B. The counter then rises by one per stock render in the shell, so StartBlockReason's GC+0x10<8 works as an 8-render guard after boot or reset. The other start condition, FrameState::BeginIteration (frame_state.cpp:9: s==1 && GC+0xC8), which the analysis does not mention, is also met in the shell. A longer guard is optional and not required.

- [refuted] Telemetry tick checks must accept dm_frame=0 in the shell
  m_frame advances by 6 per tick in the shell, the same as in battle. The current log shows '515 tick checks, 0 errors' with 28 checks in 30 mode. Most of those almost certainly fall in the menu period (17:25:04 to 17:25:12), since the in-game 30-mode window was only about 0.4 s (INFERRED attribution). CloseTickWindow (telemetry.cpp:175) needs no change.

- [confirmed] The APT render swaps W3D sync to an absolute APT real-time clock with a 100 ms clamp, and it is idempotent per render
  0x4A8A3C..0x4A8A83: esi = [DCBBBC] + (timeGetTime - [DCBBC0]). If esi - [DCBBC4] > 100, it rebases to DCBBC4+100. It then calls set-sync 0x516E20(esi) and stores DCBBC4=esi. The exit at 0x4A8AAD..0x4A8ACD restores the saved values. Extra writes ([0xDD1EDC]=0.0, [0xD9B068]=0) are constant stores and also idempotent. So View3D mapper animation would advance per render at real-time speed. That the mappers use sync time remains INFERRED.

- [refuted] 60 FPS gives about 16 ms lower display latency in the menu
  Input sampling (GATE_GC_KBDSEL/MOUSESEL), the APT update including the hover handler (GATE_CU_APT purpose text), Shell::update, popups and the UI sequence are all A-only. Every visible reaction therefore appears on an A-render at the stock 30 Hz times, and the B-render only repeats it. The hardware cursor (GD+0x9C6=1 at 0x642E38) is moved by the OS regardless of frame rate. There is no measurable latency gain for menu interaction.

- [uncertain] Video service is A-only through AptPlayer::update 0x624F31 -> 0x814218; the draw is pure
  The call to 0x814218 at 0x624F31 inside AptPlayer::update (gated by GATE_CU_APT) is confirmed, and Display::update is gated (GATE_GC_DISPUPD). I did not re-trace the stream-target math (0x4909A9) or the purity of the BinkMovie draw chain. The depth-1 write scan of the video layers 0x65D03B is clean, but it does not follow virtual calls.

- [uncertain] LAN lobby [0xDE8D90] and online shell [0xDEA36C] singletons identify multiplayer screens
  The references exist: 0x848C3D creates it at 0x848D29 and 0x847189 clears it at 0x84722F. Likewise 0x91E133 creates at 0x91E1D6 and 0x91DBCF clears at 0x91DC4C. Their meaning is still INFERRED and needs runtime confirmation. TheNetwork is NULL in the lobbies, so the existing 'network game' check does not catch them.

- [confirmed] Ungated per-render calls in the shell are harmless (propagate, CRC guard, 0x63239D, D3D queue, freeze queries, draw, vt90)
  This matches the sites.json coverage classes. One omission: 0x648591 TheSnowManager::update runs on every render and consumes the W3D sync delta. Its counters are gated elsewhere and sync is effectively idle in the shell, so it is harmless, but it should be listed.

## MISSED
**Still missing for a safe 60 FPS main menu**

1. **Mode controller.** The analysis's start-rule changes are not needed: the existing StartBlockReason and FrameState::BeginIteration already work in the shell. Keep the 'Living World map transition' block for mode 9 so the War of the Ring setup preview stays at 30. Note that a leftover active LW view after quitting a Living World campaign would keep the menu at 30, which is safe. Make 'Display+0x38 != 0' (full-screen movie playing) a required block, not an optional one. Block credits on Shell+0x5D, not only on GE+0xC, because Pacing=nominal ignores GE+0xC.

2. **Credits entry timing.** The credits screen is entered from inside the A-render (Shell::update → AptMainMenu::update). GE+0xC becomes 100 before 60 mode can drop, and BeginIteration switches off only at the next X boundary. So one B half is paced with P=10 ms. This is harmless but should be expected in the log.

3. **Runtime confirmations before shipping:**
   - GL+0x110 value in the main menu, Options, Skirmish and Campaign screens.
   - TheLivingWorldLogic+0xB4 and the LW view +0x18 in the plain menu.
   - The LAN and online singletons, null in the SP menus.
   - Cursor redraw mode (AotR could configure an animated W3D cursor).
   - Whether the active AotR menu (MainMenu.apt or SkyrimMenu.apt) actually shows the Background.apt View3D model. Without it there is essentially no visible gain at all.

4. **Implementation chores** if GATE_GC_DISPMODE is added:
   - stub in stubs.asm, using the standard g_m60/g_uiTick template;
   - sites.json entry, and reclassify coverage entry 0x6488A1 as a gate;
   - regenerate sites.gen.h / kSiteCount, run verify_sites.py.

   The log reports 144 installed sites against 148 in sites.json. Reconcile this before counting.

5. **Untested shell behaviour at 60:** alt-tab / minimize, Options apply with a resolution change (device reset and DisplayBlockReason re-evaluation), and the menu → loading screen → game handoff (reset hook firing inside the Y logic step after a B-render), measured with telemetry.

6. **User-facing expectation.** APT animation cadence, hover and click response, and cursor will look and feel exactly as they do at 30. The only change is a possibly smoother background mapper animation, at about twice the GPU cost in the menu at 4K. The user should decide whether that is worth it before phase 7 work begins.