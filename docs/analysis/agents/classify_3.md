# Chunk 3 timing-ref classification (99 functions)

Verified anchor: GameClient::getFrame = GameClient vtable+0x7c -> 0x9f9f1a `mov eax,[ecx+0x10]`; setFrame = vtable+0x38 (0x993d2e). It is incremented by GameEngine vtable+0x9c (0x632409: `if (GameClient[+0xc8]) setFrame(getFrame()+1)`), which the frame stepper 0x6325a0 calls first thing every rendered frame => GameClient frame is the 30 Hz client-frame counter (60 Hz after the change).

Counts: {'keep': 88, 'client_scale': 9, 'uncertain': 2}. All LTR / INIT_ltr refs in this chunk are logic (or logic-time shown in UI) -> keep; LTR itself is not changed by the plan. Client-frame refs: 0x8a2d40, 0x8a6080, 0x8a60c0, 0x8b864c (Drawable fades - see coordination note), 0x8b8f8c, 0x8b8fbc, 0x8e9e1b, 0x8ea686, 0x8ea697, 0x8ea896, 0x92bab3, 0x99eafc, 0x99ec8e, 0x99eca0.

Coordination note (Drawable fade): fadeOut 0x670a50 / fadeIn 0x670aa2 / fade-chain 0x67309d store a duration in GameClient frames (+0x130); updater in FUN_00675996 (0x675ac3..0x675b60) adds GameClient-frame deltas. ~37 call sites; many pass hard-coded frames (10 @0x79b05e,0x8aee86,0x8b1513,0x8b2392,0x85832e; 30 @0x8583c9; 0x8a @0x7954a8; LTR pushed as frames @0x664ff2,0x665031) or GlobalData BuilderFadeIn/OutTime (+0x11e0/+0x11dc, parsed with the LOGIC ms parser 0x73a429). Best fix = central (double +0x130 in the three setters, or feed the fade with getFrame()>>1). If the central fix is used, do NOT redirect 0x8a2d40/0x8a6080/0x8a60c0/0x8b864c (would become 4x).

| entry | purpose | domain | cadence | verdict | conf | refs to redirect / note |
|---|---|---|---|---|---|---|
| 0088d993 | DozerActionDoActionState::update (AI dozer state): repair/build amount per logic frame = rate*x/LTR -> 0x690584 | logic | per_logic_tick | **keep** | high | ref 0x88dc5e. Same fn calls Drawable::fadeOut(GlobalData.BuilderFadeOutTime) at 0x88dde3 - see extra finding builder_fade |
| 00890738 | FlammableUpdate::update: deadline = logicFrame + 3*LTR | logic | per_logic_tick | **keep** | high | ref 0x890a62 |
| 00890bbb | FlammableUpdateModuleData ctor: default field = 5*LTR logic frames | logic | once | **keep** | high | ref 0x890bf6; createData 0x64cd41 |
| 00893871 | LargeGroupBonusUpdateModuleData ctor: field = INIT_ltr 0xde93d4 (=LTR, 1 s) | logic | once | **keep** | high | ref 0x893883 |
| 00895741 | PickupStuffUpdate scan (from update 0x8957b7): lastScan + LTR*data.seconds < logicFrame | logic | per_logic_tick | **keep** | high | ref 0x89575d |
| 00897987 | SpecialPowerModule (shared base, slot 10): timed object status for REAL_TO_INT(LTR*sec) frames via 0x68b581->0x8e2c0f (expiry=logicFrame+n) | logic | event | **keep** | high | ref 0x897a77 |
| 00898a2d | CommandButtonHuntUpdateModuleData ctor: scan rate = LTR | logic | once | **keep** | high | ref 0x898a3d |
| 00899a96 | AutoPickUpUpdateModuleData ctor: scan delay = LTR | logic | once | **keep** | high | ref 0x899ab5 |
| 00899e8c | BoredUpdateModuleData ctor: field = LTR | logic | once | **keep** | high | ref 0x899eb4 |
| 0089a765 | BannerCarrierUpdateModuleData ctor: 15*LTR, 10*LTR x3 defaults | logic | once | **keep** | high | refs 0x89a795/7a1/7ad/7b9 |
| 0089d647 | OneRingPenaltyUpdate::update: returns UpdateSleepTime = LTR | logic | per_logic_tick | **keep** | high | ref 0x89d6ea |
| 0089e169 | HordeAIUpdate command handler: compare (logicFrame - LTR) with last-command frame | logic | event | **keep** | high | ref 0x89e380 (branch not shown in Ghidra C) |
| 0089e4da | HordeWorkerAIUpdate ctor: setWakeFrame(LTR) via 0x850c32 | logic | event | **keep** | high | ref 0x89e53c |
| 008a1b9f | ProductionUpdate::update: LTR*tmpl[0x354] logic countdown +0x108 and timed status 0xda; +0x118=4*LTR, wake logicFrame+4*LTR; FPS30*tmpl[0x354] -> Drawable::fadeIn(frames) on produced objects | mixed | per_logic_tick | **client_scale** | high | 0x8a2d40 CLIENT_FPS_INT_30 -- 0x8a2a75/0x8a2c1c/0x8a2c2f = logic, keep. 0x8a2d40 = client-frame fade duration (Drawable fade counts GameClient frames). Redirect this ref OR use the central fade fix (extra finding drawable_fade), not both |
| 008a52b2 | SlavedUpdate stop-repair: framesToWait(+0x34) = INIT_ltr 0xde95fc (LTR/4 = SLAVED_UPDATE_RATE) | logic | event | **keep** | high | ref 0x8a52bb |
| 008a58fa | SlavedUpdate::setRepairState WELDING: welding particle-system lifetime = framesToWait*LTR (Generals bug frames*fps) -> ParticleSystem lifetime range (0x5f3397 -> sys+0x14) | client | event | **uncertain** | medium | 0x8a5a5b LTR_INT_5 -- ref 0x8a5a5b. Value is in particle-update ticks; keep if particles stay on a 30 Hz tick, x2 by code patch if particles tick per 60 Hz frame. Never redirect the LTR global for it. |
| 008a5b75 | SlavedUpdate::doRepair: heal per logic frame = repairRatePerSecond / LTR | logic | per_logic_tick | **keep** | high | ref 0x8a5bf6 |
| 008a5d00 | SlavedUpdate::update: countdown +0x24 = LTR/4 (logic); data[+0x68] ms * 0.03 -> Drawable::fadeOut/fadeIn (client frames) | mixed | per_logic_tick | **client_scale** | high | 0x8a6080 F_0.03_client_per_ms; 0x8a60c0 F_0.03_client_per_ms -- 0x8a5d23 keep. 0x8a6080/0x8a60c0 client-frame fade durations; redirect OR central fade fix, not both |
| 008aeba6 | (Ghidra mis-bounded) ref lies in undecompiled update 0x8aebd3 (vtable 0xc6b28c, factory map: WallUpgradeUpdate): returns UpdateSleepTime = LTR | logic | per_logic_tick | **keep** | high | ref 0x8aec6a. Neighbour 0x8aec79 calls fadeIn(10) hard-coded (extra finding drawable_fade) |
| 008af542 | LargeGroupAudioUpdateModuleData ctor: update delay = ceil(500 ms*0.005) = 3 logic frames | logic | once | **keep** | medium | ref 0x8af565; logic update-module period |
| 008af912 | RainOfFireUpdate::update: GameLogicRandomValueReal * rate / LTR accumulated per logic frame | logic | per_logic_tick | **keep** | high | ref 0x8af9f3 |
| 008b36b2 | RespawnUpdateModuleData per-level lookup (respawn time): default (int)(0.005*30000)=150 logic frames | logic | event | **keep** | medium | ref 0x8b371c; callers Living World records 0x6e2d92/0x6e2e18 and 0x8b3c1c |
| 008b4eb1 | EmotionTrackerUpdate set-emotion (via Object 0x68f3cb): seconds -> ceil(0.005*s*1000) logic frames | logic | event | **keep** | high | ref 0x8b4ec9 |
| 008b5738 | EmotionTrackerUpdate::update: +0xac = logicFrame + 60*LTR | logic | per_logic_tick | **keep** | high | ref 0x8b5dba |
| 008b65a2 | EntEnragedUpdateModuleData ctor: field = LTR | logic | once | **keep** | high | ref 0x8b65d3 |
| 008b8464 | ObjectCreationUpgrade: created object drawable fadeIn(tmpl[0x16c] ms*0.03) | client | event | **client_scale** | high | 0x8b864c F_0.03_client_per_ms -- client-frame fade duration; redirect OR central fade fix, not both |
| 008b8f40 | SubObjectsUpgrade fade rates: (1/fadeSec)/CLIENT_FPS = alpha step per client frame, passed via Drawable 0x672823 -> draw module vtable+0x88 | client | event | **client_scale** | medium | 0x8b8f8c CLIENT_FPS_INT_30; 0x8b8fbc CLIENT_FPS_INT_30 -- per-frame rate; final integrator in draw module not traced |
| 008ba64c | (Ghidra mis-bounded: dtor) ref lies in ModelConditionUpgrade::upgradeImplementation 0x8ba668: LTR*seconds -> Object timed status 0x68b581 (logic expiry) | logic | event | **keep** | high | ref 0x8ba6c1 |
| 008bc9b0 | SpawnUnitBehavior::update: returns sleep LTR; logicFrame < LTR guard | logic | per_logic_tick | **keep** | high | refs 0x8bc9d3, 0x8bcafb |
| 008bcc06 | (Ghidra mis-bounded: dtor) ref lies in OathbreakersFadeAwayBehavior::update 0x8bcc22: returns sleep LTR | logic | per_logic_tick | **keep** | high | ref 0x8bcc4a |
| 008bf4f0 | CallHelpOnDamageModuleData ctor: field = 4*LTR | logic | once | **keep** | high | ref 0x8bf51e |
| 008c01d1 | (Ghidra mis-bounded ctor) refs lie in undecompiled collide handler 0x8c0201 (vtable 0xc70ef4, factory map: AODCrushCollide): logicFrame + 2*LTR expiries | logic | event | **keep** | high | refs 0x8c02f4/0x8c0310/0x8c0321; module name medium |
| 008ca978 | SiegeDeployHordeSpecialPower update: tick counter vs LTR (1 s) | logic | per_logic_tick | **keep** | high | ref 0x8ca9c8 |
| 008cdce9 | BeaconClientUpdate::clientUpdate: every N logic frames, radar pulse lifetime = logicFrames*0.2 s -> TheRadar createEvent(pos,6,seconds) | client | per_client_frame | **keep** | high | ref 0x8cdd67: seconds are rate-independent (radar converts with CLIENT_FPS inside 0x6d93eb, other chunk); gate is logic-frame based |
| 008d0bf3 | Object module helper (callers 0x6930c9/0x699368): setWakeFrame(2*LTR) | logic | event | **keep** | high | ref 0x8d0c6b |
| 008d2adb | HordeNotifyTargetsOfImminentProbableCrushingUpdateModuleData defaults: 2*LTR x2 | logic | once | **keep** | high | refs 0x8d2ae5/0x8d2aef |
| 008dc182 | TurretAIData ctor (AIUpdate Turret block): recenterTime = 2*LTR | logic | once | **keep** | high | ref 0x8dc1f0 |
| 008e302a | (mis-bounded) refs lie in undecompiled update 0x8e30dd (vtable 0xc781e4): next = logicFrame+LTR, returns sleep LTR | logic | per_logic_tick | **keep** | high | refs 0x8e314e/0x8e3162 |
| 008e38dd | logic update (vtable 0xc11e10): countdown beeper phase = remaining/(10*LTR), tint flash + audio toggled per logic tick | logic | per_logic_tick | **keep** | medium | ref 0x8e3a1b |
| 008e9db2 | InGameUI message line update (vtable 0xc790ac slot 10): alpha = 255 - (clientNow-fadeStart)*255/CLIENT_FPS (1 s fade), clientNow = GameClient::getFrame | ui | per_client_frame | **client_scale** | high | 0x8e9e1b CLIENT_FPS_INT_30 --  |
| 008ea5d8 | InGameUI message add (via 0x69bc16, e.g. GUI:PlayerHasBeenDefeated): end = now+15*CLIENT_FPS or now+sec*CLIENT_FPS, fadeStart = end-CLIENT_FPS, per-line start += CLIENT_FPS | ui | event | **client_scale** | high | 0x8ea686 CLIENT_FPS_INT_30; 0x8ea697 CLIENT_FPS_INT_30; 0x8ea896 CLIENT_FPS_INT_30 --  |
| 008ee765 | SkirmishAI tactic: (logicFrame-last) vs LTR*data seconds | logic | per_logic_tick | **keep** | high | refs 0x8ee7c0/0x8ee82f |
| 008f0502 | AISkirmishPlayer: next = now + 2*LTR | logic | per_logic_tick | **keep** | high | ref 0x8f05ef |
| 008f0783 | AISkirmishPlayer ctor: +0x158 = 30*LTR | logic | once | **keep** | high | ref 0x8f0855 |
| 008f1725 | AI: LTR * TheAI data seconds (0xde4938+0x980) vs frame count | logic | per_logic_tick | **keep** | high | ref 0x8f173c |
| 008f2a12 | frames(+0x74)/LTR -> seconds for score screen h:mm:ss (0x9cd79a) | ui | event | **keep** | high | ref 0x8f2a17; logic-time display |
| 008f3efe | AI team countdowns: clamp 3*LTR, reset 2*LTR | logic | per_logic_tick | **keep** | high | refs 0x8f3f25/0x8f3f51 |
| 008f457d | (mis-bounded) ref lies in AISkirmishPlayer update 0x8f47eb: next = now + 5*LTR | logic | per_logic_tick | **keep** | high | ref 0x8f4800 |
| 008f7949 | AI: t + 60*LTR < now | logic | per_logic_tick | **keep** | high | ref 0x8f796e |
| 008f805d | AI team: frames = data*LTR | logic | per_logic_tick | **keep** | high | ref 0x8f8223 |
| 008f8f68 | AI team: countdown reload = LTR | logic | per_logic_tick | **keep** | high | ref 0x8f8f89 |
| 008f96b6 | AI team: countdown reload = 5*LTR | logic | per_logic_tick | **keep** | high | ref 0x8f96f7 |
| 008fa451 | AI/dock helper: now <= t + 4*LTR | logic | per_logic_tick | **keep** | high | ref 0x8fa45e |
| 0090b1f4 | AITargetChooser: random chance every LTR*data seconds | logic | per_logic_tick | **keep** | high | ref 0x90b23d |
| 00910070 | logic contain/collide test: template speed*0.2 s per logic frame | logic | per_logic_tick | **keep** | high | ref 0x9100f3 |
| 009257f2 | Score screen timeline graph: logic frames / LTR -> seconds axis labels | ui | event | **keep** | high | ref 0x925c22; logic-time display |
| 0092ba91 | InGameUI selection-cycle timeout: deadline = GameClient::getFrame + InGameUI[+0x988] ms*0.03, checked in 0x92d9f4 | ui | event | **client_scale** | high | 0x92bab3 F_0.03_client_per_ms -- purpose medium |
| 00944cc0 | ControlBar OCL timer text: (OCLUpdate end logic frame - now)/LTR seconds | ui | per_client_frame | **keep** | high | ref 0x944d21; logic-time display, rate independent |
| 0094a048 | AI state: +0x44 = now + 3*LTR; now + LTR | logic | per_logic_tick | **keep** | high | refs 0x94a154/0x94a26e |
| 0094a986 | AI state timeout 30*LTR | logic | per_logic_tick | **keep** | high | ref 0x94aa68 |
| 0094c310 | AI state: now + 4*LTR | logic | per_logic_tick | **keep** | high | ref 0x94c3fc |
| 0097d035 | AITargetHeuristicExpansion: counter vs INIT_ltr 0xdebd74 (60*LTR) | logic | per_logic_tick | **keep** | high | ref 0x97d07c |
| 0097dd6b | (mis-bounded) SkirmishAI: +0x58 = now + LTR*30.0 | logic | per_logic_tick | **keep** | high | ref 0x97ddf4 |
| 0098bdde | HordeContain formation helper: wait 4*LTR / 2*LTR | logic | per_logic_tick | **keep** | high | refs 0x98c35e/0x98c367 |
| 0098c6a8 | HordeContain update: t + LTR < now | logic | per_logic_tick | **keep** | high | ref 0x98c949 |
| 0098d478 | HordeContain helper: wait 4*LTR / 2*LTR | logic | per_logic_tick | **keep** | high | refs 0x98da69/0x98da72 |
| 0098e3e7 | (mis-bounded) HordeContain logic: now + LTR, t + LTR compare | logic | per_logic_tick | **keep** | high | refs 0x98e8e6/0x98ea34 |
| 0098f780 | Horde formation data ctor (parse 0x86c30a): 2*LTR, 2*LTR, 3*LTR | logic | once | **keep** | high | refs 0x98f7d5/7df/7e9 |
| 00993019 | AISpecialPower base (AISPecialPower.cpp): ref is adjacent virtual getUpdatePeriod()=5*LTR (slot 3 of ~56 vtables); 0x99302d compares tick counter | logic | per_logic_tick | **keep** | high | ref 0x993024 |
| 0099eac1 | InGameUI text effect ctor ("SachaWynter"): +0x2c = CLIENT_FPS/4+1 = per-line duration/stagger (slot-4 getter added to running start frame at 0x8e9d55) | ui | event | **client_scale** | medium | 0x99eafc CLIENT_FPS_INT_30 --  |
| 0099eb97 | Text effect init ("TextElvenClouds"): phase windows CLIENT_FPS, CLIENT_FPS+1, CLIENT_FPS/2+CLIENT_FPS+1 vs elapsed GameClient frames (draw 0x99ecec) | ui | event | **client_scale** | high | 0x99ec8e CLIENT_FPS_INT_30; 0x99eca0 CLIENT_FPS_INT_30 -- see extra finding text_effect_anim for hard-coded >>2 |
| 0099f313 | AI money accumulation per logic frame (0.2 s) | logic | per_logic_tick | **keep** | high | ref 0x99f365 |
| 0099faef | AIUpgradeScienceBuilder: now-last >= INIT_ltr 0xdebe48 (60*LTR) | logic | per_logic_tick | **keep** | high | ref 0x99fb09 |
| 009a24fc | AIUnitUpgrader: now-t >= INIT_ltr 0xdebe5c (10*LTR) | logic | per_logic_tick | **keep** | high | ref 0x9a2610 |
| 009a3040 | AIUnitUpgrader: now-t >= 10*LTR | logic | per_logic_tick | **keep** | high | ref 0x9a3115 |
| 009b53c6 | AI tactic: next = now + 2*LTR | logic | per_logic_tick | **keep** | high | ref 0x9b5483 |
| 009b565f | AI tactic: now + INIT_ltr 0xdebe8c (30*LTR) | logic | per_logic_tick | **keep** | high | ref 0x9b578c |
| 009b583c | AI tactic: now + INIT_ltr 0xdebe90 (10*LTR) | logic | per_logic_tick | **keep** | high | ref 0x9b5870 |
| 009b67ca | (mis-bounded) AI tactic method 0x9b6bd0: now + INIT_ltr 0xdebe9c (15*LTR) | logic | per_logic_tick | **keep** | high | ref 0x9b6bd8 |
| 009b6f70 | AI tactic: now + 15*LTR | logic | per_logic_tick | **keep** | high | ref 0x9b7046 |
| 009b7f56 | AIRoamingDefenseTactic NextLogic = now + 5*LTR | logic | per_logic_tick | **keep** | high | ref 0x9b7fee |
| 009b8e1e | AI tactic: now + INIT_ltr 0xdebec0 (30*LTR) | logic | per_logic_tick | **keep** | high | ref 0x9b8f3d |
| 009b9056 | AI tactic: now + INIT_ltr 0xdebecc (5*LTR) | logic | per_logic_tick | **keep** | high | ref 0x9b9087 |
| 009b9e06 | AI tactic: now + 2*LTR | logic | per_logic_tick | **keep** | high | ref 0x9b9ead |
| 009ba3c2 | AI tactic: now + 2*LTR | logic | per_logic_tick | **keep** | high | ref 0x9ba469 |
| 009ba4ef | AI tactic: now + INIT_ltr 0xdebed8 (30*LTR) | logic | per_logic_tick | **keep** | high | ref 0x9ba60e |
| 009ba655 | AI tactic: now + INIT_ltr 0xdebee4 (5*LTR) | logic | per_logic_tick | **keep** | high | ref 0x9ba686 |
| 009baf6c | AI tactic blackboard timers 5*LTR | logic | per_logic_tick | **keep** | high | refs 0x9baff8/0x9bb07c |
| 009bb1c4 | AIFarmKillSquad: next = now + GameLogicRandomValue(10*LTR, 60*LTR) | logic | per_logic_tick | **keep** | high | refs 0x9bb25e/0x9bb258 |
| 009bbfd1 | (mis-bounded ctor) AI tactic method: now + GameLogicRandomValue(20*LTR, 90*LTR) | logic | per_logic_tick | **keep** | high | refs 0x9bc014/0x9bc00e |
| 009c0e74 | CreateAHero APT screen update (shell; via 0x919c26): counter +0x194 ++ per screen update while async loader idle; after LTR+2 (=7) updates triggers hero preview step | ui | per_client_frame | **uncertain** | medium | 0x9c0f00 LTR_INT_5 -- ref 0x9c0f00: LTR used as a plain UI frame-count delay (233 ms at 30 FPS, 117 ms at 60). Harmless; for exact wall-clock patch the compare to 2*LTR+4 (code patch, not the LTR global). Per-rendered-frame cadence inferred |
| 009ce8f3 | Post-game stats builder (via 0x9270ad): frames/LTR -> seconds | ui | event | **keep** | high | ref 0x9ceea1 |
| 009cfa65 | Post-game results: LTR*GlobalData.SecondsBeforeBaseCheckActive+LTR vs VictoryConditions frame; logicFrame/LTR seconds | ui | event | **keep** | high | refs 0x9cfd65/0x9cfd9f/0x9d044a |
| 009e7ca3 | AISpellBookShroudReveal: now > INIT_ltr 0xdec148 (10*LTR) | logic | per_logic_tick | **keep** | high | ref 0x9e7cb8 |
| 009e7f0f | AI spellbook power: stuck counter per AI tick vs 3*LTR | logic | per_logic_tick | **keep** | high | refs 0x9e82af/0x9e833a |
| 009e880b | AISpellBookAssistBattle: GameLogicRandomValue(INIT_ltr 0xdec14c=3*LTR, 0xdec150=10*LTR) | logic | per_logic_tick | **keep** | high | refs 0x9e888d/0x9e8887 |
| 009e8a42 | AI spellbook power: counter per AI tick vs 3*LTR | logic | per_logic_tick | **keep** | high | refs 0x9e8f50/0x9e9034 |
| 009e9222 | AISpellBookArmyBreaker: GameLogicRandomValue(15*LTR, 30*LTR) | logic | per_logic_tick | **keep** | high | refs 0x9e92a4/0x9e929e |
| 009ed351 | (mis-bounded ctor) AI method 0x9ed374: next = now + LTR*(5*(7-difficulty)[-5]) | logic | per_logic_tick | **keep** | high | ref 0x9ed3c8 |

## Extra findings (per-client-frame systems seen while classifying)

1. **GameClient frame counter** (client clock). GameClient+0x10, getFrame = vtable+0x7c (0x9f9f1a), setFrame = vtable+0x38 (0x993d2e). Incremented once per rendered frame by GameEngine vtable+0x9c = 0x632409 (`mov ecx,[TheGameClient]; call [esi+0x7c]; inc eax; push eax; call [esi+0x38]` at 0x632433..0x632440, gated by GameClient+0xc8 which the stepper sets when not paused). At 60 FPS it counts 60/s, so every getFrame()-based duration (Drawable fades, InGameUI messages, selection-cycle timeout 0x92ba91, text effects, radar events) halves in wall-clock unless made 60-based. Option A: make every duration 60-based. Option B: increment only on even rendered frames (counter stays 30 Hz) - fixes all consumers at once but quantises them to 30 Hz and must be checked against other getFrame users (e.g. W3D sync gate 0x44b788 uses getFrame deltas and `%0x1e`).
2. **Drawable opacity fade** (0x670a50 fadeOut, 0x670aa2 fadeIn, 0x67309d fade-chain; updater FUN_00675996 0x675ac3-0x675b66: `delta=getFrame()-[+0x37c]; elapsed=min(elapsed+delta,[+0x130]); opacity=elapsed/dur`). ~37 call sites, many hard-coded in client frames: 10 (0x79b05e, 0x79b09d, 0x85832e, 0x8aee86, 0x8b1513, 0x8b2392), 30 (0x8583c9), 0x8a (0x7954a8), LTR pushed as frames (0x664ff2, 0x665031), GlobalData BuilderFadeInTime/BuilderFadeOutTime (0x88d79e, 0x88ddd4), template fields * 0.03/FPS30 (0x8a2d40, 0x8a6080, 0x8a60c0, 0x8b864c, 0x862c2e, ...). Central fix: store 2*duration at 0x670a87, 0x670ad9, 0x6730b2 (each `89 86 30 01 00 00`, needs a code cave) - then do NOT redirect the per-caller refs in this chunk. Alternative: feed the four getFrame calls (0x670a95, 0x670ae7, 0x6730d4, 0x675acb) with getFrame()>>1.
3. **Builder fade durations parsed with the logic parser**: GlobalData BuilderFadeInTime (+0x11e0) / BuilderFadeOutTime (+0x11dc) use parser 0x73a429 (ms*0.005 -> logic frames) but are consumed as GameClient-frame fade lengths by DozerActionDoActionState (0x88d79e fadeIn, 0x88ddd4 fadeOut). Stock quirk (fade 6x shorter than the INI ms); at 60 FPS 2x shorter again. Covered by the central fade fix; otherwise double at the two consumer pushes. Never change 0x73a429 / 0xD9F610 (shared logic parser).
4. **Text-effect image animation** in 0x99ecec: frame = ((elapsedClientFrames - phaseStart) >> 2) % numFrames at 0x99ee31 (`c1 e8 02`). At 60 FPS the animation runs 2x. Fix: byte 0x99ee33 02 -> 03.
5. **SlavedUpdate welding sparks** lifetime = framesToWait*LTR (0x8a5a5b) in particle ticks; depends on the particle tick strategy (keep if particles stay on 30 Hz ticks, x2 if they tick every 60 Hz frame).
6. AotR note: AotR detours ProductionUpdate-area code at 0x8a144d (FUN_008a13ee -> 0xed1a00), not the timing site 0x8a2d40 in ProductionUpdate::update. No timing ref in this chunk lies in AotR code.
