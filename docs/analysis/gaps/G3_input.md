# G3_input

## Summary

I recommend P2 (strictly P2'). Keyboard and mouse polling, the per-frame input functions and the InGameUI keyboard camera run only on g_uiTick renders (A-renders, or every other render while paused or frozen). propagateMessages keeps running on every render. Camera smoothness comes from split-step on the camera primitives during the A-render input windows. Nothing in the input chain can then run 2x, and none of the per-call counters needs a patch.

Key facts, verified in the dump:
1. BFME2 has no MSG_FRAME_TICK. I scanned every appendMessage (MessageStream vt+0x48) site: none passes type 1, and no translator handles type 1. The Generals frame-tick work in LookAt was moved into direct per-frame calls from GameClient::update: 0x6484B2 to LookAt 0x83B471, 0x6484BD to the LivingWorld translator 0x8392A7, and 0x6484C8 to the chord state machine 0x50EB3C. The message that actually arrives every frame is RAW_MOUSE_POSITION (type 3). Mouse::createStreamMessages emits it unconditionally, once per call, at 0x5EE173..0x5EE19E.
2. Translators are attached only in GameClient::init (0x646955..0x646C91, via attachTranslator 0x710EEC, sorted by priority). Each entry below is priority / translateGameMessage / vtable:
   - 4 and 10 / 0x812C89 / 0xC046DC: Window translator, two instances.
   - 5 / 0x83EE14 / 0xC53E2C: APT mouse translator.
   - 20 / 0x5DA83D / 0xBF1C2C: Meta translator.
   - 25 / 0x75B23B, which goes to 0x75B068 / 0xC046B8: HotKey translator.
   - 27 / 0x838EBA / 0xC53AF4: LivingWorld translator, global 0xDE8CAC.
   - 30 / 0x83E486 / 0xC53DC4: Placement translator.
   - 35 / 0x83E2EE / 0xC53DBC: L+R chord state machine, global 0xDE8CC8.
   - 40 / 0x83D7AD / 0xC53D6C: GUICommand translator.
   - 50 / 0x83C29E / 0xC53D64: Selection translator.
   - 60 / 0x83AC4A / 0xC53D3C: LookAt translator, global 0xDE8CB0.
   - 70 / 0x81F8D8 / 0xC50A08: Command translator.
   - 100 / 0x83816E / 0xC046C0: HintSpy.
   - 999999999 / 0x838291 / 0xC046C8: dispatcher. It keeps only types 0x3E8..0x7CF plus 0x1D..0x22, 0x6A5, 0x7D9 and 0x7ED, which go on to TheCommandList 0xDE639C. Logic consumes that list only at sub 1 (0x779A3D loop).
3. Per-frame or per-call state found in the input chain:
   - Keyboard auto-repeat: 0x63F473, counted in input frames (+0xE1C++ in Keyboard::update 0x63F667).
   - Mouse "held" counters: 0x5EE0B0..0x5EE16D, with +0x5000 for left and +0x5004 for right.
   - APT translator, per type-3 message: the +0x200 capture countdown (0x83EEF1) and the stuck-button release counters +0x1F4/+0x1F8/+0x1FC with "> 5" tests (0x83EF12..0x83EFA2).
   - LookAt special-mode edge rotate: ±0.046 rad (0xC53D5C) per type-3 message (0x83B0A5/0x83B0C0).
   - LookAt per-frame integrators: scrollBy at 0x83B8A0, the LivingWorld scroll at 0x83B86D, numpad rotate at 0x83B994/0x83B9C6, numpad zoom at 0x83B9DD/0x83B9F4.
   - LivingWorld per-frame scroll, rotate and zoom: 0x8392A7.
   - InGameUI keyboard rotate/zoom/scroll: 0x6A21DF..0x6A23CD.
   - WindowManager transitions: 0x6C1602.
   - Everything else is event-, pixel-, millisecond-, m_frame- or logic-frame-based.
4. The camera primitives are virtual and each has a unique vtable slot: scrollBy 0x48C774 @0xBDD4EC, setAngle 0x48CA7F @0xBDD58C, setZoom 0x48CBFE @0xBDD5C0, LivingWorld scroll 0x49B799 @0xBDE944. There are no direct calls to any of them. scrollBy is linear in its offset for a fixed camera orientation and zoom (basis 0x488F7E, translation-invariant). setAngle is additive with wrap. setZoom sets an absolute value. So splitting each A-render camera change into two halves gives exact per-pair totals, up to float rounding and the edge clamp.

Why P2 and not P1:
- Under P2 no type-3 message exists on non-tick renders. Every translator's per-frame work (APT counters, edge rotate, mouseover picking, WindowManager hover) keeps stock cadence automatically. Device counters (key repeat, mouse held) are untouched. This matches the B-render safe-default principle.
- P1 needs about 20 extra patches. It also leaves residual risk in unaudited GUI gadget input procs (WindowManager vtEC chain) and APT internals.
- What P1 would gain is about 8 ms average lower latency for GUI hover/press and camera onset. Unit commands are quantised to 5 Hz logic ticks anyway.
- P2 latency equals stock. Camera onset appears at half amplitude on the A-render and full on the next render.
- The cursor is the hardware Win32 cursor by default (GlobalData+0x9C6=1 at 0x642E38 makes Mouse redraw mode 0 at 0x5EE521; SetCursor at 0x44141D). So cursor motion stays at full OS rate under either policy.
- Double-click is OS WM_*DBLCLK. Drag and click tolerances use OS-timestamp milliseconds and pixels. The middle-click camera-reset window uses m_frame (30 Hz under model H). All of these are fine under both policies.

All patch sites are byte-identical in game.dat, delayfix.dat and game820.dat.

## Design notes

RECOMMENDATION: P2'

What runs where:
- Input polling and the per-frame input functions run only on renders where g_uiTick=1.
- Use g_uiTick, not g_isARender, so that pause and frozen time keep 30 Hz input.
- propagateMessages runs on every render.
- The camera is smoothed by split-step proxies on the camera primitives.

Exact P2 patch set:
- (a) Gates:
  - 0x6484B2: LookAt per-frame; the stub also applies the PRE stash on non-tick renders.
  - 0x6484BD: LivingWorld per-frame.
  - 0x6484C8: chord state machine.
  - 0x6485CC and 0x6485FC: 8-byte selector stubs that zero ECX so the existing `74 10` skips update+stream for keyboard and mouse.
  - 0x6488A6: InGameUI::update. Only a POST window, unless the allow-list runs it on B; then also gate 0x6A21DF.
  - 0x64863A: WindowManager update.
  - Optionally 0x6324B4.
- (b) 0x6324A1 PropStub: POST window on tick renders; POST stash on the other renders.
- (c) .rdata vtable proxies: 0xBDD4EC, 0xBDD58C, 0xBDD5C0, 0xBDE944. Proxies are active only when g_m60 && g_splitWin && this==TacticalView or LW view.
- No counter or immediate patches in the input chain are required.

Safety rules:
- If a stash is non-empty at the start of a tick render, apply it first.
- When g_m60 changes, which happens only at s==1, flush all stashes.
- Proxies should stash only the half that actually took effect (compare state before and after). This handles guard early-outs and locked cameras.

Display timeline under P2:
- Pre-draw changes (LookAt and LivingWorld scroll): A shows half, B shows the full stock state.
- Post-draw changes (InGameUI keyboard camera, translator rotate and zoom): B shows half, the next A shows the full stock state.
- So every render moves the camera by half a stock step, and the per-pair totals are exact.
- After each B-render the camera state equals the stock post-render state, so A-frame logic (which runs after the B-render) sees stock camera values. This matters for script getters 0x48731A/0x487321/0x487328 and position reads.

Why P2 over P1:
- (1) Safe default: with no device poll on non-tick renders there is no type-3 message, so every translator's per-frame work stays at stock cadence by construction. That covers the APT capture and stuck counters, LookAt edge rotate, mouseover picking, WindowManager hover and gadget procs, and the LivingWorld +0x42 flag. Device counters (auto-repeat, held) also stay at stock cadence.
- (2) Logic is unaffected under both policies:
  - The command list is consumed only at sub 1.
  - The replay-camera message 0x447 is isTick-gated (0x83BA0C uses [0xDE4324] FUN_0063252F), so it is sent once per tick either way.
  - With propagate on every render, P2' also keeps the stock invariant that the stream is empty when logic runs.
- (3) Latency:
  - P2 equals stock: polling every 33 ms. Camera onset appears on the next A at half amplitude, full amplitude 16.7 ms later. GUI hover/press stays at 30 Hz.
  - P1 would cut about 8 ms on average, which is negligible for orders that are quantised to the 5 Hz tick.
- (4) Double-click (OS DBLCLK), drag (pixels plus OS-timestamp milliseconds via [Mouse+0x12E8]/[+0x12F0]) and the middle-click reset (m_frame) are rate-independent under both policies.
- (5) Visible P2 downsides are the same as stock 30 FPS: the drag-box corner, the placement ghost and hover highlights update at 30 Hz while the hardware cursor moves at OS rate.

P1 alternative, if ever wanted after M5 instrumentation:
- Keyboard auto-repeat: 0x63F499 0A->15 and 0x63F4D6 0C->14.
- Mouse held: imm 05->09 at 0x5EE0BF, 0x5EE0D5, 0x5EE0E5, 0x5EE12F.
- APT translator: 05->0B at 0x83EF1E, 0x83EF60, 0x83EFA4, plus a tick-gated decrement stub at 0x83EEF1.
- Transitions gate at 0x6C160A.
- LookAt scroll: halving stub at 0x83B8A0 (`8d 55 f0 52 ff 50 5c`); the setScrollAmount argument stays full.
- LookAt LivingWorld branch: halving stub at 0x83B869 (`8d 55 f0 52 ff 50 2c`).
- LivingWorld per-frame vt2C: halve offset and rotation; per cua, 0x49B7AD 0.5->0.25 and 0x49B8D9 [0xD99B64]x0.5.
- Rotate speed: GD+0xC2C x0.5 in 60 mode. Readers: 0x6A2205, 0x6A222D, 0x83B984, 0x83B9B6, and 0x8392A7 (x±15).
- Keyboard zoom half-maps at 0x83B9DD, 0x83B9F4, 0x6A225E, 0x6A227B: zoom in z->0.9797959z-0.5051026, zoom out z->1.0246951z+0.4939015. These compose exactly to 0.96z-1 and 1.05z+1.
- LivingWorld zoom keys: tick-gate.
- InGameUI scroll speed: 0xDA0B24 250->125 (single reader).
- Special-mode edge rotate: 0xC53D5C 0.046->0.023 (only readers 0x83B0A5/0x83B0C0).
- Plus an audit of the gadget input procs.

Corrections to earlier notes:
- (i) cua's auto-repeat fix 0x14/0x13 gives 350 ms; the exact first-repeat value is 0x15/0x14. The held fix 10 gives 150 ms; the exact value is 9 (133 ms).
- (ii) The special-mode edge pitch goes through vt1F0 = 0x9F3A3C, which is just `ret 4` (a no-op). Only the edge rotate matters.
- (iii) Order inside a render: LookAt and LivingWorld per-frame run first, before the keyboard/mouse poll. Draw is at 0x648869, InGameUI::update at 0x6488AE, propagate at 0x6324A1. So translator and InGameUI camera changes are post-draw, and stock has a 1-frame input-to-camera structure. P2 preserves it.
- (iv) Mouse button frame stamps +0x4F2C/+0x4F38/+0x4F44 have no readers. Tooltips use timeGetTime.
- (v) Zoom is not eased. setZoom writes +0x40 directly; the height settle at 0x48C36A eases a terrain-height field. So keyboard zoom needs the split (P2) or half-maps (P1); the height ease does not hide it.
- (vi) Found a per-render LivingWorld zoom-velocity integrator in LW view vt6C 0x49AAB8 (called at 0x6484D9), outside the input block. Hand it to the camera/LivingWorld task with the exact a=1/(1+sqrt d), d'=sqrt d conversion.

All patch-site bytes are identical in game.dat, delayfix.dat and game820.dat.

## Items

### G3-01 — MessageStream propagation  [run_on_B_with_fix, high]
- **Site:** clientUpdate 0x632409: 0x63249B mov ecx,[0xDE6398]; 0x6324A1 call 0x7128C3 (propagateMessages)
- **What:** For each translator (list +0x14, priority order), for each message, call translator->vt[0](msg) and destroy the message on return 1. Survivors go to TheCommandList via 0x711034, then the stream is cleared. With an empty stream no translator code runs.
- **Cadence:** Once per render (only direct caller in the render path is 0x6324A1; the other callers 0x62A6BD/0x62A758 are in logic-side FUN_0062A413). Under P2 the stream on non-tick renders holds no input messages, because the devices are not polled.
- **Reason:** Keep the call on every render (P2'). This preserves the stock invariant that the stream is empty when GameLogic::update runs: logic itself propagates the stream inside FUN_0062A413 (called from 0x62E4E8). It also keeps stock latency for deferred client-to-client messages appended by per-render code, e.g. W3DView 0x48B107 -> 0x4872A7 appends 0x452. The stub additionally opens the POST split window on tick renders and applies the POST camera stash after the call on non-tick renders.
- **Risk if wrong:** If propagation is skipped on B, messages appended during a B-render are propagated by logic's own propagate (0x62A6BD) inside the next A-frame. That changes the order of client message handling, though not the logic math. If input messages were present on B (P1), every per-type-3 translator integrator and counter would run 2x.
- **Fix @ 0x6324A1:** e8 <PropStub>. If !g_m60: call 0x7128C3. Else if g_uiTick: g_splitWin=POST; call 0x7128C3; g_splitWin=0. Else: call 0x7128C3, then apply the POST stash through the original primitives (scrollBy/setAngle/setZoom/LW scroll). (orig: e8 1d 04 0e 00 (call 0x7128C3; ECX already = [0xDE6398] from 0x63249B))

### G3-02 — LookAt per-frame camera (Generals' MSG_FRAME_TICK handler, moved here)  [split_step, high]
- **Site:** GameClient::update 0x64849E: 0x6484A8 mov ecx,[0xDE8CB0]; 0x6484B2 call 0x83B471 (LookAt per-frame)
- **What:** Computes the scroll offset per render. RMB mode 1: (anchor-cur)*GD+0xA9C/AA0 plus KSF^2*normalised. Keyboard mode 2: KSF*H*100 [0xBD88D8]. Edge mode 3: KSF*GD+0xAA4*ramp%, with the ramp from timeGetTime vs GD+0xAA8. Then calls InGameUI vtB4 setScrollAmount and TacticalView vt5C scrollBy (0x83B8A0..0x83B8A4), or LW view vt2C (0x83B86D) when the LW view is active. Numpad rotate: setAngle(getAngle()-/+GD+0xC2C) at 0x83B984/0x83B994 and 0x83B9B6/0x83B9C6. Numpad zoom: vt134/vt138 at 0x83B9DD/0x83B9F4. isTick-gated replay-camera message 0x447 at 0x83B9FA..0x83BA8C (only when GD+0xB70 and MP/replay conditions hold).
- **Cadence:** Per render: called unconditionally from GameClient::update, which runs once per render. Runs pre-draw; the display draw is at 0x648869.
- **Reason:** Run only on g_uiTick renders, inside the PRE split window. The vt5C/vtFC/vt130 and LW vt2C proxies apply half of each requested change and stash the other half. On non-tick renders, skip the function and apply the PRE stash at this point, still pre-draw, so the B draw shows the full stock step. scrollBy is linear in its offset; setAngle is additive mod 2pi; zoom uses midpoint split. Per-pair totals equal stock, and after every B-render the camera state equals the stock post-render state, so logic sees stock camera values. The replay-camera message 0x447 stays once per tick: FUN_0063252F(s==1) is true only on the render after the tick, which is a tick render.
- **Risk if wrong:** Running it on both renders unpatched makes scroll, keyboard rotate and zoom 2x. Running it on A only without the split gives 30 Hz camera steps (judder) at 60 FPS. Note that scrollBy's out-of-bounds return flags (used by RMB mode at 0x83B8A7..0x83B92A to re-anchor) are computed after the half step: at a map edge the anchor reset may come one A-render later (cosmetic).
- **Fix @ 0x6484B2:** e8 <LookAtStub>. If !g_m60 || g_uiTick: apply any leftover stash (safety), g_splitWin=PRE, call 0x83B471, g_splitWin=0. Else apply the PRE stash (scrollBy(stash.off), setAngle(getAngle()+stash.ang), setZoom(getZoom()+stash.zoom), LWscroll(stash.lwOff, stash.lwRot)) via the original function pointers, then clear it. (orig: e8 ba 2f 1f 00 (call 0x83B471; ECX=[0xDE8CB0]))

### G3-03 — Living World (War of the Ring) camera input  [split_step, medium]
- **Site:** 0x6484B7 mov ecx,[0xDE8CAC]; 0x6484BD call 0x8392A7 (LivingWorld translator per-frame)
- **What:** Per render. Clears the +0x42 'mouse moved' flag; if no type-3 message arrived since the last call, stops edge scroll via 0x838DC0(8). Computes RMB/keyboard/edge offsets (350.0 [0xC53B00]) and the rotate (+0x28-+0x20)*0.005. Calls LW view vt2C 0x49B799 (offset scaled 0.5*zoom and rotated, angle += [0xD99B64]*rot, clamped). Keyboard rotate: vt2C(&0, GD+0xC2C*(+/-15)). Keyboard zoom: vt54(+/-1), which does velocity += dir*k (0x49AA11).
- **Cadence:** Per render, pre-draw, called unconditionally from GameClient::update.
- **Reason:** Same as G3-02: run on tick renders inside the PRE window; the LW-scroll proxy (vtable slot 0xBDE944) halves offset and rotation and stashes the other half. LW zoom keys add to a velocity that LW view vt6C 0x49AAB8 integrates and damps on every render (see G3-17), so they are not split.
- **Risk if wrong:** Unpatched on both renders, LW scroll and rotate run 2x. On A only without the split you get 30 Hz LW camera steps.
- **Fix @ 0x6484BD:** e8 <LWInStub>. If !g_m60 || g_uiTick: g_splitWin=PRE; call 0x8392A7; g_splitWin=0. Else do nothing (the LW stash is applied by LookAtStub's non-tick path). (orig: e8 e5 0d 1f 00 (call 0x8392A7; ECX=[0xDE8CAC]))
- **Fix @ 0xBDE944 (.rdata, LW view vtable 0xBDE918+0x2C):** &ProxyLWScroll (thiscall, ret 8). If g_m60 && g_splitWin && this==[0xDE4958]: call orig(off*0.5, rot*0.5) and add the other halves to the stash. Else call orig. VirtualProtect is needed; this slot is the only reference to 0x49B799. (orig: 99 b7 49 00 (0x0049B799))

### G3-04 — L+R mouse chord state machine (priority-35 translator)  [skip_on_B, medium]
- **Site:** 0x6484C2 mov ecx,[0xDE8CC8]; 0x6484C8 call 0x50EB3C -> [this+8]->vt4 (state objects 0xC53D80: 0x83DD75; 0xC53D8C: 0x83DDAB; 0xC53D98: 0x83DE6B; 0xC53DA4: 0x83E14F; 0xC53DB0: 0x83E1D2)
- **What:** Polls the mouse button states (+0x4F24 / +0x4F30) and transitions states. The delayed state re-appends a deferred button-down message. Its windows use pixels (0x83DC6D: <5 px) and milliseconds (0x83DCA4: <500 ms on message timestamps); there are no frame counters.
- **Cadence:** Per render, pre-draw.
- **Reason:** It should poll once per mouse update, like stock. Polling again on a non-tick render, with no new mouse data, could cascade two state transitions on one input sample.
- **Risk if wrong:** Low: possible extra state transition or chord misdetection; no timing integrator.
- **Fix @ 0x6484C8:** e8 <ChordStub>: call 0x50EB3C only if !g_m60 || g_uiTick. (orig: e8 6f 66 ec ff (call 0x50EB3C))

### G3-05 — Keyboard polling (DirectInput)  [skip_on_B, high]
- **Site:** GameClient::update 0x6485CC..0x6485E5: TheKeyboard [0xDE4334] vt28 (0x4985CB -> Keyboard::update 0x63F667) and vt3C (0x63F19A createStreamMessages)
- **What:** Keyboard::update increments the input frame +0xE1C, reads buffered DI events (0x63F61B), applies them (0x63F4E2), and runs auto-repeat 0x63F473: the first repeat when inputFrame-stamp > 10 (0x63F497 cmp edx,0xA), then stamp = now-12 (0x63F4D4), which repeats every frame. createStreamMessages emits 0x15 KEY_DOWN / 0x16 KEY_UP per unconsumed event; repeats carry state 0x102.
- **Cadence:** Per render (GameClient::update). There is an extra drain-only update at 0x6324B4 while 0x80000F() reports input disabled.
- **Reason:** Keeps auto-repeat at stock timing (367 ms initial, then 30/s) with zero patches. DI buffer depth per poll equals stock (33 ms). Key-state readers (FUN_0063F0E2) only read state.
- **Risk if wrong:** Under P1 without the imm fixes, auto-repeat would start at 183 ms and repeat at 60/s. That affects text entry and the LookAt/InGameUI key flags. Meta commands ignore repeats (0x5DA83D tests &0x100).
- **Fix @ 0x6485CC:** e8 <KbdSel> 90 90 90. KbdSel: mov ecx,[0xDE4334]; if (g_m60 && !g_uiTick) xor ecx,ecx; test ecx,ecx; ret. The existing je then skips update and stream on non-tick renders. (orig: 8b 0d 34 43 de 00 85 c9 (mov ecx,[0xDE4334]; test ecx,ecx); followed by 74 10 at 0x6485D4, which skips both vt28 and vt3C to 0x6485E6)

### G3-06 — Mouse polling (Win32 ring buffer 0x441370, 256 entries)  [skip_on_B, high]
- **Site:** GameClient::update 0x6485FC..0x648615: TheMouse [0xDE36E0] vt28 (0x44136B -> 0x5ED573: +0x4F94++, 0x5ED3EA fetch) and vt40 (0x5EDD69 createStreamMessages)
- **What:** Converts buffered events to messages: 4/5/6/8 left, 10/11/12/13 middle, 14/15/16/18 right, 0x13 wheel (delta/120). 'Held' counters +0x5000/+0x5004 are set to 0 on down and incremented each call while below 5 (0x5EE0BD/0x5EE0D3); at 5 they emit 9 or 0x11 (0x5EE0DF/0x5EE129). Then type 3 RAW_MOUSE_POSITION is emitted unconditionally (0x5EE173). This type-3 message is the de facto per-frame message for all translators.
- **Cadence:** Per render.
- **Reason:** With no poll on non-tick renders, no type-3 message exists on those renders. That keeps every translator's per-frame work at stock cadence with zero patches: APT counters, LookAt edge rotate, Selection mouseover pick and hint 0xAC/0xAD, WindowManager hover (vtB8 0x6C23AD), Placement ghost, LW +0x42. Mouse held stays at 133 ms after the down frame. The 'button frame' stamps +0x4F2C/+0x4F38/+0x4F44 have no readers. Per-render readers of the mouse position/delta outside Mouse (0x48D26B, 0x8E9DB2, 0x8EAA55, 0x83B045, 0x83EE76) only read, they do not integrate.
- **Risk if wrong:** If the mouse ran every render (P1) without fixes: held messages at 67 ms, APT stuck-release at 100 ms, APT capture window halved, special-mode edge rotate 2x, double picking CPU.
- **Fix @ 0x6485FC:** e8 <MouseSel> 90 90 90. MouseSel: mov ecx,[0xDE36E0]; if (g_m60 && !g_uiTick) xor ecx,ecx; test ecx,ecx; ret. (orig: 8b 0d e0 36 de 00 85 c9 (mov ecx,[0xDE36E0]; test ecx,ecx); followed by 74 10 at 0x648604, which skips to 0x648616)

### G3-07 — InGameUI keyboard camera (meta-driven flags +0x8BB..+0x8C2, set by Command translator cases 0x83..0x8A)  [split_step, high]
- **Site:** InGameUI::update 0x6A1F4D block 0x6A21DF..0x6A23CD (called at 0x6488A6..0x6488B0, post-draw)
- **What:** Rotate: setAngle(getAngle()-/+GD+0xC2C) at 0x6A2205/0x6A222D -> 0x6A223D. Zoom: vt134/vt138 at 0x6A225E/0x6A227B (z*0.96-1 / z*1.05+1). Scroll: offset = KSF*H*250 [0xDA0B24, only reader 0x6A2291] -> scrollBy at 0x6A23CA.
- **Cadence:** Per render (InGameUI::update per render), after TheDisplay draw (0x648869).
- **Reason:** On tick renders, run inside the POST split window (proxies halve and stash). On non-tick renders the block must not run; the POST stash is applied after the B draw at 0x6324A1 (G3-01). Displayed result: B shows the half, the next A shows the full stock state, so motion is smooth with exact per-pair totals. If the B allow-list already skips InGameUI::update on B, only the POST window is needed. If InGameUI::update is allowed on B, also gate the block.
- **Risk if wrong:** Unpatched on B: keyboard scroll, rotate and zoom 2x. A only without the split: 30 Hz steps.
- **Fix @ 0x6488A6:** e8 <IguiStub> 90x6. ecx=[0xDE4830]. If g_m60 && g_uiTick: g_splitWin=POST, call vt28, g_splitWin=0. Else follow the allow-list decision (call or skip). (orig: 8b 0d 30 48 de 00 8b 01 ff 50 28 (mov ecx,[0xDE4830]; mov eax,[ecx]; call [eax+0x28]))
- **Fix @ 0x6A21DF (only if InGameUI::update runs on B):** e9 <KbdCamGate> 90. If g_m60 && !g_uiTick: jmp 0x6A23CD. Else: mov al,[esi+0x8BB]; jmp 0x6A21E5. (orig: 8a 86 bb 08 00 00 (mov al,[esi+0x8BB]))

### G3-08 — LookAt translator camera actions inside propagate  [split_step, high]
- **Site:** LookAt translator 0x83AC4A (priority 60): type 3 / 0x13 / 10 / 12 / 14 / 16 / 0x15-0x16 / 0x24..0x33 / 0x70 / 0x452
- **What:** Type 3 (per render under P1):
- Special mode (W3DView+0x2449, vt1CC 0x48B612): edge rotate ±0.046 [0xC53D5C] per message (0x83B0A5 fadd / 0x83B0C0 fsub -> vtFC). Edge pitch ±0.02 goes to vt1F0 = 0x9F3A3C, which is just 'ret 4' (a no-op).
- MMB drag rotate: angle += dx*0.001 or dx*0.005 [0xBD88A0/0xC53B04], last=cur. This is delta-based.
- RMB and edge-scroll start use pixel tolerance (0x5ED4B9).
Other types:
- 0x13 wheel: one vt134/vt138 call per notch (event).
- 12 (MMB up): camera reset vtC8 if released within 5 m_frames (0x83AD70); m_frame is 30 Hz under model H.
- 0x15/0x16: arrow-key flags.
- 0x24..0x33: bookmarks.
- 0x452: vt74.
- **Cadence:** Type-3 handling runs once per mouse poll; the other types are events.
- **Reason:** Under P2 these calls happen only in the POST window of tick renders, because type 3 exists only there. The vtFC and vt130 proxies split them: edge-rotate and MMB rotate become smooth 60 Hz with exact per-pair totals. Wheel and bookmark/reset camera calls get split into 2-render transitions (cosmetic). No constant patch is needed under P2. P1 would need 0xC53D5C (only readers 0x83B0A5/0x83B0C0) changed from 0.046 to 0.023.
- **Risk if wrong:** Under P1 without the fix, special-mode edge rotate runs 2x. Under P2 without the split, translator-driven rotation shows 30 Hz steps.

### G3-09 — APT/Flash UI mouse routing  [not_on_B_path, high]
- **Site:** APT mouse translator 0x83EE14 (priority 5, size 0x204): +0x200 countdown 0x83EEF1..0x83EEFE; stuck-button counters 0x83EF12 (+0x1F4), 0x83EF54 (+0x1F8), 0x83EFA0 (+0x1FC); floor-2 sets 0x83F013..0x83F019 (edi=2, also the APT button id) and 0x83F06D..0x83F076
- **What:** Per type-3 message:
- +0x200 (APT-owns-mouse frames, set to at least 2 on button events) is decremented; FUN_0083EB7A uses it to route mouse input to APT.
- If APT believes a button is down but the Mouse says it is up for more than 5 consecutive type-3 messages, a synthetic up is sent (FUN_00AE0B10(0,1,1)).
- **Cadence:** Once per mouse poll (type 3).
- **Reason:** Under P2 type 3 is emitted only on tick renders, so stock timing is kept: 2-frame capture, 200 ms stuck release. These are P1-only fixes.
- **Risk if wrong:** Under P1 unpatched: APT capture window 33 ms instead of 67 ms, stuck release 100 ms instead of 200 ms (UI glitches in Palantir/menus).
- **Fix @ 0x83EF1E, 0x83EF60, 0x83EFA4 (P1 only):** 0x0B (fires on the 12th render = 200 ms) (orig: 05 (imm of 83 be f4 01 00 00 05 / 83 be f8 01 00 00 05 / 83 38 05))
- **Fix @ 0x83EEF1 (P1 only):** Stub: decrement +0x200 only on g_uiTick renders. Do not change the floor 2 at 0x83F015, because edi=2 is also the APT button id. (orig: 8d 86 00 02 00 00 8b 08 3b cb 7e 03 49 89 08)

### G3-10 — Keyboard  [not_on_B_path, high]
- **Site:** Keyboard auto-repeat 0x63F473 (thresholds 0x63F497 cmp edx,0xA; 0x63F4D4 sub eax,0xC)
- **What:** Repeat when inputFrame-stamp > 10, i.e. 11 frames (367 ms). Afterwards stamp = now-12, giving a repeat every input frame.
- **Cadence:** Once per Keyboard::update.
- **Reason:** Covered by G3-05 (no keyboard update on non-tick renders). These are P1-only exact fixes: 0x15 at 0x63F499 matches the 367 ms initial delay (cua's 0x14 gives 350 ms), and 0x14 at 0x63F4D6 gives a repeat every 2 renders.
- **Risk if wrong:** P1 without the fix: repeat 2x faster.
- **Fix @ 0x63F499 (P1 only):** 15 (orig: 0a (83 fa 0a))
- **Fix @ 0x63F4D6 (P1 only):** 14 (orig: 0c (83 e8 0c))

### G3-11 — Mouse  [not_on_B_path, high]
- **Site:** Mouse held counters 0x5EE0B0..0x5EE16D
- **What:** +0x5000/+0x5004 are set to 0 on the down event and incremented each createStreamMessages call while below 5. Type 9 (left held) or 0x11 (right held) fires when the counter reaches 5, i.e. 4 frames (133 ms) after the down frame.
- **Cadence:** Once per mouse poll.
- **Reason:** Covered by G3-06. P1-only exact fix is 5 -> 9 (fires on the 9th call = 8 x 16.7 = 133 ms); cua's value 10 gives 150 ms.
- **Risk if wrong:** P1 without the fix: held messages at 67 ms; GUI and other consumers of held react early.
- **Fix @ 0x5EE0BF, 0x5EE0D5, 0x5EE0E5, 0x5EE12F (P1 only):** 09 (orig: 05)

### G3-12 — GUI (GameWindowManager)  [not_on_B_path, medium]
- **Site:** Window translator 0x812C89 (priorities 4 and 10, vtable 0xC046DC, +4 = 0/1) -> TheWindowManager [0xDE495C] vtB8 0x6C23AD / vtBC 0x6C1AF1
- **What:** Forwards mouse types 3..0x13 and keys 0x15/0x16 to winProcessMouseEvent and winProcessKey. Grabbed-window drag uses the message delta. Gadget input procs are reached through vtEC.
- **Cadence:** Once per mouse poll (type 3) plus events.
- **Reason:** Under P2 GUI hover and press update at 30 Hz, the same as stock; the draw is at 60 Hz. The gadget procs are unaudited for per-message counters, which only matters for P1.
- **Risk if wrong:** P1: unknown gadget per-message counters (e.g. scroll-arrow repeat) could run 2x.

### G3-13 — Mouseover, drag box and hints  [not_on_B_path, high]
- **Site:** Selection translator 0x83C29E (priority 50), HintSpy 0x83816E (priority 100) -> InGameUI vt7C 0x69F637 / vt68 / vt80
- **What:** Type 3: drag-box hint while L is held, or pick the drawable under the cursor (TacticalView vt24) and append hint 0xAC (drawable) or 0xAD (location). HintSpy forwards the hint to InGameUI (0x69F637 createMouseoverHint: state and cursor only, no counters). The dispatcher destroys hints (types below 0x3E8, not on the keep list).
- **Cadence:** Once per mouse poll.
- **Reason:** Hints never reach the command list or logic. Under P2 picking stays at 30 Hz (stock CPU cost). The drag box and placement ghost update at 30 Hz while the hardware cursor moves at OS rate, as in stock.
- **Risk if wrong:** None for timing. Under P1, picking CPU per second doubles.

### G3-14 — Remaining translators  [n/a, medium]
- **Site:** Meta 0x5DA83D (20), HotKey 0x75B23B->0x75B068 (25), LW translator 0x838EBA (27), Placement 0x83E486 (30), GUICommand 0x83D7AD (40), Command 0x81F8D8 (70), Dispatcher 0x838291 (999999999)
- **What:** Key-to-meta mapping, with repeats ignored for one-shot metas (&0x100 test). Up events become click messages 0x17..0x1C via insertMessage, with pixel tolerance [Mouse+0x12E8]. Placement ghost rotation by drag distance (pixels). Command and GUI click/drag tolerance uses pixels and DragToleranceMS [Mouse+0x12F0] against OS message timestamps. Selection group double-tap uses the logic frame. Dispatcher filtering is described in G3-01.
- **Cadence:** Event-driven. Their type-3 handling only sets position-based flags.
- **Reason:** No per-frame integrators or counters found. Rate-independent under both policies.
- **Risk if wrong:** Low.

### G3-15 — Camera smoothing mechanism for P2  [split_step, medium]
- **Site:** Split-step camera proxies: W3DView vtable 0xBDD490 slots 0xBDD4EC (scrollBy 0x48C774, ret 4), 0xBDD58C (setAngle 0x48CA7F, ret 4), 0xBDD5C0 (setZoom 0x48CBFE, ret 4)
- **What:** These are the only references to the three functions; there are no direct E8/E9 calls, so the proxies see every caller.
- scrollBy: early-outs on GC+0xC0&&view+0x44 or +0x2501. Otherwise computes the world delta as a linear function of the offset, using the centre pick-ray basis (0x488F7E, depends on angle/pitch only) and the zoom/scale. Adds it to pos (+0xC/+0x10), optionally constrains it (0x4859E3), accumulates |delta| into +0x24F4 (getter 0x48731A), calls setCameraTransform (0x48B7B1), and returns the out-of-bounds flags.
- setAngle: normalises the angle (0x484F05), accumulates +0x24F8, sets the angle, clears the camera-move flags, transforms.
- setZoom: absolute value (+0x40), clamped, accumulates +0x24FC.
- **Cadence:** Called by input code on tick renders. Proxies are active only while g_splitWin != 0 (PRE or POST windows).
- **Reason:** Proxy logic: if g_m60 && g_splitWin && this==[0xDE447C]:
- scrollBy: r = orig(off*0.5); if pos changed, stash[win].off += off*0.5; return r.
- setAngle: d = wrap(a - getAngle()); orig(getAngle() + d/2); stash += d/2 if it took effect.
- setZoom: d = z - [this+0x40]; orig([this+0x40] + d/2); stash += d/2 if it took effect.
Otherwise call orig.
The PRE stash is applied at 0x6484B2 and the POST stash at 0x6324A1 on the following non-tick render. Per-pair totals equal stock: exact for linear and additive changes, up to float rounding and clamp/constraint edge cases. The accumulators sum to the same totals. Zoom uses midpoint split, so the end value is exactly 0.96z-1 or 1.05z+1 when no clamp engages.
- **Risk if wrong:** If proxies were active outside the input windows, scripted or eased camera calls would be split. That is prevented by g_splitWin, which is set only around 0x83B471, 0x8392A7, InGameUI::update or its block, and propagate.
- **Fix @ 0xBDD4EC:** &ProxyScrollBy (VirtualProtect .rdata) (orig: 74 c7 48 00 (0x0048C774))
- **Fix @ 0xBDD58C:** &ProxySetAngle (orig: 7f ca 48 00 (0x0048CA7F))
- **Fix @ 0xBDD5C0:** &ProxySetZoom (orig: fe cb 48 00 (0x0048CBFE))

### G3-16 — GUI window transitions  [skip_on_B, medium]
- **Site:** WindowManager::update 0x6C15FD (call at 0x64863A..0x648642): 0x6C1478 destroy list, then TheTransitionHandler [0xDE3654] vt28 (0x6C1602, tail jmp)
- **What:** Transition frame += direction per call (0x5DB4B1, per cua); deferred window deletion.
- **Cadence:** Per render, gated only by GD+0xAF2/0xAF3.
- **Reason:** It is not input but sits next to it. Skipping it on B keeps WindowTransitions.ini timing at stock. Destroy-list processing at 30 Hz is harmless.
- **Risk if wrong:** If run on B unpatched: menu and window transitions 2x fast.
- **Fix @ 0x64863A:** e8 <WMStub> 90x6: call [0xDE495C]->vt28 only if !g_m60 || g_uiTick. Alternatively gate only the transition tail-jump at 0x6C160A (74 05 8b 01 ff 60 28). (orig: 8b 0d 5c 49 de 00 8b 01 ff 50 28)

### G3-17 — Living World camera presentation (cross-task, found here)  [run_on_B_with_fix, medium]
- **Site:** LW view update vt6C 0x49AAB8 (call 0x6484D7..0x6484D9 on [0xDE4958]), then 0x645750 at 0x6484E2
- **What:** Per render: zoom += velocity (+0x134 += +0x138 at 0x49AB21); velocity *= damping LWdata+0x1DC (0x49AB3E); stop below 0.001. Velocity is fed by LW zoom keys and wheel through vt54 0x49A9CF (velocity -= dir*LWdata+0x1D8).
- **Cadence:** Per render, with no gate.
- **Reason:** This is a per-render integrator outside the input block. If it is allowed on B for smooth LW zoom, convert it per render: velocity factor a = 1/(1+sqrt(d)) on the position add and damping d' = sqrt(d). That gives an exact per-pair total (a*v + a*v*sqrt(d) = v, v'' = v*d). Otherwise skip it on B (30 Hz LW zoom).
- **Risk if wrong:** Unpatched on B: LW zoom glides 2x fast and decays 2x faster.
- **Fix @ 0x49AB21 / 0x49AB3E (design only):** Stub implementing a and sqrt(d) in 60 mode, or gate the call at 0x6484D9 with g_uiTick. Owner: camera/LW task. (orig: addss xmm0,[esi+0x134] / mulss xmm0,[esi+0x138] with [LW+0x1DC])

### G3-18 — Input-disable / transition tasks  [run_on_B_as_is, medium]
- **Site:** clientUpdate 0x6324A6 call 0x80000F (timed task queue [0xDE8900], real-time via 0x7FFFAC timeGetTime) -> 0x6324B4 TheKeyboard vt28 (drain) when bit0; 0x6324BF..0x6324EE InGameUI 0x69B5C7 / Mouse 0x5EDCF7 on toggle
- **What:** Real-time callbacks. While input is disabled the keyboard is drained, which also increments the keyboard input frame +0xE1C.
- **Cadence:** Per render, real-time based.
- **Reason:** Callbacks receive elapsed seconds from timeGetTime, so they are rate-independent. A drain on B only advances +0xE1C while input is disabled, which matters only for keys held across the disabled period. Optional strict gate of the drain call.
- **Risk if wrong:** Negligible.
- **Fix @ 0x6324B4 (optional):** e8 <DrainStub> 90x6: call the keyboard vt28 only if !g_m60 || g_uiTick. (orig: 8b 0d 34 43 de 00 8b 01 ff 50 28)

### G3-19 — Message types  [n/a, high]
- **Site:** Per-frame message identification: no MSG_FRAME_TICK in BFME2
- **What:** Every TheMessageStream vt+0x48/0x4C call site was scanned (patterns mov ecx|eax,[0xDE6398] ... push imm). No type 1 was found, and no translator switch handles type 1. Types used: 3 position; 4/5/6/8 left down/dbl/up/drag; 9 left held; 10/11/12/13 middle down/dbl/up/drag; 14/15/16/18 right down/dbl/up/drag; 0x11 right held; 0x13 wheel; 0x15/0x16 key down/up; 0x17..0x1C clicks (inserted by Meta). Hints are 0xAC.., and command-list types are 0x3E8..0x7CF plus the keep list.
- **Cadence:** n/a
- **Reason:** The Generals FRAME_TICK work lives in the direct per-frame calls at 0x6484B2/0x6484BD/0x6484C8 (G3-02/03/04). Type 3 is the only unconditional per-frame message.
- **Risk if wrong:** If a FRAME_TICK equivalent existed elsewhere, its handlers would need the same gating. The scan found none.

### G3-20 — Cursor  [run_on_B_as_is, medium]
- **Site:** Mouse cursor rendering: Mouse init 0x5EE519..0x5EE529 (GD+0x9C6 ? mode 0 : mode 1); GD+0x9C6=1 at 0x642E38; Win32Mouse setCursor 0x4413E3 (SetCursor import 0xBD0894 at 0x44141D); W3DMouse::draw 0x498CBC modes 3 (DX8 GetCursorPos), 2 (polygon at Mouse+0x4F0C), 1 (W3D)
- **What:** The default redraw mode is 0 (Windows hardware cursor), so the OS moves the cursor at full rate. setRedrawMode is vt68 0x499A56.
- **Cadence:** OS rate in mode 0. In mode 2 the position comes from the last Mouse::update.
- **Reason:** P2 causes no visible cursor lag in the default mode. If another mode is ever selected (mode 2/1), the cursor position would update only on tick renders. Option: in that case refresh only the cursor-draw position via GetCursorPos on B.
- **Risk if wrong:** 30 Hz cursor in non-default cursor modes.

## Open questions

- Does the B-render allow-list run InGameUI::update on B? If it does, the keyboard-camera gate at 0x6A21DF is also required. If it does not, the POST window around 0x6488A6 is enough and the POST stash is applied at 0x6324A1.
- Owner and fix for the per-render LivingWorld view update 0x49AAB8 (zoom velocity and damping) and FUN_00645750 at 0x6484E2: neither is input, both are per render. Skip on B, or apply the a/sqrt(d) conversion?
- Some translator-triggered camera actions inside the POST window (wheel zoom per notch, bookmark jump vt174 0x65E980, middle-click reset vtC8 0x48D26B) would become 2-render transitions if they call setAngle/setZoom/scrollBy virtually. Is that acceptable, or should the POST window be narrowed to LookAt type-3 handling only? Narrowing needs per-site stubs at 0x83B0C6/0x83B14E and the MMB-rotate vtFC calls.
- Is a non-default cursor redraw mode ever selected via vt68 0x499A56 (DX8/polygon/W3D) by AotR or any options path? If so, P2 shows a 30 Hz software cursor unless the B-render refreshes the cursor-draw position.
- In edge cases the RMB-scroll out-of-bounds re-anchor flags are computed after the half step (0x83B8A7..0x83B92A), so the anchor reset can come one A-render late at map edges. Is that acceptable, or should the proxy call scrollBy with the full offset, record the stock flags and position, then write back the midpoint position (state split)?
- Runtime check (M5): under P2 the per-second counts of Keyboard::update (0x63F667), Mouse::createStreamMessages (0x5EDD69), translator invocations with type 3, 0x83B471/0x8392A7 and TransitionHandler updates must equal stock (about 30.3/s), including while paused and during frozen-time cinematics. Under M6, the per-pair camera deltas must match stock.
