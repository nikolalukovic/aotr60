# AREA lw-modes

## SUMMARY
Enabling 60 FPS on the Living World (LW) strategic map needs a new mode predicate and one new gate. The stepper, the start condition and the pacer need no change.

**Mode predicate.** The map must be identified by GL+0x110 == 8 together with the LW client and LW logic state, not by GL+0x125. After the MSG 0x6BB fade-in handler (battle return, map entry), GameLogic::reset clears GL+0x125 (0x6BF21B -> 0x625FC2 -> 0x62D232 -> 0x62D285) while the LW client stays active. The map can therefore run with GL+0x125 == 0 (tactical paths, already covered by the battle sites) or with GL+0x125 == 1 (LW paths). The exact stay predicate is in the findings. In summary it requires, behind AllowLivingWorldMap:
- mode 8;
- LW logic [0xDE4950] +0xB4 (active) and +0xB5 (running);
- LW client [0xDE4958] +0x18 (active), not +0x19 (suspended), and +0x14 not 2 or 3 (no fade);
- no pending battle-transition flags (+0x24, +0x25, LW logic +0x176, +0x177);
- an empty UI-sequence queue ([0xDE8900] == 0);
- GL+0x125 set only together with mode 8;
- every existing generic check.

StartBlockReason is unchanged.

**Cadence.** The stepper is the same: step(1) calls GameLogic::update for every sub (through the LogicUpdateWrapper slot). The LW path at 0x62E5AC processes the command list and sets GC+0xC8=1 at sub 1 (0x62E5D1). TheLivingWorldLogic::update (0x632A92) runs only inside the stock step of the Y iteration, at 5 Hz, with g_uiTick=1 and g_inClientUpdate=0. GC+0xC8 is set by the stepper at 0x6325FB and cleared only by pause (0x632AFC) or frozen time (0x62E5EF), so the switch-on condition s==1 && C8 is valid on the LW map. Per-tick invariants (12 renders / 6 A-renders / Δm_frame 6 / subs 1..6) hold.

**Pacer.** The limited path 0x63A196 is used on the LW map. [0xDE4320] = GD+0x26 when TV+0x23D4 <= 1, 0x603491 is false (pause/FF/debugger) and GD+0xBBD is 0; no LW-specific condition exists. GE+0xC is changed only at 0x779DCF and by the credits (0x91B6DC/0x91B7AA), and with no network the multiplier is 1.0, so T = 16.5 ms.

**What stays A-only.** Every LW per-render update is already A-only:
- LookAt; the LW translator 0x8392A7;
- the LW view 0x49AAB8 (it contains 0x6C0E4D):
  - armies 0x6C0BD2 and icons 0x6C038B;
  - the LW manager 0x6121C5: region effects 0x6112A9/0x6110BE/0x7EFB7C, the eye tower 0x7FB3F0 (logic RNG) and the Palantir 0x61226F;
  - camera fly 0x6BF78E;
- 0x645750; Display::update 0x65C1EA; the Shell; InGameUI; APT; particles (C7).

The B-render on the LW map runs only the draw (the LW draw's direct-call closure contains no logic RNG and no GE/GC/GL reads), propagate (empty), 0x532D6F, GC vt90, the snow manager and the UI-sequence runner 0x80000F.

**Proposed sites.**
- GATE_CU_UISEQ (0x6324A6): makes 0x80000F A-only, so sequence-driven clearGameData and LW activation can no longer run inside a B-render. In today's SP LW battles the exit reset already lands in a B-render (logic-safe, but against PLAN §1.8).
- TEL_LW_LOGIC_UPDATE (0xC1459C): brings the LW logic's logic-RNG use (LW AI and auto-resolve) into the telemetry window and adds a trace record per LW tick.

**Transitions.**

| Transition | Path | Reset hook 0x44181A |
|---|---|---|
| Shell -> LW map (new campaign) | MSG 0x1F 0x779D36 -> 0x6BE09E -> 0x6B5286 (setGameMode 8, startNewGame 0) | Not passed; 60 is already off in modes 4/9 and switch-on flushes the caches |
| LW map -> battle | Fade-out state 2 -> setActive(0) 0x49AC45 -> MSG 0x6BA -> 0x612274 -> clearGameData 0x6122A9 | Yes, in the logic step; the predicate leaves 60 when the fade starts |
| Battle -> LW map | MSG 0x7ED -> 0x62679C -> clearGameData 0x6267B8 / 0x626786, or the sequence 0x611D95 -> 0x610B59 | Yes (with the sequence, currently on a B-render); then 0x925699 -> 0x8004C9 setActive(1) + fade-in -> MSG 0x6BB handler, all in 30 mode (mode 9) |
| LW load | 0x6DEB9A always calls the reset (plus clearGameData for types 1/4/6); loadPostProcess 0x6BD0D5 runs the parchment-fade sequence | Yes |
| LW save | APT save menu, A-only | Not needed |
| Defeat / campaign end | Step 0x6111AB -> clearGameData 0x6111DF | Yes |
| Quit to menu | MSG 0x1D | Yes |
| End of turn | LW logic in step(1) | No transition |
| LW cinematics | moveTo is A-only; no LW code starts a blocking movie | n/a |

## COVERAGE
**Method.** Static analysis only of game.dat (identical to rotwk\game.dat; cmp verified) with Ghidra dump queries, capstone disassembly and raw-byte scans. Scratch scripts are in <analysis workspace>\lw_tmp: rd.py, vtcalls.py, ptrscan.py, relscan.py, closure.py, closure_refs.py, spanscan.py. No project or game files were created, modified or deleted.

**Not verified at runtime:**
- that GL+0x125 is really 0 on the LW map after a battle return or a new campaign (static trace through the 0x6BB handler's GameLogic::reset; the finding says how to log it);
- who enters LW client state 2 (vt4C(2)); it was not located statically (likely an APT callback or the LW logic). The predicate handles either case.

**Call-graph limits.** Logic-RNG reachability used the direct-call graph plus the known virtual edges (LW manager vt28, Palantir vt28). The virtual render-object calls inside WW3D::Render of the LW scene (0x518000) were not swept; that belongs to the LW draw / phase 4a audit.

**Out of scope here (other phase-6 areas):** LW camera smoothing (0x49AB21/0x49AB3E, 0x49B4A5, moveTo 0x6BF78E/0x6BFB88), per-draw integrators in LW render objects (army lines, banners, clouds), and the APT 'LivingWorldMap' embedded window, which is touched only from APT update and is already A-only.

**Proposed site bytes.** Both proposed sites have identical bytes in game.dat, delayfix.dat and game820.dat and overlap none of the 129 existing sites.

## FINDING [high] GL+0x125 is not a reliable 'LW map shown' flag: the LW map can run with GL+0x125=0
The 60-mode predicate cannot identify the strategic map by GL+0x125. GL+0x125 is written only when the LW client's active state changes, and GameLogic::reset clears it while the LW client stays active. The static trace shows that after the MSG 0x6BB handler, which runs after every state-3 fade-in (battle return), the strategic map runs with mode 8, the LW client active and GL+0x125 == 0. In that state GameLogic::update takes the full tactical path, GameClient::update runs the drawable block, W3DDisplay::update and Shell::update, and drawFrame runs updateViews, while the LW scene is still drawn at 0x449F45 because the drawFrame check reads only LW client +0x18/+0x19. GL+0x125 stays 1 after a LW save load (0x6DED03, plus setActive(1) at the end of the parchment-fade sequence) and during fade-ins. The reverse also exists: the shell's AptMapPreview activates the LW client suspended, which gives GL+0x125 = 0 with the client active. There is no bug today because the mode whitelist rejects mode 8.
EVIDENCE: Writers of GL+0x125 (listing scan of 'MOV byte ptr [reg+0x125]'; 0x6CE056/0x6CE63F belong to another class): 0x6301FF (ctor), 0x62D194 (GameLogic::init 0x62CE75, vt4), 0x62D285 (GameLogic::reset 0x62D232, vt24 of 0xBD8590), 0x6C0069 (setActive 0x6BFF7B: GL+0x125 = active && !suspended, written only inside 'if (param != +0x18)'), 0x6DED03 (loadGame, save types 4/6). Battle-return handler (MSG 0x6BB, LW handler 0x6BE6A6 reached from logicMessageDispatcher 0x779A86 for msgs 0x6A4..0x76B): 0x6BF1F0 LW client vt98 (+0x25=0), 0x6BF207 LWL+0xB5=1, 0x6BF21B call 0x625FC2(0) with ECX=GL -> 0x625FCA call [vt+0x24] = 0x62D232 -> 0x62D285 GL+0x125=0, then 0x6BF23E 0x62602B (mode 8) and 0x6BF248 GL+0x44=1. 0x6BB is sent by 0x6B4C78, polled every LW tick in 0x6B5BD3 from LW client +0x25, which vt88 sets at the end of the state-3 fade-in (0x49ABAC..0x49ABDC). State 3 is entered by 0x8004C9 (setActive(1) then vt4C(3,0)), pushed by 0x925699. Suspended activation: 0x975659 (AptMapPreview) vt50(1) then vt28(1). Script path: ScriptActions 0x7CAFA5 -> 0x7CE74F -> 0x7BD775 setActive(1).
REC: Identify the strategic map with mode 8 plus the LW client and LW logic state (next finding). Treat GL+0x125 only as 'must imply mode 8', and accept both GL+0x125 values in mode 8. GL+0x125=0 uses the battle paths, which the existing sites already cover. GL+0x125=1 uses the LW paths: GATE_GC_DISPUPD_LW 0x64884B, no updateViews, no drawable block. Confirm at runtime: log mode, GL+0x125 and LW client +0x14/+0x18/+0x19 at every mode-controller decision, once after a new campaign, after a battle return and after a LW load.

## FINDING [high] Exact Living World map predicate for BlockReason (stay and start)
Proposed replacement for the 'mode not in {0,2,6}' and 'GL+0x125' checks in src/frame_ctl.cpp BlockReason. All other generic checks stay: TheNetwork NULL, GL+0x114 not in {1,2}, GL+0x9D, replay, GD+0xBBD, GD+0xD45, intro, GD+0x26, script debugger, unknown path, fallback, LOD, display. StartBlockReason needs nothing new: m_frame >= 8, no GL+0x9C/0xA8 and TV+0x23D4 <= 1 all hold on the LW map, and the switch-on condition s==1 && GC+0xC8 is valid there (see summary). The predicate keeps every LW-client activation or deactivation, GL+0x125 flip and LW battle transition in 30 mode. The LW-client fade-out completion (state 2) calls setActive(0) inside an A-render's LW view update, before the draw, which would flip that pair from the LW to the tactical path. With the stay check, 60 mode ends at the first pair boundary after the fade starts.
EVIDENCE: Code:
uint32_t mode = Field<uint32_t>(gl,0x110); uint8_t* lwc = Ptr(0xDE4958); uint8_t* lwl = Ptr(0xDE4950);
bool lwActive = lwc && Field<uint8_t>(lwc,0x18);
if (mode == 8 || lwActive || Field<uint8_t>(gl,0x125)) {
  if (!g_cfg.allowLivingWorldMap) return "Living World strategic map";
  if (mode != 8) return "Living World map outside mode 8";   // map preview, LW-MP 1/5, script-shown map, type 4/6 load window
  if (!lwl || !lwc) return "Living World not ready";
  if (!Field<uint8_t>(lwl,0xB4) || !Field<uint8_t>(lwl,0xB5)) return "Living World logic not running";
  if (!lwActive || Field<uint8_t>(lwc,0x19)) return "Living World map not shown";
  int32_t st = Field<int32_t>(lwc,0x14); if (st == 2 || st == 3) return "Living World map fade";
  if (Field<uint8_t>(lwc,0x24) || Field<uint8_t>(lwc,0x25) || Field<uint8_t>(lwl,0x176) || Field<uint8_t>(lwl,0x177)) return "Living World battle transition";
  if (Read<uint32_t>(0xDE8900)) return "UI sequence running";
} else if (mode != 0 && mode != 2 && mode != 6) return "game mode";
Field meanings, traced: LWL+0xB4 active (0x6B9033 =1) and +0xB5 running (0 at the battle start 0x6BF133, 1 at 0x6BF207, default 1 at 0x6BA05A/0x6BA514); 0x441E4A tests both. LWC+0x14 is the state set only through vt4C -> 0x6BF3A5 (ctor 0x6C0D7B=1): state 2 fade-out 0x49ABE4..0x49AC4C ends with vt4C(1,1), setActive(0) at 0x49AC45 and vt84 (+0x24=1); state 3 fade-in 0x49ABAC.. ends with vt88 (+0x25=1). +0x24/+0x25 are polled by 0x6B5BD3, which sends MSG 0x6BA/0x6BB through 0x6B4BEC/0x6B4C78 and latches LWL+0x176/+0x177; the latches are cleared at 0x6BF118 and 0x6BF1F6. +0x19 is suspended (vt50 0x49BE23). The config key AllowLivingWorldMap is parsed (config.cpp:111) but not consulted anywhere yet.
REC: Implement as above behind AllowLivingWorldMap and keep the existing hysteresis (start only at s==1 && GC+0xC8). Add the LW fields to the 'mode:' log line. Optionally move the [0xDE8900]==0 check into the generic stay conditions: it would also end 60 mode before the SP LW battle-exit fade, which makes the GATE_CU_UISEQ site a pure safety net there.

## FINDING [medium] UI-sequence runner 0x80000F runs on B-renders; SP LW battle exit already resets inside a B-render
0x80000F (step queue [0xDE8900]) is on the B allow-list as 'event-driven and empty in game'. In practice the queue drives every LW transition: FadeScreenToBlack followed by clearGameData at SP LW battle exit, LW client activation with fade-in after the score screen, the parchment fade followed by setActive(1) after a LW load, and the LW defeat clearGameData. Window transitions advance only on A-renders (GATE_GC_WM) and their completion is seen by the same A-render's 0x80000F call, so the next step's first call (setActive or clearGameData) runs deterministically on the following B-render. In today's 60-mode SP LW battles, GameEngine::reset therefore runs inside a B-render's clientUpdate. clearGameData's teardown before the reset hook runs with g_skipB=1 and the B operand values active. After the hook, the rest of that B-render runs stock gates (for example an extra audio update). This is logic-safe because the stock step follows either way, but it contradicts PLAN §1.8 ('resets happen on A-renders') and would let GL+0x125 and LW-client flips land mid-pair on the LW map.
EVIDENCE: 0x80000F/0x7FFFAC (timeGetTime start per step, first-call flag); pop on bit2 at 0x800023..0x800031. Battle-exit chain: logicMessageDispatcher 0x77CFD3 (msg 0x7ED) -> 0x62679C -> 0x626662 (LWL+0xB4 set and LW client inactive, SP) -> 0x611D95 pushes 0x80046E and LAB 0x610B4F (clearGameData(1,0) at 0x610B59). Other LW steps: 0x8004C9 (pushed by 0x925699), 0x6B5D7D and 0x6B5D26 (pushed by 0x6B7702 from LW loadPostProcess 0x6BD0D5 at 0x6BD273), 0x6111AB (clearGameData at 0x6111DF). Transition progress: WindowManager::update 0x6C15FD -> [0xDE3654] at 0x6C1602 (A-only); 'finished' query 0x5DB742 -> 0x5DB4D7/0x5DB292. Direct-call closure of the runner and all LW step functions (1225 functions) reaches no logic-RNG function (0x6D328E/0x6D332C/0x6D34A0).
REC: Install GATE_CU_UISEQ (0x6324A6, A-only with the cached return on B; see sites). That keeps step cadence exactly stock and makes every sequence-driven reset or LW activation happen on an A-render or in a logic step. Also keep [0xDE8900]==0 in the LW stay predicate. Correct PLAN §1.8 and G1 CU-08 accordingly.

## FINDING [medium] Telemetry blind spot: TheLivingWorldLogic::update consumes the logic RNG outside LogicUpdateWrapper
On the LW map the actual campaign logic runs in TheLivingWorldLogic::update, called from step(1) after GameLogic::update(1) returns. LogicUpdateWrapper takes its seed snapshot before that call, so LW AI and auto-resolve RNG use is counted as 'seed changes outside logic' (false positives that hide real B-render consumption). The Telemetry=2 trace has no record per LW tick: GameLogic::getCRC does not cover LW state and GL+0x40 is reset by the 0x6BB handler's GameLogic::reset. PLAN phase 6's check ('identical logic-RNG traces per LW tick') cannot be done with the current telemetry. The per-tick invariants themselves still hold on the LW map: GameLogic::update(1..6) is still called through the vt34 slot every tick, and pause or freeze mark the window stalled.
EVIDENCE: 0x632A82 call [GL vt34] (slot 0xBD85C4 = LogicUpdateWrapper), then 0x632A8A..0x632A92 [0xDE4950]->vt28 = 0x6BE50E. RNG chain from body 0x6BE51A: 0x6BE20A -> 0x900AA7 -> 0x900533 -> 0x6D328E and 0x900AA7 -> 0x9000D4 -> 0x9A63BD -> 0x9A6004 -> 0x6D332C. Telemetry: LogicUpdateWrapper sets g_seedAtExit after 0x62E4E8 only (src/telemetry.cpp). The eye tower (client, A-only) also changes the seed between logic calls, in stock as well, so the seedOutside baseline on the LW map is not zero.
REC: Add TEL_LW_LOGIC_UPDATE (ptr_slot 0xC1459C -> LwLogicUpdateWrapper, see sites). Refresh g_seedAtExit after the LW update and write 'LW frame (+0x100), seed' per call at Telemetry=2. Compare seedOutside per tick on the LW map against a 30-mode baseline: about 6 eye-tower updates per tick while LWM+0x2CC is set.

## FINDING [low] The scene-render presentation window opens around the LW draw; it is inert there, and the LW camera stays at 30 Hz steps
On the LW path (GL+0x125=1) drawFrame skips updateViews at 0x449D29, so S1/S2 never run and SCENE_OPEN 0x449DAB opens the window without camera history. On every A-render it sets GE+0x3C to (2k-1)/12 and the C4 key, and increments g_cameraStats.aNoHistory (telemetry noise, about 30/s). The LW draw 0x49B618 is inside that window; its direct-call closure (346 functions) references neither TheGameEngine, TheGameClient, TheGameLogic nor the RNG seed, so the swapped fraction is not read. Presentation result: W3D animations on the LW map are half-stepped by C3 on A-renders, but the LW camera (stepped only in A-only 0x8392A7 and 0x49AAB8) is presented at M_k on both A and B. B shows exactly stock render k; A shows the camera at k with animations at k-1/2, so LW camera motion stays at 30 Hz steps.
EVIDENCE: drawFrame 0x449D24..0x449D2F (GL+0x125 skips vtA0 updateViews and 0x479FC4), SCENE_OPEN at 0x449DAB, LW draw at 0x449F43..0x449F45 (requires LWC+0x18 && !LWC+0x19), restore 0x44A271. camera.cpp SceneOpen_A: r1.renderId != g_renderId -> ++aNoHistory. 0x49B618 recomputes +0x13C/+0xD8 from +0x198 and statics, sets opacities from +0x134, then camera vt18, 0x49B4A5 (builds the camera transform; audio vt58 0x450A08 only sets +0x6AB=1) and 0x518000 WW3D render: all idempotent per draw.
REC: No safety change needed. Optionally skip OpenWindow in SceneOpen_A when GL+0x125 is set, to drop the telemetry noise. For smoothness, an LW-camera presentation window (interpolating the LW camera between M_{k-1} and M_k on A) belongs to the LW camera/draw audit (0x49AB21/0x49AB3E, moveTo 0x6BF78E/0x6BFB88, 0x49B4A5).

## FINDING [low] GATE_LWM evidence correction: 0x6C0E4D is also a base-class vtable slot
sites.json GATE_LWM states that 0x6C0E4D has 'no pointer references'. It is also slot +0x6C of the base LW client vtable 0xC14770 (dword at 0xC147DC). That vtable is live only inside the base ctor and dtor, and the W3D subclass overrides slot +0x6C with 0x49AAB8, so the gate still covers every runtime path.
EVIDENCE: Pointer scan: 0x6C0E4D appears at 0xC147DC. 0xC14770 is stored only at 0x6C0765 (base ctor) and 0x6C0D77 (base dtor). The W3D vtable 0xBDE918 has +0x6C = 0x49AAB8 and +0x28 = 0x49D565, while the base has +0x28 = 0x6BFF7B and +0x9C = 0x6BFB88. The only E8 caller of 0x6C0E4D is 0x49AAD4.
REC: Update the GATE_LWM evidence text; no code change.

## VERIFY SITE GATE_CU_UISEQ -> confirmed
My only writes were two scan scripts in <analysis workspace>\lw_tmp (uiseq_scan.py, uiseq_vt.py).

The site holds on every check. One evidence sentence has a wrong bit number, and two context lines are worth adding; the JSON below fixes both and changes no code.

1. **Bytes.** pefile reads e8 64 db 1c 00 at 0x6324A6 in a copy of game.dat, rotwk/game.dat, aotr/zGameDats/delayfix.dat and rotwk/game820.dat. 0x6324AB + 0x1CDB64 = 0x80000F. The instruction starts after 'call 0x7128C3' at 0x6324A1 and ends where 'mov ebx,eax' starts at 0x6324AB.

2. **No branch into the span.** I rescanned every section of the whole image for E8/E9, 0F8x, EB/7x/E0-E3 and abs32 dwords landing on 0x6324A7..0x6324AA: 0 hits. listing.asm has only the instruction itself. 0x6324A6 is the only rel32 caller of 0x80000F; the one abs dword equal to it (0xA3DD89) is unrelated. The span doesn't touch GATE_CU_RADAR (0x632486..0x63248F) or GATE_CU_KBD_DRAIN (0x6324BA). No other site in tools/sites.json and no AotR hook is in range. No existing gate covers it: PLAN 1.4 and G1 CU-08 deliberately left 0x80000F on the B allow-list.

3. **30-mode path is exact.** The stub does 'jmp [T_80000F]' with [esp]=0x6324AB and ESP unchanged. 0x80000F reads no ECX or stack args (it loads [0xDE8900], pushes/pops only EBX and ends with a plain ret at 0x800055). The RUNCNT/SKIPCNT macros are a single 'inc mem' and touch only EFLAGS, which are dead (0x6324AF 'and esi,1').

4. **A path is safe.** It is a nested call with one extra return address. 0x80000F→0x7FFFAC→0x7FFF64 uses no caller stack args. EDI, which the caller needs, is callee-saved and the stub never touches it. x86 SEH is chained through fs:[0], so an unregistered stub frame doesn't affect unwinding.

5. **B path never toggles.** The B-render's 0x6324BF compares ESI with [0xDE4330]. The A-render of the same pair wrote it at 0x6324FB, and GATE_CU_AUDIO re-emits that store on every path. So B returning the cached value means no toggle: the 0x69B5C7/0x5EDCF7 calls are skipped, and EBX (bit 2) is only read on the toggle path at 0x6324E1. If bit 0 is set, the drain at 0x6324B4 is already skipped on B by GATE_CU_KBD_DRAIN.

6. **The cache is always fresh.** g_uiSeqLast is written before any B-render. FrameState::BeginIteration turns 60 mode on only at a pair boundary with nextIsX=true, and the switch-off check also runs only before an X. The only way an A-render can skip 0x6324A6 is the early exit 0x632450→0x63239d→jmp 0x632521, and that needs TheNetwork [0xDE4468] non-null, which BlockReason refuses ("network game"). If the reset hook clears g_m60 mid-pair, the gate takes the stock tail path.

7. **The problem it fixes is real.** 0x611D95 (via 0x779A3D→0x62679C→0x626662) pushes FadeScreenToBlack 0x80046E, then clearGameData 0x610B4F. 0x80046E returns 1 on its first call and 3 (bit 1 = pop) once 0x5DB742 reports the transition done. 0x80000F runs only one step per call. Window transitions advance only on A (GATE_GC_WM), and GameClient::update (0x632490) runs before 0x6324A6. So the pop is seen on an A-render and clearGameData's first call lands on the next render, a B-render. 0x610B59 calls 0x7792BC, which reaches [0xDE4324]->vt24 = 0x44181A (GameEngine::reset / the RESET hook) inside that B-render. That breaks PLAN 1.8's "resets happen on A-renders". With the gate, steps advance once per stock render, the same cadence as stock.

8. **It runs on the LW map.** clientUpdate 0x632409 is called every render through C0 (0x6325CF; vt+0x9C slots 0xBD857C/0xBFE2FC), whatever the game mode. Among the queue pushers I checked are the LW ones 0x925699 (pushes 0x8004C9) and 0x6B7702.

**Text fixes** (no code change):
- Bit 1 (test bl,2), not bit 2, is the pop. After a pop that empties the queue, the result is reduced to bit 2 (0x80004C).
- The site also supersedes PLAN.md 1.4's allow-list entry "input lock 0x80000F", which should be updated with G1 CU-08.

**Optional, more robust alternative.** The B path could be stateless: 'mov eax, dword ptr [0xDE4330]'. Only 0x6324FB writes it, always with flags&1. That needs no g_uiSeqLast and can't go stale. Both variants are correct under the current BlockReason.
CORRECTED: {"id":"GATE_CU_UISEQ","phase":"6","group":"CU_ALLOWLIST (clientUpdate 0x632409 gates)","address":"0x6324A6","length":5,"original_hex":"e8 64 db 1c 00","original_asm":["0x6324a6: e8 64 db 1c 00  call 0x80000f  ; UI-sequence runner (queue head [0xDE8900]); returns flags&5 in EAX"],"kind":"call_gate","replacement_hex":"e8 <rel32:Gate_CU_UISEQ>","stub":"Gate_CU_UISEQ PROC            ; entered by the call at 0x6324A6, [esp]=0x6324AB, no stack args\n    cmp byte ptr [g_m60], 0\n    je tail                      ; 30 mode: exactly the original call (tail jump)\n    cmp byte ptr [g_uiTick], 0\n    je skip                      ; 60 B-render (call site is only reached inside clientUpdate on the main thread)\n    RUNCNT IDX_GATE_CU_UISEQ\n    call dword ptr [T_80000F]    ; 60 A-render: stock call (one extra return address on the stack; 0x80000F uses no stack args)\n    mov dword ptr [g_uiSeqLast], eax\n    ret\ntail:\n    RUNCNT IDX_GATE_CU_UISEQ\n    jmp dword ptr [T_80000F]\nskip:\n    SKIPCNT IDX_GATE_CU_UISEQ\n    mov eax, dword ptr [g_uiSeqLast] ; the flags the paired A-render got; bit0 then equals [0xDE4330] (stored by A at 0x6324FB, re-emitted by GATE_CU_AUDIO), so B toggles nothing\n    ret\nGate_CU_UISEQ ENDP\n(DEFTARGET 80000F). 30 mode: identical to 'call 0x80000F'. 60 A: stock call + cache. 60 B: no sequence step runs. Equivalent stateless B alternative: 'mov eax, dword ptr [0xDE4330]' (only writer 0x6324FB, value flags&1).","resume_address":"0x6324AB","live_after":"EAX = flags&5 (0x6324AB mov ebx,eax; 0x6324AD mov esi,ebx; and esi,1; bit0 compared with [0xDE4330] at 0x6324BF; bit2 tested at 0x6324E1 only on the toggle path). EDI = 0 from 0x632488 and is used at 0x6324C7/0x6324D1/0x63250A/0x632511, so it must be preserved (callee-saved; the stub never touches it); EBX/ESI are overwritten right after, EBP = frame. ECX/EDX dead (ECX reloaded at 0x6324B4/0x6324C9). EFLAGS dead (and esi,1 at 0x6324AF). x87 depth 0, no XMM live. 0x80000F ends with plain ret at 0x800055 and only push/pops EBX.","branch_into_span_check":"Interior 0x6324A7..0x6324AA: whole-image raw scan (E8/E9 rel32, 0F 80-8F rel32, EB/70-7F/E0-E3 rel8, abs32 dwords) = 0 hits; listing.asm has only the instruction itself at 0x006324a6. 0x6324A6 is the fall-through after 'call 0x7128C3' at 0x6324A1. 0x80000F has exactly one rel32 caller (0x6324A6); the only dword equal to 0x80000F (0xA3DD89) is unrelated data. No overlap with GATE_CU_RADAR (0x632486..0x63248F), GATE_CU_KBD_DRAIN (0x6324BA), GATE_CU_AUDIO (0x6324F9), any other site in tools/sites.json, or any AotR hook. Bytes identical in rotwk/game.dat, aotr/zGameDats/delayfix.dat and rotwk/game820.dat.","purpose":"In 60 mode the UI-sequence runner (the timeGetTime-based step queue that drives window-transition sequences) runs only on A-renders, and B-renders reuse the A-render's flags. Sequence steps then init, poll and complete once per stock render, exactly as in stock. The Living World transition steps can no longer run inside a B-render: LW client activation 0x8004C9/0x6B5D26 (setActive(1), which writes GL+0x125), clearGameData 0x610B4F/0x6111AB (GameEngine::reset), 'PreParchmentMapFade_LoadGame' 0x6B5D7D and 'FadeScreenToBlack' 0x80046E.","evidence":"0x80000F: head [0xDE8900]; 0x7FFFAC passes (now-start seconds, first-call flag) to the step functor via 0x7FFF64; bit1 (test bl,2 at 0x800023) of its result pops the step (0x800028..0x800031) without calling the next one; if the queue becomes empty the result is reduced to bit2 (0x80004C); returns ebx&5. Window-transition progress happens in WindowManager::update (A-only, GATE_GC_WM), and GameClient::update (0x632490) runs before 0x6324A6, so a step's completion is seen by an A-render's 0x80000F call and, without this gate, the next step's first call lands on the following B-render. Concrete case already reachable in 60 mode: SP LW battle exit 0x779A3D -> 0x62679C -> 0x626662 -> 0x611D95 pushes [0x80046E FadeScreenToBlack (returns 1 on first call, 3 when 0x5DB742 reports done), 0x610B4F clearGameData(1,0)]. 0x610B59 calls 0x7792BC, which calls [0xDE4324]->vt24 = 0x44181A (GameEngine::reset / RESET hook) inside a B-render's clientUpdate, contradicting PLAN 1.8. Other pushers of 0x8000AA: 0x611E04, 0x628F37, 0x62B385, 0x64849E, 0x6B7702, 0x91C108, 0x91C2FC, 0x925699.","dll_vars":[{"name":"g_m60","ctype":"volatile uint8_t","note":"60 mode active"},{"name":"g_uiTick","ctype":"volatile uint8_t","note":"0 only during a 60-mode Y iteration (B-render clientUpdate)"},{"name":"g_uiSeqLast","ctype":"uint32_t","note":"flags returned to the last 60-mode A-render; written before any B-render because 60 mode starts and stops only at pair boundaries with an X first, and the A-render early exit at 0x632450 needs TheNetwork, which BlockReason refuses"},{"name":"T_80000F","ctype":"const uint32_t","note":"0x0080000F"}],"risks":"Supersedes G1 CU-08 'run_on_B_as_is' and PLAN.md 1.4's B allow-list entry 'input lock 0x80000F' (update both). Steps see time sampled only at A-renders, i.e. stock 30 Hz polling. Keyboard drain at 0x6324BA stays gated by GATE_CU_KBD_DRAIN. If 60 mode is left mid-pair by the reset hook, g_m60=0 and the gate takes the tail path. If 60 mode is ever allowed with TheNetwork non-null, the A-render can skip 0x6324A6 via 0x63239d and g_uiSeqLast could be stale; the stateless B variant (return [0xDE4330]) avoids that."}

## VERIFY SITE TEL_LW_LOGIC_UPDATE -> needs_changes
The verification was read-only: I changed no files outside lw_tmp (I wrote only lw_tmp/v_tel.py).

What checks out:
- Read with pefile from a copy of game.dat: the slot 0xC1459C (file offset 0x81459C) holds 0e e5 6b 00, which is 0x6BE50E. The vtable 0xC14574 is stored only at 0x6BA001 and 0x6BA237 (both `mov dword ptr [esi],0xC14574`). The pointer 0x6BE50E appears only in this slot. A raw E8/E9 scan of the executable sections finds no direct call to 0x6BE50E or to 0x6BE51A.
- In listing.asm, the only `call [reg+0x28]` within 5 lines of a 0xDE4950 load (713 refs) is 0x632A92.
- Call site: 0x632A82 call [GL vt+0x34], then cmp edi,1 / jne 0x632A95, then 0x632A8A mov ecx,[0xDE4950] / mov eax,[ecx] / call [eax+0x28]. At 0x632A95, mov eax,[FPS30] overwrites EAX. No code runs between the GameLogic::update return and the LW call, so moving g_seedAtExit to after the LW call loses no client-side window.
- 0x6BE50E is thiscall with no stack args: an SEH prolog through 0xA3CEF0, then leave/ret at 0x6BE6A5. A __fastcall(ECX,EDX) wrapper with no stack args is ABI-identical, so the 30-mode path is exact.
- The logic RNG use is real: 0x6D328E and 0x6D332C both do `mov ecx,0xDA1CA4`. This means the current LogicUpdateWrapper counts LW-logic RNG use as seedOutside at the next call.
- The 60-mode X halted branch (0x6325DE) returns before step(1), so the LW update runs only on Y/stock steps.
- The site is not covered by any existing site: sites.json has no 0xC1459C, 0x6BE50E or 0x632A9x.
- The slot is in data, so there is no AotR hook overlap and no branch-into-span issue.
- This is a per-logic-tick telemetry site, not a per-render site. For telemetry that is correct; it has no B path to check.

What is wrong in the proposed stub:
1. The trace line breaks the existing trace parser. tools/compare_traces.py splits every line that does not start with '#' as `frame, sub, seed, crc, mode = line.split()`, which needs 5 fields. The proposed 'LW <frame> <seed> <30/60>' has 4 fields and raises ValueError. step(1) calls [0xDE4950]->vt28 unconditionally every stock tick, with no null check, in every mode including skirmish and campaign. So every Telemetry=2 trace would get these lines and the determinism check would crash on all traces, not only LW ones. Fix: prefix the line with '#' ('# LW ...'), which the parser already skips, and write it only when the LW frame actually advanced.
2. The LW-tick condition is inaccurate. `inc [esi+0x100]` at 0x6BE66E is reached only when +0xB4 != 0, the 0x90F92C(GL) check returns 0, +0xB5 != 0 and [[esi+0xB0]+8] != 0. Testing only +0xB4 && +0xB5 over-counts. Fix: read the dword at +0x100 before and after the call and count a tick when it changed.

Everything else in the site (address, length, original bytes, kind, ABI and live_after) is correct as proposed.
CORRECTED: {
 "id": "TEL_LW_LOGIC_UPDATE",
 "phase": "6",
 "group": "core",
 "address": "0xC1459C",
 "length": 4,
 "original_hex": "0e e5 6b 00",
 "original_asm": [
  "TheLivingWorldLogic vtable 0xC14574 slot +0x28 = 0x006BE50E (LivingWorldLogic::update: thiscall, no args, SEH prolog via 0xA3CEF0, leave/ret at 0x6BE6A5)"
 ],
 "kind": "ptr_slot",
 "replacement_hex": "<abs32:LwLogicUpdateWrapper>",
 "stub": "extern \"C\" void __fastcall LwLogicUpdateWrapper(uint8_t* lwl, void*): f0 = *(uint32_t*)(lwl+0x100); call 0x6BE50E as thiscall(lwl); then g_seedAtExit = [0xDA1CA4] (g_seedValid unchanged/true), so LW-logic RNG use (0x6BE20A -> 0x900AA7 -> 0x6D328E / 0x6D332C on 0xDA1CA4) counts as inside logic and not in seedOutside. If *(uint32_t*)(lwl+0x100) != f0 (the LW frame advanced: the inc at 0x6BE66E runs only when +0xB4, !0x90F92C(GL), +0xB5 and [[+0xB0]+8]!=0): ++g_t.lwTicks (and ++g_t.lwTicks60 when g_m60), and when g_trace is open write one COMMENT line '# LW <lwl+0x100> <seed %08X> <30|60>\\r\\n'. The leading '#' is required: tools/compare_traces.py skips '#' lines and otherwise unpacks exactly 5 fields, and this slot is called every stock tick in every mode (skirmish/campaign too; the body early-outs on +0xB4==0). Behaviour is identical in 30 and 60 mode (telemetry only).",
 "resume_address": "- (returns to 0x632A95 in GameEngine step(1) 0x6329B0)",
 "live_after": "thiscall with ECX = TheLivingWorldLogic, no stack args, plain ret, so __fastcall(ECX, EDX) with no stack args is ABI-identical. At 0x632A95: EAX dead (mov eax,[0xD9F60C]); EDI = sub (1) and EBP = engine are callee-saved; EBX/ESI callee-saved; EFLAGS dead; x87 empty; no XMM live.",
 "branch_into_span_check": "n/a (data slot in .rdata, file offset 0x81459C). Checks done: 0x6BE50E is referenced only by this slot (pointer scan). A raw E8/E9 scan of the executable sections finds no direct call to 0x6BE50E or 0x6BE51A. The vtable 0xC14574 is stored only at 0x6BA001/0x6BA237 (ctors). The only vt+0x28 call on [0xDE4950] is 0x632A92 (listing window scan over all 713 refs). The 60-mode halted branch 0x6325DE returns before step(1), so the slot is reached only on stock/Y steps.",
 "purpose": "Extends the per-logic-call telemetry and logic-RNG window to TheLivingWorldLogic::update. It runs right after GameLogic::update(1) returns (0x632A8A..0x632A92, sub==1 only), outside LogicUpdateWrapper, and on the LW map it consumes the logic RNG.",
 "evidence": "step(1) 0x6329B0: 0x632A82 call [GL vt34] (LogicUpdateWrapper via 0xBD85C4); 0x632A85 cmp edi,1 / jne 0x632A95; 0x632A8A..0x632A92 [0xDE4950]->vt28 with no null check. Body: early exit if [esi+0xB4]==0 (0x6BE525), if 0x90F92C(GL) is true (0x6BE547), if [esi+0xB5]==0 or [[esi+0xB0]+8]==0; LW frame inc [esi+0x100] at 0x6BE66E. LW logic RNG: 0x6BE63B call 0x6BE20A -> 0x900AA7 -> 0x900533 -> 0x6D328E (mov ecx,0xDA1CA4 at 0x6D329D), and 0x900AA7 -> 0x9000D4 -> 0x9A63BD -> 0x9A6004 -> 0x6D332C (mov ecx,0xDA1CA4 at 0x6D3347). LogicUpdateWrapper (src/telemetry.cpp) samples g_seedAtExit at GameLogic::update exit, so LW-logic RNG use is currently counted in seedOutside at the next call.",
 "dll_vars": [
  {
   "name": "g_seedAtExit / g_seedValid",
   "ctype": "uint32_t / bool",
   "note": "shared with LogicUpdateWrapper"
  },
  {
   "name": "g_trace",
   "ctype": "HANDLE",
   "note": "Telemetry=2 trace file; LW lines must start with '#' (compare_traces.py format)"
  },
  {
   "name": "g_t.lwTicks / g_t.lwTicks60",
   "ctype": "uint64_t",
   "note": "new counters, incremented when lwl+0x100 changed across the call"
  }
 ],
 "risks": "Telemetry only. Must not touch x87/MXCSR. Must not call game code other than 0x6BE50E. The slot is hit every stock tick in all modes, so the trace output must stay in comment form ('#') to keep tools/compare_traces.py working."
}

## VERIFY FINDING GL+0x125 is not a reliable 'LW map shown' flag: the LW map can run with GL+0x125 -> confirmed
I checked every load-bearing claim against game.dat. Each one holds, with two small corrections.

Writers of GL+0x125: a listing scan finds 0x62D194 (init), 0x62D285 (reset, which writes BL=0 after `xor ebx,ebx` at 0x62D24B), 0x6301FF (ctor), 0x6C0069 and 0x6DED03 (=1). The 0x6CE0xx and 0x8B0xxx/0x8CAxxx/0x8E1xxx hits belong to other classes.

setActive 0x6BFF7B:
- `cmp bl,[esi+0x18]; je 0x6C00D8` at 0x6BFF94 skips the GL+0x125 write when the active state does not change.
- When it does change, it writes `GL+0x125 = +0x18 && !+0x19` (0x6C0051..0x6C0069).
- The LW client vtable is at 0xBDE918. vt28 is 0x49D565, the wrapper that calls 0x6BFF7B. vt50 is 0x49BE23 and writes +0x19 at 0x49BE63, so it is setSuspended. vt20 is 0x49B618, vt6C is 0x49AAB8, vt88 is 0x49D0EE (+0x25=1) and vt98 is 0x49D100 (+0x25=0).

Battle return:
- 0x8004C9 calls setActive(1) at 0x800520, then state 3.
- vt6C handles state 3 (+0x14==3 at 0x49AAE5) with the fade at 0x49ABAC; when the fade ends it calls vt88 at 0x49ABDC.
- 0x6B5BD3 polls vt90 (+0x25) and calls 0x6B4C78, which sends 0x6BB. In SP this always happens because there are fewer than 2 humans.
- The case 0x6BB handler reaches 0x6BF1E8: vt98, LWL+0xB5=1, then 0x625FC2 with ECX=GL. That calls `[vt+0x24]` at 0x625FCA, which goes to GameLogic::reset and GL+0x125=0. After that come 0x62602B and GL+0x44=1.
- Nothing writes GL+0x125=1 again while the client stays active. Every later vt28(1) returns early. The setActive(0) calls at 0x6BD0BD/0x6BD284 are skipped because 0x441E4A (LWL+0xB4 && +0xB5) is now true.

AptMapPreview 0x975659 calls vt50(1) and then vt28(1), which gives a suspended active client with GL+0x125=0. Confirmed.

GameLogic::update, GameClient::update and drawFrame read GL+0x125 at these sites: 0x62E5AC, 0x648734, 0x648821, 0x64887C and 0x449D29. The LW scene draw at 0x449F45 checks only LW client +0x18/+0x19. Confirmed.

"No bug today" also holds. The fade-in runs with GL+0x125=1, which blocks 60 mode. The 0x6BB handler then sets GL+0x125=0 and the mode in the same logic tick, so the block reason switches from 0x125 to the mode check and 60 mode is never allowed.

Corrections:
1. 0x62602B sets mode 8 only when GL+0x114==0; 0x114 values 1 and 2 give mode 1 and mode 5. This does not matter in single player, because 0x114 is 0 there.
2. The recommendation says "GL+0x125=0 uses the battle paths, which the existing sites already cover". That is incomplete. With GL+0x125=0 in mode 8 the frame is a hybrid. The tactical paths run: the full GameLogic::update, the drawable block, the W3DDisplay::update vt28 at 0x648835/0x648840, and updateViews of a world that has just been reset. The LW paths keep running at the same time: LW client vt6C at 0x6484D9 and 0x645750 are called without any condition whenever TheLivingWorldClient exists (0x6484CD..0x6484E2), and the LW draw at 0x449F45 still runs. So the tactical sites and all LW sites both apply to this state.

CORRECTED REC: Do not use GL+0x125 alone to identify the strategic map. Define the strategic map as: mode 8, GL+0x114==0, TheLivingWorldClient present with +0x18==1 and +0x19==0, and TheLivingWorldLogic active (LWL+0xB4). Accept both values of GL+0x125 in that state.

In mode 8 the two GL+0x125 values need different handling:
- **GL+0x125==1 (pure LW path):** video update 0x65C1EA at 0x64884B (GATE_GC_DISPUPD_LW), no updateViews, no drawable block, and the LW logic dispatch from GameLogic::update.
- **GL+0x125==0 (hybrid path):** the frame does not just use the battle paths. Every tactical render site applies: updateViews at 0x449D29, the drawable block, W3DDisplay/terrain update via vt28 at 0x648835/0x648840, and the S2/SCENE_OPEN presentation windows on a tactical view whose world has just been reset. Every LW site applies at the same time, because LW client vt6C at 0x6484D9, 0x645750, the LW camera at 0x8392A7 and the LW draw vt20 at 0x449F45 run whenever the LW client exists and is not suspended. Verify both site sets in this state. Check in particular that the A-render tactical presentation windows do nothing harmful on the empty tactical view.

GL+0x125 in other modes:
- Keep refusing 60 mode when GL+0x125==1 and mode is not 8. This covers the state-3 fade-in after a battle, which still runs in the battle mode while setActive(1) has already set GL+0x125=1.
- Keep refusing when the LW client is active but suspended (+0x19, the AptMapPreview shell case).

Confirm at runtime: log mode, GL+0x114, GL+0x125, LW client +0x14/+0x18/+0x19/+0x25 and LWL+0xB4/+0xB5 at every mode-controller decision. Do this after a new campaign, after a battle return (before and after the 0x6BB handler) and after a LW save load (0x6DED03).

## VERIFY FINDING Exact Living World map predicate for BlockReason (stay and start) -> uncertain
I did not verify the finding, so the Living World map predicate for BlockReason is still unchecked.

## VERIFY FINDING UI-sequence runner 0x80000F runs on B-renders; SP LW battle exit already resets  -> confirmed
What I checked in the binary:
- **Runner 0x80000F.** It calls 0x7FFFAC, which reads timeGetTime, sets a first-call flag at +0xC and calls the step functor through 0x7FFF64 with (elapsed, first). When the result has bit 2 (`test bl,2` at 0x800023) it pops the head at 0x800028..0x800031, deletes it and returns flags&5. The next step's first call happens on the next 0x80000F call. In clientUpdate the runner is called at 0x6324A6, after GameClient::update (0x632498) and propagate (0x6324A1). sites.json classifies it as "run" ("time-based and event-driven"), and PLAN 1.4 puts it on the B allow-list. There is no GATE_CU_UISEQ in sites.json, so the recommendation's "see sites" is wrong.
- **Battle-exit chain.** 0x62679C/0x779A3D call 0x626662, which tests 0x610B62 (LWL[0xDE4950]+0xB4) and LW client [0xDE4958]+0x18==0. It then checks 0x610A21 (GL+0x114 not in {1,2}, i.e. SP) and calls 0x611D95. 0x611D95 pushes 0x80046E and 0x610B4F through 0x8000AA into [0xDE8900]. On its first call, 0x80046E starts the FadeScreenToBlack group (0x5DB9A6 with immediate=0, plus 0x5DB31E setting +0x69=1) and returns 1. On later calls it returns 3 once 0x5DB742 reports finished. 0x610B4F does `mov ecx,[0xDE412C]; push 0; push 1; call 0x7792BC`, which is GameLogic::clearGameData. clearGameData calls [0xDE4324]->vt24, and Win32GameEngine vtbl 0xBD84E0+0x24 = 0x44181A, the reset hook. So GameEngine::reset runs inside that call.
- **Transition progress.** "Finished" (0x5DB4D7/0x5DB292: the per-window transition's +0x10->+8 flag, or +0x24==0) only changes in the TransitionHandler update, vtbl 0xBF1DA8 slot 0x28 = 0x5DB60A. That update runs through 0x5DB4B1 and is reached only from WindowManager::update 0x6C15FD (`jmp [eax+0x28]` at 0x6C160E). That path is gated A-only by GATE_GC_WM (0x64863A). On B, drawFrame (0x44A210) calls only vt30 = 0x5DB725 (draw, 0x5DB2CF -> transition vt10), which does not advance it. 0x5DB9A6 with no active transition stores straight into +0x24, so the finished query checks the new fade.
- **Order in a pair.** The fade finishes in A-render k's WM update, and the same A-render's 0x6324A6 sees finished and pops. Then comes the halted step. Then the B-render's 0x6324A6 makes the first call to 0x610B4F, so clearGameData and reset run inside B. Nothing in BlockReason/StartBlockReason turns 60 mode off during this fade: GL+0x110 stays 0, and 0x114, 0x9D and 0x125 are all 0.
- **Inside that B-render.** OnPreRender set g_skipB=1 and the B integrator values (SetIntegratorVariablesForB(true)), so clearGameData's teardown up to the vt24 call runs with them. OnEngineReset then calls LeaveSixty (g_m60=0, g_uiTick=1, g_skipB=0). The rest of clientUpdate therefore runs stock gates, for example audio [0xDE42FC] vt28 at 0x632501, a second time in this pair.
- **Other sequences.** The other pushers check out the same way: 0x6B7702 (called by 0x6BD0D5) pushes 0x6B5D7D and 0x6B5D26, 0x925699 pushes 0x8004C9, and 0x6111AB calls clearGameData at 0x6111DF.

Correction to the finding: it says this is "logic-safe because the stock step follows either way", and that is too weak. In stock, render k pops the fade, then step k runs, then render k+1 calls clearGameData. In 60 mode, clearGameData and reset run in B_k, before stock step k. Step k then runs on cleared state (GL+0x110=9) instead of the battle. That is one stock step reordered against the clear, so today's behaviour is not exactly stock at LW battle exit. It is most likely harmless because the results were already recorded at 0x626662, but it breaks the exactness claim.

The proposed fix works. 0x6324A6 is a 5-byte `call 0x80000F` that starts an instruction. A brute rel8/rel32/jcc32 scan finds no branch into 0x6324A7..0x6324AA, and no AotR hook is nearby (nearest 0x629D11). On B, a cached return is safe: bit 0 then equals [0xDE4330], which A stored, so no InGameUI or Mouse toggle fires, bit 4 is only tested on a toggle, and the keyboard drain is already gated. Making the runner A-only restores the stock cadence (one call per stock render). Its steps get real elapsed time from timeGetTime, so time-based steps are unaffected.
CORRECTED REC: Add a new site GATE_CU_UISEQ: a call_gate at 0x6324A6 (bytes `e8 <rel32:0x80000F>`, 5 bytes, resume 0x6324AB). It must be added to sites.json, because no such site exists yet.
- **30-mode path:** byte-exact, and the call always runs when g_m60==0 or g_uiTick!=0.
- **B-render path:** do not call 0x80000F. Return EAX = the last A-render's return value cached by the stub. A minimal alternative is EAX = [0xDE4330] (bit 0 only), which also gives no toggle, since the drain gate already handles bit 0.
- **live_after:** EAX holds the result; ECX, EDX and EFLAGS are dead (0x6324AB `mov ebx,eax`, flags rewritten by `and esi,1`); x87 depth 0, no XMM.

With this gate, every sequence step's first call runs on an A-render, after the stock step that follows the A-render where the previous step finished. The fade -> clearGameData(1,0) -> GameEngine::reset at SP LW battle exit, the LW defeat clearGameData (0x6111DF) and the LW post-load and score-screen activations (0x6B5D7D/0x6B5D26, 0x8004C9) then happen in the same logic-step order as stock. They also stop running with g_skipB=1 or B integrator values, and no GL+0x125 or LW-client flip can land between A and B.

Fix the documents. PLAN 1.4 should move 0x80000F to the A-only list. PLAN 1.8 currently says resets happen on A-renders; it should explain that this is true only once 0x80000F is gated too. G1 CU-08 should be reclassified from run_on_B_as_is to A-only, because the queue drives the LW battle exit, the LW load fade-in and the defeat transitions and is not empty in game.

Also correct the finding: today's behaviour is not merely cosmetic. It moves clearGameData ahead of stock step k, so it breaks exactness at LW battle exit.

For phase 6, also require [0xDE8900]==0 in the LW stay/start predicate as defence in depth.

## VERIFY FINDING Telemetry blind spot: TheLivingWorldLogic::update consumes the logic RNG outside -> confirmed
What the code confirms:
- Call order in step (FUN_00632994). 0x632A82 calls [GameLogic vt+0x34]. Then 0x632A85 `cmp edi,1` / `jne 0x632A95`, and 0x632A8A..0x632A92 calls [[0xDE4950]+0x28]. So the LW update runs only at sub 1, after GameLogic::update returns.
- In the binary, the vtable at 0xC14574 has slot +0x28 at 0xC1459C, which holds 0x6BE50E. Slot 0xBD85C4 holds 0x62E4E8, and sites.gen.h already patches that slot as TEL_LOGIC_UPDATE -> LogicUpdateWrapper.
- 0x6BE50E is a thiscall with no arguments (it ends `leave; ret` at 0x6BE6A5). Its body calls 0x6BE20A at 0x6BE63B when +0xB4 and +0xB5 are set.
- The static call chain checks out: 0x6BE20A -> 0x900AA7 -> 0x900533 -> 0x6D328E, and 0x900AA7 -> 0x9000D4 -> 0x9A63BD -> 0x9A6004 -> 0x6D332C. Both RNG functions load ECX=0xDA1CA4, which is kLogicRngSeed, the logic RNG.
- In src/telemetry.cpp (lines 387-419), LogicUpdateWrapper sets g_seedAtExit right after calling 0x62E4E8. Any logic-RNG use by the LW update therefore shows up as seedOutside (and seedOutside60) at the next wrapper entry. This is a real false-positive source that hides genuine B-render consumption. The 'seed changes outside logic' counter is not usable on the LW map as it stands.
- The finding's statement that GameLogic::update(1..6) still runs through the vt34 slot each tick on the LW map is right. 0x62E5AC..0x62E5E5 shows the GL+0x125 path calling 0x5FF9D3 every sub and 0x625804 plus GC+0xC8=1 at sub 1.

What is overstated or unverified:
- 'The Telemetry=2 trace has no record per LW tick' is wrong as stated. TraceLogicCall writes one line per GameLogic::update call (frame GL+0x40, sub, seed, CRC at sub 1), and the wrapper still runs on the LW map. On the LW map, subs 2..6 only run 0x5FF9D3. So the sub-2 line's seed already includes the LW update's RNG use, mixed with any render-side use (eye tower, or B-render leaks) from the frames in between.
- Likewise, 'the check cannot be done' is too strong. A per-tick seed comparison can be done with the existing trace. What it lacks is attribution (LW logic versus render) and the LW frame counter (+0x100).
- GL+0x40 only increments when 0x625130 is true (GL+0x44 set, +0xA8 and +0x9D clear) and the game is not paused (0x62E568..0x62E577). I did not verify whether it advances on the LW map, or the claim that the '0x6BB handler' resets it.
- I did not verify the claim that getCRC (0x625886) leaves out LW state.
- I did not trace the claimed eye-tower baseline of about 6 per tick. It matches one A-only render per sub, but it is unconfirmed.

Proposed site: 0xC1459C does not appear in tools/sites.json, is not one of the AotR hooks, and is a data slot. A ptr_slot there is valid and leaves 30 mode byte-exact if the wrapper is a no-argument thiscall (__fastcall(ecx, edx) with no stack arguments) that tail-forwards to 0x6BE50E.
CORRECTED REC: This is a real telemetry gap, but low priority. It is not a correctness issue for 60 mode.

1. Add TEL_LW_LOGIC_UPDATE as a ptr_slot. It replaces slot 0xC1459C (original bytes 0E E5 6B 00 = 0x6BE50E) with <abs32:LwLogicUpdateWrapper>, in phase Telemetry, like TEL_LOGIC_UPDATE. Declare the wrapper `void __fastcall LwLogicUpdateWrapper(uint8_t* lw, void*)` with no stack arguments. It calls 0x6BE50E as a thiscall, then refreshes g_seedAtExit from 0xDA1CA4 (keeping g_seedValid set). At Telemetry=2 it writes one trace line tagged LW, carrying [lw+0x100], GL+0x40 and the seed. That way seedOutside on the LW map counts only client and render consumption.
2. Correct the finding's wording. The existing trace already has per-sub lines, and on the LW map the sub-2 seed already includes the LW update's RNG use. So per-tick seed comparison is possible today. It just cannot tell LW logic consumption apart from render consumption.
3. Before using seedOutside60 as a B-leak detector on the LW map, measure a 30-mode baseline with the new wrapper. Expect eye-tower changes at roughly one per render while it is active; this is not yet traced. The pass criterion is that the 60-mode per-tick count equals the 30-mode count, not zero.
4. Do not rely on GL+0x40 or getCRC as the LW tick key until it is checked that +0x40 advances on the LW map (0x625130 requires GL+0x44) and whether the CRC covers LW state. Use LW+0x100 as the key instead.

## SITE {
 "id": "GATE_CU_UISEQ",
 "phase": "6",
 "group": "CU_ALLOWLIST (clientUpdate 0x632409 gates)",
 "address": "0x6324A6",
 "length": 5,
 "original_hex": "e8 64 db 1c 00",
 "original_asm": [
  "0x6324a6: e8 64 db 1c 00  call 0x80000f  ; UI-sequence runner (queue head [0xDE8900]); returns (step flags)&5 in EAX"
 ],
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:Gate_CU_UISEQ>",
 "stub": "Gate_CU_UISEQ PROC            ; entered by the call at 0x6324A6, [esp]=0x6324AB, no stack args\n    cmp byte ptr [g_m60], 0\n    je tail                      ; 30 mode: exactly the original call (tail jump)\n    cmp byte ptr [g_uiTick], 0\n    je skip                      ; 60 B-render (call site is only reached inside clientUpdate on the main thread)\n    RUNCNT IDX_GATE_CU_UISEQ\n    call dword ptr [T_80000F]    ; 60 A-render: stock call (one extra return address on the stack; 0x80000F uses no stack args)\n    mov dword ptr [g_uiSeqLast], eax\n    ret\ntail:\n    RUNCNT IDX_GATE_CU_UISEQ\n    jmp dword ptr [T_80000F]\nskip:\n    SKIPCNT IDX_GATE_CU_UISEQ\n    mov eax, dword ptr [g_uiSeqLast] ; the flags the paired A-render got; bit0 then equals [0xDE4330] (stored by A at 0x6324FB), so B toggles nothing\n    ret\nGate_CU_UISEQ ENDP\n(DEFTARGET 80000F). 30 mode: identical to 'call 0x80000F'. 60 A: stock call + cache. 60 B: no sequence step runs.",
 "resume_address": "0x6324AB",
 "live_after": "EAX = flags&5 (0x6324AB mov ebx,eax; 0x6324AD mov esi,ebx; and esi,1; bit0 compared with [0xDE4330] at 0x6324BF; bit2 tested at 0x6324E1). EDI = 0 from 0x632488 and is used at 0x6324C7/0x6324D1/0x63250A/0x632511, so it must be preserved (callee-saved); EBX/ESI are overwritten right after, EBP = frame. ECX/EDX dead (ECX reloaded at 0x6324B4/0x6324C9). EFLAGS dead (and esi,1 at 0x6324AF). x87 depth 0, no XMM live. 0x80000F ends with plain ret at 0x800055 and only push/pops EBX.",
 "branch_into_span_check": "Interior 0x6324A7..0x6324AA: whole-image raw scan (E8/E9 rel32, 0F 80-8F rel32, EB/70-7F/E0-E3 rel8, abs32 dwords) = 0 hits; rg listing.asm for 0x006324a[6-9a] finds only the instruction itself. 0x6324A6 is the fall-through after 'call 0x7128C3' at 0x6324A1. Raw E8/E9 scan: 0x80000F has exactly one caller (0x6324A6); the only dword equal to 0x80000F (0xA3DD89) is mid-instruction data in the CRT area. No overlap with GATE_CU_RADAR (0x632486..0x63248F), GATE_CU_KBD_DRAIN (0x6324BA), GATE_CU_AUDIO (0x6324F9) or any of the 129 sites in tools/sites.json, and not an AotR hook. Bytes are identical in rotwk/game.dat, aotr/zGameDats/delayfix.dat and rotwk/game820.dat.",
 "purpose": "In 60 mode the UI-sequence runner (the timeGetTime-based step queue that drives window-transition sequences) runs only on A-renders, and B-renders reuse the A-render's flags. Sequence steps then init, poll and complete once per stock render, exactly as in stock. The Living World transition steps can no longer run inside a B-render: LW client activation 0x8004C9/0x6B5D26 (setActive(1), which writes GL+0x125), clearGameData 0x610B4F/0x6111AB (GameEngine::reset), 'PreParchmentMapFade_LoadGame' 0x6B5D7D and 'FadeScreenToBlack' 0x80046E.",
 "evidence": "0x80000F: head [0xDE8900]; 0x7FFFAC passes (now-start seconds, first-call flag) to the step functor; bit2 of its result pops the step (0x800023..0x800031); returns ebx&5. Window-transition progress happens in WindowManager::update 0x6C15FD (via [0xDE3654] at 0x6C1602), which is A-only (GATE_GC_WM). The step's completion is therefore seen by the same A-render's 0x80000F call (0x648642 runs before 0x6324A6), and the next step's first call lands on the following B-render. Concrete case that already happens in 60 mode: in SP LW battle exit, MSG 0x7ED (0x77CFE1) -> 0x62679C -> 0x626662 -> 0x611D95 pushes [0x80046E FadeScreenToBlack, 0x610B4F clearGameData(1,0)]. The call 0x7792BC at 0x610B59 then reaches GameEngine::reset (vt24 = 0x44181A) inside a B-render's clientUpdate, while g_skipB and the B operand values are still set up to the reset hook. Other pushers: 0x6B7702 (from LW loadPostProcess 0x6BD0D5 at 0x6BD273), 0x925699 (score/LW screen, pushes 0x8004C9), 0x628F37 (startNewGame for modes 1/2/5), 0x91C108/0x91C2FC (load UI), and the one-shot startup functors.",
 "dll_vars": [
  {
   "name": "g_m60",
   "ctype": "volatile uint8_t",
   "note": "60 mode active"
  },
  {
   "name": "g_uiTick",
   "ctype": "volatile uint8_t",
   "note": "0 only during a 60-mode B-render clientUpdate"
  },
  {
   "name": "g_uiSeqLast",
   "ctype": "uint32_t",
   "note": "flags returned to the last 60-mode A-render; written before any B-render because 60 mode always starts with an X iteration"
  },
  {
   "name": "T_80000F",
   "ctype": "const uint32_t",
   "note": "0x0080000F"
  }
 ],
 "risks": "Supersedes the G1 CU-08 classification 'run_on_B_as_is'. Steps see time sampled only at A-renders: completion can be up to one B interval later than when it is first observable, which is exactly stock (stock polls once per 33 ms). Keyboard drain at 0x6324BA stays gated by GATE_CU_KBD_DRAIN. If 60 mode is left mid-pair by the reset hook, g_m60=0 and the gate takes the tail path."
}

## SITE {
 "id": "TEL_LW_LOGIC_UPDATE",
 "phase": "6",
 "group": "core",
 "address": "0xC1459C",
 "length": 4,
 "original_hex": "0e e5 6b 00",
 "original_asm": [
  "TheLivingWorldLogic vtable 0xC14574 slot +0x28 = 0x006BE50E (LivingWorldLogic::update: thiscall, no args, SEH frame, plain ret at 0x6BE6A5)"
 ],
 "kind": "ptr_slot",
 "replacement_hex": "<abs32:LwLogicUpdateWrapper>",
 "stub": "extern \"C\" void __fastcall LwLogicUpdateWrapper(uint8_t* lwl, void*): calls 0x6BE50E as thiscall(lwl). Then: g_seedAtExit = [0xDA1CA4] (g_seedValid stays true), so the LW AI and auto-resolve logic-RNG use counts as inside logic and not in seedOutside. ++LW-tick counters when lwl+0xB4 && lwl+0xB5. At Telemetry=2, one trace line 'LW <lwl+0x100 LW frame> <seed> <30/60>' after each call, so phase 6 can compare logic-RNG traces per LW tick. Behaviour is identical in 30 and 60 mode (telemetry only).",
 "resume_address": "- (returns to 0x632A95 in GameEngine step(1) 0x6329B0)",
 "live_after": "thiscall with ECX = TheLivingWorldLogic, no stack args, plain ret, so __fastcall(ECX, EDX) with no stack args is ABI-identical. At 0x632A95: EAX dead (mov eax,[0xD9F60C]); EDI = sub (1) and EBP = engine are callee-saved; EBX/ESI callee-saved; EFLAGS dead; x87 empty; no XMM live.",
 "branch_into_span_check": "n/a (data slot in .rdata, file offset 0x81459C). Checks done: 0x6BE50E is referenced only by this slot (pointer scan). A raw E8/E9 scan finds no direct call to 0x6BE50E or 0x6BE51A. The vtable 0xC14574 is stored only at 0x6BA001/0x6BA237. The only vt+0x28 call on [0xDE4950] is 0x632A92 (instruction-window scan of all refs to 0xDE4950). Bytes are identical in all three builds.",
 "purpose": "Extends the per-logic-call telemetry and logic-RNG window to TheLivingWorldLogic::update. On the LW map it runs right after GameLogic::update(1) returns (0x632A8A..0x632A92), outside LogicUpdateWrapper, and it consumes the logic RNG.",
 "evidence": "step(1) 0x6329B0: 0x632A82 call [GL vt34] (LogicUpdateWrapper via 0xBD85C4), then 0x632A8A..0x632A92 [0xDE4950]->vt28 when sub==1. LW logic RNG (direct call graph from body 0x6BE51A): 0x6BE20A (loop over LW players with +0x44==1, i.e. AI) -> 0x900AA7 -> 0x900533 -> 0x6D328E, and 0x900AA7 -> 0x9000D4 -> 0x9A63BD -> 0x9A6004 -> 0x6D332C. LogicUpdateWrapper (src/telemetry.cpp) samples g_seedAtExit at GameLogic::update exit, so every LW-logic RNG call is counted in seedOutside at the next call.",
 "dll_vars": [
  {
   "name": "g_seedAtExit / g_seedValid",
   "ctype": "uint32_t / bool",
   "note": "shared with LogicUpdateWrapper"
  },
  {
   "name": "g_trace",
   "ctype": "HANDLE",
   "note": "Telemetry=2 trace file"
  }
 ],
 "risks": "Telemetry only. Must not touch x87/MXCSR. Must not call game code other than 0x6BE50E."
}