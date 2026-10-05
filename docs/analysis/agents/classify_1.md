# Timing-ref classification — chunk 1 of 4 (101 functions)

Verdicts: **keep** = must keep the 30/5-based value (logic or rate-independent); **SCALE** = client per-frame use, needs the 60 FPS value (ref listed); **?** = uncertain.

## Key facts verified while doing this chunk
- **TheGameClient = DAT_00de4388** (string "TheGameClient" 0xbfed74 pushed with 0xde4388 at 0x63bdeb). W3DGameClient vtable 0xbda6e0: **vt+0x7c = getFrame()** (`mov eax,[ecx+0x10]; ret` at 0x9f9f1a), **vt+0x38 = setFrame()** (0x993d2e).
- **Client frame counter advances once per rendered frame**: FUN_00632409 (GameEngine vt+0x9c, called at the top of frame stepper 006325a0): `if (client+0xc8) setFrame(getFrame()+1)` (0x632435 CALL [ESI+0x7c]; 0x63243e INC EAX; 0x632440 CALL [ESI+0x38]). So every `getFrame()` delta below is in **client frames**: 30/s now, 60/s at 60 FPS.
- TheGameLogic = DAT_00de412c (+0x40 = logic frame). GameLogic vt+0x34 = FUN_0062e4e8 = GameLogic::update(sub). The GameEngine sub-frame update FUN_006329b0 (GameEngine vt+0x98) calls it for **every** client sub-frame (0x632a82).
- TheTerrainLogic = DAT_00de4690. TerrainLogic subsystem vtable 0xc113bc slot 0x28 = 0067d08d. It is updated from GameLogic::update only when sub==1.
- TheInGameUI = DAT_00de4830 (base vtable 0xc134e8). vt+0x28 = 006a1f4d (update, per client frame). vt+0xd4 = 006a5723. vt+0xd8 = 0069e5bb. vt+0x150 = 006a3b59 → 0069dea9 and vt+0x154 = 006a0bc0, both called from W3DInGameUI draw 0x48ea29 on every rendered frame. FUN_006a536c is called from the display draw FUN_00449cf8 on every rendered frame, including while paused.
- Palantir HUD object = DAT_00de4a70 (created 0x48ecb8). Update chain: InGameUI::update → vt+0x28 → 0x6d7b33 → 0x6d7647 → 0x6d577c. **AotR hook inside 0x6d577c at 0x6d57af** (JMP 0xed0b00 .danetta → returns 0x6d57b4). Do not patch those bytes.
- The INIT globals (from timing_initializers.json): 0xde45d8 = FPS/2 = 15; 0xde45f0 = 3·LTR = 15; **0xde4848 = LTR = 5, used by both a logic-frame site (0069df5e) and a client-frame site (006a5449)**, so fix it per instruction and do not change the global; 0xde4a0c = 4·LTR; 0xde784c = 2·LTR; 0xde820c = 3·LTR as a float.
- Shared INI parsers: 0x73a403 (parseDurationReal), 0x73a429 (parseDurationUnsignedInt, about 250 fields), 0x73a46f (only Anim2D AnimationDelay), 0x73b071 (only LargeGroupAudio HandOffModeDuration) and 0x73a4b6 (parseVelocity ×0.2, e.g. FloatingTextMoveUpSpeed/VanishRate). All of them use the logic factors (0.005, 0.2).

## Table
| entry | what / where | domain | cadence | verdict | refs to redirect / note |
|---|---|---|---|---|---|
| 0066b3fd | AI unit: blocked → FUN_0074168e(1, LTR*20) state timeout | logic | logic tick | keep | |
| 0066c748 | AIUpdate sub-update returns LTR as sleep frames | logic | logic tick | keep | |
| 0066d16e | AI attack/hunt: frame%LTR phasing, logicFrame+2·LTR | logic | logic tick | keep | |
| 0066da5f | AI move state: logicFrame+2·LTR, timeout LTR*10 | logic | logic tick | keep | |
| 0066e8ff | AIUpdateModuleData ctor: field = 2·LTR | logic/init | once | keep | |
| 0066f1a8 | build-time calc LTR·sec·(1-x) (caller 008a04da) | logic | event | keep | |
| 00673114 | Drawable opacity pulse (from updateDrawable): total=FPS·sec, counter--, phase+=0.0333/period·k | client | client frame | **SCALE** | 006731ca, 00673222, 00673289 |
| 006732dc | Drawable client physics type 9 (via 0067bdc0, once per getFrame): v·(LTR/FPS) per-frame | render | client frame | **SCALE** | 00673494 (FPS). LTR 0067349c keep. The spring-damper steps are also per frame (X5). If physics is gated to 30 Hz instead, keep 30 here. |
| 006740f5 | drawable icon shown if logicFrame-stamp ≤ INIT 0xde45f0 | client | client frame | keep | logic-frame compare |
| 00675996 | Drawable::updateDrawable: flash when getFrame()%INIT_half==0 | client | client frame | **SCALE** | 00675d5b (0xde45d8 15→30). Hard-coded envelopes in X3/X4 |
| 006760f9 | Drawable pulse setter: [0xd4]=ftol(FPS·sec) | client | event | **SCALE** | 00676151 |
| 0067d08d | TerrainLogic::update (sub==1): height-change step, frame%LTR | logic | logic tick | keep | |
| 0067dbf7 | TerrainLogic add request: step=(Δ)/(LTR·dur) | logic | event | keep | |
| 0068c7c1 | field = logicFrame + (ms/1000)·LTR | logic | event | keep | |
| 0068c89b | predicate x+2·LTR ≤ logicFrame | logic | event | keep | |
| 0068c91b | field = logicFrame+10·LTR | logic | event | keep | |
| 0068c933 | LTR·sec vs logic frames | logic | event | keep | |
| 0068d440 | FUN_008e3b71(logicFrame+2·LTR) | logic | event | keep | |
| 006903b6 | FUN_008e2c0f(status, 3·LTR) = status until logicFrame+N | logic | event | keep | |
| 0069068d | FUN_008e2c0f(0x68, 3·LTR) | logic | event | keep | |
| 00691059 | set status; FUN_008e2c0f(cond, LTR) | logic | event | keep | |
| 00691106 | clear status; same | logic | event | keep | |
| 0069320d | FUN_005e39e6(v, LTR): [0x60]=logicFrame+LTR | logic | event | keep | |
| 00696800 | same | logic | event | keep | |
| 0069dea9 | InGameUI postDraw (each rendered frame): world-text y += speed/LTR | ui | client frame | **SCALE** | 0069df0e (divisor 5→10). 0069df5e (INIT 0xde4848 vs logic frames) keep |
| 0069e5bb | InGameUI vt+0xd8: [0x16b]=getFrame()+1.5·FPS | ui | event | **SCALE** | 0069e5e3 |
| 006a0bc0 | InGameUI vt+0x154 draw: timer text (Δlogic)/LTR, Δ·0.2 | ui | client frame | keep | logic frames → seconds |
| 006a1770 | refs are in the next, unsplit function 0x6a1778 = addFloatingText: expire=getFrame()+FPS/3 or +0.03·TimeOut(ms) | ui | event | **SCALE** | 006a181a, 006a183f |
| 006a1875 | add world-text (+0x8cc): expire=logicFrame+LTR·sec | ui | event | keep | |
| 006a1978 | add UI particle (+0x8d0): expire=**getFrame()**+LTR·sec | ui | event | **SCALE** | 006a1a32 (5→10) |
| 006a1f4d | InGameUI::update: getFrame()%FPS==0 → once-per-second check | ui | client frame | **SCALE** | 006a2456. LTR 006a1fb5 keep |
| 006a536c | UI-particle update (each rendered frame): fade window INIT 0xde4848 vs client frames | ui | client frame | **SCALE** | 006a5449 (5→10, per instruction). Integrator: X7 |
| 006a5723 | InGameUI vt+0xd4: hint fade 1-Δ/(FPS·0.5), Δ/FPS vs sec | ui | client frame | **SCALE** | 006a5795, 006a5901 |
| 006a6aad | InGameUI ctor: FloatingTextTimeOut default (float)(FPS/3) | ui/init | once | keep | the consumer treats it as ms (×0.03) and INI overrides it, so keep 10 |
| 006a9807 | AI data ctor (LogicFramesTill… = LTR·480/60/180) | logic | once | keep | |
| 006af269 | player update: frame%LTR==0 → send msg 0x462 | logic | logic tick | keep | |
| 006b3360 | LivingWorld: [0x16c]=4·FPS countdown (dec in 006b5929) | client | event | **SCALE** | 006b336e |
| 006b3fd7 | LivingWorld: [0xe4]=FPS countdown | client | event | **SCALE** | 006b3fe3 |
| 006b4779 | X·LTR (sec→logic frames) | logic | event | keep | |
| 006b47c8 | ceil(frames/LTR) → seconds | logic | event | keep | |
| 006bf96d | LivingWorld camera scroll time → frames=FPS·sec | client | event | **SCALE** | 006bfa5e. 006bfa17 (0.0333 converts zoom-ticks → sec) keep |
| 006c7344 | list prune: Δlogic < INIT 0xde4a0c | logic | logic tick | keep | |
| 006c7677 | progress = Δlogic·0.2 / sec | logic | per update | keep | |
| 006c9d99 | Weapon INI DelayBetweenShots ms→logic frames | ini_parse | once | keep | |
| 006c9e7a | Weapon INI ClipReloadTime | ini_parse | once | keep | |
| 006d3dac | Palantir count-up anim: delay=5·FPS (dec per update) | ui | event | **SCALE** | 006d3dae |
| 006d3dce | Palantir anim: delay=6·FPS | ui | event | **SCALE** | 006d3dd6 |
| 006d455f | Palantir timer[+0x12c]=max(cur,FPS·n); dec at 0x6d7304 | ui | event | **SCALE** | 006d456c |
| 006d47a5 | Palantir/LivingWorld entry frames=FPS·sec | ui | event | **SCALE** | 006d47b4 |
| 006d8e0f | ref is in the unsplit function 0x6d8e2b (Radar update): Δlogic > 3·LTR → refresh | client | client frame | keep | logic-frame compare |
| 006d93eb | Radar::createEvent: die=getFrame+FPS·s, fade=die-FPS·0.5 | client | event | **SCALE** | 006d946f, 006d94a8 |
| 006d9b4d | Radar::tryEvent: dedupe Δclient < FPS·10 | client | event | **SCALE** | 006d9b75 |
| 006f0fd5 | debug path draw: LTR·3 passed to empty stub 0x63f3bf | logic(debug) | – | keep | no effect |
| 006f2364 | path-request servicing with budget (×100 while frame<5·LTR) | logic | **every sub-frame** | keep | per-call budget, see X2 |
| 006ffbef | TAiData ctor (LTR/2, LTR) | logic | once | keep | |
| 0071c3a1 | control-bar button blink frame%LTR ≤ LTR/2 | ui | client frame | keep | logic-frame blink |
| 00732926 | per-draw pulse: getFrame()%ceil(0.03·ms); rot=getFrame()·0.0333·k | render | client frame | **SCALE** | 0073297a, 00732b4f |
| 0073a403 | INI parseDurationReal (0.005), shared | ini_parse | once | keep | handle client consumers at the use site |
| 0073a429 | INI parseDurationUnsignedInt (0.005), about 250 fields | ini_parse | once | keep | same |
| 0073a46f | INI AnimationDelay (only Anim2DTemplate), compared with getFrame() | ini_parse→client | once | **SCALE** | 0073a496: needs 2·ceil(ms·0.005) (see X8) |
| 0073a74b | INI Envelope (Attack/Decay/Sustain/Release/InitialDelay) for W3D laser draw +0x4c and block 0xc08668 | ini_parse | once | ? | consumer not found |
| 0073b071 | INI HandOffModeDuration (LargeGroupAudio) | ini_parse | once | ? | consumer not found |
| 0073c39e | build time LTR·sec | logic | event | keep | |
| 0073fb7a | AI data ctor defaults LTR·30/10/2 or GlobalData | logic | once | keep | |
| 0074168e | AI setState timeout logicFrame+N (cap LTR·60) | logic | event | keep | |
| 00741e38 | ref is in the next unsplit AI function: path recompute throttle Δlogic<LTR | logic | logic tick | keep | |
| 00742271 | AI path recompute throttle | logic | logic tick | keep | |
| 00743bd2 | AI onEnter rand(0,3·LTR) | logic | event | keep | |
| 00743d0f | AI onEnter rand(0,LTR) | logic | event | keep | |
| 0074697a | AI state logicFrame+3·LTR | logic | logic tick | keep | |
| 00746a99 | AI state logicFrame+2+LTR | logic | event | keep | |
| 00746db6 | AI throttle | logic | logic tick | keep | |
| 007471cf | AI state | logic | logic tick | keep | |
| 00747ef4 | AI state | logic | logic tick | keep | |
| 007489b6 | AIIdleState onEnter rand(0,INIT 0xde784c) | logic | event | keep | |
| 00748e46 | AI move stuck check | logic | logic tick | keep | |
| 00749a3e | AI | logic | logic tick | keep | |
| 00749d46 | AI | logic | logic tick | keep | |
| 0074a594 | AI | logic | logic tick | keep | |
| 0074acad | AI | logic | logic tick | keep | |
| 0074b1d6 | AI | logic | logic tick | keep | |
| 0074d46d | AI Δ<LTR/4 | logic | logic tick | keep | |
| 0074e6ce | AI | logic | logic tick | keep | |
| 0074eb5c | AI | logic | logic tick | keep | |
| 00751f01 | AIAttackFireDuringApproach throttle | logic | logic tick | keep | |
| 007553ab | AI idle sleep INIT 0xde784c | logic | logic tick | keep | |
| 00755798 | AI | logic | logic tick | keep | |
| 00759a6c | script wait: logicFrame+audioLen/200 | logic | event | keep | |
| 00759b84 | same | logic | event | keep | |
| 007651b1 | client display element: life=getFrame()+0.03·ms | client | event | **SCALE** | 0076523b |
| 0076cb71 | logic module update: t += 1/LTR | logic | logic tick | keep | |
| 00771f49 | rand(0,LTR)+logicFrame | logic | event | keep | |
| 00774897 | same | logic | event | keep | |
| 007779e3 | passes (logic count·0.2) seconds to 006760f9 | client | event | keep | rate-independent |
| 00779a3d | command/player processing (sub==1) | logic | logic tick | keep | |
| 00792559 | module-data ctor LTR,2·LTR,LTR | logic | once | keep | |
| 00792dbd | toss solver: simulates ≤3·LTR logic frames | logic | event | keep | |
| 007966bb | setAnimationLoopDuration(INIT 0xde820c·0.5 = 7) → W3DModelDraw slot 0x64 ×200 ms | client | event | keep | rate-independent |
| 007987ee | camp: FPS·sec → Drawable::fadeIn **and** status timer (logic) | mixed | event | keep | the ref feeds logic; fade fix in X10 |
| 00799acb | logicFrame < LTR·x | logic | logic tick | keep | |
| 0079cb47 | camp capture: Drawable::fadeOut(FPS·sec) for all objects | client | event | **SCALE** | 0079cb70 |

## Hard-coded per-frame systems found (details in the structured output, extra_findings)
- X1: the GameClient frame counter is the time base of every SCALE item above.
- X2: GameLogic::update(sub) splits logic work over sub 2..6, and FUN_006f2364 services path requests on every call. Logic must still be called exactly 6×/tick with sub 1..6.
- X3: Drawable TintEnvelope plays with hard-coded 30/30-frame attack and decay (0x675dc8…). X4: Drawable per-frame fade/opacity integrators.
- X5: Drawable client physics (0067bdc0 dispatcher) runs once per client frame. X6: floating-text age and vanish advance per client frame.
- X7: UI particles (006a536c) integrate per rendered frame. X8: Anim2D frame advance uses client frames. X9: Palantir count-up animation (AotR hook inside).
- X10: 007987ee mixes a logic and a client use of the same value. X11: LivingWorld countdown cadence.
