# AREA sim

## SUMMARY

I extended the calibrated simulator with both deferred-B designs: (T) a presenter thread and (C) single-threaded checkpoints. The old pacer stays as the baseline and is reproduced bit for bit (selftest, 30 cases against hsim.run / tsim.run60). All results are model output, so INFERRED.

**What a helper thread alone does.** Handing B's Present to a helper thread splits the long hold into two medium holds, but only when B is presented at the midpoint of the predicted A_k→A_{k+1} gap.
- Presenting B at its normal time (T0, i.e. only dropping the spacing wait before logic) gains just 0-3 ms.
- Midpoint target, 'ai' load, IMMEDIATE, gap p99 for P1..P4: 33.5/41.1/47.1/56.4 → 30.1/33.8/36.0/39.2 ms.
- Same case, frames > 34 ms per second: 0.56/1.49/4.55/4.88 → 0.00/0.58/0.94/1.25.

**What makes the real difference.** Once B's display time is decoupled from its render time, the Y iteration can be released right after its X (spending X's idle slack on the logic step). This shortens A_k→A_{k+1} itself; I call this early release "Eall".
- Recommended combination: T + midpoint target with a median-of-3 predictor + Eall + the proportional repay rule (M2).
- 'ai' load IMMEDIATE, gap p99: 26.3/30.9/32.7/37.3 ms; frames > 34 ms: 0/0.04/0.26/1.02 per second.
- Per-tick content-vs-display error (p2p): 9.4/17.2/21.4/30.5 → 2.4/7.5/11.6/20.7 ms.
- Speed error: -3.20 % → -0.01 %.
- FIFO 120 Hz, 'ai': frames > 34 ms 0.53/0.95/2.72/4.81 → 0/0.01/0.06/0.56 per second; p99 50/58.3 → 33.3/33.3 ms at P3/P4.

**Checkpoints (C).** With checkpoints every 2 ms or less (4 ms is nearly as good) and a 'wait' end rule, C matches T within about ±1 ms. The 'wait' rule presents B inside the pacer wait at its target, not at the end of the step. Three caveats:
- The literal "forced Present at the end of the step" rule hurts light ticks, because B is shown too early.
- With no checkpoints at all, the result is worse than old.
- Under FIFO 60 Hz each Present inside the logic step blocks the main thread for 1.3-3.9 ms.

**Recommendation.** Variant T. Target = max(normal spacing time, midpoint). Predictor = median of the last 3 values at the same (logic frame % 10, sub), indexed from game state rather than a call counter. Combine with Eall and M2.

## DETAILS

FILES (all under <analysis workspace>/hitch\mt\simulation\)

**Code**
- mtsim.py: the simulator.
  - MT config; MTPacer = tsim.Pacer plus the optional M2 repay; run() and metrics(); seq_r() for per-tick sequences; selftest().
  - Deferred-B handoff and target computation: mtsim.py:266-296. The midpoint is computed at :288.
  - Forced Present at C0: flush(), mtsim.py:177-195.
  - C checkpoints: mtsim.py:313-333.
  - Early release (Eall / pred): mtsim.py:347-360.
- matrix.py: 32 variants × 6 loads × P0-P4 × {IMMEDIATE, FIFO 120 Hz 3 images, FIFO 60 Hz} × 3 seeds × 60 s. Output: out_matrix.json.
- summary.py → out_summary.txt (everything) and out_final.txt (the key variants).
- aux.py → out_aux.txt: skips, forced Presents, C0 waits, C logic extension, early releases, late releases, debt.

**Test outputs**
- out_seqs.txt: per-tick sequences.
- out_spike.txt: a spike replayed by the predictor.
- out_shift.txt: predictor slot shift.
- out_ameas.txt: Present blocking counted as render time.
- out_bias.txt: bias on the target time.
- out_cgran.txt: checkpoint spacing g.

**Model (INFERRED; it inherits the tsim V3 calibration and its ±3 ms uncertainty on L)**
- Logic speed is unaffected by the helper thread; spare CPU cores are available and a Present costs 0.15 ms of CPU.
- The helper presents exactly at its target time.
- At C0 the main thread waits until the helper's Present has returned (device lock / back buffer).
- FIFO is modelled as a 3-deep queue that blocks when full.
- Not modelled:
  - pauses;
  - device calls from inside the logic step;
  - the 'cal' load uses the main RNG, so variants see different draws. The other loads are paired.

**Variants**
- old: the committed pacer, src/pacer.cpp (spacing wait at :339-349, repay at :247-255).
- T0: B handed off; presented at the old spacing target (lastPresent + half − 0.25 ms, capped at release + 0.6·half, only while owed == 0); logic starts at once.
- Tm-{last|min2|med3|mean2|oracle}: target = max(T0 target, (A_k + predicted A_{k+1})/2).
  - Predicted A_{k+1} = max(h + Lpred + tail_est, next X deadline − expected pay) + a_est[k_next].
- +E: release a Y early when its overrun is predicted. +Eall: release every Y right after its X. Both use the existing borrow mechanism, so the absolute 33 ms pair schedule is kept.
  - With E, the 0.6·half release cap on the B target is dropped. Keeping it shows B about 10 ms after A: T0+Eall had 18 frames > 24.75 ms per second under FIFO.
- +M2: pay = min(owed, max(T/8, owed/6), slack).
- C{g}: checkpoints every g ms with random phase; the Present call, including FIFO blocking, lengthens the logic step.
  - 'end': forced Present at the end of the step.
  - 'w': present inside the pacer wait at the target, at the latest at C0.

**Results** (3 seeds × 60 s; P1..P4 unless noted; ms)

IMMEDIATE, 'ai' load (+8 ms every 5th tick, +15 ms more every 10th):

| Variant | gap p99 | frames > 34 ms /s | p2p error | Speed |
|---|---|---|---|---|
| old | 33.5 / 41.1 / 47.1 / 56.4 | 0.56 / 1.49 / 4.55 / 4.88 | 9.4 / 17.2 / 21.4 / 30.5 | -3.20 % |
| T0 | 30.7 / 39.5 / 47.0 / 56.4 | 0.52 / 1.28 / 4.25 / 4.88 | | |
| Tm-med3 | 30.1 / 33.8 / 36.0 / 39.2 | 0 / 0.58 / 0.94 / 1.25 | 6.0 / 13.8 / 17.4 / 24.7 | -3.14 % |
| Tm-orc | same as Tm-med3 (the AI pattern is fully predictable) | | | |
| Tm-med3+Eall+M2 | 26.3 / 30.9 / 32.7 / 37.3 | 0 / 0.04 / 0.26 / 1.02 | 2.4 / 7.5 / 11.6 / 20.7 | -0.01 % |
| C1w+Eall+M2 | 26.4 / 31.0 / 32.9 / 37.4 | | | |
| C4 'end' | 30.4 / 34.3 / 36.2 / 39.4 | | P0 4.0 vs 2.6 | |

IMMEDIATE, 'cal' load (heavy sub-step i.i.d., cv 0.3):

| Variant | gap p99 | Other |
|---|---|---|
| old | 30.9 / 40.3 / 47.7 / 59.2 | speed -1.83 % |
| Tm-last | 26.3 / 32.1 / 36.7 / 43.8 | |
| Tm-min2 | 26.9 / 33.2 / 37.6 / 45.0 | |
| Tm-med3 | 26.1 / 31.8 / 35.4 / 42.9 | |
| Tm-orc | 23.3 / 28.5 / 31.9 / 37.6 | |
| Tm-med3+Eall+M2 | 20.2 / 28.7 / 32.4 / 41.2 | > 34 ms: 0.02 / 0.19 / 0.48 / 1.27; p2p 2.2 / 6.8 / 10.2 / 19.7; speed -0.11 % |
| Tm-orc+E | 19.9 / 25.8 / 29.6 / 35.0 | |

Other loads:
- 'rand' (AI pattern plus 30 % lognormal variation), gap p99: old 37.7 / 47.5 / 55.6 / 66.6 → Tm-med3+Eall+M2 27.1 / 33.9 / 38.2 / 45.2. Speed -3.84 → -0.33 %.
- 'move' (the heavy work lands on a random sub-step), gap p99:
  - old 25.8 / 33.5 / 39.1 / 48.6.
  - Tm-med3 22.8 / 31.1 / 36.4 / 44.9: the predictor fails here.
  - Tm-med3+Eall+M2 18.3 / 24.6 / 29.0 / 39.2. Eall needs no predictor, and frames > 34 ms at P3 drop 4.52 → 0.00 per second.

FIFO 120 Hz, 'ai':
- gap p99: old 33.3 / 41.7 / 50.0 / 58.3 → Tm-med3+Eall+M2 25.0 / 33.3 / 33.3 / 33.3.
- Frames > 34 ms per second: 0.53 / 0.95 / 2.72 / 4.81 → 0 / 0.01 / 0.06 / 0.56.
- Frames > 24.75 ms per second: 6.10 / 6.74 / 6.75 / 9.38 → 2.23 / 4.99 / 6.87 / 9.80. At P4 the count rises because one ~56 ms hold becomes two ~28 ms holds, so > 34 ms is the meaningful metric there.

FIFO 60 Hz:
- The 3-deep queue absorbs the holds for all variants (p99 16.7 ms); the old pacer skips 0.6-1.0 B per second.
- At P4, frames > 24.75 ms per second: old 1.21 → 0.10 with E+M2.
- T: the main thread waits for the helper at C0 for 1-4 ms per second.
- C: Presents inside the logic step block for 1.3-3.9 ms each.

Max gaps are dominated by unpredictable outliers and stock spikes (cal+sp: old 403 ms, T 231 ms). The variants cannot reduce them.

**Theoretical floor.** The larger of A_k→B and B→A_{k+1} is at least half of A_k→A_{k+1}. With Eall, A_k→A_{k+1} ≈ max(33, a + 1.4 + b + L + 0.6) ms. At L = 59 (P4 plus the 10th-tick AI work) that is about 74 ms, so the hold cannot drop below about 37 ms.

**Checkpoint spacing** (out_cgran.txt; +Eall+M2, med3, 'cal' IMMEDIATE, gap p99):
- T: 28.7 / 32.4 / 41.2
- g = 0.5 / 1 / 2: 27.9-27.4 / 32.0-31.5 / 40.0-39.3
- g = 4: 27.2 / 30.8 / 38.8, with p2p +0.5-1 ms at P0/P1.
- g = 8: P0/P1 degrade (p2p 4.3 / 8.9).
- No checkpoints: 39.1 / 45.0 / 56.2, worse than old.

**Literal 'end' forcing.** It shows B early on light ticks. At P0 under FIFO 120, frames > 24.75 ms per second go from 1.64 (old) to 1.89 (C1) and 3.48 (C4); 'wait' fixes it.

**Predictor tests**
- **Spike replay** (150 ms logic spike on sub 3, out_spike.txt): 'last' replays it 10 ticks later as B forced at C0, one 25.0 / 7.5 ms pair (IMMEDIATE P1). This is bounded by the C0 force and appears once. min2 and med3 show no replay. Capping the predicted L at 50 ms does not help.
- **Slot shift** (one logic call not counted, out_shift.txt): the deferred variants fall back to old-like holds (max 56-60 ms at P2/P3) for about 2 s with 'last' and about 4 s with min2/med3, never worse than old. The oracle is unaffected. Indexing by game state removes the problem.
- **Present blocking counted as render time** (out_ameas.txt): no measurable effect, since the C0 wait is at most 4.5 ms per second. Still measure the A-render from C0 end, after any wait for the helper.
- **Target bias** (out_bias.txt): +2 ms gains 1-3 ms of IMMEDIATE p99 but raises FIFO 120 frames > 24.75 ms from 1.5 to 5.5 per second at P1. Keep 0.

**Example ticks** (out_seqs.txt; heavy sub-step 5 runs after B4)

IMMEDIATE P3, +8 ms AI tick (L ≈ 37):
- old: …B4:16.3 A5:42.6
- T0: …B4:16.2 A5:39.5
- Tm-med3: …B4:28.2 A5:26.5
- Tm-med3+Eall+M2: …B4:25.6 A5:23.1
- C1w+Eall+M2: …B4:26.2 A5:22.6

IMMEDIATE P3, +23 ms AI tick:
- old: A5:57.3
- Tm-med3: 35.5 / 35.9
- Tm-med3+Eall+M2: 32.0 / 33.5

IMMEDIATE P1, +8 ms AI tick:
- old: A5:30.6
- Tm-med3: 22.6 / 20.9
- Tm-med3+Eall+M2: 18.6 / 17.8

FIFO 120 P3, +23 ms AI tick:
- old: A5:58.3
- Tm-med3+Eall+M2: B4:33.3 A5:33.3

**Metric caveat.** Skipped B frames (FIFO 60 late skips) do not appear in the gap or p2p metrics: the content jumps 33 ms in one vblank without raising a gap. T without E barely improves p2p (17.2 → 13.8 at P2), because A_{k+1} itself stays late; only E moves it.

## RISKS

**Model limits (INFERRED)**
- L carries ±3 ms.
- The helper is assumed to present exactly on time and not to slow the logic thread.
- Device calls made inside the logic step are not modelled. If the logic step creates or locks D3D resources (e.g. model or texture loads when objects spawn), they serialize against the helper's Present under D3DCREATE_MULTITHREADED. This needs checking in the code areas.
- Pauses are not modelled, and the DXVK FIFO model is a plain 3-deep blocking queue.

**T needs a thread-safe device**
- It needs D3DCREATE_MULTITHREADED on both CreateDevice paths (0x524190 → call 0x5241B6, and the retry at 0x5241EF → 0x524222). This is from the earlier synthesis (M6), not re-verified here.
- A pending Present must be drained or cancelled before Reset (0x522000), on leaving 60 mode, on Alt-Tab and on device loss.

**Eall assumptions**
- It assumes the B-render is a pure repeat of render k that may run before its 16.5 ms slot (A-only camera) and that release time does not affect logic. Logic is frame-stepped, so this is INFERRED safe, but should be confirmed with the replay determinism check.
- The B target must not use the old release + 0.6·half cap when early release is on.

**Predictor**
- A call-count index (g_logicCalls % 60) misaligns after a pause or a failed tick attempt and costs 2-4 s of old-like holds. Index by game state instead.
- 'last' replays 50-200 ms spikes once, as a 25 / 7 ms pair.
- Mispredictions are always bounded by the forced Present at C0: B is never held past the next A-render.

**Checkpoints (C)**
- They need hooks no more than about 2 ms (at most 4 ms) apart inside every heavy loop, including the pathfinder and AIPlayer::update. Without them C is worse than old.
- The end-of-step forcing as specified regresses light ticks.
- Under FIFO 60 the main thread blocks inside logic for 1.3-3.9 ms per Present.

**Metric changes the user may notice**
- At P4, frames > 24.75 ms per second rise, because one ~56 ms hold becomes two ~28 ms holds.
- FIFO 120 short frames (8.3 ms) stay around 7-10 per second.

## RECOMMENDATION

**Implement variant T, the presenter thread**
1. At the B-render's PRESENT_STUB (0x522644), hand the Present (device vt+0x44, same arguments) to one helper thread and continue straight into the logic step, without the spacing wait at pacer.cpp:339-349.
2. Helper target = max(lastA + half − 0.25 ms, (A_k + Â_{k+1})/2).
   - Â_{k+1} = max(h + L̂ + tail_est, next X deadline − expected pay) + a_est[k_next].
   - a_est is a per-k EWMA (1/8) of C0-end → PresentSkip entry, measured after any wait for the helper.
   - tail_est is an EWMA of the Y tail.
   - No target bias.
3. At the next C0, before the A-render, if the B Present is still pending, wait for the helper's Present to return; if the target has not been reached yet, force the Present first.
4. Keep the FIFO late-skip rule for B.
5. Cap the A spacing at lastA + 33 ms − 0.5 ms.

**Predictor:** L̂ = median of the last 3 values at slot (GameLogic frame % 10, sub). Read the frame from GL+0x40 and the sub from GE+0x34, not from a call counter. Use min2 as an equivalent fallback; avoid 'last' because it replays spikes.

**Combine with:**
- **Eall:** release every Y immediately after its X through the existing borrow (absolute pair schedule kept), and drop the release-based B cap.
- **M2 repay:** pay = min(owed, max(T/8, owed/6), slack).

Together these are the main gain. Without them, T alone only splits the hold: about −7 to −17 ms of gap p99 at P2-P4, with the speed loss unchanged.

**Rollout:** default off, behind a config switch, with telemetry for helper waits at C0, forced Presents per second, and per-tick max gap. Validate against old in one session: 60 FPS, Telemetry=1, same map, P3/P4 stage, IMMEDIATE and FIFO 120.

**Fallback (no thread):** variant C with checkpoint hooks no more than 2 ms apart (4 ms acceptable) and the 'wait' end rule: present inside the pacer wait at the target, at the latest at C0, never forced at the end of the step. Only worth it if the checkpoint spacing can be guaranteed in the heavy loops.

## VERIFIER OVERALL
The simulation work itself holds up. I reran it and got the headline numbers exactly; the code and address references check out, except one small table mismatch. Where it overreaches is the conclusions it draws from those numbers.

**What reproduced.**
- The selftest passes (30 cases).
- An independent 3-seed × 60 s rerun of old, old+M2, Tm-med3, Tm-med3+Eall(+M2) and Tm-orc+Eall+M2 matches out_matrix/out_final to 0.1 ms, for both 'ai' and 'cal' loads, IMMEDIATE and FIFO 120.
- My scripts are in `<analysis workspace>/hitch\mt\verify_sim\` (chk2.py, chk3.py with `out_chk3.txt`, chk4.py with `out_chk4.txt`). `mtsim2.py` there is a copy of mtsim.py with two added knobs: `contend` (ms the main thread loses when the helper's Present lands inside the logic step) and `hjit` (helper wake-up lateness).
- pacer.cpp references are correct (at the commit 'display line without the unused fullscreen refresh field'): spacing wait at :339-349, repay at :247-255.
- PRESENT_STUB at 0x522644 is `call [edx+0x44]` (Present) with four NULL arguments, device pointer [0xDD3474].
- CreateDevice is called at 0x5241B6 and 0x524222 (vt+0x40). Both take BehaviorFlags from the single global [0xDD345C] (written at 0x5240E6/0x5240F6/0x52410D), so one OR of 0x4 covers both paths.

**Refuted or overstated.**
1. The speed claim (-3.20 % → -0.01 %) is not a threading gain. M2 alone gives old+M2 at -0.08 %. The loss also exists only at P4 (speed 0.968; P1-P3 are about 1.000).
2. "Never worse than old" after a predictor slot shift is wrong for the > 24.75 ms metric. FIFO P3: Tm-med3+Eall+M2 has 8.33/s in 20-22 s against old's 6.83; Tm-med3+E+M2 has 8.67/s in 22-24 s against 7.17. IMMEDIATE P3: Tm-last+E+M2 has 5.67 against 5.00. Only the max gap is never worse.
3. "Max gap old 403 ms → T 231 ms" is RNG noise, not an effect. 'cal' draws from the main RNG, so the spikes differ per variant. Under FIFO 60 the same table has old 216.7 and old+M2 433.3.
4. Results are quoted for P1-P4 only, which hides a P0 regression for T without early release. Tm-med3 'ai' IMMEDIATE at P0: gap p99 22.7 → 25.4, frames > 24.75 ms 0.52 → 0.91 per second.
5. The table value for Tm-orc+E 'cal' is wrong: it is 19.9/25.4/29.7/35.6, not 19.9/25.8/29.6/35.0.
6. The FIFO 60 range "Present inside logic blocks 1.3-3.9 ms" covers only the C…w+Eall+M2 variants; across C variants it is 0.15-3.93 ms.
7. The ameas test proves little. Under FIFO 120 the C0 wait is about 0, so the 'clean' and 'release' rows are identical. Present is modelled as 0.15 ms and never blocks under IMMEDIATE.

**What the model leaves out, and it changes the recommendation.** DXVK's Present holds the device lock for the whole call (d3d9_swapchain.cpp:112). That lock is a RecursiveSpinlock and only becomes real with D3DCREATE_MULTITHREADED. While holding it, Present can block in `acquireNextImage` (it waits for the previous present and the swapchain fence, dxvk_presenter.cpp:83-104) and in `SyncFrameLatency` (:911). Frame latency is min(3, BackBufferCount+1). BackBufferCount is 2 in fullscreen and 1 windowed: 0x524B39-0x524B40 computes `sbb/add 2`.

So any device call the main thread makes while the helper is inside Present spins for that time. The model has none of this.

My contention sweep (the `contend` knob, applied only when the helper's Present lands inside the logic step):

| Contention per overlap | Effect on the gain |
|---|---|
| 0.5 ms | negligible |
| 2 ms | about 0.5-2 ms of p99 lost |
| 5 ms | about half the gain lost |
| 10 ms | mostly gone (FIFO 120 'ai' P1 p99 back to 33.3; 'cal' IMMEDIATE P1 back to 29.9 against old 30.9) |

Helper lateness up to 3 ms barely moves IMMEDIATE, but under FIFO 120 it raises frames > 24.75 ms at P1 from 2.23 to 5.56 per second. The helper therefore needs the spin-tail timer, not plain Sleep.

**Recommendation.** The model shows T and C perform the same: C at 1-4 ms checkpoint spacing with the 'wait' end rule is within about ±1 ms of T, and C at g=2-4 is even slightly better in 'cal'. C needs no MULTITHREADED flag and has no lock contention, so recommending T as primary is not supported by the evidence.

With early release, about 30 B frames per second are deferred, but only 1-6 per second land inside the logic step (`out_chk4.txt`). The other ~24 land in the pacer wait, where the main thread can present them itself. A hybrid is the safer design to evaluate:
- the main thread presents B from AotR60_Pacer's wait whenever the target falls there;
- only a target that falls inside the logic step goes to the helper (or to checkpoints).

All performance numbers remain INFERRED from the model. The pre-existing ±3 ms uncertainty on L and the 'ai' load being perfectly periodic (oracle equals med3) mean in-game predictability is unproven.

- [confirmed] Baseline 'old' reproduces hsim/tsim bit for bit (selftest, 30 cases)
  I ran mtsim.py and got 'selftest ok'. An independent check (P3, 'ai', seed 3) also gives identical shown lists. This only validates the old path; the T/C/E code paths have no reference test.

- [confirmed] 'ai' IMMEDIATE gap p99 old 33.5/41.1/47.1/56.4 → Tm-med3 30.1/33.8/36.0/39.2; frames > 34 ms 0.56/1.49/4.55/4.88 → 0/0.58/0.94/1.25
  My rerun (chk2.py, seeds 51-53, 60 s) matches exactly.

- [confirmed] Tm-med3+Eall+M2 'ai' IMMEDIATE p99 26.3/30.9/32.7/37.3, > 34 ms 0/0.04/0.26/1.02, p2p 2.4/7.5/11.6/20.7
  My rerun matches exactly. All of it is model output (INFERRED).

- [refuted] Speed error -3.20 % → -0.01 % comes from the recommended combination
  old+M2 alone gives -0.08 % (0.9992 at P4); Tm-med3+Eall without M2 gives 0.9888. The loss exists only at P4: old speed is 1.0000/0.9998/0.9995 at P1-P3. The speed fix is M2, not the thread.

- [confirmed] FIFO 120 'ai': frames > 34 ms 0.53/0.95/2.72/4.81 → 0/0.01/0.06/0.56; p99 at P3/P4 50/58.3 → 33.3/33.3
  My rerun matches. Pacer stats in out_aux are identical for IMMEDIATE and FIFO 120 because the modelled 3-deep queue never fills at 60 Presents/s on 120 Hz; that is consistent, not a bug.

- [refuted] 'cal' Tm-orc+E p99 19.9/25.8/29.6/35.0
  out_summary.txt shows 19.9/25.4/29.7/35.6. The mismatch is minor.

- [confirmed] T0 (B handed off at the normal time) gains only 0-3 ms
  'ai' IMMEDIATE: 33.5→30.7, 41.1→39.5, 47.1→47.0, 56.4→56.4.

- [confirmed] T alone (Tm-med3) gives about -7 to -17 ms of gap p99 at P2-P4
  'ai': -7.3, -11.1 and -17.2 ms. However, Tm-med3 regresses P0 (p99 22.7→25.4, > 24.75 ms 0.52→0.91 per second) and raises > 24.75 ms at P4 (4.89→9.28 per second); the summary leaves P0 out.

- [confirmed] C with checkpoints ≤ 2-4 ms and the 'wait' end rule matches T within about ±1 ms; no checkpoints is worse than old
  out_cgran.txt: C g=1-4 is within ±1.3 ms of T, and slightly better in 'cal' (P4 38.8 vs 41.2), because quantization acts like a small positive target bias. g=1000 is worse than old. This undercuts choosing T over C on performance.

- [refuted] Slot shift: falls back to old-like holds for 2-4 s, never worse than old
  The duration is right. 'Never worse' fails for > 24.75 ms: FIFO P3 Tm-med3+E+M2 has 8.67/s against old's 7.17 (22-24 s window) and Tm-med3+Eall+M2 has 8.33 against 6.83 (20-22 s); IMMEDIATE P3 Tm-last has 5.67 against 5.00. Only the max gap stays at or below old.

- [refuted] Max gap cal+sp: old 403 ms vs T 231 ms
  'cal' uses the main RNG, so spike draws differ per variant. FIFO 60 in the same matrix shows old 216.7 against old+M2 433.3. These are noise, not effects.

- [uncertain] Present blocking learned as render time has no measurable effect
  In the model the C0 wait is about 0 under FIFO 120 and Present never blocks under IMMEDIATE (0.15 ms fixed), so the test cannot show an effect. DXVK Present can block under its lock in acquireNextImage and SyncFrameLatency, which is not modelled.

- [confirmed] T needs D3DCREATE_MULTITHREADED at 0x5241B6 and 0x524222
  Both are `call [edx+0x40]` (IDirect3D9::CreateDevice). Both push BehaviorFlags from [0xDD345C], written only in FUN_00524070 at 0x5240E6/0x5240F6/0x52410D. DXVK makes D3D9DeviceLock a no-op unless the flag is set (d3d9_device.cpp:55, d3d9_multithread.h). The lock is a RecursiveSpinlock, so a blocked helper makes main-thread device calls spin.

- [uncertain] The helper's Present does not slow the logic thread (0.15 ms CPU, presented exactly on time)
  This holds only if the main thread makes no device calls during the helper's Present. DXVK holds the device lock across EndFrame, Flush, acquireNextImage (waits for the previous present and the fence) and SyncFrameLatency (frame latency 3 in fullscreen: BackBufferCount=2 per 0x524B39-0x524B40). Sensitivity run (chk3): 2 ms of contention per overlap costs 0.5-2 ms of p99, 5 ms about half the gain, 10 ms nearly all of it. Helper lateness of 0-3 ms raises FIFO 120 P1 > 24.75 ms from 2.23 to 5.56 per second.

- [confirmed] Mispredictions are bounded by the forced Present at C0
  In the model, flush() forces tp=c0. In real code this also requires draining in every main-thread path that touches the swapchain before C0. That includes Reset 0x522000, which has six callers (0x516C40, 0x517B30, 0x522460, 0x5224D0, 0x5225E0, 0x5249A0), some reachable from window-message handling between the B handoff and C0.

- [uncertain] Recommendation: variant T as the primary implementation
  The model shows C equal to T. With early release about 30 B/s are deferred, but only 1-6 B/s land inside the logic step (out_chk4.txt); the rest land in the pacer wait, where the main thread can present them itself. T adds a global MULTITHREADED lock and unmodelled contention; C adds a checkpoint-coverage burden. Neither is proven better; a hybrid (main thread presents in the pacer wait, helper or checkpoint only for in-logic targets) carries the least risk.

## VERIFIER MISSED
Missing before a safe implementation:

1. **The Present result is consumed synchronously.** At 0x52265A..0x5226A7 the stock code uses the HRESULT: on DEVICELOST it calls TestCooperativeLevel (vt+0xC), then Reset 0x522000 or Sleep(200) via 0x5B8830, and 0x51EDF0 on other errors. A deferred Present must return S_OK the way the existing skip path does, and carry DEVICELOST back so that g_gapDevLost is set and the next A Present runs the stock recovery. CONFIRMED bytes; the recovery path is INFERRED.
2. **The main thread touches the device right after the handoff.** End_Scene continues at 0x5226AF..0x5227C5 with W3D Release_Ref calls (call [edx] destructors on [0xDD4298], [0xDD42A0], [0xDD4020+i*4], [0xDD401C]) concurrently with the helper's Present. Whether DXVK resource destruction takes the device lock is unverified. The logic step and the overhead segment (asset streamer, serviceWindowsOS) can also make device calls (sync area: 151 device functions).
3. **Every swapchain or device teardown path needs a drain.** All six Reset callers, Set_Render_Device / CreateDevice 0x5249A0, mode toggles and shutdown must drain the pending Present first. The helper must capture the device pointer and must not outlive a device release.
4. **The DXVK source has not been matched to the installed DLL.** The dxvk/ copy is not confirmed to be the same version as rotwk\d3d9.dll, so lock and blocking behaviour must be checked against the shipped build.
5. **Pacer state is written from two threads.** AotR60_PresentSkip and AotR60_PresentDone return early when !OnMainThread(). g_lastPresent, g_presentsSinceBlock and g_presentCall would now be written from the helper and need atomics or a hand-back. g_presentsSinceBlock drives the FIFO late-skip rule.
6. **Early release is an unproven assumption.** It assumes the B-render (render, Present and the allow-listed post-Present work) is time-invariant. Real-time client effects such as water, particles on WW3D sync time and the cursor would be stamped about 8 ms early. The claim that the logic is bit-identical with early release needs the replay or determinism check.
7. **FIFO handling is crude.** The midpoint target is not aligned to vblank. Helper jitter matters under FIFO 120, and the 3-deep queue is a crude model of DXVK's acquire plus frame-latency waits.
8. **There is no measured basis for the predictor or for L.** The 'ai' load is perfectly periodic, so oracle equals med3. In-game per-(frame % 10, sub) predictability, which the GE+0x34 / GL+0x40 index assumes, has not been measured; the D3 per-sub telemetry would supply it. The ±3 ms uncertainty on L is inherited.
9. **No variant isolates M2 or early release from the threading.** There is no "old + early release + M2 without deferral" variant. The speed fix (M2) is independent of threading and could ship on its own.
10. **D3DCREATE_MULTITHREADED has an uncosted overhead.** Every device call takes the spinlock (thousands per frame) and DXVK may change other behaviour with the flag. This should be measured with an A/B of render time before committing to T.