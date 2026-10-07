# AREA engine

## SUMMARY

The hitching comes from one heavy logic sub-step per tick. Once the battle grows, that step pushes the next frame out by 10-25 ms five times a second (once every 198 ms logic tick). The game sees the same delay at stock 30 FPS, but it is hidden inside the 33 ms frame there. Nothing in the 60 FPS design makes the logic heavier.

GameLogic::update (0x62E4E8) does different work on each of the six sub-steps:
- **Sub 1:** scripts, Lua and some per-object loops.
- **Sub 2:** partition manager and collision manager.
- **Subs 3 and 4:** each runs half of the AI update modules (bucket 0, all 17 AIUpdate classes).
- **Sub 5:** everything else. That is buckets 1 and 2 (163 module classes: physics, weapons, production, stealth, slow death, horde containers), plus TheAI::update 0x6FEC63, which runs a second pathfinder pass and ThePlayerList::update with every AI player's think, plus 12 more managers.
- **Sub 6:** bucket 3 is empty for every registered module class, so sub 6 is light.
- **Every sub:** the pathfinder queue (budget MaxPathfindCellsPerFrame = 4000).

In 60 mode each logic sub-step runs in a Y iteration, after the B-render has been presented and before the pacer releases the next iteration (0x6325D5 halt → 0x6326C6/0x6326F0 → 0x632A82, then the limiter site 0x63A196). Render k is followed by sub k+1. So sub 5 runs right after B-render 4 and delays A-render 5 one-for-one (CONFIRMED).

A Y iteration has to fit the B-render (4.4-6 ms in the CSV), Present and the logic sub into 16.5 ms. Stock has to fit render plus logic into 33 ms. A sub stays invisible in 60 mode only below about 11 ms; in stock the limit is about 24-27 ms.

The data matches this:
- Draw count went from 157 to 433 to 824 calls per tick, measured by INT_RECOIL.
- From t≈283 s there are 1.1-1.34 late releases per logic tick, a single heavy step per tick.
- Iterations on time stay at about 90 %.
- The implied heavy Y iteration grows from about 24 ms to about 39 ms, so the heavy sub grows from about 18-20 ms to about 28-32 ms.

A model of the current pacer.cpp with one heavy sub of 20 or 30 ms reproduces the CSV: 1.0 late releases per tick, mean lateness 10.3 or 21.4 ms, about 91.5 % on time.

What that looks like on screen:
- **60 mode:** one frame of about 26-44 ms every 198 ms among 16.5 ms frames, 1.6 to 2.7 times a normal frame. At the end, time owed is almost never zero, so the even Present spacing wait is off and every frame is spaced unevenly (present_wait_ms drops from 300-650 to 0-3 ms per window).
- **Stock at t≈283-325 s:** perfectly even 30 FPS, since render plus heavy sub is about 25-29 ms, under 33 ms.
- **Stock at t≈410-432 s:** one 35-44 ms frame per tick, 1.1 to 1.3 times a normal frame, which is barely visible. Stock also loses about 2-3 % game time, because the stock limiter never catches up.

The fallback policy never triggers, because only about 10 % of iterations are late and its threshold is more than 50 %.

The separate 55-210 ms spikes appear in every window, including before t=260 s. They are wall-clock stalls that stock has too (the stock baseline shows 100-260 ms hitches). Engine candidates are below, but at 10.6 s resolution their period cannot be measured. The snapshot came from an older build that does not log the per-stall sub and frame lines; the current telemetry.cpp does.

Scratch files: <analysis workspace>/hitch\engine\ (phases.py and phases.txt map module classes to their update phase, subsim.py is the pacer and stock-limiter model, vt.py resolves vtables). Also hitch\ge_init.asm, hitch\gl_update.asm, hitch\subsys_map.txt (subsystem globals).

## FINDING [confirmed] What runs on each of the six logic sub-steps
GameLogic::update(sub) 0x62E4E8 does different work for each sub (disassembly in hitch\gl_update.asm). Global names come from the init-subsystem calls in GameEngine::init 0x63AD4F and GameLogic::init 0x62CE75 (hitch\subsys_map.txt).

**Every sub:**
- Pathfinder queue 0x6F2364, ECX = [TheAI+0x10], called at 0x62E69F.
- 0x629DA6 and 0x5FF9D3.

**Sub 1:**
- Logic frame++ at 0x62E577.
- TheScriptEngine vt28 at 0x62E6B7, TheLuaScriptEngine at 0x62E6C2, TerrainLogic at 0x62E6D0, TheVictorySystem at 0x62E6DF.
- TheRecorder at 0x62E8B0, TheFireLogicSystem at 0x62E8BB, TheGlobalWeatherSystem at 0x62E8C6.
- 0x820EF0 (list GL+0x174; each entry has its own period) and 0x81BE85 (list GL+0x178, the ReinvisibityDelay stealth list; runs every LTR = 5 frames, i.e. 1 s).
- Command list 0x779A3D.
- Per-object loops at 0x62E910 and 0x62EC5D..0x62EC9D, calling 0x690A42, 0x690AB9, 0x690AE5 and 0x697EB6 for every object.
- TheLivingWorldLogic vt28 at 0x632A92.

**Sub 2:**
- ThePartitionManager vt28 at 0x62E943 → 0xA3B4E0: re-cells every object that moved.
- TheCollisionManager vt28 at 0x62E94E → 0xB6E6B0: sweep-and-prune broadphase, with a full-rebuild branch 0xB6CC30, 0xB6CD60, 0xB6C930.
- Per-object 0x6260E1 at 0x62E96F.

**Subs 3 and 4:** bucket 0 (GL+0xC8 vector).
- Sub 3 stops at size/2 (0x62EA09).
- Sub 4 starts at size/2 (0x62E9D4) and compacts the list.
- The bucket is chosen by the module's getUpdatePhase, vt+0x30 (0x62BA8B, 0x62A070). Bucket 0 holds all 17 AIUpdate classes: AIUpdateInterface, HordeAIUpdate, WorkerAIUpdate and others; vt+0x30 = 0x851E97 returns 0.

**Sub 5:**
- Buckets 1 and 2:
  - Bucket 1: the 3 HordeContain classes; 0x490AC4 returns 1.
  - Bucket 2: 163 classes including PhysicsBehavior, AutoHeal, FireWeaponUpdate, ProductionUpdate, StealthUpdate, SlowDeath, MissileUpdate and the Horde*Contain classes; 0x64DFDA returns 2.
- TheAI vt28 at 0x62EB71 → 0x6FEC63: a second pathfinder pass at 0x6FEC66, then a jump to ThePlayerList vt28 0x6A84DB. That runs Player::update 0x6AF269 for each of the 20 player slots, and through player+0x2FC vt14 reaches AIPlayer::update 0x8F9705.
- TheShroudManager, TheTaintManager, TheBuildAssistant, TheLargeGroupAudio.
- processDestroyList 0x62A2C9.
- TheWeaponStore, TheLocomotorStore, TheVictoryConditions, TheDelayedExperienceLevelGrantSystem, 0x80F4D3.
- TheSkirmishAIManager 0x6A96A0, TheMineshaftPortalNetworkManager, TheTeamFactory (0x62EB85..0x62EC0D).

**Sub 6:** bucket 3 only. No registered module class returns phase 3, so sub 6 does only the per-sub common work.
EVIDENCE: - Decompile and disassembly of 0x62E4E8.
- hitch\engine\phases.txt: 183 module classes resolved through the module factory 0x6579C9 → create function → constructor vtable → slot 0x30. Phase 0: 17 classes, phase 1: 3, phase 2: 163, phase 3: 0.
- Other classes were not resolved: either slot 0x30 is not getUpdatePhase (upgrades, bodies, die modules, special powers) or their vtable is shorter.
IMPLICATION: - The object, AI and physics work is not spread evenly. Sub 5 carries physics and all other non-AI update modules, a second pathfinder pass and all AI player thinking. Subs 3 and 4 each carry half of the per-unit AI. Sub 6 is nearly empty.
- The engine itself concentrates most per-unit cost into one or two calls per tick.
- No 60-mode patch changes these calls.

## FINDING [confirmed] In 60 mode the logic step runs between the B Present and the next A-render, so its cost delays the A-render directly
Order inside GameEngine::update 0x6325A0:
- Asset manager vt28 at 0x6325B0.
- Script-debug bridge 0x6325B9 and 0x6325C4.
- C0 → clientUpdate at 0x6325CF, which renders and calls Present at 0x522644.
- Halt check at 0x6325D5:
  - X iteration: 0x6325DE (GC+0xC8 = 0, Debug vt94, return).
  - Y iteration: 0x6325F8, s++ at 0x632625, step at 0x6326C6 or 0x6326F0, then GameLogic vt34 at 0x632A82.

After that, control returns to the GameEngine::execute loop, whose limiter sits at 0x63A196 (now the QPC pacer). So each Y iteration is: release → B-render k → B Present → logic sub k+1 (sub 1 when k = 6) → pacer.

In a tick, sub 5 runs after B-render 4 and decides when A-render 5 starts; its Present lands about L5 + a_rel after the B Present. Sub 1 runs after B-render 6 and delays A-render 1. A-render 1 is the isTick render and is itself the heaviest A-render: per drawable it calls 0x68D8F7 and shifts the history at 0x648777..0x6487CB, and it calls GhostObjectManager [0xDE4BD0] vt18 at 0x64875A.
EVIDENCE: - Disassembly of 0x6325A0..0x632708, 0x6329B0..0x632B10 and 0x63A180..0x63A21F.
- PLAN 1.3: halt and isTick contracts.
IMPLICATION: - One long logic sub-step leaves one long gap: the B frame stays on screen for about L + a_rel.
- The pacer can only carry the lateness as time owed; it cannot hide it.
- With L about 30 ms, no pacing policy can make that Present gap shorter than about 30 + a_rel ms. Only stock's 33 ms frame hides it.

## FINDING [confirmed] Budget: a Y iteration must fit B-render + Present + logic in 16.5 ms; stock fits render + logic in 33 ms
CSV values:
- b_rel_present rises from 1.4 to 6-7 ms; a_rel_present from 2.3 to 9.3 ms.
- Draw count, using INT_RECOIL per tick from the cumulative aotr60.log lines: 157 in t≈63-114 s, about 433 in t≈114-235 s, about 824 in t≈235-356 s. So both A- and B-render costs rise with the number of units.

The heavy sub is invisible in 60 mode only while L < 16.5 − b_rel (minus the Present call), about 11-12 ms. Stock allows L < 33 − render, about 24-27 ms.

From the CSV, the implied duration of the late Y iteration is 16.5 + mean lateness:
- t=283-315: mean lateness 5.7-8.1 ms → about 22-24.6 ms → L ≈ 18-20 ms.
- t=357-410: 16-19 ms → about 33-36 ms → L ≈ 25-30 ms.
- t=421-432: 22.3-22.6 ms → about 39 ms, partly inflated by spikes, since debt is 350 ms per window.

Late releases per logic tick: 0.04-0.48 until t=251 (except 1.48 in the t=145 window). Then 0.61 at t=262-272, and 1.02-1.34 from t=283 to the end, with on-time iterations at 89-91 %. That is one budget-breaking sub per tick, not two. If the AIUpdate halves (subs 3 and 4) were the heavy part, there would be about 2 per tick.
EVIDENCE: - hitch\aotr60_rates.csv columns late_releases, late_release_ms, a/b_rel_present_ms and on_time_pct (computed per window in this session).
- aotr60.log INT_RECOIL averages: 157.14 over 245 ticks, 352.80 over 844, 550.00 over 1452. Interval rates (352.8·844 − 157.14·245)/599 ≈ 433 and (550·1452 − 352.8·844)/608 ≈ 824.
- pacer.cpp:211-243 (lateness carried and the schedule re-anchored).
IMPLICATION: - "Starts after a couple of minutes" is the moment the heaviest sub-step crosses about 11 ms as the armies grow (around t≈262-283 s, about 3.3-3.7 min after 60 mode switched on at t≈63).
- Earlier windowed sessions in small skirmishes stayed under it.
- The heavy sub is INFERRED to be sub 5 from its content plus about 1 late release per tick. The current build's stall lines can confirm it.

## FINDING [likely] The model reproduces the CSV pattern with one heavy sub-step
hitch\engine\subsim.py follows the current pacer.cpp (AotR60_Pacer, AotR60_PresentSkip, IMMEDIATE present) and the stock limiter. Each run below is 10.6 s.

**60 mode, with A = 9, B = 5 and the other subs at 2-5 ms:**

| Heavy sub L5 | Late per tick | Mean lateness | On time | Spacing p95 / max |
|---|---|---|---|---|
| 20 ms | 1.02 | 10.3 ms | 91.5 % | 26 / 30 ms |
| 30 ms | 1.00 | 21.4 ms | 91.6 % | 38.5 / 44 ms |
| 30 ms, plus a 60 ms spike every 2 s | 1.02 | 25.5 ms | 91.5 % | max 102 ms |

The spike run also gives debt of 145 ms per window and logic at 4.90 Hz. CSV for comparison: 1.1-1.34 late per tick, 17.9-22.6 ms mean lateness, 89-91 % on time, debt 0-350 ms, logic 4.59-5.0 Hz.

**Stock, same costs:**
- L5 = 20: every frame is 33.0 ms.
- L5 = 30: one frame of about 39-44 ms per tick, logic 4.89 Hz (stock loses time and never catches up).
- With the spikes: max frame 103 ms, logic 4.72 Hz.
EVIDENCE: Runs of `python subsim.py 9.0 5.0 5 4 5 5 30 2` and variants (output in this session), with SPIKE_EVERY and SPIKE_MS set through environment variables.
IMPLICATION: A single heavy sub-step plus occasional stock-type stalls explains every CSV column. No 60-mode-specific engine work is needed to explain the data.

## FINDING [likely] Stock 30 FPS with the same work: smooth until about t≈330 s, then mildly longer frames once per tick
- **Limiter semantics (CONFIRMED):** the stock limiter 0x63A1B6..0x63A1F8 busy-waits until 33 ms after [0xDE4318] and then sets [0xDE4318] to now. There is no catch-up: a long iteration simply lasts longer.
- **t≈283-325 s:** render (about 6-8 ms) + L5 (about 18-20 ms) ≈ 25-29 ms, under 33 ms. Stock is perfectly even. 60 mode shows a 26-30 ms frame among 16.5 ms frames every 198 ms (1.6-1.8 times a normal frame) and pays the time back afterwards in roughly 2 ms steps.
- **t≈410-432 s:** stock is about 35-41 ms, so one frame per tick is 1.1-1.25 times a 33 ms frame (barely visible), and stock loses about 2-4 % game time. 60 mode shows about 36-44 ms where 16.5 ms is expected (2.2-2.7 times), a clearly visible 5 Hz judder, while holding speed through repayment until time owed overflows its cap. In 60 mode, logic in the CSV stays at 4.93-5.03 Hz except the last two windows (4.93, 4.59).
- **55-210 ms spikes:** these are the same wall-clock stalls in both modes. The stock baseline already shows 100-260 ms hitches costing 1-2.5 % of game time.
EVIDENCE: - 0x63A196..0x63A21F disassembly.
- subsim.py stock() results.
- CSV a_rel, b_rel, logic_ticks_s.
IMPLICATION: - The regular hitch is how 60 mode looks with work that stock hides inside its 33 ms frame. It is not extra work.
- Stock would visibly hitch only in the very last minutes, and much more mildly.

## FINDING [confirmed] Even Present spacing switches off once time owed builds up, so all frames become uneven late in the session
- AotR60_PresentSkip applies its spacing wait only when time owed is zero (pacer.cpp:339).
- With about 1.2 late releases per tick and repayment capped at T/8 ≈ 2.06 ms per on-time iteration (pacer.cpp:247-255), time owed seldom returns to zero.
- CSV present_wait_ms falls from 164-647 ms per window (t≤315) to 195, 106, 27.7, 2.7 and 0.0 at t=357, 368, 389, 421 and 432.
- With repayment at its limit (about 22 ms per tick), owed overflows its 6T cap and the overflow is counted as debt: 168-350 ms per window at the end.
EVIDENCE: - pacer.cpp:226-258 and :339.
- CSV present_wait_ms and debt_ms.
IMPLICATION: - Besides the one long frame per tick, the other 11 frames per tick are spaced unevenly (shortened by repayment, no spacing wait). That makes the judder more visible.
- Fallback never triggers: lateRatio is about 0.1 against the >0.5 threshold (pacer_policy.cpp:5), and lostRatio is at most about 0.033.

## FINDING [likely] Periodic and event-driven engine work beyond the 5 Hz tick
Addresses CONFIRMED; costs and alignment INFERRED.

**(a) Pathfinder queue 0x6F2364**
- Runs on every sub (6 per tick), plus a second call in sub 5 through TheAI::update at 0x6FEC66.
- Budget is GD+0x11E8 MaxPathfindCellsPerFrame = 4000 (aotr\data\ini\gamedata.ini:11817). It is multiplied by 100 while the logic frame is below 5·LTR, i.e. the first 25 ticks.
- Half the budget goes to queue +0x1D1E8, the rest to +0x1C9E0. Each path runs through object+0x260 vt+0x230 or vt+0x234.
- The budget is checked only between paths, so one search can run up to MaxCellsFindPathLimit 15000 or MaxCellsToExamineTowardsGoal 25000 cells (gamedata.ini:11817-11830). The cost grows with the number of units asking for paths.

**(b) Path-zone update 0x93A530**
- Called at the start of each pathfinder pass. It does work only when flags +0x1BA30 or +0x1BA31 are set, and then rebuilds each dirty 16×16 block plus a global pass.
- Triggered by events (structures placed or destroyed), not periodic.

**(c) AI players (sub 5) through AIPlayer::update 0x8F9705**
- AISkirmishPlayer vt+0x48 (0x8F3EFE): counter +0x28 drops by 1 per tick. When it reaches 0 it runs team recruitment 0x8F9487 (searches with 0x7A41D4 per team slot) and vt+0x68, then resets to 2·LTR = 10 ticks, so it repeats every 1.98 s.
- 0x8F8F68 resets +0x6C to LTR, so it repeats every 0.99 s.
- 0x8F7949 compares team ages against LTR·60 = 300 ticks.
- Both counters start at 0 in the AIPlayer constructor (0x8F7F7D, 0x8F7F94), and the startup delay +0x18 = 2 is the same for all. AI players created together therefore probably fire in the same tick, giving aligned spikes every 1 s and 2 s.

**(d) TheSkirmishAIManager 0x6A96A0 (sub 5):** every tick once the logic frame is above 9; per entry it calls 0x8E3CF3 and 0x8EDDF6.

**(e) Sub 1:** 0x81BE85 runs every LTR = 5 ticks (1 s); 0x820EF0 runs each entry on its own period.

**(f) Collision broadphase 0xB6E6B0 (sub 2):** every tick. It falls into a full rebuild when the change count exceeds its threshold (branch in 0xB6D8A0). The cost grows with unit density.

**(g) Partition 0xA3B4E0 (sub 2):** every tick, proportional to the number of objects that moved.

**(h) Player::update 0x6AF269:** every LTR frames it sends one message for the local player. Cheap.

**(i) Work that now runs on every iteration (60/s), none of it periodic or heavy:**
- Asset streamer [0xDEF548] vt28 = 0xA37E50: finishes 7 async load queues. It is driven by first use of new assets and is a one-off stall source; calling it more often adds no work.
- Script-debug bridge 0x604189 and 0x603452: does nothing in retail ([0xDE3B98] is NULL).
- Watchdog 0x631D04: time() inside a critical section, trivial.
- Debug vt94 and IsIconic 0x44181F (minimized loop only).

**(j) AotR Palantir sampler:** every 30 Palantir updates, about 1 s (0xED0816). Runs on A-renders only, cheap.

**(k) Autosave:** no periodic path found in the skirmish loop. 'AutoSave' strings are referenced only by save/GUI code (0x6DCB35, 0x6DE8F1, 0x817667).
EVIDENCE: - Decompiles of 0x6F2364, 0x93A530, 0x6FEC63, 0x6AF269, 0x8F3EFE, 0x8F7F2B, 0x8F3DF3, 0x8F9487, 0x8F8F68, 0x6A96A0, 0x81BE85, 0x820EF0, 0xA3B4E0, 0xB6E6B0/0xB6D8A0, 0xA37E50 and 0x631D04.
- AISkirmishPlayer vtable 0xC79788 (slot 0x14 → 0x8F9705, slot 0x48 → 0x8F3EFE).
- GameData parse table: 0xC010EC MaxPathfindCellsPerFrame → +0x11E8.
IMPLICATION: - The likely sources of the 55-210 ms spikes are: aligned AI-player routines (1 s or 2 s period), long single path searches or zone rebuilds after building changes, and asset finishing on first appearance.
- All of these behave the same in stock.
- Whether the spikes repeat every 5 or 10 ticks can only be seen from stall lines that log the logic frame.

## FINDING [confirmed] Client work per A-render and per B-render that grows with unit count
**A-render only:**
- Drawable update block, run once per m_frame (0x648705: [0xD9F6F8] == m_frame means skip). It calls 0x675996 for every drawable on every A-render.
- On the isTick A-render (k = 1) it also calls 0x68D8F7 (shroud status) and does the history shift for every drawable with +0xFC set, and calls [0xDE4BD0] vt18 at 0x64875A.
- Particle manager update (C7 0x449D40) runs on A-renders only.

**Both A and B:**
- The W3DView drawable pass at 0x48C701 (B reaches it through the S1 jump).
- The scene render (RenderViews) with shadows, water and RenderUI, and particle drawing.

The CSV shows both growing with draw count: a_rel 2.3 → 9.3 ms, b_rel 1.4 → 6-7 ms. A-only sites run once per m_frame, as in stock. The B-render cost is new in 60 mode: it sits directly in front of the logic step in every Y iteration.
EVIDENCE: - Disassembly of 0x6486C0..0x6487D0.
- PLAN 1.4 and 1.6.
- Log: GATE_GC_DRAWBLK 60: 0/6, INT_RECOIL and INT_FXEV_GATE growth.
- CSV a_rel and b_rel.
IMPLICATION: - The B-render's 5-7 ms shrinks the logic budget of the Y iteration from 16.5 ms to about 10-11 ms. That is why the threshold is crossed about twice as early as it would be with a free B-render.
- X iterations have about 7 ms of slack, but the stepper order cannot use it for logic.

## REC [now] Confirm the heavy sub and the spike period with the current telemetry build
The snapshot came from an older DLL. Its log has no 'stall' lines and the CSV lacks the iter_max_ms..big_stalls columns that telemetry.cpp:320-325 and :381 now write.
- Re-run one long skirmish with Telemetry=1 on the current build.
- Check that the 'stall 60 B ... logic X (sub N, frame F)' lines show N = 5 for the about 1-per-tick stalls. That would confirm the sub-5 inference; N = 3 or 4 would point at AIUpdate instead.
- Bucket the larger stalls by logic frame mod 5 and mod 10 to test the aligned AI-player routines (0x8F8F68 every 5 ticks, 0x8F3EFE every 10 ticks).
- Log the presentation interval ([0xDD302C]) as well.
RISK: None (measurement only, read-only).

## REC [after-diagnostics] Add a fallback or user-facing rule for the 'one long frame every tick' pattern
The current FallbackPolicy needs lateRatio > 0.5 or lostRatio > 0.25, 0.05 or 0.003 combined with on-time < 0.5 (pacer_policy.cpp:5-8). Here only about 10 % of iterations are late and at most about 3.3 % of time is lost, so it never fires.

Yet once the heaviest logic sub-step exceeds about 16.5 − b_rel ms, 60 mode shows a 1.6-2.7× frame five times a second while stock looks smooth. A suggested criterion: late releases ≥ about 0.8 per logic tick and mean lateness > about T/2 for N consecutive windows → back to 30 (or a lower even cadence), and probe again later.

The engine cannot remove the gap itself. A single GameLogic::update(5) call cannot be split without the A-render seeing a half-updated logic step, and changing MaxPathfindCellsPerFrame or similar INI values would change logic results.
RISK: Pacer or policy change outside this area. Risk of flapping between modes in mid-size battles; needs hysteresis and should be tested with the pacer simulator.

## REC [optional] Make the B-render cheaper on iterations whose logic sub-step is heavy
The B-render (4.4-7 ms CPU before Present) is in the same 16.5 ms slot as the logic sub-step. Every millisecond saved there raises the point where the per-tick stretch starts.

Candidates:
- Re-use more of A's work in B (the B drawable pass at 0x48C701 and the scene render).
- Skip B-render content on the Y iteration whose next sub is known to be heavy (by sub number and an EWMA of measured cost).

This only delays the problem, from an L5 of about 11 to about 16 ms. It does not fix the gap at L5 of about 30 ms.
RISK: Presentation-path change; the B-render must keep filling the drawable transform cache exactly like stock render k before logic runs (PLAN 1.2, C4), so any shortcut must keep that pass.

## REC [now] Tell the user what to expect
The regular hitch is the engine's per-tick logic step (most likely sub 5: unit physics and behavior modules, AI player thinking, a second pathfinding pass) becoming longer than half a stock frame as the battle grows. Stock spends the same CPU time but hides it inside its 33 ms frame. It is not a bug in the 60 FPS design or a speed problem: logic held about 5.0 Hz.

The occasional 100-200 ms freezes also happen in stock. In large late-game battles, toggling to 30 FPS with Ctrl+Shift+F11 gives the smoother picture until a fallback rule exists.
RISK: None.

## OPEN
- **Which sub is heavy:** sub 5 is inferred from the work it contains plus about 1.0-1.3 late releases per tick; static analysis cannot measure time per sub. Subs 1 (scripts and Lua on AotR maps), 2 (collision broadphase) and 3/4 (AIUpdate halves) cannot be ruled out as occasional second late releases.
- **What the 55-210 ms spikes are:** asset finishing, path-zone rebuild, aligned AI-player routines (1 s or 2 s), long single path searches or DXVK pipeline compiles. They cannot be told apart at 10.6 s CSV resolution. They are present before t=260 s and in stock, so they are not caused by 60 mode.
- **AI timer alignment:** that all AI players' 0x8F3EFE and 0x8F8F68 counters fire in the same tick is inferred from identical constructor values (0) and the startup delay (2). It was not traced through player creation order or the +0x358 enable timing.
- **Unresolved module phases:** module classes whose slot 0x30 is not a constant return were not resolved. A few could be update modules with a computed phase, but none map to bucket 3 as far as checked.
- **Present mode and refresh rate:** the session's present mode (FIFO vs IMMEDIATE) and 120 Hz vblank quantization were not logged. They change how the 26-44 ms gap is displayed (whole 8.33 ms refreshes under FIFO), not its cause.
- **Repayment ceiling:** the stretch-plus-repayment numbers assume repayment is limited by T/8 (pacer.cpp:248). Beyond about 22 ms owed per tick, owed overflows to debt, as seen at t=421-432 (debt 350 ms per window, logic 4.93 and 4.59 Hz).

## VERIFIER OVERALL
The engine analysis gets the main mechanism right, but it overstates how much the data proves and misses three things that matter.

**What holds up (code and data verified):**
- The order of work in a Y iteration is B-render, B Present, then logic sub k+1, then the pacer (0x6325CF → 0x6325D5 → 0x632622..0x6326F0). Render 6 is followed by sub 1.
- The sub-to-bucket mapping in 0x62E4E8 is correct (0x62E982..0x62E9B7): subs 3/4 = bucket 0 split in half, sub 5 = buckets 1+2, sub 6 = bucket 3. The phase functions are correct: 0x851E97 returns 0, 0x490AC4 returns 1, 0x64DFDA returns 2.
- I recomputed most CSV numbers and they match: late releases per tick 1.01-1.38 from t=283; mean lateness 7.7/8.1/5.7/7.9 ms at t=283-315 and 22.3/22.6 ms at the end; INT_RECOIL interval rates 433 and 824.
- The repay arithmetic holds: at most T/8 ≈ 2.06 ms per on-time iteration, about 22 ms per tick, against about 30 ms of lateness per tick at the end, so time owed overflows into debt.
- The fallback analysis holds.

So "one long iteration per 198 ms logic tick, growing with army size, that stock hides in its 33 ms frame" is the best-supported explanation for "regular hitches after a few minutes".

**What it missed that matters:**
1. **The pacer moves the threshold, not just the engine.** While nothing is owed, the Present spacing wait (pacer.cpp:339-350) holds the B Present until about lastPresent + 16.25 ms, capped at release + 9.9 ms. That wait sits between the B-render and the logic step, so it adds about a_rel − b_rel (2.2-4.0 ms in the CSV) to the Y iteration. The real budget is about 16.5 − a_rel, not 16.5 − b_rel. In a model run with onset render costs (A 7, B 4.5 ms), the heavy step becomes late once per tick at L5 ≈ 10-11 ms with the wait, versus about 13-14 ms without it. At L5 = 20 ms the mean lateness is 12.9 ms with the wait and 8.7 ms without. This is fixable on the DLL side, for example by not waiting before the B Present.
2. **The model does not reproduce one CSV column.** Every window shows Present spacing minimums of 1.9-5.7 ms, about equal to b_rel. The model never goes below 8.4 ms. This points to A-renders that sometimes take 14 ms or more to reach Present, or late X iterations, in every window. It is not explained.
3. **The session DLL had a once-a-second display query on the game thread.** The session build (the phase-6 commit '60 FPS on the Living World strategic map', run 14:32-14:40) ran DisplayBlockReason with MonitorFromWindow / GetMonitorInfoW / EnumDisplaySettingsW about once a second whenever vsync was on. It was removed in 'no display queries in the frame loop; frame-timing stall diagnostics' at 14:52 as a suspected 1 Hz fullscreen hitch. The analysis never mentions it. On the test PC's desktop the three calls take 0.005 ms median and 0.13 ms max over 200 runs, so it is probably not the cause; exclusive fullscreen was not measured.

**Smaller problems:**
- The "model reproduces the CSV" argument does not single out sub 5. A heavy isTick A-render reproduces the same late-per-tick and on-time numbers.
- A better argument the analysis did not make: the mean a_rel caps the heaviest A-render at about 6·a_rel − 5·b_rel. That is about 19 ms at t=283 and about 29 ms at t=421, which is too little to produce the observed lateness by itself. So most of the lateness is on the logic side (INFERRED).
- Several numbers are cherry-picked or not reproducible; see the checks below.

- [confirmed] Each Y iteration runs release → B-render k → B Present → logic sub k+1 (render 6 → sub 1) → pacer, so the cost of sub 5 delays A-render 5 directly
  Checked the disassembly of 0x6325A0..0x632708:
- C0/clientUpdate at 0x6325CF, then the halt test at 0x6325D5.
- s++ at 0x632622/0x632625. When s > 6, s is set to 1 at 0x6326BB and step(1) is called at 0x6326C6; otherwise step(s) at 0x6326F0.
- The limiter follows at 0x63A196.

This is CONFIRMED.

- [confirmed] What runs on each sub-step: bucket 0 on subs 3/4 (halves), buckets 1 and 2 on sub 5, bucket 3 on sub 6; TheAI and the managers on sub 5 only; partition and collision on sub 2; the pathfinder queue on every sub
  - 0x62E982..0x62E9B7 sets the ranges: sub ≤ 2 none, subs 3/4 → [0,1), sub 5 → [1,3), sub 6 → [3,4).
- Half split: 0x62E9D4 (sub 4 starts at size/2) and 0x62EA09 (sub 3 stops at size/2).
- Sub 5 only: `cmp [ebp+8],5` at 0x62EB63 and 0x62EB7B.
- Sub 2: `cmp 2` at 0x62E935.
- 0x6F2364 is called at 0x62E69F on every sub.
- 0x6FEC63 calls 0x6F2364 and then jumps to [0xDE4928] (ThePlayerList) vt28.
- Bucket choice: module vt+0x30 at 0x62BB20; never-wake modules (0x3FFFFFFF) go to a separate list at +0xF8.
- Phase functions verified: 0x851E97 returns 0, 0x490AC4 returns 1, 0x64DFDA returns 2.

Minor errors:
- phases.txt header says '== 2 160', not 163.
- MissileUpdate, listed in the phase-2 group, is actually in the unresolved list.
- The names Partition and Collision for 0xDE4354 / 0xDE4360 are not in subsys_map.txt (unverified).

- [refuted] The heavy sub-step is invisible in 60 mode only while L < 16.5 − b_rel (about 11-12 ms)
  This ignores the Present spacing wait. When owed == 0 (pacer.cpp:339-350), the B Present waits until lastPresent + halfPair − 0.25 ms, capped at release + 0.6·halfPair = 9.9 ms. That is about the A Present's phase, so the wait sits before the logic step.

The CSV shows waits of 300-390 ms per window at the onset (t=262-325). In that phase a_rel − b_rel = 2.2-2.5 ms.

Model (hitch\verify_engine\sim2.py, A=7, B=4.5, light subs 3 ms): late releases per tick by L5:

| L5 | Spacing wait on | Spacing wait off |
|---|---|---|
| 9 ms | 0.40 | 0.00 |
| 10 ms | 0.79 | 0.00 |
| 11 ms | 0.92 | 0.25 |
| 12 ms | 1.02 | 0.57 |
| 13 ms | 1.02 | 0.85 |

So the effective threshold is about 16.5 − a_rel (about 9-10 ms at onset), and part of the onset is caused by the pacer. The L estimates in the analysis are therefore about 2-4 ms too high in the onset phase.

- [confirmed] Recomputed CSV figures: 0.04-0.48 late per tick until t=251, 0.61 at t=262-272, 1.02-1.34 from t=283; mean lateness 5.7-8.1, then 16-19, then 22.3-22.6 ms
  My recomputation (late_releases / (logic_ticks_s · 10.6)):
- Before t=262: 0.04-0.44, plus 1.48 at t=145.
- t=262 and t=272: 0.61.
- From t=283: 1.01-1.38. The analysis said 1.34; 1.38 is the last window.
- Mean lateness: 7.7 / 8.1 / 5.7 / 7.9 ms (t=283-315), 16.3-19.2 ms (t=357-410), 22.3 / 22.6 ms (t=421, 432).

The means include the 55-210 ms spikes. Removing the largest spike per window gives about 6.5 ms at t=283 and about 21 ms at t=421.

- [confirmed] Draw count per tick 157 → 433 → 824 (INT_RECOIL)
  - (352.80·844 − 157.14·245)/599 = 432.8.
- (550.00·1452 − 352.80·844)/608 = 823.7.
- Values from aotr60.log lines 9, 61 and 113.

The time mapping is about 7 s early: mode-on 14:34:07.7 falls at CSV t≈70, not 63, because the 52.9-71.4 window includes loading (renders 3.46/s). So the summaries fall at t≈121, 242 and 363.

- [refuted] The model of the current pacer.cpp with one heavy sub of 20 or 30 ms reproduces the CSV (20 ms: mean late 10.3 ms, spacing p95/max 26/30)
  I reran `subsim.py 9 5 5 4 5 5 20 2`: mean lateness 12.9 ms (not 10.3), p95/max 28.7/32.8 ms. The 30 ms case and the spike case match what the analysis reported.

More important problems:
1. The model's minimum Present spacing is 8.4-10 ms, while the CSV shows 1.9-5.7 ms in every window. That is about b_rel, which points to A-renders reaching Present 14 ms or more after release, or late X iterations.
2. A heavy isTick A-render reproduces the same signature: A1=24 gives 1.00 late per tick, 7.9 ms, 91.6 % on time; A1=32 gives 16.0 ms. So the CSV alone cannot single out sub 5. The claim that the model 'explains every CSV column' is wrong.

- [uncertain] Sub 5 is the heavy step (INFERRED by the analysis)
  There is a better argument than the one in the analysis.
- B-render work is a subset of A-render work, so A_other ≥ about b_rel. Then the heaviest A-render ≤ 6·a_rel − 5·b_rel: at t=283 that is 6·6.94 − 5·4.51 = 19.1 ms; at t=421 it is 6·8.99 − 5·5.02 = 28.8 ms. Those cap the X lateness at about 2.6 ms and 12.3 ms, plus post-Present work, against observed means of about 6.5 and 21 ms. So most of the lateness is on the Y (logic) side (INFERRED).
- Within Y, sub 5 versus sub 1 (scripts and Lua; it runs right before isTick A-render 1) or sub 2 cannot be told apart. The extra 0.1-0.38 late releases per tick beyond 1.0 fit the AI routines every 5 and 10 ticks (0x8F8F68 sets +0x6C to LTR; 0x8F3EFE sets +0x28 to 2·LTR; both verified).

- [confirmed] The pacer carries lateness as owed; repayment ≤ T/8 per on-time iteration; owed above 6T becomes debt; fallback never triggers
  - pacer.cpp:211-258: clamp 3T, cap 6T, pay = min(owed, T/8, slack), re-anchor on late.
- About 10.8 on-time iterations per tick × 2.06 ms gives ≤ 22.3 ms of repayment per tick, against about 1.36 × 22 ≈ 30 ms of lateness per tick at the end. That makes about 7.7 ms/tick, roughly 400 ms per window, of debt, matching 350 ms.
- pacer_policy.cpp:5-8 thresholds: lateRatio about 0.1, lostRatio ≤ about 0.033.

- [uncertain] Present spacing wait switches off as owed builds up: present_wait_ms drops 195 → 106 → 27.7 → 2.7 → 0.0
  The mechanism is confirmed (pacer.cpp:339 requires g_owed == 0). The series is cherry-picked: it skips t=378 (112.3), t=400 (115.5) and t=410 (143.7), so the decline is not monotonic.

Consequence (CONFIRMED in code): with the wait off, Presents land at release + a_rel and release + b_rel, so spacing alternates by about ±(a_rel − b_rel), 3-4 ms, on top of the long frame.

- [uncertain] Logic held stock rate except the last windows, which fell because owed overflowed (4.93, 4.59 Hz)
  - t=336 is also low (4.892 Hz).
- At t=432 the logic rate is 4.589 Hz and sync_ms_s is 923.8, a loss of about 7.6 %, but debt (350 ms) accounts for only 3.3 % and gaps = 0.
- The rest probably comes from logic not advancing, for example the menu open before the exit logged at 14:40:14 ('game reset'). That is INFERRED, not debt.
- The t=421 and t=432 windows report almost identical debt (349.99 and 350.03 ms), worth a check.

- [confirmed] Stock limiter: waits until 33 ms after [0xDE4318], then sets it to now; no catch-up
  0x63A196..0x63A1F8 checks timeGetTime − [0xDE4318] < P in a Sleep(0) loop (Sleep with EBX=0 at 0x63A1DC; a yield loop, not a pure spin), then writes [0xDE4318] = now at 0x63A1F8. There is no catch-up.

The stock-side frame estimates (even at t≈283-325, about 35-44 ms per tick at the end) remain model inferences.

- [confirmed] The snapshot comes from an older build without stall lines; nothing in 60 mode makes the logic heavier
  - Build: the session (14:32-14:40) ran the phase-6 build ('60 FPS on the Living World strategic map', committed 14:29:55). 'No display queries in the frame loop; frame-timing stall diagnostics' (14:52) added the stall lines and new CSV columns.
- Per-logic-call DLL work in the session build (LogicUpdateWrapper) is constant time at Telemetry=1. getCRC 0x625886 runs only at Telemetry=2.
- The C5 table (c5_physics.cpp) is a fixed 4096-slot table with 8 probes, so no growth over time.
- The analysis did miss that the session build ran DisplayBlockReason display queries about once a second (frame_ctl.cpp lines 49-80, from BlockReason line 184, called every C0 at line 363). See 'missed'.

- [confirmed] A-render 1 (isTick) does extra per-drawable work
  - Per-drawable work happens only when the isTick result is set (0x648741/0x648768): 0x68D8F7 at 0x648786 and 0x678F54 at 0x6487CC.
- GhostObjectManager [0xDE4BD0] vt18 is called at 0x64875A.
- The block is m_frame-gated at 0x648705.

The 'history shift' wording is loose; the calls are a shroud/obscured status update.

## VERIFIER MISSED
1. **The pacer's Present spacing wait helps cause the onset.**
   - pacer.cpp:339-350 delays the B Present to about the A Present's phase (up to release + 9.9 ms) whenever owed == 0, and that wait comes right before the logic step.
   - In the onset phase (present_wait 300-390 ms per window, a_rel − b_rel ≈ 2.2-2.5 ms) the heavy step's budget is therefore about 16.5 − a_rel, not 16.5 − b_rel.
   - In the model the once-per-tick late release starts at L5 ≈ 10-11 ms instead of about 13-14 ms, and every heavy Y iteration gets about 4 ms more lateness (L5=20: 12.9 versus 8.7 ms).
   - This can be fixed without touching the engine, for example by not applying the wait to B Presents. That trades away some evenness of Present spacing. The analysis recommends a cheaper B-render instead and does not mention this.

2. **Present spacing minimums of 1.9-5.7 ms in every window are unexplained.**
   - The model never goes below 8.4 ms.
   - The minimums are about equal to b_rel, which points to an A-render (probably the isTick render) that reaches Present 14 ms or more after release, or a late X iteration, in every window.
   - That gives one short and one long Present interval, a second source of uneven frames that the analysis ignores.
   - The current build's render_a / render_b and stall columns can measure it.

3. **The session DLL ran display queries about once a second.**
   - The session build (14:32) called MonitorFromWindow, GetMonitorInfoW and EnumDisplaySettingsW on the game thread about once a second whenever vsync was on (frame_ctl.cpp lines 49-80 in that commit).
   - The commit 'no display queries in the frame loop' removed it at 14:52 as a suspected 1 Hz fullscreen hitch.
   - The CSV shows 10-22 late releases per window (about 1-2 per second, mean about 12 ms) from t=92 on, a once-a-second pattern the analysis folds into 'spikes'.
   - On the test PC's desktop the three calls take 0.005 ms median and 0.13 ms max over 200 runs (hitch\verify_engine\dispq.py), so they are probably not the cause. Exclusive fullscreen and DXVK were not measured.
   - The AI routines every 5 ticks (0x8F8F68) are an equally good fit for the once-a-second pattern.

4. **Fullscreen FIFO at 120 Hz was not considered.** With FIFO the pacer's 33.000 ms pair (60.6 Hz) gives one frame only 8.3 ms of display about every 50 frames (about 0.8 s). That is a regular micro-judder independent of load. The present mode was not logged; the current build logs it.

5. **The last window's logic rate is not explained by debt.** The 7.6 % loss at t=432 is more than the 3.3 % debt can explain, and is probably a pause or menu before the exit.

The analysis's 'model explains every column' and 'sub 5' claims should stay INFERRED until the current build's stall lines show the sub number and render split.

Scratch files: <analysis workspace>/hitch\verify_engine\sim2.py (the analysis model plus NOSPACE=1 to turn off the spacing wait, A1=<ms> for a heavy isTick A-render, and the spacing minimum) and dispq.py (display-query timing).