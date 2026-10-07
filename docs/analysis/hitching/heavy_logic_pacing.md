# SIM

## problems

1. **Early release without a skip makes the onset worse.** This is the 0.5 ms < overrun ≤ T/2 path (pacer.cpp:331-339 without 340-342). It does not shorten the hold; it moves the B Present earlier.
   - The shift is conservative: max-of-4 B estimate + 0.5 ms margin, giving a 6.24 ms shift against a 5.78 ms actual overrun at P1. So the B→A hold becomes T + shift (25.0 ms against 21.1 old), after an A→B frame of 6.5-8.7 ms.
   - IMMEDIATE P1: p2p 8.6→10.3 ms (steady) and 9.4→10.4 (ai); frames < 10 ms 0.20→4.32/s; gap max 28.5→34.6. FIFO P1 is neutral (9.5→9.2).
   - The 'never skip (early only)' variant is worse than old overall (sweep.py: p2p 19.9 vs 19.25, short frames 8.3/s vs 3.0).

2. **Holds get longer wherever B is skipped.**
   - The early shift is capped by the X slack, g_deadline − now ≈ 5.6-7.3 ms (pacer.cpp:333).
   - From P2 on the overrun is 12.6-25 ms, so the A-render is held for 2T + 6-20 ms: 36 / 41 / 47-53 ms, against the old B→A hold of 29-31 / 34 / 39-44 ms.
   - gap p99 rises by 7-8 ms (P2 34.0→41.7, P4 49.1→56.2). Frames > 34 ms go 0.66→4.99/s at P2.
   - The per-tick mean largest hold rises by 6.6-7.5 ms (osc.py).
   - Late releases are not reduced from P2 on (5.3-5.6/s), so owed and repay remain.
   - This is the stated trade: a smaller timing error for a longer single freeze. The 'hitch' does not go away, it changes shape.

3. **The predictor fails when the heavy work is not at a fixed position in the 10-tick cycle.** The prediction is the last value at index g_logicCalls % 60 (pacer.cpp:81-87, 488-501).
   - In the 'move' load: 3.36 B skips per second that the logic did not need, 3.53 missed heavy steps per second, frames > 1.5T 5.1→8.5/s, and p2p unchanged (15.6→14.4).
   - A 50-200 ms logic spike stored in the table is replayed as a needless skip with early release 2 s later. With stock spikes that is about 0.04/s (reviewed) or 0.18/s with an EWMA in the predictor.
   - Nothing detects or turns off a mispredicting table.

4. **Several knobs make no measurable difference.** Across 0 / 0.5 / 1.5 / 3 ms, p2p moves by at most 0.05 ms and skips are unchanged (sweep.py), because predicted overruns are bimodal: either < 0 or several ms. This covers:
   - the 0.5 ms margins (pacer.cpp:331, 442);
   - the B room cap (440-444);
   - the late-branch skip (302-309), which gives identical metrics on or off;
   - the max-of-4 B estimate (304, 330): last, mean4 and max8 are within 0.1 ms.

   The room cap omits the B Present and post-Present time (room = release + T − logic − tail − 0.5 ms). This only matters when the predicted overrun is between 0 and 0.5 ms, so it is harmless.

5. **The spacing wait is still gated on g_owed == 0** (pacer.cpp:436). The earlier recommendation to keep spacing while owed > 0 (M1) is not implemented. It matters less now that the faster repay keeps owed small: mean owed at P4 is 80.6 → 7.6 ms.

6. **The two halves of the change have separate effects.**
   - The repay change (pacer.cpp:315-324) alone fixes the speed loss: worst-case speed 0.958 → 0.990 (heavy off); at P4, -1.7 / -3.2 / -3.8 % → about 0.
   - It does nothing for judder: p2p 19.35 vs 19.25.
   - The p2p gain comes only from the B skip.

7. **No new artifacts found.**
   - No oscillation: lag-1 autocorrelation of the per-tick largest hold is between −0.18 and +0.34 for old and new alike, its spread is unchanged, and owed decays smoothly. The B-render, tail and logic estimates do not depend on release time, so there is no feedback loop.
   - No FIFO late skips in any run.
   - No extra 1-vblank frames under FIFO; there are fewer: P2 12.1→6.4/s, P3 15.4→9.0, P4 20.8→17.1.
   - With a predictable load (steady and ai), no B is skipped without need.

8. **Model limits.**
   - L carries ±3 ms.
   - The AI work is put on sub 5.
   - X slack depends on the A-render model (a ≈ 5.4-8.8 ms + 2 ms on the isTick A-render).
   - Whether a clean 33-50 ms A hold looks better than old's 29-44 ms hold plus catch-up has to be judged by eye.

## tuning

Evidence: heavysim sweep.py, sweep2.py, sweep3.py and sweep4.py, with outputs in out_sweep*.txt.

**Recommended rules**

(a) **Release Y early only together with a B skip.**
- In pacer.cpp:333-339, apply the shift only inside the skip branch. For overruns below the skip threshold, keep the late release plus the new repay.
- Gain at T/2: IMMEDIATE P1 p2p 10.6→7.9 ms, short frames 4.2→3.2/s (sweep.py).

(b) **Skip threshold g_interval/4 instead of /2** (pacer.cpp:305, 340).
- With (a) and the EWMA predictor, P1 p2p is 4.2 at T/2, 3.2 at T/4 and 3.2 at 0 (ai, IMMEDIATE).
- Rand P1: 5.7 at T/2, 3.9 at T/4.
- B skips the logic did not need, per second, at 0 / 0.15 / 0.25 / 0.35 / 0.5 T: 0.35 / 0.12 / 0.03 / 0.02 / 0.01 (ai) and 0.95 / 0.53 / 0.36 / 0.27 / 0.18 (rand). T/4 is the knee.
- At P2+ the threshold does not matter, because every heavy step overruns by more than T/2.

(c) **Predictor = max(value 10 ticks back at the same position, per-sub EWMA with α = 1/4).**
- Implement the EWMA as ew[g_logicCalls % 6] updated in PacerOnLogicCall (pacer.cpp:488-501).

| Load | Predictor | Needless skips/s | Missed skips/s | p2p (ms) |
|---|---|---|---|---|
| ai | last | 0.00 | 0.16 | 9.47 |
| ai | ewma_max | 0.03 | 0.03 | 9.57 |
| rand | last | 0.27 | 0.40 | 10.74 |
| rand | ewma_max | 0.36 | 0.13 | 10.19 |
| rand | oracle (ceiling) | 0 | 0 | 10.10 |
| cal | last | 0.28 | 0.36 | 9.78 |
| cal | ewma_max | 0.30 | 0.07 | 8.65 |

- Rejected alternatives:
  - **max of the last 2 at the same position:** more needless skips (rand 0.46, move 4.35/s) for little gain.
  - **EWMA alone:** smoothes out the 5- and 10-tick AI peaks (ai missed 0.12/s).

(d) **Add a hit-rate guard.**
- In the learning block (pacer.cpp:239-245), for each Y that was predicted to overrun by more than the skip threshold, record whether the actual overrun (latest g_bRender + g_iterLogic + tail − g_interval) was > 0.
- Allow the skip, and with (a) the early release, only while at least 6 of the last 8 such predictions came true.
- Effect:
  - Free when the predictor is right: steady, ai and rand p2p change by ≤ 0.1 ms, and missed skips 0.13→0.21/s.
  - Rescues the 'move' case: needless skips 3.4-5.7 → 0.1/s, gap p99 38.4 → 33.2 ms, and p2p back to old (15.0 vs 14.96).
- The guard needs (a). With the reviewed early-without-skip, 'move' gets worse with the guard: p2p 16.5, short frames 6.5/s.

(e) **Keep these as they are:**
- the 0.5 ms margins, the room cap and the max-of-4 B estimate (all insensitive: within ±0.05 ms p2p across 0-3 ms margins and the last / mean4 / max8 estimates);
- the late-branch skip (no effect);
- the new repay (it alone removes the 1.7-3.8 % speed loss at P4).

**Result of the tuned rules** (out_final.txt)

| Case | old | reviewed | tuned |
|---|---|---|---|
| P1 IMMEDIATE p2p, steady | 8.6 | 10.3 | 2.8 |
| P1 IMMEDIATE p2p, ai | 9.4 | 10.4 | 3.3 |
| P1 IMMEDIATE p2p, rand | 12.3 | 9.5 | 4.3 |
| P1 FIFO p2p, steady | 9.5 | 9.2 | 1.8 |

- P2-P4 are the same as the reviewed rules (7.3 / 11.6 / 21.3 ms IMMEDIATE, steady).
- Cost at P1: one 33 ms A frame per tick (frames > 1.5T 1.9 → 5.0/s, gap p99 26 → 33.5 ms) instead of a 21 ms hold plus compressed frames.

**Not solvable by release or skip rules:** the hold after the heavy step is at least L + about 10 ms whether or not B is shown, so from P2 on any variant leaves a 36-53 ms freeze per tick. Shortening that hold, or switching back to 30 at that point (M4 in the earlier synthesis), is a separate measure. Validate on the real game:
- the CSV columns heavy_predicted / heavy_early / heavy_skips / heavy_early_ms, which should show about 5 skips/s at P2+ and fewer than 0.3/s needless;
- the 1 Hz AI pattern, by eye, with HeavyLogicPacing on and off.

## sequences

Displayed Present-to-Present intervals for one logic tick, starting at the release of X1. Under FIFO they are the vblank-quantised display intervals. The heavy sub-step 5 runs in Y4, after B4. "S" = B Present skipped. Source: heavysim\out_seqs.txt, seed 21, t ≈ 20 s.

**IMMEDIATE**
- P1 onset, steady:
  - old: A1:- B1:16.3 A2:16.3 B2:16.3 A3:16.3 B3:16.3 A4:16.3 B4:16.3 A5:21.1 B5:14.6 A6:16.5 B6:12.0 (next A1:19.6)
  - new: A1:- B1:16.3 A2:16.3 B2:16.3 A3:17.1 B3:16.3 A4:16.3 B4:8.7 A5:25.0 B5:16.3 A6:16.3 B6:16.3
    - The early release only moves B4 forward: an 8.7 ms frame followed by a 25 ms hold, versus 21.1 ms + 14.6/12.0 compression in old.
  - tuned: ... A4:16.3 B4:S A5:32.8 B5:16.3 A6:16.3 B6:16.3
    - A4 is held one stock frame, with exact timing.
- P2 mid, steady:
  - old: A1:- B1:9.9 A2:16.7 B2:16.3 A3:17.6 B3:16.3 A4:16.3 B4:16.3 A5:29.2 B5:13.9 A6:14.2 B6:14.1 (A1:19.1)
  - new and tuned: A1:- B1:16.1 A2:16.3 ... A4:16.3 B4:S A5:36.4 B5:13.4 A6:15.1 B6:16.3
- P3, steady:
  - old: ... A4:16.3 B4:16.3 A5:33.6 B5:10.9 A6:16.4 B6:13.0, with 9.9/11.1 frames in sub 1-2
  - new: ... A4:16.3 B4:S A5:41.4 B5:10.7 A6:17.7 B6:11.8; the rest of the tick is a clean 16.3
- P4 end, steady:
  - old: B1:7.9 A2:17.9 B2:11.5 A3:17.1 B3:10.2 A4:18.9 B4:12.4 A5:44.0 B5:8.8 A6:17.6 B6:11.3
    - Repay saturation leaves alternating 8-12 / 17-19 frames all tick.
  - new: B1:9.7 A2:17.1 B2:12.6 A3:18.3 B3:16.3 A4:16.3 B4:S A5:47.3 B5:9.5 A6:20.9 B6:9.8
    - The next tick reaches A5:52.6.
- AI tick (+23 ms) at P0:
  - old: ... B4:16.3 A5:37.2 B5:11.7 A6:16.8 B6:11.7
  - new: ... B4:S A5:41.7 B5:12.7 A6:14.9 B6:14.2
- AI tick at P1:
  - old: ... A5:45.8 B5:13.2 A6:15.0 B6:13.3
  - new and tuned: ... B4:S A5:52.1 B5:11.6 A6:16.3 B6:11.6
- AI tick at P3:
  - old: B1:9.5 A2:17.8 B2:10.9 ... A5:57.3 B5:12.7 A6:17.1 B6:12.4
  - new: B1:16.3 ... B4:S A5:63.5 B5:9.9 A6:15.1 B6:9.2
  - tuned: ... B4:S A5:65.4 B5:7.5 A6:12.5

**FIFO 120 Hz**
- P1, steady:
  - old: ... B4:16.7 A5:25.0 B5:8.3 A6:16.7 B6:16.7
  - new: ... B4:8.3 A5:25.0 B5:16.7 ...
    - Same frames, swapped order.
  - tuned: ... B4:S A5:33.3 B5:16.7 A6:16.7 B6:16.7
- P2, steady:
  - old: B1:8.3 A2:16.7 B2:16.7 A3:25.0 B3:8.3 A4:16.7 B4:16.7 A5:33.3 B5:16.7 A6:8.3
  - new: B1:16.7 A2:16.7 B2:16.7 A3:8.3 B3:16.7 A4:16.7 B4:S A5:41.7 B5:8.3 A6:16.7 B6:16.7
- P3, steady:
  - old: B1:8.3 A2:16.7 B2:8.3 A3:25.0 B3:16.7 A4:8.3 B4:16.7 A5:33.3 B5:16.7 A6:16.7 B6:8.3
  - new: B1:8.3 A2:16.7 B2:16.7 A3:16.7 B3:16.7 A4:16.7 B4:S A5:41.7 B5:8.3 A6:25.0 B6:8.3
- P4, steady:
  - old: B1:8.3 A2:16.7 B2:8.3 A3:16.7 B3:8.3 A4:25.0 B4:8.3 A5:50.0 B5:8.3 A6:16.7 B6:8.3
  - new: B1:8.3 A2:16.7 B2:16.7 A3:16.7 B3:16.7 A4:16.7 B4:S A5:50.0 B5:8.3 A6:16.7 B6:8.3
    - The next tick reaches A5:58.3.
- AI tick at P0:
  - old: ... B4:16.7 A5:33.3 B5:16.7 A6:16.7 B6:8.3
  - new: ... B4:S A5:41.7 B5:8.3 A6:16.7 B6:16.7
- AI tick at P1:
  - old: ... A5:50.0 B5:8.3
  - new: ... B4:S A5:50.0 B5:16.7 A6:16.7 B6:8.3
- AI tick at P3:
  - old: ... B4:16.7 A5:58.3 B5:16.7 A6:16.7 B6:8.3
  - new: ... B4:S A5:66.7 B5:8.3 A6:16.7 B6:8.3

**Pattern**
- Old: B4 is shown on time, then a hold of T + overrun, then compressed frames while owed time is repaid.
- New: one A frame held for 2T + residual, and the rest of the tick stays clean.
- The early shift is capped by the X slack. Mean shift vs mean actual overrun (out_shift.txt):

| Phase | Mean shift (ms) | Mean actual overrun (ms) |
|---|---|---|
| P1 | 6.24 | 5.78 |
| P2 | 6.24 | 12.57 |
| P3 | 7.31 | 18.15 |
| P4 | 5.63 | 25.15 |

So from P2 on, 6-20 ms of each overrun is still late and owed.

## verdict mixed

# SIMCHECK

## port_mismatches
I found no mismatch that changes a result. I read the working-tree <game folder>\aotr60\src\pacer.cpp (1-501), the frame_ctl.cpp and telemetry.cpp hunks of the diff, and heavysim\hsim.py, line by line.

**Matches**
- Learning block: hsim.py:127-142 = pacer.cpp:237-252, including the int64 truncation of the yTail update (trunc_div).
- Max-of-4 over a ring of 8 = g_bRender[4].
- Late branch: hsim 164-188 = cpp 279-310.
- Repay, early release and skip: hsim 189-223 = cpp 311-348, including shift = min(overrun, g_deadline − now) with the unshifted deadline, and 0.5 ms = FREQ/2000.
- Heavy skip before the FIFO late skip: hsim 243-250 = cpp 411-428.
- Spacing wait with the B room cap and the earlyRid exclusion: hsim 251-265 = cpp 436-454.
- Hooks: hsim 284-295 = cpp 482-501.

**Hook placement is right**
- PacerOnPostRender runs at the end of clientUpdate (frame_ctl.cpp:438). The sim calls post_render after b_post, before the logic step.
- The X iteration makes no logic call. HALT_STUB (stubs.asm:219-227) takes the stepper's halted branch, so exactly 6 LogicUpdateWrapper calls (telemetry.cpp:600-629) happen per tick. That keeps g_logicCalls % 60 a 10-tick cycle.

**Self-test re-run passes**
- Old pacer in the new driver = tsim.run60, bit for bit.
- heavy=0 with the old repay = old pacer.
- heavysim\tsim.py has the same SHA-256 as timeline\tsim.py.

**Not modelled (all inert for these scenarios)**
1. The staleness gate in PredictNextLogic (cpp:83-85). The sim runs logic every Y, so the gate never fires.
2. Living World addToLast (cpp:490-494). Not relevant in a skirmish.
3. The g_gapDevLost / g_gapReset gaps and EvaluateWindow / fallback (cpp:261, 354). The fallback cannot fire at these rates.
4. The camera forced skip (cpp:393).
5. Pacing=Nominal rounding. The default is stock (config.h:14).

**Two caveats about the harness, not the port**
- (a) The 'needless' / 'missed' labels (hsim 421) take the actual overrun of a skipped Y without its Present cost (0.15 ms). This is negligible.
- (b) The p2p judder metric (tsim.py:443-470) scores only frames that are shown. A skipped B removes a sample. The 2T hold it creates moves no content off its time, so p2p never penalises it.
  - Trace (heavyverify\v3.py): a replayed spike gives "B1:S A2:32.0", and the error stays flat at +29 ms.
  - So p2p alone flatters every skip variant; only the gap metrics show the cost.

## rerun
All scripts and outputs are in <analysis workspace>\hitch\heavyverify\: v1-v5.py and out_v1.txt, out_v2_*.txt, out_v4.txt, out_v5.txt. Unless noted, each case ran 3 fresh seeds (61-63) of 40 s each. I added a 'repay' config: the new code with HeavyLogicPacing=0, which keeps the new repay.

**1. Key cells reproduce within noise (out_v1.txt)**
- IMMEDIATE, steady load, p2p (ms):

| Phase | old | new | tuned |
|---|---|---|---|
| P1 | 8.4 | 10.3 | 2.8 |
| P2 | 15.4 | 7.2 | 7.1 |
| P4 | 28.2 | 20.7 | 20.6 |

- Gap p99, steady: P2 33.6 → 41.4 ms, P4 48.9 → 55.7 ms.
- Frames < 10 ms at P1: 0.22 → 4.64/s (new).
- Speed at P4: −1.47 % → +0.02 %.
- move load, new, P2: 3.18 B skips/s the logic did not need, 3.43 heavy steps missed/s, frames > 1.5T 5.05 → 8.40/s.
- FIFO 120 Hz, steady, p2p: P2 17.7 → 9.3, P4 25.8 → 18.0.
- ai and rand loads match the analyst's table to ±1 ms.

**2. Logic-cost sweep L = 17…23 ms (cv 0.15, out_v2_sweep.txt)**
- The reviewed code flips between skip and no-skip from tick to tick around overrun ≈ T/2:
  - B skips 1.2-4.3/s;
  - 0.5-1.15 heavy steps missed per second.
- IMMEDIATE p2p, old → new: L17 8.6 → 9.6 (worse), L18 9.4 → 8.4, L19 10.5 → 5.6, L23 14.3 → 5.0.
- Tuned stays at 3.0-4.7 throughout.

**3. Other heavy positions (out_v2_subs.txt)**
- Heavy step on sub 1 or sub 2, or two heavy steps per tick (5+6, 2+5): the same pattern. p2p falls 25-50 %, gap p99 rises 6-9 ms.
- Sub 5+6 at 22+22 ms: old loses 2 % speed; new loses nothing.

**4. Spikes at 0.5/s (out_v2_spikes.txt)**
- At P0, replays of spikes stored in the table cause 0.36 needless skips/s.
- IMMEDIATE p2p90: 30.5 → 42.1; gap p99: 34.0 → 41.3 ms.
- The tuned guard brings these back: p2p90 22.2, gap p99 24.2.

**5. Other present modes (out_v4.txt, ai load)**
- Mailbox 120 Hz and FIFO 144 Hz look like FIFO 120.
- FIFO 60 Hz is a regression (see worse_cases).

## verdict_holds
True

## worse_cases
Cases where the new rules (as reviewed, and in several cases the tuned ones too) do worse than the old pacer:

**1. Onset (L ≈ 17-18 ms), under IMMEDIATE**
- Early release without a skip (pacer.cpp:331-339) moves B forward: B4 8.7 ms, then A5 25 ms.
- p2p 8.4 → 10.3 ms; frames < 10 ms 0.22 → 4.64/s; gap max 28 → 34 ms.
- Confirmed. The fix is the analyst's rule (a).

**2. From P2 on: a longer single freeze**
- Every tick has one A frame held for 2T plus the residual overrun:
  - gap p99 rises 7-8 ms (P2 33.6 → 41.4, P4 48.9 → 55.7);
  - frames > 34 ms go 0.42 → 5.0/s at P2.
- Late releases are not reduced. Confirmed, and it holds for any skip variant.

**3. Heavy work that moves between sub-steps ('move' load)**
- 3.2 B skips/s the logic did not need; frames > 1.5T 5.05 → 8.4/s; no gain in p2p.

**4. A logic spike replayed from the table**
- 10 ticks after a spike, the table predicts it again and skips a B for nothing, with an early release.
- That turns a clean 16.3 ms cadence into a 30-32 ms hold (v3 trace).
- At 0.5 spikes/s: P0 p2p90 30.5 → 42.1 ms.

**5. FIFO at 60 Hz (new, not covered by the analyst), out_v4.txt, ai load**
- With the old pacer, the 3-deep swap queue absorbs the logic overrun completely: gap p99 16.7 ms, 0-1.1 frames > 1.5T per second.
- Reviewed and tuned rules both:
  - drop a frame the queue would have shown;
  - give gap p99 44-50 ms and gap max 67 ms;
  - give 3.6-3.9 holds of 33 ms per second.
- A gate on g_presentsSinceBlock does not help (v5): the skips drain the queue, so Presents stop blocking and the gate reopens.
- A static gate is needed: no heavy skip under FIFO when refresh < about 2 × 60.6 Hz.

**6. The repay change on its own (pacer.cpp:314-324)**
- It applies even with HeavyLogicPacing=0, and its cost was not reported where the skips do not absorb the overrun. At P4, 'repay' vs old:
  - FIFO 120 p2p: steady 25.8 → 30.9, ai 28.8 → 33.5, rand 29.3 → 32.8, move 26.3 → 33.5;
  - IMMEDIATE frames < 10 ms: 8.4 → 13.0/s (steady), 8.3 → 14.1/s (ai).
- It is a speed-versus-smoothness trade: the old pacer threw the time away as debt (−1.5 to −3 %).
- The tuned guard on the 'move' load therefore ends up worse than old at P4 FIFO: p2p 33.5 vs 26.3 ms (mine); 30.1 vs 26.2 in the analyst's own out_final.txt:141-143.

## corrections
The 'mixed' verdict holds, and so do tuning rules (a)-(d): skip-only early release, T/4 threshold, the EWMA-max predictor and the guard. Tuned is never worse than reviewed in my runs, and it fixes the onset regression, flapping near the T/2 boundary and the spike replays. Corrections and additions:

**1. "The guard brings the 'move' case back to old-like behaviour" is true only up to P3**
- At P4 under FIFO it is 4-7 ms worse in p2p than old (out_final.txt:141-143; out_v1.txt move P4).
- The cause is the new repay, not the skip.

**2. "The repay change alone … does nothing for judder" (p2p 19.35 vs 19.25) hides a phase-dependent cost**
- At P4 the repay alone worsens FIFO p2p by 4-7 ms and adds about 5 compressed frames/s under IMMEDIATE.
- It removes the 1.5-3 % speed loss; the old pacer dropped that time as debt.
- So it is a trade, not a free fix.
- Consider the larger repay only when owed is near the cap, or keep T/8 and accept the debt.

**3. "Settle by eye using the HeavyLogicPacing switch" is not a clean A/B**
- HeavyLogicPacing=0 still runs the new repay (pacer.cpp:314-324 is not gated on g_cfg.heavyLogicPacing).
- At P4 that 'off' state judders more than the committed pacer.
- Gate the repay on the same switch, or add a separate switch, before the eye test.

**4. Add a static display gate**
- Disable the heavy skip and early release under FIFO when the refresh rate is below about 2 × 60.6 Hz; a 60 Hz FIFO display shows a clear regression.
- The refresh rate is already known from DisplayBlockReason in frame_ctl.cpp.
- A g_presentsSinceBlock-based gate does not work (v5).

**5. Read the p2p gains with caution**
- The metric does not count the 2T hold a skip creates (tsim.py:443-470).
- Judge the gap metrics (p99, > 34 ms per second) alongside p2p.
- From P2 on, those get worse with every skip variant.

**6. Optional robustness: index the table by the logic frame instead of the call count**
- Use (logic frame from logic+0x40 % 10) × 6 + sub − 1 (telemetry.cpp:631-632) rather than g_logicCalls % 60.
- The AI periods follow the logic frame number; this would survive any dropped or extra call. The model cannot test it.

**7. The analyst's remaining claims check out**
- Port exactness, no oscillation, no FIFO late skips at 120 Hz.
- The 0.5 ms margins, the room cap and max-of-4 make no measurable difference.
- From P2 on, a 36-53 ms hold per tick remains whatever the skip/release rules.

# REVIEW

## overall

I found no correctness bugs in the schedule, game logic, speed, saves, threading or the CSV. The problems are the quality and robustness of the predictor that drives the new early releases and skips.

**Checked and sound:**
- **Deadline schedule:** the on-time branch moves only the release (pacer.cpp:327-346, release = g_deadline - shift). g_deadline and g_owed stay unchanged, so the next X deadline is exact and nothing is owed. In the ported simulator the long-run speed is 1.000 at every phase, and 0.985 -> 1.000 at P4 from the faster repay at :315-326. Pay is bounded by owed and slack, and pay + shift <= slack.
- **Iteration kind:** nextIsY is correct when set. 60 mode can only stop before an X (frame_state.cpp:6), and an engine reset clears g_skipRid/g_earlyRid through LeaveSixty -> PacerOnModeChange (:205-207). g_lastRenderB is set before OnPostRender clears g_inB (frame_ctl.cpp:438 vs :450). 30 mode never reaches AotR60_Pacer (stubs.asm PACER_CAVE).
- **Skip ordering:** the heavy skip comes after the forced skip and before the FIFO late skip. It needs paced && bRender, does not touch g_lastPresent, g_presentCall or g_presentsSinceBlock, and is deliberately left out of the fallback late-skip ratio.
- **Logic model:** integer-only arithmetic. The overflows are irrelevant (2^32 calls is ~4.5 years). The extra QPC reads at Telemetry 0 are harmless: g_it/g_tw keep growing, but as int64.
- **Threading:** everything runs on the main thread. GameLogic::update is called only through the vt34 slot from the stepper (sites.json), and PresentSkip is guarded.
- **Telemetry:** Report reads p/q before g_prevPacer is updated (telemetry.cpp:267-303). The format has 15 timing columns, matching the 15 header columns. The stall diagnostics from 'no display queries in the frame loop; frame-timing stall diagnostics' (TelemetryIterationBegin/OnPostRender) have no defects.

**Problems with the prediction:**
- **Pause and frozen time:** failed tick attempts are counted as logic calls, about 30/s. This breaks the 'logic not running' test and shifts the 60-call cycle after every pause.
- **Too many false positives:** the single-sample predictor, plus max-of-4 B-renders, plus the tail estimate, is wrong most of the time at onset. In the simulator, 80 of 85 early releases and 11 of 11 skips at P0 were not needed. Frame pacing at P0/P1 comes out worse than the old pacer: P1 long frames/s 2.1 -> 3.7 and iv max 50 -> 60-67 ms. It only helps from P2 on (judder p2p50 15.5 -> 9.6, 20.5 -> 12.1, 27.2 -> 20.5 ms).
- **No outlier handling:** one spike in a B-render or Y tail, including device-lost or alt-tab gaps, forces 4 consecutive B skips.
- **Present blocking is learned:** time blocked in a full FIFO queue counts as B-render time, which an early release cannot recover. This should feed back on 60 Hz vsync displays; it does not trigger in the 120 Hz test session.
- **Smaller items:** the spacing cap ignores the Present call and post-Present work; the first Y after mode-on or a gap is never protected; model state is not reset on mode change or load; one stats column mixes two thresholds; DisplayBlockReason caches the refresh rate per parameter signature only.

**Recommended before default-on:**
- Key the model by (logic frame % 10, sub) and record only calls that advanced.
- Predict min(last 2 cycles). In the simulator this cut P0 early releases from 0.73 to 0.04/s and P1 skips from 1.46 to 0.44/s, and kept most of the P2-P4 gain.
- Sanitize and clamp the learned samples, and exclude Present blocking from them.
- Clear the model state on mode-on and reset.
- Treat HeavyLogicPacing=1 as an A/B option until the per-sub telemetry (D3) confirms the variance.

Simulator and outputs are in <analysis workspace>/hitch\review_heavy\: nsim.py (exact port of the working-tree pacer on tsim's workload and display), cmp.py/cmp2.py with out_cmp.txt/out_cmp2.txt/out_cmp3.txt (old vs new vs variants), acc.py with out_acc.txt (prediction precision), spike.py with out_spike.txt (spike contamination and cycle shift).

## [medium/confirmed] Failed tick attempts during pause or frozen time are counted as logic calls. This breaks the 'logic not running' test and shifts the 60-call cycle.
LOC: src/telemetry.cpp:625-629 (LogicUpdateWrapper -> PacerOnLogicCall(d,false)); src/pacer.cpp:81-87 (PredictNextLogic), :488-499 (PacerOnLogicCall)
SCENARIO: While the game is paused, in the ESC menu or in a frozen cinematic, the stock step in every Y still calls GameLogic::update(1) through vt34. That is one failed tick attempt per pair, about 30/s (docs/analysis/reviews/review_speed_pacing.md:45; docs/analysis/gaps/G5_modes.md:36). Each attempt sets g_logicCallRid = g_renderId and does ++g_logicCalls (pacer.cpp:496-498). Consequences: (a) the 'logic not running' test at pacer.cpp:83 never fires in pause or frozen time, so the comment at :80 is wrong. For the first ~2 s of a pause in a large battle, the heavy positions keep early-releasing Ys and skipping B Presents, so the paused picture runs at 30 FPS in those pairs. (b) Each pause advances the cycle by an arbitrary number of calls. After unpausing, the 60 slots are misaligned with (logic frame, sub) and hold pause-call durations for up to 10 ticks (2 s). Heavy-step protection is then off or applied to the wrong Y. Simulated: a single one-call misalignment at P1 raised displayed intervals >24.75 ms from 22 to 30 over the next 4 s (review_heavy/out_spike.txt).
FIX: Key the table by (GL+0x40 % 10, sub) instead of by the running call count, and record only calls that advanced. For sub 1, compare GL+0x40 before and after the call, or use the wrapper's return value. A failed attempt goes to its own 'attempt' estimate. In AotR60_Pacer, find the next sub from the stepper value Field<int32_t>(engine,0x34): s<6 means sub s+1, s>=6 means a sub-1 attempt. Predict the attempt cost (or 0) when the last sub-1 attempt failed.

## [medium/suspected] The single-sample predictor (+ max-of-4 B, + yTail) produces mostly false positives at onset, so frame pacing at P0/P1 is worse than the old pacer
LOC: src/pacer.cpp:81-87, :302-309, :328-344 (overrun = MaxOf(g_bRender,4) + g_logicAt[pos] + g_yTail - T)
SCENARIO: Ported the working-tree pacer exactly into review_heavy/nsim.py and ran it on the calibrated V3 workload (heavy sub 5 lognormal cv 0.3, sub 1 6 ms cv 0.7). Prediction accuracy over 120 s at FIFO 120 Hz (out_acc.txt): at P0, 80 of 85 early releases and 11 of 11 B skips were not needed, while 131 of 135 real overruns were missed (median of predicted minus actual = +8.4 ms). At P1, 125 of 165 skips were not needed and 116 of 156 real overruns >T/2 were not skipped. Display effect, mean of 3 seeds (out_cmp2.txt): P1 IMMEDIATE long frames/s 2.10 -> 3.70, off-nominal/s 3.22 -> 6.83, iv max 50 -> 60 ms; P1 FIFO iv max 50 -> 66.7 ms; P0 IMMEDIATE p2p90 4.9 -> 7.0 ms. Gains appear only from P2 on (p2p50 15.5 -> 9.6, 20.5 -> 12.1, 27.2 -> 20.5 ms for P2/P3/P4). Every false early release still shortens the A frame by the shift: A->B = T - shift + b_pre - a_pre.
FIX: Require the evidence to repeat. Keep two cycles (int64 g_logicAt[2][60]) and predict min(last two at this position). Optionally use the second-largest of the 4 B samples instead of the maximum. Simulated with min2: P0 early releases 0.73 -> 0.04/s; P1 skips 1.46 -> 0.44/s; P1 long/s 3.70 -> 2.60 and iv max 60 -> 51 ms; most of the P2-P4 judder gain is kept (P3 p2p50 12.9 vs 12.1, old 20.5). Changing the skip criterion to the predicted A->B gap, or capping the shift (skipmode gapskip/capshift, out_cmp3.txt), made little difference. The predictor is the lever. Validate against real per-sub data (D3) before shipping default-on.

## [medium/confirmed] Learned B-render/tail samples are not sanitized: one spike forces several consecutive B skips and early releases
LOC: src/pacer.cpp:239-249 (learning runs before the gap/late tests at :261-278), :241 (g_bRender), :242-244 (g_yTail EWMA 1/8)
SCENARIO: Any long B iteration is learned. This includes a stock-like 55-210 ms hitch inside a B-render, the device-lost Sleep(200) at 0x522696 inside Present, a Present blocked for a full FIFO queue, an alt-tab, or a 'gap' iteration with late > 250 ms. MaxOf(g_bRender,4) keeps such a sample for the next 4 Ys, and g_yTail decays by only 7/8 per Y. Simulated on a deterministic P0/P1 workload (out_spike.txt): one +60 ms or +150 ms B-render gives 4 consecutive B Presents skipped (A held for 4 pairs, about 132 ms at 30 FPS) right after the hitch. A +150 ms tail spike gives 2-4 skips over 0.2-0.5 s plus early releases. Each spike is therefore followed by a second, self-inflicted stutter.
FIX: Do not learn from iterations that end in the gap branch or that run late by more than about T/4. Clamp each sample to about 1.5T before storing it. Use the second-largest of the last 4 B samples, or a median, instead of the maximum, and clamp the tail sample before the EWMA, for example min(tail, T/2).

## [medium/suspected] The B duration includes time blocked in Present, which an early release cannot recover; with a full FIFO queue (60 Hz vsync) this feeds back on itself
LOC: src/pacer.cpp:241 (g_postRender - g_releaseTime - g_iterSpacingWait includes the Present call), :330-342
SCENARIO: On a 60 Hz FIFO display the swap queue fills: the pacer renders 60.6/s against 60 Hz, which is the reason the late-skip at :422-427 exists. Present then blocks until the next vblank, for up to ~16 ms depending on the beat phase. That blocking is learned as B-render time, so the predicted overrun exceeds 0.5 ms or T/2 on light positions too. The pacer then releases Y early, but the earlier B Present just blocks longer, so nothing is gained, and the larger samples keep the prediction high. Overruns above T/2 skip the B Present, which drops toward 30 FPS. These skips are not counted in g_windowLateSkips, so the fallback does not see them. The 120 Hz test session had late_skips=0 and no Present over 1 ms, so it does not trigger there. It is expected on 60 Hz vsync setups.
FIX: Remove the blocked part of the Present call from the B sample. In AotR60_PresentDone, record took for the current rid. When learning, subtract max(0, took - 1 ms), or measure B as (release -> Present submit) + (Present return -> OnPostRender). Optionally disable early release while g_presentsSinceBlock < 2.

## [low/confirmed] The B spacing cap ignores the B's Present call and post-Present work
LOC: src/pacer.cpp:440-444
SCENARIO: room = release + T - logic - yTail - 0.5 ms assumes the logic starts as soon as the spacing wait ends. In fact the Present call and the rest of clientUpdate (r2, until OnPostRender) come first, and g_yTail starts at g_postRender, so neither is counted. With the model's 0.15 ms Present and 0.3 ms b_post, the 0.5 ms margin is used up, and with any Present blocking the logic step starts late again.
FIX: Keep an EWMA of (g_postRender - g_presentCall) for B iterations and subtract it in room, or subtract g_bRender minus the B's pre-Present time.

## [low/confirmed] The first Y after mode-on and after every gap is never protected; the gap branch ignores the prediction
LOC: src/pacer.cpp:239 (nextIsY requires g_releaseTime != 0 && g_pacerRanRid == g_renderId-1 && g_postRender > g_releaseTime), :203, :264-268
SCENARIO: PacerOnModeChange sets g_releaseTime = 0 (:203), so the pacer at the end of the first X sees nextIsY == false. After an unpaced iteration (camera M>1 unlimited path, device lost/reset, alt-tab) the next Y is also unprotected, and the gap branch never sets g_skipRid. The cost is small, one Y per event, but the iteration-kind test is tied to the learning preconditions without any need.
FIX: Derive nextIsY only from g_lastRenderB == false && g_m60 (the X/Y parity cannot change after an X, per FrameState::BeginIteration). Keep the extra conditions for learning only. Optionally apply the skip test in the gap branch too.

## [low/confirmed] Model state survives mode switches, resets and save loads
LOC: src/pacer.cpp:189-208 (PacerOnModeChange clears g_earlyRid/g_skipRid/g_postRender only); src/frame_ctl.cpp:453-467 (OnEngineReset)
SCENARIO: g_logicAt, g_logicCalls phase, g_bRender, g_yTail and g_lastRenderB carry over from the previous battle. After loading a save in a large battle, 60 mode starts after m_frame >= 8, so 48 of the 60 slots are fresh. The other 12 slots and the B/tail estimates still hold the old battle's values, and a reset that happens mid-tick also misaligns the cycle. The result can be a few spurious early releases or B skips in the first 2 s.
FIX: Clear g_logicAt, g_bRender, g_yTail and g_lastRenderB in PacerOnModeChange(true) and on engine reset. Re-align the cycle at reset; this is automatic with (frame, sub) keying.

## [low/confirmed] heavy_predicted mixes two thresholds; heavy skips extend present_spacing_max
LOC: src/pacer.cpp:305-306 vs :331-332; :411-416 with RecordPresentSpacing :370-382; src/telemetry.cpp:294
SCENARIO: In the late branch predictedLate is counted only when overrun > T/2; in the on-time branch it is counted when overrun > 0.5 ms. The CSV column heavy_predicted is therefore not one quantity. A heavy skip leaves g_lastPresent at the A Present, so the next A's spacing becomes A->A (>= 33 ms + residual) and present_spacing_max_ms reads like a stall. tools/analyze_stalls.py and the CSV readers need to know this.
FIX: Count predictedLate with the same threshold in both branches, or split it into predicted_over and predicted_skip. Add a column for A->A spacing across skipped Bs, or document it.

## [low/confirmed] DisplayBlockReason never re-checks the OS refresh rate when the D3D parameters stay the same
LOC: src/frame_ctl.cpp:54-105 (commit 'no display queries in the frame loop')
SCENARIO: [0xDD3028] is always 0, so the OS query runs once per parameter-signature change, in fullscreen too. The current comment already says so. In windowed mode, moving the window to a 50 Hz monitor or lowering the desktop refresh rate keeps the old 'allowed' decision for the rest of the session. Before this commit it was re-checked every second. The FIFO late-skip limits the damage, but the PLAN 1.7 rule is no longer enforced.
FIX: Also invalidate the cache at mode-on (PacerOnModeChange(true)), on g_gapReset (Reset_Device), and when the window's monitor changes (MonitorFromWindow is cheap; skip GetMonitorInfo/EnumDisplaySettings unless the HMONITOR changed). Never do this per frame.

## [low/confirmed] Design note: an early release does not shorten the visible hold; it moves the overrun into a short A frame
LOC: src/pacer.cpp:328-344, :436-437
SCENARIO: Algebra, checked in the simulator: the B->next-A display interval is T + shift + (a_pre - b_pre) with early release and T + overrun + (a_pre - b_pre) without it. These are equal when shift = overrun. What changes is that A->B shrinks to T - shift + b_pre - a_pre (e.g. 13 - 6 = 7 ms) instead of the compressed repay frames afterwards. The B skip makes the longest single interval longer: P4 IMMEDIATE iv max 83.6 -> 112.7 ms, iv p95 41.5 -> 49.6 ms. In exchange, content/display judder falls at P2-P4 (verr 3.1 -> 2.3, 4.1 -> 2.8, 6.1 -> 4.9).
FIX: This is not a code bug. Make the default a deliberate choice, since this is the M5 trade-off the synthesis said to decide by eye. Consider default 0 until the A/B test, or require min2 evidence (finding 2) before skipping.