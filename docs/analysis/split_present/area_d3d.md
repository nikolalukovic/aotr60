# AREA d3d

## SUMMARY

From the D3D side, (T) is feasible on both DXVK 2.6.2 and native d3d9, but only if the device is created with D3DCREATE_MULTITHREADED. It isn't today: the behaviour flags are 0x40 (HWVP) | 0x10 (PUREDEVICE) on DXVK. FPU_PRESERVE is never set either, because its only writer, the handler at 0x7BA006, is never referenced. The game has exactly one CreateDevice function, 0x524070, which makes two calls (0x5241B6 and the retry at 0x524222). Both read [0xDD345C], so one 5-byte jmp_detour at 0x524112 (a1 70 34 dd 00) that ORs 4 into [0xDD345C] covers both. Bytes and branch targets are verified, and nothing reads the flags back (no GetCreationParameters call). In DXVK 2.6.2 the flag switches on a device-wide recursive spinlock, which is a no-op without the flag. No dxvk.conf option forces or disables it, and none appears in the binary. Present holds that lock for its whole duration, including acquire and the frame-latency wait (3 frames). Present uses only GetClientRect: no SendMessage, SetWindowPos or ShowWindow. vkQueuePresentKHR already runs on DXVK's own dxvk-submit thread. Under default options Present never returns DEVICELOST. Native d3d9 allows Present from any thread; only CreateDevice, Reset, TestCooperativeLevel and the final Release are tied to the window thread. The stock game already presents from a non-window thread: the load-screen worker 0x65CE28 -> 0x4475E4 -> End_Render 0x516DA0 -> 0x522644, under the game's own DX mutex [0xDD1FD8]. A W3DMouse cursor thread also calls ShowCursor, SetCursorPosition and SetCursorProperties without that mutex. The MT flag serializes both of these and adds roughly 0.05-0.4 ms per frame on DXVK (INFERRED). Verdict: GO for (T) from the D3D side, under the requirements listed in the recommendation.

## DETAILS

(1) CREATEDEVICE AND BEHAVIOUR FLAGS (CONFIRMED)
- The game creates IDirect3D9 with Direct3DCreate9 (non-Ex) through LoadLibrary("D3D9.DLL") in 0x524FD0 (strings 0xBE71F4 / 0xBE7204, stored to [0xDD3470] at 0x525211). There is no Direct3DCreate9Ex.
- I classified every vtable call on [0xDD3470] (34 refs, script hitch\mt\d3d\d3dcalls.py). The only CreateDevice calls (IDirect3D9 vt+0x40) are in DX8Wrapper::Create_Device 0x524070, whose only caller is Set_Render_Device 0x5249A0 (call at 0x524D30; the Reset_Device path is 0x524D26 -> 0x522000).
  - Call 1 at 0x5241B6. Pushes: ppDevice 0xDD3474, pp 0xDD2FF8, flags = [0xDD345C] (loaded at 0x524190), hFocus = [0xDD3440], type, adapter = EBP.
  - Retry at 0x524222. Reached only for 16-bit back buffers (0x17/0x18/0x19) with depth format 0x47/0x4B/0x4D. It sets the depth format to D16 ([0xDD3020] = 0x50) and reloads flags from [0xDD345C] at 0x5241EF.
- How the flags are built:
  - 0x5240D2..0x5240E6: VS version (caps+0xC4) below 1.1 gives 0x20 (SWVP), otherwise 0x40 (HWVP).
  - 0x5240ED: if HWVP and DevCaps has PUREDEVICE (caps+0x1E bit 4), OR in 0x10.
  - 0x5240FB: if [0xDD343C] != 0, OR in 0x02 (FPU_PRESERVE).
  - DXVK reports PUREDEVICE (d3d9_adapter.cpp:394), so the DXVK flags are 0x50. MULTITHREADED (0x4) is never set.
- [0xDD343C] is 0 in .data. Its only writer is 0x7BA01B (`mov [0xDD343C], atoi(argv[1])`) in the command-line handler 0x7BA006. No dword 0x7BA006 exists anywhere in the image: the handler is absent from the switch table at 0xC35C00.. and unreferenced. So FPU_PRESERVE is never set, and x87 PC=24-bit is applied on the creating (main) thread. DXVK does this once at creation (SetupFPU, d3d9_device.cpp:91). The x87 control word is per-thread, so a presenter thread cannot change the main thread's precision.
- Device-side calls: there are no GetCreationParameters (vt+0x24) calls on [0xDD3474] (vtable tally of 804 refs). Reset (vt+0x40) occurs only at 0x52220F in Reset_Device 0x522000. Present (vt+0x44) occurs only at 0x522650 (the PRESENT_STUB site).
- Patch site: 0x524112 `mov eax,[0xDD3470]`.
  - It runs after all three flag stores and before both CreateDevice calls, so one site covers both.
  - Branches into the span go only to its start (0x524109 `je 0x524112`).
  - Interior 0x524113..0x524116: no listing operand, no image-wide absolute pointer, and no target in a capstone linear sweep of all executable sections (2.74 M instructions).
  - EFLAGS are dead: `xor ebx,ebx` at 0x524128 rewrites them before any read. EDX is dead.
  - The alternative is two 6-byte sites at 0x524190 and 0x5241EF (8b 0d 5c 34 dd 00 each). Both are also branch-target-only at their starts (0x524131 jbe; 0x5241DF/0x5241E4 jz).
- The flag decision must be known by W3DDisplay::init (0x446330 -> 0x5168B0 -> 0x5249A0). It cannot change later without recreating the device; Reset keeps the behaviour flags.

(2) DXVK v2.6.2 (CONFIRMED)
- Version facts from rotwk\game.dat_d3d9.log: "DXVK: v2.6.2", x86 gcc 15.1.0. The swapchain is "VK_PRESENT_MODE_FIFO_KHR (dynamic: yes)", 4K, 3 images.
- dxvk.conf sets only memory and buffer-caching options. The effective configuration lists nothing else, so there is no built-in app profile.
- I checked the option names in the d3d9.dll strings. There is no d3d9 option that forces or disables multithreading; d3d11.enableContextLock is D3D11-only.
- Sources fetched for tag v2.6.2 are in hitch\mt\d3d\dxvk\.
- How MULTITHREADED works:
  - d3d9_device.cpp:55 `m_multithread(BehaviorFlags & D3DCREATE_MULTITHREADED)`.
  - D3D9Multithread::AcquireLock (d3d9_multithread.h:63) returns an empty lock when not protected. So without the flag there is no internal locking at all: CS chunk, state and swapchain are unprotected.
  - With the flag, every API entry takes `D3D9DeviceLock lock = LockDevice()` (for example d3d9_device.cpp:263, 343, 420, 435, 1802, 8715). This is a sync::RecursiveSpinlock (sync_recursive.cpp): a CAS of the owner thread id from GetCurrentThreadId, recursion counter, release store.
  - Under contention it spins 2000 x `pause`, then SwitchToThread (sync_spinlock.h:21-37, thread.h:123).
- Present path on the calling thread:
  - D3D9DeviceEx::Present -> PresentEx (d3d9_device.cpp:554/4109). The software-cursor block at 4116 runs before the lock; it is only active if SetCursorProperties fell back to a software cursor.
  - D3D9SwapChainEx::Present (d3d9_swapchain.cpp:112) takes the device lock for the whole call. It checks IsDeviceLost, then UpdateWindowCtx (map lookup) and UpdatePresentRegion (wsi::getWindowSize = GetClientRect, wsi_window_win32.cpp:109). UpdateWindowedRefreshRate returns early in fullscreen (m_monitor set).
  - PresentImage (824):
    - EndFrame and Flush submit the CS chunk.
    - Presenter::acquireNextImage (dxvk_presenter.cpp:77) blocks until the previous frame's queue present is done (m_presentPending) plus a swapchain fence.
    - EmitCs runs the blit and `presentImage`, then FlushCsChunk.
    - SyncFrameLatency (1127) waits on frameLatencySignal for frame N-3. GetActualFrameLatency = min(DefaultFrameLatency 3, BackBufferCount 2 + 1). That signal comes from the dxvk-frame thread after vkWaitForPresentKHR under FIFO (1233-1294).
  - vkQueuePresentKHR runs on the dxvk-submit thread (dxvk_queue.cpp:130-173). So the real WSI present already happens off the window thread.
  - No SendMessage, SetWindowPos or ShowWindow in Present. SetWindowPos appears only in enter/leave fullscreen (CreateDevice/Reset) and in DXVK's focus-window hook D3D9WindowProc (d3d9_window.cpp:62-89, WM_ACTIVATEAPP). That hook runs on the window thread and calls device->NotifyWindowActivated, which takes the device lock (8715). It can wait for a presenter-held lock, but the presenter never waits on the window thread, so there is no cycle.
- Device loss: m_deviceLostState=Lost is set only when d3d9.deviceLossOnFocusLoss=True (d3d9_device.cpp:8717-8725; option default false, d3d9_options.cpp:71). So with this config Present never returns D3DERR_DEVICELOST.
- GPU submission: EndScene (0x522628, before the stub) calls ConsiderFlush(ImplicitStrongHint), which flushes when 3 or more CS chunks are pending (util_flush.cpp:43-45). A 4K frame's draws are therefore normally submitted to the GPU before Present (INFERRED). Deferring Present delays only the blit and queue-present.
- Lock overhead (INFERRED): uncontended, about 20-30 cycles per API call (TEB read + lock cmpxchg + store), roughly 5-8 ns. For 10k-50k D3D calls per frame that is about 0.05-0.4 ms per render. Contended, a main-thread D3D call made while the presenter is inside Present spins or yields for the remaining Present time. Measured Present calls are at most about 1 ms with FIFO (../hitching/synthesis.md:63; B present wait about 1.0-1.2 ms, ../hitching/synthesis.md:41).

(3) NATIVE d3d9.dll (players without DXVK)
- CONFIRMED from Microsoft docs (Multithreading Issues; IDirect3D9::CreateDevice remarks): only CreateDevice, Reset, TestCooperativeLevel and the final Release must run on the focus-window thread. Present is not restricted.
- D3DCREATE_MULTITHREADED makes the runtime take its device critical section on each call. INFERRED cost is about 20-40 ns per call, roughly 0.2-1 ms per heavy frame.
- The runtime hooks the focus window. Mode-change messages (WM_ACTIVATEAPP, WM_DISPLAYCHANGE, WM_SIZE) are handled on the window thread while the runtime holds internal critical sections during Reset/CreateDevice.
- In non-Ex D3D9, focus loss in exclusive fullscreen makes Present return D3DERR_DEVICELOST on whichever thread calls it. The state is sticky until Reset, so the next main-thread A Present sees it too and runs the stock handling: TCL 0x522683 -> Reset_Device 0x52268F or Sleep(200) 0x522696.
- INFERRED: Present itself does not SendMessage to the focus window. A blocking Present (render-ahead 3, FLIP, BackBufferCount 2, interval DEFAULT = vsync per G6) holds the runtime lock while it waits.
- Deadlock risk: suppose the main thread waits at a join while it is inside a D3D call or the runtime's mode-switch WndProc. It then holds the runtime critical section that the presenter's Present needs. So joins must be bounded and cancellable, and must never run from inside a WndProc.

(4) EXISTING MULTI-THREADED D3D USE (CONFIRMED unless marked)
- Game DX mutex: Win32 mutex [0xDD1FD8] (created in 0x524FD0 at 0x525071).
  - Lock: 0x51EEC0 (WaitForSingleObject, 20 s, then error dialog; owner [0xDD34C8], depth [0xDD34CC] under CS 0xDD1F80).
  - Try-lock: 0x51EF50. Unlock: 0x5208D0. Held-by-me checks: 0x51EFA0 and 0x51EFC0.
  - 116 functions call 0x51EEC0.
- drawFrame 0x449CF8 holds the mutex from 0x449D0D to 0x44A271. So at PRESENT_STUB on a B-render the main thread owns it.
- Load-screen/movie worker 0x65CE28 (started in 0x65D604): calls TheDisplay vt+0x18C = 0x4475E4.
  - 0x4475E4 locks at 0x44763D, calls Begin_Render 0x516C40 (TCL vt+0xC at 0x516C52, i.e. stock already calls TCL off the creating thread), and End_Render 0x516DA0 at 0x447839 -> End_Scene 0x5225E0 -> Present at 0x522644 on that worker thread.
  - It unlocks at 0x447843. So stock already presents from a non-window thread on a non-MT device, relying only on the game mutex.
- W3DMouse (vtable 0xBDE600) cursor thread: the ThreadClass at 0xDCB7E8 is started by setRedrawMode 0x499A56 or 0x498ADB when mode [this+0x12DC]==3 (RM_DX8). Its loop 0x4987FE calls [0xDE36E0]->vt+0x30 = 0x498CBC under mouse mutex 0xDCB7E0, not the DX mutex.
  - When mode==3 (0x498CFA), 0x498CBC calls device ShowCursor (vt+0x30, 0x498D22), SetCursorPosition (vt+0x2C, 0x498D5D) and SetCursorProperties (vt+0x28, 0x498DF4).
  - So in RM_DX8 mode stock makes unlocked D3D calls from a second thread during gameplay (whether AotR runs RM_DX8 is INFERRED/unchecked).
- Effect of MULTITHREADED on these threads: per-call serialization. That fixes the latent cursor-thread race and does not change the load screen's semantics (the game mutex still orders it).
- Lock ordering is safe if the presenter never takes the game DX mutex. The D3D lock is then always innermost: DXVK and the native runtime never call game code while holding it, except native and DXVK mode-switch window messages on the main thread itself.

D3D-SIDE VERDICT ON (T): feasible and lower-risk than ../hitching/area_mitigation.md assumed, provided the requirements in the recommendation are met. (C) needs no MT flag, but it does need an x87/D3D-state audit at its checkpoints.

## RISKS

- Per-call lock cost in both modes once enabled. The flag is fixed for the device's lifetime, so 30 mode also runs on an MT device. Estimates (INFERRED): about 0.05-0.4 ms per render on DXVK and about 0.2-1 ms on native. Measure with the flag on and the presenter off before shipping.
- Lock-hold time. DXVK holds its spinlock for the whole Present, including acquireNextImage and SyncFrameLatency (frame N-3). Any main-thread D3D call made at that moment spins or yields: at most about 1 ms today, more if the FIFO queue fills. The cursor thread (RM_DX8) can also stall behind it.
- Deadlocks:
  - If the presenter takes the game DX mutex [0xDD1FD8] while main holds the D3D or runtime lock during a mode switch, main → WndProc → game mutex deadlocks. The presenter must never take that mutex.
  - If main waits at a join while inside a D3D call or a runtime WndProc, it holds the lock the presenter needs. Joins need a timeout or cancel.
  - Native Present sending messages to the focus window is unverified (INFERRED no). An unbounded join could deadlock if it does.
- Device loss on native: a deferred Present returning DEVICELOST must not call TestCooperativeLevel or Reset on the helper thread (native fails or misbehaves off the creating thread). Rely on the sticky state and the next main-thread A Present.
- Reset, Set_Render_Device or shutdown while a Present is pending: the presenter must be idle before 0x522000 (covers 0x5249A0 → 0x524D26, 0x516C40, 0x517B30, 0x52268F, 0x5225C9) and before the device's final Release. If ExitProcess kills the presenter mid-Present, the DXVK spinlock is left owned and later D3D calls spin forever.
- Back-buffer hazard: Present must be issued before the A-render's first back-buffer write (Clear/BeginScene in drawFrame). Otherwise DXVK's in-order CS blit, or the native flip, shows partial A content.
- The presenter must call vt+0x44 directly and must not re-enter PRESENT_STUB, AotR60_PresentSkip or AotR60_PresentDone (they assume the main thread), nor any game code. It must not touch the game globals [0xDD34C4], [0xDD1F38] or [0xDD34D8]; main already handled them through the S_OK return path.
- DXVK's software-cursor block in PresentEx (d3d9_device.cpp:4116) runs before the lock. If SetCursorProperties ever falls back to a software cursor while the RM_DX8 cursor thread updates it, that is a race the MT flag does not cover. Unlikely with a hardware cursor (INFERRED).
- Third-party Present hooks (RTSS, ReShade, Steam or Discord overlays) will now run on the presenter thread, and RTSS limiters would pace that thread. Untested.
- Whether AotR uses W3DMouse RM_DX8 (cursor-thread D3D calls) is unverified.

## RECOMMENDATION

Go ahead with (T) from the D3D side, with these requirements.

(a) Install D3D_MT_FLAGS (0x524112, a1 70 34 dd 00 → e9 rel32 to D3D_MT_CAVE), gated by an ini key read before W3DDisplay::init. A flag change takes effect at the next game start. Do not rely on any dxvk.conf option; none exists. Keep d3d9.maxFrameLatency and d3d9.maxFrameRate unset.

(b) Hand off only in PRESENT_STUB, on the main thread, for a 60-mode B-render (EndScene 0x522628 already done), and only when no other deferred Present is outstanding. Return S_OK, as the existing skip path does.

(c) The presenter thread:
- Create it lazily on the main thread at the first 60-mode C0, never in DllMain. Give it ABOVE_NORMAL or HIGHEST priority and use a high-resolution waitable timer.
- It calls only `device=[0xDD3474]; device->Present(NULL,NULL,NULL,NULL)` (vt+0x44) and records the HRESULT and QPC timestamps. On D3DERR_DEVICELOST it sets g_gapDevLost.
- It never calls TestCooperativeLevel, Reset, Release or EndScene, never game code, and never the game DX mutex. No FPU setup is needed; the control word is per-thread.

(d) Joins:
- Use an atomic state PENDING → RUNNING → DONE.
- A join tries CAS PENDING → CANCELLED. On success the main thread presents inline at once. If the state is RUNNING it waits for DONE with a bounded wait (about 100-250 ms) that services only sent messages: MsgWaitForMultipleObjectsEx(QS_SENDMESSAGE) + PeekMessage(PM_NOREMOVE|PM_QS_SENDMESSAGE). On timeout it logs and continues; the D3D lock still serializes.
- Join at:
  - the next C0 (OnPreRender), or at the latest before drawFrame's lock at 0x449D0D;
  - GAP_RESET_CAVE (0x522000 entry, which covers every Reset including Set_Render_Device);
  - LeaveSixty and OnEngineReset (new game, load, exit);
  - before device shutdown and process exit.
- Never join from inside a WndProc or a D3D callback.

(e) Device loss is handled by stock code on the main thread through the next A Present. Under DXVK 2.6.2 defaults it never happens.

(f) Telemetry to add first:
- MT on with the presenter off, measuring render-time delta (expected ≤0.4 ms per render on DXVK);
- presenter Present duration and lock-hold time;
- main-thread logic-step time with versus without deferral, which detects D3D contention during logic;
- a count of joins that found PENDING (inline present), RUNNING (waited) or timed out.

Test native d3d9 (no rotwk\d3d9.dll) with alt-tab in exclusive fullscreen before enabling by default. Keep (C) as the fallback if native shows any deadlock or if the MT cost is measurable.

## SITE {
 "id": "D3D_MT_FLAGS",
 "address": "0x524112",
 "length": 5,
 "original_hex": "a1 70 34 dd 00",
 "original_asm": "0x524112: a1 70 34 dd 00  mov eax, dword ptr [0xdd3470]   ; IDirect3D9* (DX8Wrapper::D3DInterface), first instruction after the three behaviour-flag stores (0x5240E6/0x5240F6/0x52410D) and before both CreateDevice calls (0x5241B6, retry 0x524222)",
 "kind": "jmp_detour",
 "replacement_hex": "e9 <rel32:D3D_MT_CAVE>",
 "stub": "D3D_MT_CAVE PROC  ; entry on the main thread inside DX8Wrapper::Create_Device 0x524070 (called from Set_Render_Device 0x5249A0 at 0x524D30). EBX/EBP/ESI/EDI live (pushed at 0x524119/0x52411A, EDI pushed at 0x5240A2); EAX/ECX/EDX/EFLAGS dead; x87 empty.\n    cmp   byte ptr [g_featD3DMT], 0       ; set from the ini (e.g. SplitPresent=1) before W3DDisplay::init; 0 = stock\n    je    stock\n    or    dword ptr ds:[0DD345Ch], 4      ; D3DCREATE_MULTITHREADED into the behaviour flags read at 0x524190 (CreateDevice 0x5241B6) and 0x5241EF (retry 0x524222)\nstock:\n    mov   eax, dword ptr ds:[0DD3470h]    ; == original 5 bytes\n    jmp   dword ptr [T_524117]            ; resume at 0x524117 'mov ecx,[eax]'\nD3D_MT_CAVE ENDP\nWith g_featD3DMT==0 the executed instructions are the stock instruction plus a DLL-flag compare. Flags become 0x54 (DXVK: HWVP|PUREDEVICE|MULTITHREADED) or 0x24 (SWVP) when enabled.",
 "live_after": "At 0x524117: EAX = [0xDD3470] (IDirect3D9*; mov ecx,[eax] and push eax at 0x524121), EBX/EBP/ESI/EDI unchanged (push ebx 0x524119, push ebp 0x52411A, mov ebp,[0xD9B088] 0x52411B), ESP unchanged, x87 empty. ECX dead (overwritten at 0x524117), EDX dead (next written at 0x524145 before use; callee vt+0x10 clobbers), EFLAGS dead (xor ebx,ebx at 0x524128 before any flag reader). Memory side effect: [0xDD345C] |= 4 (only readers 0x524190 and 0x5241EF; no other refs in the image; never read back via GetCreationParameters).",
 "branch_into_span_check": "Interior 0x524113..0x524116: listing.asm operand grep = 0 hits for each address; image-wide 32-bit absolute-value scan = none; capstone linear sweep of all executable sections (2,738,379 insns) = no jmp/jcc/call/loop target in the interior. 0x524112 itself is the target of 'je 0x524112' at 0x524109 (span start, allowed) and the fall-through of 'mov [0xDD345C],eax' at 0x52410D. 0x524117 (resume) is not a branch target.",
 "purpose": "Create the D3D9 device with D3DCREATE_MULTITHREADED so a helper (presenter) thread may call IDirect3DDevice9::Present concurrently with main-thread D3D calls. Covers both CreateDevice calls of the game (0x5241B6 and the 16-bit-depth retry 0x524222) with one site."
}

## SITE {
 "id": "D3D_MT_FLAGS_ALT1 (alternative to D3D_MT_FLAGS, do not install both)",
 "address": "0x524190",
 "length": 6,
 "original_hex": "8b 0d 5c 34 dd 00",
 "original_asm": "0x524190: 8b 0d 5c 34 dd 00  mov ecx, dword ptr [0xdd345c]   ; behaviour flags for CreateDevice 0x5241B6",
 "kind": "jmp_detour",
 "replacement_hex": "e9 <rel32:D3D_MT_CAVE1> 90",
 "stub": "mov ecx, ds:[0DD345Ch] / cmp byte ptr [g_featD3DMT],0 / je @f / or ecx,4 / @@: jmp dword ptr [T_524196]",
 "live_after": "At 0x524196: ECX = flags (pushed at 0x5241A7), EBX/EBP/ESI/EDI live, EAX/EDX dead (reloaded at 0x524196/0x52419B), EFLAGS dead (no reader before call 0x5241B6; test eax,eax at 0x5241B9 rewrites).",
 "branch_into_span_check": "0x524190 is the target of 'jbe 0x524190' at 0x524131 (span start). Interior 0x524191..0x524195: listing grep 0 hits. Needs the same image-wide/linear-sweep check as the primary before use.",
 "purpose": "Alternative MT flag for CreateDevice call 1 only; must be paired with ALT2 for the retry call."
}

## SITE {
 "id": "D3D_MT_FLAGS_ALT2 (pair of ALT1)",
 "address": "0x5241EF",
 "length": 6,
 "original_hex": "8b 0d 5c 34 dd 00",
 "original_asm": "0x5241ef: 8b 0d 5c 34 dd 00  mov ecx, dword ptr [0xdd345c]   ; behaviour flags for retry CreateDevice 0x524222",
 "kind": "jmp_detour",
 "replacement_hex": "e9 <rel32:D3D_MT_CAVE2> 90",
 "stub": "mov ecx, ds:[0DD345Ch] / cmp byte ptr [g_featD3DMT],0 / je @f / or ecx,4 / @@: jmp dword ptr [T_5241F5]",
 "live_after": "At 0x5241F5: ECX = flags (pushed at 0x524204), EBX/EBP/ESI/EDI as at entry, EAX/EDX dead (reloaded 0x5241F5/0x52421F), EFLAGS dead.",
 "branch_into_span_check": "0x5241EF is the target of 'jz 0x5241ef' at 0x5241DF and 0x5241E4 (span start). Interior 0x5241F0..0x5241F4: listing grep 0 hits; full sweep still to run if chosen.",
 "purpose": "Alternative MT flag for the 16-bit-depth retry CreateDevice."
}

## VERIFIER OVERALL
I checked the d3d analysis claim by claim. Its byte-level and static claims hold: the patch site, the behaviour-flag build-up, the CreateDevice calls, the unreferenced FPU_PRESERVE handler, the load-screen worker presenting off the main thread, and the W3DMouse D3D calls. The DXVK v2.6.2 source claims also match the fetched sources: the lock is a no-op without D3DCREATE_MULTITHREADED, the lock is held through the whole swapchain Present, the frame-latency formula is right, and vkQueuePresentKHR runs on dxvk-submit.

Four things in it are wrong or overstated.
1. "Present ≤ ~1 ms" was never measured. It cites the pacer's spacing-wait figure (../hitching/synthesis.md:41), not a Present duration. The late_skips=0 argument (../hitching/synthesis.md:63) is a conjunction and cannot bound Present time. The analysed log predates the presentMax telemetry (src/telemetry.cpp:357-359).
2. "DEVICELOST only via deviceLossOnFocusLoss" is incomplete. A failed Reset also sets NotReset (d3d9_device.cpp:530, :539), and Present then returns D3DERR_DEVICELOST (d3d9_swapchain.cpp:116).
3. The join point "at the latest before drawFrame's lock 0x449D0D" is unsafe on native d3d9. Present between BeginScene and EndScene on the back buffer fails with D3DERR_INVALIDCALL. TestCooperativeLevel callers at 0x444CEB, 0x466FDB, 0x472F24, 0x473CDE, 0x4754FC and 0x4777DB suggest render-to-texture passes, and their placement relative to drawFrame is unverified. DXVK has no in-scene check (d3d9_swapchain.cpp:105-200), so the bug would show only on native.
4. The deadlock analysis misses one cycle. DXVK's WM_ACTIVATEAPP hook takes the device lock on the window thread (NotifyWindowActivated, d3d9_device.cpp:8715, before the option check). If the presenter is inside acquireNextImage → recreateSwapChain and the Vulkan ICD needs the window thread, the two threads wait on each other. A message-servicing join cannot break this, because the main thread is spinning in the DXVK lock, not waiting at the join.

The thread inventory is also incomplete: there are 10 CreateThread sites and 3 _beginthread(ex) sites. The device-shutdown join site is not named; it is 0x517AA0 → 0x5257A0 → 0x521140, with the final Release at 0x52121B and [0xDD3474]=0 at 0x52121F.

Verdict: a qualified GO for the MT-flag patch site itself. "GO for (T) from the D3D side" is premature until the Present duration is measured, joins are restricted to C0 and the reset/shutdown hooks, and native d3d9 alt-tab is tested.

- [confirmed] Behaviour flags 0x40 HWVP / 0x20 SWVP plus 0x10 PUREDEVICE plus optional 0x02, built at 0x5240D2-0x52410D and stored in [0xDD345C]; read only at 0x524190 and 0x5241EF; MULTITHREADED never set
  Re-disassembled 0x524070-0x52423B. The cmp/sbb/and/add sequence gives 0x20 if VertexShaderVersion (caps base esp+8, +0xC4) is below 0xFFFE0101, else 0x40. DevCaps byte at caps+0x1E bit 4 is D3DDEVCAPS_PUREDEVICE 0x100000. ghq refs and an image dword scan of 0xDD345C give only the 3 writes and 2 reads (0x5240E7/F7/0x52410E/0x524192/0x5241F1).

- [confirmed] FPU_PRESERVE is never set because its only writer (0x7BA01B, handler 0x7BA006) is unreferenced
  0x7BA006 does atoi(argv[1]) and stores it to [0xDD343C]. The dword 0x7BA006 does not occur in the image. Its neighbours 0x7B9FC6 and 0x7BA024 are in the command-line table at 0xC35DD4/0xC35DBC, and 0x7BA006 is not. My rel8/rel32 byte-superset scan of all executable sections found no branch or call to 0x7BA006..0x7BA01B. 0xDD343C occurs only at 0x5240FD (read) and 0x7BA01C (write).

- [confirmed] One 5-byte jmp_detour at 0x524112 (a1 70 34 dd 00) covers both CreateDevice calls; no branch into 0x524113..0x524116; EFLAGS/ECX/EDX dead
  Bytes and order verified: it runs after all three flag stores and before 0x524190 and the retry load at 0x5241EF. My superset scan (hitch/mt/d3d_verify/bscan.py, sanity-checked against the known targets 0x524112/0x524190/0x5241EF) finds no target in 0x524113-0x524117, nor inside the ALT1/ALT2 spans. Nit: the flag-killing 'xor ebx,ebx' is at 0x52412A, not 0x524128. ECX is overwritten at 0x524117. The spot is not already used: tools/sites.json has no site in 0x5240xx-0x5242xx.

- [uncertain] Present (vt+0x44) on the device only at 0x522650, Reset (vt+0x40) only at 0x52220F, no GetCreationParameters
  A listing scan of call [reg+off] within 12 lines of a [0xDD3474] load finds Present only at 0x522650 and Reset only at 0x52220F. The other +0x44 hit, 0x530F0B, is a texture method. The authors' d3dcalls.py takes only the first call within 80 bytes of each reference, so a device pointer kept in a register or object can be missed. GetCreationParameters is not proven absent, though nothing would break if it existed.

- [confirmed] BackBufferCount 2, FLIP, so DXVK frame latency is min(3, 2+1)=3
  0x524B0C-0x524B45: CL=[0xDD3446] (windowed); neg/sbb/add 2 gives 2 in fullscreen and 1 windowed, stored to BackBufferCount 0xDD3004 and SwapEffect 0xDD3010. The DXVK log shows 'Swap effect: 2'. GetActualFrameLatency (d3d9_swapchain.cpp:1131) is therefore 3 in fullscreen and 2 windowed. The analysis did not mention the windowed case.

- [confirmed] DXVK: without the MT flag there is no locking; with it, a recursive spinlock (2000 pauses, then yield) is held for the whole swapchain Present, including acquireNextImage and SyncFrameLatency; vkQueuePresentKHR runs on dxvk-submit
  d3d9_device.cpp:55 and d3d9_multithread.h AcquireLock returns an empty lock when not protected. d3d9_swapchain.cpp:112 takes the lock, and PresentImage (:824) calls acquireNextImage (:841, waits on m_presentPending in dxvk_presenter.cpp:77-87) and SyncFrameLatency (:911). sync_recursive.cpp spins 2000 times, then this_thread::yield. dxvk_queue.cpp submitCmdLists calls presenter->presentImage on 'dxvk-submit'.

- [refuted] Under default options Present never returns D3DERR_DEVICELOST on DXVK
  This is only true while Reset succeeds. D3D9DeviceEx::Reset sets m_deviceLostState=NotReset when losable resources are still alive (d3d9_device.cpp:530, countLosableResources defaults to True) or when ResetSwapChain fails (:539). Present then returns DEVICELOST (d3d9_swapchain.cpp:116), and TestCooperativeLevel returns DEVICENOTRESET (:268). In practice it is rare, but the claim as stated is wrong.

- [refuted] Measured Present calls are at most about 1 ms with FIFO (../hitching/synthesis.md:63; B present wait ~1.0-1.2 ms, ../hitching/synthesis.md:41)
  ../hitching/synthesis.md:41 is the pacer's spacing wait (presentWaitTicks, pacer.cpp:339-341), not time spent inside Present. ../hitching/synthesis.md:63 infers the bound from late_skips=0, but a late skip needs both g_presentsSinceBlock<2 and being a full interval behind (pacer.cpp:322-327), so zero skips do not bound Present time. The analysed aotr60.log, and the current %APPDATA% one, have no Present-duration field; presentMax was added in later telemetry (src/telemetry.cpp:357-359). The lock-hold time, and therefore the main thread's contention cost, is unmeasured.

- [confirmed] Stock already presents from a non-window thread: load-screen worker 0x65CE28 -> ... -> 0x4475E4 -> End_Render 0x516DA0 -> PRESENT_STUB 0x522644 under DX mutex [0xDD1FD8]
  CreateThread at 0x65D69C with proc 0x65CE28. The worker calls 0x65C19B (0x65CE4F), which locks 0x51EEC0, calls [this]->vt+0x18C (0x65C1CA) and unlocks 0x5208D0. Slot 0x18C of vtable 0xBD9C28 is 0x4475E4 (dword at 0xBD9DB4). 0x4475E4 locks at 0x44763D, calls Begin_Render 0x516C40 and End_Render(1) at 0x447839, then unlocks at 0x447843. 0x516DA0 calls 0x5225E0, which presents at 0x522650. That the worker's object has vtable 0xBD9C28 at runtime is INFERRED from the slot match.

- [confirmed] W3DMouse cursor thread makes unlocked D3D calls (ShowCursor/SetCursorPosition/SetCursorProperties) in RM_DX8; whether AotR uses RM_DX8 is unverified
  Vtable 0xBDE600+0x30 = 0x498CBC (dword at 0xBDE630). Mode test at 0x498CFA; device calls vt+0x30 0x498D22, vt+0x2C 0x498D5D, vt+0x28 0x498DF4. The risk is smaller than stated: the Mouse constructor at 0x5EE519/0x5EE521 selects redraw mode 0 (OS cursor) when GlobalData+0x9C6 is nonzero, and the GlobalData constructor sets it to 1 at 0x642E38 (also repo docs ../gaps/G3_input.md:40). The cursor thread exists only if an INI or option path selects mode 3, which is unverified.

- [uncertain] drawFrame holds the DX mutex 0x449D0D..0x44A271; joining before 0x449D0D is enough to protect the back buffer
  The lock range is confirmed (0x449D0D call 0x51EEC0, Begin_Render 0x516C40 at 0x449D17, unlock 0x44A271). But 'at the latest before 0x449D0D' is not proven safe. Native d3d9 returns D3DERR_INVALIDCALL for a Present issued while another thread has a BeginScene/EndScene pair open on the current render target. Render-to-texture code with its own TestCooperativeLevel/scene (0x444CEB, 0x466FDB, 0x472F24, 0x473CDE, 0x4754FC, 0x4777DB) has not been placed relative to drawFrame or C0. DXVK does not check InScene in Present, so this fails only on native.

- [uncertain] The presenter never waits on the window thread, so the DXVK lock cannot form a cycle with the WM_ACTIVATEAPP hook
  NotifyWindowActivated takes the device lock before checking deviceLossOnFocusLoss (d3d9_device.cpp:8715-8718), so every focus change spins on the main (window) thread while the presenter holds the lock. If the presenter's Present reaches recreateSwapChain (dxvk_presenter.cpp:110-125, out-of-date or suboptimal on alt-tab or a mode change) and the Vulkan ICD's swapchain creation or present needs the window thread, the threads deadlock. The proposed message-servicing join cannot help, because the main thread is inside WndProc spinning, not waiting at the join. Not demonstrated; it needs an alt-tab or mode-change stress test, or a rule that falls back to an inline Present after any focus or size message.

- [uncertain] EndScene's ConsiderFlush(ImplicitStrongHint) normally submits B's draws to the GPU before Present (flush at >=3 chunks)
  util_flush.cpp:43 (minChunkCount 3) and d3d9_device.cpp:1807 are confirmed. Below 3 chunks since the last flush, up to 2 dispatched chunks plus the open chunk wait for the deferred Present's Flush (PresentImage :825-826), so B's GPU tail is delayed by the whole deferral. Marked INFERRED as small at 4K on the test GPU. A main-thread D3DQUERYTYPE_EVENT GetData(D3DGETDATA_FLUSH) at hand-off would remove the question.

- [uncertain] Lock overhead is about 5-8 ns per call, 0.05-0.4 ms per render on DXVK, 0.2-1 ms on native
  Back-of-envelope only. The 10k-50k D3D calls per frame figure has no source. The game counts its own D3D calls in [0xDD34D8] (incremented at 0x52262E and 0x522653), which could be logged to get the real count. No benchmark was run. These are INFERRED numbers and must be measured with MT on and the presenter off, in both 30 and 60 mode, since the device flag is fixed for the session.

- [confirmed] Present uses only GetClientRect; no SendMessage, SetWindowPos or ShowWindow
  This holds for DXVK's own code in d3d9_swapchain.cpp:105-200 (UpdateWindowCtx, UpdatePresentRegion via wsi::getWindowSize). It does not cover the swapchain-recreate path or driver WSI internals; see the deadlock item above.

- [confirmed] The x87 control word is per-thread; DXVK sets PC24 once at creation on the creating thread
  d3d9_device.cpp:91-92 calls SetupFPU only without FPU_PRESERVE, and SetupFPU (:5770-5810) does fnstcw/fldcw on the calling thread. The presenter thread cannot change the main thread's precision, so logic bit-identity is not touched by Present running elsewhere.

- [uncertain] The simulation figures (L=31 ms: 13.5/40 -> ~25/25 ms)
  The d3d analysis produced no simulation of its own; these figures come from the task text. The simulation area's out_summary.txt shows T and C variants with p99 gaps of about 26-37 ms at P1-P4, not a clean 25/25 split, and assumes zero lock contention. The extra main-thread time a deferred Present could add through the lock (up to the unmeasured Present duration) is not modelled anywhere.

## VERIFIER MISSED
These items are missing before (T) can be implemented safely.

1. **Device shutdown join.** Join at WW3D shutdown 0x517AA0 (callers 0x4027F7, 0x446330, 0x449861), which leads to 0x5257A0 and then 0x521140. The final device Release is at 0x52121B, and 0x52121F sets [0xDD3474]=0. The analysis says "before the final Release" but names no site. The presenter should also take the device pointer at hand-off rather than re-reading [0xDD3474], which 0x52121F and 0x5250E4 can change.

2. **Join placement.**
   - Join strictly at C0 (OnPreRender), GAP_RESET_CAVE 0x522000, LeaveSixty/OnEngineReset and the shutdown site.
   - Do not allow "as late as 0x449D0D": native d3d9 returns INVALIDCALL for a Present issued inside another thread's BeginScene/EndScene, and render-to-texture passes have not been placed.
   - Confirm C0 runs before every main-thread BeginScene in the frame.

3. **Re-entrancy of the message-servicing join.** Servicing sent messages in the join (MsgWait with QS_SENDMESSAGE) runs the game WndProc at C0 or inside the Reset_Device entry, which stock never does. WM_ACTIVATEAPP handlers could re-enter game or D3D code while the DX mutex or Reset state is held. This has not been analysed.

4. **Focus-change deadlock and other Present-time callers.** The WM_ACTIVATEAPP / recreateSwapChain cycle described above needs a mitigation: fall back to an inline Present while a focus, size or display-change message is pending, or while the window is not foreground. Telemetry does not cover it yet.

5. **Measurements needed before a GO.**
   - Real Present duration and lock-hold time under FIFO, using the presentMax telemetry that now exists.
   - Count of D3D calls per render, via [0xDD34D8].
   - Whether the logic sub-steps make any D3D calls at all (object/drawable creation leading to asset or texture loads), which decides whether contention ever happens.

6. **Thread inventory.** There are 10 CreateThread sites (0x43D295, 0x461291, 0x4A8098, 0x65D69C, 0x9CB8B2, 0xA7E3C2, 0xA7E7B4, 0xA92B26, 0xA994C8, 0xADE144) and 3 _beginthread(ex) sites (0xA241B3, 0xA38D2D, 0xB53926). Only two were classified; a WW3D TextureLoader background thread, if present, should be checked.

7. **Code that moves ahead of the deferred Present.** End_Render's post-Present calls 0x529E50 (stats), 0x529090 and 0x51F690 (render-state cache reset, wrapper buffer Releases), plus the rest of drawFrame, now run before the B Present is executed. They look harmless (no back-buffer writes, INFERRED) but should be listed.

8. **Other unaddressed items.**
   - g_gapDevLost written asynchronously from the presenter: use an atomic and consume it only at C0.
   - The pacer's AotR60_PresentDone and g_presentsSinceBlock lose their meaning for deferred B Presents, so the late-skip rule needs a new input.
   - The 30-mode byte-exact goal is technically broken by an MT device created at startup. This needs an explicit decision, or (C).
   - Windowed mode uses BackBufferCount 1, giving frame latency 2 and DISCARD; test that case too.