# review_logic_identity

1. **Camera time multiplier above 1: the draw-skip counter breaks the per-window match with stock.** (most severe)
   - **What's wrong:** §1.4 keeps 60 mode on through scripted multiplier >1 and only adds C6. In W3DDisplay::draw, when `vtDC() > 1`, the code at `0x44B964` decrements `[0xD98CB4]`. While it is >1 it jumps to `0x44BC5F` and skips drawFrame (`0x44B971 jg`), so no updateViews and no camera step. At `0x44B985` the counter is reloaded from vtDC.
   - **Why it breaks logic:** that counter runs once per draw call. In 60 mode it runs on both A and B renders. The windows that contain camera steps then differ from stock (for m=2 every step lands on the same parity). The time-freeze release (`+0x23D0`), the finished flags (vt78 `0x486352`), the `0x452` waypoint messages and AIGroups therefore land in a different window, and logic reads freeze/vt78 at `0x62E528`/`0x62E53A`.
   - The B-render also reloads the counter from `+0x23D4`. The A-render's half step wrote that value from `ease((2c+1)/2N)` (`0x48A417` tail, path stepper `[0x8F5]`), so it is not stock.
   - **Fix:** under §1.4 "Camera time multiplier > 1", add: "decide draw/skip once per pair. Run the dec/reload at `0x44B964..0x44B985` only on g_uiTick renders. The following B-render draws iff its A-render drew (stub at `0x44B95F`). C6 applies only to drawn renders. g_camDt is A=h, B=33−h, consumed only by drawn renders. Do the same for any other per-render drawFrame skip (`[display+0x115]` at `0x44B995`)."
   - **Also correct the text:** logic does not read `+0x23D4`. `0x62E528` is vtD8 (`+0x23D0`), `0x62E53A` is vt78. vtDC is read only at `0x44B959`/`0x44B97F` and by the limiter enable in `0x639FEF`.

2. **The halt rule for B-iterations is not defined during pause, freeze or stalls.**
   - **What's wrong:** §1 defines the B-step as following an "A-render". During pause, frozen time or a stalled tick there are no A-renders, because GC+0xC8 is 0 on every render.
   - **Why it matters:** if halting is keyed on isARender/C8, the stock stepper runs every iteration. Tick attempts then run at 60/s, so vt98(1) runs LivingWorldLogic::update (`0x632A8A`) twice as often, and the stats at `+0x50`/`+0x5C` double. Attempts also stop being tied to even camera half-steps, so the freeze release can slip a window.
   - **Fix:** in the §1 table add: "Halt iff the render just finished had g_uiTick==1. The stock stepper runs only after g_uiTick==0 renders. Never derive the halt from GC+0xC8 or isARender. Read the mode after any reset hook in the same iteration."

3. **"Per-pair totals are exact" is false for the incremental camera steppers.**
   - **Path stepper `0x48688A` (and `0x489817`):** it accumulates distance in float, `acc[+0x298] += D·(ease(t)−ease(t_prev))`. Splitting the step into 16+17 is not bit-equal to one 33 step.
   - Several logic-visible results come from that drifting accumulator:
     - the waypoint crossings, which emit vt70 → `0x4872A7` → `0x452` messages, AIGroups and `+0x23CC`;
     - the path end (`[+0x284]=0`, then finish on the next call);
     - `+0x23D4 = floor(lerp+0.5)`.
   - A crossing can therefore move across a tick boundary. It is rare per crossing but certain to happen over a campaign.
   - **Rotate-toward branch of `0x48A417` (`+0x1C8`):** it chases `angle += (target−angle)·ease(t)` once per call. With N'=2N the pose differs from stock by more than rounding error.
   - **Fix (§1.2 "ms paths"):** state-split these steppers. On the A-render, snapshot W3DView, step with dt=h and suppress `0x4872A7` appends. Restore when TheDisplay vt30 returns. On the B-render, run the stock step with dt=33.
   - Widen the Phase-0 RainOfFireUpdate rule from "camera input splits" to all camera integrators: eases, rotate-toward, paths and shakes.

4. **The reset hook switches to 30 FPS even when a doubled camera move is in flight.**
   - **What's wrong:** §1.4 forces 30 mode in the reset hook, but nothing guarantees no move is in flight there. W3DView move counters are not saved (View::xfer `0x65ED04`), and `+0x23D0` has no reset writer (its only writers are the steppers and `0x48B5CB`). A load during a cinematic could keep N'/hold' running at 30 Hz, which freezes logic twice as long.
   - **Fix:** "On any forced switch-off, halve the in-flight IDIV counters: total, cur and hold at `+0x1AC/+0x1B0/+0x1BC`, `+0x208/+0x20C` and the other setups' fields. They are always even at that point, because UI loads run before the draw at `0x648869` and logic resets run after a B-render. Alternatively, prove that GameEngine::reset clears them."

5. **The save-compatibility test is not valid as written.**
   - **What's wrong:** `0x635D11` never resets GE+0x34, and loads happen mid-tick in the render phase (`0x818456`, `0x6E047A`). Logic after a load therefore depends on s at load time, even in stock.
   - **Fix (§4):** "Load only from the paused Esc menu (s≥7) in both modes. Log s at load and require it to match. Compare from the first sub 1 after load."
   - The skirmish-replay determinism test cannot exercise camera-to-logic coupling. Add campaign cinematic maps (freeze, waypoint paths, multiplier >1) and LW battles. Log per tick: vt78, `+0x23D0`, `+0x23CC`, TheAI+0x1C, and the number of messages appended through `0x711034`.

6. **The B-render guard only covers B-renders.**
   - **What's wrong:** A-renders and the halted step also changed from stock. A-renders run the gated systems with fraction (2k−1)/12, sync S_{k−1}+h and a half-stepped camera. For example, updateDrawable → `0x68DB35` → `0x6765B9` runs at the A fraction.
   - **Fix:** "Snapshot `[0xDA1CA4]` and getCRC at vt98 exit, then compare at the next vt98 entry." That covers the A-render, the halted step and the B-render. Add `0x6D34A0` to the logic-RNG prologue counters.

7. **The isTick detour must be keyed on the clientUpdate bracket, not on "last render was B".**
   - A logic-side caller exists: update module `0x7779E3` → `0x776F03` → updateDrawable `0x675996` → isTick at `0x6759BB`. It runs inside GameLogic right after the B-render.
   - A sticky B flag would make isTick return 0 at sub 1, where stock returns 1.
   - **Fix:** the §1 table should say "returns 0 only while g_inClientUpdate && B-render".

8. **The drawable transform cache that C4 re-keys is read by logic code and written into saves. Audit it.**
   - Logic-side callers of `0x6765B9`: `0x76B668` (AI state with logic RNG), `0x6CE95D` (weapon fire), `0x8707A0` (an unidentified group-centre virtual at vtable `0xC5B1F8` slot 100, feeding TheTerrainLogic vt1C), and Drawable::xfer at `0x67A5B4`, which writes the matrix into saves.
   - At logic time each returns either the matrix cached during the last render or one recomputed at the logic-time fraction. That depends on which drawables the B-render's camera made visible, and the eases and split proxies only make that camera close to stock, not equal.
   - Render-phase saves taken on an A-render store a pose at fraction (2k−1)/12.
   - **Fix:** add to the Phase 0 audit: "prove every `0x6765B9` consumer is FX or client only. If any feeds logic, the B camera must be state-split so it is bit-exact."

**Right; do not change:**
- **Route 2, verified in code:**
  - The bytes at `0x6325D5` are `84 db a1 88 43 de 00`; the stub must set `eax=[0xDE4388]`.
  - The halted path `0x6325DE..0x6325F3` sets C8=0, calls Debug vt94 and jumps to `0x632705`. That correctly skips `pop edi`, because edi was only pushed at `0x632638`.
  - It touches none of `+0x34/+0x38/+0x3C/+0x40/+0x50..+0x5C`.
  - The failed-tick restore (`s=edi`, so s grows 7, 8, 9… during stalls) and the `+0x40` refresh therefore behave exactly as stock.
- **Fraction:** logic always sees the fraction the stepper computes (`0x632642`/`0x6326BE` run before vt98), so C0's fraction writes cannot reach logic. The B-render fraction 2k/12 = k/6 is the same float, and 1.0 at s>6 is correct.
- **Pause check:** vt98 checks pause only at sub 1 (`0x6329C7..0x632A77`), so subs 2–6 keep stock order.
- **isTick and messages:**
  - isTick `0x63252F` (`a1 0c f6 d9 00`) is s==1 only.
  - `0x4872A7` re-emits until acknowledged, so propagate must run on every render. With that, `+0x23CC` and the `0x452` message count match stock.
  - Replay-camera message `0x447` is excluded from AIGroup creation.
- **IDIV doubling:** this part is exact.
  - Stock already clamps N≥1 (`0x485DC7`/`0x485DE4`).
  - Steppers compute `(float)cur/(float)N` (`0x486644..0x48664D`), so at even renders cur'/N' equals stock.
  - The `0x48A417` main branch and `+0x23D4` use absolute values from ease(t).
  - Doubled holds and finish tests are integer.
- **Other parts that hold:**
  - m_frame advances on A-renders only.
  - C3 lag gives Sync=S_k at the B-render.
  - The eye tower and the Palantir callee gate `0x5039C1` stay A-only.
  - Mode turns on only when s==1 and C8==1.
  - No GlobalData writes.
  - The pacer trampoline at `0x63A196` sits after the `0x63A18E` enable test, so the stock limiter bypasses (multiplier >1, fast-forward, the first 6 frames) stay stock.
