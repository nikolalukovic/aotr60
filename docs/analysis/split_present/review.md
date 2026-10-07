# REVIEW asm
I found no defects in the assembly stubs for the 13 SP_* sites or in the modified PRESENT_STUB and GAP_RESET_CAVE. I checked the source against tools/sites.json and against game.dat (disassembled before and after each site). I also checked the encoded instructions with dumpbin /disasm /relocations on build\aotr60_runtime.dir\Release\stubs.obj, which was built after the source changes, and pattern-searched build\Release\dinput8.dll for the fxsave/fxrstor encodings. Scratch files are in <analysis workspace>/hitch\mt\review\asm\ (stubs_obj.txt, sites.txt, brute.py).

What I verified:

- **Stack balance and return addresses.**
  - Every call_gate site (MOD, PLAYER, SKAI, SCRIPT, OBJ1, PART, COLL, STEP_DRAIN) is entered by a 5-byte call, so the return address is the site address + 5.
    - MOD: the callee's ret lands on the two NOPs at 0x62EA9C, then 0x62EA9E. The stack depth is the same as stock.
    - PART: ret goes to the NOP at 0xA3B569, then 0xA3B56A.
    - COLL: ret goes to 0xB6D123.
    - PLAYER, SKAI, SCRIPT, OBJ1: the stub tail-jumps, so the callee sees the stock return address. The SCRIPT arguments stay in place.
    - STEP_DRAIN: 0x6325A0 ends in a plain ret with no stack arguments (checked at 0x632708), so the extra 4-byte frame is balanced.
  - Every func_detour site (PATH, PUMP, TCL, BEGIN_RENDER, SHUTDOWN) jmps to the cave, and the cave jumps to the resume address past the NOP padding: 0x6EC0D8, 0x441A82, 0x516C45, 0x517B3A and 0x517AA7. The SEH-prolog helper call at 0x441A82 is still executed as a call by the game, so its stack is stock.
- **Displaced instructions.** All are reproduced byte-exactly in the object file:
  - `8D 4B 10 / 8B 01 / FF 20` (jmp [eax] replacing call [eax])
  - `56 / 8D 91 00 08 00 00`
  - `8B B7 20 01 00 00`
  - `8B 4E 04 / 8B 16`
  - `B8 56 11 B7 00`
  - `A1 74 34 DD 00`
  - `83 EC 28 / 80 3D 14 1E DD 00 00`
  - `6A FF / 68 B8 8D B7 00`
  - `64 A1 00 00 00 00`
- **Flags.**
  - BEGIN_RENDER_NET: the displaced cmp is the last flag writer before `jmp [T_517B3A]`. On the net path, popfd runs first and the sub/cmp are then executed again, so the ZF the stock je at 0x517B3A reads is correct.
  - At every other site, EFLAGS are dead where the stub clobbers them with its cmp (function entries, or a flag writer follows before any reader).
- **Registers.**
  - The fast paths touch only EFLAGS and the displaced registers.
  - The slow paths keep everything: pushfd/pushad, XMM0-7, and fxsave/fxrstor.
  - Enum and index values match the C++ side:
    - CP_SLOW indices 0-7 match `index < 8` in AotR60_Checkpoint.
    - SPNET reasons 0, 1, 2 are kNetPump, kNetTcl and kNetBeginRender.
    - GAP_RESET pushes 0, which is kCancelReset.
  - AotR60_SpDrain takes no arguments and is called with none. The spec's push of SP_DRAIN_STEP was dropped on both sides.
  - PRESENT_STUB: `test eax,eax` and `cmp eax,2` are correct for the `int` return of AotR60_PresentSkip, and the deferred path returns S_OK.
- **fxsave/fxrstor encoding and alignment.** `fxsave g_cpFx` assembles to `0F AE 05 disp32` and fxrstor to `0F AE 0D disp32`, both with DIR32 relocations to _g_cpFx. In the DLL, all 8 pairs point at 0x1003B2E0, which is 16-byte aligned. The DLL can be rebased by ASLR, but only by 64K multiples, so the alignment holds. PresentNow's g_spFx (0x1003B6B0) and g_spFxAfter (0x1003B8B0) are also 16-byte aligned.
- **x87 state.** The x87 stack is empty at every checkpoint:
  - All sites but PART and COLL are at call boundaries or function entries. There is no x87 code around the PATH callers.
  - PartitionManager::update (PART) has no x87 code.
  - In the collision loop (COLL), the x87 temporaries are popped by 0xB6D17C before the loop returns to its top.
  - fxsave/fxrstor preserves the full state either way.
- **Re-entrancy.** g_cpFx is used only by CP_SLOW. Inside the slow path, AotR60_Checkpoint and PresentNow reach no game logic: the only game code they call is the DX mutex functions 0x51EF50 and 0x5208D0, which contain no checkpoint sites. A nested checkpoint on the same thread therefore cannot happen.
- **MASM labels.** Labels are correctly local to each PROC in the macro expansions (CP_SLOW inside CP_CALL, SPNET, GAP_RESET's `stock`). Every `jmp go` / `jmp stock` resolves to its own procedure's label.
- **Targets.** All 10 new DEFTARGET entries exist and are referenced by the right stubs. symbols.cpp maps all 13 stub names. The sites.gen.h replacement tokens match sites.json.
- **Branches into the spans.** A brute-force rel8/rel32/jcc32/abs32 sweep of game.dat's executable sections found only the three documented false positives (0x516C3D, 0x5FB46D, 0x5FB6C8).
- **30-mode.** g_ppState is never set, so STEP_DRAIN costs call + cmp + ret, the nets run stock, and the checkpoints cost one byte compare.

Notes, not defects:

- **Shared g_cpFx.** g_cpFx is a single static buffer, and CP_SLOW runs fxsave before AotR60_Checkpoint checks the thread. Two threads in a checkpoint slow path at once would corrupt each other's FPU/SSE state. This needs logic code on a second thread while g_cpDue is set, and I found no such path. A stack-aligned save area or a thread check before the fxsave would rule it out.
- **Profile mode cost.** With SplitPresent=2 (profile mode), g_cpDue stays 1 for the whole profiled logic call. Every checkpoint, up to about 15k collision pairs per tick, then pays for the full save, fxsave/fxrstor and call. This adds measurable overhead to the gaps the profile reports.

# REVIEW cpp
I found no blocking defects in the C++ side; the most serious finding is medium. I rebuilt with build.ps1: the build is clean with no warnings and all unit tests pass.

Checked and correct:
- **Pacer speed.** A Python model of the deadline math confirms that the borrow and M2 repay do not change long-run speed. The model is at <analysis workspace>/hitch\mt\review\cpp\pacer_model.py. Over 90,992 on-time X releases, wall time matched the ideal and PacerNextXDeadline equalled the actual X deadline exactly. There is no double counting with owed or pay: the borrow is added back before the lateness test.
- **Predictor slot mapping.** The slot read at the B-render (GE+0x34, GL+0x40) matches the slot learned in SpOnLogicEnd (frame after the call), including the wrap at s=6 and failed sub-1 attempts not being learned. Learning in 30 mode is moot: the tables are flushed when 60 mode starts, and g_gapReset stays set in 30 mode.
- **Timer-thread handshake.** The target is written before the gen with InterlockedExchange64 (cmpxchg8b), and the thread reads gen then target, so the hand-over is safe.
- **Telemetry output.** The CSV header has 59 columns and the values are 59. The new summary log line fits the 1024-byte Log buffer.
- **Compiled PresentNow.** I checked the disassembly: no value is held in an XMM register across fxrstor.
- **No stuck PENDING state.** Every path out of PENDING is covered: checkpoint retry with g_cpDue=1, the drain, the C0 net, and cancels.

The findings listed are the real problems, ranked by severity.

## [medium/confirmed] Stalled stepper (s>=7) is predicted as a real sub-1 step, so frozen-time cinematics can get early release plus a late B on every Y
LOC: src/split_present.cpp:337-356 (NextLogicSlot); used by SplitTarget (src/pacer.cpp:369-381) and the early release (src/pacer.cpp:286-298)
SCENARIO: In a frozen-time cinematic (camera time frozen: GameLogic::update 0x62E4E8 takes the bVar14 branch and clears GC+0xC8), every sub-1 attempt fails, and the stepper sits at s=7, 8, and so on. GL+0x40 stays constant, so NextLogicSlot returns the same slot (sub 1, (F+1)%10) on every Y for the whole freeze. SpOnLogicEnd correctly does not learn failed attempts, so that slot still holds the last real sub-1 cost. If that slot is a heavy one (the 5- or 10-tick AI frame, about 1 in 5 or 1 in 10 freezes), every Y in the cinematic does two things. First, the X-end pacer releases it early by `over`. Second, SplitTarget pushes the B target to the predicted midpoint. Since the real attempt is cheap, AotR60_SpDrain then waits until min(target, PacerNextXDeadline) and presents B at about the X release. The A follows roughly 9 ms later. Result: a steady ~24/9 ms A->B/B->A judder during cinematic camera pans, where the old pacer gave 16.5/16.5. The gates do not catch this: GL+0x124 is not set for frozen time. Mechanism CONFIRMED by code reading (stepper 0x632622..0x6326E4, GameLogic 0x62E4E8 head). The visible effect depends on the slot.
FIX: In NextLogicSlot, return false (no prediction) when s >= 7: a stall is in progress and the next call is a repeat of a failed attempt. Optionally also return false when the tactical view reports frozen time (TV vt+0xD8 / vt+0x78, as GameLogic checks). That gives target = tNormal and no early release while stalled.

## [low/confirmed] Deferred Presents are not counted in the PRESENT run counter, so presents_s and the per-tick site summary under-report 60-mode output
LOC: src/stubs/stubs.asm:796-798 (PRESENT_STUB 'deferred' path has no counter); src/telemetry.cpp:250; src/split_present.cpp:262-335 (PresentNow)
SCENARIO: With SplitPresent=1, almost every B is deferred, light ticks included: target = tNormal is usually more than 0.3 ms ahead. The stub's deferred path returns S_OK without RUNCNT or SKIPCNT, and PresentNow never touches g_siteRun[PRESENT]. In aotr60_rates.csv, presents_s therefore drops from about 60 toward 30 plus the non-deferred Bs. The 120 s per-tick site summary likewise shows PRESENT at about 6 run per tick instead of 12. The T3 A/B comparison (sp=1 vs sp=0 windows) would read as a halved present rate. The spec asked for a DEFERCNT counter in the stub.
FIX: Count a successful deferred Present: either ++g_siteRun[sites::PRESENT] in PresentNow after the device Present, or add the DEFERCNT counter in the stub plus a 'presented later' counter, and use run + deferred-presented for presents_s.

## [low/suspected] PresentNow enters PRESENTING before taking the DX mutex, so a cancel from another thread can be lost
LOC: src/split_present.cpp:262-279 (PresentNow), 240-246 (LockDx non-checkpoint loop), 249-259 (Cancel)
SCENARIO: The spec's safety argument (E, 'a cancel never races a running Present, because state 2 is held under the mutex') assumes the mutex is taken first. The code instead does CAS Pending->Presenting and then LockDx. The cancels that matter all come from a thread that holds the DX mutex: the GAP_RESET cave on a Reset_Device, AotR60_PresentSkip off-main, and the TCL/BEGIN_RENDER nets on the load-screen thread. These are exactly the cases where TryLock fails. Sequence: main is in a drain or net, LockDx(false) spins up to 50 x 2 ms with state=PRESENTING; the other thread's AotR60_SpCancel CAS(Pending->Idle) fails silently. Main then either restores Pending and cancels (timeout), or gets the mutex after the other thread's Reset or Present and presents a back buffer the other thread has rendered into or reset. The checkpoint path has the same race over a shorter window: it restores kPending at line 270. Side issue: Cancel writes the 64-bit g_cooldownUntil non-atomically from those other threads while main reads it in SpTryDefer. A torn value can give a cooldown of minutes. Practically rare, because no other thread takes the DX mutex during 60-mode battles: the streamer thread 0xA361A0 has no direct path to 0x51EEC0/0x51EF50. SUSPECTED for reachability.
FIX: Take the mutex first (LockDx), then CAS Pending->Presenting; if the CAS fails, unlock and return. On a checkpoint lock failure, leave the state at Pending (never touch it). In Cancel, write g_cooldownUntil only on the main thread (set a flag the main thread converts at C0), or store it with InterlockedExchange64.

## [low/confirmed] Timer thread check-then-store race can mark the next job due immediately
LOC: src/split_present.cpp:176-186 (TimerThread), 591-594 (SpTryDefer)
SCENARIO: The thread checks `g_armGen == gen && g_ppState == kPending` and then stores g_cpDue = 1 as a separate step. Suppose it is preempted between line 176 and line 181 while main presents job N (drain) and, one iteration later, defers job N+1. SpTryDefer has already cleared g_cpDue at line 591, but the stale thread's store then marks job N+1 due at its first checkpoint, before its target. That shows B early, a timing glitch only. It needs the HIGHEST-priority thread to be descheduled for over 16 ms, so it is rare (CPU saturation). The other direction, a stale due with state Idle, is harmless as the spec says.
FIX: Make the due flag generation-tagged. For example, g_cpDue holds (gen & 0xFF) | 0x80 via a CAS from 0, and AotR60_Checkpoint presents only when it matches g_pb.gen. Alternatively, after the store, re-check g_armGen and clear the flag (CAS 1->0) if it changed and the state is not Pending for that gen.

## [low/confirmed] Early release ignores the per-B gates, and the FIFO late-skip measures lateness against the shifted deadline
LOC: src/pacer.cpp:286-298 (early release), src/pacer.cpp:434-435 (late-skip test)
SCENARIO: SplitPresentEarlyRelease() checks only the global state. A predicted-heavy Y is released early by `shift` even when its B is certain to fail a SpTryDefer gate: FIFO queue full (the normal state on a 60 Hz FIFO display), cooldown, background window, LW map, or capture. Then (a) with g_owed>0, which is common in heavy phases, there is no spacing wait, so the B is shown up to `shift` early and the preceding A's hold shrinks. (b) On FIFO with a full queue, the late-skip test `now - (g_deadline - g_owed) >= g_interval` uses the early g_deadline, while the absolute schedule of this Y is g_deadline + g_borrow - g_owed. Lateness is overstated by g_borrow, so a B that is on schedule can be dropped as 'a frame behind'. The spec's borrowedButNotDeferred counter, which was meant to watch this, is not implemented.
FIX: Gate the early release on the predictable B gates at the X-end pacer: queueRoom (IMMEDIATE or g_presentsSinceBlock>=2), the cooldown, ForegroundOk, the GL mode/0x125 check and the capture check. Exposing a SpDeferLikely() from split_present would do this. Use `g_deadline + g_borrow - g_owed` in the late-skip test. Add the borrowedButNotDeferred counter (B handoff with g_borrow>0 that returned false).

## [low/confirmed] A-spacing cap applies even when split present is off, so the Ctrl+Shift+F9 sp=0 baseline is not the old pacer
LOC: src/pacer.cpp:455-457
SCENARIO: `target = min(target, g_lastA + g_pairTicks - f/2000)` for A-renders is applied unconditionally: with SplitPresent=0, with the hotkey off, and in profile mode. When a B Present ran late (render overrun, or a blocking B Present), the A is now pulled earlier than the old 'evenly spaced' rule. T3 compares sp=1 against sp=0 windows of the same session to judge the feature, but part of the S1 change is active in both arms. The spec stages each change behind its own switch.
FIX: Apply the cap only when SplitPresentActive() (or when the last B was deferred), so sp=0 windows reproduce the committed pacer exactly. RepayProportional is already a separate switch.

## [low/confirmed] Profile mode samples the wrong tick for sub 1 and cannot produce the T2 acceptance data
LOC: src/split_present.cpp:480-485 (SpOnLogicBegin), 608-621 (AotR60_Checkpoint), src/telemetry.cpp profile Log
SCENARIO: Profiling triggers on `Field<uint32_t>(logic,0x40) % 10 == 0` read before the call. Sub 1 increments GL+0x40 near its start (0x62E4E8), so the profiled sub-1 call is the one that produces frame ≡1. The predictor (frame-after) names the heavy slot ≡0, and sub 1 of that frame is never profiled. Also, only a lifetime max gap per sub and per-site hit counts are recorded. There is no per-tick distribution, so the spec's acceptance 'p99 of the per-tick max gap <= 4 ms' cannot be evaluated, and there is no site pair around the max gap, which is needed to choose the optional sites. The max gap is also never reset per telemetry window.
FIX: For sub 1, test (frame+1) % 10 == 0, or decide after the call using the frame-after value. Record the per-tick max gap into a small histogram (for example 0.5 ms buckets) per sub, and keep the (previous site, next site) indices of the largest gap.

## [low/suspected] CP_SLOW uses one global fxsave buffer with no main-thread check
LOC: src/stubs/stubs.asm CP_SLOW macro (fxsave/fxrstor g_cpFx); src/split_present.cpp:33, 626
SCENARIO: While g_cpDue=1 (a pending job, a stale due, or the whole profiled call in SplitPresent=2), any thread that executes a checkpoint site takes the slow path, and fxsave/fxrstor operate on the single static g_cpFx. AotR60_Checkpoint rejects non-main callers only after the fxsave. If two threads overlap in the slow path, one restores the other's x87 CW, MXCSR and XMM0-7, which changes logic floats (PC24) on the main thread. The spec says 'main thread only', but the stub does not enforce it. No second thread is known to run these logic sites, so SUSPECTED.
FIX: In CP_SLOW, use IS_MAIN_KEEP_EAX (already in stubs.asm) and skip the slow path entirely (jmp go) when not on the main thread, before SAVE_ALL/fxsave.

## [low/confirmed] Feature ships enabled by default, against the spec's rollout, and the new keys are missing from the default ini and the config log
LOC: src/config.h:22-25, src/config.cpp:28-31, src/dllmain.cpp:66-68
SCENARIO: Spec T6: 'Default SplitPresent=0 until T1-T4 pass'. The code defaults SplitPresent=1, SplitPresentEarly=1 and RepayProportional=1, so a fresh install runs the untested in-logic Present path, plus M2 and early release. kDefaultConfigText adds only SplitPresent, while the spec (A.1) says all four keys are added, so users cannot see or turn off Early, Repay or Native. The startup 'config:' log line prints none of them, so a telemetry session cannot show whether RepayProportional or Early were on.
FIX: Default splitPresent=0 until T1-T4 pass, or confirm with the user that default-on is intended. Add the SplitPresentEarly, RepayProportional and SplitPresentNative lines to kDefaultConfigText, and log all four keys in the dllmain config line.

## [low/confirmed] Logic telemetry still records raw logic time, including the in-logic Present
LOC: src/telemetry.cpp:693-704 (LogicUpdateWrapper: g_it.logic/logic1Max/logicOtherMax use d)
SCENARIO: Spec F: 'Use d_corr for the predictor and for logic telemetry'. Only the predictor subtracts g_presentInCall (SpOnLogicEnd). logic1_max_ms, logic_other_max_ms and the stall counters in the CSV include the B Present time (up to one refresh if DXVK blocks). In the sp=1 vs sp=0 comparison, logic looks slower with split present, and Present blocking is misattributed to logic.
FIX: Have SpOnLogicEnd return, or expose, the corrected duration (d - g_presentInCall) and use it for the telemetry fields. Optionally keep the raw value as a separate logicExt column.

# REVIEW safety
Review of determinism and D3D/Windows safety for the uncommitted split-present code. The game folder, the repo and %APPDATA% were not modified; nothing was written to disk.

What holds (CONFIRMED by reading game.dat):

(1) Determinism.
- The CP_SLOW save/restore is complete and correct: pushfd/pushad and XMM0-7 (SAVE_ALL), then fxsave/fxrstor into the 16-aligned g_cpFx, which also covers x87 CW/SW/TW, the x87 stack and MXCSR.
- PresentNow's _fxsave/_fxrstor wraps the Present correctly. No XMM value is live across the _fxrstor, because SetLastError is a call that follows it.
- LastError and the RNG seed [0xDA1CA4] are restored and checked around the Present.
- CP_COLL 0xB6D11E is a call boundary with an empty x87 stack.
- Wall-clock reads reachable from GameLogic::update by direct calls do not affect logic:
  - timeGetTime at 0x60CD8E/0x60CE34 only feeds a 'slow script on logic frame %d = %d ms' sprintf into a stack buffer;
  - the QPC at 0x6F255E is gated by GlobalData+0x11C0 and only sets a drawable debug flag through 0x6ED163;
  - 0x62C159 is the flag-gated benchmark;
  - the GetLocalTime calls are on save/timestamp paths.

(2) DX mutex.
- 0x51EF50 and 0x51EEC0 are WaitForSingleObject plus owner=tid and count++ under CS 0xDD1F80.
- 0x5208D0 does count--, clears the owner at 0, and always calls ReleaseMutex once.
- So the recursive lock/unlock in the nets (TCL_NET/BEGIN_RENDER_NET run inside drawFrame's bracket) is balanced. The owner/count skip rule correctly detects 'main inside a bracket'.
- The W3DMouse thread uses its own mutex, not the DX mutex.
- The load-screen thread and the asset-streamer thread can hold the DX mutex. The streamer yields 0xA33CA0/0xA35980 release and re-take it from the main thread.

(3) D3D ordering.
- Both back-buffer readers require [0xDC7568] > 0, so the capture gate at handoff covers them: the tile/movie loop 0x44B9E3 and the frame save 0x44BC28/0x44BC54. The only writer is the setter at 0x444CAD.
- The second drawFrame (0x44BC20 -> 0x44BBF0) passes TCL_NET at 0x449D17 first.
- The draw-tail call 0x4791E4 only releases objects.
- The remaining main-thread D3D calls between EndScene and Present (SetTexture NULL, Releases, CreateVertexBuffer/Evict) are legal.

(4) Window messages.
- The in-logic pumps 0x84EEFA and 0x98B16C call [TheGameEngine]+0x5C. The dword at 0xBD853C is 0x441A7D, i.e. serviceWindowsOS, so PUMP_NET covers them.
- The only other PeekMessage/GetMessage users are 0x63F6BF (startup) and 0xA92C00, the message loop of its own thread (CreateThread at 0xA92B26).

(5) Device and shutdown.
- Device creation and Reset go only through 0x5249A0 (Create_Device when the device is null, otherwise Reset_Device), so GAP_RESET covers resolution changes.
- 0x5257A0, which releases the device, is reached only from WW3D::Shutdown 0x517AA0. Its callers are WinMain exit and the W3DDisplay destructor, so the sticky disable at shutdown is fine.
- The thread exit is clean and uses no loader lock.

Required fixes, most important first:

1. Arm the 2 s cooldown on every Reset, device loss, pacer gap and focus loss or restore, not only when a pending job is cancelled. Today the alt-tab restore case lets DXVK swapchain re-creation run inside logic.
2. Back off checkpoint retries after a failed try-lock instead of re-arming g_cpDue, which causes a slow-path plus WaitForSingleObject storm while the streamer holds the mutex.
3. In PresentNow, take the DX mutex before the CAS 1->2, so that a cancel from the load-screen thread's Reset or TCL path cannot be lost.
4. Ship with SplitPresent=0 (and Early off) by default until T1-T4 pass.

Smaller fixes:
- The second-Present net should cancel, not present.
- Mask the MXCSR/FSW status bits in fpChanged.
- Do not sticky-disable on one drain mutex timeout.
- Make the due flag generation-tagged.
- Drop the HIGHEST-priority 0.7 ms spin.

Relevant files:
- <game folder>\aotr60\src\split_present.cpp
- <game folder>\aotr60\src\stubs\stubs.asm
- <game folder>\aotr60\src\pacer.cpp
- <game folder>\aotr60\src\config.h
- <game folder>\aotr60\src\config.cpp

## [medium/confirmed] The 2 s cooldown is armed only when a pending job is cancelled, so it is skipped after an idle Reset, a device loss, a pacer gap or a focus regain
LOC: src/stubs/stubs.asm:826-827 (GAP_RESET_CAVE skips the C++ call when g_ppState==0); src/split_present.cpp:249-258 (Cancel arms the cooldown only after a successful CAS); src/pacer.cpp:224-233 (the gap branch does not touch split present); src/stubs/stubs.asm:816-818 (stock A-Present DEVICELOST only sets g_gapDevLost)
SCENARIO: The spec (D gates, I, risks 2) requires 'cooldown 2 s after any gap, DEVICELOST, Reset, mode change' as the main mitigation against swapchain re-creation running inside logic. In the code only SplitPresentReset (mode switch or engine reset), a deferred-Present DEVICELOST and a successful Cancel call Cooldown(). Example: the user alt-tabs out of exclusive fullscreen under DXVK. The pump minimises the window and the game spins in the IsIconic loop with nothing pending, so nothing is cancelled. When the user returns, DXVK's activate hook restores the window with SetWindowPos. The first Y iteration then passes every gate (foreground, not iconic, no cooldown) and defers its B. DXVK's first Present after the restore hits OUT_OF_DATE and runs recreateSwapChain / vkCreateSwapchainKHR from a logic checkpoint inside GameLogic::update. That is the in-logic window-message and re-entrancy exposure the cooldown was meant to exclude. Native d3d9 and resolution changes behave the same way: Reset_Device runs while idle, GAP_RESET_CAVE jumps straight to stock and arms no cooldown.
FIX: Add SplitPresentCooldown() (sets g_cooldownUntil = now + 2 s). Call it unconditionally from GAP_RESET_CAVE (move the cmp/je so that a cheap C call or a byte flag consumed at C0 always runs), from AotR60_Pacer's gap branch (pacer.cpp:228) and its late > freq/4 branch, and from PRESENT_STUB's DEVICELOST path. Also call it from SplitPresentOnC0 whenever !ForegroundOk(), so the cooldown runs from the moment focus or restore returns.

## [medium/suspected] A failed DX-mutex try-lock at a checkpoint re-arms g_cpDue at once, so every later checkpoint hit takes the slow path with a kernel wait (retry storm inside logic)
LOC: src/split_present.cpp:226-238 (LockDx checkpoint path), 268-272 (g_ppState=kPending; g_cpDue=1 on failure)
SCENARIO: A non-main thread can hold the game DX mutex [0xDD1FD8] during logic. The asset-streamer thread (proc 0xA36390, started at 0xA38D2D) loads assets. The main thread's streamer waits 0xA33CA0 and 0xA35980 fully release the DX mutex (loop 0x51EFA0/0x5208D0), Sleep(1), and then re-acquire it, which only makes sense if the streamer thread takes it for D3D resource creation (INFERRED; the stage vt+8/vt+0x18 targets were not traced). While it is held, each checkpoint hit runs pushfd/pushad, 8 movdqu, fxsave/fxrstor, the C++ call, 2 interlocked ops, GetCurrentThreadId and WaitForSingleObject(mutex,0), a syscall, which comes to roughly 0.5-1.5 us. CP_COLL alone is INFERRED at up to ~15k hits per tick. A few ms of streamer ownership during the collision phase can therefore add several ms to the very logic step the feature is meant to split. It also hammers a contended mutex and inflates tryLockFails by thousands. The mutexOwnedSkips path (main inside a bracket) storms the same way, without the syscall.
FIX: On a checkpoint try-lock failure or an owned skip, leave g_cpDue at 0. Re-arm through the timer thread with a backoff of the same gen: InterlockedExchange64(&g_armTarget, now + Ms(0.25)); InterlockedExchange(&g_armGen, gen); SetEvent(g_arm). Cap the retries per job (for example 8), then leave the job to STEP_DRAIN. Count failures per job, not per hit.

## [medium/confirmed] PresentNow enters PRESENTING before it owns the DX mutex and reverts with a plain store, so a cancel from the mutex-holding thread is lost
LOC: src/split_present.cpp:264-278 (CAS 1->2 at 264, LockDx at 268, 'g_ppState = kPending' at 270/274/283)
SCENARIO: The spec's invariant (E, SpCancel) is that state 2 is held under the mutex, so a cancel never races a running Present. The code breaks it. The main thread at a checkpoint does CAS 1->2. Meanwhile the load-screen thread (0x65CE28 -> 0x4475E4, which locks at 0x44763D) holds the mutex, so try-lock(0) fails. In that window the load thread runs TCL_NET (0x516C40), then TCL -> Reset_Device -> GAP_RESET_CAVE, or PresentSkip off-main. AotR60_SpNet and AotR60_SpCancel see state 2, and their CAS 1->0 fails, so nothing is cancelled. The main thread then stores state=1 and later presents the pre-Reset or pre-load B from the next checkpoint where try-lock succeeds. That shows an undefined or reset back buffer (one garbage or black frame) and violates 'no Present on a reset swapchain'. The window is narrow, a few microseconds, and needs a load or Reset on another thread while a B is pending.
FIX: Take the lock first, then claim the job. Return early unless g_ppState==kPending. Call LockDx(checkpoint); on failure, apply the retry/cancel handling WITHOUT any state transition. Then do InterlockedCompareExchange(&g_ppState, kPresenting, kPending); if it fails, call 0x5208D0 and return. Every off-main cancel site (TCL_NET, BEGIN_RENDER_NET, GAP_RESET from the load thread, PresentSkip off-main) runs with the DX mutex held, so a job then either is still PENDING and gets cancelled, or is PRESENTING and the holder cannot exist. The remaining plain stores (the !device path) become cancels from state 2 under the lock.

## [medium/confirmed] Split present, early release and proportional repay default to ON before the determinism and stress gates (T1-T4) have run
LOC: src/config.h:22-24 (splitPresent = 1, splitPresentEarly = true, repayProportional = true); src/config.cpp:30 (default ini text 'SplitPresent = 1')
SCENARIO: The spec's T6 says 'Default SplitPresent=0 until T1-T4 pass'. With the current defaults, every install (including an existing aotr60.ini that has no SplitPresent key) installs the 13 sp sites and presents from inside GameLogic::update. That path has not been through compare_traces (stack-content independence is unproven per spec H/risk 3), the alt-tab/minimise/load stress, or the profile session that validates checkpoint spacing. A logic divergence or a hang would hit normal play, not only the test sessions.
FIX: Set splitPresent = 0 (and splitPresentEarly = false) in config.h and write 'SplitPresent = 0' in kDefaultConfigText. Enable it explicitly in the test ini until T1-T4 pass, then flip the defaults as T6 describes.

## [low/confirmed] The PRESENT_STUB re-entry net presents the pending B, but at that point the back buffer already holds the new frame
LOC: src/pacer.cpp:401-403 (SpNetBeforePresent -> AotR60_SpNet(kNetPresent) -> PresentNow)
SCENARIO: This net fires only when a new Present arrives (after its EndScene) while a B is still pending, which means a frame was drawn without passing TCL_NET or BEGIN_RENDER_NET. The 'pending B' Present then flips the new frame's back buffer. The stock Present that follows flips the rotated back buffer, which under DXVK FLIP is the frame from two Presents ago. The screen shows the new frame and then a stale one: a visible backward jump. If nothing was drawn, it shows B twice. In both cases stock behaviour is to show only the current back buffer.
FIX: On the main thread in AotR60_PresentSkip, when SpPending(): call AotR60_SpCancel with a new reason (for example kCancelSecondPresent), keep incrementing nets[kNetPresent] for telemetry, and let the current Present proceed. Cancelling (B never shown) is legal and matches stock output.

## [low/suspected] fpChanged compares the full MXCSR, including the sticky exception flags, so the T1 gate 'fpStateChanged = 0' can fail falsely
LOC: src/split_present.cpp:294-298
SCENARIO: MXCSR bits 0-5 (IE/DE/ZE/OE/UE/PE) are sticky status flags. Any SSE float math inside DXVK's Present, or in our own code between the two fxsaves, sets them (for example DE or PE if they were clear). Logic resets the FP state at GameLogic::update entry (0x440809: _fpreset + _controlfp). The bytes differ even though fxrstor restores them exactly. The determinism gate in the test plan then reports a change that does not exist. That either blocks rollout or teaches the tester to ignore the counter.
FIX: Compare only the control fields: (FCW & 0x1F3F) at offset 0 and (MXCSR & 0xFFC0) at offset 24. Optionally count status-flag differences separately as information only. Keep the full fxrstor.

## [low/confirmed] A DX-mutex timeout at the drain or a net disables split present for the whole session and stalls the main thread for up to 100 ms
LOC: src/split_present.cpp:240-246 (50 x try-lock(2 ms)), 273-278 (g_disabled = true)
SCENARIO: If the asset-streamer thread holds the DX mutex for more than 100 ms (a large 4K texture batch while units stream in during a big battle), the drain at STEP_DRAIN spins up to 100 ms, cancels, and sets the sticky g_disabled. The feature is then silently off until restart. Telemetry shows only one mutexTimeout cancel. Stock drawFrame would simply have waited in 0x51EEC0.
FIX: On a timeout, Cancel(kCancelMutexTimeout) plus Cooldown(), without a sticky disable. Disable only after K consecutive timeouts (for example 3) and log the owner [0xDD34C8]. Bound the wait by the next pacer deadline (PacerNextXDeadline) instead of a fixed 100 ms.

## [low/confirmed] The timer thread can leave a stale due flag that releases the next B too early
LOC: src/split_present.cpp:176-181 (check gen/state, then store g_cpDue=1); 622-629 (Checkpoint presents any PENDING job when due)
SCENARIO: The timer thread checks g_armGen==gen && g_ppState==kPending and is preempted before 'g_cpDue = 1'. Meanwhile the main thread presents job N (for example the drain at its target), runs X, and defers job N+1 (SpTryDefer clears g_cpDue and arms gen N+1). The thread resumes and stores the stale g_cpDue=1. The first checkpoint of the Y logic step then presents B(N+1) immediately instead of at its target. The effect is cosmetic: an uneven hold. The 'stale flag is harmless' argument in the spec covers only the state != PENDING case.
FIX: Publish the generation: the timer thread stores g_dueGen = gen and then g_cpDue = 1. AotR60_Checkpoint presents only if g_dueGen == g_pb.gen; otherwise it clears g_cpDue and returns. An alternative on the slow path only: if (g_mode != 3 && PacerNow() < g_pb.target) { g_cpDue = 0; return; }.

## [low/suspected] The timer thread spins for up to 0.7 ms at THREAD_PRIORITY_HIGHEST per deferral, with no benefit at a 1-4 ms checkpoint granularity
LOC: src/split_present.cpp:188-193 (YieldProcessor spin), 217 (THREAD_PRIORITY_HIGHEST)
SCENARIO: That is about 30 deferrals per second, each burning up to 0.7 ms of a core at HIGHEST priority, with no affinity mask. When all cores are busy in a heavy battle (main thread, DXVK CS/submit/frame threads, the streamer), the scheduler preempts a normal-priority thread, which can be the main logic thread or DXVK's CS thread, for that spin. The precision bought (under 0.7 ms) is far below the checkpoint spacing the design itself assumes (1-4 ms, with A* gaps up to 4 ms), so it cannot improve presentation timing measurably.
FIX: Drop the spin. Set g_cpDue on the CREATE_WAITABLE_TIMER_HIGH_RESOLUTION wake, which is accurate to about 0.5 ms. If a spin is kept, limit it to 0.1-0.2 ms and lower the priority to THREAD_PRIORITY_ABOVE_NORMAL. Log wakeLate as now.

# CONSOLIDATED

## verdict
Not ready for the first in-game test yet, but close. The reviews and my re-check found no crash, hang or determinism defect: the asm stubs, the save/restore, the pacer's long-run speed and the state machine's exits all check out. Six items should land before playing:
- B1: arm the cooldown on every gap, Reset, device loss and focus change. The spec makes this a required mitigation.
- B2: count the deferred Presents in presents_s.
- B3: subtract the in-logic Present time from the logic telemetry.
- B4: gate the A-spacing cap on SplitPresentActive(), so sp=0 really is the old pacer.
- B5: in PresentNow, take the DX mutex before claiming the job.
- B6: default SplitPresent to off (enable it in the test ini), add the missing ini keys and log all four.

Each is a few lines in split_present.cpp, pacer.cpp, telemetry.cpp or the config files.

Worth doing in the same pass because they are one-liners: N2 (return no prediction when s>=7), N4(a) (the late-skip should use g_deadline+g_borrow) and N3 (generation-tagged due flag).

In the first test, watch tryLockFails and mutexOwnedSkips (they decide whether N1 is needed), nets[kNetPresent], cancels, fpChanged and wakeLateMax.

Fix N5 before the T2 profile session and N7 before the T1 determinism gate. Rejected: R1 (the drain timeout disable is per spec) and R2 (the HIGHEST spin is a spec-accepted trade-off).

Files: <game folder>\aotr60\src\split_present.cpp, <game folder>\aotr60\src\pacer.cpp, <game folder>\aotr60\src\telemetry.cpp, <game folder>\aotr60\src\config.h, <game folder>\aotr60\src\config.cpp, <game folder>\aotr60\src\dllmain.cpp, <game folder>\aotr60\src\stubs\stubs.asm.

## blocking
I re-checked every item against the working tree (split_present.cpp, pacer.cpp, stubs.asm, telemetry.cpp, config.*) and, where needed, against game.dat. "Blocking" means: fix before the first in-game test, because the item is a safety mitigation the spec requires or because it would make that test's telemetry misleading. All of these are small changes.

B1. The 2 s cooldown is armed only when a pending job is cancelled. [medium, CONFIRMED, from the safety review]
- Code: Cooldown() is called only from a successful Cancel(), SplitPresentReset() and the deferred-Present DEVICELOST branch.
- The spec says otherwise: spec.md:148 and :262 ask for a cooldown after any gap, DEVICELOST, Reset or mode change. GAP_RESET_CAVE (stubs.asm) skips the C++ call when g_ppState==0. The pacer's gap branches (pacer.cpp:228 and the `late > g_freq/4` branch) never touch split present. A stock A-Present DEVICELOST only sets g_gapDevLost.
- Failure: alt-tab back from exclusive fullscreen, or an idle Reset_Device. The very next B is deferred. DXVK's out-of-date swapchain recreation then runs inside GameLogic::update.
- Fix (no asm change needed):
  - (a) Add a public SplitPresentCooldown() that does InterlockedExchange64(&g_cooldownUntil, PacerNow()+2*Freq()). Also make every write of g_cooldownUntil interlocked: Cancel() runs on other threads, and today a torn 64-bit value is possible.
  - (b) In AotR60_Pacer, call SplitPresentCooldown() in the `if (gap)` branch and in the `late > g_freq/4` branch.
  - (c) In SpTryDefer, add the gate `else if (g_gapReset || g_gapDevLost) gate = kGateCooldown;`. This covers a Reset or device loss earlier in the same iteration, before the pacer runs.
  - (d) In SplitPresentOnC0, when g_installed && !ForegroundOk(), call Cooldown(). The 2 s then count from the moment focus or restore comes back.

B2. Deferred Presents are not counted in presents_s or in the per-tick PRESENT summary. [low severity, but invalidates test data; CONFIRMED, from the cpp review]
- The `deferred:` path in stubs.asm PRESENT_STUB has no counter, and PresentNow never touches g_siteRun. telemetry.cpp:250 computes presents_s from g_siteRun[sites::PRESENT] only.
- Effect: with SplitPresent=1, presents_s reads about 30-45 instead of 60, which wrecks the sp=1 vs sp=0 comparison.
- Fix: in PresentNow, right after the device present() call, add `++g_siteRun[sites::PRESENT];` (runtime.h is already included). Optionally add a separate spDeferredPresented stat.

B3. Logic telemetry still records the raw logic time, including the in-logic Present. [low severity, test-data validity; CONFIRMED, from the cpp review]
- At telemetry.cpp:693-704, g_it.logic, logic1Max/Sum and logicOtherMax use `d`. Spec F (spec.md:226) asks for d_corr = d - presentInCall. Only the predictor subtracts it today.
- Effect: in the A/B comparison, logic looks slower with sp=1.
- Fix: expose `int64_t SpPresentInCall()`, which returns g_presentInCall (reset in SpOnLogicBegin, so it is still valid after SpOnLogicEnd). In LogicUpdateWrapper use `dc = d - SpPresentInCall()` for those fields. Optionally keep the raw `d` max as a logicExt column.

B4. The A-spacing cap is applied when split present is off, so the Ctrl+Shift+F9 sp=0 arm is not the committed pacer. [low; CONFIRMED, from the cpp review]
- At pacer.cpp:455-457, the `!bRender && ... target > g_lastA + g_pairTicks - g_freq/2000` clamp runs with SplitPresent=0, with the hotkey off and in profile mode.
- Fix: add `SplitPresentActive() &&` to that condition.
- Note for the test plan: RepayProportional also applies to both arms. Either keep it fixed for the whole session or record it (see N9).

B5. PresentNow claims the job (CAS 1->2) before it owns the DX mutex. [medium; race CONFIRMED by code, reachability SUSPECTED; both the cpp and safety reviews found it]
- At split_present.cpp:264-279 the CAS comes before LockDx. The cancels that matter (GAP_RESET_CAVE, AotR60_PresentSkip off the main thread, TCL_NET or BEGIN_RENDER_NET on the load-screen thread) all run on a thread that holds the mutex. They find state 2 and their CAS 1->0 fails silently. Main then stores kPending again with a plain store and later presents a B whose swapchain was reset or whose back buffer was drawn into. This breaks the spec's invariant (E) that state 2 is only ever held under the mutex.
- Not a crash risk, but it is a 10-line fix to the core invariant, so do it in this pass. Fix:
  1. Return false at once if g_ppState != kPending.
  2. Call LockDx(checkpoint). On failure, change no state: at a checkpoint use the N1 backoff; at the drain or a net, Cancel(kCancelMutexTimeout) and set g_disabled as today.
  3. Then CAS(kPresenting, kPending). If it fails, unlock (0x5208D0) and return false.
  4. In the !device branch (already holding kPresenting under the lock), set g_ppState=kIdle, ++cancels[kCancelReset], call Cooldown() and unlock. Do not go through Cancel().

  A recursive lock by main inside the drawFrame bracket (TCL/BEGIN_RENDER nets) still succeeds, because it is a Win32 mutex.

B6. SplitPresent, SplitPresentEarly and RepayProportional default to ON. [medium for shipping; CONFIRMED by both reviews]
- config.h:22-24 sets splitPresent=1 and early/repay=true, and kDefaultConfigText (config.cpp:30) writes "SplitPresent = 1". Spec T6 (spec.md:506) says to default to 0 until T1-T4 pass.
- This does not block the first test: set SplitPresent=1 explicitly in the test aotr60.ini. It does block commit and release.
- Fix:
  - Set splitPresent=0 and splitPresentEarly=false in config.h, and "SplitPresent = 0" in the default text. Keep repayProportional as the user decides; it changes the pacer even with sp off.
  - Add commented SplitPresentEarly, RepayProportional and SplitPresentNative lines to kDefaultConfigText.
- Correction to the cpp review: SplitPresentInit already logs mode and early when SplitPresent!=0. Repay and Native are never logged, though, and nothing is logged at all for sp=0. Log all four keys unconditionally in the dllmain config line.

## non_blocking
Ordered by severity. None of these is needed for a first battle test. Fix each one before the test stage it affects.

N1. Retry storm: a failed checkpoint try-lock sets g_cpDue=1 at once. [medium; mechanism CONFIRMED, trigger SUSPECTED; from the safety review]
- Code: split_present.cpp:268-272, plus the mutexOwnedSkips path in LockDx.
- While another thread holds the DX mutex, every later checkpoint hit (CP_COLL is INFERRED at about 15k per tick) pays SAVE_ALL + fxsave/fxrstor + a WaitForSingleObject(0) syscall. That inflates exactly the logic step being split.
- The reviewers disagree on whether the streamer thread takes the mutex. I checked: the thread is _beginthread(0xA36390 -> 0xA361A0) at 0xA38D2D. It calls its stages through vt+8 and vt+0x18 (virtual calls, not traced). The main-thread wait 0xA33CA0 drops the DX mutex around Sleep(1), which suggests, but does not prove, that the streamer needs it. So the trigger stays SUSPECTED.
- Fix:
  - Add a `retries` field to Pending (reset in SpTryDefer).
  - On a checkpoint lock failure or an owned skip, leave g_cpDue at 0. If ++retries <= 8, re-arm the same gen via the timer: InterlockedExchange64(&g_armTarget, PacerNow()+Ms(0.25)); InterlockedExchange(&g_armGen, gen); SetEvent(g_arm). Otherwise leave the job to STEP_DRAIN.
  - Count failures per job.
- If you defer this fix, watch tryLockFails and mutexOwnedSkips in the first test.

N2. A stalled stepper (s>=7) is predicted as a real sub-1 step. [medium, cosmetic; CONFIRMED]
- Code: NextLogicSlot, split_present.cpp:337-356.
- I verified the stepper at 0x632622-0x6326E4. When sub 1 fails (GC+0xC8 clear), it restores s=edi (7, 8, ...), so a frozen-time cinematic keeps hitting the same (sub 1, (F+1)%10) slot. If that slot holds a heavy AI tick, every Y is released early and its B is pushed late, which gives about 24/9 ms judder.
- Fix: in NextLogicSlot, add `if (s >= 7) return false;`.

N3. Timer-thread check-then-store can mark the next job due early. [low; CONFIRMED by both reviews]
- Code: split_present.cpp:176-181.
- Fix: tag the due byte with the generation. The asm only tests it for nonzero, so no asm change is needed:
  - The timer stores `g_cpDue = 0x80 | (gen & 0x7F)`. Stress mode and the N1 retry use the tag of g_pb.gen.
  - In AotR60_Checkpoint (non-profile), read `due = g_cpDue`. If `g_ppState != kPending || due != Tag(g_pb.gen)`, do `InterlockedCompareExchange8((char*)&g_cpDue, 0, due)` and return.
  - The CAS keeps a newer legitimate store. A plain clear, or a `now < target` check, could lose one.

N4. Early release ignores the per-B gates, and the FIFO late-skip measures lateness against the shifted deadline. [low; CONFIRMED]
- Code: pacer.cpp:286-298 and :434-435.
- The latest+g_borrow cap only applies while g_owed==0. A Y released early whose B fails a gate (FIFO full, cooldown, focus, LW map, capture) is shown early, or wrongly late-skipped.
- Fix:
  - (a) pacer.cpp:435: use `now - (g_deadline + g_borrow - g_owed) >= g_interval`. This one-liner is worth doing now.
  - (b) Refactor the time-independent gates of SpTryDefer into a shared function, expose `SpDeferLikely()`, and require `SpDeferLikely() && (immediate || g_presentsSinceBlock >= 2)` before the early release.
  - (c) Add the borrowedButNotDeferred counter from spec.md:303: in AotR60_PresentSkip, count when bRender && g_borrow>0 && SpTryDefer returned false.

N5. Profile mode samples the wrong tick for sub 1 and cannot produce the T2 acceptance data. [low; blocks the T2 profile session, not the first test; CONFIRMED]
- Code: split_present.cpp:480-485.
- Sub 1 is profiled when frameBefore%10==0, which is the sub 1 that produces frame ≡1. The other subs are profiled at frame ≡0.
- Fix:
  - For sub 1, test `(frame+1) % 10 == 0`.
  - Record the per-call max gap into a per-sub histogram (0.5 ms buckets) and keep the (prev site, next site) pair of the largest gap.
  - Reset per telemetry window.
  - Set g_profileLast at checkpoint exit, so the slow-path overhead the asm review noted is not counted as a gap.

N6. The PRESENT_STUB re-entry net presents the pending B after the back buffer was reused. [low; CONFIRMED by reasoning; diverges from spec.md:207]
- Code: pacer.cpp:401-403.
- This path is reached only when a frame was drawn without passing TCL_NET or BEGIN_RENDER_NET. Presenting the pending B there shows the new frame and then a rotated or stale buffer.
- Fix: on the main thread, when SpPending(), call AotR60_SpCancel with a new kCancelSecondPresent, still count nets[kNetPresent], and let the current Present proceed. Cancelling is always legal.

N7. fpChanged compares the full MXCSR, including the sticky exception flags. [low; mechanism CONFIRMED, frequency SUSPECTED]
- Code: split_present.cpp:294-298.
- Fix before the T1 determinism gate: compare `FCW & 0x1F3F` (offset 0) and `MXCSR & 0xFFC0` (offset 24) only. Optionally count status-only differences separately. Keep the full fxrstor.

N8. CP_SLOW uses one global g_cpFx with no main-thread check before the fxsave. [low hardening; SUSPECTED]
- The asm review raised this as a note, the cpp review as a finding. No second thread is known to run these sites.
- Fix in the stubs.asm CP_SLOW macro: start with `IS_MAIN_KEEP_EAX` and `jne` to a skip label that does `jmp go`. That needs a local label in the macro, or the check can go in each stub before `slow`. This also removes the Checkpoint non-main early return.

N9. Config visibility. [low; CONFIRMED]
- Already covered by B6: the missing ini lines and logging Repay/Native always.

## rejected
R1. "A DX-mutex timeout at the drain or a net sticky-disables the feature and stalls for up to 100 ms" (safety, low). Rejected as a defect.
- This is the specified behaviour: spec.md:176 and :446 say a 100 ms 0x51EF50 timeout disables the feature.
- The stall is no worse than stock. The next Begin_Render/drawFrame waits on the same mutex in 0x51EEC0, which has a 20 s timeout.
- A sticky disable after a 100 ms ownership by another thread is a deliberately conservative choice, and it is logged with the owner thread id.
- Optional only: bound the wait by PacerNextXDeadline.

R2. "The timer thread spins up to 0.7 ms at THREAD_PRIORITY_HIGHEST" (safety, low). Rejected.
- This is a trade-off the spec accepts (spec.md:100, :115, :392).
- The claim that the spin brings no benefit is wrong. Checkpoint spacing is usually far below 0.7 ms (CP_MOD per module update, CP_COLL per pair). Only the A* and other unsplittable gaps reach 1-4 ms. So a sub-0.7 ms wake does improve B timing.
- wakeLateMax already gives the data to tune it. Revisit only if telemetry shows main or DXVK thread preemption.

R3. The cpp review's claim that the config log line prints none of the new keys. Partly incorrect: SplitPresentInit logs mode and early release when SplitPresent!=0. The real gap (Repay and Native are never logged, and nothing is logged for sp=0) is kept in B6.

R4. The cpp review's separate concern about a torn g_cooldownUntil. Not a separate item: it is folded into B1(a), where all writes become interlocked.

R5. The asm review: no defects, and I accept that verdict. Its two notes are not separate defects:
- The shared g_cpFx is kept as N8, low hardening.
- The profile-mode cost is folded into N5.

R6. Disagreement over whether the streamer thread holds the DX mutex (safety: INFERRED yes; cpp: no direct path). I checked the code: thread proc 0xA361A0 reaches its stages only through virtual calls (vt+8, vt+0x18). The main-thread helper 0xA33CA0 drops the DX mutex around Sleep(1). That is consistent with the streamer taking it but does not prove it. Neither side is confirmed, so N1 stays SUSPECTED and gated on the tryLockFails and mutexOwnedSkips telemetry.