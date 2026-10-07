# root_causes
Ranked causes of the regular hitch (session 14:32-14:40, the phase-6 build ('60 FPS on the Living World strategic map'), exclusive fullscreen FIFO at 4K / 120 Hz).

**Session facts**
- **Present mode (CONFIRMED):** <game folder>\rotwk\game.dat_d3d9.log, written 14:40:27, says "Windowed: false" and "Present mode: VK_PRESENT_MODE_FIFO_KHR" with 3 images. The earlier sessions that "worked perfectly" were windowed IMMEDIATE, so this is not a like-for-like comparison.
- **Build (CONFIRMED):** bin/dinput8.dll at the phase-6 commit hashes to 784ab403..., the session build. 'No display queries in the frame loop; frame-timing stall diagnostics' was committed at 14:52, after the session, and is the build installed now.

**1. One logic sub-step per 5 Hz tick outgrows its 16.5 ms Y slot as the battle grows.**
Confidence: high for the mechanism, medium for which sub-step it is.

- **Loop order (CONFIRMED in game.dat):**
  - Render and Present run first: clientUpdate at 0x6325CF, Present at 0x522644.
  - Then the halt test at 0x6325D5, s++ at 0x632622/0x632625.
  - Then GameLogic::update(sub) via vt98, at 0x6326C6 (sub 1) or 0x6326F0 (subs 2-6).
  - So each Y iteration is: B-render k, B Present, logic sub k+1, then the pacer.
  - The B_k Present to A_{k+1} Present gap therefore always contains L + post-Present work + the A-render up to its Present, about L + 10 ms.
- **Data (CONFIRMED, aotr60_rates.csv):**
  - Before t≈262 s: 0.04-0.44 late releases per tick, except a 1.48 burst at t=145 that looks like a fight. That burst points to load, not a leak.
  - t=262/272: 0.61 per tick.
  - t≥283: 1.01-1.38 per tick, on-time 89-91 %.
  - Lateness per tick: 8.9 ms at t=283, 11.5 at 326, 20.7 at 368, 24.3 at 389, 29-30 at 421/432.
- **Growth tracks unit count (CONFIRMED):** per-interval INT_RECOIL calls per tick are 157 → 433 → 824 (the log prints cumulative values: aotr60.log 157.14/352.80/550.00 over 245/844/1452 ticks). C5 replays grow the same way, and the C5 table has a fixed size.
- **Where the time goes (INFERRED):**
  - Implied heavy step L: about 10-17 ms at onset, about 23 ms at t≈330, and about 33-39 ms at the end.
  - Stock 30 has room for about 23-25 ms of logic in its frame; 60 mode has about 7-11 ms.
  - The lateness is mostly on the logic side. The heaviest possible A-render is bounded by 6·a_rel − 5·b_rel, about 19 ms at t=283 and 29 ms at t=421. That caps X-side lateness at about 2.6 and 12 ms, against about 6.5 and 21 ms observed.
- **Which sub-step:** not measured.
  - Sub 5 is the prime suspect: buckets 1 and 2, i.e. 163 module classes for physics, weapons and production; TheAI with a second pathfinder pass; all AIPlayer::update; and 12 more managers (0x62EB63..0x62EC0D, CONFIRMED by code reading).
  - Sub 1 (scripts, Lua, per-object loops) cannot be excluded, and neither can a heavy isTick A-render 1.
- **What the user sees (INFERRED, model):**
  - Onset: one 24-30 ms hold among 16.5 ms frames, five times a second.
  - End: a 42-50 ms hold five times a second, i.e. 5-6 vblanks at 120 Hz FIFO.
  - This fits "starts after a couple of minutes and repeats at regular intervals".

**2. The pacer amplifies it. This is DLL-side and fixable.**
Mechanisms CONFIRMED in pacer.cpp; magnitudes INFERRED from simulation.

- **(a) Spacing wait eats the logic budget.** While owed == 0, the spacing wait (pacer.cpp:339-341) holds the B Present until about lastPresent + 16.25 ms, capped at release + 9.9 ms. That wait sits before the logic step, so the budget is about 16.5 − a_rel instead of 16.5 − b_rel.
  - The once-per-tick late release starts at L ≈ 10-11 ms instead of about 13-14 ms.
  - At onset it adds about 1.5-2.5 ms to each stall (+27 %).
  - Measured present wait at t=262-326 is about 1.0-1.2 ms per B Present.
- **(b) Spacing switches off while owed > 0.** A and B Presents then alternate by ±(a_rel − b_rel), about 3-4 ms. This only fully applies in the last ~22 s: present_wait is 2.7 and 0.0 ms per window at t=421/432, while at t=347-410 it was still 28-216 ms per window.
- **(c) Repay saturates.** Repayment is capped at T/8 per on-time iteration (pacer.cpp:248), about 22 ms per tick, against 24-30 ms of lateness per tick at t≥389. Owed overflows the 6T cap into debt: 168-350 ms per window, about 3 % game time lost.
  - Part of that debt is the 100 ms spikes beyond the 3T clamp, which no repay rule recovers.
  - The t=432 window also contains a pause (renders/mframe 2.099), so its 4.59 ticks/s is not pacer loss.

**3. Once-per-second display query in C0 (session build, active because the session ran FIFO).** Confidence: low-medium.

- In the session build, src/frame_ctl.cpp:49-84 ran MonitorFromWindow + GetMonitorInfoW + EnumDisplaySettingsW about once a second via BlockReason (:184, :363), on A- and B-renders alike. CONFIRMED that it ran.
- **For:** from t=283 there are about 10 more late releases than ticks per 10.6 s window, roughly 1/s. A ~16-30 ms once-per-second C0 event fits the timeline simulator better (scores 4.1 vs 49.5 at P0) and reproduces the 3-6 ms present_spacing_min. Its 1000 ms period drifts against the 198 ms tick, so it periodically lands on the heavy Y and produces 40-50+ ms hitches every few seconds.
- **Against:**
  - Desktop timing of the three calls is 0.005-0.075 ms mean, 0.13-0.5 ms max. Exclusive fullscreen under DXVK was not measured.
  - The AI routines at 0x8F8F68 (every 5 ticks = 1 s) and 0x8F3EFE (every 10 ticks) fit the 1 Hz excess equally well.
- Already removed in 'no display queries in the frame loop; frame-timing stall diagnostics'.

**4. Background factors that do not explain the onset.**
- **(a) 120 Hz FIFO beat.** 33.000 ms pairs against 33.333 ms of vblanks slip one vblank every 825 ms (CONFIRMED arithmetic). It exists from the first second and is load-independent.
- **(b) 55-210 ms spikes.** About one per window from t=92 on, with no trend. This is the stock hitch class: the 30 FPS baseline lost 1-2.5 % to them.

**Ruled out**
- **DLL overhead:** stubs cost about a plain call (1.4-1.9 ns), C5 and camera cost microseconds, there is no growth, no locks, and no WARN lines (CONFIRMED, bench.exe re-run).
- **WaitUntil overshoot:** about 0 µs unless all logical CPUs are busy (CONFIRMED).
- **Present blocking:** late_skips = 0 everywhere, so no Present took more than 1 ms with FIFO active (CONFIRMED).
- **Fallback policy:** it cannot fire here: about 10 % late, at most about 3.3 % lost, about 89 % on time (pacer_policy.cpp:5-8, CONFIRMED).

**Known model gaps**
- present_spacing_min is 1.9-5.7 ms in every window, which a low-jitter model cannot reproduce. Either iteration times vary a lot or A-renders sometimes overrun.
- The simulators over-predict the present wait by about 2× at P0/P1.
- So L values carry ±3 ms, and the predictor-based mitigations need validation against the real distribution.

# stock_comparison
Model-based (INFERRED). There is no 30-mode data at the heavy battle stage in this snapshot.

**Stock limiter (CONFIRMED):** 0x63A196..0x63A1F8 spins with Sleep(0) and timeGetTime until 33 ms after [0xDE4318], then sets last = now. There is no catch-up.

**What the same per-tick work looks like at stock 30**
- A stock frame is render (a ≈ 7-9.3 ms + Present) + logic sub. A heavy sub stays invisible while L ≤ about 22-25 ms.
- **t ≈ 283 to ~330 s** (L ≈ 10-23 ms): stock is a perfectly even 30 FPS. 60 mode already shows a 24-35 ms hold every 198 ms.
- **t ≈ 330-400 s** (L ≈ 23-30 ms): stock shows one 33-40 ms frame per tick, 1.0-1.2× normal and barely visible. 60 mode shows a 33-40 ms hold among 16.5 ms frames (2-2.4×) plus repay compression.
- **End, t ≈ 410-435 s** (L ≈ 33-39 ms): stock shows one ~42-49 ms frame per tick, 1.3-1.5× its 33 ms frame. That is mild 5 Hz unevenness, and stock also runs about 4-6 % slow because it never catches up. 60 mode shows a 42-50 ms hold among ~14.4 ms frames (about 3×) with uneven A/B frames in between, at about 3 % speed loss.
- **Timing error per tick** (display time minus content time, peak-to-peak), stock vs 60: about 3/4/6/10 ms against 8.5/14.6/20.4/26.6 ms at the four phases. The 60 FPS judder is 2-3× more visible at every stage.

**Common to both modes**
- The 55-210 ms spikes (about one per 10 s) and the occasional 100-260 ms stock hitches look the same in both modes.
- At 120 Hz FIFO, stock has its own 825 ms beat: 25 / 41.7 ms frames instead of 33.3.

**Answer:** stock 30 would not have shown the regular hitch for most of this session (roughly until t ≈ 330-360 s, about 4.5-5 minutes into 60 mode). In the last 1-2 minutes it would show a much milder version: about 1.3-1.5× frames five times a second, plus slow-down. 60 mode makes the engine's existing per-tick cost visible about 2-3 minutes earlier and about 2-3× more prominently, because the logic sub-step shares a 16.5 ms slot with the B-render instead of a 33 ms frame with the A-render.

**Confirm with data:** in the next session, toggle Ctrl+Shift+F11 at the heavy stage and compare the 30-mode stall lines (threshold 49.5 ms) and logic times with the 60-mode ones.

# diagnostics
Most of what is needed is already in the installed build, 'no display queries in the frame loop; frame-timing stall diagnostics' (rotwk\dinput8.dll = bin\dinput8.dll 17a4f8d1..., written 14:51:47). It has:
- stall lines at telemetry.cpp:372-385: check / render (Present, spacing wait) / logic (sub, frame) / pacer wait / other / owed;
- the 10 s CSV columns iter_max, render A/B max, logic1_mean/max, logic_other_max, check_max, present_max, stalls, big_stalls;
- a display line on mode-on (telemetry.cpp:486-490);
- tools/analyze_stalls.py.

The query that ran once per second has been removed. Before the next session, make these small changes; all stay read-only for game state and behind Telemetry ≥ 1.

**D1. Lower the stall threshold so the onset is caught.**
- Today a stall needs total > 1.5 slots = 24.75 ms (telemetry.cpp:372). An onset Y iteration is about 16.5 + 6-9 ms, so it is borderline and mostly not logged.
- Change it for 60 mode to: `total > slot + 3 ms || late_at_next_release > 3 ms`.
- Keep the log cap of 600 lines, then 1 in 50 plus every line over 6 slots (:378).

**D2. Add the Present-to-Present gap to the stall line and as its own trigger.**
- In AotR60_PresentDone (pacer.cpp:~357): `gap = g_presentCall - prevPresentCall` (skipped Presents excluded).
- Log `gapstall <gap> ms t=<s>: prev <kind><k> [pc, r2, logic <ms> (sub, frame), post] | pacer [wait|late, owed] | this <kind><k> [pre, check, r1, spacing]` when gap > 25 ms (60) or > 50 ms (30).
- This decomposes the visible hold across both iterations. The parts must sum to the gap, which serves as a self-check.
- r1 = C0 end → PresentSkip entry; r2 = Present return → OnPostRender; pre = release → C0; post = logic end → pacer entry.

**D3. Per-sub logic columns in the 10 s CSV.**
- In LogicUpdateWrapper (telemetry.cpp ~597) and LwLogicUpdateWrapper (~565), keep the QPC duration per sub.
- Append `logic_mean_s1..s6, logic_max_s1..s6, lw_max`.
- Also append `gap_max_ms, gaps25, gaps40, stall_ticks` (ticks whose largest gap is > 25 ms), `a_r1_mean/max, b_r1_mean/max, pc_max, blocked_presents, owed_mean_ms, owed_max_ms, pay_ms, spacing_waits`.

**D4. Fix the display line.**
- [0xDD3028] is always 0: its only writer is 0x524B62 with EBP=0, so the line logs "refresh 0 Hz".
- On mode-on only (never per frame), log:
  - the real refresh from MonitorFromWindow([0xDD3014]) + EnumDisplaySettingsW;
  - Windowed [0xDD3018], PresentationInterval [0xDD302C], SwapEffect [0xDD3010], BackBufferCount [0xDD3004];
  - the d3d9.dll path (GetModuleFileNameW, to confirm DXVK).

**D5. Optional: 30-mode Present timing.**
- In PRESENT_STUB (stubs.asm ~774), add a branch on `g_frameLog` that calls timing-only hooks when !g_m60.
- This gives the stock render's r1/pc and the stock gap, so the 30-mode gapstall lines are comparable at the same battle state.

**D6. Optional: per-iteration frame log, FrameLog=1.**
- Write aotr60_frames.csv with one row per iteration: `t_rel_ms, rid, mode, kind(A/B/S), s(GE+0x34), lframe(GL+0x40), sub(0=halted), pre_us, chk_us, r1_us, spacing_us, pc_us, r2_us, logic_us, lw_us, post_us, wait_us, late_us, pay_us, owed_us, gap_us, flags(skip|blocked|late|fallback)`.
- About 70 B per row (≈16 MB/h). Use a 64 KB buffer flushed at each 10 s Report, and stop at 200 MB.
- This gives the period directly: max gap against lframe mod 5 and mod 10, and the beat against the 1 s / 825 ms events.

**How to read the next session**
- **Cause 1 (heavy logic sub):** gapstalls dominated by `logic` with a fixed sub, every tick, growing with time.
- **Heavy isTick A instead:** gapstalls dominated by this-A r1 with kind A1.
- **Once-per-second event:** check or pre spikes about 1 s apart. With the installed build there should be none from the display query; a remaining 1 Hz pattern with a logic spike at lframe%5 points to the AI routines 0x8F8F68 / 0x8F3EFE.
- **FIFO:** blocked Presents before gaps, and gaps on whole 8.33 ms multiples.
- **Spikes:** pre, post or pc of 50-200 ms.
- **Same work at stock or not:** compare L per sub between the 60-mode and 30-mode stretches.

**Session protocol**
- Same map and AI count as the last session, fullscreen as normally played, Telemetry=1, at least 8 minutes.
- Once the hitching is clearly visible, press Ctrl+Shift+F11 to drop to 30 for about 30 s, then back to 60. Do this twice.
- Note the in-game time when the hitch was first noticed.
- Optionally, run one windowed session on the same map.
- Snapshot %APPDATA% aotr60.log, aotr60_rates.csv, aotr60_frames.csv and rotwk\game.dat_d3d9.log before the next launch, then run tools/analyze_stalls.py.

# mitigations
The goal for all of these is to keep logic bit-exact and long-run speed at stock. Every change below is presentation or timing only.

**Implement now (pacer.cpp only, low risk; mechanisms confirmed, effects simulated)**

**M1. Budget-capped Present spacing that also runs while owed > 0.**
- In AotR60_PresentSkip, replace the `g_owed == 0` block (pacer.cpp:339-350). The new block runs when presentPacing && paced && the release was not late && lastPresent and prevRelease are set:
  ```
  target = lastPresent + (g_releaseTime - g_prevReleaseTime) - 0.25 ms   // the release cadence, T or T - pay
  target = min(target, g_releaseTime + 0.6*T)
  target = min(target, g_deadline + g_interval - pc_est - tail_est[kind, j] - 0.5 ms)
  wait only if target > now
  ```
- Estimates:
  - j = the next sub (s+1, or 1 when s == 6), read from GE+0x34 at C0.
  - For a B, tail_est = the second-largest of the last 8 measured (Present return → pacer entry) of Y iterations with the same j.
  - For an A, tail_est = EWMA(1/8).
  - pc_est = EWMA of the Present call.
- Effect:
  - A B before a predicted-heavy step gets no hold, which raises the onset threshold from L ≈ 10 to about 13 ms (simulated: 5.05 late/s → 0.45/s at L=9).
  - The ±3-4 ms A/B alternation goes away while repaying.
- Do not simply drop the wait on every B: under 120 Hz FIFO that raises the share of 1-vblank frames from about 11 % to 19 %.

**M2. Owed-proportional repay.**
- In the on-time branch (pacer.cpp:244-257): `pay = min(owed, max(g_interval/8, owed/6), slack)`. Keep the 3T clamp and the 6T cap.
- Effect: no repay-saturation debt up to L ≈ 40 ms (simulated: speed 0.963 → 0.9999 at L=40; T/4 gives 0 debt at the end case). The few frames after a hold run at 11-14 ms.
- Spike lateness beyond the 3T clamp is still written off, and that part is unavoidable.

**M3. Keep the on-change display check from 'no display queries in the frame loop'** (already done). The D1-D4 telemetry confirms that no 1 Hz cost remains in C0.

**Expected result of M1+M2:** the onset hitch is delayed by roughly 0.5-1 min of battle growth, and the irregular frames around each hold are removed. The hold itself stays about L + 10 ms. The loop order (0x6325CF before 0x6326C6/0x6326F0) means no release or repay rule can shorten it (CONFIRMED). Do not reduce or remove repayment: that makes the game 5-13 % slow (simulated), slower than stock.

**After the diagnostics (when the heavy sub, its period and L60 ≈ L30 are confirmed)**

**M4. Logic-stall fallback to 30.**
- Add stallTickRatio to PacerWindow: ticks in the 5 s window whose largest Present gap > StallFallbackMs, divided by ticks. Default StallFallbackMs = 40; 0 = off.
- Leave 60 when stallTickRatio > 0.5 for 2 consecutive windows, using the existing backoff (30 s, doubling to 480 s).
- Return to 60 only when the backoff has expired AND the predicted hold `P80_ticks(max_j L_j) + qB_est + pc_est + a_est < StallFallbackMs - 8 ms` for 2 windows. L_j is measured in 30 mode via LogicUpdateWrapper; a_est needs D5.
- Show "logic-bound fallback" in the title and log both transitions with their numbers.
- In this session it would have engaged at L ≳ 28-30 ms, about t ≈ 400 s. Stock 30 then gives 2-3× less timing error, at a speed loss stock has anyway.
- Make it configurable, since some players prefer 60 FPS with judder.

**M5 (A/B test option). Early-Y release plus skipping the B Present before a predicted-heavy step.**
- When a heavy Y is predicted (b_est + pc + tail_est[j] > T + 1 ms), release Y_k immediately after X_k and skip its B Present (the existing skip path), so A_k stays on screen for the whole pair. Keep the absolute 33.000 ms pair schedule by shifting the deadline by the borrowed time.
- The two analyses rank it differently:
  - Timeline sim: content/display timing error drops by about half (8.5 → 3.4 ms at onset, 20.4 → 10.8 at P3).
  - Mitigation sim: the hold gets about 8 ms longer (one 33 ms frame per tick).
- It trades a smaller jump for a longer hold, so it should be decided by the user's eye, not by default. It also depends on a reliable per-sub predictor, which the high measured variance may defeat.

**Optional / experimental, after a fresh audit**

**M6. Deferred mid-step B Present on a helper thread** (SplitStallPresent=0 by default).
- Needs D3DCREATE_MULTITHREADED on both CreateDevice calls: flags at 0x524190 → call 0x5241B6, AND the retry path that reloads at 0x5241EF → call 0x524222.
- Cancel or drain the deferred Present on Reset (0x522000) and on leaving 60 mode.
- The simulated hold reduction (50 → 31 ms at L=40) is an oracle upper bound. With a realistic predictor and the measured jitter it is much smaller (L=40: 37.9 vs 32.1 ms).
- Risk is high with DXVK, device loss and alt-tab.

**M7. Cheaper B-render on predicted-heavy Y iterations.**
- Saves up to b ≈ 4.5-6 ms at onset only.
- Must keep the drawable pass that fills the transform cache (C4 key, PLAN §1.2, docs/analysis/audits/transform_cache_consumers.md).

**M8. Pacing=nominal for the 120 Hz FIFO 825 ms beat.** It costs 1 % speed, so it should stay user opt-in, not the default.

**Docs:** fix PLAN §1.7. It still says "0.3 % of wall time" and "clamp 1.25·T", but the code uses a 3T clamp and a 6T cap (pacer.cpp:213, 236).

# user_questions
1. **How often does the hitch repeat?**
   - (a) a steady judder several times a second, like a stutter inside every second;
   - (b) about once a second;
   - (c) every few seconds;
   - (d) a big freeze every ~10 s.
   
   (a) is the per-tick logic hold. (b) or (c) points to the once-per-second query or the AI routines. (d) is the stock-like spikes.

2. **When did you first notice it** (minutes after the match started), and **did it get steadily worse** until you quit? Did it ever go away during quieter moments?

3. **The session ran exclusive fullscreen with vsync** (FIFO, 4K, per the DXVK log). Is that how you normally launch from the AotR launcher? Did the earlier smooth sessions run windowed? Can you play one session windowed on the same map for comparison?

4. **Is G-SYNC / VRR enabled** for this game or globally? Is the desktop at 120 Hz, or 144/240 Hz?

5. **Which map, how many AI players and which difficulty**, and roughly how big were the armies when it started? Was there a large fight about 1-2 minutes into 60 FPS mode? The data shows a short burst at that point.

6. **Did you press Ctrl+Shift+F11 to drop to 30 FPS** while it hitched? If so, did the hitch disappear, get milder, or stay the same? In the next session, please toggle to 30 for about 30 s and back, twice, once the hitch is clearly visible.

7. **Did you pause or open the menu shortly before quitting** at about 14:40? The last data window contains a pause.

8. **Which do you prefer once a battle gets very large:** an automatic switch back to 30 FPS, which is smoother but also slows slightly like stock, or staying at 60 FPS with a visible stutter? This decides the default for the stall fallback.

9. **Please run the next test with the currently installed build** ('no display queries in the frame loop', now in rotwk). Keep Telemetry=1 and play at least 8 minutes. Before launching the game again, copy aotr60.log, aotr60_rates.csv and rotwk\game.dat_d3d9.log from that session somewhere safe.