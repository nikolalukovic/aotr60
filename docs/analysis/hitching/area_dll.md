# AREA dll

## SUMMARY

I couldn't pin the hitches on the DLL. Its own code costs about 25-50 µs per 33 ms render pair, with an upper bound of about 0.3 ms when the camera is moving. Nothing in it grows over time. The only part that scales with unit count is the per-draw stub calls, and each one costs about the same as a plain call (1.4-1.9 ns). What does grow is the game's own work as the battle gets bigger:
- A-render CPU time went from 2.3 to 9.3 ms.
- B-render went from 1.4 to 6.0 ms.
- Per-draw site calls per logic tick went 157 -> 433 -> 824 (interval values; the log prints cumulative averages).
- The logic tick, estimated from the data, went from about 18 to about 33 ms.

In 60 mode, logic only runs on Y iterations, and the 5 Hz logic tick always runs in the same 16.5 ms slot right after a full B-render (stepper 0x632601-0x6326C6). From about t=283 s every tick iteration runs late: 59-70 late releases per 10.6 s window, against about 53 ticks. Mean lateness grows from 6-8 ms to 22 ms. So every 198 ms one frame lasts 23-39 ms instead of 16.5 ms. That 5 Hz judder fits "starts to hitch after a couple of minutes, in regular intervals".

From about t=389 s the lateness per tick (24-30 ms) is more than the pacer can pay back. It repays at most 2.06 ms (T/8) per on-time iteration, about 22 ms per tick. The rest is counted as lost time ('debt'), up to 350 ms per window.

Three DLL-side things add time to that tick iteration:
- **The B-render itself** (4.5-6 ms), which is the design.
- **The Present-spacing wait before the B Present** (pacer.cpp:336-347), which only runs when no time is owed. It added about 1.4-2.3 ms to the tick in the t≈262-330 phase and nothing at the end.
- **Camera-swap shadow refits** while the camera moves (estimated ≤0.3 ms).

Everything else I checked is negligible or bounded:
- **Pacer wait (WaitUntil):** it uses the high-resolution timer, and its overshoot was about 0 µs when idle. It only overshoots by milliseconds when every logical CPU is busy. timeBeginPeriod makes no difference to this path.
- **Periodic file and display calls:** the CSV write every 10 s, the log summary every 120 s and the once-per-second display query (vsync mode only) each cost under 1 ms.
- **Per-logic-tick telemetry:** under 1 µs.

Scratch files are in <analysis workspace>/hitch\overhead\:
- bench.cpp / bench.exe: x86 micro-benchmarks that copy the DLL's calls.
- wfbench.cpp: WriteFile latency test.
- derive.py: per-window numbers derived from the CSV.
- head\: snapshots of the HEAD sources. All line numbers below refer to HEAD, the phase-6 commit ('60 FPS on the Living World strategic map'), which is the build this session ran. The installed rotwk\dinput8.dll has the same SHA-256 as bin\dinput8.dll (784ab403...). The working tree now has uncommitted telemetry changes.

## FINDING [confirmed] The DLL's own work per render is tiny and does not grow except linearly per draw call, at about the cost of a plain call
I read every code path the DLL runs per render, per draw call and per logic tick, and measured the expensive parts with x86 copies of the same code on the test PC. Per render, OnPreRender (frame_ctl.cpp:349-405) costs about 3-5 µs. Most of that is two GetAsyncKeyState calls in PollHotkeys (frame_ctl.cpp:304), because the && short-circuits after the first key. The rest is field reads, QPC and GetTickCount. UpdateTitle (frame_ctl.cpp:259-300) builds a string every 8th render, but calls SetWindowTextW only when the text changes, which never happens while 60 mode is on. The pacer and Present hooks (pacer.cpp:190-273, 295-367) use 2 timeGetTime calls, 4-6 QPC calls and one SetWaitableTimerEx + WaitForSingleObject. The per-draw stubs, such as STUB_FXEV_GATE (stubs.asm:1367), SKIPBCALL STUB_RECOIL (stubs.asm:635-650), the IF_B_GOTO family (stubs.asm:1002-1479) and C4_KEY_STUB (stubs.asm:258), are 5-12 instructions with one fs:[24h] read, one counter increment and one predicted indirect jump. The C5 table (c5_physics.cpp:23-68) is fixed at 4096 slots with up to 8 probes per lookup. The per-logic-call wrapper (telemetry.cpp:465-497) copies two arrays of 135 counters per tick.
EVIDENCE: bench.exe results on this machine: QPC 26.7 ns; timeGetTime 9.8 ns; GetTickCount 2.9 ns; GetAsyncKeyState 1.40 µs (2.8 µs per render). Stub copy: A skip path 1.43 ns, B run path 1.90 ns, plain call+jmp 1.43 ns. C5 Store 2.3-2.9 ns and Find 1.2-1.3 ns: 0.7 µs per A+B pair at 200 locomotors, 3.2 µs at 800. C5Reset memset of 147 KB: 2.1 µs. At the end of the session INT_RECOIL and INT_FXEV_GATE ran about 824 times per logic tick, about 137 per render (derived from the cumulative log lines 14:36:59/14:39:00). Even at 20 stub hits per drawable that is about 2700 hits, or about 5 µs per render. C5 replays per B-render grew 1.6 -> 12 (CSV), with 'drops 0' in every telemetry log line.
IMPLICATION: The DLL's own code is 0.02-0.3% of a 16.5 ms slot. It cannot explain a_rel growing 2.3->9.3 ms, b_rel growing 1.4->6.0 ms, or late releases growing to 22 ms. Optimizing the stubs or the table is not worth it.

## FINDING [confirmed] No unbounded growth, no O(n^2), no heap allocation and no locks in the DLL's per-render, per-draw or per-tick paths
Every per-frame data structure has a fixed size: the C5 table (4096 entries, reused when stale, c5_physics.cpp:54), the camera records g_rec[2] and g_lwRec[2] (camera.cpp:43,59), the Living World snapshot arrays (lw_present.cpp:26-45; Living World map only, LwSceneShown is false in skirmish), and the counter arrays g_siteRun/g_siteSkip[135] (runtime.cpp:186-187). The pacer's owed time is capped at 6T (pacer.cpp:236-241). The only list walk is ShakerReachesEye (camera.cpp:129-150), bounded at 4096 nodes. It walks the engine's camera-shaker list, which the engine walks itself every frame. Strings and heap objects appear only at init (log.cpp:15-22, telemetry.cpp:260-286). There are no critical sections or mutexes. The only locked instructions are uncontended lock inc/or in PalGate, LwmGate and Vt188Detour (stubs.asm:394-459), a few calls per tick. Code bytes are written once in DllMain.
EVIDENCE: A grep of the HEAD sources for CriticalSection, Mutex, SRWLock, std::vector, new/malloc and std::string/wstring finds only init-time uses. In the log, every telemetry line reports 'C5 ... drops 0', and there are no WARN or ERROR lines in the whole session (aotr60.log).
IMPLICATION: Nothing in the DLL gets slower as the session goes on. Whatever grows over time is in the engine.

## FINDING [confirmed] Periodic DLL work (1 s, 5 s, 10 s, 120 s) costs well under 1 ms and cannot produce the regular hitch
These are all the DLL's periodic jobs. (1) DisplayBlockReason (frame_ctl.cpp:49-81) is cached for 1 s and only calls MonitorFromWindow, GetMonitorInfoW and EnumDisplaySettingsW when the present interval is not IMMEDIATE (frame_ctl.cpp:61-64), i.e. only in fullscreen vsync. (2) EvaluateWindow runs every 5 s (pacer.cpp:96-131) and is arithmetic only. (3) The CSV Report runs every 10 s: one snprintf and one WriteFile (telemetry.cpp:196-249, 337-339). (4) LogSiteSummary plus one telemetry line run every 120 s: about 56 Log() calls, each GetLocalTime + vsnprintf + WriteFile with no flush (telemetry.cpp:155-172, 340-360; log.cpp:24-46). (5) CheckAnomalies runs every 64 renders and does no I/O unless something changes.
EVIDENCE: bench.exe: the uncached display-query body averaged 75 µs (p99 189 µs, max 502 µs; EnumDisplaySettingsW alone averaged 36.5 µs, max 444 µs) on the test display. A Log() line was p50 7 µs, p99 52 µs. The 56-line summary burst was p50 467 µs, p99 1.15 ms. wfbench.exe, same file pattern on the same drive: 3 x 4000 writes, p50 18-24 µs, p99.9 124-161 µs, max 0.65 ms. The one exception was a single 13.8 ms write, the 5th write right after CreateFile(CREATE_ALWAYS), which only happens at startup. In the log, the 120 s summaries are at 14:34:59, 14:36:59 and 14:39:00.
IMPLICATION: File I/O on the render thread and the display query are not the cause. A ~0.5 ms burst every 120 s is not noticeable. The display query only exists in vsync mode, and the snapshot does not record whether this session used vsync.

## FINDING [confirmed] The pacer's WaitUntil is accurate and timeBeginPeriod does not matter for it; it only overshoots when the CPU is oversubscribed
WaitUntil (pacer.cpp:70-94) sets a high-resolution waitable timer to fire 1 ms early, waits on it with a 100 ms safety timeout, then spins with YieldProcessor for the rest (any remaining time ≤2 ms is spun). The log has no 'pacer: high-resolution timer unavailable' line (pacer.cpp:66), so the high-resolution timer was in use. The deadline is absolute (pacer.cpp:211 adds the interval to the previous deadline), so an overshoot only shortens the next slot and never builds up. The Sleep(1) fallback (pacer.cpp:88) only runs if SetWaitableTimerEx fails. The game itself calls timeBeginPeriod(1) at 0x517A47 and 0xA2E9F3 (also at 0x63A507). The stock limiter at 0x63A1DC-0x63A1F3 spins with Sleep(0) and timeGetTime.
EVIDENCE: bench.exe, pacer-like loop (16.5 ms deadlines, 2-12 ms of work, 240 iterations). High-resolution timer, idle: mean overshoot 0.04 µs, p99 0.1 µs, max 8 µs; with timeBeginPeriod(1): 0.03 µs, i.e. the same. With one busy thread per two logical CPUs: p99 1.5 µs, max 120 µs. With one busy thread per logical CPU: p90 0.82 ms, p99 2.3 ms, max 3.5 ms. With 1.5 busy threads per logical CPU: mean 1.0 ms, p99 5.4 ms, max 7.8 ms. For comparison, the plain waitable timer and Sleep(1) overshoot by 3.3-4.0 ms on average (max about 14-15 ms) even with timeBeginPeriod(1). That is because Windows 11 ignores timer-resolution requests from processes without a visible window, which the benchmark process is. The game has a visible window, so the request holds there, and the high-resolution timer path does not depend on it either way.
IMPLICATION: The timer does not oversleep under normal game load (a handful of busy game and DXVK threads on a multi-core CPU). It is not a source of hitches. Keep the high-resolution timer. If it ever fell back to Sleep(1) or a plain timer, waits could overshoot by up to about 15 ms whenever the game window is not visible.

## FINDING [likely] What grows is the engine's own render and logic-tick cost; from about t=283 s every logic tick overruns its 16.5 ms slot, which gives a 5 Hz judder
Logic only runs in Y iterations (X takes the forced halt, HALT_STUB stubs.asm:219-229). The tick, GameLogic::update(1), runs in the stock step after the B-render that saw s==6 at C0: s++ goes past 6, s is set to 1 and vt98(1) is called (0x632601-0x6326C6). That one iteration therefore holds B-render + Present + the full logic tick in a 16.5 ms slot. In stock 30 FPS the same work sits in a 33 ms frame together with a full render. Before about t=262 s only about a third of ticks were late. From t≈283 s there are 59-70 late releases per window against about 53 ticks, and on-time drops to about 90%. Mean lateness per late release grows 7.7 -> 13 -> 17 -> 22 ms (t=283 -> 336 -> 368 -> 421). If the late releases are the tick iterations, release-to-next-pacer time at the tick is 24 -> 39 ms. Subtracting b_rel (4.5 -> 6 ms) puts the logic tick at about 18 -> 33 ms. The gap between the B Present before the tick and the A Present after it is about tick + a_rel, roughly 25-42 ms, against 16.5 ms normally. When late time per tick goes above the repayment capacity (about 11 on-time iterations x T/8 = 21.8-23.8 ms per tick), the cap turns the rest into debt: t=389: 24.3 ms per tick, debt 168 ms; t=421/432: 29.1/30.3 ms per tick, debt 350 ms per window (pacer.cpp:226-243, 244-257).
EVIDENCE: derive.py on aotr60_rates.csv. late_releases: t=251: 19, t=262: 33, t=283: 61, ... t=421: 70. ms per late release: 8.9, 12.2, 7.7, ... 22.3. late ms per tick vs repayment capacity per tick: t=283 8.9 vs 22.5; t=389 24.3 vs 21.9; t=421 29.1 vs 21.8. a_rel 2.3-5 -> 9.3 ms, b_rel 1.4-3 -> 6.0 ms. Per-draw sites per tick (interval values derived from the cumulative log lines): 157 (245 ticks), 433 (ticks 245-844), 824 (ticks 844-1452). logic_ticks_s and mframe_s stay at stock until the debt phase. present_spacing_max is 55-210 ms in every window from t=92 on, so there are also larger stock-like spikes on top.
IMPLICATION: The 'hitch after a couple of minutes, in regular intervals' is most likely the per-tick long frame. It appears once B-render + logic tick no longer fit in 16.5 ms, and gets worse as the battle grows. The engine's costs grow, not the DLL's. The absolute stall at the tick (about 42 ms at the end) is about the same as stock's tick frame (render + tick). It just looks much worse next to 16.5 ms frames than next to 33 ms frames. Pacing changes can remove the debt. Hiding the gap would need frames queued ahead, i.e. more latency.

## FINDING [likely] The Present-spacing wait before the B Present delays the logic tick (when no time is owed)
AotR60_PresentSkip (pacer.cpp:336-347) holds every paced Present until lastPresent + halfPair - 0.25 ms, at most until release + 0.6*halfPair (9.9 ms), whenever g_owed == 0. On a B-render this is about a_rel - b_rel - 0.25 ms after the B-render finishes (the A Present came a_rel after its release). In the tick iteration that wait comes before the stock step, so the logic tick starts that much later. The time owed after a late tick (6-10 ms in the t≈262-330 phase) is repaid within 3-5 on-time iterations (T/8 = 2.06 ms each, pacer.cpp:247-255), so owed is usually 0 again by the next tick. In the end phase owed almost never reaches 0, so the wait is off.
EVIDENCE: present_wait_ms per B Present (derive.py): about 1.0-1.2 ms on average across all B Presents for t=262-330, matching a_rel - b_rel = 1.6-2.6 ms. It falls to 0.01-0.00 ms at t=421-432. The tick lateness in the same phase was 6-10 ms per tick, so this wait is roughly 15-30% of it. The DLL can tell which B precedes the tick: GE+0x34 == 6 at C0 (stepper 0x632601-0x6326C6).
IMPLICATION: This is a small, DLL-caused addition to the tick overrun in the middle phase, and it is cheap to remove. It does not explain the end-phase hitches.

## FINDING [likely] Camera swaps add up to 4 extra shadow refits per pair while the camera moves; the cost is fixed per map, not per unit
BeginSwap and CamSwapEnd (camera.cpp:90-111, 172-185) call ShadowRefit 0x47D37D on the A swap (SceneOpen_A camera.cpp:325 / SceneRestore camera.cpp:425). On B they do so only if the camera changed after the A-render recorded M_k (CamSwapToMk_B camera.cpp:196-206; scroll or keyboard camera input after S2). Stock refits once per camera change in W3DView::update (0x48BD90) and setCameraTransform (0x48BA87). 0x47D37D is a camera copy (0x534AD0, about 1 KB) plus frustum and shadow-hull math with three small vector allocations (0x47BBBE). It also calls terrain vt+0x234 = 0x4E0F6F, which loops over every terrain block (grid [+0x3890] x [+0x3894], 0xD4-byte records) and calls camera vt+0x208 (a frustum-box test) for each block.
EVIDENCE: From the decompiles of 0x47D37D, 0x534AD0, 0x47C276 and 0x4E0F6F (vtable 0xBE4760 slot 0x234 = 0x4E0F6F, read from the binary). The block count depends on the map and is not known. At about 20-40 ns per box test, 1000-4000 blocks come to about 30-150 µs per refit. cam_a_swaps reached up to about 300 per 10.6 s window (CSV).
IMPLICATION: This is at most about 0.1-0.6 ms per pair while the camera moves (≤0.3 ms of it in the tick iteration). It does not depend on unit count, so it is not what grows over time. It is a possible small optimization only.

## FINDING [possible] Single-slot transform cache: A-render presentation windows cause extra engine transform recalculations
C4_KEY_STUB (stubs.asm:258-271) uses key 2*m_frame - window, and the getter 0x6765B9 has a single slot (docs/analysis/audits/transform_cache_consumers.md, flag F5). Each drawn drawable is therefore recalculated in A window 1 (key 2m-1), possibly again by A code after the window (key 2m), possibly again in scene window 2 (2m-1), and again in B (2m). Stock recalculates once per frame. A recalculation is a lerp plus a Catmull-Rom at the current fraction (0x676688).
EVIDENCE: Audit F5, stub code. Estimate only, not measured: about 0.1-0.3 µs per recalculation x about 140 drawn drawables x 1-2 extra = about 15-80 µs per pair at the end of the session.
IMPLICATION: This is engine work the DLL causes, and it scales with units, but it is two orders of magnitude below the observed growth. Not a cause.

## FINDING [confirmed] The B-render is the large DLL-induced cost per pair, by design
The B-render runs the full drawable pass (S1 jumps to 0x48C701) and the full scene render, UI and Present. All gated A-only systems are skipped (gates in stubs.asm:274-375, Cave_GC_DRAWBLK 379-391). Its CPU time before Present (b_rel) is about 55-75% of the A-render's and grows with drawn units, the same as the A-render.
EVIDENCE: b_rel 1.40 ms (t=102) -> 4.5 ms (t=283) -> 5.0-6.0 ms (t=421-432); a_rel 2.34 -> 6.9 -> 9.0-9.3 ms (CSV). Per tick at the end: 6 x 9.3 (A) + 6 x 5.5 (B) + about 33 (tick) ≈ 122 ms of 198 ms, about 62% average load. The problem is how the work is distributed across iterations, not the total.
IMPLICATION: Rendering twice adds about 6 ms per 33 ms pair at the end, and 4.5-6 ms of that sits in front of the logic tick in the same slot. This is what pushes the tick iteration over 16.5 ms. Making B cheaper would help the tick iteration directly, but that is a design question, not overhead.

## FINDING [confirmed] Telemetry caveats: the site summary prints cumulative averages, and Telemetry=2 would add a full logic checksum per tick
CloseTickWindow adds into g_sums (telemetry.cpp:147-152), which is never reset, so LogSiteSummary (telemetry.cpp:155-172) prints averages since session start. The logged 157.14 / 352.80 / 550.00 for INT_RECOIL are cumulative; the per-interval values are 157 / 433 / 824. Telemetry=2 calls the engine's deep checksum (getCRC 0x625886, telemetry.cpp:389-394) on every tick, which serializes every object. That cost is O(objects) and grows with the battle. This session used Telemetry=1 (log line 2), so it was off.
EVIDENCE: (844 x 352.80 - 245 x 157.14) / 599 = 432.8; (1452 x 550.00 - 844 x 352.80) / 608 = 823.7 (aotr60.log 14:34:59, 14:36:59, 14:39:00).
IMPLICATION: Read the per-tick site rates as interval differences. Never run performance tests with Telemetry=2.

## FINDING [possible] Other per-iteration engine work now runs at 60/s, but it drains queues and the total work is unchanged
GameEngine::update runs the asset streamer [0xDEF548]->vt28 = 0xA37E50 (0x6325B0), the script-debug hooks 0x604189/0x603452, Debug vt94 and the Windows message pump on every iteration, so twice per pair. 0xA37E50 drains work queues under EnterCriticalSection, shared with the streaming thread 0xA36390.
EVIDENCE: Disassembly 0x6325A8-0x6325C9 and the decompile of 0xA37E50; docs/analysis/gaps/CRITIC.md U8 and G6_pacing.md list it as unmeasured.
IMPLICATION: The fixed cost per call is small (a few µs). Calling it more often finishes the same work sooner rather than adding work. Unlikely to matter.

## FINDING [confirmed] Which build ran
The session (14:32-14:40) ran the DLL built at 14:29:45 from HEAD (the phase-6 commit). The working tree now has uncommitted telemetry additions (TelemetryIterationBegin/OnPostRender, pacerWaitTicks, presentCallTicks, owedTicks) and a newer build/Release/dinput8.dll from 14:47. The HEAD files I read (pacer.cpp 372 lines, frame_ctl.cpp 440, telemetry.cpp 497) match the snapshots in hitch\overhead\head\.
EVIDENCE: sha256: rotwk\dinput8.dll = bin\dinput8.dll = 784ab4037e...; build\Release\dinput8.dll = 6a7283eb... (14:47:49). git diff --stat: frame_ctl +2, pacer +4, pacer.h +3, runtime.h +2, telemetry +151.
IMPLICATION: All line numbers above refer to HEAD. The new iteration-timing telemetry in the working tree is the right tool to confirm the split between B-render and logic tick.

## REC [now] Don't spend effort on DLL micro-overhead
The stubs, C5, camera bookkeeping, telemetry and pacer system calls add up to about 25-50 µs per pair, and at most about 0.3 ms while the camera moves (≤1% of 33 ms). None of it grows over time. Look at the logic-tick slot and the B-render cost instead.
RISK: None.

## REC [now] Skip the Present-spacing wait on the B-render that comes right before the logic tick
In OnPreRender, remember whether this Y iteration's stock step will tick: GE+0x34 == 6 at C0 (stepper 0x632601-0x6326C6; the late-skip path is not involved). In AotR60_PresentSkip, do not run the spacing wait (pacer.cpp:336-347) for that B Present. The logic tick then starts right after the B Present. That saves about a_rel - b_rel - 0.25 ms (1.4-2.3 ms in this session's middle phase) of overrun per tick.
RISK: Very low. Only the spacing of that one B Present changes, and the long tick frame follows it anyway. Determinism is not affected because the wait is presentation-only.

## REC [after-diagnostics] Confirm the tick explanation with direct timing before changing the pacer
Use the iteration-timing telemetry already in the working tree, or time GameLogic::update(1) in LogicUpdateWrapper (telemetry.cpp:490) with two QPC calls (about 50 ns). Log per window: logic-tick ms (mean and max), B-render ms in the tick iteration, Present call ms, and how many late releases were tick iterations. Also log the D3D present interval and windowed flag (0xDD302C, D3DPRESENT_PARAMETERS) when 60 mode switches on. This session could not tell vsync from IMMEDIATE, and the vsync-only paths (display query, late B-Present skip) depend on it.
RISK: None. Measurement only.

## REC [after-diagnostics] Pacing changes (pacing area): remove the debt and soften the tick gap
At the end of the session the late time per tick (29-30 ms) was higher than what the pacer can repay (11 x T/8 ≈ 22 ms per tick). Meanwhile each X iteration had about 7 ms of slack and each non-tick Y iteration about 10 ms. Raising the repay rate (for example T/4) or the cap would turn the 350 ms per window of debt back into kept game time. The visible gap at the tick (about tick + A-render ≈ 42 ms with nothing presented) cannot be scheduled away on one thread. Hiding it would need frames queued ahead (vsync plus queue depth) at the cost of about one or two frames of latency. Otherwise, make the B-render cheaper.
RISK: A faster repay shortens later frames and makes their spacing less even. Queuing frames ahead adds input latency and changes how the late B-Present skip and the fallback policy behave.

## REC [optional] Optional: fewer shadow refits during camera swaps
The A swap refits twice per pair (camera.cpp:106-109, 181-183), and the B swap twice more when the camera moved after S2. Each refit walks every terrain block (0x4E0F6F). Options: skip the refit on CamSwapEnd when the next render's W3DView::update will refit anyway, or reuse the A-render's result on B. This saves an estimated 0.1-0.6 ms per pair while scrolling.
RISK: Moderate. The shadow manager state may be read before the next refit, which could show wrong shadows for one frame. Needs a visual check.

## REC [optional] Optional telemetry fixes
Reset g_sums after each LogSiteSummary, or print interval values (telemetry.cpp:147-172). Document that Telemetry=2 runs getCRC on every tick (telemetry.cpp:389-394), O(objects), and must not be used for performance or hitch tests. Optionally open the CSV and log earlier: the first writes after CreateFile(CREATE_ALWAYS) can take about 14 ms (only at startup).
RISK: None.

## OPEN
1. Vsync (fullscreen) or IMMEDIATE? The snapshot has no present-interval column. late_skips=0 fits both: at 120 Hz with vsync the Present queue never fills. If it was vsync, the 1 s EnumDisplaySettingsW path was active. I measured it at about 75 µs mean and 0.5 ms max on the desktop, but not in exclusive fullscreen under DXVK.
2. The logic-tick cost (about 18 -> 33 ms) is inferred from late-release lateness minus b_rel. It assumes most late releases are tick iterations and that the 5 sub-steps without a tick are cheap. Direct timing of GameLogic::update(1) would settle it.
3. The shadow-refit cost depends on the map's terrain block count, which I couldn't determine statically.
4. Does the user's 'regular intervals' mean the 5 Hz tick judder (one 23-39 ms frame every 198 ms), or something slower, every few seconds? The 10.6 s CSV windows can't resolve that. Per-frame timing (the new iteration telemetry) would.
5. There is no 30-mode in-battle baseline in this session, so I couldn't directly compare A-render cost against the stock render or check that the logic tick costs the same as stock. Code reading shows the DLL adds only about 100 ns per logic call, and the transform-cache audit shows logic-side lookups behave as in stock.
6. The benchmark ran on the test PC but outside the game. The game's thread load during big battles, including DXVK pipeline-compile bursts, could oversubscribe the CPU briefly. The pacer's wait only overshoots by milliseconds when all logical CPUs are busy.

## VERIFIER OVERALL
The "dll" analysis is mostly right on its main points. The DLL's own CPU cost is negligible and does not grow over time, and the overrun follows the size of the battle. I re-ran bench.exe on the test PC and got QPC 24.8 ns, GetAsyncKeyState 1.26 us, per-draw stubs 1.43/1.88 ns against 1.38 ns for a plain call, the display query 55 us mean / 0.32 ms max on the desktop, and timer overshoot about 0. It is also right that from t≈283 s the 5 Hz logic-tick iteration overruns its 16.5 ms slot, and that the cap of T/8 repayment per on-time iteration produces the end-phase debt. I re-implemented pacer.cpp:190-273 and 336-347 in verify_dll\psim.py. With a=9.3, b=6 and logic L≈39 ms, it reproduces about 28.8 ms of lateness per tick (observed 29.1-30.3) and 309 ms of debt per 10.6 s (observed 350). With a T/4 repay the debt drops to 0.

What the analysis gets wrong or leaves unproven:
(1) The vsync question it leaves open can be answered. <game folder>\rotwk\game.dat_d3d9.log (mtime 14:40:27, so this session) shows "Windowed: false", "Setting display mode: WxH@0" and "Present mode: VK_PRESENT_MODE_FIFO_KHR", 3 images. The session was exclusive fullscreen with vsync, unlike the earlier windowed IMMEDIATE sessions that "worked perfectly". So the once-per-second display query on the game thread (frame_ctl.cpp:49-81, reached from BlockReason on every render, frame_ctl.cpp:184) was active. Its desktop timing says nothing about cost in exclusive fullscreen under DXVK. The main session has since removed that query in commit 'no display queries in the frame loop; frame-timing stall diagnostics' (14:52) and installed that build.
(2) The tick-cost estimate is internally inconsistent. Late releases exceed ticks by about 10 per 10.6 s window from t=283 on (+1 to +17, mean ≈10), so not all late time sits in the tick iteration. The tick iteration lasts 16.5 + 22.3 = 39 ms (per late release) to 16.5 + 29.1 = 46 ms (per tick). The logic tick is therefore about 33-39 ms, not ≈33 ms. The B Present to A Present gap at the end is about 42-49 ms, not the summary's "23-39 ms frame".
(3) Its claim that the B-render "pushes the tick iteration over 16.5 ms" only holds at the onset. At the end the logic tick alone is more than 2T.
(4) It misses how FIFO presentation turns the DLL's pacing into frame judder (see missed).
Build note: rotwk\dinput8.dll and bin\dinput8.dll now hash to 17a4f8d1... ('no display queries in the frame loop', written 14:51:47). The analysis's "installed = 784ab403" is stale but fits the session's build: the phase-6 commit's bin is 784ab403.
Scratch files: <analysis workspace>/hitch\verify_dll\derive2.py (per-window recomputation) and psim.py (pacer + FIFO 120 Hz display simulation).

- [confirmed] The DLL's own per-render, per-draw and per-tick CPU cost is tiny (stubs about the same as a plain call; QPC, GetAsyncKeyState and the C5 table in the µs range)
  I re-ran overhead\bench.exe noload. QPC 24.8 ns, timeGetTime 9.8 ns, GetAsyncKeyState 1260 ns (2.5 µs per render), stub A/B 1.43/1.88 ns against 1.38 ns for a plain call, C5 at n=800 5.1 µs per pair with 0 drops, C5Reset 2.1 µs. HEAD-snapshot code paths match (pacer.cpp:190-273, frame_ctl.cpp:349-405). CONFIRMED.

- [confirmed] Per-interval per-draw site calls are 157 / 433 / 824 per logic tick (the log prints cumulative averages)
  aotr60.log: INT_RECOIL 60-mode run is 157.14 at 245 ticks (14:34:59), 352.80 at 844 (14:36:59) and 550.00 at 1452 (14:39:00). (844*352.80-245*157.14)/599 = 432.8 and (1452*550-844*352.80)/608 = 823.7. telemetry.cpp:147-152 adds into g_sums and never resets them. CONFIRMED.

- [confirmed] The logic tick runs only in the Y iteration, right after the B-render that saw s==6 (stepper 0x632601-0x6326C6)
  Disassembly: C0/clientUpdate at 0x6325CF comes before the step. At 0x632622-0x632625 s+1 is stored. If it is >6 (0x63264A), ebx=1 is stored at 0x6326BB and vt98(1) is called at 0x6326C6. Otherwise vt98(s) is called at 0x6326F0. The halt hook at 0x6325D5 takes the halted branch after X (PLAN §1.3). CONFIRMED.

- [confirmed] From t≈283 s there are 59-70 late releases per window against about 53 ticks, mean lateness 7.7→13→17→22 ms, and late time per tick goes above the T/8 repay capacity (about 22 ms per tick), producing debt up to 350 ms per window
  derive2.py: late 61/63/60/59/63/66/70/55/64/64/68/59/58/70/67; ms per late release 7.7 (283), 13.0 (336), 17.1 (368), 22.3 (422); late ms per tick 8.9→29.1/30.3; capacity (on-time × 2.0625 / ticks) 21.8-23.6. Debt appears exactly where per-tick lateness exceeds capacity (390: 24.3>21.9 gives 168; 422: 29.1>21.8 gives 350). My re-implementation of pacer.cpp (psim.py) with a=9.3, b=6, L=39 gives 28.8 ms per tick and 309 ms debt per 10.6 s. CONFIRMED (the mechanism is simulated, so INFERRED for the real frame sequence).

- [uncertain] Logic tick cost grows from about 18 to about 33 ms
  It is derived as 16.5 + (mean lateness per late release) - b_rel. Late releases exceed ticks by +1 to +17 per window (mean ≈10), so some late time is not tick-related. Attributing all late time to the tick instead gives 16.5+29.1-6-0.3 ≈ 39 ms. The real value lies between about 33 and 39 ms at the end. One part is now pinned down: late_skips=0 throughout, while FIFO was active and owed was often ≥T in the end phase (pacer.cpp:324-325). That means no Present call blocked >1 ms (pacer.cpp:361), so the Present call is not part of the overrun. INFERRED.

- [refuted] 'So every 198 ms one frame lasts 23-39 ms instead of 16.5 ms' (summary)
  23-39 ms is the length of the tick iteration, not the gap between presented frames. The displayed gap is the B Present, then about 0.3 ms + L of logic, then the late-anchored release, then the A-render (a_rel 9.3), then the A Present: ≈42-49 ms at the end. The analysis's own finding 5 says 25-42 ms. psim.py at the end: 8.3 % of displayed frame intervals are 5-6 vblanks (41.7-50 ms) at 120 Hz FIFO.

- [refuted] Whether the session used vsync is unknown; the display query only runs in vsync mode
  rotwk\game.dat_d3d9.log (mtime 14:40:27, after 60-mode off at 14:40:14, before any later run) shows 'Windowed: false', 'Setting display mode: WxH@0' and 'Present mode: VK_PRESENT_MODE_FIFO_KHR', 3 images. FIFO means PresentationInterval was not IMMEDIATE, so DisplayBlockReason's uncached path (frame_ctl.cpp:61-75: MonitorFromWindow + GetMonitorInfoW + EnumDisplaySettingsW) ran on the game thread once per second during every render's BlockReason (frame_ctl.cpp:184). CONFIRMED that it ran. Its cost in exclusive fullscreen under DXVK was not measured.

- [uncertain] Periodic DLL work (1 s display query, 10 s CSV, 120 s summary) costs well under 1 ms and cannot produce the regular hitch
  The desktop numbers reproduce: query body mean 55 µs, max 0.32 ms. My Log() run had one 13.5 ms outlier, and the 56-line burst max was 14.1 ms, consistent with the analysis's first-writes-after-CreateFile note. But the session was exclusive fullscreen, which the benchmark does not represent. One pattern fits a stall at about 1 Hz that only shows once slack shrinks: from t=283 the late releases in excess of ticks average ≈10 per 10.6 s window. Early on the X slack (≈16.5-a_rel ≈11-14 ms) would absorb a stall of a few ms; at the end it is only ≈6-7 ms. The data cannot prove or exclude this. The main session already removed the periodic query (commit 'no display queries in the frame loop'). Unproven either way.

- [uncertain] The present-spacing wait before the B Present delays the logic tick by about 1.4-2.3 ms in the middle phase; skipping it on the tick B is very low risk
  The code (pacer.cpp:336-347) and the numbers check out: present_wait per B is 1.03-1.21 ms for t=262-326 and 0.01/0.00 at t=422/432. psim mid case: removing the wait cuts lateness per tick 7.6→5.3 ms. But the mean wait is only about half of a-b-0.25 (≈2.2 ms), so the wait was active on only about half of the B Presents. Whether it is active at the tick B is likely but unmeasured. The 'very low risk' rating ignores FIFO: in the psim mid case, removing the wait on every B raises 1-vblank (8.3 ms) displayed frames from 10.7 % to 18.8 %. Removing it only on the tick B moves that one B Present ≈2 ms earlier, which can drop it one vblank early in front of the long tick frame.

- [refuted] The B-render is what pushes the tick iteration over 16.5 ms
  That holds only at the onset (t≈262-283), where B (4.5) plus the wait (≈2) make up most of the ≈8-9 ms overrun. At the end the logic tick alone is about 33-39 ms, more than 2T, so the iteration would overrun even with a free B-render. The B-render then adds only 6 ms to a 45-49 ms gap.

- [confirmed] Raising the repay rate (e.g. T/4) would remove the end-phase debt
  psim.py end case (a 9.3, b 6, L 39, FIFO 120 Hz): T/8 gives 309 ms debt per 10.6 s (3.1 % lost); T/4 gives 0 debt (0.03 %). X/Y slack (≈6-10 ms) covers 4.1 ms of repayment. Side effect in FIFO: slightly more 1-vblank frames (33 %→35 %). INFERRED (simulation).

- [confirmed] The WaitUntil high-resolution timer is accurate and does not depend on timeBeginPeriod; deadlines are absolute
  Re-run: hi-res idle overshoot mean 0.18 µs, max 22 µs; with timeBeginPeriod(1) max 35 µs; plain timer / Sleep(1) mean 3.6-3.9 ms, max ≈14 ms. pacer.cpp:211 adds the interval to the target deadline, not to the wake time. The log has no 'high-resolution timer unavailable' line. timeBeginPeriod(1) is at 0x517A47/0x517A49, 0xA2E9F3 and 0x63A507 (call [0xBD091C]). CONFIRMED.

- [confirmed] Camera-swap shadow refit calls terrain vt+0x234 = 0x4E0F6F
  Reading game.dat with pefile: [0xBE4760+0x234] = 0x4E0F6F. The cost per refit (30-150 µs) is an unmeasured estimate; it does not scale with units. Not a cause.

- [confirmed] No unbounded growth, locks or allocation in per-frame DLL paths; no WARN/ERROR lines
  Fixed-size structures and the owed cap of 6T (pacer.cpp:236-241) check out. aotr60.log has no WARN or ERROR lines, and every telemetry line shows 'drops 0' and '0 errors'. CONFIRMED.

- [uncertain] Build that ran = the phase-6 commit (dinput8 784ab403); installed dll = bin dll
  bin/dinput8.dll at that commit hashes to 784ab4037e..., which fits. But rotwk\dinput8.dll and bin\dinput8.dll are now both 17a4f8d1... (commit 'no display queries in the frame loop; frame-timing stall diagnostics', files written 14:51:47), and the 'uncommitted telemetry' is now committed together with the display-query change. The installed-hash claim is stale. The next test run will use that build: no periodic display query, plus stall logging.

- [confirmed] Fallback did not trigger
  By design: pacer_policy.cpp:5-8 needs lost >5 % (three windows), lateRatio >50 %, or onTime <50 % with lost >0.3 %. The end phase had lost ≈3.2 %, onTime ≈89 % and late >T/4 ≈10 %. PLAN §1.7 still says '0.3 % of wall time' and 'clamped at 1.25·T', which does not match the code (3T clamp, 6T cap). The game silently ran ≈3.5 % slow at t=422 (renders 58.38/s, mframe 29.19).

## VERIFIER MISSED
1. The session was exclusive fullscreen with FIFO vsync. Source: rotwk\game.dat_d3d9.log, written by this session (mtime 14:40:27): Windowed false, FIFO, 3 images, display WxH@0 on a 120 Hz desktop. The earlier sessions that "worked perfectly" were windowed IMMEDIATE, so the analysis compared the wrong presentation path. late_skips=0 throughout, even though the end phase usually had owed ≥ T. So Presents never blocked for more than 1 ms, and the display was not 60 Hz (INFERRED).

2. Under FIFO, the DLL's own pacing gets snapped to vblanks, and the analysis does not consider this. Two mechanisms:
- Once owed > 0, the even-spacing wait is switched off (pacer.cpp:336). A and B Presents then alternate at about 16.5 ± (a_rel - b_rel). At the end that is 16.5 ± 3.3-4.0 ms.
- On-time iterations pull deadlines forward by up to T/8 = 2.06 ms each (pacer.cpp:247-254).
At 120 Hz without VRR, psim.py shows the share of displayed frames that last only one vblank (8.3 ms) growing from 2 % (early load) to 11 % (t≈300) to 33 % (end). That is on top of one 42-50 ms frame per tick (5-6 vblanks). Removing the tick (L=0) brings it back to 3.8 %. This judder is caused by the DLL. It grows with battle load, so it fits "starts after a couple of minutes, in regular intervals". It is INFERRED: unverified if G-SYNC/VRR was active, and DXVK presenter latency is not modelled.

3. The periodic display query was live in this session. From t=283 there are about 10 more late releases per 10.6 s window than ticks (mean ≈10), which fits (but does not prove) a stall of about 1 Hz. Such a stall would be hidden early by 11-14 ms of X-iteration slack and become visible once the slack shrinks to about 6-7 ms. The main session has already removed the query (commit 'no display queries in the frame loop'). The stall log in that build has a 'mode check' part, which will confirm or exclude it.

4. present_spacing_max is 55-210 ms in every window from t=92 on, about 100 ms in most. That means a separate stall class of about 100 ms roughly every 10 s that exists from the start and does not grow. Example: t=103 had 2 late releases, 108 ms in total, 34 ms debt. This is probably the stock hitch class (the 30 FPS baseline lost 1-2.5 %). It is not the regular hitch the user describes. The analysis only touches on it.

5. Data issues in the analysis:
- The estimate for the tick-to-next-frame gap ("23-39 ms") understates it; it is 42-49 ms at the end.
- The logic tick at the end is about 33-39 ms, not 33.
- At the end the B-render is not what puts the tick iteration over budget.
- The PLAN §1.7 thresholds (0.3 % fallback, 1.25·T clamp) do not match the code, so the game silently lost about 3.5 % speed at t≈422 without a fallback.
- Next measurement needed: run the 'no display queries in the frame loop' build fullscreen and per-frame stall lines, and record whether G-SYNC/VRR is on. Then test (a) spacing kept on while owed > 0, or a repay that keeps vblank phase, and (b) T/4 repay.