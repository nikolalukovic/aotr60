# review_speed_pacing

1. **The hard-fallback trigger is broken both ways.** The plan says "A+B pair exceeds 33 ms in more than 20% of pairs over 10 s".
   - **Spurious trigger on 60 Hz.** On a 60 Hz FIFO display, Present blocks on vsync, so every pair lasts 33.33 ms. Only the one pair in each 1.65 s that skips its Present is short. About 99% of pairs therefore "exceed 33 ms", and fallback fires 10 s after every enable. The user's DXVK log shows FIFO in fullscreen, so 60 FPS would never stay on.
   - **Real slowdown missed.** Logic cost sits in one sub-step: sub 5 runs TheAI plus 13 subsystems (synthesis 1.3). That is 1 pair in 6, or 16.7%, below the 20% threshold. When that pair's Y-iteration (B-render plus sub 5) goes past the 1.25·T clamp (about 37 ms), the overrun is dropped every tick. The game runs permanently slow and the trigger never fires.
   - **Present-skip only recovers vsync wait.** The skipped B frame is still drawn on the CPU and rendered on the GPU. So when the game is CPU- or GPU-bound (for example 4K), only the fallback protects speed.
   - **Fix (replace the bullet):** "The pacer keeps a debt counter: lateness dropped by the clamp, plus deadline resets that were not caused by a gap. Fall back when debt exceeds 0.3% of wall time over a sliding 5 s window, or when Present-skips exceed 10% of B-renders over 5 s. Pair durations are logged only, never used as a trigger. Do not enable 60 mode when vsync is on and the refresh rate (EnumDisplaySettings) is below 59 Hz (G6-16)."

2. **§1.4's camera time multiplier >1 is wrong.** It is fast time, not "slow-motion".
   - **How stock paces it.** 0x63A001..0x63A00A leave 0xDE4320 at 0 when TV vtDC()>1. Then 0x63A194 takes the unlimited path 0x63A200, and the trampoline never runs. The only pacing is in the draw: 0x44B964 `dec [0xD98CB4]` runs on every draw call, a frame renders only when the counter is ≤1 (every M−1 calls), with a 29 ms wait at 0x44B9C1.
   - **What goes wrong in 60 mode.** The counter decrements on both A and B draws. For odd M, the rendered draws lock to one parity. Every g_uiTick-gated item inside the draw then runs at 2× (A parity) or 0× (B parity): C7 particles, view-filter INCs, trees, recoil, shake decay, and in Phase 2 the whole camera, including stepper 0x48A417, which writes +0x23D4.
   - **Phase 2 with B parity.** The camera never steps, so +0x23D4 never returns to 1. The game stays in fast time, with logic at (M−1)× per 29 ms, and the cinematic never ends.
   - **Fix (replace the paragraph and add it to Phase 2):** "M>1: hook 0x44B964 (`ff 0d b4 8c d9 00`) so 0xD98CB4 is decremented only on A-draws. A B-draw renders only if the A-draw before it rendered, and then jumps to 0x44B9E3 (eax=[0xDE4364]) without waiting and without writing 0xDC7700. 0x44B98E/0x44B9C3 stay stock; drop the 14/15 stub."
   - This gives one rendered A+B pair every M−1 sub-steps with a 29 ms wait per pair, the same as stock, for every gate and both camera models.

3. **The pacer misses gaps, which leaves Present-skip working off a stale deadline.**
   - Iterations that take 0x63A200 never call the pacer: M>1, the first 6 m_frames after [0xDE4308] (0x63A17B), UseFPSLimit off, and fast-forward. The plan only resets the deadline on "minimise, load, mode switch".
   - G6-02 asked for detection of "not called in the previous iteration". Without it, every B-Present is skipped for the whole M>1 cinematic, and a catch-up burst (up to 1.25·T of frames run with no wait) follows.
   - **Fix:** "C0 increments g_iter. If the pacer did not run in the previous iteration, or the device-lost Sleep(200) at 0x522696 or Reset_Device ran, set deadline = now with no catch-up. Present-skip is allowed only if the pacer ran in the previous iteration."

4. **Dynamic LOD reads twice the game rate.**
   - 0x4438DA calls 0x601F94 with Display+0x180, the per-draw QPC average from 0x4430BB. In 60 mode that reading is ~2× the game rate.
   - AotR's `aotr\data\ini\gamelod.ini` sheds particles below MinimumFPS 25/20/10. In 60 mode that only happens once the game is at about 40% speed, so the engine's own load shedding is disabled and overloads last longer than in stock.
   - The same LOD manager also sets values that logic reads: SlowDeathScale ([0xDE3B84]+0x179C, read by SlowDeathBehavior at 0x860B62/0x860F7B) and DebrisSkipMask (+0x1798, read by ObjectCreationList at 0x5F1295). They are neutral today only because that INI sets 1.0/0.
   - **Fix:** "In 60 mode, sample the 0x4430BB ring on A-renders only (or pass +0x180/2 at 0x4438DA). In Phase 0, after INI load, assert SlowDeathScale==1 and DebrisSkipMask==0 for every DynamicGameLOD level; otherwise stay at 30."

5. **The halt predicate is undefined.** The table says "B-iterations", but the diagram halts after the A-render. If the halt is keyed on GC+0xC8 / isARender, every render while paused or frozen counts as non-A. The stock stepper then attempts a tick every iteration (60/s), and TheLivingWorldLogic::update (0x632A85..0x632A92) runs at 2× during frozen time.
   - **Fix:** "Force the halted branch iff g_uiTick==1 for the render just completed."

6. **Per-render integrators missing from §1.2.**
   - **Shrubs:** the call at 0x4E83F9 goes to 0x4E5D2C, the same per-call sway-phase integrator the trees use. It is reached from the terrain render (0x4E28F0 sets +0x4FB6D; then 0x47177E → 0x470F44 → 0x46B0A0 → 0x4E837E), so the 0x449D55 tree gate does not cover it. Synthesis row 24 already lists it.
   - **Boat wake:** 0x4D052E steps ±[0xD9A3A0]=0.005 per draw at 0x4D05B3/0x4D05BD. The table names it but gives no site.
   - **Debris draw 0x4B135C** (row 31) and **subobject fade 0x8B8F40→0x672823** (row 82) are not listed.
   - **Fix:** add the rows "Shrubs | call 0x4E83F9 | A-only" and "Boat wake | 0x4D05B3, 0x4D05BD | operand → DLL 0.0025 (or A-only)". Gate 0x4B135C and 0x8B8F40 A-only until the phase-4 sweep traces them.

7. **UI particle split is wrong.** "p+=v/2, damping on A" gives p += v/2 + v·d/2 per pair, so trails travel only (1+d)/2 of the stock distance.
   - **Fix:** "p += v/2 every render; v *= d only on the g_uiTick=0 render, after its half-step" (G2-D14).

8. **The §4 checks cannot catch the errors above.**
   - "Sync 990 ± 1%" passes the uncompensated −1% FIFO deficit.
   - Nothing checks pause, frozen time or M>1.
   - The getCRC guard (a full logic-state CRC at 0x625886) runs before and after each B-render inside the speed runs, which adds load to exactly those iterations.
   - **Fix:**
     - Logic Hz and sync ms/s within ±0.2% of 5.0505 and 990 over 5 min.
     - Sync 0 ms/s and tick attempts 30.3/s (counters on GameLogic vt34 and 0x6BE50E) while paused and in a frozen cinematic.
     - Logic frames per wall second in an M>1 cinematic equal to stock.
     - Debt and Present-skip-ratio lines in the log.
     - getCRC only in the determinism runs; speed runs keep only the [0xDA1CA4] snapshot.

9. **Minor wording fixes.**
   - `Pacing=nominal` is 1% slower, which breaks the requirement. Label it "game −1%" or remove it.
   - Phase 7 delayfix: "5 renders per tick" must read "5 A/B pairs (10 renders) per 165 ms tick" (G8). As written it is 2× fast.
   - Fly transition 0x502458: choose the caves at 0x50257B/0x5026F0. The data variant is approximate (row 54).
   - Add the M>1 fix from item 2 to the Phase-2 patch list.

**Correct; keep as is:**
- Route 2: stepper bytes untouched, the isTick detour, m_frame stays at 30 Hz.
- Trampoline at 0x63A196 (`ff 15 20 09 bd 00`): stock code when 60 mode is off. On entry esi=engine and ebx=0; a cdecl pacer preserves them. It must exit via `jmp 0x63A21F`, which keeps the 0x631D04 heartbeat and the loop head at 0x639EC6, and must write 0xDE4318/0xDE4314/0xDE4310/0xDE430C.
- T = trunc(1000/(fps·mult))/2 = 16.5 ms. That makes 33.000 ms per pair, the same as stock: stock spins on timeGetTime after timeBeginPeriod(1) (0x63A507), which is exactly 33 ms long-run. Use int64 QPC and never touch the x87 control word or MXCSR.
- Present-skip instead of dropping B-renders. EndScene (0x522628) and 0x5766A0 still run, and the stub must return S_OK in eax with ebx=0 (0x52265A). It is legal D3D9, lost-device detection is only one render late, and the FPS average is unaffected.
- Not touching the present interval: stock fullscreen is FIFO, windowed is IMMEDIATE via 0x4466DF.
- The C3 lag rule: immediate 33, and the B-render adds owed time only after an advancing A-render.
- g_uiTick as the single gate. Snow runs every render with only 0x49448D/0x494494 gated. Message propagation runs every render.
- Integer doubling for scripted camera moves (N'=2·max(N,1), hold ×2, instant moves step twice). No mode switch while a move is in flight. Reset hook on 0x44181A.
