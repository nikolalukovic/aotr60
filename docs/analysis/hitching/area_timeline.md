# AREA timeline

## SUMMARY

The regular hitch is a 5 Hz stall: once per logic tick (every 198 ms), the pacer releases one iteration late. That iteration is the X right after the Y whose logic sub-step no longer fits. In 60 mode a logic sub-step shares a 16.5 ms iteration with the B-render: the B-render costs 4.5-6 ms and the Present-spacing wait 0-2 ms, which leaves about 9-11 ms for logic. In stock the same step gets 33 ms minus the render, about 23-25 ms.

The calibrated model gives the heavy sub-step (the work after the B Present) as below 9 ms before t≈252 s, then about 17 ms (t=283-316), 23 ms (325-358), 29 ms (368-411) and 36 ms (421-433), each about ±3 ms. This tracks the battle size: per-tick draw counts go 157 → 432 → 825. Late releases jump from ≤0.36 to 1.02-1.34 per tick at t≈262-283 s, about 3.5 minutes into the match.

The pacer does not create the 5 Hz pattern, but it shapes and amplifies it:
- **Spacing wait:** in the clean state the Present-spacing wait sits before the logic step in Y. It adds about 1.7 ms (+27%) to each stall at onset.
- **Repayment:** each stall is followed by frames shortened by T/8 = 2.06 ms, so motion runs 14% fast after it.
- **Uneven A/B cadence:** while owed > 0 the spacing wait is off, so A and B Presents alternate by ±(a_rel − b_rel).
- **Saturation:** from t≈389 s the per-tick lateness (≈26 ms) exceeds the repay capacity (11 × 2.06 = 22.7 ms). Owed stays pinned at 76-99 ms and every frame runs at 14.44 ms with a 17.7/11.2 A/B alternation, plus one ≈45 ms stall per tick. About 3% game time is lost (debt 350 ms per window).

Stock 30 with the same work would show no visible hitch until t≈360 s (heavy frame 25-32.5 ms < 33 ms). At the very end it would show a mild 1.15-1.4× long frame every 6th frame and run 3-6% slow. Its per-tick timing error is 2-3× smaller than at 60.

Best pacer-side mitigation in the simulator: predict the overrunning Y per stepper index, release it as soon as its X finishes (borrowing the X slack), and skip that B Present. This keeps exact speed and cuts the per-tick timing error 8.5 → 3.4 ms at onset (stock: 2.9 ms), 14.6 → 7.4 ms at t=325-358 and 20.4 → 10.8 ms at t=368-411. At the end (L ≈ 36 ms) the heavy pair exceeds even 33 ms, so no pacing scheme gets close to stock 30's timing there.

## FINDING [confirmed] One late iteration per logic tick from t≈262-283 s: per-tick work, not render cost
From t=283 s on, late releases per 10.6 s window are 55-70 against 50-54 logic ticks: 1.02-1.34 late per tick. Before t≈252 s it was 0.15-0.36 per tick. On-time drops from 97-99% to 89-91%, so about 11 of the 12 iterations per tick are on time and one is late. Lateness per tick (late_release_ms / ticks) grows 8.9 (t=283) → 11.5 (326) → 20.7 (368) → 24.3 (389) → 29.1/30.3 ms (421/432). Mean release→Present stays far below T=16.5 ms (a_rel 6.9 → 9.3 ms, b_rel 4.5 → 6.0 ms), so on average neither render overruns by itself.
EVIDENCE: Data: <analysis workspace>/hitch/aotr60_rates.csv (late_releases, logic_ticks_s, on_time_pct, late_release_ms). Per-window derivation printed in this session: t=283 late/tick 1.15; t=421 1.30; t=432 1.34. Loop order CONFIRMED in game.dat: 0x6325CF call [eax+0x9C] (clientUpdate: render + Present) → 0x6325D5 halt test → 0x632622/0x632625 s++ → step(s) at 0x6326EB. So Y_k = B-render k followed by step(k+1), and the step's time is charged to the next AotR60_Pacer call: late = now − deadline (src/pacer.cpp:211-212), re-anchored at pacer.cpp:242.
IMPLICATION: The hitch rhythm is the logic tick rate (5.05 Hz, every 198 ms): a regular stutter. It starts when one Y iteration (B-render + heavy logic sub-step) exceeds 16.5 ms.

## FINDING [likely] Estimated heavy logic sub-step L over time (calibrated simulator)
X lateness ≈ b_rel + ~0.75 ms (Present call, post-Present client work, loop overhead) + spacing wait + L − 16.5. Fitting the timeline simulator to pooled CSV phases gives L ≈ <9 ms (t=187-252), 17 ms (283-316), 23 ms (325-358), 29 ms (368-411) and 36 ms (421-433), about ±3 ms each. The resulting stall per tick is ≈5.8 / 13.4 / 18.2 / 25.8 ms. Onset threshold: L > 16.5 − b_total − wait ≈ 9-10 ms, crossed at t≈262-283 s (60 mode on at 14:34:07.668, log line 10, about t≈63 s in the CSV). Growth tracks battle size: INT_RECOIL calls per tick are 157 (log line 16) and cumulative 352/550 (lines 68/120), i.e. interval averages 157 → 432 → 825. L here is all work after the B Present in the heavy Y iteration (logic sub-step + post-Present client work + loop overhead).
EVIDENCE: Simulator <analysis workspace>/hitch/timeline/tsim.py (exact port of pacer.cpp:190-367), calibration phases.py, fit2.py, land.py; result out_verify.txt. Sim vs data per 10.6 s (P1/P2/P3/P4):
- late: 60/62/60/60 vs 61/64/63/69
- late_ms: 532/890/1115/1453 vs 448/830/1121/1539
- on-time: 90.6/90.3/90.6/90.4 vs 90.5/90.1/90.2/89.3
- present wait: 705/281/118/0.1 vs 336/271/101/1.4
- debt: 26/30/40/291 vs 27/92/52/350
Score landscapes (land_P2.txt, land_P4.txt) have unique minima at L=22-24 (P2) and L=34-38 (P4).
Alternative ruled out: a per-tick heavier isTick A-render (>~3 ms extra). Through the spacing-wait ratchet it would multiply the present-wait totals (sim: extra 3 ms → ×2, 6 ms → ×3-5) far above the measured 101-391 ms/window (waitcheck.py).
Occasional A-side overruns do exist: present_spacing_min is 3.4-5.7 ms (≈ b_rel) in every window, which needs an X late with its excess before the A Present.
IMPLICATION: The root cost is game logic in one sub-step per tick (assumed step(5), AI + update buckets 1..2 + 13 subsystems; docs/analysis/agents/core.md §0), growing with unit count. Which sub-step it is was not measured.

## FINDING [confirmed] End of session: lateness exceeds the repay capacity, owed pinned at the cap
Repayment is at most T/8 = 2.0625 ms per on-time iteration (pacer.cpp:247-256), so with 11 on-time iterations per tick the capacity is 22.7 ms per tick. Where Σlate exceeds 2.0625 ms × N_ontime, the excess appears as debt: t=389: 129 vs debt 168; t=421: 392 vs 350; t=432: 334 vs 350. In all other windows the difference is negative and debt comes only from spikes over the 3T clamp. In those saturated windows owed never reaches 0, so the spacing wait (pacer.cpp:336, owed == 0 only) stops: present_wait_ms = 27.7, 2.7 and 0.0 at t=389/421/432, against 100-650 earlier. In the simulator owed oscillates between 76 and 99 ms (cap 6T = 99 ms, pacer.cpp:236-241). Every on-time iteration is shortened to 14.44 ms, and about 3.2% game time is lost (logic 4.93/s vs 5.05).
EVIDENCE: CSV rows t=389.5, 421.6, 432.5 (debt_ms, present_wait_ms, on_time_pct, late_release_ms); arithmetic table printed in this session; deterministic trace out_trace_immediate.txt (P4: owed 90.75 → 76.31 → 99.00 per tick).
IMPLICATION: In the last minute the whole tick is irregular, not just one frame: 11 compressed frames plus a ≈45 ms stall. The pacer can no longer keep exact speed either.

## FINDING [likely] Presented frame sequence the user sees
Deterministic per-tick traces, Present-to-Present intervals in ms:
- P1 (L≈17), IMMEDIATE: 16.25 ×7, then 24.1 (stall B4→A5), 12.6, 16.3, 12.6, then even 16.25 again.
- P4 (L≈36), IMMEDIATE: releases every 14.44 ms, A/B alternating 17.7/11.2, one 45.5 ms gap per tick.
- FIFO 120 Hz (fullscreen): P1 shows 16.7 ×N, 25.0, 8.3; P4 shows 16.7/8.3 mixtures with 33-50 ms holds.
Per-tick timing error (display time minus content time, peak-to-peak, median), IMMEDIATE / FIFO: P1 8.5/9.5, P2 14.6/17.3, P3 20.4/23.9, P4 26.6/25.8 ms; P0 (no heavy step) 2.45/1.8 ms. Display intervals > 1.5× nominal: 2.2-5.4 per second (P1) and 5.3-10 per second (P4).
EVIDENCE: out_trace_immediate.txt, out_trace_fifo.txt (trace.py/trace2.py), out_seq.txt (realistic noise), out_stock_cmp.txt (judder() in tsim.py).
IMPLICATION: A rhythmic 5 Hz freeze-then-hurry. At onset it is one 24-30 ms frame plus a few 12-13 ms frames; by the end it is a 45 ms hold with uneven fast motion in between. This matches the report: it starts after a few minutes and repeats at regular intervals.

## FINDING [confirmed] How the pacer shapes and amplifies the stall (it does not create the 5 Hz pattern)
(a) Spacing-wait ratchet. AotR60_PresentSkip delays a Present until last Present + 16.25 ms (capped at release + 9.9 ms) while owed == 0. On a B-render this wait sits before the logic step of the same Y iteration, so it lengthens the heavy iteration. After any late Present (e.g. the isTick A) the phase of every later Present only drifts back 0.25 ms per Present. Effect: at P1 the stall is 8.24 ms vs 6.51 ms with PresentPacing off (+27%), with 59 vs 52 late releases and Σlate 464 vs 345 ms per window. At P2 the stall is +1.6 ms; at P3/P4 there is no effect, because owed is never 0 there.
(b) Catch-up. After each stall the next on-time iterations are shortened by min(owed, T/8, slack), so motion runs 14% fast for λ/2.06 frames.
(c) No spacing while owed > 0. A and B Presents land at release + a_rel and release + b_rel, which alternate the intervals by ±(a_rel − b_rel): 12.6/16.3 at P1, 11.2/17.7 at P4.
(d) No re-anchor oscillation: each late release resets the deadline (pacer.cpp:242), so lateness does not compound. The extra 0.15-0.3 late releases per tick come from occasional A-side overruns or a second heavier sub-step, not from the pacer.
Without a heavy step (P0) the timing error is 2.45 ms, i.e. noise only.
EVIDENCE: Code: src/pacer.cpp:336-347 (wait only when owed == 0; target lastPresent + halfPair − freq/4000; latest = release + halfPair×6/10), pacer.cpp:307-314 (a_rel/b_rel measured before the wait), pacer.cpp:244-256 (repay), pacer.cpp:220-243 (late branch). Present stub: src/stubs/stubs.asm PRESENT_STUB (AotR60_PresentSkip runs before the device Present inside the render). Sim: out_amp.txt, out_trace_immediate.txt (B1 at release+8.12 drifting to B4 at release+6.66 after A1 at +8.37).
IMPLICATION: The visual problem is about 2× worse than a bare stall: one long frame becomes a long frame plus 3-11 irregular frames. Removing the wait before predicted-heavy Ys and anchoring the spacing to the release would cut the onset stall by about a quarter.

## FINDING [likely] Stock 30 with the same per-tick work: little or no visible hitch until the very end
Stock iteration = A-render (a_total) + the logic sub-step, with a 33 ms limiter and no catch-up (0x63A196..0x63A1FE: last = exit time). The heavy stock frame is 25.1 ms (P1), 32.5 (P2), 37.8 (P3) and 46.5 ms (P4).
Timing error per tick, stock 30 vs 60 (IMMEDIATE): 2.9 vs 8.5, 4.1 vs 14.6, 6.1 vs 20.4, 10.3 vs 26.6 ms.
Stock 30 game speed: 0.999 / 0.988 / 0.969 / 0.941. The 60-mode pacer holds 1.000 / 1.000 / 1.000 / 0.986.
The difference is the budget for the logic sub-step: 33 − a_total ≈ 23-25 ms in stock against 16.5 − b_total − wait ≈ 9-11 ms in 60 mode.
EVIDENCE: out_stock_cmp.txt and out_seq.txt (run30 in tsim.py models the stock limiter as documented in docs/analysis/gaps/G6_pacing.md G6-01). Loop order verified by disassembly at 0x6325CF..0x6326EB.
IMPLICATION: At 30 FPS the user would not have seen this hitch for most of the session (t < ~360 s). At the end stock would show a milder 5 Hz long frame (1.15-1.4×) and run 3-6% slow instead.

## FINDING [likely] Pacer-side mitigations evaluated
IMMEDIATE, per-tick timing error p2p median, and game speed, for P1 / P2 / P3 / P4:
- **Current:** 8.5 / 14.6 / 20.4 / 26.6 ms; speed 1.000 / 1.000 / 1.000 / 0.986.
- **No repayment (stock-like loss):** 7.4 / 13.3 / 16.7 / 25.0 ms, but speed 0.957 / 0.923 / 0.903 / 0.863. Rejected.
- **Repay T/16:** about the same error, speed 0.988 / 0.968 / 0.935 from P2 on.
- **Repay T/4 or T/2:** same p2p, more off-cadence frames (FIFO off/s 12-21 vs 13-31).
- **No spacing wait:** same p2p, A/B unevenness everywhere (velocity error 2.9 vs 1.9 ms at P1).
- **Spacing wait also while owed:** p2p unchanged, fewer off-cadence frames on FIFO, but speed 0.952 at P4.
- **Early-Y release alone (borrow the X slack for a predicted-heavy Y):** worse (11.1 at P1) because B is shown early.
- **Early-Y + skip that B Present:** 3.4 / 7.4 / 10.8 / 21.9 ms, speed 1.000 / 1.000 / 1.000 / 0.994, late releases 5.5 → 2.8 per second at P1. Cost: one 33 ms (30 FPS) frame per tick; max hold rises at P4 (95 vs 74 ms).
- **Drop B only when the borrow covers the overrun:** worse (7.2 / 15.7 / 23.1 / 29.9 ms).
- **Time-aligned A-render fraction (renderer change, causal clock):** on top of early-Y + skip B it reaches 2.6 / 5.0 / 8.5 / 17.2 ms, but frame-to-frame velocity error rises.
- **Stock 30 fallback, for reference:** 2.9 / 4.1 / 6.1 / 10.3 ms at speed 0.999 / 0.988 / 0.969 / 0.941.
FIFO 120 Hz shows the same ranking.
EVIDENCE: mitig.py → mitig_imm.txt, mitig_fifo.txt; mitig2.py → out_mitig2.txt; timealign.py → out_timealign.txt (all under <analysis workspace>/hitch/timeline).
IMPLICATION: Releasing the predicted-heavy Y early and skipping its B Present removes the hitch at onset (P1 ≈ stock) and halves it at P2-P3, at exact speed. Once a_total + b_total + L > 33 ms (t≳360 s) no pacing scheme gets close to stock 30's smoothness.

## FINDING [confirmed] 120 Hz cadence beat (secondary, load-independent)
33.000 ms pairs against 4 vblanks (33.333 ms) drift one vblank every 825 ms. Under FIFO or DWM-composited flip at 120 Hz this gives one 8.33 ms display interval every 825 ms; with render jitter it becomes a cluster of 8.33/25 ms intervals around each crossing (1.6 long display intervals per second at P0 under FIFO, 0.1 under IMMEDIATE). Stock 30 at 120 Hz has the same 825 ms beat as 25/41.7 ms frames.
EVIDENCE: out_beat.txt (beat.py): events exactly 825 ms apart with no jitter; out_stock_cmp.txt P0 FIFO row.
IMPLICATION: This exists from the start of 60 mode in fullscreen, so it does not explain an onset after minutes. It could add to the perceived irregularity. Pacing=nominal (T = 1000/60, README) removes it at a 1% speed cost.

## FINDING [likely] Present interval of the session is undetermined; FIFO did not block
late_skips = 0 in every window. In the saturated windows the schedule condition of the FIFO skip (now − (deadline − owed) ≥ interval) is always true, because owed is 76-99 ms. So under FIFO, any Present taking more than 1 ms would have produced late skips. Either the session ran IMMEDIATE, or FIFO at 120 Hz never filled the queue. Both are consistent with the per-tick logic stall being the cause, not GPU or vsync blocking.
EVIDENCE: src/pacer.cpp:319-329 (FIFO skip needs g_presentsSinceBlock < 2) and pacer.cpp:354-366 (block = Present took > freq/1000); CSV late_skips column all 0.
IMPLICATION: The hitch is CPU and game-logic bound, not display bound. The PresentationInterval [0xDD302C] should be logged so this can be checked.

## REC [now] Release a predicted-heavy Y right after its X and skip that B Present
In AotR60_Pacer keep a per-k EMA of each Y iteration's duration (release → next pacer call). At the end of X_k, if the predicted Y_k overrun is more than about 1 ms, release Y_k immediately instead of waiting for the deadline. Shift the deadline by the borrowed amount and add it back to the next interval, so the absolute pair schedule stays at 33.000 ms. Have AotR60_PresentSkip skip that B Present (the existing skip path, pacer.cpp:301-303 / PRESENT_STUB) and its spacing wait, so A_k stays on screen for the pair. Simulated: per-tick timing error 8.5 → 3.4 ms at onset (stock 30: 2.9), 14.6 → 7.4 and 20.4 → 10.8 ms later, game speed exact, late releases 5.5 → 2.8 per second at P1.
RISK: The prediction assumes the heavy work stays on a fixed stepper index. Skipping the B Present shows one 33 ms frame per tick. When even the pair overruns (L ≳ 30 ms) the longest hold grows (sim P4: 95 vs 74 ms) for only a small gain. This changes timing only; logic and render content are untouched (INFERRED from PLAN §1.2).

## REC [now] Remove the spacing-wait amplification before heavy logic steps
Skip the Present-spacing wait on a B-render whose Y is predicted to overrun; this is implied by the recommendation above. Optionally anchor the spacing target to release + EMA(a_rel) instead of g_lastPresent + 16.25 ms (pacer.cpp:337), so one late Present no longer delays the following ones (today they drift back only 0.25 ms per Present). Simulated effect of no wait on the heavy Y: onset stall 8.2 → 6.5 ms, late releases 59 → 52 per window.
RISK: The anchoring change was not simulated. Without spacing on those B-renders, the A/B intervals there alternate by ±(a_rel − b_rel), about 2-4 ms.

## REC [after-diagnostics] Add telemetry that pins down the heavy iteration
Record per stepper index k: the Y iteration duration and QPC around step(sub) (around call 0x6326EB). Record per k the X duration and a_rel, including the isTick A-render. Log PresentationInterval [0xDD302C] at each mode change. Keep a ring buffer of the last ~2 s of release/Present/owed values and dump it when lateness exceeds 10 ms. This confirms which sub-step is heavy, separates logic-side from A-side overruns, and shows whether L in 60 mode equals stock (compare with Enabled=0 in the same battle).
RISK: None for the game. Keep it behind Telemetry ≥ 1; QPC calls cost well under 1 µs each.

## REC [after-diagnostics] Judder-aware fallback to 30 when the per-tick stall exceeds what pacing can hide
The current policy (pacer_policy.cpp:11-14) cannot trigger here: in this session late iterations were about 11% (< 50%), lost time 0-3.2% (< 5%) and on-time about 89% (> 50%). Add a criterion such as: at least 0.8 stalls per tick with a mean stall above about 12 ms, or a predicted a_total + b_total + L > 33 + δ ms, for 2-3 windows → fall back to 30. In the simulator stock 30 has 2-3× less timing error than any 60-mode variant from about t=360 s on, at a 3-6% speed loss that stock has anyway.
RISK: Switching on the per-tick hitch would toggle 30/60 during big battles. It needs hysteresis and the existing backoff. Players who prefer 60 with stutter over 30 may want it configurable.

## REC [optional] Do not change the repay rate or drop repayment
Simulated: no repayment cuts the timing error only 10-18% and makes the game 4-14% slow. T/16 loses 1-6.5% speed from P2 on. T/4 and T/2 do not lower the timing error and add off-cadence frames on FIFO. Keep T/8.
RISK: None (no change).

## REC [optional] Optional: time-aligned A-render fraction and a cheaper B-render on skipped Presents
(a) Let the A-render presentation fraction follow its actual Present time within [k−1, k], using a causal smoothed display clock. On top of the first recommendation the sim shows a further 15-25% lower timing error (P1 3.4 → 2.6 ms, P3 10.8 → 8.5 ms), but frame-to-frame velocity error rises. (b) When a B Present is skipped, render only the drawable pass the logic needs (PLAN §1.2) and skip the scene render. That saves part of b (≈4-6 ms) in the heavy pair and makes larger L fit. (c) On 120 Hz displays Pacing=nominal removes the 825 ms beat at a 1% speed cost.
RISK: (a) and (b) change the renderer and need a fresh audit of the B-render invariants (the drawable transform cache and C4 key). (c) makes the game 1% slower than stock.

## OPEN
- **Which sub-step is heavy, and does it cost the same at 60 as at stock?** The telemetry cannot tell which logic sub-step is heavy; the model assumes step(5) (AI and buckets) at a fixed index. Nor can it tell whether L in 60 mode equals L at stock 30, for example because of extra work from C5 replay, cache effects or telemetry. Per-sub QPC timing is needed, plus a comparison with Enabled=0 in the same battle.
- **A/B render-time distributions do not fully match.** The simulator over-predicts the present wait at t=187-316 (about 2×) and its minimum spacing is 6-8 ms where the data shows 3.4-5.7 ms. The real A/B distributions differ in detail: occasional A-side overruns and possibly B-renders that are sometimes slower than A. This does not change the conclusions.
- **The 55-210 ms stalls.** One appears in nearly every window (stock-like hitches). Their origin and period are unknown, so it is open whether the user also means these by "regular intervals". The onset after a few minutes points to the 5 Hz per-tick pattern.
- **Present mode.** The present interval of this session (fullscreen FIFO via the launcher vs windowed IMMEDIATE) was not logged.

Files (all under <analysis workspace>/hitch\timeline\):
- Simulator and calibration: tsim.py (pacer port and simulator), phases.py (calibrated workload).
- Fits: fit2.py, fit2_V1/V2/V3.txt, land.py, land_P1/P2/P4.txt.
- Calibration check: verify.py → out_verify.txt.
- Tick traces: trace.py, trace2.py → out_trace_immediate.txt, out_trace_fifo.txt.
- Stock 30 comparison: stock_cmp.py → out_stock_cmp.txt.
- Mitigations: mitig.py → mitig_imm.txt, mitig_fifo.txt; mitig2.py → out_mitig2.txt; timealign.py → out_timealign.txt.
- Spacing-wait amplification: amp.py → out_amp.txt.
- 120 Hz beat: beat.py → out_beat.txt.
- Realistic display sequences: seq.py → out_seq.txt.

## VERIFIER OVERALL
The core claim holds, but the analysis is incomplete. From about t≈283 s one pacer release per logic tick is late, and that lateness grows with battle size; I recomputed this from the CSV and the code and the loop order matches. It is the main load-driven, regular hitch.

The analysis has three problems:
1. **The session's present mode is known.** It says the mode is "undetermined". The DXVK log written by this very session (rotwk\game.dat_d3d9.log, mtime 14:40:27; the AotR60 log ends 14:40:14 and no later run exists) shows "Windowed: false" and "Present mode: VK_PRESENT_MODE_FIFO_KHR". So the session ran fullscreen with vsync.
2. **It missed a per-second stall in the build that ran.** In fullscreen, the build used for the session (the phase-6 commit '60 FPS on the Living World strategic map', frame_ctl.cpp DisplayBlockReason) ran MonitorFromWindow, GetMonitorInfoW and EnumDisplaySettingsW on the game thread about once a second. This happens in C0, before the render, in both A- and B-renders. The commit 'no display queries in the frame loop; frame-timing stall diagnostics' (14:52) already removed it as a "suspected cause of a hitch about once a second in fullscreen".
3. **Its calibration needs extra tuning knobs; a once-per-second event fits better.** The calibration (phases.py V3) needs several made-up parameters: a 6 ms heavy sub-step 1 with CV 0.7, 2 ms of extra isTick work, and 0.1/s spikes. Even with them it misses min Present spacing (7.5-8.5 vs 3.8-4.6 ms in the data) and over-predicts the present wait by about 2×.

I reran the analysis's simulator with one heavy sub-step plus a once-per-second, roughly 20 ms C0 event (the per-second query modelled with GetTickCount gating). It fits every phase better: score 4.1 vs 49.5 at P0, 8.8 vs 33 at P1, 1.3 vs 4.6 at P3, 5.2 vs 11.9 at P4. It reproduces min spacing (4.7-5.8 ms) and the 1 extra late release per second. The heavy-step estimates stay where the analysis put them, within ±2 ms (P1 15-17, P3 27-29, P4 34-36 ms). That model also produces 40-50+ ms hitches every few seconds where the per-second event lands on or next to the heavy Y. Those fit "regular intervals" at least as well as a 5 Hz judder.

The other claims are model-only and not established by data: the "stock 30 would show no hitch until ~360 s" comparison and the mitigation rankings. Recommendation 3 (telemetry) is already implemented in the 'no display queries in the frame loop' commit.

- [confirmed] Loop order: render+Present (0x6325CF call [eax+0x9C]), then halt test, s++ (0x632625), then step(s), so the Y's logic step is charged to the next AotR60_Pacer call
  Disassembly 0x6325CF call [eax+0x9C], 0x6325D5 test bl, 0x632622/0x632625 s=s+1, step via call [edx+0x98] at 0x6326F0 (0x6326EB is the jump target; for s>6 the call is at 0x6326C6). Late is computed at pacer.cpp:211-212 and re-anchored at :242 (line numbers match the session build and the current file for lines <253). CONFIRMED.

- [uncertain] From t=283 s on: 1.02-1.34 late releases per tick, before t≈252 only 0.15-0.36
  Recomputed: t=283-432 gives 1.02-1.34 late per tick, and lateness per tick goes 8.9 / 11.5 / 20.7 / 24.3 / 29.1 / 30.3 ms (CONFIRMED). But the 'before' range is wrong. t=145.4 had 79 late releases, 1.48 per tick, 87.7% on time, 5.8 ms per late release; t=262/272 had 0.61. The same signature therefore appeared briefly early on (probably a fight) and went away by t=156. That supports load dependence over a leak, but the analysis misstates it.

- [refuted] The excess late releases beyond one per tick come from occasional A-side overruns or a second heavier sub-step
  The excess (late − ticks) for t≥283 averages about 10 per 10.6 s window, i.e. ~0.95/s (from 1.1 to 17). Early windows show 8-19 late releases per window, ~1-2/s. A once-per-second event explains this directly. The running build had one: DisplayBlockReason (src/frame_ctl.cpp:49-84 at the session-build commit, called from BlockReason at :184 on every C0, :363). It is gated by GetTickCount ≥1000 ms and is active whenever PresentationInterval != IMMEDIATE. In fullscreen that holds (docs/analysis/gaps/G6_pacing.md: 0x524B5C stores 0 = DEFAULT), and DXVK confirms fullscreen. My sim (timeline_verify/qfit2.py) fits better with this event than with the analysis's sub-1/isTick knobs. INFERRED: the query cost (~16-30 ms in the fit) was not measured.

- [uncertain] Heavy logic sub-step L ≈ <9 / 17 / 23 / 29 / 36 ms across the phases
  The fit is reproducible, and in my alternative model (once-per-second C0 event instead of V3's knobs) the L values stay within ±2 ms (P1 15-17, P3 27-29, P4 34-36). So the order of magnitude and the growth are robust. But L is identified only through a simulator with many free parameters, and which sub-step it is was never measured. INFERRED, as the analysis itself rates it ('likely').

- [confirmed] Growth tracks battle size: INT_RECOIL per tick 157 → 432 → 825
  aotr60.log lines 16/68/120: cumulative 157.14 over 245 ticks, 352.80 over 844, 550.00 over 1452. Interval averages are 432.8 and 823.7. C5 replays per tick grow the same way: 7.9 → 17.7 → 44.5 (lines 62/114/166). The C5 table is a fixed 4096×8-probe hash (c5_physics.cpp:23-42), so there is no growing lookup cost. CONFIRMED.

- [uncertain] 60 mode on at about t≈63 s in the CSV; onset about 3.5 min into the match
  g_start is the first render with renderId%64==0 after the first C0 (telemetry.cpp in the session build :325-331). The 120 s site summaries (14:34:59.058 − 120 s) put g_start at ≈14:32:59. So 60 mode turns on at t≈69-71 and off at ≈436 (consistent with frac60 0.475 in the last row). The onset is therefore about 3.3-3.5 min after 60 mode turned on. Minor error, conclusion unchanged.

- [confirmed] Saturation at t=389/421/432: Σlate exceeds the T/8 repay capacity, owed stays >0, the spacing wait stops, ~3% game time lost
  Recomputed: capacity (on-time × 2.0625) vs late_ms gives excess +129 / +392 / +334 against debt 168 / 350 / 350. Present wait is 27.7 / 2.7 / 0.0. t=421 runs at 4.926 ticks/s (−2.5%). The t=432 window's 4.589/s also includes a halt or pause: m_frame per pair is 0.953 (renders 58.74/s vs mframe 27.99/s), so its 9% speed loss is not pacing. 'Owed pinned at 76-99 ms' and 'every frame 14.44 ms' come from the simulator only.

- [confirmed] Spacing-wait mechanics: wait only when owed==0, target lastPresent+16.25 ms capped at release+9.9 ms, on B it sits before the logic step
  pacer.cpp (session build) :336-347: owed==0 test, target g_lastPresent + halfPair − freq/4000, latest g_releaseTime + halfPair*6/10. The PRESENT stub runs it before the device Present, and the step follows clientUpdate. The 27% amplification number is simulator-only (INFERRED).

- [refuted] Present interval of the session is undetermined (IMMEDIATE or FIFO)
  <game folder>\rotwk\game.dat_d3d9.log was last written 2026-10-05 14:40:27, matching this session: the AotR60 log ends 14:40:14.23, the CSV 14:40:22, and no later run exists. It shows 'Windowed: false', 'Setting display mode: WxH@0', 'Present mode: VK_PRESENT_MODE_FIFO_KHR', 3 images. Snapshot copied to timeline_verify/game.dat_d3d9_snapshot.log. late_skips=0 means only that the 3-image FIFO queue never filled at 120 Hz. CONFIRMED fullscreen FIFO.

- [uncertain] A heavier isTick A-render is ruled out because it would multiply the present-wait totals
  The argument rests on the simulated present wait. Yet the analysis's own calibrated model over-predicts present wait 2.2× at P0 (842 vs 391 ms) and 2× at P1 (705 vs 336) (timeline/out_verify.txt). My alternative model also over-predicts it. A metric the model does not reproduce cannot discriminate between hypotheses. Not ruled out.

- [uncertain] Stock 30 with the same work: no visible hitch until t≈360 s, and 2-3× smaller timing error
  Pure model output. There is no stock-30 data for this battle, and it assumes L is identical in both modes (the analysis lists this as open). The stock limiter description is CONFIRMED (0x63A196..0x63A1FE: Sleep/timeGetTime spin, last=exit time at 0x63A1F8, no catch-up). Minor inconsistency: the 'pair exceeds 33 ms' threshold is already crossed at P2 (a_total ≈ 9.45 + b_total ≈ 6.85 + L 23 ≈ 39 ms), not at t≳360.

- [confirmed] The fallback policy cannot trigger in this session
  pacer_policy.cpp:5-8: lateRatio counts late > T/4 and is ~11% < 50%; lostRatio max 3.2% < 5%; on-time ~89% > 50%. The log has no 'pacer: falling back' line.

- [confirmed] 120 Hz cadence beat every 825 ms; Pacing=nominal removes it at a 1% speed cost
  33.000 vs 4×8.333 ms drifts 0.333 ms per pair, i.e. one vblank per 25 pairs = 825 ms. Nominal uses freq/30 (pacer.cpp:195), 33.33 ms, 1% slower. FIFO is now confirmed, so this beat was present in the session. It is load-independent.

- [refuted] Recommendation: add telemetry for per-sub logic time, PresentationInterval logging and a stall breakdown
  Already implemented in commit 'no display queries in the frame loop; frame-timing stall diagnostics' (2026-10-05 14:52, bin/dinput8.dll 14:51). TelemetryIterationBegin logs every iteration over 1.5 slots with the breakdown check / render / Present / spacing wait / logic (sub, frame) / pacer wait / owed (telemetry.cpp:340-386). It adds new CSV columns (logic1_max/mean, logic_other_max, iter_max...), a 'display: vsync (presentation interval ...)' log line, and tools/analyze_stalls.py. The open questions (which sub-step, A-side vs logic overruns) can be answered by rerunning with the current build. No new design is needed first.

## VERIFIER MISSED
1. **The present mode is known, and it activates the per-second display query.** The session ran fullscreen with FIFO (DXVK log <game folder>\rotwk\game.dat_d3d9.log from 14:40:27; snapshot at <analysis workspace>\hitch\timeline_verify\game.dat_d3d9_snapshot.log). In that mode the session build ran MonitorFromWindow, GetMonitorInfoW and EnumDisplaySettingsW on the game thread about once a second, in C0 before the render of whichever A- or B-iteration it hit (src/frame_ctl.cpp:49-84 at the session-build commit, called via BlockReason :184/:363). The analysis never considers it.
   - **Fit:** my rerun of its simulator (<analysis workspace>\hitch\timeline_verify\tsim_q.py, qfit.py → out_qfit.txt, qfit2.py → out_qfit2.txt) fits P0-P4 better with one heavy sub-step plus a once-per-second C0 event of ~16-30 ms. It matches min Present spacing and the ~1/s extra late releases the analysis attributes to its extra fitted knobs (sub-1 heavy step, isTick extra, spikes).
   - **Beat with the logic tick:** the query period is 1000 ms with 15.6 ms GetTickCount steps; the logic tick is 198 ms. The query therefore drifts through the 12 iterations of a tick and periodically lands on or next to the heavy Y. In the simulator (timeline_verify/beatq.py, P1) that turns ~1.5 per minute of >40 ms hitches into ~22 per minute, and adds >50 ms hitches every few seconds. That is a credible reading of "hitches at regular intervals", and it gets worse as the battle grows.
   - **Status:** INFERRED; the query cost was never measured. The commit 'no display queries in the frame loop' already removed the query and logs its cost.
2. **The next step is a rerun, not new pacer work.** The current build ('no display queries in the frame loop; frame-timing stall diagnostics') already has the per-stall breakdown with logic sub and frame, plus tools/analyze_stalls.py. One fullscreen skirmish of 5+ minutes with it would separate the three candidates (heavy sub-step, per-second query, isTick A-render) before any of the "now" pacer redesigns (early-Y release, skipping the B Present) are built.
3. **The early burst at t=145 s was not mentioned.** It had 79 late releases, 1.48 per tick. It is evidence of load-driven (not cumulative) behaviour, and it means hitches can appear early in a big fight.
4. **The last CSV window includes a pause.** t=432 has an m_frame-per-pair ratio of 0.953 (logic did not advance for part of the window), so its 4.589 ticks/s is not pacing loss. The analysis avoided using it for the speed figure, but did not say so.
5. **Rejecting the isTick A-render alternative relies on present-wait totals.** Its own model gets those 2× wrong at P0/P1, so this alternative is still open.