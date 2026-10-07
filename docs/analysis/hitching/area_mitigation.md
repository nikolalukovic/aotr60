# AREA mitigation

## SUMMARY

The likely cause of the hitching is one logic sub-step per 5 Hz tick that takes longer than its frame slot. INFERRED: no per-sub timing exists yet, but a model fits the data closely (below).

**Why it shows at 60 and not at 30.** The game thread runs in a fixed order: the render, which includes its Present (0x6325CF → 0x522644), then the stepper's logic call (0x6326C6 / 0x6326F0), then the next render. So the gap between the B_k Present and the A_{k+1} Present always contains the whole logic step plus the next A-render up to its Present. That gap is about L + 10 ms.
- At 60 FPS it is visible once L is about 10 ms or more (a hold over 21 ms instead of 16.5 ms).
- Stock 30 FPS hides L up to about 22 ms (33 − a − qA).

**How the data fits.**
- From about t=283 s there are about 1.15 late releases per logic tick, averaging 7.7 ms late. That matches a heavy sub of about 18 ms.
- By t=421 s it is 1.3 late releases per tick averaging 22.3 ms, which matches L ≈ 33-40 ms.
- A replica of pacer.cpp in a simulator reproduces this: 5.05/s × 9.2 ms at the t≈283 inputs, and 5.03/s × 23.4 ms at the t≈421 inputs.
- So late in the battle the screen freezes about 43-50 ms five times per second, followed by catch-up. At that load stock 30 FPS would also show a 44 ms frame per tick and run about 5 % slow.

**Three pacer effects that make it worse (CONFIRMED in pacer.cpp, effect quantified in the simulator).**
1. The Present spacing wait holds the B Present up to release + 9.9 ms. That leaves only about 6.7 ms for the logic step that follows, so every tick is already late from L ≈ 7-8 ms. Without the wait the threshold is about 10 ms.
2. Spacing switches off whenever owed > 0. In the heavy phase present_wait drops to 0, and the A/B Presents alternate by a − b ≈ 3-4 ms (about 11/17 ms).
3. The T/8 repay rate is below the lateness volume. owed gets pinned at the 6T cap and time is lost. Predicted debt is 36/31 ms/s against 32/32 ms/s observed, and the game ran 2.5-9 % slow in the last windows.

**What a pacer can and cannot change (simulator, sub 1 = L = 10/20/30/40 ms).**
- No release rule changes the freeze length. Gentler or no repay only loses game speed: 0.95/0.91/0.87 at L = 20/30/40, slower than stock 30.
- Merge (skip the B Present before the heavy step) makes the freeze about 8 ms longer and drops to 55.6 fps.
- Pre-pay / centring leaves peak-to-peak error unchanged.
- Only a deferred mid-step B Present shortens it: per-tick freeze 30/40/50 → 21/25/31 ms (ideal display), 25/25.5/33 ms at 120 Hz.

**Recommendation.**
1. **Diagnostics now.** A per-iteration timing CSV, stall lines keyed on the Present-to-Present gap with an exact two-iteration breakdown, per-sub logic columns every 10 s, and the real refresh rate in the display line. One session (A1).
2. **Pacer fixes now (fix1), exact.** Budget-capped spacing that also runs while repaying, and repay at max(T/8, owed/6). Effect: no lateness up to L ≈ 10 ms, no A/B alternation, no lost time up to L ≈ 40 ms. The freeze itself is unchanged.
3. **Logic-stall fallback after diagnostics.** Measured, with predictor-based re-entry (StallFallbackMs, default 40 ms). In this session it would have engaged at L ≳ 30 ms, around t ≈ 400 s.
4. **Optional, experimental.** Split the freeze with a deferred mid-step Present (helper thread + D3DCREATE_MULTITHREADED).

**Other candidates.**
- DLL overhead: in fullscreen, DisplayBlockReason queried EnumDisplaySettingsW about once a second in C0. The uncommitted working tree already moves this to on-change only. It is a possible 1 Hz contributor; the check segment will confirm it.
- FIFO/vsync: unlikely (late_skips = 0 throughout).
- Render growth alone: unlikely.
- Isolated ~100 ms engine spikes: present all session, but not the regular pattern.

Simulators and scripts are in <analysis workspace>/hitch\mitig\:
- sim2.py — main model;
- scen.py, repay.py, fallback_eval.py — scenarios;
- csv_model.py — CSV fit;
- sim_hitch.py — first draft.

## FINDING [likely] The data fits one over-budget logic sub-step per tick, growing with the battle (H1)
INFERRED. From about t=262-283 s the late-release rate steps up: 0.61 per tick at t=262-272, then 1.15 per tick at t=283. The mean lateness is small at first (7.7 ms) and grows to 22.3 ms by t=421, while the logic and m_frame rates stay at stock. That is the signature of one Y iteration per tick crossing its 16.5 ms slot.

Model: Y(heavy) = b + spacing hold + Present call (0.3) + qB + L.
- At t≈283 (a 7.0, b 4.5): the simulator with L1 = 18 ms gives 5.05 late/s × 9.2 ms. Measured: 5.75/s × 7.7 ms.
- At t≈421 (a 9.3, b 6.0): L1 = 33 ms gives 5.03/s × 23.4 ms. Measured: 6.42/s × 22.3 ms.
- The extra ~1.4 late/s and the extra debt measured near the end point to either a second heavy sub on some ticks or the ~100 ms spikes.
- At t≈421 that means a freeze of about 43 ms, five times per second, then catch-up. Before t≈260 (0.2-0.4 late/tick) only some ticks crossed, giving irregular hitches of about 25-35 ms roughly every 0.5-1 s. Both fit the report: "after a couple of minutes", "regular intervals".
EVIDENCE: aotr60_rates.csv rows t=262..432: late_releases, late_release_ms, on_time_pct, a/b_rel_present.

<analysis workspace>\hitch\mitig\csv_model.py output: late/tick 0.61 → 1.15 → 1.30; mean late 12.2 → 7.7 → 22.3 ms.

<analysis workspace>\hitch\mitig\scen.py: 't~283 s estimate' and 't~421 s estimate' blocks.

Logic work split per sub (docs/analysis/agents/core.md §0): sub 1 scripts/Lua/commands; sub 2 per-object pass; sub 3/4 bucket 0; sub 5 TheAI + 13 subsystems; sub 6 bucket 3. Which sub is heavy is unknown.
IMPLICATION: The freeze is the game's own logic cost becoming visible at the shorter slot. The diagnostics must give per-sub logic times plus logic frame numbers so the heavy sub, and any period (every tick or every N frames), can be read off directly.

## FINDING [confirmed] The freeze length is set by the loop order; no release/repay rule can shorten it
CONFIRMED from the code and the binary, quantified in the simulator.

GameEngine::update 0x6325A0 calls clientUpdate at 0x6325CF. That is the render, whose only Present sits at 0x522644 inside DX8Wrapper::End_Scene. Only after the render returns does the stepper run GameLogic::update(sub), via vt98 at 0x6326C6 (sub 1) or 0x6326F0 (subs 2..6).

So the span between the B_k Present and the A_{k+1} Present always contains, in sequence:
Present call + B post-Present work + L_j + post-step work + pacer + pre-C0 work + A_{k+1} render up to its Present.
That is about L + 10.1 ms with a = 9.3. The simulator confirms it: 30.1 / 40.0 / 48.0 ms at L = 20 / 30 / 38, against L + 10.1 = 30.1 / 40.1 / 48.1.

Stock 30 hides L as long as a + pc + qA + L ≤ 33, i.e. L ≤ ~22 ms. At 60 the hold exceeds 1.25 slots (21 ms) from L ≈ 11 ms (simulator L-sweep: 20.2 ms at L=10, 22.1 at 12, 26.1 at 16). At 120 Hz a 25 ms frame already appears at L ≈ 8-10 with the current pacer.
EVIDENCE: Disassembly 0x6325A0..0x632708: clientUpdate at 0x6325CF; halt 0x6325D5; logic sub 1 via vt98 at 0x6326C6; subs 2..6 at 0x6326F0.

End_Scene Present at 0x522644/0x522650 (docs/analysis/gaps/G6_pacing.md).

<analysis workspace>\hitch\mitig\fallback_eval.py: predicted vs simulated freeze.

scen.py L sweep.
IMPLICATION: Option (B1) (gentler or no repay) and option (B2) (release earlier, pre-pay, phase shift by release timing) cannot remove the freeze. The only levers are:
- fewer frames (a 30 FPS fallback);
- presenting a frame during the logic step (deferred Present);
- less work in that span, which is not available without breaking the A = stock-render rule.

## FINDING [confirmed] The Present spacing wait eats the budget of the logic step that follows it
CONFIRMED in the code; effect INFERRED via the simulator.

When owed == 0, AotR60_PresentSkip holds every Present to lastPresent + 16.25 ms, capped at release + 0.6·T = 9.9 ms (pacer.cpp:339-350 in the working tree). The A Present comes about a after its release and the B render is cheaper (b < a). So each B Present, which is followed by the logic step, is held to about release + a − 0.25.

The Y iteration's logic budget becomes T − (a − 0.25) − pc − qB ≈ 6.7 ms (a = 9.3). Without the hold it would be T − b − pc − qB ≈ 9.7 ms.

Simulator L-sweep (a 9.3, b 6.0):
- current pacer: late on every tick from L = 8 ms (4.97/s; 3.90/s at L=7);
- budget-capped spacing: late from L ≈ 10-11 ms (0.45/s at L=9, 3.35/s at L=10, 4.92/s at L=11).

In the session's moderate phase (t=145-272, 0.2-0.6 late per tick) this hold turned near-misses into late releases. Each one set owed > 0 and switched spacing off.
EVIDENCE: src/pacer.cpp:339 `if (g_cfg.presentPacing && paced && g_owed == 0 && ...)`; :341 `latest = g_releaseTime + g_halfPair * 6 / 10`.

<analysis workspace>\hitch\mitig\scen.py, 'L sweep' block.
IMPLICATION: Cap the spacing hold by the predicted remainder of the iteration (fix1). This is free, exact, and lifts the visible threshold from about L ≈ 7-8 ms to about 10 ms.

## FINDING [confirmed] Spacing is off whenever owed > 0, so A/B Presents alternate by a − b in heavy phases
CONFIRMED in the code and the data.

The spacing wait requires g_owed == 0 (pacer.cpp:339). In the heavy phase owed is never repaid to 0: present_wait_ms falls from 300-650 ms per 10 s window (t ≤ 262) to 10-36 ms (t=347-410) and to 0.2 / 0.0 ms at t=421 / 432.

Without spacing, A Presents land at release + a and B Presents at release + b. The cadence alternates by a − b = 3.3-4 ms (a_rel 9.0 / b_rel 5.0 at t=421), on top of the T/8 repay compression.

The simulator's current-pacer cycle at L=30 shows exactly that: B6 39.2 ms freeze, then A 12.3 / B 17.0 / A 12.1 / B 17.3 / A 11.5 / B 17.6 / A 10.9 ... for the whole tick. With the fix: 12.9 / 14.3 / 13.9 / 15.2 / 14.2 / 14.9 / 14.2. At 120 Hz the alternation quantises into stray 8.3 ms and 25 ms frames.
EVIDENCE: aotr60_rates.csv present_wait_ms column vs a_rel/b_rel_present_ms.

src/pacer.cpp:339.

<analysis workspace>\hitch\mitig\sim2.py ideal-display cycles.
IMPLICATION: Keep spacing active while repaying, targeting the release cadence and capped by the budget (fix1). It is a secondary judder source, not the main hitch.

## FINDING [confirmed] Repay capacity (T/8 per on-time iteration) is below the lateness volume, so time is lost
CONFIRMED from data and model.

The repay capacity is on-time iterations × T/8: 107 ms/s at t=421 and 108 ms/s at t=432. Lateness volume is 143 and 139 ms/s. The excess pins owed at the 6T cap (pacer.cpp:236), and the overflow is written off as debt:
- predicted 36.0 / 30.7 ms/s, measured 32.1 / 32.1 ms/s;
- also at t=389: predicted 12.1, measured 15.7.

Result: logic ran at 4.93 and 4.59 ticks/s against 5.05 in the last two full 60-mode windows.

Simulator, current pacer at L=40: speed 0.963, debt 36 ms/s. With repay ≤ max(T/8, owed/6): speed 0.9999, debt 0. Repay rate never changes the freeze or the jump (repay.py, all variants: freeze 30.2 / 40.1 / 50.1 ms at L = 20 / 30 / 40).
EVIDENCE: <analysis workspace>\hitch\mitig\csv_model.py (columns repayCap/s, debt/s, pred_debt/s).

<analysis workspace>\hitch\mitig\repay.py.

src/pacer.cpp:213 (clamp 3T), :236 (cap 6T), :248 (pay ≤ T/8).
IMPLICATION: A faster, owed-proportional repay keeps exact long-run speed at no visual cost relative to the freeze. Repaying more gently is not viable (see the next finding).

## FINDING [confirmed] Rejected: gentler or no repay, merge (skip the B before the heavy step), pre-pay / centring
CONFIRMED in the simulator (sim2.py, repay.py; ideal display; a 9.3, b 6.0, qA 1.5, qB 0.5).

**(1) Gentler or no repay.** The freeze and the jump are unchanged.
- No repay: speed 0.950 / 0.908 / 0.868 at L = 20 / 30 / 40. That is slower than stock 30 itself (0.9999 / 0.960 / 0.916), because 60 mode loses more per heavy tick.
- T/16: 0.957 at L=30, 0.913 at L=40.

**(2a) Merge (B Present skipped, heavy Y released right after its X).**
- Freeze longer: per tick 38.1 / 48.2 / 58.1 ms vs 30.2 / 40.2 / 50.1.
- Post-freeze jump smaller: 5.1 / 15.1 / 25.1 vs 13.6 / 23.6 / 33.6.
- fps 55.6.
- Worse with irregular heavy ticks (periodic case: p2p 42.4 vs 33.6).

**(2b) Release earlier / pre-pay with centring.** Moves the compression ahead of the freeze. Peak-to-peak position error is unchanged: L=20 16.6 vs 16.0 ms; L=30 27.2 vs 26.9. The jump is identical.

**Full B-render skip.** Gives the same freeze as now: hold ≈ pc + qA + L + a. The B pass fills the transform cache that logic-side FX consumers read; without it they would recompute at fraction (k+1)/6, giving FX offsets (docs/analysis/audits/transform_cache_consumers.md). It needs its own audit.
EVIDENCE: <analysis workspace>\hitch\mitig\sim2.py output: strategies cur/fix1/fix2/merge/split.

<analysis workspace>\hitch\mitig\repay.py.

<analysis workspace>\hitch\mitig\scen.py, periodic case.
IMPLICATION: Do not spend effort on these. The predictive / tick-shaping options from the brief do not reduce what the user perceives as a hitch.

## FINDING [likely] Only a deferred mid-step B Present shortens the freeze (about halves it)
Gains CONFIRMED in the simulator; feasibility INFERRED.

If the B Present before a predicted-heavy step is issued at the midpoint between the A_k Present and the predicted A_{k+1} Present, while the main thread runs the logic step, the freeze splits into two holds.

| L (ms) | Freeze per tick now | With split, ideal display | With split, 120 Hz |
|---|---|---|---|
| 20 | 30.2 | 20.8 | 24.7 |
| 30 | 40.2 | 25.2 | 25.5 |
| 40 | 50.1 | 31.2 | 33.2 |

At the t≈421 estimate it cuts holds over 40 ms from 4.9/s to 0. With two heavy subs (25 + 15 ms) the per-tick freeze drops from 34.8 to 25.0 ms.

It needs a Present outside the main thread's sequence. Options:
- a helper thread with the device created MULTITHREADED. CreateDevice at 0x5241B6 takes its behaviour flags from [0xDD345C], loaded at 0x524190 `mov ecx,[0xDD345C]`. A 6-byte site could OR in 0x4.
- a mid-logic main-thread hook, which needs an x87/D3D audit.

Presentation only, so logic stays exact. It is the realisation of "shift the present phase so the long gap is split evenly". It degrades gracefully when the prediction is wrong: if the step ends early, present at the next C0.
EVIDENCE: <analysis workspace>\hitch\mitig\sim2.py, strategy 'split' (ideal and vb120).

<analysis workspace>\hitch\mitig\scen.py.

Disassembly 0x524190..0x5241B6: push [0xDD345C] as BehaviorFlags, params 0xDD2FF8, device 0xDD3474.
IMPLICATION: This is the only route to 60 FPS without visible freezes for L between about 20 and 45 ms. It should be an experimental option after diagnostics, default off.

## FINDING [possible] DLL overhead candidate (fullscreen): a once-per-second display query in C0, already being changed in the working tree
CONFIRMED in the committed code; impact unverified.

The committed DisplayBlockReason ran MonitorFromWindow + GetMonitorInfoW + EnumDisplaySettingsW about every 1000 ms (GetTickCount) in OnPreRender. It did so only when the presentation interval was not IMMEDIATE, i.e. fullscreen/vsync. That matches "windowed sessions worked perfectly".

It cannot explain the 5 Hz pattern, and early windows show < 1 late release/s, so its cost fit inside the early slack. Once the slack is gone it adds about 1 Hz lateness.

The uncommitted working tree (src/frame_ctl.cpp DisplayBlockReason, about lines 46-100) now evaluates only when the present parameters change. Its comment asserts the queries "coincided with a hitch about once a second"; I could not verify that from the 10 s CSV.

The engine's FullScreen_RefreshRateInHz is always 0: the only writer is 0x524B62 `mov [0xDD3028],ebp` with EBP = 0. So the new code's fullscreen fast path falls through to one EnumDisplaySettingsW per change, which is correct.
EVIDENCE: git diff src/frame_ctl.cpp (working tree, not committed).

Ghidra listing line 367432: 00524b62 MOV [0x00dd3028],EBP — the only reference.

docs/analysis/gaps/G6_pacing.md: "FullScreen_RefreshRateInHz is 0".
IMPLICATION: Keep the on-change evaluation. The per-iteration 'check' segment (C0 entry → end of OnPreRender) in the new telemetry will show whether any 1 Hz cost remains.

## FINDING [unlikely] FIFO/vsync interplay and render-cost growth alone do not fit; isolated engine spikes are a separate, stock-like component
INFERRED.

**FIFO/vsync (H4).** late_skips = 0 and presents_s = renders_s in every window. The skip condition (owed ≥ T behind the absolute schedule) was met often in late windows, so no Present had blocked for more than 1 ms just before (presentsSinceBlock < 2 never held), or the session was IMMEDIATE. 60.6 Presents/s on a 120 Hz 3-image FIFO never fills the queue. Present call time was not logged.

**Render growth alone (H2).** An X iteration is about a + pc + qA ≈ 11 ms < 16.5 ms even at a = 9.3. H2 would produce up to ~6 small late releases per tick; the data shows ~1.2 per tick averaging 22 ms. Render growth still matters indirectly: a sits inside the freeze (L + qB + pc + a) and shrinks every budget.

**Spikes (H3).** present_spacing_max is 55-210 ms in every window, including t=92-134 when hitching was not reported. These are 1-3 spikes of about 100 ms per 10 s, like the stock 100-260 ms hitches. In the simulator, 100 ms spikes every 5 s cost about 1 % speed and are untouched by any option.
EVIDENCE: aotr60_rates.csv: late_skips and forced_skips all 0; presents_s == renders_s; present_spacing_max_ms per window.

src/pacer.cpp:327 skip condition.

scen.py: 'L1 20 + random 100 ms spikes' block.
IMPLICATION: The diagnostics should still log the Present call time (blocked > 1 ms), the presentation interval and the real refresh rate to rule FIFO out. The stall breakdown separates spikes (pre-C0 / post / Present) from logic.

## FINDING [confirmed] The working-tree telemetry covers most of (A) but has gaps that would leave the cause ambiguous
CONFIRMED in the code: uncommitted diff in telemetry.cpp, pacer.cpp/.h, frame_ctl.cpp, runtime.h. It adds TelemetryIterationBegin/OnPostRender, 10 s timing columns, stall lines and a display line. Gaps:

1. Stall criterion is iteration length > 1.5 slots (telemetry.cpp:372), not the visible Present-to-Present gap. A heavy Y at L ≈ 18 (Y ≈ 23-25 ms) sits at the threshold although the visible gap is about 26-28 ms. A gap spans two iterations, so one line cannot decompose it.
2. 'render' lumps the pre-Present render, the spacing wait, the Present call and the post-Present client work (InGameUI::update runs after the draw, G1). The A-render's critical pre-Present part cannot be told apart.
3. 'other' lumps release→C0 (execute loop head, asset streamer 0x6325B0, script-debug bridge 0x6325B9/0x6325C4) with post-step work (Debug vt94, Win32GameEngine::update tail 0x441827: IsIconic + vt5C message pump, execute loop tail 0x63A001..0x63A194).
4. Logic is kept only as sub 1 mean/max plus the max of the other subs. Per-sub 1..6 mean/max is needed to find the heavy sub (sub 5 = TheAI + 13 subsystems is a prime suspect).
5. The display line prints refresh from 0xDD3028, which is always 0 (0x524B62), so it will log "refresh 0 Hz".
6. No Present timing in 30 mode: PRESENT_STUB (stubs.asm:774) calls the C++ hooks only when g_m60.
7. 600-line log cap; no per-iteration record, so periodicity over logic frames cannot be computed.
EVIDENCE: git diff of src/telemetry.cpp: TelemetryIterationBegin, stall Log, display Log reading 0xDD3028.

src/stubs/stubs.asm:774-800.

0x441827..0x4418C2 Win32GameEngine::update tail (decompile: IsIconic loop, vt5C).
IMPLICATION: Extend it as specified in recommendation A1 so one session separates H1 (which sub, periodicity) from H2/H3/H4/H5.

## REC [now] A1. Diagnostics: per-iteration timing CSV + stall lines keyed on the Present gap (one session pins the cause)
Hook points: existing C++ entry points only, plus one optional stub change. All QPC; main thread only; Telemetry ≥ 1.

**Timestamps and segments.**
- **AotR60_Pacer** (pacer.cpp:190): t_in at entry. This closes iteration n. Record: post = t_in − max(t_postRender, t_logicEnd); late; pay; owed after the decision; wait (pacerWaitTicks delta); t_rel = g_releaseTime (pacer.cpp:266). post covers Debug vt94, IsIconic + vt5C message pump (0x441827..0x4418C2) and the execute loop tail.
- **OnPreRender** (frame_ctl.cpp:371): t_c0 as the very first statement, so pre = t_c0 − t_rel (loop head, asset streamer 0x6325B0, debug bridge 0x6325B9/0x6325C4). t_chk at the end (TelemetryOnPreRender), so check = mode controller. Also record kind A/B/S, s = GE+0x34, logic frame GL+0x40, m_frame GC+0x10, next sub j = (s==6 ? 1 : s+1).
- **AotR60_PresentSkip** (pacer.cpp:298): t_pin at entry gives r1 = t_pin − t_chk (render up to Present). After WaitUntil: spacing = g_presentCall − t_pin. Record the skip flag.
- **AotR60_PresentDone** (pacer.cpp:357): t_pout, giving pc = t_pout − g_presentCall, blocked = pc > 1 ms, and gap = g_presentCall − previous g_presentCall (submission gap; skipped Presents excluded).
- **OnPostRender** (frame_ctl.cpp:430): t_post, giving r2 = t_post − t_pout (rest of drawFrame, InGameUI after the draw, audio).
- **LogicUpdateWrapper** (telemetry.cpp:597) and **LwLogicUpdateWrapper** (:565): per-call duration, sub, frame at entry, t_logicEnd. Already partly there.
- **30 mode** (optional): add a `cmp byte ptr [g_frameLog],0` branch in PRESENT_STUB (stubs.asm:774) calling timing-only hooks, so the stock render's r1/pc/r2 are measured too. Without it, 30-mode render is r1+pc+r2 combined and the wait comes from [0xDE4310] in whole ms.

**Outputs.**

1. aotr60_frames.csv, one row per iteration. Integers in µs: t_rel_ms(0.01), rid, mode, kind, s, lframe, sub(0 = halted), pre, chk, r1, spacing, pc, r2, logic, lw, post, wait, late, pay, owed, gap, flags (skip | blocked | gap | late | fallback). About 70 bytes per row → 4.3 KB/s, about 16 MB/h. Use a 64 KB buffer flushed when full and at each 10 s Report; stop at 200 MB.

2. Stall line in aotr60.log when gap > 25 ms (60) or > 50 ms (30). It decomposes the gap exactly:
   prev[pc + r2 + logic(sub, frame, LW) + post] + pacer[wait or late, owed] + this[pre + chk + r1 + spacing]
   The parts sum to the gap, which self-checks. Example: `stall 43.2 ms t=421.3s: prev B6 pc 0.2 + r2 0.5 + logic 31.0 (sub 1, frame 2104) + post 0.4 | late 13.1 owed 41.0 | this A1 pre 0.3 + chk 0.1 + r1 9.4 + spacing 0.0`. Log all of the first 300, then 1 in 10 plus every gap > 50 ms.

3. 10 s CSV, appended columns:
   - gap_max, stalls25, stalls50, stall_ticks (ticks whose largest gap > 25 ms);
   - logic_mean_s1..s6, logic_max_s1..s6, lw_max;
   - A r1 mean/max, B r1 mean/max, A r2 mean/max, B r2 max;
   - pc_max, blocked_presents;
   - pre_max, chk_max, post_max;
   - x_iter_max, y_iter_max;
   - owed_mean/max, pay_ms, spacing_waits.

4. At every mode-on, one line with:
   - WxH (0xDD2FF8/0xDD2FFC), Windowed [0xDD3018], SwapEffect [0xDD3010], BackBufferCount [0xDD3004], PresentationInterval [0xDD302C];
   - the real monitor refresh: MonitorFromWindow([0xDD3014]) + EnumDisplaySettingsW, or reuse DisplayBlockReason's value — not 0xDD3028, which is always 0;
   - the d3d9.dll path (GetModuleHandleW + GetModuleFileNameW: rotwk\d3d9.dll = DXVK).
   Only on mode change, never per frame.

Overhead: about 10 QPC reads (~0.3 µs) plus one row format (~2 µs) per iteration. The stall Log is unbuffered WriteFile at ≤ 5/s.

**Reading the next session.**
- H1: stall component = logic with a fixed sub; the freeze tracks L + qB + pc + a; the same L in the 30-mode stretches. Per-tick max logic vs logic frame gives the period.
- H2: r1/r2 dominate.
- H3: pre/post/pc spikes.
- H4: blocked Presents before gaps, gaps on vblank multiples.
- H5: chk spikes about 1 s apart.

Session protocol: same large AI skirmish, fullscreen as normally played, Telemetry=1, at least 8 min. After the hitching starts, press Ctrl+Shift+F11 to 30 for about 20 s and back, twice (gives the 30-mode L and the stock render cost for the same battle state).
RISK: Negligible CPU and file size. Only one optional stub edit (the 30-mode Present timing branch). Log/CSV writes stay off the per-frame path except one buffered row.

## REC [now] B1. Pacer hygiene (fix1): budget-capped Present spacing that also runs while repaying, plus owed-proportional repay
Exact; long-run speed unchanged; touches only pacer.cpp.

**(a) Spacing.** In AotR60_PresentSkip, replace the `g_owed == 0` spacing block (pacer.cpp:339-350) with a block that runs when presentPacing && paced && the iteration was not released late && lastPresent and prevRelease are set:
- target = lastPresent + (g_releaseTime − g_prevReleaseTime) − 0.25 ms. This is the release cadence: T, or T − pay while repaying.
- target = min(target, g_releaseTime + 0.6·T).
- target = min(target, g_deadline + g_interval − pc_est − tail_est − 0.5 ms), where g_deadline + g_interval is the point the next pacer call compares against.
- Estimates:
  - tail_est for an A: EWMA(1/8) of A (Present return → pacer entry);
  - tail_est for a B: second-largest of the last 8 measured (Present return → pacer entry) of Y iterations that ran the same next sub j (j = s+1, or 1 when s = 6, from GE+0x34 at C0);
  - pc_est: EWMA of the Present call.
- Wait if target > now.

This gives no hold on a B whose step is predicted heavy, and no A/B alternation while owed > 0. The FIFO late-skip stays as is.

**(b) Repay.** In the on-time branch (pacer.cpp:244-257): pay = min(owed, max(interval/8, owed/6), −late). The 3T clamp and 6T cap are unchanged.

**Effect (simulator, a 9.3 / b 6.0):**
- late releases at L = 9: 5.05/s → 0.45/s; at L = 10: 5.05/s × 3.4 ms → 3.35/s × 0.6 ms;
- at 120 Hz, the 25 ms hold per tick at L=10 disappears (B6 16.7 ms);
- post-freeze cadence L=30: 12.3/17.0 alternation → 12.9-15.2 even;
- L=40 speed 0.963 (debt 36 ms/s) → 0.9999 (debt 0). That matches the observed 32 ms/s debt at t ≥ 421.

The freeze per tick is unchanged (≈ L + 10 ms). Unit-test the new rules in tests/ against the simulator cases in <analysis workspace>\hitch\mitig\sim2.py ('fix1').
RISK: Low.
- The predictor (P80 of the last 8 per sub) can under-predict a sporadic heavy tick. That only lets a spacing hold happen, as today.
- Faster repay compresses a few frames after a freeze: 11-14 ms instead of 14.4 ms.
- No game-state effect.

## REC [now] B2. Keep the display-mode check out of the per-frame path (already in the working tree)
Keep the uncommitted DisplayBlockReason change: evaluate only when {interval, windowed, refresh, hwnd} change, never once a second. Make the new 10 s chk_max column (A1) confirm that no periodic cost remains in C0. Note: [0xDD3028] is always 0 (sole writer 0x524B62), so fullscreen always takes the single EnumDisplaySettingsW fallback, which is fine. Reuse its result for the telemetry display line.
RISK: None. If the display changes refresh while hwnd and the present parameters stay the same, the cached block reason is stale until the next device reset (which resets the parameters).

## REC [after-diagnostics] B3. Adaptive quality: logic-stall fallback rule with measured re-entry
Add to PacerWindow (pacer_policy.h): stallTickRatio = ticks in the 5 s window whose largest Present submission gap > StallFallbackMs ÷ ticks. Ticks are counted in LogicUpdateWrapper on sub 1; the gaps come from PresentDone.

**Leave 60.** In FallbackPolicy::AddWindow: stalled = stallTickRatio > 0.5; stallWindows_ ≥ 2 → fall back, using the existing backoff (30 s doubling to 480 s).

**Return to 60.** While in fallback (30 mode), keep measuring:
- L_j via LogicUpdateWrapper (mode-independent);
- the stock render up to Present a_est (needs the 30-mode Present timing from A1);
- qB_est and pc_est carried over from the last 60 stretch.

Per window: freeze60_pred = P80 over ticks of max_j(L_j + qB_est) + pc_est + a_est. In the model this predicts the simulated freeze within 0.1 ms (fallback_eval.py). PacerFallbackActive() stays true until the backoff has expired AND freeze60_pred < StallFallbackMs − 8 ms in the last 2 windows. Log the numbers at both transitions.

**Setting.** StallFallbackMs = 40 (0 = off). 40 ms is about 2.4 slots.

**Quantified.**
- Fraction of ticks over 40 ms with fix1 (vb120): 0.13 / 0.36 / 0.57 / 0.79 / 0.94 at L = 24 / 26 / 28 / 30 / 32. So the rule engages at L ≳ 28-30 ms, i.e. from about t ≈ 400 s in this session.
- Trade-off: at that load stock 30 also shows a 41-44 ms frame per tick and runs 4-5 % slow (stock30 L=30: speed 0.960; L=33: 0.947). The fallback swaps a 43 ms freeze among 16.5 ms frames (jump ~26 ms, then catch-up) for a 44 ms frame among 33 ms frames with no catch-up, at a slower game.
- If B4 is enabled, the freeze halves and this rule would only trigger at L ≳ 50 ms.

The existing rules (overloaded / slow / sustained / skipping) never fire here: late ratio ≈ 10 %, lost ≈ 3 %.
RISK: Low.
- Switching happens at pair boundaries, as today (exact).
- Risk of flapping near the threshold, handled by the 8 ms hysteresis, 2-window confirmation and backoff.
- Needs A1's 30-mode Present timing for a good a_est; otherwise use C0 → OnPostRender minus the last 60-mode A r2.

## REC [optional] B4. Experimental: split the freeze with a deferred mid-step B Present (SplitStallPresent=0 by default)
Condition: when the coming step is predicted heavy, i.e. b_est + pc + tail_est[j] > T + 4 ms (B1 estimators).

1. AotR60_PresentSkip returns 'skip' for that B and sets g_deferredPresent with t_target = (lastPresent + predA)/2, where predA = max(g_deadline + g_interval, now + pc + tail_est[j]) + a_est.
2. A helper thread, created lazily at the first 60-mode C0 (never in DllMain), waits on a high-resolution waitable timer until t_target. Then InterlockedExchange(g_deferredPresent, 0) and, if it was set, calls IDirect3DDevice9::Present(NULL×4) on [0xDD3474]. A D3DERR_DEVICELOST result is recorded into g_gapDevLost.
3. OnPreRender of the next iteration: if the flag is still set (the step ended early), present synchronously first.
4. Reset_Device (GAP_RESET_CAVE, 0x522000) and LeaveSixty must drain or cancel the pending Present.
5. Requires D3DCREATE_MULTITHREADED: a new 6-byte site at 0x524190 (`mov ecx,[0xDD345C]`) → stub that ORs 0x4 into the behaviour flags pushed to CreateDevice (0x5241B6), written in DllMain like every other site. Alternative: a main-thread mid-logic hook, which needs an x87-control-word and D3D-state audit.

Presentation only: content and logic are unchanged, so exactness is unaffected.

Gain (simulator): per-tick freeze 30.2 / 40.2 / 50.1 → 20.8 / 25.2 / 31.2 ms (ideal) and 30.0 / 40.1 / 50.3 → 24.7 / 25.5 / 33.2 ms at 120 Hz, for L = 20 / 30 / 40; holds over 40 ms at the t≈421 estimate go from 4.9/s to 0.
RISK: Medium-high engineering risk:
- D3D9/DXVK multithreading (the MT lock costs an estimated 0.1-0.3 ms per frame);
- device-lost/reset races and alt-tab;
- AotR or other hooks on the device;
- a wrong prediction makes the split uneven (it degrades to today's behaviour or a short B frame).

It must not change any game-visible state. Ship it only after A1 confirms H1 and only behind the setting; test with DXVK on and off, fullscreen and windowed.

## REC [after-diagnostics] B5. Settings and telemetry to expose
**Settings.**
- StallFallbackMs = 40 (0 = off) for B3.
- SplitStallPresent = 0 (experimental) for B4.
- The per-iteration CSV under Telemetry ≥ 1 (or a separate FrameLog = 1 if Telemetry=1 sessions must stay small).
- Keep the repay divisor (6) and the spacing margins internal constants, unit-tested.

**Title-bar status.** Show 'logic-bound fallback' distinctly from 'performance fallback'.

**Per-10 s CSV columns to keep permanently** (cheap and diagnostic):
- gap_max, stall_ticks;
- logic_max_s1..s6;
- A/B r1 mean;
- owed_max, debt, pay_ms.

**Log.** A one-line summary per 120 s: heaviest sub, its P80/max, the per-tick freeze P80, and freeze60_pred while in fallback.
RISK: None.

## OPEN
1. **Which logic sub is heavy, and is it every tick or every N logic frames?** The per-sub columns and the frame CSV will show it. Sub 5 (TheAI + 13 subsystems) and sub 1 (scripts, Lua, commands, LW logic) are the prime suspects. A period of N frames would also make a frame-number predictor better than "P80 of the last 8".
2. **Is the logic step slower in 60 mode than in 30?** Twice the renders could mean colder caches. The F11 toggle stretches in the next session answer this.
3. **Was this session fullscreen FIFO at 120 Hz or windowed IMMEDIATE?** It was not logged. late_skips = 0 suggests no blocking Presents either way.
4. **How much of the 1 Hz C0 display query, which the working-tree comment links to hitches, remains after the on-change rewrite?** The chk segment answers this.
5. **What does the user prefer once L ≳ 30 ms?** A 30 FPS fallback that still shows a 44 ms frame per tick and runs about 5 % slow, or 60 FPS with a 43 ms freeze per tick? This sets the StallFallbackMs default.
6. **Is the deferred-Present experiment (B4) acceptable risk?** It needs D3DCREATE_MULTITHREADED with DXVK and a helper thread. On the device side: does AotR or anything else hook IDirect3DDevice9 in this install?
7. **Where does the B-render's cost go?** The optional timing at the B drawable pass (S1/CamSwapToMk_B) vs the scene render (SceneRestore, 0x44A23E) would tell whether a cheaper B (drawable pass only) is worth auditing for exactness. It would reduce CPU load (H2), not the freeze.
8. **What is in the residual lateness?** The end-of-session data shows about 1.4 more late releases/s and more debt than a single 33 ms sub explains: a second heavy sub on some ticks, or the ~100 ms spikes. The stall breakdown will separate them.
9. **Caveat on the model.** All quantitative conclusions rest on the simulator (<analysis workspace>\hitch\mitig\sim2.py), with a, b, qA and qB fitted from the a/b_rel columns; qA, qB and the Present call time are assumed (1.5 / 0.5 / 0.3 ms) and must be replaced by the measured values.

## VERIFIER OVERALL
The analysis is largely sound on structure and code, but it overclaims in several places. Recomputed or re-run: the CSV rates, the loop order in the binary, the pacer code (HEAD, which is the 'no display queries in the frame loop' commit, and the session build, which is the Living World phase-6 commit), sim2.py, scen.py and repay.py, plus my own variants in <analysis workspace>\hitch\mitig_verify\ (v1.py, v2.py, v3.py, sim2p.py).

What holds:
- The loop order puts the whole logic sub-step between the B_k Present and the A_{k+1} Present, so the freeze is about L+qB+pc+a. CONFIRMED in the binary (0x6325CF, then 0x6326C6 / 0x6326F0).
- No release or repay rule can shorten that freeze. CONFIRMED in the simulator.
- The late-release statistics are correct: 0.61 / 1.15 / 1.30 late releases per tick, averaging 12.2 / 7.7 / 22.3 ms.
- The spacing hold before the B Present eats the budget of the logic step that follows it. CONFIRMED at pacer.cpp:339-341.
- 0xDD3028 is always 0. CONFIRMED.
- The existing fallback rules never fire here. CONFIRMED at pacer_policy.cpp:5-8.

What is wrong or overclaimed:
- The present_wait trend mixes units and overstates the drop. The spacing wait was still about half active at t=347-410.
- The repay-cap explanation of the lost time is marked CONFIRMED but is not reproduced by the analysis's own simulator at the fitted inputs (2.1 vs 32 ms/s).
- The "9 % slow" figure comes from a window that contains a game pause.
- The split gains are oracle upper bounds.
- The proposed MULTITHREADED patch misses a second CreateDevice call.

H1 (one heavy logic sub per tick) stays a plausible INFERRED hypothesis. The data does not single it out over high-variance or heavy A-renders. The 5 % jitter model contradicts the measured present_spacing_min. The session ran the phase-6 build ('60 FPS on the Living World strategic map'), and the stall lines of 'no display queries in the frame loop; frame-timing stall diagnostics' (committed at 14:52, after the session) already log the logic sub and frame, so one more session with the current build answers "which sub" without A1.

- [confirmed] Loop order: the render with its Present (0x6325CF -> 0x522644) runs before the logic sub-step (vt98 at 0x6326C6 for sub 1, 0x6326F0 for subs 2..6). So the B_k->A_{k+1} gap is about L+qB+pc+a, and no release or repay rule shortens it.
  CONFIRMED.

- Disassembly of 0x6325A0..0x632708: `call [eax+0x9C]` at 0x6325CF, then the s>6 wrap with `push ebx`(=1) and `call [eax+0x98]` at 0x6326C6, or `call [edx+0x98]` at 0x6326F0.
- Re-running scen.py reproduces the per-tick hold ~L+10 (30.2 / 40.2 / 50.1 at L=20/30/40).
- Holding the B Present only delays the logic on the same thread, so it moves the gap rather than shrinking it.

- [confirmed] Recomputed rates: late per tick 0.61 (t=262) -> 1.15 (t=283) -> 1.30 (t=421); mean lateness 12.2 -> 7.7 -> 22.3 ms; 5.75/s and 6.42/s; a_rel 2.3->9.3, b_rel 1.4->6; INT_RECOIL 157 -> ~820 per tick.
  CONFIRMED from aotr60_rates.csv.

- 33/(5.083*10.6)=0.61, 61/(5.018*10.6)=1.15, 70/(4.926*10.9)=1.30.
- 402.6/33=12.2, 472.7/61=7.75, 1561.6/70=22.3 ms.
- The log counts are cumulative (245 / 844 / 1452 ticks). Per-interval INT_RECOIL is 157 -> 433 -> 824 per tick (aotr60.log lines 11/63/115).

- [uncertain] H1: the hitch is one over-budget logic sub per tick, and the simulator fits the data closely.
  INFERRED. The ~1 late release per tick is consistent with H1 but not unique to it.

**Variance contradicts the model.** The model uses 5 % jitter. In it, the per-window minimum Present spacing is 10.4-10.9 ms at the t=283 inputs and 8.6-9.2 ms at t=421 (v2.py). Measured present_spacing_min is 3.45-5.7 ms in every late window, and 1.9-4.7 ms early. That needs large render/iteration variance or occasional X (A-render) overruns. With 35 % jitter the simulator gives 2.3-3 ms.

**The fitted model misses the residuals.** At L=33 it reproduces 5.03/s x 23.4 ms but not the measured 6.42/s, and not the debt: simulated 2.1 ms/s vs measured 32 ms/s (scen.py rerun).

**An alternative is not excluded.** A heavy first A-render after the logic tick (pre-Present) would produce the same 1-per-tick freeze. The H2 rejection rests on the mean a (9.3 ms) and does not rule it out.

- [refuted] present_wait_ms falls from 300-650 ms per 10 s window (t<=262) to 10-36 ms (t=347-410) and to 0.2/0.0 at t=421/432, showing spacing is off in the heavy phase.
  CONFIRMED from the CSV that the numbers mix units.

- **Per window:** 164-647 ms (t<=262), 215.8 / 195.7 / 106.4 / 112.3 / 27.7 / 115.5 / 143.7 ms (t=347-410), 2.7 / 0.0 ms (t=421 / 432).
- **Per second:** 15-61, then 10-20 (2.6 at t=389), then 0.25 / 0.

So the spacing wait (owed == 0) was still active about 1/3-1/2 as often at t=347-410. It was essentially off only in the last ~22 s. The A/B alternation is therefore real only at the very end.

- [confirmed] The spacing wait holds the B Present to about release + a - 0.25, which leaves about 6.7 ms for the logic step. Late releases start at L ~7-8 ms instead of ~10 ms.
  CONFIRMED in the code: pacer.cpp:339 (`g_owed == 0`), :340 (target lastPresent + halfPair - 0.25 ms), :341 (cap release + 0.6*halfPair). The session build has the same code at :336.

Re-running the scen.py sweep reproduces it: cur 3.90/s at L=7 and 5.05/s from L=9; fix1 0.45/s at L=9 and 3.35/s at L=10.

Caveats:
- This is a ~2-3 ms threshold shift that matters only in the transitional phase.
- It applies only when b < a - 0.25. At t=156 / 187 / 230, b_rel >= a_rel and there is no B hold.

- [uncertain] CONFIRMED: repay capacity (T/8 per on-time iteration) is below the lateness volume, so owed is pinned at 6T and time is lost (pred 36.0/30.7 vs measured 32.1/32.1 ms/s). Logic ran at 4.93/4.59 ticks/s, i.e. 2.5-9 % slow.
  The arithmetic is reproduced: 107.3 / 108.4 ms/s capacity vs 143.3 / 139.1 ms/s lateness at t=421 / 432 (dt 10.9 s). But the mechanism is overclaimed.

- **The model does not reproduce it.** The analysis's own sim at its fitted t=421 inputs gives debt 2.1 ms/s, speed 0.9966. 32 ms/s only appears with L>=38 or with 100 ms spikes (v1.py).
- **Spikes count too.** present_spacing_max is 117 / 115 ms in those windows. Lateness beyond the 3T clamp (pacer.cpp:213-233) goes straight to debt, and no repay rate recovers it.
- **owed was not logged** in the session build. owedTicks was only added in the 'no display queries in the frame loop' commit, so the pinning is not observed.
- **The t=432 window contains a pause.** renders/mframe = 2.099 vs 2.000 elsewhere (and 2.509 at t=124). The 4.59 ticks/s and the '9 % slow' are not pacer loss. Lost renders give 30.8-36.7 ms/s, i.e. ~3.1-3.7 %.
- **Only 3 windows show a deficit** (389 / 421 / 432). Elsewhere debt is mostly <7 ms/s.

- [confirmed] Rejected options: gentler or no repay loses speed, merge lengthens the freeze, pre-pay/centring leaves p2p unchanged. Faster repay max(T/8, owed/6) gives exact speed with an unchanged freeze.
  CONFIRMED within the simulator (repay.py and scen.py re-run):
- none: 0.9504 / 0.9075 / 0.8679;
- T/16: 0.9570 / 0.9130 at L=30 / 40;
- T/8 at L=40: 0.9630, 35.9 ms/s;
- owed/6: 0.9999;
- freeze unchanged across all variants;
- merge per-tick hold 51.2 vs 42.9 at the t=421 inputs.

The model's 5 % jitter and missing spikes still apply. With spikes, fix1 keeps ~11 ms/s of debt (scen.py spikes block), so 'no lost time' is spike-free only.

- [uncertain] Only a deferred mid-step B Present shortens the freeze: per-tick 30/40/50 -> 21/25/31 ms, and >40 ms holds go from 4.9/s to 0. It is implementable by OR-ing D3DCREATE_MULTITHREADED at the 6-byte site 0x524190.
  **The gains are oracle numbers.** sim2.py places the deferred Present using the actual end of the logic step: `nextA = max(deadline + T, tt) + est_a`, where tt already includes L. Replacing that with the P80 tail prediction (sim2p.py / v3.py) gives:
- 5 % jitter: about the same (L=40 ideal 32.0 vs 31.2);
- 35 % jitter: L=30 30.9 vs 27.1, L=40 37.9 vs 32.1 (ideal); >40 ms holds at vb120 drop only from 3.8 to 3.2/s at L=40, not to 0.

**The site is incomplete.** CreateDevice is called twice: at 0x5241B6 (flags loaded at 0x524190) and on the depth-format retry path, which reloads [0xDD345C] at 0x5241EF and calls at 0x524222. A single site at 0x524190 misses the retry. The writers of [0xDD345C] are 0x5240E6 / 0x5240F6 / 0x52410D.

- [confirmed] FullScreen_RefreshRateInHz [0xDD3028] is always 0 (sole writer 0x524B62 with EBP=0), so the telemetry display line logs refresh 0 Hz.
  CONFIRMED.
- ghq refs: the only reference is 0x524B62 `mov [0xdd3028],ebp`.
- EBP is zeroed at 0x5249CC / 0x5249ED; the struct is set up from 0x524B05 (`mov edi,0xDD2FF8`).
- HEAD telemetry.cpp:488-489 logs Read(0xDD3028).
- PresentationInterval [0xDD302C] is also written with EBP (0 = DEFAULT) at 0x524B5C, then set by FUN_00522460 (0x80000000 / 2 / 4 / 1).

- [uncertain] DLL candidate: the committed DisplayBlockReason ran MonitorFromWindow + GetMonitorInfoW + EnumDisplaySettingsW about every second in fullscreen/vsync, and the uncommitted working tree moves it to on-change.
  CONFIRMED in code: in the session build, frame_ctl.cpp:49-80 checks every 1000 ms (GetTickCount) unless IMMEDIATE.

The 'uncommitted' premise is stale. The change is now the commit 'no display queries in the frame loop; frame-timing stall diagnostics' (14:52:27), after the session (14:32-14:40). The session therefore ran the per-second version.

Impact: running <analysis workspace>\hitch\enumtime.py on the test PC measured EnumDisplaySettingsW at 0.019 ms median / 0.049 ms max, and the other two calls at <0.07 ms. So it is very unlikely to cause a visible hitch. Exclusive-fullscreen cost was not measured (INFERRED).

- [confirmed] FIFO/vsync is unlikely because late_skips = 0 throughout.
  CONFIRMED.
- late_skips and forced_skips are 0 in every window, and presents_s == renders_s.
- The skip at pacer.cpp:327 needs !immediate && g_presentsSinceBlock < 2 && behind >= interval.
- With owed high in the late windows, 'behind' was usually satisfied. So either the session was IMMEDIATE or no Present blocked for more than 1 ms.

The present interval itself was not logged.

- [uncertain] The working-tree telemetry has gaps that would leave the cause ambiguous: stall threshold 1.5 slots, no per-sub logic columns, refresh read from 0xDD3028, no 30-mode Present timing, 600-line cap.
  The individual gaps are CONFIRMED:
- telemetry.cpp:372-373 (`total > slot*3/2`);
- the :378 cap of 600 stall lines;
- PRESENT_STUB calls the hooks only when g_m60 (stubs.asm ~774-797);
- the 0xDD3028 refresh read.

But 'ambiguous' is overstated. The HEAD stall line already logs `logic %.1f (sub %d, frame %u)` per stalled iteration (telemetry.cpp:380-384). At L >= ~19 ms a heavy Y exceeds 24.75 ms, so the next session with that build already identifies the heavy sub and its frame period. That covers t > ~300 s here. A1 is a refinement, not a prerequisite.

- [confirmed] The existing fallback rules never fire for this pattern.
  CONFIRMED.
- pacer_policy.cpp:5-8 rules: lateRatio > 0.5, lostRatio > 0.05 for 3 windows, skip > 0.10, or onTime < 0.5.
- Measured: on-time ~89-90 %, lost at most ~3.2-3.7 %, no skips.

## VERIFIER MISSED
1. **The variance evidence.** present_spacing_min is 2-7 ms in every 60-mode window. A near-deterministic model (5 % jitter) cannot produce that; it gives about 9-11 ms minimum. So iteration and render times vary strongly, or A-renders occasionally overrun their slot.
   - Every predictor-based proposal was validated only in the low-variance model: the B1 tail predictor, B3's "freeze60_pred within 0.1 ms", and the B4 split timing.
   - A heavy first A-render after the logic tick fits the same 1-per-tick signature and would defeat a Y-tail predictor.

2. **The session build.** It was the phase-6 commit ('60 FPS on the Living World strategic map', 14:29:55). 'No display queries in the frame loop; frame-timing stall diagnostics' was committed at 14:52, after the session. The current build already prints stall lines with the logic sub and the logic frame. The cheapest next step is one more session with the current build, at Telemetry=1, with a Ctrl+Shift+F11 30/60 toggle at the heavy stage. That directly shows which sub is heavy, its period, and whether L is the same in 30 mode. There is no 30-mode data at the heavy battle state in this snapshot, so "stock 30 would also show a 44 ms frame" is model-only.

3. **Pauses in the data.** The t=124 and t=432 windows contain pauses: renders/mframe is 2.509 and 2.099 against 2.000. So the low logic rates there are not pacer loss. The pacer's real loss was about 3 % and only in the last ~22 s, so lost game time is not what the user noticed.

4. **Spikes.** Present gaps of about 100-200 ms occur in every window from the start, with no trend. Lateness beyond the 3T clamp is written off as debt directly, which the repay fix cannot recover. Those spikes, not the repay rate, explain part of the 32 ms/s.

5. **The second CreateDevice call** (0x5241EF / 0x524222) is not covered by the proposed MULTITHREADED site.

6. **Not addressed.** If the session was FIFO at 120 Hz, 60.6 Presents/s on a 120 Hz grid inherently gives about 1.2 single-vblank (8.3 ms) frames per second. This beat exists from the start, so it is not the "after a couple of minutes" cause. The presentation interval should still be logged to rule it in or out.

7. **What the user reported.** At the late stage the 43-50 ms freeze recurs 5 times per second. That is a continuous judder, not separated "hitches at intervals". The earlier phase (0.1-0.36 late releases per tick, i.e. one 25-40 ms freeze every 0.5-2 s) fits the wording better. The analysis does not say which phase the user means.