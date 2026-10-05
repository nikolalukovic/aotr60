**AUDIT 2: consumers of drawable transform getters 0x6765B9, 0x67171D and 0x676711**

**Verdict.** No consumer can change logic state or logic-RNG use. Every logic-invoked consumer gets a bit-identical value under the plan, as long as the in-window coverage rule (flag F1) holds. There are two edge flags (F2 key collision, F3 wrong span length).

**Getter mechanics (verified in game.dat)**
- **0x6765B9** has a single-slot cache. The key at `+0x244` is compared with GC vt7C (m_frame); the C4 span at 0x6765D5 is 12 bytes, `8b 0d 88 43 de 00 8b 01 53 ff 50 7c`, and the compare is at 0x6765E3.
  - On a miss it lerps `+0x3AC/+0x3DC` and runs Catmull-Rom `+0x40C..+0x430` at the live fraction GE+0x3C (0x676688). It stores the key at 0x6766DD.
  - The snap branch (`+0x3A4 < GL+0x40-2`) copies the object matrix once and does not depend on the fraction.
- **Key invalidation (key set to -1):**
  - 0x674B1F (0x674B2C);
  - Drawable reactToTransformChange 0x67460B (0x674611);
  - the constructor (0x679CE6);
  - GameLogic::update sub 1 at 0x62E926, which runs 0x674B1F(0) on every object-bound drawable after the module-update loop 0x779A3D. So all keys are -1 after each sub-1 history update.
- **The key is not saved.** Drawable::xfer (this = drawable+0x60) xfers +0x258, +0x2A4 and +0x2F0 (0x67A58E..0x67A5A7), not +0x244.
- **0x676711 and 0x67679B are uncached.** They always use the live fraction and write the scratch field `+0x238`. Their only side effect is the first-use init through 0x674B1F (when `+0x444==0`).
- **0x67171D has one caller,** 0x67C25D in 0x67C1FB. 0x67C1FB is called only by Drawable::draw 0x67C482, which in turn is called only from the pass callback 0x485331 and the sub-drawable draw 0x4C77F4. It is render-only.
- **References.** A full E8/E9 scan finds 35 direct call sites of 0x6765B9 (in the 26 functions below). No absolute pointer to 0x6765B9, 0x67171D, 0x676711 or 0x67679B exists, so there are no vtable or function-pointer consumers.
  - Correction: ghq attributes 0x48EC4E and 0x48EC75 to the wrong function (0x48EA29). The real function is 0x48EAD8, InGameUI vtable slot 0x210 (pointer at 0xBDDB9C).

**Render / client / FX callers of 0x6765B9 (never logic sinks)**
- **Model draws using their own drawable `[mod+8]`:**
  - 0x4B2C18, 0x4B7BA0, 0x4B9696, 0x4C04A3, 0x4C6514, 0x4B8803 (draw slot 0x3C), 0x4B2A9B (decal draw slot 0x6C → 0x68DB35);
  - the reactToTransformChange slot (0x98) functions 0x4B7748, 0x4B1339 and 0x4CF5EE;
  - debris draw 0x4B135C.
  - 0x4BF15E calls the getter only when the drawable has no object (returns +8).
- **Client calls on other drawables:**
  - InGameUI 0x48EAD8 (first selected drawable);
  - TerrainVisual slot 0x68 0x49249D (bib);
  - terrain render 0x4E3EC5/0x4E42DE (via 0x4E53D3 ← 0x4E28F0) and 0x4E41E2;
  - 0x4F42BF (← 0x4F43A6 ← 0x499D6D ← 0x470F44 / 0x49C3DF (LW vtable 0xBDE848));
  - ParticleSystem::update 0x5FA2CE;
  - tornado bone cache 0x676BD7 (← 0x678BF2 ← 0x5F2EBC ← particle 0x96AAD2);
  - Drawable::draw 0x67C1FB.
- **Client modules:** sway 0x8CD55F uses the client RNG 0x6D33AB (seed 0xDA1C74; the logic seed is 0xDA1CA4).

**Logic-invoked callers and where the result goes**
- **0x6CE95D** (weapon fire, via 0x69213E ← 0x74C258..0x7520D5): result goes only to `0x494615` → FXList::doFXPos 0x5E21D8, behind the throttle 0x5E275B.
- **0x76B668** (AI state, vt 0xC2DA48+0x10): result goes to doFXPos only. Its logic-RNG call (0x6D328E, line 0x37B) comes earlier and does not depend on the getter.
- **0x895ECE** (vt 0xC64818+0x58): doFXPos only.
- **0x853EDF** (vt 0xC563D0/0xC76858 +0x44, 8 callers): doFXPos only (0x854334 → 0x854346).
- **Disguise path 0x7779E3 → 0x776F03 → updateDrawable 0x675996 → 0x68DB35:**
  - 0x68DB35 reads the drawable's interpolated translation, or for hordes calls `[obj+0x258]`→vt7C→slot 0x190 = 0x8707A0, the member centroid.
  - The result goes only to draw module[0] vt+0x64 (0x672C55), which is client state.
  - 0x8707A0 writes only its out-parameter. Its TheTerrainLogic vt1C call has a 5-argument signature that matches getLayerHeight, a query.
- **Logic-created or moved drawables:** 0x67460B → model-draw reactToTransformChange → render-object transforms (client).
- **0x67A544 Drawable::xfer:** save/load only, never the CRC (getCRC 0x625886 xfers objects at +0x60, not drawables). It saves a cosmetic drawable matrix and sets the drawable's own transform (0x70BA76).
- **FX and the logic RNG:** the FXList code range 0x5D0000-0x5E6000 has no logic-RNG call sites. The one apparent hit, 0x5E4CD3, is called only by AI 0x76D25F.
- **0x676711 has 85 call sites; logic ones include 0x6CEA2C and 0x89601F.** At logic time its value depends only on the history (written by 0x62E926 or by first-use init at the same logic state) and on the stepper fraction (0x632642/0x6326BE). So it is identical in 30 and 60 mode whatever the consumer does with it.

**Value identity (stock vs plan: B-render = stock render k, C4 key = 2·m_frame − window)**
- **Sub 1, before 0x62E926** (all four FX consumers and the disguise path):
  - Stock: a cache hit from render 6 at fraction 6/6 if the drawable was drawn, otherwise a recompute at 1/6.
  - Plan: the B-render 6 pass uses the same camera and region (S1 → 0x48C701; the region comes from `[ebx-0x3C]`, `[ebx-0x38]`, `[ebx+0x100]`). Drawable::draw calls 0x67C1FB at 0x67C4F9 before the modules, so every drawn drawable gets key 2m at 6/6. Undrawn drawables miss and recompute at 1/6. Identical.
- **Sub 1 after 0x62E926:** every key is -1, so both modes recompute at 1/6.
- **Subs 2..6:** both modes get a hit at k/6, or a recompute at (k+1)/6.
- **xfer:** APT is A-only and runs outside the presentation windows. Before m_frame++, key 2·m_old hits the B/logic entry (stock: m_old). After the increment both modes miss and recompute at k/6.
- **Required condition:** every getter call made inside an A presentation window must be repeated outside the windows before logic runs. The A window writes key 2m−1 into the single slot; if nothing rewrites it, logic recomputes at (k+1)/6 where stock hits at k/6.
  - Checked against the A-only gates that sit inside windows:
    - debris 0x4B135C queries only its own drawable, which the B-render already refreshed at 0x67C1FF;
    - recoil 0x4C78CE → 0x4B4699 and shrubs 0x4E83F9 → 0x4E5D2C do not reach 0x6765B9.
  - Trees 0x449D55 and particles 0x449D40/0x444CF2 run outside the windows. The other in-window callers that query other drawables (0x4C77F4, 0x48EAD8, terrain 0x4E3EC5/0x4F42BF, decal 0x4B2A9B) are not gated and run again on B.

**Flags**
- **F1 (spec rule).** verify_sites must reject any A-only gate inside a presentation window that can reach 0x6765B9 for a drawable other than the one being drawn. That window is the S2 drawable pass 0x48C701..0x48C765 and the scene render 0x449DAB..0x44A23E. Also, the material gate at 0x67C4CB must not skip the 0x67C4F9 call.
- **F2 (key collision).**
  - The A-window key 2m−1 equals the -1 "invalid" marker when m_frame is 0. Every invalidated drawable would then get a false cache hit and return a stale `+0x208`.
  - After a 60→30 switch, the stored keys 2m and 2m−1 can equal the 30-mode keys m+1..m+6 before the next 0x62E926 reset whenever m ≤ 7.
  - Fix: the mode controller should require m_frame ≥ 8. This costs nothing and keeps the 30-mode path stock.
- **F3 (wrong span length).** The 0x671774 span is 14 bytes, `8b 0d 88 43 de 00 8b 01 83 c4 10 ff 50 7c`, ending at 0x671782. PLAN §1.3 says 11 bytes, which would end at 0x67177F, before `call [eax+0x7c]`. G8 already lists 14 bytes. 0x67173B (11 bytes) and 0x6765D5 (12 bytes) are correct. No branches target the inside of any of these spans.
- **F4 (cosmetic).** On the first A-render after a 30→60 switch, lookups before m_frame++ (for example an APT save calling xfer) use key 2m against a stored m. That forces a recompute at k/6 where stock would hit at (k−1)/6. It affects only drawable matrices in a save made on that exact frame, never logic.
- **F5 (performance).** The single slot alternates between 2m−1 and 2m, so it recomputes about twice per pair, even while paused. It is deterministic and logic-neutral.
- **Residual (not proven statically).**
  - The other 33 `call [reg+0x190]` sites are not proven never to dispatch to 0x8707A0. The only one shown to reach it is 0x68DB5B.
  - That TheTerrainLogic vt1C has no side effects is inferred from its signature, not decompiled.
  - The Telemetry=2 RNG trace and getCRC check cover both.
- **review_logic_identity #8 is resolved.** The B camera is now exactly M_k with the identical drawable-pass region, not "close to stock". Every 0x6765B9 consumer reached from logic ends in FXList or draw-module/client state.