# classify_2 (chunk 2 of 4) - timing-global reference classification

Status: analysis complete (all 101 functions of an/chunks/chunk_2.json read). Plan mode became active
before an/findings/classify_2.md could be written; this file holds the same table. Pending action once
execution is allowed: copy this table verbatim to
`an\findings\classify_2.md` (analysis workspace)
(and optionally delete the scratch helpers in an/findings/tmp2/).

Key identities verified: DAT_00de412c=TheGameLogic (+0x40 logic frame); DAT_00de4388=TheGameClient
(vt+0x7c -> 0x9f9f1a `mov eax,[ecx+0x10]` = GameClient frame); DAT_00de4324=TheGameEngine (GameMain 0x6443b0;
+0x38 = client frames per logic frame, set by frame stepper); DAT_00de4418=TheDisplay (W3DDisplay vtable 0xbd9c28);
DAT_00de4958=Living-World camera/view (vtable 0xbde918; vt+0x9c=0x6bfb88 moveTo(pos,angle,zoom,frames));
DAT_00dd1e0c=WW3D::SyncTime (written only by FUN_00516e20 = WW3D::Sync); DAT_00de4364=GameData.
Drawable fade elapsed is measured in GameClient frames (0x675ac3..0x675aff: elapsed += clientFrameNow - [+0x37c]).

| entry | ref(s) | what | domain/cadence | verdict |
|---|---|---|---|---|
| 0079ccf2 | 79cf57,79d035,79d0d4 | refs lie in undefined update fn 0x79cf2a (vtable 0xc30e08), castle/camp: timer -= 1/LTR s per update, returns sleep LTR | logic/tick | keep |
| 0079dc0c | 79dc20 | frame/LTR = game seconds (stats UI 0x9cdec1, minutes 0x79df02) | logic->ui/event | keep |
| 0079dc27 | 79dc2f | [+0xf4]/LTR seconds | logic/event | keep |
| 0079fe73 | 79fe88 | deadline = s*LTR + logicFrame | logic/event | keep |
| 007a267d | 7a283e | team generic-script re-run delay LTR*s + logicFrame | logic/tick | keep |
| 007a2c96 | 7a3734 | TeamTemplate from map Dict: teamInitialIdleSeconds*LTR | logic/init | keep |
| 007a3c49 | 7a3c9c | script delay LTR*s + logicFrame | logic/tick | keep |
| 007b10b4 | 7b1102,7b1160 | FXParticleSystem storage: birth/expiry time = SyncTime(ms)*30*0.001 (+lifetime) | client/frame | keep (ms->30Hz-unit factor; needs W3D clock in real ms) |
| 007b12f4 | 7b12fe | FX particle expiry test vs SyncTime*30/1000 | client/frame | keep (same) |
| 007b13a5 | 7b140b | FX particle emit, birth time SyncTime*30/1000 | client/frame | keep (same) |
| 007b1f5b | 7b1fb5 | SpecialPowerTemplate ctor default 10*LTR | logic/init | keep |
| 007bbfb2 | 7bbfc8 | doCameoFlash count = 30*s/10 (even); consumers %10 at 0x71fd68,0x9314e9 | client/frame | keep + patch consumers 10->20 (else client_scale) |
| 007bc013 | 7bc03e, 7bc049(0xde87dc) | doNamedFlash count = LTR*s/15; consumer 0x675d5b uses frame % [0xde45d8] | client/frame | keep both (0xde45d8 must become 30, 0xde87dc must stay 15) |
| 007bd477 | 7bd495 | drawable icon expiry = logicFrame + LTR*s (FUN_006739e0) | client, logic-timed | keep |
| 007bd582 | 7bd59a | TheGameLogic+0xa0 = LTR*s | logic/event | keep |
| 007bdd7a | 7bdd7a | hero-bar per-hero flash frames = 30*s; decremented per ControlBar update (FUN_0092cf64) | ui/frame | client_scale |
| 007bdd97 | 7bdd97 | hero-bar flash frames = 30*s; decremented per update | ui/frame | client_scale |
| 007c083e | 7c098b | TheDisplay->setCinematicTextFrames(LTR*s) (+0xf8, no reader found) | ui/event (dead) | keep |
| 007c09e3 | 7c0a40, 7c0a4a(0xde87dc) | doTeamFlash (as 7bc013) | client/frame | keep both |
| 007c1704 | 7c1766 | team script wait frames | logic | keep |
| 007c299d | 7c29dd | team icon expiry (logic frames) | client, logic-timed | keep |
| 007c495b/49bc/4a34 | 7c499c/7c4a14/7c4a79 | special-power timers logicFrame+LTR*s | logic | keep |
| 007c6404/649a | 7c645c/7c6502 | model condition for LTR*s (FUN_008e2c0f expiry=logicFrame+n) | logic | keep |
| 007c8e49/9d7d/9df6/9e48 | 7c8e9e/7c9ddc/7c9e2d/7c9ecf | unit/team script wait LTR*n | logic | keep |
| 007cafa5 | 7cdf02 | ScriptActions dispatcher: ceil(LTR*s) team wait | logic | keep |
| 007e4866/48ed/5640/6017/64cd/8954/9c33, 007eaf48/b046 | (see chunk) | ScriptConditions (logic-frame windows, throttles, 0.005*ms) | logic | keep |
| 007ee166 | 7ee17f | (logicFrame-last)*LTR >= 3.0 refresh throttle | logic | keep |
| 007ef198 | 7ef24b | LargeGroupAudio Sound default MaximumAudioSpeed=20*0.2 (parser 0x73a4b6 x0.2) | logic data/init | keep |
| 007fa0a6 | 7fa501 | LW tutorial/session task wait = s*5.0 + [DAT_00de4950+0x100] | logic(LW)/event | keep (medium) |
| 007fb29c | 7fb2ce,7fb334 | LivingWorldEyeTower lerp step 1/((rand+4)*30) per LW-manager update | client/frame | client_scale |
| 007fea48 | 7fea4b | ms*30/1000 -> client frames for LW control-point actions (camera moveTo frames) | client/frame | client_scale |
| 008050d3 | 805192 | AttributeModifier "Upgrade ... Delay:ms" x0.005 | logic/ini | keep |
| 00808f53 | 808f81 | VictoryConditions grace LTR*GameData[0x110c] | logic | keep |
| 0080ace3 | 80b08e | anim length (30 fps data)*0.0333*LTR -> model-condition logic frames | logic | keep |
| 0080f475/0080fe19 | 80f47f/80fe33 | LW logic timer duration s*5.0 vs logic frame | logic | keep |
| 00819e12/0081a0a7/0081a11e | 819f0e/81a0b2/81a16d,81a1be | stats dump (logic frames<->s) | logic/debug | keep |
| 0081aa85 | 81aadc | GameData[0x11d8] frames / LTR -> seconds into Drawable FUN_006760f9 | client, units only | keep |
| 0081be85 | 81bea6 | periodic every LTR logic frames | logic | keep |
| 00821319 | 8213cb | model condition 0xd2 for 3*LTR | logic | keep |
| 008251fa | 82522b | ScoredKillEvaAnnouncer default MaximumTimeForAnnouncementMS = LTR (parser x0.005) | logic/init | keep |
| 00838d3a | 838d60 | LW camera moveTo(..., frames=30/4) | client/event->frame | client_scale |
| 0083ab1f | 83ab37 | UI cooldown in logic frames (>= LTR) | ui, logic-timed | keep |
| 0083c29e | 83ce55,83cfe2 | CommandTranslator control-group double-tap window < LTR logic frames | ui, logic-timed | keep |
| 0085123f..0085df88 (85123f,85266d,8530ba,85393b,855533,8558c0,8574ae,857e77,85c177,85d216,85d9d1,85df88) | | update-module sleeps / logic timers / INIT_ltr model conditions | logic | keep |
| 0085ebee | 85eed9,85eef5 | die/behavior drawable fade (FUN_0067309d) frames = ms*0.03 | client/frame | client_scale |
| 00860e93 | 860fe0,860fee | SlowDeathBehavior LTR/2+1, LTR+1 | logic | keep |
| 00862b07 | 862c2e | drawable fadeIn frames = ms*0.03 | client/frame | client_scale |
| 008633a4 | 8634ed | update counter reload INIT_ltr | logic | keep |
| 0086404d,008682d9,00868759 | | logicFrame + LTR(*s) AI timers | logic | keep |
| 0086a943 | 86ab63,86aba1 | contain enter fade frames = ms*0.03 | client/frame | client_scale |
| 0086abdc | 86ae59,86ae87 | contain exit fadeIn frames = ms*0.03 | client/frame | client_scale |
| 0086b86d | 86b91a | contain heal x0.2 per logic frame | logic | keep |
| 0086bdd3 | 86beab | (undefined fn 0x86be9a) +0x170 = logicFrame+3*LTR | logic | keep |
| 0086d259,008719e4,00872efc,00878ee5,0087c8df | | AI timers/sleeps/ctor defaults | logic | keep |
| 0087d649 | 87d65c | drawable fadeIn(LTR*5 = 25 client frames) | client/frame | client_scale (needs x2: imm 5->10 at 0x87d661) |
| 0087d9f6,0087ee53,0087f7b4,0087ffc7,008807ca,00881298,00882cb2,00884cf1,008868a6,00887458,00887bab | | AI/heal/model-condition/module defaults in logic frames | logic | keep |
| 00889aaf | 889c3d | die-module drawable fadeIn(30*FadeInTimeSeconds) | client/frame | client_scale |

Extra findings (details in StructuredOutput): cameo flash %10 consumers; Drawable flash %[0xde45d8]; Drawable
fade setters called with hard-coded client-frame literals (0x7954a8 0x8a, 0x79b05e/0x79b09d/0x85832e/0x8aee86/
0x8b1513/0x8b2392 0xa, 0x8583c9 0x1e, 0x664ff2/0x665031 [LTR]); hero-bar flash countdown; LW camera moveTo
per-update step; GameEngine+0x38 consumers; GameClient frame counter cadence assumption.
