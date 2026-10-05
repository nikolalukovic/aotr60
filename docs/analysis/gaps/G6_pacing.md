# G6_pacing

## Summary

G6 (frame pacing, vsync, Present). I verified everything against game.dat. All patch-site bytes are identical in delayfix.dat and game820.dat. (1) Device creation. DX8Wrapper::Set_Render_Device 0x5249A0 zero-fills D3DPRESENT_PARAMETERS at 0xDD2FF8 (0x524B05 `rep stosd`, 0xE dwords) and sets the fields as follows. BackBufferCount (0xDD3004) and SwapEffect (0xDD3010) both come from the same expression (0x524B39 `sbb eax,eax; add eax,2`): fullscreen gives 2 / FLIP, windowed gives 1 / DISCARD. Windowed (0xDD3018) is set to the flag 0xDD3446. 0x524B5C `89 2d 2c 30 dd 00` stores EBP=0 to PresentationInterval 0xDD302C (D3DPRESENT_INTERVAL_DEFAULT, i.e. vsync). FullScreen_RefreshRateInHz is 0 and Flags is 0. CreateDevice is IDirect3D9 vt+0x40 at 0x5241B6 in 0x524070. The behaviour flags never include D3DCREATE_FPU_PRESERVE ([0xDD343C]=0 and has no writers), so D3D9 and DXVK set the x87 precision control to 24-bit. NEW (corrects core and synthesis row 14): Set_Swap_Interval 0x522460 is used. W3DDisplay::init 0x446330 calls the thunk 0x516E40 at 0x4466DF with arg EBX=0 (`53 e8 5c 07 0d 00`) whenever getWindowed() (Display vt+0x54, byte +0x18) is true. That sets IMMEDIATE (0x80000000) and calls Reset_Device 0x522000 (Reset at 0x52220F). So stock fullscreen runs with vsync and stock windowed runs without it. The DXVK 2.6.2 log (<game folder>\rotwk\game.dat_d3d9.log) confirms fullscreen, swap effect 2, 'Setting display mode ...@0', 'Present mode: VK_PRESENT_MODE_FIFO_KHR (dynamic: yes)' and 3 images. The test dxvk.conf sets no present or latency options. (2) Present. The only device Present on the game path is in DX8Wrapper::End_Scene 0x5225E0: EndScene vt+0xA8 at 0x522628, then Present(NULL x4) vt+0x44 at 0x522650 (15-byte sequence starting at 0x522644). On D3DERR_DEVICELOST it calls TestCooperativeLevel. DEVICENOTRESET leads to Reset_Device(1); any other result leads to Sleep(200) (0x522696 -> 0x5B8830). It is reached only through WW3D::End_Render 0x516DA0. drawFrame 0x449CF8 presents exactly once per call (0x449EFA / 0x44A228, flip=1). Other flip=1 callers are the LOD benchmark 0x4436DD and a W3DDisplay vtable function 0x4475E4. Render-to-texture callers 0x47D5C9 and 0x47F1AC pass flip=0. Flip_To_Primary 0x5227F0 has no callers. (3) Blocking. The loop is single-threaded and renders inside GameEngine::update before the stepper, so time blocked in Present counts as frame time. The stock limiter uses last=now and has no catch-up, so the game is frame-locked: any blocking below the target rate slows the game. Stock at 30.3 FPS never fills the FIFO queue on displays of 60 Hz or more, so it is CPU-paced. On 60 Hz it already shows one 1-vblank frame about every 1.65 s. In 60 mode on a 60 Hz FIFO display the queue fills and Present paces the game at 60.00 instead of 60.61, which is 1.0% slow if nothing is done. (4) Limiter 0x63A196..0x63A1FE analysed instruction by instruction. P=_ftol2(1000/(fps*mult))=33. elapsed=now-last. Stats only feed the debug display 0x448B51. The wait is a Sleep(0)+timeGetTime spin. 0x63A1F8 sets last=exit time. timeBeginPeriod(1) is confirmed at 0x63A507 (arg 1 pushed at 0x63A4C5, GameEngine ctor 0x63A49C), and also at 0x517A49 (WW3D::Init) and 0xA2EA02, so timeGetTime has 1 ms granularity and the stock period is exactly 33 ms long-run. (5) New constraint found: GameLogic::update sub 1 (0x62E4E8) treats time as frozen when TacticalView vtD8() && !vt78() (camera movement not finished) || FUN_0060342F. Logic progress is therefore coupled to per-render camera progress. Any pacing measure that skips a render, or switches between 30 and 60 mode, while a camera move or freeze is in flight can change logic. Recommended scheme: a QPC deadline pacer with T = stock P/2 = 16.5 ms per iteration (33.000 ms per A+B pair, exactly stock). Lateness is clamped to about 1.25*T. Catch-up comes from dropping B-renders: skip clientUpdate at the C0 site when at least one frame late, gated against camera, freeze and pause. Gaps (minimise, load, device lost, limiter off) reset the deadline. Every allow-listed B-render integrator is parameterised by 'sub-frames since the last executed render' (1 or 2). A hard fallback to 30 mode with hysteresis is switched only at a safe tick boundary. By default the DLL leaves the present interval alone and DXVK options are documented. An optional 'nominal 1000/60' pacing gives a perfect cadence on 60/120/240 Hz at a cost of 1% game speed.

## Design notes

PACING SCHEME (recommended; replaces synthesis C2):

1. Hooks.
   - Trampoline at 0x63A196: e9 to LimEntry. Stock code runs when !g_m60; Pacer60(engine) runs when g_m60, then jmp 0x63A21F (needs esi=engine, ebx=0).
   - C0 at 0x6325CF: e8 to C0Stub. It increments g_iter and decides whether to drop the B-render.
   - Optional Present timing hook at 0x522644.

2. Pacer60(engine):
   - P_ms = trunc(1000.0f / (fps[engine+0xC] * mult[0xD9F498])), the same value stock computes (33).
   - T = P_ms * QPF / 2000 (16.5 ms). Optional "nominal" target: T = QPF/60 when fps==30 && mult==1.
   - If g_pacerIter != g_iter-1 or g_needReset: g_next = now (gap: limiter was off, load, minimise, mode switch).
   - g_pacerIter = g_iter; g_next += T; late = now - g_next.
   - If late > Cap (default 1.25*T): g_next = now - Cap.
   - Else if late < 0: wait until g_next (high-resolution waitable timer to g_next - 1 ms, then spin).
   - Write the stock stats (0xDE4314 / 0xDE4310 / 0xDE430C) and [0xDE4318] = timeGetTime().
   - Use int64 QPC only: x87 precision is 24-bit (no FPU_PRESERVE), so never touch the x87 control word or MXCSR.

3. Drop rule (C0Stub): skip clientUpdate if all of these hold:
   - g_m60 && g_lastStepB && !bl
   - QPC - g_next >= T
   - DropAllowed: not paused, not loading, not shell, TacticalView vt78() (camera finished) && !vtD8() && !FUN_0060342F, last A-step succeeded, not Living World (until audited).
   One drop recovers one B-render's cost. Cap >= T is required for drops to trigger.

4. Answers to the five requirements.
   (1) Long-run rate is exactly stock: 33.000 ms per A+B pair, 30.303 pairs/s, 5.05 logic Hz. Exact 1000/60 is offered only as an opt-in "nominal / display-friendly" target (1% slower game, perfect cadence on 60/120/240 Hz).
   (2) A-heavy iterations. The loop renders before it steps, so the iterations are:
       - X = A-render (m_frame++, drawable block, full client update) + B-step (no logic)
       - Y = B-render (allow-list) + A-step (logic sub-step)
       The pair budget of 33 ms equals stock's per-sub-step budget. Deadlines carry slack across the pair (an overrun in one is recovered by a shorter wait in the next). If lateness reaches one full frame, the next B-render is dropped.
   (3) No permanent slowdown: deadline pacing with lateness clamped to about 20 ms, plus gap resets. Long hitches are not repaid (stock-like). Small ones are repaid by shorter waits and at most one drop.
   (4) Displays. On 60 Hz FIFO, Present pacing (60.00) is corrected by one drop per ~99 frames. That is the same hitch frequency stock 30.3 FPS already has on 60 Hz (one 1-vblank frame per ~1.65 s). On ≥120 Hz, CPU-paced, no blocking. VRR is ideal. MAILBOX (presentInterval=0 + tearFree=True) is the alternative with no blocking. Windowed mode is already IMMEDIATE in stock.
   (5) Fallback. Soft: drops at up to 100% reproduce stock-30 pacing with exact speed. Hard: g_m60 off when the drop ratio is >20% over 10 s. Re-probe with backoff, and switch only at s==1 when no camera move or freeze is active.

5. All allow-listed B-render integrators must take n = sub-frames since the last executed render (1 or 2). n=2 reproduces stock code exactly. Otherwise drops or fallback would slow the camera.

CONTRADICTIONS / CORRECTIONS TO THE SYNTHESIS:
(a) Synthesis row 14 and core say Set_Swap_Interval 0x522460 is unused. It is called from 0x4466DF via thunk 0x516E40 in windowed mode with arg 0, so windowed = IMMEDIATE (no vsync). Vsync DEFAULT applies to fullscreen only.
(b) Synthesis C2 (16/17 timeGetTime with last=now) slips permanently on any 16 ms overrun and has ±1 ms jitter. Use the QPC deadline pacer (or at least the 0x63A1F8 deadline variant).
(c) The synthesis treats A-frames as "logic + render". In the actual loop order the logic sub-step shares an iteration with the B-render (Y), and the A-render shares one with the logic-free B-step (X).
(d) New hard constraint: GameLogic sub 1 freeze test (TacticalView vtD8 && !vt78 || FUN_0060342F, 0x62E4E8) couples logic to per-render camera progress. B-render drops and 30<->60 switches (synthesis 2.3) must be gated on camera-finished / not-frozen. The camera half-steps on B-renders are mandatory, not just cosmetic.
(e) C6 imm 0x0E gives 28 ms per paused pair vs stock 29 (3.6% fast). 0x0F gives 30 ms (never faster). Either is cosmetic.
(f) R5 "stock unaffected by vsync" holds, but stock on 60 Hz also hitches once per ~1.65 s, so exact-speed 60 mode with drops is no worse than stock.
(g) Under H the W3DDisplay::draw "frozen" flag (sete at 0x44B812) is true on every B-render. It only affects d, the fast-forward branch and the paused loop test (isGamePaused), so this is fine, but it must not be reused as a "paused" signal.

DXVK: no DLL change of the present interval by default. Optional opt-in sites are 0x524B5C (flags-preserving stub) and 0x4466DF.

## Items

### G6-01 — frame limiter  [run_on_B_with_fix, high]
- **Site:** GameEngine::execute limiter tail 0x639FEF..0x63A21F (FUN_00639fef); core block 0x63A196..0x63A1FE
- **What:** Flag 0xDE4320 is computed at 0x639FF9..0x63A188. 0x63A196 calls timeGetTime -> edi. 0x63A19C..0x63A1AD computes fild [esi+0xC] (FramesPerSecondLimit=30), fmul [0xD9F498] (1.0 in SP), fdivr [0xBD4388] (1000.0f), then _ftol2 0xA3CFA4 truncates to P=33. Then ecx=now-[0xDE4318]. 0x63A1C2..0x63A1CB: wait = elapsed<P ? P-elapsed : 0 (sbb/and). Stats: [0xDE4314]=elapsed, [0xDE430C]+=wait, [0xDE4310]=wait. If elapsed>=P (unsigned) there is no wait; otherwise loop {Sleep(0) (push ebx=0); edi=timeGetTime} until edi-last>=P. 0x63A1F5 restores esi=[ebp-0x20] (engine). 0x63A1F8 sets last=edi. With the flag off, 0x63A200..0x63A21A sets last=now. The period is max(P, work) with 1 ms quantisation and no catch-up, so an overrun permanently slips the schedule. The wait spins a full core.
- **Cadence:** once per main-loop iteration (real time; per render)
- **Reason:** The limiter runs every iteration, A and B alike. In 60 mode each iteration must get exactly half of the stock frame period, so the pair equals stock P=33 ms, and it needs deadline semantics. Recommended: a trampoline at 0x63A196 that runs the stock code unchanged when !g_m60 and a QPC pacer when g_m60 (see design_notes). Minimal alternative: the synthesis C2 pair (0x63A1AD returns 16/17 alternating; 0x63A1F8 becomes last+=P when lateness is within the cap, else last=now). That alternative keeps 1 ms quantisation and cannot drive B-render drops.
- **Risk if wrong:** A plain P/2 with last=now gives 16 ms frames (3.1% fast) or a permanent slowdown after every A-iteration overrun. Integer 16/17 without a deadline drifts. Unbounded catch-up makes the game fast-forward after hitches.
- **Fix @ 0x63A196:** e9 <LimEntry> 90. LimEntry: if !g_m60 { call [0xBD0920]; jmp 0x63A19C } else { push esi; call Pacer60 (cdecl, preserves ebx/esi); add esp,4; jmp 0x63A21F }. At 0x63A21F the loop needs esi=engine and ebx=0; edi is dead. (orig: ff 15 20 09 bd 00 (call [timeGetTime]))
- **Fix @ 0x63A1AD (minimal variant only):** call HalfP: eax=_ftol2(st0); g_m60 ? (toggle ? P-P/2 : P/2) : P (orig: e8 f2 2d 40 00 (call _ftol2))
- **Fix @ 0x63A1F8 (minimal variant only):** e8 <LastStub> 90: g_m60 && edi-(last+Pcur) <= Cap ? last+=Pcur : last=edi (orig: 89 3d 18 43 de 00 (mov [0xDE4318],edi))

### G6-02 — limiter gating  [n/a, high]
- **Site:** limiter enable flag 0xDE4320: 0x639FF9 (clear), 0x63A001..0x63A023 (UseFPSLimit GD+0x26 if TacticalView vtDC()<=1 && !ScriptEngine isTimeFast 0x603491), 0x63A02D (GD+0xBBD -> 0), 0x63A0C2/0x63A169 (MP / speed!=1 -> 1), 0x63A17B..0x63A188 (clientFrame < [0xDE4308]+6 -> 0)
- **What:** Decides whether the limiter runs this iteration. When it is off, the stock code sets last=now (0x63A21A).
- **Cadence:** per iteration
- **Reason:** The 60-mode controller should require that this flag would be on (UseFPSLimit set, no fast-forward, multiplier <= 1, no network). The first 6 m_frames after game start (0xDE4308 reset at 0x6315FB/0x77945F) run unlimited, and the 0x63A200 path never reaches the trampoline. So the pacer must detect that it was not called in the previous iteration (iteration counter incremented in the C0 stub) and reset its deadline to now, mirroring stock last=now.
- **Risk if wrong:** A stale deadline after unlimited frames, loading or a mode switch would cause a catch-up burst (frames shortened, game briefly fast) or a long stall.

### G6-03 — timer resolution  [n/a, high]
- **Site:** timeBeginPeriod(1): 0x63A507 (GameEngine ctor 0x63A49C, arg eax=1 set at 0x63A4C2..0x63A4C5), 0x517A49 (WW3D::Init 0x517A00, push 1), 0xA2EA02 (FUN_00A2E9F0), 0xBCADA2; timeEndPeriod 0x517AC6, 0x63D14F, 0xBCF4D2
- **What:** The process holds 1 ms timer resolution for its whole lifetime, so timeGetTime increments in about 1 ms steps. The stock busy-wait exits right after a tick boundary, which makes the steady-state period exactly P ms (±1 tick jitter) and 30.303 FPS long-run.
- **Cadence:** once at startup
- **Reason:** The DLL pacer should not rely on timeGetTime: integer ms gives ±6% jitter per 16.5 ms frame. Use QueryPerformanceCounter with int64 math. Sleep with a CREATE_WAITABLE_TIMER_HIGH_RESOLUTION timer (Win10 1803+; fall back to Sleep(1), which is about 1-2 ms at 1 ms resolution) until about 1 ms before the deadline, then spin with YieldProcessor. Windows 11 may ignore a raised timer resolution for minimised or occluded windows. The high-resolution waitable timer is not affected, and the minimised case is handled by the stock Sleep(5) loop anyway.
- **Risk if wrong:** If the pacer relied on Sleep granularity you would get oversleep and frame drops. Pure spinning (as in stock) is correct but keeps a CPU core at 100%.

### G6-04 — D3D9 presentation parameters  [n/a, high]
- **Site:** DX8Wrapper::Set_Render_Device 0x5249A0 (writes 0x524B05..0x524B62; Create_Device 0x524070, CreateDevice vt+0x40 at 0x5241B6/0x524222; Reset_Device 0x522000 Reset vt+0x40 at 0x52220F)
- **What:** D3DPRESENT_PARAMETERS live at 0xDD2FF8 (Width 0xDD2FF8, Height 0xDD2FFC, Format 0xDD3000, BackBufferCount 0xDD3004, MultiSample 0xDD3008=0, SwapEffect 0xDD3010, hWnd 0xDD3014, Windowed 0xDD3018, AutoDepthStencil 0xDD301C=1, DS format 0xDD3020, Flags 0xDD3024=0, Refresh 0xDD3028=0, PresentationInterval 0xDD302C). Fullscreen: BackBufferCount=2, SwapEffect=2 (FLIP), Interval=0 (DEFAULT = vsync), refresh default. Windowed: 1 / DISCARD. BehaviorFlags [0xDD345C] = HW or SW vertex processing (|PUREDEVICE) and never FPU_PRESERVE ([0xDD343C] is 0 with no writers). Set_Render_Device is called again by the activate handler 0x442A44 (from 0x4016F8), only in fullscreen, which re-sets Interval=DEFAULT.
- **Cadence:** device creation, mode change or reactivation only
- **Reason:** Stock fullscreen runs with vsync (DXVK maps DEFAULT to interval 1, FIFO; confirmed by the test log). No change is needed by default. An optional DLL override is possible only before device creation: the dinput8 proxy loads at process start because game.dat statically imports DINPUT8!DirectInput8Create. The stub at 0x524B5C must not modify EFLAGS, because `je 0x524C69` at 0x524B6D consumes the flags set by `test cl,cl` at 0x524B3E. It must also keep ebp=0, which is used afterwards. No FPU_PRESERVE means x87 precision is set to 24-bit at CreateDevice. The DLL must do pacing math in int64/SSE and must never change the x87 control word or MXCSR on the game thread.
- **Risk if wrong:** Changing the interval at runtime forces Reset_Device(1), which reloads resources. A flag-clobbering stub would break windowed/fullscreen setup. Changing the FPU control word would break logic determinism.
- **Fix @ 0x524B5C (optional, default off):** e8 <PIStub> 90; PIStub: push eax; mov eax,[g_presentInterval /*0 default, 1 ONE, 0x80000000 IMMEDIATE*/]; mov [0xDD302C],eax; pop eax; ret (no flag changes) (orig: 89 2d 2c 30 dd 00 (mov [0xDD302C],ebp))

### G6-05 — windowed vsync  [n/a, high]
- **Site:** W3DDisplay::init 0x446330: 0x4466D3 call [vt+0x54] getWindowed (0x9E7051 = mov al,[ecx+0x18]); 0x4466DE push ebx(=0, set at 0x446341); 0x4466DF call 0x516E40 (thunk jmp 0x522460 Set_Swap_Interval); else 0x4466E7 call 0x4429E2 (ClipCursor)
- **What:** In windowed mode Set_Swap_Interval(0) sets 0xDD302C=0x80000000 (IMMEDIATE) and calls Reset_Device(1). The jump table at 0x5224C0 is {0:0x522475 IMMEDIATE, 1:0x5224AE ONE, 2:0x522488 TWO, 3:0x52249B THREE(=4)}. This corrects the earlier claim that 0x522460 has no callers: Ghidra missed the thunk call, and a rel32 scan finds it.
- **Cadence:** once at display init
- **Reason:** Windowed players (GlobalData 'Windowed' +0x2C or -win) already run without vsync. In 60 mode they are paced purely by the DLL pacer at exactly 60.61, and no Present blocking or drops occur. Optional override: redirect the call so the arg maps to a configured interval.
- **Risk if wrong:** Assuming vsync in windowed mode would mis-tune vsync detection. Overriding without a Reset would have no effect.
- **Fix @ 0x4466DF (optional, default off):** e8 <SwapStub>: push (cfg>=0 ? cfg : arg); call 0x522460 path (via 0x516E40); fix stack (orig: e8 5c 07 0d 00 (call 0x516E40))

### G6-06 — Present  [run_on_B_as_is, high]
- **Site:** DX8Wrapper::End_Scene 0x5225E0 (EndScene 0x522628; Present 0x522644..0x522652 -> call [edx+0x44] at 0x522650); WW3D::End_Render 0x516DA0 (call 0x516DBB, FrameCount 0xDD1E20++); flip=1 callers 0x449EFA/0x44A228 (drawFrame 0x449CF8), 0x4437CC (LOD benchmark 0x4436DD), 0x447839 (vtable fn 0x4475E4, slot 0xBD9DB4); flip=0 callers 0x47D728/0x47D79C (0x47D5C9), 0x47F233 (0x47F1AC)
- **What:** Present(NULL,NULL,NULL,NULL) is called once per drawFrame. Success: [0xDD34C4]++ and [0xDD1F38]=0. DEVICELOST (0x88760868): TestCooperativeLevel (vt+0xC); DEVICENOTRESET (0x88760869) calls Reset_Device(1), otherwise Sleep(200) (0x522696). Flip_To_Primary 0x5227F0 (thunk 0x516DF0) has no callers.
- **Cadence:** per render (per drawFrame; several times per iteration only in the paused-camera inner loop)
- **Reason:** The draw and Present are the core of the B-render allow-list. An optional timing hook around Present measures how long it blocks (vsync or queue-full detection for diagnostics, the 'nominal' auto option and fallback stats). The stub must return the HRESULT in eax and preserve ebx (=0, used by `cmp eax,ebx` at 0x52265A).
- **Risk if wrong:** If Present is skipped on B-renders, nothing is shown at 60. The device-lost Sleep(200) must be treated as a gap by the pacer, otherwise the game fast-forwards after alt-tab.
- **Fix @ 0x522644 (optional metrics):** e8 <PresentTimed> 90*10; PresentTimed: dev=[0xDD3474]; t0=QPC; hr=dev->lpVtbl[17](dev,0,0,0,0); g_presentBlockUs=QPC-t0; return hr (orig: a1 74 34 dd 00 8b 10 53 53 53 53 50 ff 52 44)

### G6-07 — vsync / display rate  [n/a, high]
- **Site:** Present blocking interaction: drawFrame Present inside clientUpdate 0x632409 (called 0x6325CF) before the stepper; limiter 0x63A1B4 measures from previous exit
- **What:** Time spent blocked in Present counts toward the iteration, and stock last=now means the game runs at min(target, display-limited rate). Stock (30.30 FPS) on displays of 60 Hz or more never fills the FIFO queue (DXVK D3D9 frame latency, believed to default to 3), so it is CPU-paced and exactly stock speed. Even so, 30.30 content on 60 Hz shows one frame for only 1 vblank about every 1.65 s. In 60 mode on a 60.00 Hz FIFO display the queue fills, Present blocks at the vblank cadence, and the loop runs at 60.00, i.e. 1.0% slow (59.94 Hz gives 1.1% slow) with about 2 extra frames of latency. On 120/240 Hz there is no blocking; frames land on a 2- or 4-vblank cadence with one short frame about every 1.65 s. On 144/165 Hz you get irregular 2/3 vblank judder, which is inherent (stock has the 4/5 equivalent). On VRR the cadence is perfect.
- **Cadence:** real time
- **Reason:** Decision: by default keep exact stock speed (33.000 ms per pair) and absorb the display deficit with B-render drops (G6-08). On 60 Hz this gives one dropped B-render about every 1.65 s, the same hitch frequency stock already has there. An optional PacingTarget=nominal (T=1000/60 ms, logic 5.000 Hz, 1% slower than stock, which is the engine's nominal 30 client FPS) gives a perfect cadence on fixed 60/120/240 Hz.
- **Risk if wrong:** Without drops or deadline handling the game runs 1% slow on 60 Hz FIFO. An aggressive catch-up would hitch more often than stock.

### G6-08 — B-render drop (catch-up / soft 30 fallback)  [skip_on_B, medium]
- **Site:** GameEngine::update render call 0x6325CF (synthesis C0 site); bl = halt flag from 0x6325C9
- **What:** New behaviour in the C0 stub. If g_m60 && g_lastStepB (the previous stepper frame was a B-frame) && !bl && DropAllowed() && QPC()-g_next >= T, it returns without calling clientUpdate, so this iteration runs only the A-step logic. Otherwise it does the synthesis C0 bookkeeping and jumps to [eax+0x9C]. The stub also increments g_iter every iteration, for pacer gap detection.
- **Cadence:** per iteration, only for B-renders
- **Reason:** A dropped B-render is 'the allow-list with zero entries'. Everything non-allow-listed already runs only on A-renders, and m_frame is unaffected (GC+0xC8=0 after a B-step). W3D sync C3 is exact (pending=0, so the next A-render adds the full 33). Interp caches are keyed on g_renderId, which is not incremented. A drop recovers only the B-render's cost (draw + allow-list), not the A-step logic. DropAllowed = !GameLogic+0x124 (paused) && !GameLogic+0x125 (loading) && GameLogic+0x110 != 4 (shell) && !TacticalView(0xDE447C)->vtD8() && TacticalView->vt78() (camera movement finished) && !FUN_0060342F (script freeze) && the last A-step succeeded (GC+0xC8 was 1). Living World mode is excluded until audited. At 100% drops the behaviour equals stock 30-mode pacing, which gives automatic soft fallback.
- **Risk if wrong:** Dropping while a scripted camera move or time-freeze is in flight changes the logic frame at which GameLogic sub 1 unfreezes (G6-09), breaking bit-identity. Allow-listed integrators with fixed 0.5 factors would lose half-steps on every drop (camera slower under load) unless they are n-aware (G6-15).
- **Fix @ 0x6325CF:** e8 <C0Stub> 90; C0Stub: g_iter++; if(drop) {g_drops++; ret} else {synthesis C0 bookkeeping; jmp [eax+0x9C]} (ecx=esi=engine preserved; return lands at 0x6325D4 nop) (orig: ff 90 9c 00 00 00 (call [eax+0x9C]))

### G6-09 — camera progress observed by logic  [run_on_B_with_fix, high]
- **Site:** GameLogic::update sub 1 0x62E4E8 (decompile lines 29-58: frozen = (TacticalView vtD8() && !vt78()) || FUN_0060342F(); if frozen: no frame++, GC+0xC8=0 unless MessageStream vt44(0x1D)); GameClient::update 0x64849E line 78-81 (same test); W3DDisplay::draw loop test 0x44BC0C (vt78)
- **What:** Whether the logic frame advances depends on whether the client camera movement has finished at the moment sub 1 runs. Camera movements advance per render (updateCameraMovements inside drawFrame).
- **Cadence:** per render (camera) vs per tick attempt (logic)
- **Reason:** In model H the camera must take exactly 2 half-steps per stock step, with nothing skipped, between A-steps. The A-step S_i in iteration Y follows both the A-render (X) and the B-render (Y), so by S_i the camera has done 2(i-i0) half-steps, equal to stock's (i-i0) steps. That holds only if every B-render runs its camera half-step. Consequences for pacing: never drop B-renders (G6-08) and never switch 30<->60 mode (G6-14 / synthesis 2.3) while vtD8() || !vt78() || FUN_0060342F. An alternative that allows drops: on an A-render that follows a dropped B-render, run updateCameraMovements twice (n-aware), which is more invasive. Script conditions such as CAMERA_MOVEMENT_FINISHED are plausible further consumers (not verified).
- **Risk if wrong:** Logic divergence and save or replay non-identity during cinematics and scripted camera moves.

### G6-10 — paused-camera render loop  [run_on_B_with_fix, high]
- **Site:** W3DDisplay::draw inner limiter: 0x44B946 now=timeGetTime; 0x44B98C `83 c6 e2` prev=now-30; 0x44B9B8..0x44B9D8 wait while UseFPSLimit && now-prev<29 (`83 f9 1d` at 0x44B9C1); 0x44B9DD prev=now; loop back 0x44BBFE..0x44BC20 only while frozen([ebp-0xd] from sete at 0x44B812) && !vt78 && isGamePaused; GD+0xAF6==1 skips the wait
- **What:** Repeats drawFrame every 29 ms (34.5 FPS) while the game is paused and a camera move is unfinished. Under model H the frozen flag (GC+0xC8==0) is also true on every B-render, but the loop still needs isGamePaused, so running B-renders do not loop.
- **Cadence:** per pass of the inner loop (paused only)
- **Reason:** Camera steps are halved in 60 mode, so the inner period must halve as well (synthesis C6). Stock is 29 ms per pass. imm 0x0E gives 28 ms per pair (3.6% faster). imm 0x0F gives 30 ms (3.4% slower). Exact 14/15 alternation needs a stub because the 5-byte `cmp ecx,0x1D; jge` cannot be replaced by a call without losing the jge. The difference is cosmetic because logic is paused, and the step count is unchanged so determinism holds. Recommend 0x0F (never faster), written only while g_m60.
- **Risk if wrong:** Paused camera moves take 2x as long, or run slightly fast.
- **Fix @ 0x44B98E:** f1 in 60 mode (prev=now-15); restore e2 (orig: e2)
- **Fix @ 0x44B9C3:** 0f (or 0e) in 60 mode; restore 1d (orig: 1d)

### G6-11 — FPS stats / dynamic LOD  [run_on_B_as_is, medium]
- **Site:** W3DDisplay::draw FPS average 0x4430BB (QPC, 30-sample ring 0xDC7590, result Display+0x180) -> dynamic LOD 0x4438DA (FUN_00601F94(avgFPS) -> FUN_006020E7)
- **What:** Averages the real-time FPS over the last 30 renders and selects the dynamic LOD level (particle and animation skipping, client side).
- **Cadence:** per render (real time)
- **Reason:** Real-time based. In 60 mode it measures about 60 FPS instead of 30, so the dynamic LOD level is never lower than stock's. With drops it measures 30..60, still at least stock. It is client-only (affects particles and animation skipping).
- **Risk if wrong:** Gating it would make the LOD react to a halved FPS reading (more particle culling than stock).

### G6-12 — limiter state  [n/a, high]
- **Site:** limiter stats 0xDE4314/0xDE4310/0xDE430C and last 0xDE4318
- **What:** Stats are written only by the limiter. Readers are only the debug stats display FUN_00448B51 (0x448CE7, 0x448CF2, 0x448C31/0x448C3C). 0xDE4318 is read only by the limiter.
- **Cadence:** per iteration
- **Reason:** Pacer60 should keep writing them: elapsed ms, wait ms, accumulated wait, and [0xDE4318]=timeGetTime() at exit. Then the stock path resumes seamlessly when g_m60 turns off (no huge elapsed on the first stock frame).
- **Risk if wrong:** After a 60->30 switch, a stale 0xDE4318 gives one frame without a wait (harmless) or a debug display showing garbage.

### G6-13 — rate statistics  [not_on_B_path, high]
- **Site:** stepper tick stats 0x632653..0x6326AA (every 25 ticks: timeGetTime, GameEngine+0x50/+0x54/+0x58/+0x5C; reader 0x819ECC print only)
- **What:** Measured client frames per second, +0x58 = mean(+0x38)*ticks/seconds.
- **Cadence:** per tick (A-step at s>6/12)
- **Reason:** Runs only on a tick and is print-only. With +0x38=6 it reports about 30.3 in both modes, which is correct for m_frame.
- **Risk if wrong:** None for gameplay.

### G6-14 — pacer gap handling  [n/a, high]
- **Site:** Win32GameEngine::update 0x44181F minimised loop (0x44183C IsIconic; 0x441858 Sleep(5); breaks only on quit or MP); End_Scene device-lost Sleep(200) 0x522696; loading and transition waits in FUN_0062B385
- **What:** In SP the game does not advance while minimised, and renders at ≤5 FPS while the device is lost and not minimised.
- **Cadence:** event
- **Reason:** The pacer must clamp lateness (Cap ≈ 1.25*T) and reset the deadline whenever it was not called in the previous iteration or a mode switch happened. Otherwise, after restore, a 'catch-up' would compress frames, effectively running the game fast. The stock limiter has no catch-up at all, so this bound keeps behaviour close to stock.
- **Risk if wrong:** A burst of shortened frames or dropped B-renders after alt-tab, load or minimise (visible speed-up).

### G6-15 — design rule for drops and fallback  [split_step, medium]
- **Site:** all allow-listed per-render integrators on B-renders (camera ease/scroll/zoom/scripted moves, W3D sync C3, any split-step stub)
- **What:** Each allowed B-render integrator advances by a fixed half-step today (synthesis constants such as 0.025, 1-sqrt(1-f), 16/17).
- **Cadence:** per executed render
- **Reason:** Parameterise every allowed integrator by n = stepper sub-frames since the previous executed render (1 normally in 60 mode; 2 after a dropped B-render, in hard-fallback 30 mode, or on a stock frame). Linear step: stock*n/2. Exponential ease: f' = 1-(1-f)^(n/2). Frame-counted moves: advance n half-steps. Split-step pending halves are flushed at the next executed render. With n=2 every stub reduces to the stock code exactly. C3 (sync) already behaves this way (A-render delta = 33*d - pending).
- **Risk if wrong:** Under load (drops) camera scroll, zoom and ease would run up to 2x slower, and split-step totals would no longer equal stock per pair.

### G6-16 — hard fallback to 30 mode  [n/a, medium]
- **Site:** mode controller (synthesis 2.3), evaluated at s==1 after a successful tick (stepper stubs C1d/C1e)
- **What:** Turns g_m60 off or on with hysteresis, based on pacer statistics.
- **Cadence:** per tick (5 Hz)
- **Reason:** Fallback: drop ratio > 20% of B-render opportunities over a 10 s window while running. A 60 Hz FIFO display gives about 1%, so it does not trigger. Re-probe after 30 s, doubling up to 5 min. At startup do not enable 60 mode if vsync is active and the display refresh (EnumDisplaySettings ENUM_CURRENT_SETTINGS) is below 59 Hz. Add switch preconditions beyond 2.3: TacticalView vt78() (camera finished) && !vtD8() && !FUN_0060342F && !paused. On a switch set g_needReset for the pacer. Soft drops (G6-08) already keep speed exact, so the hard switch mainly returns to pure stock code and saves CPU.
- **Risk if wrong:** Switching during a camera move or freeze changes logic (G6-09). Without hysteresis the mode flaps, causing visible 60/30 oscillation.

### G6-17 — non-60 paths  [not_on_B_path, high]
- **Site:** MP / fast-forward / camera time multiplier: 0x63A03B..0x63A169 (speed multiplier 0xD9F498 written only here), 0x44B8A2..0x44B8EF fast-forward render skip (m_frame%30), 0x44B95F..0x44B98A multiplier render skip
- **What:** Stock limiter or render-skip behaviour.
- **Cadence:** per iteration
- **Reason:** g_m60 is off in these states (synthesis 2.3), so the trampoline runs stock code. MP is untouched.
- **Risk if wrong:** If 60 mode stayed on, fast-forward and multiplier timing would be halved or doubled.

### G6-18 — DXVK present options  [n/a, medium]
- **Site:** DXVK 2.6.2 configuration: <game folder>\rotwk\dxvk.conf (the test config sets only memory and buffer-caching options); log <game folder>\rotwk\game.dat_d3d9.log (FIFO dynamic, 3 images, VK_NV_low_latency2 unsupported, VK_KHR_present_wait present)
- **What:** These options override the present path. d3d9.presentInterval (-1 = app; 0 = no vsync; n = vsync every n vblanks) is applied per Present. dxvk.tearFree (True gives MAILBOX when vsync is off; False gives FIFO_RELAXED when vsync is on). d3d9.maxFrameLatency (frames queued). d3d9.maxFrameRate (DXVK's own limiter; 0 = only when the display mode mismatches; -1 = off). dxvk.latencySleep (Auto = off here; documented as having no effect with built-in limiters). dxvk.allowFse (DXVK says VRR may need it; it breaks alt-tab). d3d9.forceRefreshRate.
- **Cadence:** per Present
- **Reason:** Recommendations. Keep the default FIFO with DLL exact pacing and drops. Optionally set d3d9.maxFrameLatency = 1 or 2 to remove about 2 frames of queue latency on 60 Hz. Alternative with no blocking and no tearing: d3d9.presentInterval = 0 plus dxvk.tearFree = True (MAILBOX; the CPU pacer fully controls timing, the compositor drops about 1 in 99 frames on 60 Hz). On a VRR display, FIFO plus VRR gives a perfect 60.6 (may need dxvk.allowFse = True or the NVCP windowed G-SYNC setting). Never set d3d9.maxFrameRate=60, which double-limits and fights the 60.6 pacer. Leave dxvk.latencySleep at Auto. Using dxvk.hud = fps,frametimes for measurement (M8) is the user's choice. The DLL should NOT set the present interval by default: stock already gives fullscreen vsync and windowed immediate, DXVK users can override it in the conf, and runtime changes need a device Reset. Keep G6-04/G6-05 as opt-in for native D3D9.
- **Risk if wrong:** Doubled limiting (DXVK maxFrameRate plus the DLL) gives a constant 1% deficit and drops. Forcing immediate on native D3D9 fullscreen causes tearing.

## Open questions

- What refresh rate does the display actually run at under DXVK fullscreen? The log only shows 'Setting display mode: WxH@0'. Does it support VRR, and does VRR work without dxvk.allowFse? This decides whether drops ever occur.
- Does the Vulkan driver on Windows expose MAILBOX for DXVK's non-FSE fullscreen swapchain (needed for the presentInterval=0 + tearFree=True alternative)?
- Is DXVK D3D9's default maximum frame latency 3? This sets how many frames queue before Present blocks on 60 Hz and the extra latency. The planned check is to measure it as part of M8.
- User preference: strict stock speed (33.000 ms per pair, a periodic drop on 60 Hz) or the opt-in nominal 1000/60 target (1% slower, perfect cadence on 60/120/240 Hz)? Should nominal be chosen automatically when FIFO and refresh is 60/120/240?
- Besides the camera freeze test in GameLogic sub 1, does any logic or script code read client per-render state (CAMERA_MOVEMENT_FINISHED-like conditions, on-screen tests, Living World camera)? Any such reader extends the drop and mode-switch gate.
- Measured costs of the X iterations (A-render + B-step) and Y iterations (B-render + logic sub-step) in large battles at 4K under DXVK. This determines the drop ratio and whether the 20% hard-fallback threshold fits.
- Does calling the asset manager update (0xDEF548 vt28 = 0xA37E50) and the ScriptEngine debug hooks (0x6325B3..0x6325C9) twice per pair (on dropped and normal B iterations) have any per-call budget effect? Probably harmless streaming; not verified.
- Living World (strategic map) cadence and camera: should drops and 60 mode be disabled there until audited?
- Does Windows 11 ignore the timer resolution when the game window is occluded but not minimised (windowed players)? The pacer's waitable-timer plus spin design avoids this either way.
