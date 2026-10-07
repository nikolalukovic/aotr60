# AREA lw-ui

## SUMMARY
LW strategic-map UI audit for phase 6 (read-only). Result: the LW UI needs no new mandatory patch sites. I found no per-render integrator and no one-shot draw request in the LW-specific UI. The only item proposed is an optional telemetry counter.

1. How the strategic HUD runs. It is entirely APT: StrategicHUD.apt plus the movies it loads (StrategicPalantir, DetailsTray, Timeline and others, overridden by AotR's AOTRStrategic*.big). AptPlayer::update 0x624EE1 is A-only through GATE_CU_APT and is wall-clock based. APT movies advance on a fixed-step accumulator per movie, so B simply repeats A's APT frame. The LW UI state machines (0x645750), the LW view vt6C (LWM::update and the LW Palantir) and InGameUI::update are already A-only gates.

2. What runs on every render, B included, and why it is safe.
- APT render 0x46203E → 0x62212F.
- Prologue 0x4A89BD sets the W3D sync to an APT real-time clock and restores it afterwards (absolute time, so nothing accumulates). Mark/hide sweep 0x81424C/0x814278 and the button-instance hit list 0xAF78A0 are rebuilt identically on each render.
- Render callbacks used by the strategic movies are all pure:
  - RenderImage 0x8158B5 and RenderImageDisabled 0x8156FD;
  - TimerOverlay 0x815920. Its strategic getter StrategicCommandButton 0x9F3C74 is pure (fld [this+0x3C] plus a one-time static color); the base CommandButton getter 0x9D6EAD is a constant.
  - _RenderText 0x9E362D;
  - _RenderProgress 0xA03F94 and _ProgressOverlay 0xA0A4A9 (fractions from integer turn fields);
  - AptPalantir::RenderGlobe 0x6D42EE → 0x504019. This is a fixed-camera W3D scene render on the APT real-time clock: smooth and stock speed at 60, but it costs a full scene render on every render.
- Embedded-window callback 0x814BEC: idempotent in steady state.
- So the strategic progress clocks are pulled from update-owned fields, not consumed by the draw. There is no UI_PBCLOCK-style problem on the LW map.

3. Generic sites that already cover the drawFrame LW tail: INT_UIPART (0x44A0B3), UI_TOOLTIP_LINGER (mouse draw 0x44A0C4), INT_OVL_FADE (0x44A12E), INT_SUBT_STATE/INT_SUBT_SCROLL (subtitles, which are enabled on the LW path through 0x441E4A at 0x44A1D9), C7 and the UI_PBCLOCK sites. The open sites.json todo for display vt+0x190 at 0x44A1FD also applies on the LW map.

4. TheShell->vt28 in the LW logic path. It runs at sub 1 inside TheLivingWorldLogic::update (0x632A85..0x632A92), which follows the B-render at stock cadence. Shell::update 0x75E1D3 is throttled to at least 32.33 ms of wall time. GameClient::update calls it on the LW map only if Shell+0x5C is set (0x64887C..0x64888F, A-only through GATE_GC_SHELL). No change needed.

5. Embedded LivingWorldMap window. It appears only in MpGameSetup (a shell screen). There the LW view is suspended, and 0x6BFF7B sets GL+0x125 = active && !suspended = 0. The strategic map draws through vt20 at 0x449F45 instead, so a phase-6 controller keyed on GL+0x125 plus mode 8 excludes it automatically.

6. Correction to the existing notes. AptPlayer+0x328 is not a movie-load flag. Only the ShowCommandInterface helper 0x6228DE (single caller 0x92F6C3) sets it. If called outside an APT update, the helper runs AptPlayer::update synchronously. Only when it is called during an APT advance (+0x312 set) is the update left pending for the next APT render. So a pending update on a B-render needs a doubly nested call and is practically unreachable. The optional TEL_APT_PENDING_B counter (0x62213B) can confirm it stays at 0.

7. AotR has no LW-specific UI code. Its only per-update code is the Palantir sampler, which stays A-only on the LW map through GATE_LWM and GATE_PALANTIR (return address 0x612272).

8. No project or game files were changed. My scratch files are only in <analysis workspace>\lw_tmp\ui; the lw_tmp\armies folder and lw_tmp\drawframe.txt, rd.py and sites_lw.txt were already there.

## COVERAGE
Examined:
- Project docs and site data: PLAN.md §1 and phase 6, README, sites.json (128 sites with group notes), G5 G5-06, ../agents/camera_ui_audio.md item 20, the phase-4a sweep summary (ui-progress and aotr-hooks areas; a working file, not in the repo).
- drawFrame LW branch and tail, disassembled in full: 0x449CF8..0x44A289.
- GameClient::update LW conditions: 0x6484B2..0x6488D7.
- APT render chain: 0x46203E, 0x4A89BD/0x4A8A96, 0x62212F, 0x81424C/0x814278, 0xAE09F0 → 0xAF78A0/0xAF4170/0xADFEA0. The traversal 0xB0B920 was not traced beyond its entry; the earlier 4a write scan of the APT runtime is relied on.
- AptPlayer::update 0x624EE1, 0x624514 and 0x624C68 (header only).
- Render-callback registration: 0x624348 call sites, 0x92B194 callers, 0x815C7D.
- Strategic callbacks and getters: 0x9E362D, 0xA03F94, 0xA0A4A9, 0x9F3C74, 0x9D6EAD, 0x6D42EE → 0x504019.
- Timer registration 0x92B27D and its callers 0x9D72DB, 0x9F4287 and 0x93178C (spellbook).
- Embedded windows: 0x8142D2, 0x814BEC, 0x812FD4 → 0x975659, window vtable 0xC84E48.
- LW setActive 0x6BFF7B and LW vt20/vt24/vt50.
- Mouse tooltip draw 0x5EE7A8.
- Shell::update 0x75E1D3 and TheLivingWorldLogic::update 0x6BE50E (start only).
- LW UI state machine 0x8385A3 (A-only, read for context).
- Readers of GE+0x3C, and callers of LW vt3C/vt38/vt34.
- APT asset scan (strings only) of rotwk\apt, bfme2\apt, aotr\*.big and the rotwk patch bigs for TimerOverlay, _WindowId, LivingWorldMap, RenderImage and C++ callback names.

Not covered (other areas, or not traced):
- LW draw vt20 0x49B618, LW camera/view tail 0x49AB21/0x49AB3E, moveTo 0x6BF78E/0x6BFB88, the LW manager and the scene render objects.
- The APT runtime's internal render traversal beyond its entry.
- The class at vtable 0xBDB550 with mode +0x678 (non-UI LW projections 0x450F8C/0x452152).
- AS-to-C++ callback bodies that run inside the A-only APT update.
- No runtime test.

Tools: scratch scripts in <analysis workspace>\lw_tmp\ui (mem.py, vt.py, ptr.py, xref.py, brute.py, up.py, ctx.py, drawframe.txt, gcupdate.txt). I created only lw_tmp\ui; lw_tmp\armies, drawframe.txt, rd.py and sites_lw.txt in lw_tmp were already there. No project or game files were created, changed or deleted.

## FINDING [low] LW strategic HUD has no per-render integrators or one-shot draw requests; strategic clocks are pull-based pure getters
The LW strategic HUD is APT: StrategicHUD.apt plus the movies it loads (StrategicPalantir, StrategicDetails*, StrategicChecklist, StrategicNextTurnInd, StrategicEndTurnButton, LivingWorldUI and others, with AotR overrides AOTRStrategicPalantir/AOTRStrategicDetailsTray/AOTRTimeline). Every C++ render-time hook these movies use is a pure read and draw. There is no consume-per-draw request like the battle UI_PBCLOCK case. The production and turn clocks read fields that only update code writes.
EVIDENCE: - Render-callback registration goes through 0x624348; direct callers are 0x6D6288, 0x7830DE, 0x814F3B.., 0x815FB1/0x815FF5/0x816039, 0x91AA1B, 0x926781, 0x92B1CB (wrapper 0x92B194) and 0x92E724.
- Strategic callbacks:
  - RenderImage 0x8158B5 and RenderImageDisabled 0x8156FD: image lookup 0x6237C0 then Display drawImage 0x44CF58.
  - TimerOverlay 0x815920: functor through AptPlayer+0xB8. StrategicCommandButton (ctor 0x9F4287, 'StrategicCommandButton') registers getter 0x9F3C74 = fld [this+0x3C] plus a static color initialised once. The base CommandButton subobject vtable 0xC841E4 slot +4 is 0x9D6EAD, which returns the constant 1.0 (no clock).
  - _RenderText 0x9E362D (class 0xC8DA00): DisplayString vt3C/vt34 draw.
  - _RenderProgress 0xA03F94 ([this+0x24]-[this+0x28])/[this+0x24] and _ProgressOverlay 0xA0A4A9 ([+0x20]-[+0x24])/[+0x20]: both call 0x4A427E inverse clock.
  - AptPalantir::RenderGlobe 0x6D42EE → W3DAptPalantir vt3C 0x504019: a fixed sin/cos camera, then scene render 0x518000.
- APT-string scan of rotwk\apt and bfme2\apt:
  - TimerOverlay appears only in StrategicHUD, StrategicDetailsArmyRetinue, InGameSpellBook, libInGameUI and PlayerTribute.
  - StrategicPalantir.big and AOTRStrategicPalantir.big reference only 'AptPalantir::RenderGlobe'.
  - LivingWorldUI.big has no C++ callbacks.
REC: No site. Keep GATE_CU_APT, GATE_GC_LWVIEW, GATE_GC_LWUI, GATE_LWM and GATE_PALANTIR as they are. Phase-6 visual check: StrategicCommandButton build/train clocks and build-queue _ProgressOverlay must not blink between A and B.

## FINDING [low] APT render runs on every render, but its per-render state is idempotent; the APT 3D globe uses an absolute real-time clock
The APT render 0x46203E runs on A and B. Its state work:
- the W3D sync swap to the APT real-time clock 0x4A89BD, restored by 0x4A8A96;
- the embedded-window mark/hide sweep 0x81424C/0x814278;
- the button-instance hit-test list, which 0xAF78A0 clears and the traversal rebuilds;
- matrix-stack resets 0xAF3FC0/0xAF4070.
All of these are rebuilt from the same APT display list on A and B, so B adds nothing. RenderGlobe (StrategicPalantir) animates on the APT clock dcbbc4 = dcbbbc + (timeGetTime - dcbbc0), so at 60 it advances by about 16.5 ms per render at the correct real-time speed. The cost is a full W3D scene render every render, so it doubles in 60 mode.
EVIDENCE: - 0x4A89BD: saves [0xDD1E0C]/[0xDD1E10], calls Sync(dcbbc4) then Sync(iVar1) with iVar1 real-time based (rebased if more than 100 ms); 0x4A8A96 calls Sync(dcbbb4) then Sync(dcbbb8).
- 0xAE09F0: 0xAF78A0 ([0xDFD428]+0x10/+0x14 'aButtonInstanceList', used by hit tests 0xB0E220), 0xAF4170, 0xB0B920(root,0), 0xADFEA0.
- 0x814BEC: create on first placeholder, '_Init' one-shot (entry+0x28), unhide if status 0x10000000, reposition 0x813038, drawWindow 0x6C1B52, entry+0x24=1.
REC: No site. If the pacer debt telemetry shows LW-map overruns, consider a perf-only B-skip of the RenderGlobe scene render (vt3C call at 0x6D433A). That is visual-equivalent only if the globe has no animated content, so measure first.

## FINDING [low] AptPlayer+0x328 'pending update' is set only by the ShowCommandInterface helper, not by movie loads; a B-render update needs a doubly nested call
The existing notes (../agents/camera_ui_audio.md item 20 and the phase-4a sweep LOW finding) describe +0x328 as set 'once after a movie load'. That is not what the code does:
- The only writer is 0x6228DE (single caller 0x92F6C3, ShowCommandInterface); the other writer 0x622923 is dead.
- 0x6228DE calls the ActionScript function, then runs AptPlayer::update synchronously unless an update is already in progress (+0x312).
- The flag therefore stays pending for the next APT render only when ShowCommandInterface is called during an APT advance.
- An advance during A's own clientUpdate APT update is consumed by A's own APT render, which matches stock.
- A B-render can consume it only if a synchronous update triggered after A's draw (propagate, InGameUI::update or Shell, all A-only) itself calls ShowCommandInterface during its advance.
In that rare case B runs one extra AptPlayer::update with dt=max(34,~16.5) ms. APT time then leads by about 16.5 ms, the hover handler 0x624C68 runs once more, and AS callbacks could append UI messages half a pair early. All of this is UI-only.
EVIDENCE: 0x62212F checks +0x328 at 0x622136. Writers of +0x328: 0x622911 (0x6228DE), 0x622959 (0x622923, no xrefs), 0x624C1D (ctor 0x624B0C, writes 0). 0x624EE1 clears +0x328 before advance thunk 0xAE3150 and sets +0x312 only around that advance. Raw E8/E9 scan: 0x6228DE ← 0x92F6C3 only; 0x62212F ← 0x462069 and vtable slot 0xBFD220.
REC: Keep stock behaviour. Do not defer the pending update on B, which would render a movie whose requested update has not run. Optionally install TEL_APT_PENDING_B (expected count 0) and correct the sites.json/analysis wording.

## FINDING [low] The embedded LivingWorldMap APT window is shell-only (MpGameSetup) and keeps GL+0x125 = 0; the strategic map draws the LW scene through vt20
PLAN phase 6 lists 'the embedded APT window' for audit. The APT embedded-window factory 0x8142D2 creates a 'LivingWorldMap' window (class vtable 0xC84E48, ctor 0x975659). Its draw slot +0xC, 0x975441, calls LW view vt24 0x49BEFD (scene render into the window rectangle) and a one-shot 0x840024 ([0xDE8D78]+0x2BE=1). Only MpGameSetup.big places this window (bfme2\apt, rotwk\apt and AotR bigs scanned). Its ctor sets the LW view suspended (vt50(1)) and then active (vt28(1)). LW setActive 0x6BFF7B writes GL+0x125 = active && !suspended = 0. drawFrame 0x449F2F..0x449F41 calls vt20 only when the view is active and not suspended. On the SP strategic map (mode 8, GL+0x125=1) the scene is therefore drawn by vt20 at 0x449F45, and the embedded window path is not used.
EVIDENCE: 0x8143A2: factory 0x812FD4 for 'LivingWorldMap' (0xC4FC84). 0x812FD4: new 0x2A4, ctor 0x975659. 0x9756BE/0x9756CB: vt50(1), vt28(1). 0x6BFF7B → 'byte[GL+0x125] = (active && !suspended)' (store at 0x6C0069). 0x49BEFD requires +0x19 (suspended) && +0x18.
REC: For phase 6, allow 60 mode with GL+0x110==8 && GL+0x125!=0 (keep TheNetwork==NULL). No extra condition is needed for the embedded map because mode 9/4 and GL+0x125=0 already exclude it, and no site is needed for 0x975441/0x840024.

## FINDING [low] TheShell->vt28 in the LW logic step keeps stock cadence; the Shell call in GameClient::update is conditional on the LW map
TheLivingWorldLogic::update 0x6BE50E calls TheShell->vt28 (Shell::update 0x75E1D3) at its start whenever LW is active. The stepper calls it only at sub 1 (0x632A85 'cmp edi,1'), so in 60 mode it runs in the stock step after the B-render, at stock 5 Hz. Shell::update fires its screen updates only when at least 32.33 ms of wall time ([0xC2C90C]) have passed since the last fire. On the LW map GameClient::update calls it only when Shell+0x5C is set (0x64887C..0x64888F), and that call is A-only through GATE_GC_SHELL. The set of calls and the wall-clock spacing match stock, so the firing rate cannot change.
EVIDENCE: 0x632A79..0x632A92; 0x6BE50E 'if (+0xB4) TheShell->vt28()'; 0x75E1D3 throttle against [0xDE7894]; 0x648877..0x648893.
REC: No site. Include Shell::update fire counts per tick in phase-6 telemetry (expected equal to the stock baseline).

## FINDING [low] drawFrame LW-branch UI calls are the battle UI functions, already covered by existing sites; nothing in them reads the stepper fraction
LW branch at 0x449F2F: vt20 LW draw at 0x449F45 (other area). Then, unless LW view+0x14==2:
- WM+0x3C=1 and W3DInGameUI::draw 0x48EA29: preDraw 0x6A3B59, view draws vt1E4 0x48EDED / vt1EC 0x48EAD8, postDraw 0x6A0BC0 (LW-aware named-timer position, logic-frame based), repaint of layer 1 and the transition handler [0xDE3654] vt30;
- IGUI vt158 0x69B7BE (timeGetTime) when [ebp-0xd]==0;
- APT render;
- WM+0x3C=0 and repaint of layer 0.
The tail is shared with battles: UI particles 0x6A536C, mouse 0x498CBC/0x5EE7A8 (tooltip ShowToolTip/MoveToolTip only on text change, linger counter), display overlay 0x65CF05, letterbox 0x443939, subtitles (enabled on the LW map through 0x441E4A) with vt190 at 0x44A1FD, and transition draw 0x44A218. Readers of GE+0x3C: 0x48E0EE, 0x4B51B5, 0x4B686D, 0x4B6F12, 0x6443B0, 0x67171D, 0x6765B9, 0x676711, 0x67679B, 0x8A0365. None is LW-UI code, so SCENE_OPEN's presentation fraction does not reach the HUD.
EVIDENCE: Disassembly 0x449CF8..0x44A289 (<analysis workspace>/lw_tmp/ui/drawframe.txt); W3DInGameUI vtable 0xBDD9B0 slots +0x30/+0x150/+0x154/+0x158/+0x17C/+0x1E0..+0x1F0. LW projection vt3C 0x49A899 has only one caller, 0x450FB6 (non-UI W3D class), so no APT overlay is anchored per render to LW world positions.
REC: No new site. Phase-6 verification must check that INT_UIPART, UI_TOOLTIP_LINGER, INT_SUBT_STATE/INT_SUBT_SCROLL and INT_OVL_FADE counters are non-zero on the LW map. The sites.json todo for vt190 at 0x44A1FD (subtitle delete on B) applies on the LW map too.

## FINDING [low] AotR has no LW-specific UI code; its Palantir per-update sampler stays A-only on the LW map
AotR's 16 code hooks are init, logic, event and reset hooks, plus the Palantir per-update hook 0x6D57AF. AOTR-08 (0x6D7267) only registers the event callback AptPalantir::OnBttnHideStats. On the LW map the Palantir update is reached through LWM::update 0x6121C5 → 0x61226F, behind GATE_GC_LWVIEW, GATE_LWM and GATE_PALANTIR (return address 0x612272). AotR's strategic APT overrides are data that use only RenderGlobe/RenderImage. The dead debug hook 0x9A3AE0 is in vtable 0xC88344 slot +0x14 and executes HLT if reached; that is the same at 30 and 60 FPS.
EVIDENCE: ../gaps/G7_aotr_hooks.md AOTR-08 and the phase-4a aotr-hooks sweep; sites.json GATE_PALANTIR; strings in aotr\AOTRStrategicPalantir.big / AOTRTimeline.big / AOTRStrategicDetailsTray.big.
REC: No change. Expected phase-6 telemetry: g_cntPalRun with ret 0x612272 = stock per tick, g_cntPalSkip = 0.

## VERIFY SITE TEL_APT_PENDING_B -> confirmed
I only ran the read-only verification and wrote no files.

Site verification (all against <analysis workspace>/ghw/bin/game.dat). pefile is not installed, so I parsed the PE headers by hand with struct.
- Bytes: 0x62212F reads 56 8d b1 28 03 00 00 80 3e 00 74 08 8b 01 ff 50 28 c6 06 00 e8 04 21 1f 00 e8 63 e9 4b 00 5e e9 25 21 1f 00. The span 0x62213B 'mov eax,[ecx]' (2 bytes) plus 'call [eax+0x28]' (3 bytes) is 5 bytes and starts and ends on instruction boundaries. Resume is 0x622140 'mov byte [esi],0'.
- Branch into the span: my own brute scan found 0 hits. It decoded rel8 jcc/jmp/loop, E8/E9 rel32 and 0F 8x rel32 at every byte offset of every executable section, and searched the whole file for abs32 values 0x62213C..0x62213F. The only nearby branch is 'je 0x622143' at 0x622139, which lands outside the span.
- 30-mode path: the stub runs the displaced 'mov eax,[ecx]' and then 'jmp [eax+0x28]' with the return address 0x622140 already pushed by the replacement call. That is identical to the stock call. The vtable slots 0xBFD1F0+0x28 and 0xBDB780+0x28 both hold 0x624EE1, which ends 'jmp 0x624C68' (0x624F90), and 0x624C68 returns with a plain RET at 0x624EE0. The same tail-call pattern is already used and documented for GATE_CU_APT.
- Register and flag state: the stub leaves ECX unchanged and restores EAX after the thread check. ESI is callee-saved. EFLAGS are dead at resume: 0x622140 is a mov followed by calls. 0x81424C, 0xAE0AB0 (a jmp to 0xAE09F0) and 0x814278 open with ordinary prologues, and their 'push ecx' instructions only reserve stack space.
- B path: the stub's behaviour is identical in every mode. g_skipB plus fs:[0x24]==g_mainTid is the established predicate (sites.json line 209), and it excludes the load-screen APT render on its own thread.
- Runs per render on the LW map: in drawFrame's LW branch (0x449F39..0x449FA4, after the LW draw vt20 at 0x449F45), 0x449F7D..0x449F85 calls [0xDE3F0C] TheAptPlayer->vt30. vt30 is 0x46203E: its only abs32 pointer sits at 0xBDB7B0 = 0xBDB780+0x30. 0x46203E calls 0x62212F at 0x462069, so this runs on every render, A and B, whenever the LW draw path runs. The object is the same TheAptPlayer whose direct vt28 call in clientUpdate is gated by GATE_CU_APT (0x632449).
- Not already covered: the 0x62213B call is a different call site from 0x632449, sits in the draw path, and has no sites.json entry. The span does not overlap any sites.json site or AotR hook.
- Evidence: the writers 0x622911 (in 0x6228DE, which sets +0x328=1 and calls vt28 unless +0x312 is set) and 0x622923 match the disassembly. 0x624EE1 clears +0x328 at 0x624F6C before 'call 0xAE32C0', and 0xAE32C0 is a 'jmp 0xAE3150' thunk. +0x312 is set and cleared around that call (0x624F6E/0x624F77), so the evidence's "thunk 0xAE3150" is accurate through the thunk.

Caveat, not a defect: "expected 0" is a hypothesis. If something sets +0x328 while +0x312 is set, the flag is set after the clear at 0x624F6C, and the next render, A or B, runs the pending update. That is exactly what the counter would reveal. Because the stub never changes behaviour, the site is safe either way.
CORRECTED: 

## SITE {
 "id": "TEL_APT_PENDING_B",
 "phase": "6",
 "group": "LW strategic UI (optional telemetry)",
 "address": "0x62213B",
 "length": 5,
 "original_hex": "8b 01 ff 50 28",
 "original_asm": [
  "0x62213b: 8b 01  mov eax, dword ptr [ecx]",
  "0x62213d: ff 50 28  call dword ptr [eax + 0x28]   ; AptPlayer::update 0x624EE1 (vtbl 0xBFD1F0/0xBDB780 slot +0x28)"
 ],
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:TEL_APT_PENDING_B>",
 "stub": "TEL_APT_PENDING_B (pure asm, behaviour identical in every mode, telemetry only). Entered by the call at 0x62213B with [esp]=0x622140, ECX=AptPlayer this (unchanged since 0x62212F), ESI=this+0x328.\n  cmp byte ptr [g_skipB],0 ; je .stock\n  push eax\n  mov eax,fs:[0x24]\n  cmp eax,[g_mainTid]\n  pop eax\n  jne .stock\n  lock inc dword ptr [g_cntAptPendB]   ; a 60-mode B-render ran the pending AptPlayer::update\n.stock:\n  mov eax,dword ptr [ecx]              ; displaced\n  jmp dword ptr [eax+0x28]             ; displaced call as a tail call; 0x624EE1 tail-jumps to 0x624C68, whose plain RET at 0x624EE0 returns to 0x622140 with stock ESP\n30 mode / A-render / other thread: stock. B-render: stock behaviour plus a counter. Expected count is 0 (see findings). Do NOT turn this into a B-skip: skipping would render a movie whose requested update has not run yet.",
 "resume_address": "0x622140",
 "live_after": "ESI = this+0x328 (0x622140 'mov byte [esi],0'; callee-saved, preserved by 0x624EE1/0x624C68). EBX/EDI/EBP callee-saved, untouched. EAX/ECX/EDX dead after the return: 0x622143 calls 0x81424C, which loads ECX=0xDE8A28 itself, and 0xAE0AB0/0x814278 take no register inputs. EFLAGS dead (0x622140 is a mov, then calls). x87 depth 0 and no XMM live (call boundary). At entry ECX must be this; the stub does not modify ECX.",
 "branch_into_span_check": "Interior 0x62213C..0x62213F. Brute rel8/rel32/jcc32 decode of every executable section plus a whole-image abs32 scan (<analysis workspace>/lw_tmp/ui/brute.py): 0 hits. listing.asm has no operand in 0x62213C..0x62213F. The only nearby branch is 'je 0x622143' at 0x622139, which targets the span end + 3, i.e. outside the span. The function 0x62212F is reached by 'call 0x62212F' at 0x462069 (APT render 0x46203E) and by vtable slot 0xBFD220.",
 "purpose": "Optional phase-6 telemetry. It confirms that the APT render's pending AptPlayer::update (AptPlayer+0x328) never runs on a B-render during LW-map play, so the APT update stays strictly A-only at stock cadence.",
 "evidence": "0x62212F: 'if byte[this+0x328] { this->vt28(); byte[this+0x328]=0 }' then 0x81424C, 0xAE0AB0 (APT runtime render 0xAE09F0) and 0x814278. The only writers that set +0x328=1 are 0x622911 in 0x6228DE and 0x622959 in 0x622923. 0x622923 has no code or pointer xref (raw E8/E9 scan plus abs32 scan), so it is dead. 0x6228DE's only caller is 0x92F6C3, in 0x92F6AA ShowCommandInterface, reached from ControlBar context code (0x71D8BE/0x943D6F/0x94475A/0x944EFB → 0x6D4671 → 0x930A1E → 0x93089B) and from AptPalantir::update 0x6D7647. 0x6228DE sets the flag and calls this->vt28 at once, unless an update is in progress (+0x312, set only around thunk 0xAE3150 inside 0x624EE1). The update clears +0x328 before its advance (0x624EE1). Bytes 56 8d b1 28 03 00 00 80 3e 00 74 08 8b 01 ff 50 28 c6 06 00 ... are identical in rotwk\\game.dat, aotr\\zGameDats\\delayfix.dat and rotwk\\game820.dat.",
 "risks": "None for behaviour, since every path runs the original call. drawFrame's load-screen path (0x449EDE) also renders APT, on the load-screen thread; the fs:[0x24]==g_mainTid check keeps that path out of the counter. No overlap with sites.json spans or AotR hooks: the nearest site is GATE_CU_APT at 0x632449 and the nearest AotR hook is 0x629D11.",
 "dll_vars": [
  {
   "name": "g_skipB",
   "ctype": "uint8_t",
   "note": "1 only during a 60-mode B-render clientUpdate (existing)"
  },
  {
   "name": "g_mainTid",
   "ctype": "uint32_t",
   "note": "existing"
  },
  {
   "name": "g_cntAptPendB",
   "ctype": "uint32_t",
   "note": "new telemetry counter; expected 0"
  }
 ]
}