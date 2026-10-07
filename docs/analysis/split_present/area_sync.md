# AREA sync

## SUMMARY

Yes, a second thread can help, but only to present the B frame. It cannot run any game logic. The B Present can be handed to a helper thread (T) or issued from logic checkpoints (C) as long as one main-thread drain point is enforced: the return of GameEngine::update at 0x441822. That point comes after the logic step and before the IsIconic loop and the message pump (0x441827..0x4418C2), the pacer (0x63A196), the asset streamer (0x6325B0) and the next render. 

Key facts (CONFIRMED in game.dat):
- **Device flags:** the device is created with HWVP|PUREDEVICE only. There is no D3DCREATE_MULTITHREADED and no FPU_PRESERVE; [0xDD343C] is never written.
- **The engine's own D3D lock:** it serialises D3D work across threads with a mutex of its own: [0xDD1FD8], acquired by 0x51EEC0 (WaitForSingleObject, 20 s) or 0x51EF50 (with a timeout) and released by 0x5208D0. There are 116 acquire sites. The load-screen thread already draws and presents from a second thread under this mutex (0x65C19B). That is the precedent for (T).
- **drawFrame holds that mutex** from 0x449D0D to 0x44A271, and the B Present at 0x522650 happens inside that window.
- **Deferring is invisible to the game:** the HRESULT the stub returns is only used for statistics ([0xDD1F38] is write-only; [0xDD34C4] is read only for stats). Returning S_OK for a deferred Present is harmless.
- **The logic step resets the FPU itself:** GameLogic::update 0x62E4E8 calls 0x440809 (_fpreset + _controlfp PC24/RN), so (C) must restore the x87 control word and MXCSR exactly.

(T) needs:
- the helper to present under the game's mutex, using the raw device vt+0x44 only;
- a lock-free job state machine with steal semantics;
- safety-net drains at every place that can render or reset;
- message-aware main-thread waits.

Its one hazard that cannot be fully excluded is DXVK's Present sending a synchronous window message (INFERRED: it does not in steady state). Recommendation: (C) or a hybrid (H), where a helper thread only times the target and sets a flag, is the lower-risk way to use a thread. (T) is acceptable with the listed guards.

## DETAILS

## 1. Main thread after PRESENT_STUB returns on a B-render (all CONFIRMED by disassembly unless marked)

Call chain of the B Present: drawFrame 0x449CF8 -> 0x44A228 WW3D::End_Render 0x516DA0 -> 0x5225E0 DX8Wrapper::End_Scene: EndScene vt+0xA8 (0x522628), 0x5766A0, then PRESENT_STUB at 0x522644. The main thread holds the DX8 mutex (0x449D0D) the whole time.

After the stub (resume 0x522653):
1. **End_Scene tail.**
   - The stats counter [0xDD34D8] is incremented.
   - The HRESULT is consumed:
     - on success: [0xDD34C4]++ (read only in the stats at 0x5225FF; zeroed by Reset_Device 0x5221EB) and [0xDD1F38] = 0 (write-only in the whole listing);
     - on 0x88760868 DEVICELOST: TestCooperativeLevel vt+0xC (0x522683), then on 0x88760869 Reset_Device(1) 0x52268F, otherwise Sleep(200) through 0x5B8830;
     - on other errors: the 0x51EDF0 logger.
   - Then wrapper VB/IB refcount drops ([0xDD4298], [0xDD42A0]; 0x538F70/0x538260 only decrement; a final release calls vt0, i.e. a D3D Release is possible) and texture refcount drops ([0xDD4020+4i] via 0xA329C0 -> vt20 delete).
   - **Returning S_OK for a deferred Present has no logic-visible effect.**
2. **WW3D::End_Render tail.**
   - 0x529E50 stats (if arg), 0x529090.
   - 0x51F690 -> 0x51C600: **16x IDirect3DDevice9::SetTexture(i,NULL) (vt+0x104)** and **IUnknown::Release (vt+8) of [0xDD1EF0+4i]**.
3. **drawFrame tail.**
   - SceneRestore DLL stub at 0x44A23E (camera matrices; shadow refit 0x47D37D, no device use INFERRED).
   - 0x441E23, 0x603418, 0x90F92C.
   - **Mutex release 0x5208D0 at 0x44A271.**
4. **W3DDisplay::draw tail (0x44BBFA..0x44BC6D).**
   - TV vt78 and 0x90F92C. If both conditions hold, **0x44BC20 jumps back to 0x44B9A8 and runs a complete second drawFrame** (mutex, TCL, RTT, Begin_Render Clear/BeginScene, draws, a second Present) in the same render.
   - The tile/capture branches (GD+0xEA0 loop at 0x44BAC7; GD+0xEA4 -> vt14C frame save) also draw or read frames.
   - 0x4791E4.
5. **Rest of the B clientUpdate (allow-list).**
   - Deferred-release queue 0x532D6F (Release under the mutex) from GameClient::update 0x648650.
   - GC vt90 deferred drawable delete (render-object/resource Release).
   - 0x7128C3, snow.
   - OnPostRender.
6. **Stepper 0x6325D5..0x632704.**
   - Halt test, s++ 0x632625, 0x6251A3.
   - GameLogic::update via LogicUpdateWrapper at 0x6326C6/0x6326F0.
   - LW logic 0x632A8A.
   - Debug vt94 0x6326F6.
   - In logic: CreateVertexBuffer vt+0x68 0x539290 (bracketed itself) and EvictManagedResources vt+0x14 0x51DFB0 are reachable from 0x62E4E8 by **direct** calls (CONFIRMED path ...0x779A3D -> 0x8CDC11 -> 0x5F530A -> 0x5FC2FE -> 0x5FBE28 -> 0x7B0F90 -> 0x53A6E0 -> 0x539290; a static-init path, so rare).
   - 115 of the 151 functions that reference [0xDD3474] are reachable only through virtual calls (unresolved), so texture, IB and particle-buffer creation and Lock during object creation are INFERRED.
   - 0x532D6F is also called from GameLogic functions 0x627C1F, 0x6282F0, 0x62A11A, 0x62B197, 0x62DC3E, 0x62DCE4, 0x62F1A4, 0x62F397, 0x62F91A, 0x6311ED (CONFIRMED callers).
7. **0x441827: Win32GameEngine::update tail.**
   - IsIconic([0xDC3C64]) loop: Sleep(5) + vt5C + audio, from 0x44184C.
   - **Tail jump vt5C = serviceWindowsOS 0x441A7D at 0x4418C2** (PeekMessageA PM_NOREMOVE / GetMessageA / TranslateMessage / DispatchMessageA; WM_TIMER handler 0x441682).
   - **This is the only per-iteration pump.** 0x63F6BF is the launcher handshake (startup only).
8. 0x639FEF..0x63A170: TV vtDC, 0x603491, network, GC vt7C; no device use found.
9. Pacer 0x63A196, then watchdog 0x631D04, then the loop head 0x639EC6.
10. Next GameEngine::update 0x6325A0:
    - **asset streamer [0xDEF548] vt28 = 0xA37E50 at 0x6325B0, every iteration, BEFORE C0**; its texture paths 0xA33CA0/0xA35980/0xA37320 take the DX8 mutex;
    - 0x604189/0x603452;
    - C0 0x6325CF.
11. Next A-render.
    - In clientUpdate before the draw: radar/shroud/terrain texture locks (INFERRED; none of them touch the back buffer).
    - drawFrame 0x449D0D mutex -> **0x449D17 call 0x516C40: TestCooperativeLevel vt+0xC = first frame-level device call** -> particles/trees -> RTT passes 0x47D5C9/0x47F1AC (own Begin/End_Render without Present) -> **0x449EA5 Begin_Render 0x517B30 (TCL, Clear vt+0xAC, BeginScene vt+0xA4)**.
    - Other draw paths 0x4436DD and 0x4475E4 also enter through 0x517B30 or 0x516C40.

### Latest safe points
- **API order (back buffer):** the Present must be issued before the next frame-level call, i.e. before 0x449D17 / 0x516C40 / 0x517B30 of any drawFrame. The same-render redraw loop at 0x44BC20 makes this reachable before the next C0.
- **Window messages:** the Present must be complete before 0x441827 (IsIconic loop, then the pump at 0x4418C2). The pump runs the game's WndProc and the DXVK or native-d3d9 window hooks (WM_ACTIVATEAPP minimise, display-mode restore), which touch the swapchain or device on the main thread.
- **Recommended primary drain:** the return of `call 0x6325A0` at 0x441822.
  - It runs after the whole logic step including LW logic and Debug vt94, before the IsIconic loop, the pump, the pacer and the streamer.
  - It costs nothing in the heavy case, because logic ends after the target.
  - In the light case it waits until min(target, this iteration's pacer deadline). The target is at most release + 0.6·half = 9.9 ms, which is less than the deadline (16.5 ms); this is the same wait PresentSkip does today (pacer.cpp:339-350).

### Main-thread D3D calls that can overlap a pending Present
- The End_Render tail: 16 SetTexture, up to 16 texture Releases, VB/IB/texture Releases.
- 0x532D6F Releases.
- GC vt90 Releases.
- Logic-side CreateVertexBuffer, EvictManagedResources and (INFERRED) CreateTexture/CreateIndexBuffer/Lock/Unlock/Release.

None of these is order-sensitive with respect to Present. All are known or believed to run inside the DX8 mutex: the End_Render tail is inside drawFrame's bracket, and 0x539290 and 0x532D6F bracket themselves. The bracketing of every logic-side call is INFERRED from the engine design: it has a deferred-release queue, the load thread presents concurrently with main-thread loading, and there are 116 bracket sites.

## 2. Window, focus, pump, deadlock

- **What the pump does on focus loss:** in exclusive fullscreen with DXVK 2.6.2 (game.dat_d3d9.log; dxvk.conf sets only memory and buffer-caching options), WM_ACTIVATEAPP is handled by DXVK's window-proc hook during the pump: it minimises the window. The game then spins in the IsIconic loop at 0x44184C without rendering, until it is restored or until GE vt54 / GL+0x110 ∈ {1,5}. Draining before 0x441827 means nothing is ever pending during the pump, a minimise or a restore.
- **Ways the main thread can block while a helper Present is in flight:**
  - (a) our drain wait;
  - (b) the game mutex in 0x51EEC0 (WaitForSingleObject 20000 ms, no pumping; on timeout it asserts and continues without ownership);
  - (c) DXVK's internal device lock, but only if MULTITHREADED is set.
- **The deadlock condition:** a deadlock needs the helper's Present to SendMessage to the main window synchronously.
  - DXVK's real vkQueuePresent and frame-latency waits already run on DXVK's own threads, and in stock the main thread does not pump during Present. So anything Present waits on other than a same-thread send would already hang stock.
  - The residual risk is only Win32 calls on the Present-calling thread that send messages (SetWindowPos/ShowWindow). They are not expected in DXVK 2.6.2's steady-state Present path (INFERRED; verify in d3d9_swapchain.cpp).
- **Waiting safely:**
  - Drain: loop on MsgWaitForMultipleObjectsEx(1, &evDone, 5 ms, QS_SENDMESSAGE, 0). On WAIT_OBJECT_0+1, call PeekMessageW(&m, 0, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE). This dispatches only inbound sent messages; posted input stays queued for the stock pump. Dispatching there is closer to stock than not, because in stock such a send would have run inline inside Present.
  - Count dispatches. Log and set DISABLED at > 0 dispatches or > 250 ms.
  - For (b), add the MUTEX_WAIT site at 0x51EECB: on the main thread, while the helper is PRESENTING, use the same message-aware wait.
  - A Present that has already started cannot be cancelled; a Present that has not started can always be stolen (see section 5).
- **Deferral pre-checks**, each failing one means presenting synchronously: [0xDC3C64] != 0, !IsIconic, GetForegroundWindow() == hwnd, device not lost, state IDLE.

## 3. Device lost, Reset_Device, mode switches, exit, exceptions

- **Device lost.** The helper only records the HRESULT. The drain runs on the main thread (the window thread; MS requires TestCooperativeLevel, Reset and the final Release there).
  - On DEVICELOST: set g_gapDevLost = 1 and DEFER_OFF_UNTIL_OK. Do not call TCL, Reset or Sleep(200) in the drain or inside logic.
  - The next drawFrame's 0x516C40 (TCL, Reset_Device on DEVICENOTRESET, skip on DEVICELOST) performs the stock recovery one render later, which the existing PRESENT site already accepts ("lost-device detection is one render late"). The only difference from stock is a missing Sleep(200).
  - Other failures: call 0x51EDF0(hr) on the main thread.
  - With DXVK, deviceLossOnFocusLoss defaults to off, so DEVICELOST practically does not occur; native d3d9 (DXVK off) does produce it.
- **Reset_Device 0x522000.** Callers: 0x5225E0, 0x516C40, 0x517B30, 0x5224D0 (via 0x516950 Set_Device_Resolution from 0x442A44/0x442B5A), 0x5249A0 (init), 0x522460. Drain at the existing GAP_RESET cave (call C++ with SAVE_ALL). A Reset with a Present in flight on another thread is fatal.
- **Mode switches.** LeaveSixty is reached from OnPreRender (C0, frame_ctl.cpp:403) and OnEngineReset (RESET hook 0x44181A, frame_ctl.cpp:451). Drain at the top of both. In steady state C0 always finds IDLE; anything else is an anomaly bit. Resets happen on A-renders by design (PLAN §1.8), but drain anyway, because a load starts the load-screen thread, which renders under the same mutex and would otherwise share the back buffer with a still-POSTED B.
- **Game exit.** WW3D::Shutdown 0x517AA0 takes the DX8 mutex, then DX8Wrapper::Shutdown 0x5257A0 releases the device (0x52121B Release, 0x52121F clears [0xDD3474]). Drain at 0x517AA0 entry. A POSTED job must never be presented after this; the helper re-reads the state after acquiring the mutex.
- **Exceptions.** GameEngine::execute wraps the update (0x639ED0 [ebp-4]=3, handlers from 0x639EF0); an unwind skips the 0x441822 drain.
  - The helper finishes a PRESENTING job on its own.
  - A POSTED job is drained by the next safety net it hits (PRESENT_STUB, 0x449D0D, 0x516C40, 0x517B30, 0x522000, 0x517AA0, C0).
  - An unwind through drawFrame can leave the mutex orphan-owned by the main thread. The helper then blocks in 0x51EF50; the main thread's drain steals the job (it may hold the mutex recursively). The helper treats WAIT_ABANDONED as acquired and re-checks the state.
  - Never wait on the helper in DllMain or PROCESS_DETACH: ExitProcess has already killed it.

## 4. (C) presenting from logic checkpoints on the main thread

- **FPU/SSE state.** GameLogic::update itself calls 0x440809 (_fpreset; _controlfp(..|_PC_24, _MCW_PC|_MCW_RC)) (CONFIRMED caller 0x62E4E8; also the CRC 0x625886 and 18 others). The x87 control word (24-bit precision, round-to-nearest) is therefore part of the logic state.
  - Around the checkpoint Present: fxsave/fxrstor of a 512-byte, 16-aligned area (x87 CW/SW/TW, stack, MXCSR, XMM0-7), pushfd/pushad, and GetLastError/SetLastError.
  - Checkpoints only at call boundaries (x87 depth 0, DF = 0).
  - Thread CRT state is unaffected: the game uses MSVCR71.dll and DXVK its own CRT. Logic RNG is [0xDA1CA4], not the CRT.
- **Re-entrancy guards:**
  - only between LogicUpdateWrapper / LwLogicUpdateWrapper entry and exit (a g_inLogic flag), never while g_inClientUpdate;
  - never nested (g_inCheckpointPresent);
  - skip if the main thread owns the DX8 mutex ([0xDD34C8] == tid && [0xDD34CC] > 0, i.e. inside a bracketed D3D sequence such as a VB lock and fill);
  - acquire with 0x51EF50(0) and skip on failure (load or streamer thread busy); release with 0x5208D0.
- **Results.** On DEVICELOST, record and defer to the next render (Reset inside logic is unsafe). Never pump.
- **WndProc.** Same-thread sends from Present run the game's WndProc inline inside logic. In stock they would run inside the render. INFERRED none in steady state. Gate on foreground and not minimised.
- **Measurement.** The Present time spent inside a checkpoint must be subtracted from LogicUpdateWrapper's timing (telemetry.cpp:617-631), or the predictor learns Present blocking as logic time.
- **Hybrid (H).** A helper thread waits on a high-resolution timer for the target and sets a byte g_bDue. Checkpoints test only that byte: no QPC, about 1 ns. All D3D stays on the main thread, so the whole deadlock and race class of (T) disappears.

## 5. State machine of the pending Present (single slot)

The state is one aligned 64-bit word {gen:32, st:32} changed only by CAS (no ABA). The job holds gen, renderId, tPost, target, latest, and the device pointer [0xDD3474] read at post.

| State | Meaning |
|---|---|
| IDLE | No job. |
| POSTED | B rendered and EndScene done; Present not yet issued. |
| PRESENTING | Owned by the helper, which holds the DX8 mutex. |
| MAIN_PRESENTING | Stolen by the main thread or a render thread. |
| DONE | {hr, tStart, tEnd} published with release semantics. |
| DISABLED | Sticky for the session. |

**Post** (main thread, PRESENT_STUB on a 60-mode B-render, when PresentSkip would present and all pre-checks hold):
- write the job, CAS IDLE -> POSTED, signal the helper;
- the stub returns EAX = 0 (S_OK) without calling Present;
- PresentSkip's spacing wait is skipped for deferred jobs.

**Helper:**
1. Wait for the job, then wait on its own high-resolution waitable timer until the target. The pacer's g_timer is not shared.
2. Loop 0x51EF50(2 ms) while st == POSTED (WAIT_ABANDONED counts as acquired).
3. CAS POSTED -> PRESENTING. If the CAS fails, release with 0x5208D0 and go idle.
4. Call the raw device vt+0x44 Present(dev, 0, 0, 0, 0). Never End_Render, never PRESENT_STUB, never PresentSkip/PresentDone (those are main-thread-only, pacer.cpp:300/359).
5. Release the mutex with 0x5208D0, store {hr, t}, set st = DONE, SetEvent.

**Drain(reason)** (main thread unless noted):
- IDLE: nothing to do.
- POSTED:
  - at the primary drain, wait while now < min(target, deadline) with the message-aware wait;
  - then, or immediately at any other reason, CAS POSTED -> MAIN_PRESENTING and Present on the main thread (inside 0x51EEC0/0x5208D0, or within the already held bracket).
- PRESENTING: message-aware wait on evDone.
- DONE: consume on the main thread:
  - RecordPresentSpacing(tStart) and the PresentDone logic (took > 1 ms -> g_presentsSinceBlock = 0);
  - hr handling as in section 3;
  - then IDLE.
- From a non-main render thread (load screen) at 0x517B30/0x516C40: it already holds the mutex, so the job cannot be PRESENTING; steal and present.

**Drain sites:** primary 0x441822. Safety nets: PRESENT_STUB entry (second Present in the same render), 0x449D0D, 0x516C40, 0x517B30, GAP_RESET 0x522000, RESET 0x44181A / OnEngineReset, OnPreRender (anomaly if not IDLE), 0x517AA0. Every safety-net hit is counted in telemetry; in steady state they must all read 0.

Logic-time cost of (T): a bracketed main-thread D3D call made while the helper is PRESENTING waits for the duration of Present. That is normally under 1 ms, and up to one refresh (8.3 ms at 120 Hz) if the FIFO queue is full. Measure it via the DX8 mutex wait time on the main thread.

## 6. MULTITHREADED

Adding D3DCREATE_MULTITHREADED turns any unbracketed concurrent call (a silent data race in DXVK, which has no lock without the flag) into a stall. But it adds an unbreakable deadlock class: a main thread blocked in DXVK's lock cannot pump. Device creation happens before the first C0, so it cannot follow g_m60; it would be a startup-time option and makes 30 mode not byte-exact. Recommendation: off by default; the game mutex is the primary protocol (the load-screen precedent); the flag is an A/B diagnostic option. Readers of [0xDD345C] are only 0x524190 and 0x5241EF (CONFIRMED by refs).

## RISKS

1. **Window messages from the helper's Present (INFERRED).** If DXVK 2.6.2's Present (or the native d3d9 runtime) sends a synchronous window message from the calling thread, the helper waits for the main thread.
   - The main thread can be stuck where it cannot pump: the game mutex in 0x51EEC0 (20 s, then an assert), or DXVK's lock if MULTITHREADED is set.
   - Mitigations: the MUTEX_WAIT site, the message-aware drain, watchdogs that disable (T), and the steady-state pre-checks.
   - To verify: read d3d9_swapchain.cpp Present in v2.6.2, and log dispatched sent messages at runtime.

2. **Unbracketed D3D calls during logic (INFERRED).** Statically, only 36 of the 151 device-using functions are provably bracketed, or reached only from bracketed callers, by direct calls; 115 are reached through virtual calls. An unbracketed main-thread call during the helper's Present is a data race in DXVK without MULTITHREADED.
   - Evidence that the engine brackets these calls: the load-screen thread presents concurrently with main-thread loading under the same mutex, VB/IB creation and lock and the deferred-release queue bracket themselves, and there are 116 acquire sites.
   - A debug-only device-vtable shim could count main-thread device calls made while the helper is PRESENTING and the DX8 owner is not the main thread.

3. **Extra logic time.** A bracketed main-thread D3D call during logic stalls for as long as the Present runs. That is normally under 1 ms, but up to one refresh (8.3 ms at 120 Hz) when the FIFO queue is full. Measure it.

4. **Second Present in the same render.** The frozen-time redraw loop (0x44BC20 -> 0x44B9A8) and the tile/capture paths can run a full second drawFrame and Present in the same render. Without the 0x449D0D/0x517B30 drains, a POSTED B would show a partly drawn next frame.

5. **Unwinding.** An exception unwind skips the primary drain, and a drawFrame unwind can orphan the DX8 mutex. Steal semantics, the safety-net drains, and treating WAIT_ABANDONED as acquired cover this. The helper must never use 0x51EEC0 (20 s wait plus assert); it uses 0x51EF50 with a timeout.

6. **Pacer state is main-thread only.** PresentSkip/PresentDone and the pacer globals (g_presentCall, g_lastPresent, g_presentsSinceBlock) are main-thread-only and unsynchronised. The helper must only fill a job record that the main thread consumes at drain. Otherwise the pacer and predictor learn wrong Present timings (the existing pitfall of Present blocking being counted as render or logic time).

7. **(C) checkpoints.**
   - A missing fxsave/fxrstor, or a checkpoint that is not at a call boundary (x87 depth not 0), changes logic floats, because GameLogic sets PC24 via 0x440809.
   - Presenting inside a bracketed D3D sequence must be skipped.
   - Same-thread WndProc sends would run inside logic.
   - DEVICELOST handling must be deferred, never Reset inside logic.

8. **MULTITHREADED option.** It changes device creation for 30 mode as well (not byte-exact) and cannot be switched at runtime.

9. **Load-screen thread.** It renders under the same mutex. A job still POSTED when a load starts would present its back buffer. Covered by the drain in OnEngineReset and at 0x517B30/0x516C40, which run on any thread.

## RECOMMENDATION

1. **Use the thread for timing first: (H) = (C) plus a timer thread.**
   - A helper thread sleeps on a high-resolution timer until the B target and sets the byte g_bDue. Logic checkpoints test only that byte and present on the main thread.
   - Around each checkpoint Present: fxsave/fxrstor, flags/GPRs, LastError, 0x51EF50(0)/0x5208D0, and a skip when the main thread owns the DX8 mutex.
   - Any remaining job is drained at STEP_DRAIN 0x441822.
   - This takes the timing benefit of a second thread without cross-thread D3D or any new deadlock class.

2. **If true off-thread Present (T) is wanted, implement exactly:**
   - the section 5 state machine (64-bit {gen, state} CAS; steal for POSTED);
   - the helper calls the raw vt+0x44 only, under 0x51EF50/0x5208D0, with its own timer;
   - PRESENT_STUB returns S_OK for a deferred job;
   - the primary drain STEP_DRAIN 0x441822 waits until min(target, deadline) and then forces the Present;
   - safety-net drains at PRESENT_STUB entry, 0x449D0D, 0x516C40, 0x517B30, GAP_RESET 0x522000, OnEngineReset/OnPreRender and 0x517AA0;
   - the MUTEX_WAIT deadlock breaker at 0x51EECB;
   - message-aware drain waits: MsgWaitForMultipleObjectsEx with QS_SENDMESSAGE, then PeekMessage with PM_NOREMOVE | PM_QS_SENDMESSAGE;
   - DEVICELOST handed to the next render's 0x516C40;
   - all pacer bookkeeping done at drain on the main thread;
   - a sticky DISABLED state on any watchdog (a Present over 250 ms, any sent-message dispatch, a mutex timeout);
   - deferral only for a paced, normal 60-mode B-render with a foreground, non-minimised window and an IDLE state.
   - Keep MULTITHREADED off (the optional site only for A/B diagnosis).

3. **Before relying on (T):**
   - check the DXVK 2.6.2 Present source for Win32 calls that send window messages;
   - add telemetry for the drain-site hit counters (every safety net must read 0 in steady state), the main-thread DX8-mutex wait time during logic, and the helper Present duration;
   - run the determinism harness (Telemetry=2) for both (C) and (T) to confirm the logic traces are identical.

## SITE {
 "id": "STEP_DRAIN",
 "address": "0x441822",
 "length": 5,
 "original_hex": "e8 79 0d 1f 00",
 "original_asm": "0x441822: call 0x6325A0 (GameEngine::update) in Win32GameEngine::update 0x44181F",
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:STEP_DRAIN_STUB>",
 "stub": "STEP_DRAIN_STUB: call dword ptr [T_6325A0] (ECX=ESI=engine as at the original call); then if g_ppState!=IDLE: call AotR60_PendingPresentDrain(DRAIN_STEP_END) (cdecl, main thread); ret. 30 mode: g_ppState is always IDLE, so only the call (stock).",
 "live_after": "At 0x441827: ESI=engine, EBX/EDI/EBP/ESP as stock (callee-saved), EAX overwritten by mov eax,[0xDC3C64], ECX/EDX dead, EFLAGS dead (test eax,eax), x87 empty, XMM dead.",
 "branch_into_span_check": "listing.asm operand scan for 0x441823..0x441826: no hit; image pointer scan and linear sweep still to run in verify_sites.py. The entry 0x44181F is reached only via vtable slot 0xBD8508.",
 "purpose": "Primary drain: the pending B Present is complete before the IsIconic loop, the message pump 0x4418C2, the pacer, the asset streamer and the next render."
}

## SITE {
 "id": "DRAW_DRAIN",
 "address": "0x449D0D",
 "length": 5,
 "original_hex": "e8 ae 51 0d 00",
 "original_asm": "0x449D0D: call 0x51EEC0 (DX8 mutex acquire) at drawFrame entry",
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:DRAW_DRAIN_STUB>",
 "stub": "DRAW_DRAIN_STUB: if main thread && g_ppState!=IDLE: SAVE regs; call AotR60_PendingPresentDrain(DRAIN_DRAW) (message-aware wait possible because the mutex is not yet held); restore; then jmp dword ptr [T_51EEC0] (tail call; returns to 0x449D12).",
 "live_after": "At 0x449D12: ESI=this (W3DDisplay), EDI/EBP/ESP as stock, EBX then zeroed by xor, EAX/ECX/EDX/EFLAGS dead, x87 empty.",
 "branch_into_span_check": "listing.asm operand scan for 0x449D0E..0x449D11: no hit; pointer scan still to run.",
 "purpose": "Safety net for a second drawFrame in the same render (frozen-time redraw loop 0x44BC20 -> 0x44B9A8, tile loop 0x44BAC7) and any abnormal path: B must be presented before the next Clear/BeginScene."
}

## SITE {
 "id": "TCL_NET",
 "address": "0x516C40",
 "length": 5,
 "original_hex": "a1 74 34 dd 00",
 "original_asm": "0x516C40: mov eax,[0xDD3474] (entry of the frame-start function that calls TestCooperativeLevel)",
 "kind": "func_detour",
 "replacement_hex": "e9 <rel32:TCL_NET_CAVE>",
 "stub": "TCL_NET_CAVE: pushfd/pushad; if g_ppState!=IDLE: call AotR60_PendingPresentDrain(DRAIN_TCL) (any thread; the caller holds the DX8 mutex, so the job can only be POSTED/DONE: steal or consume, no wait); popad/popfd; mov eax,[0xDD3474]; jmp 0x516C45.",
 "live_after": "At 0x516C45: EAX=device pointer, all other GPRs/ESP as at entry, EFLAGS dead (sub esp,0xC0), x87 empty.",
 "branch_into_span_check": "Only callers 0x447645 and 0x449D17 (span start); listing operand scan for 0x516C41..0x516C44: no hit.",
 "purpose": "Guarantees API order (Present before TestCooperativeLevel) on the draw paths 0x449CF8 and 0x4475E4."
}

## SITE {
 "id": "BEGIN_RENDER_NET",
 "address": "0x517B30",
 "length": 10,
 "original_hex": "83 ec 28 80 3d 14 1e dd 00 00",
 "original_asm": "0x517B30: sub esp,0x28 ; 0x517B33: cmp byte ptr [0xDD1E14],0",
 "kind": "func_detour",
 "replacement_hex": "e9 <rel32:BEGIN_RENDER_CAVE> 90 90 90 90 90",
 "stub": "BEGIN_RENDER_CAVE: pushfd/pushad; if g_ppState!=IDLE: call AotR60_PendingPresentDrain(DRAIN_BEGIN_RENDER) (any thread; steal or consume); popad/popfd; sub esp,0x28; cmp byte ptr [0xDD1E14],0 (must be the last flag-setting instruction); jmp 0x517B3A.",
 "live_after": "At 0x517B3A: ZF from the cmp (je 0x517C50), ESP = entry-0x28, all GPRs as at entry, x87 empty.",
 "branch_into_span_check": "Callers 0x4437B3, 0x44765F, 0x449EA5 and those in 0x47D5C9/0x47F1AC target the span start; listing operand scan for 0x517B31..0x517B39: no hit.",
 "purpose": "Present before any Clear/BeginScene (WW3D::Begin_Render) on every draw path, including RTT passes and the 0x4436DD/0x4475E4 paths."
}

## SITE {
 "id": "SHUTDOWN_DRAIN",
 "address": "0x517AA0",
 "length": 7,
 "original_hex": "6a ff 68 b8 8d b7 00",
 "original_asm": "0x517AA0: push -1 ; 0x517AA2: push 0xB78DB8 (WW3D::Shutdown SEH prologue)",
 "kind": "func_detour",
 "replacement_hex": "e9 <rel32:SHUTDOWN_CAVE> 90 90",
 "stub": "SHUTDOWN_CAVE: pushfd/pushad; call AotR60_PendingPresentShutdown() (drain, then stop the helper's acceptance: state DISABLED); popad/popfd; push -1; push 0xB78DB8; jmp 0x517AA7.",
 "live_after": "At 0x517AA7: ECX=param_1 (fastcall, passed to 0x5173A0), EDX/other GPRs as at entry, two pushes on the stack, EFLAGS dead, x87 empty.",
 "branch_into_span_check": "listing operand scan for 0x517AA1..0x517AA6: no hit.",
 "purpose": "No Present (helper or posted) after DX8Wrapper::Shutdown 0x5257A0 releases the device; exit after an exception unwound past STEP_DRAIN."
}

## SITE {
 "id": "MUTEX_WAIT",
 "address": "0x51EECB",
 "length": 6,
 "original_hex": "ff 15 34 02 bd 00",
 "original_asm": "0x51EECB: call dword ptr [0xBD0234] (WaitForSingleObject(mutex [0xDD1FD8], 20000)) in 0x51EEC0",
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:MUTEX_WAIT_STUB> 90",
 "stub": "MUTEX_WAIT_STUB (stdcall HANDLE,DWORD -> ret 8): if main thread && g_ppState==PRESENTING: loop MsgWaitForMultipleObjectsEx(1,&h,min(left,5),QS_SENDMESSAGE,0); on +1 PeekMessageW(PM_NOREMOVE|PM_QS_SENDMESSAGE) and count (anomaly, disables (T)); return the WFSO-equivalent code (0, 0x80 abandoned, 0x102 timeout). Otherwise jmp [0xBD0234].",
 "live_after": "At 0x51EED1: EAX=wait result (cmp eax,0x102), ESP balanced (8 bytes popped), EBX/ESI/EDI/EBP preserved, ECX/EDX dead, EFLAGS dead.",
 "branch_into_span_check": "listing operand scan for 0x51EECC..0x51EED0: no hit; the function is entered only at 0x51EEC0.",
 "purpose": "Deadlock breaker when a bracketed main-thread D3D call during logic waits for the helper's in-flight Present and that Present needs a sent message processed."
}

## SITE {
 "id": "MT_FLAG_OPTIONAL",
 "address": "0x5240DD",
 "length": 7,
 "original_hex": "1b c0 24 e0 83 c0 40",
 "original_asm": "0x5240DD: sbb eax,eax ; 0x5240DF: and al,0xE0 ; 0x5240E1: add eax,0x40 (behaviour flags 0x40 HWVP / 0x20 SWVP)",
 "kind": "operand_redirect",
 "replacement_hex": "1b c0 24 e0 83 c0 44",
 "stub": "None (in-place immediate change 0x40 -> 0x44 adds D3DCREATE_MULTITHREADED). Startup-time only (the device is created before the first C0), so it cannot follow g_m60.",
 "live_after": "At 0x5240E4: EAX = 0x44 or 0x24 instead of 0x40/0x20; test al,0x40 at 0x5240E4 unchanged; [0xDD345C] read only at 0x524190 and 0x5241EF (CreateDevice pushes). CF from the cmp at 0x5240D2 is consumed by the first span instruction (unchanged).",
 "branch_into_span_check": "listing operand scan for 0x5240DE..0x5240E3: no hit.",
 "purpose": "OPTIONAL diagnostic for (T): DXVK serialises device calls itself. Off by default because 30 mode is then not byte-exact and a main thread blocked in DXVK's lock cannot pump."
}

## VERIFIER OVERALL
The sync analysis largely holds up. All seven site byte strings match game.dat. No branch or pointer lands inside any of the spans: a raw rel8/rel32/abs32 sweep found only two false positives, at 0x516C3D and 0x5FB6C8. The loop order (logic, then STEP_DRAIN 0x441822, then IsIconic/pump, pacer 0x63A196, streamer 0x6325B0, C0) is CONFIRMED. The lock design is CONFIRMED: Win32 mutex [0xDD1FD8] created at 0x52506B, owner and count kept in [0xDD34C8]/[0xDD34CC]. The DXVK 2.6.2 facts mostly hold: no device lock without MULTITHREADED, and deviceLossOnFocusLoss is off by default. The pacer numbers hold too: 9.9 ms and 16.5 ms. I reran the mtsim selftest (30/30 cases equal to hsim 'old'). Its sequences agree with a roughly even split of the hold: P3, tick 105, Tm-orc gives 26.9/27.8 ms. My own arithmetic for L=31 ms with the calibrated a_pre 9 and b_pre 5.5 gives 13/40.6 old and about 26.8/26.8 deferred.

Errors and gaps found:
1. An exception unwind does NOT orphan the DX8 mutex. drawFrame's C++ EH state 0 (FuncInfo 0xD17F80, action 0xB71487 -> 0x4428A6 -> jmp 0x5208D0) releases it.
2. "DXVK's frame-latency waits run on DXVK's own threads" is wrong. SyncFrameLatency (d3d9_swapchain.cpp:1126) and acquireNextImage (dxvk_presenter.cpp:77-134) run on the thread that calls Present. That includes the wait for the previous present, vkAcquireNextImageKHR, and recreateSwapChain/vkCreateSwapchainKHR. Only vkQueuePresentKHR runs on the submit thread (dxvk_queue.cpp:172). So under (T), swapchain acquire and re-creation move off the window thread. A window message sent from driver or DXGI code there is a new cross-thread case. The argument "it would already hang stock" does not cover it.
3. A message pump is reachable INSIDE GameLogic::update. 0x62E4E8 -> 0x779A3D -> ... -> 0x84EEBA calls serviceWindowsOS at 0x84EEFA, and 0x98B12C does the same. A drain at the pump entry (0x441A7D) is missing.
4. Dispatching inbound sent messages (PM_QS_SENDMESSAGE) in the drain and in MUTEX_WAIT goes beyond stock. Stock never dispatches during Present, because DXVK's waits do not pump. So WM_ACTIVATEAPP and similar messages sent on alt-tab would run the game WndProc mid-logic or before the pump.
5. The 151-function / 36-"bracketed" figure undercounts the risk. It sees only functions that read [0xDD3474], so it misses calls through resource objects (Lock/Release on textures, VB, IB and surfaces), D3DX calls and ID3DXEffect calls. Its "bracketed" test is containment of an acquire call, not proof that the acquire comes before the device call.
6. [0xDD343C] does have a writer: 0x7BA01B stores atoi() of an argument. No reference to that function was found, so it is effectively dead.
7. [0xDD34C4] is also read by 0x522806 (a second Present routine at 0x522865, reachable only through the thunk 0x516DF0, which has no callers) and by the getter 0x51CC40 (no references). So its use is still effectively stats-only.
8. The ~116 acquire sites are really 123 call sites of 0x51EEC0 plus 1 of 0x51EF50, in 117 functions. This changes nothing.

On recommendations, I agree that (H)/(C) carries less risk. (T) is implementable only with the extra drains and gates listed under "missed". I would not make the claim "MULTITHREADED off by default" final. With DXVK, the flag turns the unproven race surface into short spinlock waits. Its deadlock precondition is the same one (T) already carries: Present sending a message to the main thread.

- [confirmed] The site bytes are as stated (STEP_DRAIN e8 79 0d 1f 00 @0x441822; DRAW_DRAIN e8 ae 51 0d 00 @0x449D0D; TCL_NET a1 74 34 dd 00 @0x516C40; BEGIN_RENDER_NET 83 ec 28 80 3d 14 1e dd 00 00 @0x517B30; SHUTDOWN 6a ff 68 b8 8d b7 00 @0x517AA0; MUTEX_WAIT ff 15 34 02 bd 00 @0x51EECB; MT_FLAG 1b c0 24 e0 83 c0 40 @0x5240DD), and no branch goes into a span interior.
  All bytes were read from a copy of game.dat (mt/sync_verify/span_check.py). An unaligned rel8/rel32/jcc/abs32 sweep gave only 2 hits. 0x516C3D->0x516C41 lies inside 'jmp 0x53e660' at 0x516C3B. 0x5FB6C8->0x441824 lies inside 'call 0xa3cef0' at 0x5FB6C7. Both are false positives. None of the spans overlaps the existing sites RESET 0x44181A, GAP_RESET 0x522000 or PRESENT 0x522644. Direct callers: 0x516C40 from 0x447645/0x449D17; 0x517B30 from 0x4437B3, 0x44765F, 0x449EA5, 0x47D6F6, 0x47D781, 0x47F218; 0x517AA0 from 0x402D27, 0x4466B4, 0x4499F5.

- [confirmed] Loop order: 0x441822 call GameEngine::update -> 0x441827 IsIconic loop -> tail jmp vt5C serviceWindowsOS at 0x4418C2 -> execute loop -> pacer 0x63A196; the streamer vt28 at 0x6325B0 runs before C0 0x6325CF.
  Disassembly of 0x44181F..0x4418C5 and 0x6325A0..0x6325D5. GameEngine::execute calls update via [eax+0x28] at 0x639EE1. The only direct caller of 0x6325A0 is 0x44181F, which is reached through vtable slot 0xBD8508.

- [uncertain] The device is created with HWVP|PUREDEVICE only (no MULTITHREADED, no FPU_PRESERVE) and [0xDD343C] is never written.
  The flags part is CONFIRMED. 0x5240DD..0x5240F6 gives 0x40/0x20, plus 0x10 if caps bit 20 is set. FPU_PRESERVE (or al,2 at 0x52410B) is used only if [0xDD343C]!=0. [0xDD345C] is read only at 0x524190 and 0x5241EF (CreateDevice). 'Never written' is technically REFUTED: 0x7BA01B does mov [0xDD343C],eax after atoi(argv) (import 0xBD0628). No call, jmp or pointer reference to its function at 0x7BA006 was found, so it is INFERRED dead and the flag stays 0.

- [confirmed] The engine serialises D3D work with its own mutex [0xDD1FD8], acquired by 0x51EEC0 (20 s) or 0x51EF50 (timeout) and released by 0x5208D0; there are 116 acquire sites.
  CreateMutexA at 0x52506B, WaitForSingleObject(…,0x4E20) at 0x51EECB, ReleaseMutex at 0x520920. Owner and count are kept in [0xDD34C8]/[0xDD34CC] under CS 0xDD1F80. 0x51EF50 is cdecl and returns al=0 only on WAIT_TIMEOUT, so WAIT_ABANDONED and WAIT_FAILED count as acquired. The count is really 123 call sites of 0x51EEC0 plus 1 of 0x51EF50, in 117 functions (callgraph), and 129 of 0x5208D0. New detail: on the 20 s timeout path, 0x51EEC0 still writes owner=tid and count++, so the bookkeeping is corrupted and the later ReleaseMutex fails.

- [confirmed] The load-screen thread already draws and presents from a second thread under the mutex (0x65C19B).
  CreateThread(LAB_0065ce28) appears in the decompile. The thread proc 0x65CE28 calls 0x65C19B(1), which does 0x51EEC0, then vt+0x18C(3), then 0x5208D0. That vt+0x18C is the draw-and-Present path is INFERRED, not traced.

- [confirmed] drawFrame holds the mutex from 0x449D0D to 0x44A271, and the B Present at 0x522650 is inside that window.
  0x449D0D call 0x51EEC0 -> ... 0x44A228 call 0x516DA0 (End_Render) -> 0x516DBB call 0x5225E0 (End_Scene) -> 0x522650 Present -> ... 0x44A271 call 0x5208D0.

- [refuted] An unwind through drawFrame can leave the mutex orphan-owned by the main thread.
  drawFrame sets EH state 0 right after the acquire (0x449D14 mov [ebp-4],ebx). FuncInfo 0xD17F80 (magic 0x19930520) has state 0 action 0xB71487 -> jmp 0x4428A6 -> jmp 0x5208D0, so C++ EH unwinding releases the mutex. Treating WAIT_ABANDONED as acquired is still prudent, but this scenario does not come from drawFrame.

- [confirmed] The HRESULT returned by the stub is used only for statistics ([0xDD1F38] write-only; [0xDD34C4] read only for stats), so returning S_OK for a deferred Present is harmless.
  [0xDD1F38] has 6 references, all byte writes (0x522665/0x52266D in End_Scene, plus 0x522842, 0x522873, 0x52287B, 0x52288D). [0xDD34C4] has two more readers than stated: 0x522805 in a second Present routine 0x5227F0 (TCL plus vt+0x44 at 0x522865, a buffer-flip loop) and a getter at 0x51CC40. 0x5227F0 is reached only through the thunk 0x516DF0, and neither the thunk nor 0x51CC40 has any call, jmp or pointer reference. The conclusion holds: the deferred Present still happens, so the count stays consistent.

- [confirmed] GameLogic::update resets the FPU itself via 0x440809 (_fpreset + _controlfp PC24/RN), so (C) must fxsave/fxrstor.
  0x440809 calls _fpreset [0xBD0590], then _statusfp [0xBD0594], then and ~0x10300, or 0x20000, then _controlfp(x,0x30300) [0xBD0598]. The result is PC_24 and RN. 0x62E4E8 and 0x625886 are among the 20 callers. DXVK SetupFPU only runs on the thread that created the device (d3d9_device.cpp:91-92).

- [confirmed] DXVK: no device lock without D3DCREATE_MULTITHREADED.
  d3d9_device.cpp:55 sets m_multithread(BehaviorFlags & MULTITHREADED). d3d9_multithread.h AcquireLock returns an empty lock unless the device is protected. Present's LockDevice (d3d9_swapchain.cpp:113) is a no-op. The lock is a RecursiveSpinlock, so a blocked thread spins and cannot pump.

- [refuted] DXVK's vkQueuePresent and frame-latency waits already run on DXVK's own threads, so the only residual hang risk is a same-thread SendMessage from Present.
  vkQueuePresentKHR does run on the submission thread (dxvk_device.cpp:341, dxvk_queue.cpp:172). But SyncFrameLatency (d3d9_swapchain.cpp:1126-1128) and Presenter::acquireNextImage run on the thread that calls Present. acquireNextImage covers the m_surfaceCond wait, updateSwapChain, vkAcquireNextImageKHR, and recreateSwapChain with vkCreateSwapchainKHR on OUT_OF_DATE (dxvk_presenter.cpp:77-134). Under (T) these move from the window thread to the helper. A message sent by the driver or DXGI during swapchain re-creation (alt-tab, mode or HDR change) becomes cross-thread. The steady-state DXVK code itself calls only GetClientRect (wsi_window_win32.cpp:109), and UpdateWindowedRefreshRate returns early in fullscreen, so 'no sends in steady state' is CONFIRMED for the DXVK layer only. Note that the local DXVK source copy is assumed to be the v2.6.2 tag.

- [confirmed] WM_ACTIVATEAPP is handled by DXVK's window-proc hook during the pump (minimise), and DEVICELOST practically never occurs with DXVK.
  d3d9_window.cpp:66-91: on deactivate it calls ShowWindow(SW_MINIMIZE), on activate SetWindowPos, then NotifyWindowActivated. NotifyWindowActivated returns early unless deviceLossOnFocusLoss is set, and that option defaults to false (d3d9_options.cpp:71). Present returns DEVICELOST only if IsDeviceLost().

- [refuted] Draining at 0x441822 means nothing is ever pending during a message pump.
  serviceWindowsOS (vt5C, 0x441A7D) is also reached from inside the logic step. With direct calls only: 0x62E4E8 -> 0x779A3D -> 0x6BE6A6 -> 0x6BE467 -> 0x6B95BB -> 0x903660 -> 0x84F08E -> 0x84EF2C -> 0x84EEBA, which calls [GE]+0x5C at 0x84EEFA. 0x98B12C, reached via 0x84F288/0x84FD2E, also calls it at 0x98B16C. Script: mt/a reachability check. These are rare (progress/save-type paths), but a pump there would run the WndProc and DXVK hook while the helper is PRESENTING. Fix: add a main-thread drain at the serviceWindowsOS entry 0x441A7D ('b8 56 11 b7 00' mov eax,0xB71156, 5 bytes; entered via vtable 0xBD853C and the tail jmp at 0x4418C2; no interior reference apart from the false-positive pointer at 0x5FB46D).

- [refuted] The message-aware drain (MsgWaitForMultipleObjectsEx QS_SENDMESSAGE plus PeekMessage PM_NOREMOVE|PM_QS_SENDMESSAGE) is 'closer to stock than not'.
  The API semantics are right: only sent messages are dispatched and posted input stays queued. But in stock the main thread never dispatches during Present, because DXVK waits on fences and condition variables, which do not pump. The drain would dispatch cross-thread sent messages such as WM_ACTIVATEAPP, WM_ACTIVATE and WM_KILLFOCUS from alt-tab, which are not sends caused by Present itself. Via MUTEX_WAIT that happens in the middle of GameLogic::update, a WndProc re-entrancy that stock never has. Safer: wait without dispatching first (for example 50-100 ms), then dispatch and set DISABLED only as a deadlock breaker.

- [confirmed] Logic reaches CreateVertexBuffer 0x539290 (self-bracketed) and EvictManagedResources 0x51DFB0 by direct calls via 0x779A3D->0x8CDC11->0x5F530A->0x5FC2FE->0x5FBE28->0x7B0F90->0x53A6E0->0x539290.
  Rerun of sync/reach.py gives the same path. 0x539290 acquires at 0x5392AE and releases at 0x539385. 0x51DFB0 is called at 0x539344, inside the bracket. Only 7 DX8-mutex acquirers are reachable from 0x62E4E8 by direct calls (mt/sync_verify/reach_mutex.py), including the deferred-release flush 0x532D6F via 0x62F397. The logic-side queueing of releases supports the 'engine brackets' theory, but it does not prove it.

- [uncertain] 115 of 151 device-using functions are only virtually reachable and 36 are provably bracketed, so unbracketed logic-side D3D is INFERRED.
  The counts reproduce (bracket.txt: V 115, B 36). The method understates the problem in two ways. First, 'B' means the function or all of its direct callers contain an acquire call somewhere, not that the acquire comes before the device call. Second, the set holds only functions that reference [0xDD3474]. Calls through resource interfaces (IDirect3DTexture9/VB/IB/Surface Lock, LockRect, Release), d3dx9_27 (D3DXCreateTexture*, D3DXLoadSurface*, D3DXCreateEffect, which take the device as an argument) and ID3DXEffect are not counted. A concurrent main-thread call of that kind collides with the helper's EndFrame/Flush/EmitCs/m_backBuffers rotation in DXVK (d3d9_swapchain.cpp:824-941). This is a data race, not just an ordering issue.

- [confirmed] Light case: the drain waits until min(target, deadline); the target is at most release+0.6*half = 9.9 ms, below the deadline of 16.5 ms; this is the same wait PresentSkip does today (pacer.cpp:339-350).
  StockFrameMs = trunc(1000/30) = 33, half = 16.5 (pacer.cpp:193-200), latest = release + half*6/10 = 9.9 ms (pacer.cpp:341). For the 'mid' rule the bound is not exactly 9.9: mid = (A_k + D2 + a_pre)/2 is about D1+9.0 with a_pre=9, and above 9.9 when a_pre > ~10.8. It is still below the deadline D1+16.5, so the min() cap is what matters. Cited lines are correct: OnMainThread checks at pacer.cpp:300 and 359, logic timing in telemetry.cpp around 622-637.

- [confirmed] Hold split for L=31 ms: about 13.5/40 -> ~25/25 ms.
  With calibrated Work (a_pre 9, b_pre 5.5, b_post 0.3, overhead 0.3) and owed>0 (no spacing wait), old gives A->B 13 ms and B->A 40.6 ms. Deferred to the midpoint gives 26.8/26.8 ms, slightly worse than 25/25 before 120 Hz quantisation. The mtsim selftest passed (old == hsim, 30 cases). out_seqs.txt, P3 ai tick 105: old B4 16.3 / A5 42.6 vs Tm-orc B4 26.9 / A5 27.8.

- [uncertain] Logic-time cost of (T): a bracketed main-thread D3D call waits for the helper's Present, normally <1 ms, at most one refresh (8.3 ms at 120 Hz).
  Not measured; the aotr60.log Present counters contain no durations. Present blocking happens in SyncFrameLatency and acquire on the calling thread, while the helper holds the DX8 mutex. On a 60 Hz FIFO display, or with a full queue, the wait can reach 16.7 ms or more. Defer only while g_presentsSinceBlock>=2 or in IMMEDIATE mode.

- [confirmed] The asset streamer's texture paths and the 0x532D6F deferred-release queue take the DX8 mutex.
  0x532D6F: if the queue [0xDD83E8..EC] is non-empty, it acquires 0x51EEC0, calls vt+8 on each entry under the lock object 0xDD83C4, then releases 0x5208D0. Callers include GameLogic 0x627C1F…0x6311ED, 0x64849E (GameClient), 0x47D5C9 and 0x522000. The streamer paths 0xA33CA0/0xA35980/0xA37320 were not re-verified.

- [confirmed] MT_FLAG_OPTIONAL in-place change 0x40->0x44 at 0x5240E3 is correct and only startup-time.
  sbb/and al,0xE0/add eax,imm gives 0x44/0x24. test al,0x40 at 0x5240E4, or al,0x10 and or al,2 are unaffected. CF comes from the cmp at 0x5240D2. CreateDevice is reached only from 0x524070 <- 0x5249A0 (init; a later Set_Render_Device takes the Reset_Device path). The judgement 'off by default' is debatable: under DXVK its deadlock precondition (Present needs the main thread to pump) is the same one (T) already has, and it removes the unproven-race class.

- [confirmed] Exit: WW3D::Shutdown 0x517AA0 is the only path to DX8Wrapper::Shutdown 0x5257A0, which releases the device (0x52121F clears [0xDD3474]).
  callers(0x5257A0) = {0x517AA0}. The only writes to [0xDD3474] are at 0x52121F (in 0x521140, called by 0x5257A0) and 0x5250E4 (in 0x524FD0, called by 0x517A00 init).

## VERIFIER MISSED
Missing for a safe implementation:
1. **Drain before the in-logic pump.** Add PUMP_DRAIN at serviceWindowsOS 0x441A7D ('b8 56 11 b7 00', func_detour, resume 0x441A82; EAX is overwritten by the displaced mov). The pump is reachable from GameLogic::update via 0x84EEBA/0x98B12C.
2. **Start the helper only inside logic.** The helper may issue Present only while the main thread is inside LogicUpdateWrapper (a g_inLogic handshake), and the main thread CAS-steals at logic exit. This keeps the B-render tail out of the overlap: End_Render tail, GC vt90 drawable deletes, 0x7128C3, snow, OnPostRender. Under the T0 rule the target can fall before logic starts.
3. **More deferral pre-checks.** GD+0xEA0 == 0 and GD+0xEA4 == 0, because the movie-capture/tile loop and vt14C frame save (0x44BC54) read the back buffer after End_Scene. Also defer only when the FIFO queue has room (g_presentsSinceBlock >= 2, or IMMEDIATE mode), so the helper never holds the DX8 mutex across a blocking Present.
4. **No message dispatch by default.** Waits use plain WaitForSingleObject with escalation. Dispatching sent messages in MUTEX_WAIT runs the WndProc mid-logic. If dispatch is ever needed, log it as a determinism anomaly, not only as a liveness event.
5. **Enable (T) only under DXVK.** Detect it via rotwk\d3d9.dll. Native d3d9/DXGI in exclusive fullscreen is documented to send window messages from Present and fullscreen transitions, so use (C) there. Also handle the case where DXVK's acquireNextImage or recreateSwapChain runs on the helper: no deferral for N frames after an out-of-date or recreate log line, a focus change or a mode change.
6. **Wider debug shim.** The vtable shim must cover IDirect3DDevice9 plus the resource interfaces (Texture, Surface, VertexBuffer, IndexBuffer, CubeTexture, VolumeTexture), ID3DXEffect, and the d3dx9_27 imports that take the device. Each call checks thread == main && state == PRESENTING && DX8 owner != main. The [0xDD3474]-based count misses all of these.
7. **Helper thread setup.** Priority ABOVE_NORMAL or higher. Its own CREATE_WAITABLE_TIMER_HIGH_RESOLUTION timer. The game sets no affinity mask; no SetProcessAffinityMask import was found. Wake latency should be logged.
8. **Drain deadline.** The drain must compute the upcoming pacer deadline exactly: g_deadline plus the next g_interval, which alternates by one tick when StockFrameMs is odd. It must reproduce PresentSkip's conditions: paced, g_owed == 0, late-skip, and the M>1 forced skip.
9. **Checkpoint granularity for (C)/(H).** No checkpoint sites are given. The worst case is the longest call without a checkpoint (a single A* search or AIPlayer::update). mtsim assumes uniform 1-4 ms checkpoints, which is unproven.
10. **Mutex-timeout handling.** A 20 s timeout in 0x51EEC0 corrupts the [0xDD34C8]/[0xDD34CC] bookkeeping (owner written without the mutex being held). The DISABLED path should also log the mutex owner and stop using 0x51EFA0 ownership checks afterwards.
11. **Lock-order check.** Confirm that no main-thread logic path holds another game lock (streamer CS, audio) while a thread that needs that lock is waiting for the DX8 mutex the helper holds.
12. **Determinism and stress gate.** Run the Telemetra=2 trace comparison for (T) and (C), plus an alt-tab / minimise / save-game / load stress run with the safety-net counters, before enabling either.

Scripts are in <analysis workspace>/hitch\mt\sync_verify\: span_check.py, calltargets.py, reach_pump.py, reach_mutex.py.