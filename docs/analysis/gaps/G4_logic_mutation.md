# G4_logic_mutation

## Summary

G4 result: the B-render allow-list is safe for logic only if four hidden couplings are handled. I verified each one in game.dat, and the same bytes are present in delayfix.dat and game820.dat.
(1) LOGIC RNG. Every logic-RNG consumer goes through the E8 calls to 0x6D328E (202 sites) and 0x6D332C (128 sites); 0x6D34A0 adds 2 more, both from BoneFXUpdate. I found no indirect references and no calls from the AotR sections. The source-file strings each wrapper logs place every site in GameLogic\ code except four: RandomValue.cpp, BuildAssistant.cpp, ThingFactory.cpp (newObject) and GameClient\LivingWorld\LivingWorldEyeTower.cpp. The only consumer reachable per render is the Living World eye tower: GameClient::update 0x6484D9 -> [0xDE4958]->vt6C = 0x49AAB8 -> 0x6C0E4D -> 0x6C0E7A TheLivingWorldManager(0xDE3C08)->vt28 = 0x6121C5 -> 0x7FB3F0 -> 0x7FB396/0x7FB29C -> 0x6D328E (0x7FB2BB, 0x7FB324, plus 0x7FB28C in a loop). It runs every render while the strategic map is active and not paused. If it runs on B-renders, both the eye animation and its logic-RNG consumption double, which changes Living World AI and auto-resolve results. Fix: run it on A-renders only.
(2) LOGIC READS CLIENT CAMERA STATE. At sub 1, GameLogic::update decides whether the logic frame advances from TacticalView vtD8 (0x48B5D3, camera time-freeze flag +0x23D0) and vt78 (0x486352, isCameraMovementFinished). Script conditions read the same state (0x7EB965). The camera steppers that run on B-renders clear these flags. So camera completion must happen in the same logic window as stock ("window-end exactness").
(3) NETWORK MESSAGE COUNT IS LOGIC STATE. The logic dispatcher 0x779A3D creates an AIGroup for every network-range message except MSG_LOGIC_CRC and MSG_SET_REPLAY_CAMERA, and each AIGroup increments TheAI+0x1C. The camera waypoint notifier 0x4872A7 (vt70) appends MSG_CHANGE_CAMERA_ARRIVED_AT_WAYPOINT (0x452) on every call until the LookAt translator acknowledges it during propagateMessages (vt74 at 0x83B469). So camera stepping on B-renders requires message propagation on the same renders. Otherwise duplicate 0x452 messages raise the AIGroup counter, and late propagation slips the message one tick.
(4) A debug-only overlay (W3DDisplay::gatherAnimationDebuggingInfo 0x44ABB2) writes Weapon::m_status from the draw path. The engine itself logs this as a potential desync.
Script engine: 0x604189/0x603452, called on every loop iteration, are only the DebugWindow.dll bridge. Scripts, Lua and LivingWorldLogic run inside the logic sub-step 1 (5 Hz).
Asset service [0xDEF548]->vt28 is the async asset streamer 0xA37E50 and does no logic work.
Debug vt94 (0x43B050) runs only on the halted path.
The game already has the verification tools needed: the client CRC guard (-verifyClientCRC, 0x6CF64E/0x6CF681 around clientUpdate, message "GameLogic changed outside of GameLogic::update()!"), getCRC 0x625886, and a per-call logic-RNG trace with file and line (-deepCRC, log object 0xDE4A30).
Corrections to the synthesis:
- 0xDE7890 is TheShell (update 0x75E1D3); TheInGameUI is 0xDE4830.
- 0x6BE51A is LivingWorldLogic::update. It runs at sub 1 from 0x632A92 and must not be gated. Only the client-side TheLivingWorldManager::update 0x6121C5 is per render.
- Failed tick attempts during frozen time do no logic work unless the command list holds MSG_CLEAR_GAME_DATA (0x62E57E..0x62E58D).

## Design notes

1. The allow-list principle holds for logic. An A-render is a stock render: same m_frame++, same position between logic sub-steps, sync equal to stock with C3. So any subsystem skipped on B-renders keeps stock logic interaction automatically. Logic exposure exists only in code allowed on B-renders, and that code must satisfy four rules:
(a) It must not call 0x6D328E/0x6D332C.
(b) It must not write CRC-covered logic state.
(c) Any client state logic reads at the next sub-step must equal stock after the B-render ("window-end exactness"). This covers camera finished/frozen (vt78/vtD8), the current waypoint +0x23CC, and possibly animation state.
(d) It must emit exactly the network-range messages stock would emit in that window, because each message creates an AIGroup and advances TheAI+0x1C.
2. Window-end exactness: the B-render is the last render before the next A-frame (logic). Its state must equal stock's after the one stock render in that window. For integer frame counters, double them (total, hold and current). For ms accumulators, use pairs that sum exactly to 33 (16/17 alternating); a monotone threshold then fires in the same window. Do not let float ease conversions feed flags logic reads. For C3, the 'lead' phase (sync at stock on A, +16 on B) leaves logic seeing sync+16. Keep a 'lag' phase variant (A:+h, B:+33-h) ready in case M2 shows logic reads animation state.
3. Couplings the allow-list must encode:
- Camera stepping on B requires propagateMessages (0x6324A1) on B, after GameClient::update, so the LookAt translator acknowledges MSG 0x452 in the same render.
- The LW view update must be split: gate the 0x6C0E4D call at 0x49AAD4 (LW manager, eye tower, Palantir-in-LW, region pulses) to A-renders, and let only the LW camera part of 0x49AAB8 run on B with fixes.
4. Corrections to the synthesis:
- 0xDE7890 is TheShell (update 0x75E1D3, ctor 0x75DC29). TheInGameUI is 0xDE4830; its vt28 is 0x48EA1F, which jumps to 0x6A1F4D. GameClient::update calls Shell at 0x648891 only conditionally and InGameUI at 0x6488AE always.
- GameClient::init names: 0xDE4518 DisplayStringManager, 0xDE4334 Keyboard (vt28+vt3C), 0xDE36E0 Mouse (vt28+vt40), 0xDE4AB0 Anim2DCollection, 0xDE8D68 AnimationSoundModuleManager, 0xDE495C WindowManager, 0xDE4958 W3D LW view (unnamed), 0xDE4A34 RayEffects, 0xDF06F8 VideoPlayer, 0xDE7D84 LanguageFilter, 0xDE3B54 SnowManager, 0xDE3C24 CloudEffectManager, 0xDE7734 CloudBreakEffectManager, 0xDE4EFC FireManager.
- Row 79: 0x6BE51A is LivingWorldLogic::update. It runs at sub 1 from 0x632A92 and must not be gated. The per-render LW update is TheLivingWorldManager::update 0x6121C5, called through the LW view. The eye-tower CLIENT_FPS references 0x7FB2CE/0x7FB334 need no change once the A-only gate is in place.
- R1: failed tick attempts during frozen time do no logic work unless MSG_CLEAR_GAME_DATA is pending.
5. Runtime safety net (recommended for release):
- Guard every B-render: compare [0xDA1CA4] before and after; on change, restore it, raise a counter, and log the return address from a prologue hook on 0x6D328E/0x6D332C while g_inB.
- Debug builds: wrap B-renders and each allowed subsystem call in getCRC 0x625886(TheGameLogic,0) before and after.
- M2: run identical replays with -deepCRC in 30 and 60 mode, hook 0x6CF86E to dump 'logicrandom (file,line)' lines, and diff. Also log the network-message count per tick and TheAI+0x1C.
6. Everything else per render that does not draw (InGameUI, Shell, Eva, Radar, Audio, ControlBar, Palantir, GameClient vt90, 0x62B385, the subsystem vt28 calls in GameClient::update, the drawable block) should stay A-only. That matches the 'safe default' and the logic-safety analysis above.

## Items

### G4-01 — logic RNG census  [n/a, medium]
- **Site:** 0x6D328E (int, 202 call sites), 0x6D332C (real, 128), 0x6D34A0 (GameLogicRandomVariable, called from 0x88A41D/0x88A7B7 BoneFXUpdate), seeding 0x6D3261 (11 callers)/0x6D321C (5), CRC xfer 0x6D320A (called from getCRC 0x625886), getSeed 0x6D3204 (0x62D44C, 0x77D7C1)
- **What:** Full list of logic-RNG call sites, found by an E8/E9 scan of .text and the AotR sections, a search for absolute references to 0xDA1CA4 and the function addresses (only the 5 wrapper instructions reference the seed), and the __FILE__ string each wrapper logs. All sites are in GameLogic\ files except RandomValue.cpp, BuildAssistant.cpp 0x797465 (logic callers only), ThingFactory.cpp newObject 0x6D165E (logic callers and loaders) and GameClient\LivingWorld\LivingWorldEyeTower.cpp (0x7FB22E, 0x7FB29C). The forward direct-call closure from the per-render roots (about 2650 functions) does not intersect the 1520 logic-RNG ancestors. Cross-checking vtables found no client class with a method that reaches logic RNG; the only hits were shared stub functions.
- **Cadence:** n/a (census)
- **Reason:** The only consumer on a per-render path is the eye tower (G4-02). Virtual dispatch was resolved only for known singletons and vtables, so verify at runtime (G4-22).
- **Risk if wrong:** An unlisted virtual path from per-render code to logic RNG would make logic depend on the render count.

### G4-02 — Living World eye tower (client)  [skip_on_B, high]
- **Site:** 0x6484D9 (GameClient::update: [0xDE4958]->vt6C = 0x49AAB8, W3D LW view, vtable 0xBDE918, created by W3DGameClient vtC8 0x44C173 at 0x646EDD) -> 0x49AAD4 call 0x6C0E4D -> 0x6C0E7A TheLivingWorldManager(0xDE3C08, vtable 0xBFB548)->vt28 = 0x6121C5 -> 0x6121F2 call 0x7FB3F0 -> 0x7FB396 -> 0x7FB29C -> 0x6D328E at 0x7FB2BB/0x7FB324, and 0x7FB22E -> 0x7FB28C
- **What:** The eye tower's look direction lerps by +0x68 += +0x64 per call, where +0x64 = 1/((rand(0..2)+4)*CLIENT_FPS). When a look finishes, it picks a new duration, target and hold time with LOGIC RNG (lines 0x163/0x16E/0x176; target re-rolled in a do-while until it differs). State 1 (0x7FABB0) holds for +0x6C per call.
- **Cadence:** R: every render via GameClient::update. Skipped when paused (0x6C0E56: GameLogic+0x124) or when the LW view is inactive (+0x18). Not gated on m_frame.
- **Reason:** In stock there are exactly 6 eye-tower updates per logic tick, interleaved with Living World AI and auto-resolve RNG use at sub 1. Running it only on A-renders keeps the stock count and order of logic-RNG calls between logic sub-steps. A split-step fix is not exact, because the float accumulator could cross 1.0 in a different window.
- **Risk if wrong:** If it runs on B-renders, the logic RNG advances twice as often in Living World mode. Living World AI and auto-resolve then diverge from 30 FPS, which breaks bit-identical logic, and the eye animation runs 2x fast.
- **Fix @ 0x49AAD4:** call stub: if (g_m60 && !g_uiTick) skip; else jmp 0x6C0E4D (ECX preserved). Gates the LW manager update, eye tower, region pulses, Palantir-in-LW and LW moveTo. The LW camera code later in 0x49AAB8 still runs on B (camera task). (orig: e8 74 63 22 00 (call 0x6C0E4D))
- **Fix @ 0x6C0E72:** Narrower alternative: e8 <stub> 90*6, where the stub calls [[0xDE3C08]]+0x28 only on A-renders (g_uiTick). Leaves 0x6C0BD2/0x6C038B per render, which then need their own fixes (G4-03). (orig: 8b 0d 08 3c de 00 8b 01 ff 50 28)

### G4-03 — Living World manager visual children  [skip_on_B, medium]
- **Site:** 0x6C0BD2 -> 0x7FC09D (region pulse +0x18 += [0xBE53C8]/[0xBDAD78] per call); 0x6C038B (items vt+0x1C); inside 0x6121C5: 0x6112A9 (vt+0xC on lists +0x248/+0x23C, particle-based region effects created at 0x6113F6), 0x6110BE (debug position text), 0x7EFB7C->0x8E646C, list +0x220 vt+0xC, tail call TheAptPalantir(0xDE4A70)->vt28; 0x6C0E86 jmp 0x6BF78E (LW moveTo)
- **What:** Per-call visual integrators and updates on the strategic map. TheLivingWorldManager::update also drives the Palantir update while in Living World mode.
- **Cadence:** R (same path as G4-02)
- **Reason:** Visual only, but they integrate per call. The 0x49AAD4 gate from G4-02 keeps them at stock cadence for free.
- **Risk if wrong:** 2x region-pulse and Palantir counters and a 2x-fast LW camera move-to (cosmetic). No logic impact found.
- **Fix @ 0x49AAD4:** Same gate as G4-02. (orig: e8 74 63 22 00)

### G4-04 — Living World logic (turns, auto-resolve, LW AI)  [not_on_B_path, high]
- **Site:** LivingWorldLogic::update 0x6BE50E (body 0x6BE51A; vtable 0xC14574+0x28), called at 0x632A8A..0x632A92 inside vt98 0x6329B0 only when sub==1
- **What:** Living World turn logic: countdown +0x100++, 0x6B5929 countdown, auto-resolve 0x6BC274->0x8FEDF2->0x8FE6AF (logic RNG), LW AI 0x6BE20A->0x900AA7->0x900533 (logic RNG), Shell vt28, GameLogic+0x1B4 FP-scope decrement.
- **Cadence:** L: once per logic tick (sub 1), about 5 Hz
- **Reason:** vt98 is called only on A-frames under model H. Synthesis row 79 is wrong to suggest gating 0x6BE51A; gating it would change logic.
- **Risk if wrong:** Gating or doubling it would change Living World logic directly.

### G4-05 — ScriptEngine debugger bridge (GameEngine::update head)  [run_on_B_as_is, high]
- **Site:** 0x6325B9 call 0x604189 and 0x6325C4 call 0x603452 (ECX = TheScriptEngine 0xDE3BAC)
- **What:** 0x604189: unless +0x1A5D9 is set, calls 0x603A0F (DebugWindow.dll SetFrameNumber(m_frame or LW frame), only if the HMODULE at 0xDE3B98 is loaded), then polls CanAppContinue into 0xDE3B9C (set to 1 when no DLL). 0x603452 = isTimeFrozenDebug: (DLL loaded && !canContinue); its result is bl at 0x6325C9, which halts the stepper. The DLL is loaded only by 0x605658 (DebugWindow.dll / DebugWindowLite.dll). No script evaluation happens here.
- **Cadence:** Every loop iteration, 60/s under H (not per render)
- **Reason:** No-op in retail. With the debugger DLL, faster polling is harmless. Real script evaluation runs only in GameLogic::update sub 1 (see G4-06).
- **Risk if wrong:** None for logic. With the debug DLL attached, a halt could start on a B iteration (debug only).

### G4-06 — script evaluation and command processing  [not_on_B_path, high]
- **Site:** GameLogic::update 0x62E4E8, sub==1 branch: TheScriptEngine(0xDE3BAC)->vt28, TheLuaScriptEngine(0xDE7804)->vt28, [0xDE4690]+4 vt28, [0xDE897C] vt28; processCommandList 0x625804 -> dispatcher 0x779A3D
- **What:** Scripts, Lua and command dispatch run only inside the logic sub-step 1.
- **Cadence:** L: 5 Hz
- **Reason:** They are reached only through vt98 on A-frames. Scripts do not run twice as often at 60 FPS.
- **Risk if wrong:** n/a

### G4-07 — async asset streaming manager (perf tag 'asst' 0x61737374)  [run_on_B_as_is, medium]
- **Site:** 0x6325A8 [0xDEF548]->vt28 = 0xA32C30 -> jmp 0xA37E50 (object built at 0x44676E in W3DDisplay::init, ctor 0xA32B90, vtable 0xC95090, freed at 0x4498C1)
- **What:** Pumps the load-stage queues for stages 0..6 under a CRITICAL_SECTION and runs the per-task stage callbacks (vt+4..+0x1C). Helpers 0x605106/0x603AF6 are a generic std::map lower_bound, not script code.
- **Cadence:** Every loop iteration (60/s under H)
- **Reason:** Streaming is already timing-dependent in stock (background thread plus I/O), and logic does not depend on it, since MP lockstep would desync otherwise. Pumping more often only makes streaming faster.
- **Risk if wrong:** Low. Only if a stage callback touches logic during gameplay; the CRC guard in G4-22 would catch it.

### G4-08 — Debug logging  [n/a, high]
- **Site:** 0x6325ED Debug(0xDC62C0, vtable 0xBD47C0)->vt94 = 0x43B050 (only on the halted path after 0x6325DE); 0x63263A FUN_006251A3 -> Debug vt90(frameId)
- **What:** Debug string-buffer processing and the debug frame id.
- **Cadence:** vt94: only while halted. 0x6251A3: per stepper frame.
- **Reason:** Debug only, no logic state. The frame-id overlap is cosmetic (C1f).
- **Risk if wrong:** None

### G4-09 — frozen-time branch (synthesis R1 correction)  [n/a, high]
- **Site:** GameLogic::update 0x62E4E8 sub 1: 0x62E528 TacticalView vtD8, 0x62E53A vt78, 0x62E57E..0x62E58D TheCommandList(0xDE639C)->vt44(0x1D = MSG_CLEAR_GAME_DATA), else C8=0 and goto 0x62EE54
- **What:** When camera time-freeze is active and the camera move is unfinished, sub 1 does nothing (only the FP scope, the debug bridge and the MP network call) unless the command list contains MSG_CLEAR_GAME_DATA. The per-call work (0x5FF9D3, 0x6F2364, ...) is skipped on this path.
- **Cadence:** One attempt per tick attempt
- **Reason:** The number of failed attempts during freeze does not affect logic. C1e (restore s to 11) is still fine, but R1's frozen-time concern is mostly moot. What does matter is the camera state that logic reads (G4-10).
- **Risk if wrong:** n/a

### G4-10 — camera state that logic reads  [run_on_B_with_fix, high]
- **Site:** Readers: 0x62E528/0x62E53A (GameLogic::update), ScriptConditions 0x7EB7CD case 0x7EB965 (CAMERA_MOVEMENT_FINISHED), 0x77CC8A (dispatcher), 0x4863B6 (reads +0x23CC). W3DView (vtable 0xBDD490): vt78 0x486352 isCameraMovementFinished, vtD8 0x48B5D3 (+0x23D0 frozen), vtDC 0x48B5DA (+0x23D4 multiplier, read only by 0x44B97F). Writers in steppers: 0x4868C5/0x4868F6 (0x48688A), 0x48986C/0x4898B3 (0x489817), 0x48A452/0x48A6C3 (0x48A417, counters +0x1AC total/+0x1B0 cur/+0x1BC hold), 0x48B2AE (0x48B107)
- **What:** Per-render camera steppers in W3DView::update clear the 'moving' (+0x1DC) and 'time frozen' (+0x23D0) flags when a scripted move finishes. Logic reads them at sub 1, which decides logic-frame advance, and in script conditions.
- **Cadence:** R (camera steppers in W3DView::update via drawFrame updateViews)
- **Reason:** The camera must run on B-renders for smoothness, but completion must land in the same logic window as stock. Integer frame-count steppers: double the total, hold and current counts, so completion falls at render 2N (the B-render of the same window), which is exact. ms-accumulator steppers (+33 per call -> 16/17 alternating): any pair sum of 33 with a monotone threshold gives the same first window. Float ease conversions (1-sqrt(1-f)) are not exact and must not feed any flag logic reads.
- **Risk if wrong:** Frozen time ends, or CAMERA_MOVEMENT_FINISHED fires, one logic tick early or late. The logic frame count, script timing and everything downstream diverge, which breaks bit-identity. Saves are fine, but outcomes differ.
- **Fix @ 0x485DB9/0x488696 (+0x1BC hold=0) and the frame-count setup sites listed in synthesis row 55:** When doubling frames (row 55), also double every non-zero hold (+0x1BC) and any other frame counter used in the completion tests at 0x48A6A0. Keep the integer arithmetic exact.

### G4-11 — camera waypoint notification -> logic message  [split_step, high]
- **Site:** Emitter 0x4872A7 (W3DView vt70; appends 0x452 = MSG_CHANGE_CAMERA_ARRIVED_AT_WAYPOINT without updating +0x23CC). Callers: path stepper 0x4868ED/0x486A06 (0x48688A), 0x48975D/0x489807, W3DView::update 0x48BE3D (vt70(0) every render while following an object). Acknowledgement: LookAt translator 0x83AC4A at 0x83B44B..0x83B469 (vt74 = 0x48B67C sets +0x23CC). Propagate: clientUpdate 0x6324A1 call 0x7128C3
- **What:** In stock, the camera emits 0x452 in GameClient::update and propagateMessages acknowledges it in the same render, so exactly one message goes to TheCommandList per waypoint change. The dispatcher then creates an AIGroup (TheAI+0x1C++) and logic reads +0x23CC.
- **Cadence:** R (event-driven inside per-render camera stepping)
- **Reason:** Coupling: every render that runs camera stepping must also run propagateMessages after it. Otherwise (a) the next render's stepper re-emits the same message before the acknowledgement, producing duplicate network messages and extra AIGroups, and (b) a message emitted on a B-render reaches TheCommandList only after the next logic sub-1. Alternatively, hook 0x4872A7 to also store +0x23CC = id when appending in 60 mode (idempotent with the later vt74) AND flush the message stream before every A-frame.
- **Risk if wrong:** TheAI+0x1C (AIGroup id counter) diverges from stock, and waypoint-arrival script logic slips by one tick in cinematics.
- **Fix @ 0x6324A1:** Keep it on the B-render allow-list, at its stock position after GameClient::update. (orig: e8 1d 04 0e 00 (call 0x7128C3, propagateMessages))
- **Fix @ 0x4872A7:** Optional hardening: in 60 mode, after appending 0x452 also write [ecx+0x23CC]=id. (orig: thiscall(id): cmp id,[ecx+0x23cc]; ...)

### G4-12 — logic command dispatch  [n/a, high]
- **Site:** Dispatcher 0x779A3D: 0x779AB4..0x779AF6 (type 0x3E8..0x7CF, excluding 0x44A and 0x447) -> TheAI 0x70042A createGroup -> AIGroup ctor 0x7707ED increments [0xDE4B40]+0x1C; empty group destroyed at 0x6FFB68; LW types 0x6A4..0x76B -> 0x6BE6A6
- **What:** Every network-range command processed at sub 1 creates an AIGroup and advances the group-id counter, so the NUMBER of logic messages is itself logic state.
- **Cadence:** L (sub 1)
- **Reason:** Rule for the allow-list: B-render code may emit network-range messages only for events stock would also emit once in that window. Input-driven commands are fine. Per-frame emitters must stay A-gated or be exactly one per window: MSG_SET_REPLAY_CAMERA is tick-gated (G4-13), drawable fade deselects are A-only (G4-15), InGameUI deselect is A-only (G4-16).
- **Risk if wrong:** Extra or missing messages change TheAI+0x1C and possibly group ordering, so CRC and saves differ.

### G4-13 — camera scroll / replay camera message  [run_on_B_with_fix, high]
- **Site:** LookAt per-frame 0x83B471 (GameClient::update 0x6484B2): 0x83B9FA GD+0xB70 && 0x83BA12 isTick 0x63252F (s==1) && (0x6253BF or mode==2 or MP) -> 0x83BA93 append 0x447 MSG_SET_REPLAY_CAMERA; other writes only to client globals 0xDE8CB4..0xDE8CB7
- **What:** Per-render scroll, rotate and zoom application. Once per tick it emits the replay-camera network message, which the dispatcher explicitly excludes from AIGroup creation (0x779AD4).
- **Cadence:** R. The message is per tick: isTick is true only on the A-render after sub 1, because B-renders see s even.
- **Reason:** No logic mutation; the message is automatically A-only under H. The scroll and rotate integrators need the camera-task fixes (rows 58-60).
- **Risk if wrong:** If isTick were ever true on a B-render, there would be a duplicate replay-camera record (harmless to logic, larger replays).

### G4-14 — input stream and message propagation  [run_on_B_as_is, medium]
- **Site:** clientUpdate 0x632409: 0x6324A1 propagate 0x7128C3 -> 0x711034 (TheCommandList vt38 append); 0x6324BC TheKeyboard(0xDE4334) vt28; 0x6324E8..0x6324EE mouse focus 0x5EDCF7; GameClient::update: 0xDE4334 vt28+vt3C (keyboard), 0xDE36E0 vt28+vt40 (TheMouse), 0x50EB3C ([0xDE8CC8]+8 vt4 = 0x83DD75 input-mode state machine)
- **What:** Raw input messages (e.g. MSG_RAW_MOUSE_POSITION each frame) go through the translators into TheCommandList, which logic consumes only at sub 1 (0x625804).
- **Cadence:** R
- **Reason:** propagateMessages must run on B because of G4-11. Raw client messages are ignored by the logic dispatcher (no group for types below 0x3E8), and user commands depend only on input. Generating input on B (keyboard/mouse createStreamMessages, 0x50EB3C) is optional; skipping it costs at most 16 ms of input latency.
- **Risk if wrong:** If a translator turns a held input into one network message per frame, 60 Hz input generation would send twice as many commands for the same input. That is not a determinism break (replays carry their commands), but it changes how input behaves. Verify with per-type message counters.

### G4-15 — Drawable update block  [not_on_B_path, high]
- **Site:** GameClient::update drawable block (runs only if DAT_00D9F6F8 != m_frame): 0x675996 updateDrawable, 0xDE4BD0 vt18, 0x68D8F7/0x678F54, 0xDE8D68 (TheAnimationSoundModuleManager) vt28, 0x64594B; Drawable fades 0x673114/0x6760F9 -> 0x671841 append 0x3ED MSG_REMOVE_FROM_SELECTED_GROUP
- **What:** Per-m_frame drawable logic-facing work, including network selection messages.
- **Cadence:** M (gated on m_frame change, so A-renders only under H)
- **Reason:** Kept at stock cadence automatically, because m_frame does not change on B-renders.
- **Risk if wrong:** If m_frame ever advanced on a B-render, there would be duplicate selection messages and AIGroups.

### G4-16 — InGameUI::update  [skip_on_B, high]
- **Site:** 0x6488A6..0x6488AE (8b 0d 30 48 de 00 8b 01 ff 50 28): TheInGameUI 0xDE4830 vt28 -> 0x48EA1F jmp 0x6A1F4D
- **What:** ControlBar update, Palantir 0xDE4A70 vt28 (0x6A23D7), 0xDE3D6C vt28, 0x8EB9B4, 0x822A35/0x82326D. Tail loop: if m_frame % CLIENT_FPS == 0, deselect drawables whose 0x68D8F7() is 3 or 4 via vt10C, which sends selection messages.
- **Cadence:** R (the deselect is m_frame-modulo)
- **Reason:** Not needed for the draw. Running it on B would fire the m_frame%30 deselect twice (idempotent) and double the message-fade, Palantir and ControlBar integrators. Label correction: 0xDE4830 is TheInGameUI; 0xDE7890 is TheShell.
- **Risk if wrong:** Doubled UI integrators. Logic risk is minimal because the deselect is idempotent.
- **Fix @ 0x6488A6:** e8 <stub> 90*6; the stub calls [[0xDE4830]]+0x28 only on A-renders (g_uiTick). (orig: 8b 0d 30 48 de 00 8b 01 ff 50 28)

### G4-17 — Shell::update  [skip_on_B, medium]
- **Site:** 0x648883..0x648893 TheShell(0xDE7890, ctor 0x75DC29, vtable 0xC2C894)->vt28 = 0x75E1D3 (called only if GameLogic+0x125==0 or shell+0x5C); also called from LivingWorldLogic::update
- **What:** Shell and window flow plus the real-time shell throttle.
- **Cadence:** R in game; also at sub 1 in Living World
- **Reason:** Safe default; not draw-related.
- **Risk if wrong:** Shell transitions run 2x (cosmetic).
- **Fix @ 0x648891:** Gate with g_uiTick (stub). (orig: 8b 01 ff 50 28)

### G4-18 — GameLogic start/loading transition  [skip_on_B, high]
- **Site:** 0x63241E and 0x648817: TheGameLogic -> 0x62B385
- **What:** Returns at once unless GameLogic+0x9C (game-start pending) is set; then runs the loading fade and waits and clears +0x9C. MP branch (TheNetwork, mode 1/2/5, logic frame < 6) calls 0x62550D and accumulates DAT_00DE4128.
- **Cadence:** R (twice per render), effectively one-shot
- **Reason:** No-op in steady state. 60 mode is enabled only after the first successful tick, when +0x9C is already 0. Running it as-is would also be fine.
- **Risk if wrong:** None found

### G4-19 — debug overlay mutating Weapon state  [skip_on_B, high]
- **Site:** W3DDisplay::draw 0x44B7D4..0x44B7DD (only if display+0x30 == 0x443683, a debug display mode) -> 0x44ABB2 gatherAnimationDebuggingInfo; SP path 0x44AF9D..0x44AFE9: FP scope 0x401174, call 0x6CDCE7 -> Weapon::getStatus 0x6CD142(&dirty) -> 0x6CA232 writes Weapon+0x10 (m_status)
- **What:** A client-side write to logic weapon state. The engine itself logs 'POTENTIAL DESYNC: Forced into EnterLogic State. (W3DDisplay::gatherAnimationDebuggingInfo)'.
- **Cadence:** R, only when this debug display mode is active
- **Reason:** Debug only. Within one window the logic frame is constant, so a B-render call would write the same value (idempotent), but it is unnecessary.
- **Risk if wrong:** Low (debug mode only).

### G4-20 — FP-mode scope counter in GameLogic  [n/a, high]
- **Site:** 0x401174 -> 0x401158 (if [GameLogic+0x1B4]==0 call setFPMode 0x440809; then +0x1B4++) / 0x40118A (+0x1B4--); also at the GameLogic::update entry and exit (0x62EE56)
- **What:** The only write to GameLogic, PlayerList, ScriptEngine, LWLogic or TheAI memory found in the per-render direct closure, and it nets to zero. setFPMode = _fpreset + _controlfp(PC_24, RC_NEAR).
- **Cadence:** R (only inside 0x44ABB2)
- **Reason:** Balanced, and logic re-applies its FP mode on every call. The DLL must not leave the x87 control word changed before A-frames (logic resets it anyway).
- **Risk if wrong:** None

### G4-21 — W3D sync clock phase vs logic  [run_on_B_with_fix, medium]
- **Site:** C3 at 0x44B911 (syncAcc 0xDC7580 -> WW3D::Sync 0xDD1E0C/PrevSync 0xDD1E10); model animation 0x4BF560 (delta of 0xDC7580 stored in draw-module +0xB8)
- **What:** With C3 as designed, sync equals stock at the A-render and runs 16/17 ms ahead after the B-render, which is exactly when the next logic sub-step runs. No logic-side reader of 0xDD1E0C/0xDD1E10/0xDC7580 exists in GameLogic::update's direct closure (7360 functions).
- **Cadence:** R
- **Reason:** Safe if logic never reads animation-time-dependent drawable state (animated bones, animation-complete flags). Not proven statically. If the CRC harness shows a divergence, switch C3 to a lag phase: the A-render adds h (16/17), the B-render adds 33-h, so sync is stock-exact when logic runs (about 16 ms more visual latency).
- **Risk if wrong:** If logic reads animated drawable state, results depend on how far rendering has advanced at the A-frame, and logic diverges.
- **Fix @ 0x44B911:** SyncStub as in synthesis C3, with an optional lag-phase mode (A: h, B: 33-h) chosen by the M2 result. (orig: a1 8c 7a dc 00 0f af c6 01 05 80 75 dc 00)

### G4-22 — built-in determinism tooling (verification for G4)  [n/a, high]
- **Site:** clientUpdate 0x632478 call 0x6CF64E (CRC before) / 0x63251C call 0x6CF681 (CRC after; reports 'GameLogic changed outside of GameLogic::update()!' and writes CLIENT_DESYNC_%s.txt only in MP, 0x441B7C); flag 0xDE87C5 set by -verifyClientCRC (handler 0x7BA6C9); getCRC = thiscall 0x625886(TheGameLogic, 0) (objects + RNG seed via 0x6D320A, deep systems with 0xDE87C7); -deepCRC/-binaryDeepCRC (0xDE87C6/0xDE87C7, checked at 0x63AFB7) create log 0xDE4A30, written through 0x6CF86E ('logicrandom = %i (%s, %i)' from every 0x6D328E/0x6D332C call, 'newObj %s id %i')
- **What:** Ready-made detectors for client writes to logic and for the order of logic-RNG calls.
- **Cadence:** n/a
- **Reason:** DLL guards to build. Release: snapshot dword [0xDA1CA4] before each B-render; if it changed afterwards, log the caller (hook the 0x6D328E/0x6D332C prologue '55 8b ec' while g_inB) and restore it. Debug: call 0x625886 before and after each B-render (and each allowed subsystem) and assert equality. For M2: run the same replay with -deepCRC in 30 and 60 mode, hook 0x6CF86E to a file and diff, and also count network messages per tick.
- **Risk if wrong:** Without these, hidden virtual-call paths stay unverified.

### G4-23 — deferred drawable/object deletion  [skip_on_B, medium]
- **Site:** 0x6488B1..0x6488B5 GameClient vt90 = 0x645F65 (frees pending objects in list GameClient+0xE4, then 0x6459CB)
- **What:** Per-render cleanup of the client-side pending-deletion list.
- **Cadence:** R
- **Reason:** Safe default. It also avoids address reuse inside a window, which matters for the C5 physics-replay cache (R8).
- **Risk if wrong:** Freeing earlier inside the window is harmless to logic, but stale C5 cache keys become more likely.

### G4-24 — RNG seeding  [not_on_B_path, medium]
- **Site:** Logic-RNG reseeds 0x6D3261: 0x648F1C/0x6494E3/0x649927 (vtable 0xC54CE0 slots A4/A8/AC, MP game launch), 0x75DED4 (Shell 0x75DE01 from dispatcher), 0x61232B (0x612274 <- 0x6BE6A6, LW transition), 0x91A05E (Create-a-Hero), 0x5EAC4A, 0x903800/0x903A45/0x904691, 0x928736; all-seed 0x6D321C at 0x6D34EF/0x6DECF4/0x77F80F/0x913378/0x92227A
- **What:** Game-start and mode-transition reseeds.
- **Cadence:** E (events)
- **Reason:** None of them is on a per-render path.
- **Risk if wrong:** n/a

### G4-25 — LW camera / camera helpers  [run_on_B_with_fix, medium]
- **Site:** 0x8392A7 LW camera (reads TheLivingWorldLogic 0xDE4950 and TheGameLogic; callees 0x441E4A, 0x6BF47F, 0x838DC0, 0x90F92C); 0x645750 (GameClient+0x13C -> 0x83881D/0x838D1C)
- **What:** Camera integrators. No writes to logic objects found.
- **Cadence:** R
- **Reason:** Camera-task fixes apply (synthesis row 57). No logic exposure.
- **Risk if wrong:** 2x LW camera speed (visual).

### G4-26 — alternate display update path  [skip_on_B, low]
- **Site:** Display vtable 0xBD9C28 +0x188 = 0x444CD4: TheLivingWorldManager->vt28 then, if no [0xDD3474] override, jmp TheFXParticleSystemManager->vt28 (0x444CF2)
- **What:** Second route into the LW manager update (eye tower RNG) and the particle update. No static caller found: no '[reg+0x188]' call with a TheDisplay receiver.
- **Cadence:** unknown
- **Reason:** If it is used (loading or transition paths), it must be gated like G4-02/C7.
- **Risk if wrong:** If it runs per render somewhere, it adds eye-tower logic-RNG consumption.
- **Fix @ 0x444CD4:** Gate with g_uiTick, or detour and log the caller to find out whether it is ever used. (orig: 8b 0d 08 3c de 00 8b 01 ff 50 28)

## Open questions

- Does any logic code read animation-time-dependent drawable or draw-module state (animated bone positions, animation-complete flags) that Drawable::draw advances on B-renders? This decides whether C3 can stay in 'lead' phase or must switch to 'lag' phase. Verify with the CRC harness (G4-22).
- Display vt188 0x444CD4 (LW manager update + FX particle update) has no static caller found. Is it reached at runtime (loading screen, LW transition, movie path), and how often?
- Do any translators (0x83AC4A LookAt, 0x83C29E CommandTranslator, 0x81F8D8, 0x940435 GUICommandTranslator, the 0x50EB3C/0x83DD75 state machine) emit network-range messages once per frame while input is held? If so, 60 Hz input generation doubles those commands; decide whether input generation should be A-only.
- Is TheAI+0x1C (AIGroup id counter) saved by TheAI xfer and included in getCRC? This determines whether duplicate camera-waypoint messages would show up in saves and CRC, or only in group ids.
- ClickReactionBehavior (0x85C75E, vtable 0xC579B8+4) uses logic RNG. How is its click counter fed: through a network message, or by direct client writes from InGameUI selection code (which would be a client->logic write on the input path)?
- Edge case: during frozen-time cinematics on the LW strategic map, does the g_uiTick alternation give exactly the stock number of LW manager updates (N for a freeze of N stock renders)? It depends on the alternation phase at freeze start.
- 0x6C038B/0x6C0BD2 list items and the 0x6112A9 region-effect objects (vt+0xC/vt+0x1C): no logic-RNG reach was found statically. Confirm with the runtime seed guard in Living World mode.
