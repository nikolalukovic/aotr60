# classify_0 (chunk 0 of 4): how the timing-global references are used

Plan mode blocked writing an/findings/classify_0.md, so the table is kept here. Copy it to
`an/findings/classify_0.md` once writes are allowed. Analysis only, nothing was patched.

Singletons confirmed from GameEngine::init (0x63ad4f, which passes each pointer to the subsystem registrar):
- TheGameClient = 0xde4388. vtable+0x7c returns the client frame (field +0x10), and +0x38 sets it.
- TheGameLogic = 0xde412c. The logic frame is at +0x40.
- TheWritableGlobalData = 0xde4364.
- TheFXParticleSystemManager = 0xde3744.
- TheDisplay = 0xde4418.
- TheAudio = 0xde42fc.
- TheEva = 0xde3670.
- TheScriptEngine = 0xde3bac.
- Stepper/GameEngine object = 0xde4324 (vtable 0xbfe260).

GameClient::update (0x64849e) runs on every client frame.

Abbreviations: CS = client_scale (needs the 60-FPS value), K = keep, U = uncertain. CF = client frame, LF = logic frame, PCF = runs per client frame.

| fn | what | dom/cadence | verdict | refs |
|---|---|---|---|---|
| 442e56 | Time-of-day light transition: (CF-start)*0.0333/dur. Reached from W3DDisplay::update (0x4430a7). | render/PCF | CS | 442e96 |
| 4431e5 | Network/debug status colours: LTR+stamp compared with LF | ui/PCF | K | 4433cc,443409 |
| 448b51 | Debug FPS line, prints SCALAR (the speed multiplier) | ui/PCF | K | 448cda |
| 44b788 | W3DDisplay::draw: syncTime += 33 (fast path) or += 33*CF delta, then WW3D::Sync | render/PCF | CS | 44b8dd,44b911 |
| 44bf2a | Setter at 0x44bf5c, W3DGameClient::setFrameRate: msframe=(int)ms (truncates) | init/once | CS | 44bf62 |
| 44d664 | Radar event marker shrinks over CLIENT_FPS*1.5 CF | ui/PCF | CS | 44d698 |
| 44dfba | Radar blink, period CF % (2*CLIENT_FPS) | ui/PCF | CS | 44e02f |
| 450abc | MilesAudio getElapsedMs (vt+0x1b4): CF delta*33.33, fallback 33.33 | audio/PCF | CS | 450b0c,450b50 |
| 452c50 | Audio volume fade; dt fallback 33.33 while paused | audio/PCF | CS | 452c9f |
| 452e0e | shouldProcessRequestThisFrame: delay<33.33 | audio/PCF | CS (low) | 452e2a |
| 45a188 | Audio delay clamped to >=33.33+1 | audio/event | CS (low) | 45a1ba |
| 45c675 | MilesAudioManager ctor: dt init = 33.33 | init/once | CS | 45c6f5 |
| 45cc91 | Audio delay<33.33 tests and clamp | audio/event | CS (low) | 45cccd,45cd0a,45cd31,45cdce |
| 45e9d1 | Ambient stream fade start: fadeTime-33.33 | audio/PCF | CS (low) | 45efe5 |
| 46154d | playAudioEvent: MaxDelay<33.33 flag | audio/event | CS (low) | 4615bf |
| 461a83 | processRequest: delay<33.33 flag | audio/PCF | CS (low) | 461af5 |
| 46f48e | Shroud "recently seen": LF < stamp+LTR*2/5 | render/PCF | K | 46f66a |
| 48017a,480215,4802aa,48033f,4803d4,480469,4804fe,480593,480628,4806bd,4808b7,48095c,480b64,480c46,480d1f,480df8,480ed1,480faa,481083,48115c,481235 | FX shader params "Bases03X..811Z": t=syncTime_ms*0.001*CLIENT_FPS gives a 30-fps keyframe index (709a0c...) | render/PCF | K (must stay 30) | each FIMUL [0xd9f60c] |
| 485db0 | W3DView::rotateCamera numFrames=ms/msframe | client/event | CS | 485dd4 |
| 485e5f | rotateCameraToward with hold frames | client/event | CS | 485e7a,485e9a |
| 485f0f | cameraModFinal*: remaining frames*msframe gives ms | client/event | CS | 485f62 |
| 486253 | Same as above, second variant | client/event | CS | 486279 |
| 48868b | rotateTowardPosition, plus zoom/pitch at 0x4887da | client/event | CS | 4886b1,4887f4 |
| 48885e | Camera mods at 0x48885e, 0x48891c and 0x4889a0 (ms/msframe) | client/event | CS | 48887a,48892e,4889b2 |
| 48a953 | updateCameraMovements: path advance by msframe ms; script cam (30/CLIENT_FPS)*frames | client/PCF | CS | 48a9f2,48aa62,48aae5,48ab06 |
| 48d26b | Camera move mode 3: frames=ms/msframe, steps per CF | client/event | CS | 48d3c5 |
| 4b2f9e | Real function at 0x4b300d, setAnimationCompletionTime(LF): ceil(LF*200) ms | render/event | K | 4b3022 |
| 4b67d4 | Animation speed matched to movement: (dist/speed per LF)*200 | render/PCF | K | 4b6855 |
| 4be587 | ScriptedModelDraw: ms*0.005 compared with weapon LF, gives a ratio | render/event | K | 4bee49 |
| 4c451d | Decal colour envelope 7323ce: INI ms*0.03 gives CF | render/event | CS | 4c4f74,4c4f8e,4c512d,4c513e |
| 4ce3a8 | W3DTreeDraw ModuleData: SinkTime/MorphTime = LTR*10 (logic parser 73a429) | init/once | K (med) | 4ce443,4ce456 |
| 4cf2b5 | W3DLightDraw::doDrawModule (runs per draw): t+=1/LTR, countdown LTR/rate | render/PCF | CS (needs 10) | 4cf2bb,4cf3cf |
| 5dc6e0 | Eva defaults LTR*3/7/30, compared with LF | init/once | K | 5dc6ee,5dc702,5dc70e |
| 5dcb28 | Eva "jump to last event": window ms*0.03 compared with CF | ui/event | CS | 5dcb6f |
| 5dd618 | Refs are in Eva::update (0x5dd86a): per-entry ms timers -= 33.33 on every CF | audio/PCF | CS | 5dd986,5dd997 |
| 5df46b | FXList CullTracking: TrackingSeconds*LTR, compared with LF (5e275b) | ini/once | K | 5df4ad |
| 5dff7a | AttachedModel FX ExpireTimer default LTR*8 (raw int); consumer not traced | init/once | U | 5dff96 |
| 5e05ab | FX terrain-decal envelope ms*0.03 gives CF | render/event | CS | 5e0776 |
| 5e1270 | ParticleSystem FX InitialDelay ms*0.005 into sys+0x120; consumer not found | client/event | U | 5e1cf4 |
| 5e36fb,5e3796,5e37d4,5e3f49 | Locomotor speed*0.2 and accel*0.04 | logic/LF | K | all refs |
| 5e4326 | LocomotorTemplate defaults: TurnTime, Acceleration, Braking = LTR | init/once | K | 5e4385,5e43ba,5e43c8 |
| 5e586f,5e6b6b,5e76ac | Locomotor step/expiry/height math per LF | logic/LF | K | all refs |
| 602ffe,60300a | ScriptEngine end-game/close-window timers = LTR*5 | logic/event | K | — |
| 605021 | Script debug "frames (secs)" text | ui/event | K | 605090 |
| 605e03,608d0b,609092,609200,60a15c | ScriptEngine delays/timers in LF | logic | K | all |
| 609685 | ScriptEngine::reset breezePeriod=LTR*10 (sway divides by stepper+0x38) | init/once | K | 609820 |
| 6164d7 | APT banner: LF remaining/LTR gives seconds | ui/PCF | K | 6165d1 |
| 626087 | Time-limit check, minutes*LTR*60 | logic | K | 626092 |
| 63239d | MP-only frame stretch: speedMult>=1 | network/PCF | K | 6323b2 |
| 63252f | "First sub-frame of tick" helper | client/PCF | CS (no-op: 6 and 12 give the same result) | 63252f,632535 |
| 6325a0 | Stepper framesPerTick=CLIENT_FPS/LTR | client/PCF | CS (=12) | 63260f,632615 |
| 632994 | Refs are in vt+0x98 (0x6329b0): catch-up only when c<6 | client/PCF | CS (no-op) | 632a95,632a9b |
| 635cf5 | Refs are in GameEngine::reset (0x635d11): +0x38/+0x44 init | init/once | CS (=12) | 635dca,635dd0 |
| 639fef | GameEngine::update: speed multiplier (forced to 1.0 in SP) and limiter | mixed/PCF | K | all 6 |
| 63cef1 | GameEngine::init tail: +0x38/+0x44 init | init/once | CS (=12) | 63cf0f,63cf15 |
| 6429ad | GlobalData defaults: OcclusionDelay, VoiceAttackChargeTimeout, EngagedStateTimeout, 3 unnamed | init/once | K | all |
| 645143 | Audio settings default: MinDelayBetweenEnterStateVoiceMS=LTR*5 (logic parser) | init/once | K | 6451c7 |
| 646771 | GameClient::init setFrameRate(33.333) gives msframe | init/once | CS | 646781 |
| 64849e | GameClient::update: recently-seen check LF<stamp+LTR*2/5 | client/PCF | K | 6487a3 |
| 65dd0f,65de5e | Network QPC pacing freq/LTR | network | K | 65dde9,65deb8 |
| 662cc6,663802,6638fc,6639cf,663c6b,665c33,667ed1,668af8,669932 | AIUpdate delays/stagger/steps in LF | logic | K | all |
| 664f93 | AIUpdate calls Drawable fadeOut/fadeIn(LTR); the fade counts CF (0x675ab5) | mixed/event | CS (needs 10) | 664ff2,665031 |

## Extra findings
- Frame limiter 0x63a19c..0x63a1ad: frameMs is an integer ftol(1000/fps). At 60 FPS that gives 17 ms, so 58.8 FPS and 4.90 Hz logic. Stock gives 33 ms, so 30.3 FPS and 5.05 Hz logic. The game would run about 3% slow. Fix: alternate 16 and 17 ms (an accumulated deadline).
- W3DDisplay::draw time-multiplier throttle: `add esi,-0x1e` at 44b98c and `cmp ecx,0x1d` at 44b9c1. Separately, `push 0x1e` at 44b8c5 is the CF%30 time-fast draw interval. In time-multiplier mode the game would run at half speed. Fix: change the throttle to 15 and 14, and the CF%30 interval to 60.
- msframe int truncation: 16.667 truncates to 16, so W3D time runs at 960 instead of 990 ms/s. Fix: alternate 16 and 17 in the sync add. At the IDIV sites, use (ms/33)*2.
- Drawable::draw 0x67c482 multiplies the material-pass opacity by 0.8 on each draw (0x67c4c7). It decays 2x fast, so use sqrt(0.8).
- Drawable fade (0x670a50/0x670aa2, updated at 0x675ab5) counts client frames. Every fadeIn/fadeOut caller needs auditing; alternatively, double the frame count inside these two functions.
- Stepper debug id LF*10+sub-1 (imul at 632631/632acf) collides when sub>10. It only feeds the Debug command queue (0x43afc0), so it is harmless.
- SwayClientUpdate (0x8cd55f) and W3DTreeBuffer (0x4e5eda, which caches stepper+0x38 into +0x5bc) both convert logic frames to client frames through stepper+0x38. They are correct if +0x38 is 12 before map load.
- MP catch-up window `add ecx,6` at 63a181 is MP only.
- GameLogic vt+0x34(sub) is called on every CF (632a82, 632ae6). Check it for hard-coded 6s; not verified.
