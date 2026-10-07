# AREA lw-camera

## SUMMARY
The Living World map camera can run at 60 FPS with two new call_gate sites, both in the LW scene draw vt20 0x49B618.

**(1) No LW camera stepping happens outside the A-only gates.** Every writer of the LW pose is either already A-only or runs in the logic phase after the B-render:
- Pose fields on the LW view [0xDE4958] (vtbl 0xBDE918): position +0xF8/+0xFC/+0x100, angle +0x11C, zoom +0x134, zoom velocity +0x138, fade +0x198.
- A-only writers:
  - LW translator 0x8392A7 (GATE_GC_LWXLAT), calling vt2C 0x49B799 and vt54 0x49A9CF.
  - LookAt translator 0x83B471, which also scrolls the LW view through vt2C at 0x83B86D (GATE_GC_LOOKAT).
  - vt6C 0x49AAB8 (GATE_GC_LWVIEW): zoom integrator 0x49AB21, velocity damping 0x49AB3E, fade, and through 0x6C0E4D the moveTo step 0x6BF78E.
- The translator message handler 0x838EBA runs during propagate, after the A draw. It only changes zoom velocity (vt5C) or sets up a moveTo (0x838D3A → vt9C 0x6BFB88). Neither moves the drawn pose.
- LW logic (sub 1 only, after the B-render) can jump the pose: DelayedSplineCamera 0x7FEE5B → 0x6BF878, the entry jump 0x6B33FB, and DelayedCamera → moveTo.
- LW logic also reads this camera state: zoom through vt60 at 0x6B96E1, the target +0x110 through vt70 at 0x6BAF63, and the fade-done flags through vt8C/vt90 in 0x6B5BD3. So vt6C must stay entirely A-only. The sites.json note on GATE_LWM that suggests letting the vt6C tail run on B should be dropped.

**(2) Presentation.** The LW W3D CameraClass at [view+0xC0] (vtbl 0xBE8748, same layout as the tactical camera) is rebuilt from the LW view state on every draw:
- 0x49B4A5 sets the clip planes (10, d·1.5+2000), the view plane (50°, auto vfov), builds the look-at matrix in 0x49AF11, then calls Set_Transform vt54.
- **B-render:** because B steps nothing and post-draw input does not move the pose, the B draw rebuilds exactly M_k. No swap is needed; a verify-and-fallback check is added.
- **A-render:** record M_k right after the build, then present a matrix-level halfway between M_{k−1} and M_k for the LW scene render only.
  - **Record/open point:** LW6_CAM_REC at 0x49B77E (`call 0x49B4A5`).
  - **Close point:** LW6_CAM_SCENE_END at 0x49B78F (`call 0x518000`, WW3D::Render(scene, camera)).
  - **Backstop:** the existing SCENE_RESTORE at 0x44A271.
- The existing swap code needs two changes for the LW camera: no shadow refit, and allowing the near/far clip planes to differ between the two poses.
- Do not interpolate the LW view state fields themselves: the draw writes the target +0x110..+0x118 and logic reads it.

**(3) Cut rules.** Present M_k without interpolation when:
- there is no contiguous A history;
- view state +0x14, active +0x18 or suspended +0x19 changed (activation, fades, and the APT-pose teleport in setSuspended 0x49BE23);
- aspect or view-plane extents changed;
- eye distance moved more than the limit or rotation exceeds 45°. This catches the instant setPos 0x6BF878 jumps (0x6B33FB, DelayedSplineCamera).

Smooth moveTo flights between regions are interpolated.

## COVERAGE
**Traced:**
- **Translators and view:** the LW translator update 0x8392A7 and its message handler 0x838EBA (vtable 0xC53AF4, ctor 0x838DF5 -> [0xDE8CAC]); the LookAt translator's LW branch 0x83B851; LW view vt2C/vt54/vt58/vt68/vt7C/vt9C and their getters; vt6C 0x49AAB8 including the tail 0x49AB21/0x49AB3E and the fade states.
- **Camera moves:** 0x6C0E4D; moveTo setup 0x6BFB88 and step 0x6BF78E with helpers 0x6BF657/0x6BF678/0x6BF96D; the instant setters 0x6BF878, 0x6B33FB and 0x49BE23.
- **Logic-side readers:** the delayed camera events processed by LW logic 0x6B96C4 (sub 1 only, stepper 0x632A85..0x632A92).
- **Draw path:** the LW draw 0x49B618, the camera build 0x49B4A5 and the matrix builder 0x49AF11; the CameraClass layout (ctor 0x534570, Set_Transform 0x533550, Set_View_Plane 0x533590, Set_Clip_Planes 0x5337C0); drawFrame's LW branch and exit path; the embedded APT draw vt24 0x49BEFD.
- **Callers:** a scan of every function referencing [0xDE4958] for camera-mover virtual calls; camera readers (eye tower, LWM picking, audio listener).
- **Constants:** livingworld.txt (identical in aotr and rotwk) and the LivingWorldMapInfo camera keys in aotr/data/ini/livingworld.ini.
- **Site checks:** listing grep, a brute rel8/rel32/jcc32 scan over every executable section, an abs32 scan, and byte identity across game.dat, delayfix.dat and game820.dat for 0x49B77E, 0x49B78F and the alternative 0x449F43.

**Not traced:**
- Whether any APT/InGameUI draw callback projects with the LW camera through a cached pointer. This is moot because LW6_CAM_SCENE_END ends the swap before the UI draw.
- The dispatch origin of the picking callers 0x9DB18B/0x9D8E9D/0x6C0E8D. They are input callbacks in A-only or propagate paths, not verified one by one.
- msg 0x99 -> 0x6B3EDD.
- The numeric value of +0xDC (the threshold is given relative to the map extent instead).
- LW army, banner and anim-object integrators inside the LW scene render; these are the LW draw audit's area.

**Scratch files:** written only under <analysis workspace>\lw_tmp\: cam_refs.txt, cam_fns.txt, cam_vtcalls.py, cam_ptrscan.py, cam_brscan.py, cam_xlat_msg.asm, sites_lw.txt. lw_tmp may be shared with other agents, so I left it in place.

## FINDING [high] Every LW camera stepper is already A-only or logic-phase; vt6C must stay fully A-only
The LW pose fields are position +0xF8/+0xFC/+0x100, angle +0x11C, zoom +0x134, zoom velocity +0x138 and fade +0x198 on [0xDE4958] (vtbl 0xBDE918).

Writers:
- (a) LW translator 0x8392A7 (GATE_GC_LWXLAT): vt2C 0x49B799 pan/rotate, which calls vt58 setPosition 0x49AA24 and updates +0x11C; and vt54 0x49A9CF, a zoom-velocity kick.
- (b) LookAt translator 0x83B471 (GATE_GC_LOOKAT): also calls LW vt2C at 0x83B86D when the LW view is active.
- (c) vt6C 0x49AAB8 (GATE_GC_LWVIEW): zoom integrator 0x49AB21 (+0x134 += +0x138, clamped by 0x40524B), multiplicative damping 0x49AB3E (+0x138 *= LWM+0x1DC, MouseWheelZoomDampenFactor 0.82), fade 0x49ABB4/0x49ABFD (+0x198 ± [0xDCB840]=0.025), and through 0x6C0E4D the moveTo step 0x6BF78E (vt68/vt58/vt7C).
- (d) The message handler 0x838EBA (translator vtable 0xC53AF4 slot 0) runs in propagate after the A draw; B has no messages. It only changes the zoom velocity (wheel msg 0x13 -> vt5C 0x49AAAC -> vt54) or sets up a moveTo (numpad-5 / msg 11 -> 0x838D3A -> vt9C 0x6BFB88). moveTo setup writes +0x54..+0x84 and +0x78, which the draw never reads.
- (e) LW logic at sub 1 (step(1) 0x632A8A -> TheLivingWorldLogic vt28 0x6BE50E -> 0x6B96C4 at 0x6BE5EC): delayed camera events. DelayedCamera (vtbl 0xC142E8 slot 0x7FEB59) -> moveTo setup. DelayedSplineCamera (vtbl 0xC14674 slot 0x7FEE5B) -> 0x6BF878, an instant vt58+vt7C.

LW logic also READS the camera state:
- isZoomedOut vt60 0x49A991 at 0x6B96E1 gates the delayed-event queue;
- vt70 0x49A64F reads the draw-derived target +0x110 at 0x6BAF63;
- vt8C/vt90 fade-done flags +0x24/+0x25, set by vt84/vt88 at the end of the vt6C fades, are read in 0x6B5BD3.

The zoom value, fade flags and target at logic time therefore steer LW logic. Running the vt6C tail on B (as the GATE_LWM risk note in sites.json anticipates for phase 6) would double zoom/fade speed and change LW event timing.
EVIDENCE: Disassembly of 0x8392A7 (calls at 0x8395FF/0x839663 vt2C, 0x8396A0 vt54); 0x83B851..0x83B86D; 0x49AAB8..0x49AC54; 0x6C0E4D tail-jmp 0x6BF78E; 0x6BF78E..0x6BF877; 0x838EBA..0x8392A4. Delayed-event classes found via vtable slots 0xC14688/0xC142F8 and creators 0x7FEB12/0x7FEF24 from 0x96CBCF -> queue 0x6B969A. GameClient::update order: 0x6484B2, 0x6484BD, 0x6484D9, then the draw TheDisplay vt30 at 0x648869.
REC: - Keep GATE_GC_LOOKAT, GATE_GC_LWXLAT, GATE_GC_LWVIEW (whole vt6C) and GATE_GC_LWUI exactly as they are on the LW map. No new stepping sites are needed.
- Replace the GATE_LWM phase-6 note with: vt6C stays A-only; 0x49AB21/0x49AB3E/fade are not half-stepped.
- Determinism check per LW tick: snapshot view+0xF8..+0x100, +0x11C, +0x134, +0x138, +0x198, +0x24/+0x25 and +0x110 at LW logic entry. These must equal the stock baseline.

## FINDING [high] B-renders need no camera swap: the LW draw rebuilds the W3D camera from unchanged state
Unlike the tactical view (../reviews/camera_feasibility_check.md Fix 2), the LW camera is not persistent between draws. The LW draw rebuilds it every time in 0x49B4A5:
- Set_Clip_Planes 0x5337C0(10, dist*1.5+2000);
- Set_View_Plane 0x533590(50 deg, -1);
- look-at matrix 0x49AF11, then Set_Transform vt54 0x533550.

All inputs are LW view fields that only A-only code or logic changes, and post-draw propagate does not touch them. So the B draw reproduces M_k bit-exactly: same code, same inputs, same 24-bit x87 control word.

The B rebuild also rewrites the derived persistent fields (+0x13C, +0xD8, target +0x110..+0x118, the LM_SunRays transform, zoom alphas) with the same values. That is idempotent.
EVIDENCE: 0x49B618/0x49B4A5/0x49AF11 disassembly. +0xD8 and +0x13C are derived from +0x198 and the livingworld.txt tables written in 0x49C79C (file read: 35/0.06/0/1/1/0.02, 65/0.06/0/1/15/0.03, 50/0.05, then 0.01/0.01/0.025). +0xDC is set at init in 0x49BB8B (0x49BCFB). Message-handler analysis: see the previous finding.
REC: Use LW6_CAM_REC's B path only as a verifier: compare the camera with lw[1] (counter lwBExact). If they differ (lwBMismatch, expected 0), present M_k via a no-refit swap and log the LW view fields once. The mismatch indicates a gating hole.

## FINDING [high] Interpolate the W3D camera matrix, never the LW view state fields
Writing a halfway pose into view+0xF8/+0x11C/+0x134 and re-running vt20/0x49B4A5 would leave halfway-derived values in persistent fields that LW logic reads:
- target +0x110..+0x118, read via vt70 0x49A64F at 0x6BAF63;
- +0x13C and +0xD8;
- the LM_SunRays transform.

Those values would reach logic whenever the following B draw is skipped. drawFrame can exit early at 0x449D1E when 0x516C40 fails, and W3DDisplay::draw has its own skip paths. A second 0x49B4A5 call would also re-run the TheAudio listener-dirty hook.

A matrix swap touches only the CameraClass, and CamSwapEnd restores it exactly.
EVIDENCE: 0x49AF11 writes [esi+0x110..0x118] at 0x49AFAA/0x49AFBE/0x49AFD3/0x49AFFD/0x49B016 and sets [esi+0x188]->vt54 at 0x49B0B6. vt70 0x49A64F reads +0x110/+0x114/+0x118. Callers of vt70 found at 0x6BAF63 (0x6BAEB6, vtbl 0xC14534).
REC: Implement the A presentation as in camera.cpp:
- record cam+0x18 (48 bytes) and cam+0xD8 (28 bytes) after 0x49B4A5;
- InterpolateCameraHalfway(M_{k-1}, M_k);
- Set_Transform plus a raw view-plane write;
- restore after the scene render.

Optionally also lerp the LM_SunRays height: [view+0x188] matrix m[11] is dist*2*f(zoom), and its x/y are constants 192.43/569.88. Save and restore it the same way.

## FINDING [high] The existing InterpolateCameraHalfway would cut on every LW zoom frame (far plane changes)
camera_math.cpp rejects any pose pair whose aspect/near/far differ. The LW camera sets zfar = dist*1.5 + 2000 on every draw (0x49B597..0x49B5B8, using [0xBDE8C8]=1.5 and [0xBDE8C4]=2000), and dist changes with zoom +0x134. So every zooming frame would be treated as a cut and zoom would stay at 30 Hz steps. znear (10) and the view-plane extents (fixed 50 deg hfov, aspect from +0xE8) are constant.
EVIDENCE: 0x49B584..0x49B5D8 disassembly; camera_math.cpp 'Aspect, near and far must be identical'.
REC: Add an LW option to CameraCutLimits (e.g. allowClipChange):
- require equal aspect (vp[4]);
- set mid.znear = min(a,b) and mid.zfar = max(a,b);
- keep the 5% extent rule.

Use it only from LwCamAfterBuild. Add a unit test for a zoom pair.

## FINDING [high] LW swaps must not do the shadow refit
BeginSwap in camera.cpp calls the shadow refit 0x47D37D whenever TerrainVisual, ShadowManager and GD+0x62 are set. In stock, the shadow manager is never fitted to the LW camera. Refitting it to the LW camera on swap and restore would leave the shadow manager in a non-stock state (camera copy via 0x534AD0, fit via 0x47C276).
EVIDENCE: camera.cpp BeginSwap/RefitWanted; ../reviews/camera_feasibility_check.md (0x47D37D copies the camera and fits).
REC: Give BeginSwap a refit parameter, or add a separate LW BeginSwap, and always pass false for [lwview+0xC0]. CamSwapEnd already skips the refit when save.refit is 0.

## FINDING [medium] Readers of the LW camera outside the draw must see exactly stock M_{k-1}/M_k
Several A-only paths read the LW camera between draws:
- the eye tower 0x7FB1DF (vt38 pick 0x49A7A1 at 0x7FB1E6) inside LWM vt28, which consumes the logic RNG;
- LWM 0x6110BE (vt38 at 0x6110F8);
- the A-only audio update, which reads the listener through TheAudio vt1A4 0x450A60 -> LW vt80 0x49A6B9 (cam Get_Position 0x53B370) after drawFrame.

In stock they see the camera left by the previous draw. Under this design that holds because the A swap is ended right after the LW scene render and the B draw rebuilds M_k. If the swap ever leaked past drawFrame, the eye tower could pick differently and desync LW logic.
EVIDENCE: Ref scan of every [0xDE4958] user. 0x450A60 decompile: TheAudio+0x678==1 -> LW vt80. TheAudio vt58 0x450A08 only sets +0x6AB=1. clientUpdate order puts propagate 0x6324A1 and audio 0x6324F9 after GameClient::update/draw.
REC: - End the LW swap at LW6_CAM_SCENE_END and keep SCENE_RESTORE as the backstop.
- Add a telemetry assert: g_swapActive==0 at C0 and at clientUpdate exit.
- Include eye-tower logic-RNG traces in the phase-6 determinism gate.

## FINDING [medium] Cut rules for LW camera jumps
Smooth moves should be interpolated:
- moveTo: vt9C 0x6BFB88 sets up a flight from the current pose; the 0x6BF78E step advances t by +0x7C per A-render, eased through the curve +0x28 / 0x90AAEF. 0x6BF96D computes the frame count from distance / LWM+0x1E0 (AutoScrollSpeed) and zoom difference, capped by MaxAutoScrollTime.
- translator/LookAt scroll, rotation (+0x11C clamped to +-1 rad in vt2C) and zoom inertia.

Instant pose changes need a cut:
- (a) 0x6B33FB (from 0x625E20) -> 0x6BF878, which also sets +0x1C/+0x20 at LW (re)entry;
- (b) setSuspended vt50 0x49BE23, which copies LWM AptZoom/AptCenter (+0x34/+0x2C/+0x30) into +0x134/+0xF8/+0xFC; unsuspend restores the aspect and calls vt1C;
- (c) activation 0x6BF8BB / vt28;
- (d) DelayedSplineCamera 0x7FEE5B -> 0x6BF878 once per LW logic tick (5 Hz).

The fade states +0x14 = 2/3 blend the target toward +0x1C/+0x20 and the angle toward 0 smoothly (0x49AFE1..0x49B016, 0x49B0EB..0x49B110).
EVIDENCE: Disassembly of 0x6BFB88, 0x6BF78E, 0x6BF878, 0x6BF96D, 0x6B33FB, 0x49BE23, 0x7FEE5B. livingworld.ini LivingWorldMapInfo: Extent X:4200 Y:3450, AutoScrollSpeed 300, MaxAutoScrollTime 1.5, MouseWheelZoomPerTick 0.01, MouseWheelZoomDampenFactor 0.82.
REC: Present M_k without interpolation when any of these holds:
- the A history is not contiguous (renderId, cam or view pointer changed);
- int[view+0x14], byte[+0x18] or byte[+0x19] differ between the two records;
- the aspect differs or an extent changes by more than 5%;
- the eye translation exceeds LwCamCutDist (start at 1500; LW map extent 4200x3450) or the rotation exceeds 45 degrees.

Count cuts and tune LwCamCutDist from telemetry. DelayedSplineCamera steps below the limit are halved, which is acceptable: stock already shows them as 5 Hz steps.

## FINDING [medium] SceneOpen_A misbehaves harmlessly on the LW map
The LW draw path passes SCENE_OPEN 0x449DAB (from 0x449D53/0x449D61/0x449D69) before vt20 at 0x449F45. On every A-render, SceneOpen_A therefore:
- opens the fraction window (g_pw2Open; GE+0x3C is changed during the LW draw and UI draw, restored at 0x44A271);
- runs the tactical camera part against TacticalCamera() = [[0xDE447C]+0x104], whose records never update on the LW map because updateViews is skipped at 0x449D29. It increments aNoHistory every A.

It does not swap (renderId mismatch), so there is no conflict with the LW swap, but the stats get polluted.
EVIDENCE: drawFrame 0x449D24..0x449D40 (GL+0x125 skips vtA0 updateViews); camera.cpp SceneOpen_A.
REC: - In SceneOpen_A, skip the tactical camera part (and optionally the fraction window) when byte[GL+0x125] != 0.
- Make CameraReset() also invalidate g_lwRec.
- Phase-6 BlockReason should allow mode 8 / GL+0x125 only when LW6_CAM_REC and LW6_CAM_SCENE_END are installed.

## FINDING [low] Embedded APT LW window (suspended mode) needs no interpolation
When the LW view is suspended (+0x19), drawFrame does not call vt20. Instead an APT window class (vtbl 0xC84E48, slot +0xC 0x975441) draws the map into a sub-viewport through vt24 0x49BEFD:
- viewport 0x47DFDB and aspect 0x533630;
- +0x13C = LWM+0x38 (AptPitch);
- its own 0x49B4A5 call at 0x49C168 (e8 38 f3 ff ff), then WW3D::Render at 0x49C179 (e8 82 be 07 00).

The pose is the AptCenter/AptZoom teleport from 0x49BE23. Only wheel zoom (msg handler -> vt6C, A-only) can move it. B rebuilds the same camera.
EVIDENCE: 0x49BEFD decompile and disassembly 0x49C154..0x49C182; 0x975441; dtor 0x9753D3 (reactivates via 0x6BF8BB).
REC: Leave it stock in phase 6, since it is exact at 30 Hz steps. If smooth wheel zoom is wanted later, reuse STUB_LW6_CAM_REC at 0x49C168 and STUB_LW6_CAM_SCENE_END at 0x49C179. Record with the suspended flag so that any transition cuts.

## FINDING [low] A-frame visual-only mismatches inside the LW scene
With a camera-only swap, two scene elements on A frames still use the S_k values while the camera shows the halfway pose:
- the LM_SunRays object [view+0x188] (fixed x/y, height = dist*2*f(zoom));
- the zoom-faded objects +0x18C/+0x190/+0x194 (alphas 1-z, (z-0.5)*2, 0.6+0.4z via 0x49A4FB -> 0x50E244).

During fast zoom this gives a half-step lag of the sun-ray height and fades. Nothing logic-visible.
EVIDENCE: 0x49B047..0x49B0B6 (sun-ray matrix), 0x49B6D2..0x49B765 (alphas); init string 0xBDE908 'LM_SunRays' at 0x49C7E2.
REC: Accept for phase 6. Optionally save, lerp and restore sun-ray m[11] alongside the camera swap. Leave the alphas alone.

## VERIFY SITE LW6_CAM_REC -> confirmed
Note on scope: this task is read-only. I only wrote a scan script under <analysis workspace>\lw_tmp.

How I checked:
- pefile is not installed, so I parsed the PE section table by hand.
- Bytes at VA 0x49B77E are `e8 22 fd ff ff`, and the target is 0x49B783 + rel = 0x49B4A5. Both match the proposal.

1. Boundaries: capstone shows `0x49B77C mov ecx,esi` / `0x49B77E call 0x49b4a5` / `0x49B783 push [esi+0xC0]` / `0x49B789 push [esi+0xC4]` / `0x49B78F call 0x518000` / `pop ecx` x2 / `pop esi` / `leave` / `ret` at 0x49B798. The span is exactly one 5-byte call.

2. Branch into span: I scanned every executable section (.text, stxt774, stxt371, .mackt, .danetta, .angmar) for rel8 (EB/7x/E0-E3), rel32 (E8/E9) and jcc32 (0F 8x) targets in 0x49B77F..0x49B782, and found none. A whole-image abs32 scan for those four values also found none.

3. Overlap: no tools/sites.json entry (129 addresses) references 0x49B4A5, 0x49B618, 0x49B77E or 0x49B783. None of the 16 AotR hooks is nearby. The site is not already covered:
   - GATE_LWM is at 0x49AAD4.
   - The LW gates (GC_LWXLAT, GC_LWVIEW, GC_LWUI, GC_DISPUPD_LW) are all in clientUpdate before the draw.
   - SCENE_OPEN only handles the tactical camera `[[0xDE447C]+0x104]`. SCENE_RESTORE / CamSwapEnd only ends a swap and is the backstop at the drawFrame exit.

4. 30-mode path:
   - With g_m60 == 0 the stub does `jmp [T_49B4A5]` with return address 0x49B783 already on the stack, which is the exact original call.
   - Only EAX and EFLAGS are touched before that. 0x49B4A5 does not read either: it starts with `push ebp`, and EAX is first written at 0x49B575 or 0x49B5FD. ECX is untouched.

5. B path:
   - drawFrame (0x449F2F..0x449F45) calls vt20 = 0x49B618 on every render where `view+0x18 && !view+0x19`, so it runs on both A and B.
   - drawFrame is reached from W3DDisplay::draw inside clientUpdate, inside the C0_PRERENDER bracket, so g_inClientUpdate and g_skipB are valid at this point.
   - The rebuild in 0x49B4A5 is a pure function of view state: +0x134, +0xD8/+0xDC, +0x13C, +0xF8..+0x10C, +0x11C, +0x198, +0x19. 0x49A4D4 and 0x49AF11 read no time or RNG. 0x49B618 recomputes +0x13C/+0xD8 from +0x198 and the tables 0xDCB86C/0xDCB878/0xDCB874/0xDCB880.
   - With all writers of those fields A-only, the B rebuild should equal M_k bit-for-bit. That makes lwBExact the expected case and lwBMismatch a gating-hole detector, as proposed.
   - The audio call at 0x49B610 sets a dirty flag that stock also sets on every render, so it is idempotent.

6. x87 and ABI:
   - Every fld in 0x49B4A5 is matched by an fstp, and 0x49A4D4's st0 return is popped at 0x49B51B and 0x49B54E, so x87 depth is 0 at return.
   - ESI, EBX, EDI and EBP are callee-saved. After 0x49B783 only ESI and EBP are read.
   - XMM is volatile across the original call, so the stub may clobber it.
   - The camera is allocated once in vt10 0x49D26D (0x534570, stored at view+0xC0). The cam+0x18 (48 B) and cam+0xD8 (28 B) layout matches camera.cpp (kCamViewPlane=0xD8, kCamDirty=0xFC).

7. Placement: on the full-screen LW map this is the only camera build between the A-only camera steps and WW3D::Render 0x518000. The other callers of 0x49B4A5 (vt04 0x49BB35, 0x49BB8B via 0x49C79C, vt50 0x49BE23, vt24 0x49BEFD) are different call sites. vt24, the embedded APT window, needs +0x19 != 0, so it never runs in the same draw as vt20. The proposal's risk note already says so.

Notes for implementation (not site defects):
- The existing `BeginSwap(cam, xf, vp)` in src/camera.cpp has no refit parameter. It calls RefitWanted()/ShadowRefit (0x47D37D) whenever terrain visual, shadow manager and GD+0x62 are present, which may be true on the LW map. The LW path therefore needs a new no-refit variant (or a parameter) so that save.refit stays false. Otherwise CamSwapEnd would run a shadow refit with the LW camera.
- The A swap depends on the sibling LW6_CAM_SCENE_END. The drawFrame tail after 0x449F48 goes on to 0x44A0AD and later the shared restore point, so the SCENE_RESTORE backstop is reached.

corrected_site_json is empty because the site JSON is unchanged.
CORRECTED: 

## VERIFY SITE LW6_CAM_SCENE_END -> confirmed
This task is read-only. Only scratchpad reads and in-memory scripts were used.

Checks on LW6_CAM_SCENE_END, all done against <analysis workspace>/ghw\bin\game.dat:

1. Bytes. pefile is not installed, so I parsed the PE sections by hand. 0x49B78F reads `e8 6c c8 07 00`, and 0x49B794 + 0x0007C86C = 0x518000. This matches original_hex.

2. Boundaries. Capstone shows `0x49B783 push [esi+0xC0]`, `0x49B789 push [esi+0xC4]`, `0x49B78F call 0x518000`, `0x49B794 pop ecx`, `0x49B795 pop ecx`, `0x49B796 pop esi`, `0x49B797 leave`, `0x49B798 ret`. The 5-byte span is one whole instruction.

3. Branch into the span. My own brute-force scan of every executable section (E8/E9 rel32, EB/7x/E0-E3 rel8, 0F8x jcc32) found 0 targets in 0x49B790..0x49B793. The abs32 scan over all sections found 0 hits, and rg of listing.asm for 0049b79[0-3] found 0 hits.

4. Overlaps. There is no AotR hook nearby. The nearest existing sites.json sites are 0x49AAD4 (LWM gate), 0x44A1FD and 0x44A271. Nothing already covers this call: SCENE_RESTORE 0x44A271 only ends the swap later, at the drawFrame exit.

5. Runs per render on the LW map. Only the LW view vtable 0xBDE918 points to 0x49B618, at slot 0xBDE938 = vt+0x20. drawFrame calls it at 0x449F45 when [ecx+0x18]!=0 and [ecx+0x19]==0. That condition does not depend on A or B, so the call runs on both renders whenever the LW scene is drawn through vt20. 0x49B618's own early-outs ([esi+0xC4], [esi+0xC0], [esi+0x18]) all jump to 0x49B796, which skips the call. In that case no LW scene render happens, and SCENE_RESTORE remains the backstop.

6. 30-mode path. The stub does `cmp g_swapActive; je stock` and then `jmp [T_518000]`. That is a tail jump with [esp]=0x49B794 and both cdecl args untouched, so it matches the original exactly. EFLAGS are dead at a call boundary. IS_MAIN_KEEP_EAX in src/stubs/stubs.asm is push eax / cmp fs:[24h],[g_mainTid] / pop eax. Pop leaves the flags alone, so `jne stock` is correct.

7. B / swap path. On entry [esp+4]=scene (the last push, [esi+0xC4]) and [esp+8]=camera. The first `push [esp+8]` pushes the camera. After that push the second `push [esp+8]` reads the old [esp+4], which is the scene. So the call is Render(scene, camera) in the original order.
   - 0x518000 has an SEH frame plus a 0x148-byte frame (0x154 in total), returns AL and uses a plain `ret` (cdecl). So `add esp,8` is correct.
   - SAVE_ALL / RESTORE_ALL (pushfd, pushad, XMM0-7) restore EAX to its value after Render, so AL is preserved.
   - The final `ret` returns to 0x49B794 with the two original args still on the stack for the pop ecx / pop ecx. ESI, EDI, EBX and EBP are callee-saved by 0x518000 and saved by the stub. The x87 stack is empty at the call.
   - LwCamSceneEnd checks g_save.cam against the LW camera, so it cannot end a tactical swap. updateViews is skipped on the LW map anyway.

8. Builds. The bytes are identical in all three builds, as the proposal states. 0x49B618 is reached only through the LW vtable, so no other caller can be affected.

Notes, not defects of this site:
- (a) LW vt24 0x49BEFD (slot 0xBDE93C) is the embedded-window LW render, used when [view+0x19]!=0. It has its own WW3D::Render call at `0x49C179: call 0x518000`, with the same `push [esi+0xC0] / push [esi+0xC4]` at 0x49C16D/0x49C173 and `pop ecx; pop ecx` at 0x49C17E. This site does not end a swap on that path. If the companion begin site can open an LW swap while the vt24 path is in use, either add a twin end site at 0x49C179 (same 5-byte call_gate shape, resume 0x49C17E) or rely on the SCENE_RESTORE backstop.
- (b) This site only does something together with the companion swap-begin site (lw[1] record / BeginSwap on the LW camera), which is not part of this proposal. Without it, g_swapActive is 0 on the LW map and the stub always takes the exact stock path.
CORRECTED: 

## VERIFY FINDING Every LW camera stepper is already A-only or logic-phase; vt6C must stay fully A -> confirmed
I checked each claim of the finding against game.dat (capstone disassembly, a manual PE read of the vtable, and ghq) and against tools/sites.json and src/sites.gen.h.

1. **vt6C 0x49AAB8 does what the finding says.**
   - It calls 0x6C0E4D at 0x49AAD4.
   - Zoom integrator: 0x49AB21 adds +0x138 into +0x134.
   - Damping: 0x49AB3E multiplies +0x138 by [[0xDE3C08]+0x1DC]. Velocity below 0.001 is snapped to 0. The result is clamped by 0x40524B between [0xDCB884] and 1.0.
   - Fade-out at 0x49ABB4 subtracts [0xDCB840] from +0x198. When it reaches 0 it calls vt88.
   - Fade-in at 0x49ABFD adds to +0x198. When it ends it calls vt4C(1,1), sets +0x198=0, calls vt28(0) and then vt84.
   - The function has no write to +0x11C. The "angle inertia" that the GATE_LWM risk note expects does not exist.

2. **The vtable 0xBDE918 slots match.** vt2C=0x49B799, vt54=0x49A9CF, vt58=0x49AA24, vt5C=0x49AAAC, vt60=0x49A991, vt6C=0x49AAB8, vt70=0x49A64F, vt84/88/8C/90=0x49D0E9/EE/F3/F7, vt9C=0x6BFB88.

3. **The input paths are as described.**
   - LW translator 0x8392A7 calls vt2C at 0x8395FF and 0x839663, and vt54 at 0x8396A0.
   - LookAt 0x83B851..0x83B86D calls LW vt2C when [0xDE4958] is non-null and +0x18 is set.
   - In sites.gen.h, GATE_GC_LOOKAT (0x6484B2), GATE_GC_LWXLAT (0x6484BD), GATE_GC_LWVIEW (0x6484D7, the whole vt6C) and GATE_LWM (0x49AAD4) all exist as A-only call gates.

4. **LW logic reads camera state, as claimed.**
   - At 0x6B96E1/0x6B96F8 it checks [0xDE4958]+0x14==1 and then calls vt60. vt60 0x49A991 compares +0x134 against [0xDCB884], so the zoom value at logic time decides whether the delayed-event queue runs.
   - At 0x6BAF63 it calls vt70, which reads +0x110/+0x114/+0x118.
   - At 0x6B5BDE/0x6B5BF7 it calls vt8C/vt90 (the fade-done flags).
   - So if the vt6C tail ran on B, zoom and fade would advance twice per stock tick, and LW event timing would change. The finding's main claim holds.

5. **The draw is safe to repeat on B.** vt20 0x49B618 only writes values derived from the pose: +0x13C and +0xD8 from +0x198, and +0x110 through 0x49AF11 inside 0x49B4A5. The other writers of +0xF8/+0x11C/+0x134/+0x138/+0x198 in 0x499000..0x49D000 are:
   - init/load: vt4 0x49BB35, 0x49BB8B (from loader 0x49C79C), 0x49A5B1 (from vt10/0x49D105);
   - vt2C, vt4C, vt50 and vt68.
   
   None of them is reached from the draw. Because B draws the same pose as A, B rewrites identical derived values.

**One limit the finding understates.** "No new stepping sites" is right for determinism. But it also means A and B draw the same LW camera pose, so LW pan, zoom and fade still move at 30 Hz on screen. Phase 6 would then only add duplicated frames for camera motion. A smooth camera would need a presentation-only interpolation, which is a design choice and not a correctness defect.
CORRECTED REC: - **Keep the existing gates.** GATE_GC_LOOKAT, GATE_GC_LWXLAT, GATE_GC_LWVIEW (the whole vt6C 0x49AAB8), GATE_GC_LWUI, GATE_GC_DISPUPD_LW and GATE_LWM stay A-only on the LW map. Never run the vt6C tail (0x49AB08..0x49AC52) on B. It holds the zoom integrator 0x49AB21, the damping 0x49AB3E and the fades 0x49ABB4/0x49ABFD, and LW logic reads their results: zoom +0x134 through vt60 at 0x6B96F8, the fade-done flags through vt8C/vt90 at 0x6B5BDE/0x6B5BF7, and +0x110 through vt70 at 0x6BAF63.
- **Fix the GATE_LWM risk note in sites.json.** Say that vt6C stays fully A-only in phase 6 and that the tail has no angle inertia; +0x11C is written only by vt2C, vt4, vt4C, vt50 and init.
- **Repeating the LW draw on B is safe.** vt20 0x49B618 only writes values derived from the pose (+0x13C, +0xD8, and +0x110 through 0x49AF11). They come out identical when the pose has not changed.
- **Determinism telemetry.** At each LW logic entry (0x6B96C4), snapshot view +0xF8..+0x100, +0x11C, +0x134, +0x138, +0x198, +0x24/+0x25 and +0x110, and compare them to a stock run fed the same input. Also check that the run counts of vt6C and 0x6C0E4D match stock per tick, and that the B-skip counts for GATE_LWM are 0.
- **Smooth LW camera motion is optional and separate.** With the gates above, A and B draw the same LW pose, so camera motion stays at 30 Hz. If smoother motion is wanted, add a presentation-only window around the LW draw only (0x449F45 → vt20). On the A-render it would save the pose fields, draw from the halfway values (for example, zoom half a velocity step back) and restore them byte-exactly before returning. Persistent state and the logic-visible fields must never change.

## VERIFY FINDING B-renders need no camera swap: the LW draw rebuilds the W3D camera from unchange -> confirmed
Scope: this was a read-only check.

What I confirmed in game.dat (ghq decompiler + capstone):

1. 0x49B618 (LW client vt20) rebuilds the camera on every draw.
   - It recomputes +0x13C and +0xD8 from +0x198, the livingworld tables at 0xDCB86C..0xDCB880 and the GD+0xD2E flag.
   - It sets the zoom alphas on +0x18C/+0x190/+0x194 through 0x49A4FB -> 0x50E244, which sets an absolute alpha.
   - It then calls the +0xC4 scene vt18(+0xE0), then 0x49B4A5, then render 0x518000.
   - 0x49B4A5 calls Set_Clip_Planes 0x5337C0(10, local_c*k+c), then Set_View_Plane 0x533590(fov, -1), then look-at 0x49AF11, then camera vt54 (Set_Transform), then TheAudio vt58.
   - TheAudio vt58 is 0x450A08. It only does `mov byte [ecx+0x6AB],1`, so it is idempotent.
   - 0x49AF11 reads +0x134, +0xF8..+0x10C, +0x1C/+0x20, +0x198 and +0x11C. It writes +0x110..+0x118 without reading the old values back, and calls Set_Transform on +0x188 with an absolute matrix.
   - Every write in the draw is a pure function of its inputs, so the derived fields are idempotent, as the finding says.

2. Who writes those inputs. Only pre-draw A-only code, logic, or init/load writes them:
   - vt6C 0x49AAB8 does +0x134 += +0x138 and ramps +0x198. It also calls LWM 0x6C0E4D -> moveTo step 0x6BF78E, whose only caller is 0x6C0E4D, which writes vt68/vt58/vt7C.
   - The LW camera 0x8392A7 -> vt2C 0x49B799 does the pan and rotate.
   - Init/load: 0x49BB8B, 0x49C79C, ctor 0x49CF5C.
   - The LW translator's message handler 0x838EBA runs during propagate, after the draw. It only writes the zoom velocity +0x138 (vt5C -> vt54 0x49A9CF) and the moveTo record (vt9C 0x6BFB88 writes +0x54..+0x84 and +0x78). The draw reads neither; both are integrated pre-draw on the next A.
   - So with continuous input, B redraws exactly the A/stock-k camera. The idempotence claim holds.

3. Caveats the finding missed:
   - **The camera is not fully rebuilt.** Aspect ratio (0x533630) and viewport (0x47DFDB) persist on the camera. Only the AptMapPreview draw path vt24 0x49BEFD and setSuspended vt50 0x49BE23 set them. Set_View_Plane(fov, -1) uses the stored aspect. This is harmless in steady state, because the same per-frame sequence repeats. But a verifier that compares only the transform misses it.
   - **Rare event writers.** These can change draw inputs on transitions:
     - AptMapPreview teardown 0x9769E8 calls vt50(0) and vt28(0); it restores aspect and viewport and clears +0x19.
     - The fade callback 0x8004C9 calls vt4C 0x49C185, which sets +0x198 to 0 or 1 and +0x134/+0x138 to 0.
     - setSuspended(1) writes +0x134, +0xF8 and +0xFC from [0xDE3C08] and calls 0x49B4A5 outside the draw.
     - I did not trace whether these callbacks run in A's post-draw section (shell, window or propagate). If they do, B shows the post-event state for one frame.
   - **The 24-bit x87 argument is weak.** Most of the math is SSE. The real reason B matches is the same code on the same inputs.
   - **Evidence is incomplete.** The finding says +0xDC is set in 0x49BB8B. It is also written in 0x49AC8D, in 0x49C79C (ini) and in the ctor; all of those are init/load.

The finding's conclusion (no camera swap needed on B) holds.
CORRECTED REC: Do not swap the camera on B. Let the LW draw 0x49B618 run on B unchanged, because it rebuilds clip planes, view plane, look-at and Set_Transform from fields that only pre-draw A-only code (vt6C 0x49AAB8, LWM 0x6C0E4D -> 0x6BF78E, LW camera 0x8392A7 -> 0x49B799) or logic/init writes. Post-draw LW translator input (0x838EBA) only touches +0x138 and the moveTo record, which the next A integrates.

Keep LW6_CAM_REC on B as a verifier only (lwBExact / lwBMismatch), with three changes:

1. **Compare the whole camera state against the A record:**
   - transform cam+0x18..+0x47;
   - view plane / aspect +0xD8..+0xE8;
   - clip planes +0xEC/+0xF0;
   - viewport.

   Comparing only the matrix misses the persistent aspect and viewport, which 0x49BEFD and 0x49BE23 set.
2. **Expect mismatches only on transition frames.** They can come from AptMapPreview teardown 0x9769E8 (vt50(0)/vt28(0)), the fade callback 0x8004C9 -> vt4C 0x49C185, or setSuspended(1). Log each one together with +0x19, +0x198 and +0x134.
3. **On a mismatch, don't apply a transform-only no-refit swap.** It cannot restore a changed viewport, aspect or +0x19 picture. Either accept the one-frame visual difference, or, if exactness is wanted, snapshot and restore the full camera block listed above.

Separately, find out whether those three event callbacks run in A's post-draw section (shell vt28, window update or propagate). If they do and exactness matters, defer their effect, or do the full-block restore on that one B.

## VERIFY FINDING Interpolate the W3D camera matrix, never the LW view state fields -> confirmed
Note: the task is read-only, so I wrote nothing outside the scratchpad.

The core claim holds; several supporting details are overstated or wrong.

Verified in the binary:
- **What the LW draw does.** LW draw vt20 0x49B618 rebuilds the camera every draw. It writes view+0x13C and +0xD8, sets the fade layers +0x18C/+0x190/+0x194 via 0x49A4FB, calls Set_Transform on the scene at 0xC4, then calls 0x49B4A5 and then 0x518000(scene +0xC4, camera +0xC0).
- **What 0x49B4A5 does.**
  - It derives dist and pitch from +0xD8/+0xDC/+0x13C/+0x134. When byte +0x19 is set, pitch is overridden by [0xDE3C08]+0x38.
  - It calls Set_Clip_Planes 0x5337C0(10, dist*1.5+2000) and Set_View_Plane 0x533590(0.872665, -1).
  - It calls 0x49AF11 and then camera vt54 (Set_Transform), followed by TheAudio [0xDE42FC] vt58.
- **What 0x49AF11 writes.** It writes the persistent target +0x110/+0x114/+0x118 (0x49AFAA/0x49AFBE/0x49AFD3/0x49AFFD/0x49B016). The value is lerp(+0xF8 to +0x104) by f(+0x134), then lerped toward +0x1C/+0x20 by +0x198. It also sets the LM_SunRays transform via [view+0x188] vt54 at 0x49B0B6: identity, m[3]=192.43, m[7]=569.88, m[11]=dist*2*pitch.
- **The vt70 reader is real.** The LW view object is [0xDE4958] (vtbl file slots: +0x20 = 0x49B618, +0x6C = 0x49AAB8, +0x70 = 0x49A64F). vt70 0x49A64F returns +0x110..+0x118. The only call through [0xDE4958]+0x70 is at 0x6BAF6F, in 0x6BAEB6. That is slot 0 of the vtable 0xC14534, which the ctor 0x6B9F80 installs at +0x18 of TheLivingWorldLogic [0xDE4950]. It is an ally/enemy-defeated handler.
- **The skip path exists.** drawFrame skips to 0x44A23E (je at 0x449D1E) when 0x516C40 fails, so a following draw can be skipped.

Corrections:
1. **+0x13C and +0xD8 do not come from the halfway inputs.** vt20 computes them from the globals 0xDCB86C..0xDCB880 and +0x198. Writing a halfway +0xF8/+0x11C/+0x134 would not change them unless +0x198 were also lerped.
2. **The vt70 consumer does not reach logic state.** 0x6BAEB6 only feeds a camera move: 0x6BF96D is a pure duration computation (vt74/vt64 reads, no writes), followed by view vt9C (moveTo) and a message box. It touches no logic state, RNG or save data. Leftover halfway values would make the camera move start from a non-stock point. That is a presentation deviation, not a logic or determinism one, so "high" severity is overstated.
3. **The audio argument is weak.** On B, the draw already re-runs vt20, which calls 0x49B4A5 and with it TheAudio vt58, so stock-equivalent B already makes that call.
4. **The sun-rays term is pitch, not zoom.** m[11] is dist*2*pitch. The pitch is lerp(+0x13C to 0) by f(+0x134), or [0xDE3C08]+0x38 when +0x19 is set.

The recommended design is sound:
- With vt6C (GATE_GC_LWVIEW) and LW input (GATE_GC_LWXLAT) A-only, nothing writes the LW view state on B. B's vt20/0x49B4A5 is idempotent and rebuilds exactly M_k.
- Swapping only CameraClass +0x18 (48 bytes) and +0xD8 (28 bytes) around 0x518000 leaves every view field stock. The clip planes, including the dist-dependent zfar, are inside those 28 bytes.
- CamSwapEnd restores the camera exactly. camera.cpp's BeginSwap/CamSwapEnd already use this CameraClass layout: Set_Transform is vt54, +0xFC is the dirty byte.
CORRECTED REC: Keep all LW view-state fields stock and present the A-render halfway picture through the camera only:
- **Fields to leave alone:** +0xD8/+0xDC, +0xF8..+0x11C, +0x134, +0x13C, +0x198, and the target +0x110..+0x118.
- **Where to hook:** add one call_gate on the `call 0x518000` inside 0x49B618, which runs after 0x49B4A5 has run Set_Transform and set the clip and view planes.
- **What the stub does on a 60-mode A render (main thread, g_skipB==0):**
  1. Record M_k: camera = [view+0xC0], 48 bytes at +0x18 and 28 bytes at +0xD8, keyed by renderId and camera pointer. This needs its own record, separate from TacticalCamera().
  2. If M_{k-1} from the previous A render is valid, call InterpolateCameraHalfway(M_{k-1}, M_k).
  3. Call BeginSwap: Set_Transform, a raw 28-byte view-plane write, and cam+0xFC=0. The shadow refit is skipped because TerrainVisual is absent on the LW map.
  4. Call 0x518000.
  5. Call CamSwapEnd right after it, before the APT/UI part of the LW draw. SceneRestore at 0x44A271 stays as the backstop.
- **B and 30 mode:** both get the plain call. B needs no swap, because B's vt20 rebuilds exactly M_k from the unchanged view state, provided vt6C, LW input and GATE_LWM stay A-only.
- **Optional sun-rays lerp:** also lerp the LM_SunRays [view+0x188] m[11] (dist*2*pitch; x/y are the constants 192.43/569.88) between the k-1 and k values, saving and restoring it around 0x518000 the same way.
- **Accepted limits:** the fade layers +0x18C/+0x190/+0x194 and the LW armies still step at 30 Hz.
- **Why not write halfway values into the view fields:** that would leave halfway-derived +0x110..+0x118 behind whenever the next draw is skipped (0x449D1E), and the TheLivingWorldLogic defeat handler 0x6BAEB6 reads them via vt70 at 0x6BAF6F to start a camera moveTo. That is a presentation deviation, not logic or RNG, so rate the finding medium rather than high.

## VERIFY FINDING The existing InterpolateCameraHalfway would cut on every LW zoom frame (far plan -> confirmed
The finding is correct. I checked it against the binary (capstone disassembly of the scratch copy, which is identical to <game folder>\rotwk\game.dat) and against <game folder>\aotr60\src\camera_math.cpp, camera_math.h and camera.cpp.

1. **Where the camera is built.** FUN_0049B4A5 builds the LW camera. Its callers include the LW draw FUN_0049B618, so it runs on every LW draw.
2. **Camera distance.** At 0x49B4F5..0x49B549 it computes dist = [esi+0xD8] + ([esi+0xDC] - [esi+0xD8]) * f(0.5*(z*z+z)) and stores it at [ebp-8]. Here z is the zoom at [esi+0x134] and f is 0x49A4D4.
3. **Clip planes.** At 0x49B592..0x49B5B8 it calls 0x5337C0 with znear = [0xBD83D8] = 10.0 and zfar = dist*[0xBDE8C8] (1.5) + [0xBDE8C4] (2000.0).
   - 0x5337C0 is the CameraClass clip-plane setter. It writes camera+0xEC (znear) and camera+0xF0 (zfar) and clears +0xFC.
   - camera.cpp records vp from kCamViewPlane = 0xD8: min.x, min.y, max.x, max.y at 0xD8..0xE4, then aspect at 0xE8, znear at 0xEC, zfar at 0xF0. So zfar is vp[6].
4. **View plane.** 0x533590 is then called with hfov 0.872665 rad (50 degrees) and vfov -1, so the extents come from the fixed hfov and the aspect at camera+0xE8. Aspect, znear and the extents stay constant while zooming.
5. **Zoom changes gradually.** At 0x49AB21/0x49AB29 (the LW view tail, inside vt6C, which is A-only) the zoom +0x134 is incremented by the velocity at +0x138. A zoom is therefore a run of updates where +0x134 changes every update, and dist and zfar change with it.
6. **The cut check.** InterpolateCameraHalfway starts with memcmp(&a.vp[4], &b.vp[4], 12) and returns false when they differ. Every pose pair during a zoom differs in vp[6], so it is treated as a cut. The A-render then falls back to the unsmoothed picture and zoom stays at 30 Hz steps.

**Notes:**
- LwCamAfterBuild does not exist in the repo yet. The finding is about the hook this design proposes, and the current code would behave as described if that hook reuses the existing function.
- Taking max(zfar) is the safe choice (no far-plane popping on the halfway frame). Because zfar is linear in dist, the average of the two zfars is also exactly what stock would compute for the halfway dist, if dist is interpolated linearly. Either is acceptable. Keep requiring equal znear: it is the constant 10, and a changed znear would point to a real camera change.
CORRECTED REC: Add an LW-only option to CameraCutLimits, e.g. `bool allowFarChange = false`, and set it only from the new LW camera hook (LwCamAfterBuild).

When the option is set:
- Compare only aspect and znear (vp[4], vp[5]) byte-exactly. Both are constant on the LW camera (aspect from camera+0xE8, znear 10.0).
- Let vp[6] (zfar) differ, and set mid.vp[6] = max(a.vp[6], b.vp[6]). The average is also valid, since stock zfar = 1.5*dist + 2000 is linear in dist.
- Keep the 5% extent rule for vp[0..3], plus the existing distance and angle limits. The extents come from the fixed 50 deg hfov and the aspect, so they do not change while zooming.

Leave the tactical path unchanged: allowFarChange stays false, so the memcmp over all three values still applies.

Add a unit test in tests/test_camera_math.cpp:
- A zoom pair whose zfar differs (e.g. 2600 vs 2750) and whose translation moves along the view axis should interpolate, with mid zfar equal to the max (or average).
- The same pair with allowFarChange=false should still cut.
- A pair with a different znear or aspect should still cut.

## VERIFY FINDING LW swaps must not do the shadow refit -> confirmed
I checked this against <game folder>\aotr60\src\camera.cpp and game.dat. The finding is real, but it is a rule for code that does not exist yet. Today BeginSwap has only two callers, CamSwapToMk_B and SceneOpen_A, and both swap the tactical camera. No LW camera swap exists yet, so nothing is broken right now.

What the code shows:
1) BeginSwap (camera.cpp:71-92) calls ShadowRefit(cam) and sets g_save.refit=true whenever RefitWanted() is true. RefitWanted() only checks that [0xDC78EC] (TerrainVisual) and [0xDC7A38] (ShadowManager) are non-null and that GD+0x62 is set. It does not check which camera is being swapped. CamSwapEnd (lines 149-162) refits again with the restored camera only when g_save.refit is set.
2) 0x47D37D is thiscall(sm, cam). It runs: mov ecx,[esi+4]; push cam; call 0x534AD0, which copies the whole CameraClass into the shadow manager's own camera at sm+4. Then it runs mov ecx,esi; call 0x47C276, which does the fit and is itself gated on GD+0x62 and TerrainVisual. So the refit changes persistent shadow-manager state.
3) In stock, 0x47D37D has exactly two callers: 0x48B7B1 and 0x48BCF2. Both are W3DView methods and both pass the tactical view's own camera, view+0x104 (viewB4+0x50). The LW draw 0x49B618 renders the scene [lwview+0xC4] with a different camera object, [lwview+0xC0], through 0x518000. That camera never reaches 0x47D37D. drawFrame also skips updateViews on the LW map, so in stock the shadow manager is never fitted to the LW camera.
4) The only writes to 0xDC78EC and 0xDC7A38 are at 0x465F52, 0x46C349, 0x491512 and 0x491A34 (TerrainVisual) and 0x499F96 and 0x49A2B7 (ShadowManager). I did not identify those writers, but the gate reads them as persistent objects rather than per-battle ones. If that holds, RefitWanted() can be true on the LW map whenever shadows are on.

So a future LW camera swap through the current BeginSwap would copy the LW camera into sm+4 and refit, and CamSwapEnd would refit again with the real LW camera. That leaves the shadow manager fitted to the LW camera, which stock never does. Any later render or battle that reads that shadow state would then differ from stock.
CORRECTED REC: Any LW camera swap must not touch the shadow manager. Add an explicit `bool refit` parameter to BeginSwap. CamSwapToMk_B and SceneOpen_A (tactical camera) pass RefitWanted(); every swap of the LW camera ([lwview+0xC0], the camera 0x49B618 passes to 0x518000) passes false. When refit is false, BeginSwap must leave g_save.refit=false; CamSwapEnd already skips ShadowRefit in that case, so it needs no change. As an extra guard, also skip the refit when cam != TacticalCamera(), since stock only fits view+0x104 (0x48B7B1/0x48BCF2). Record this in the phase 6 notes as a rule for the LW camera work; no current code needs to change because no LW swap exists yet.

## VERIFY FINDING Readers of the LW camera outside the draw must see exactly stock M_{k-1}/M_k -> refuted
Checked against the game.dat disassembly and the aotr60 sources. The listed readers do exist, but the finding's main claim is wrong: no logic or RNG result depends on the LW camera.

1) Eye tower 0x7FB1DF, called by tail jump from 0x7FB3F0. It calls [0xDE4958]->vt38 with output pointer lea edx,[esp+8], a 12-byte local made by sub esp,0xC. The local is never read again. The code goes on to copy +0x5C/+0x60 into +0x24/+0x28 and calls 0x7FAE32 and 0x7FAB71(edi=esi+0x24); neither receives the local.
   - The pick 0x49A7A1 writes only to [edi], the caller's local. It reads the camera via 0x53B370 on [esi+0xC0].
   - Its helper 0x6BFE7C hides subobjects with vt194 (0x6BF5BB(…,0) and 0x6BFAEE), ray-casts with vtF0, then un-hides them with 0x6BF5BB(…,1). It has no persistent logic effect.
   - The eye tower's logic-RNG draws (0x7FB28C, 0x7FB2BB, 0x7FB324) depend only on its own state machine and how many times it runs. That count is already controlled by GATE_LWM (A-only).
   - So a leaked or interpolated camera cannot make the eye tower pick differently or desync LW logic.
2) 0x6110BE (LWM, at 0x6110F8). It only runs when byte[this+0x2D0]!=0. It formats the picked x/y as integers into a string and sends it to [0xDE36E0] (0x5EE30E). That is a debug position text, presentation only.
3) Audio. 0x450A60 really does reach LW vt80 0x49A6B9 and Get_Position 0x53B370 when TheAudio+0x678==1. That is the listener position, presentation only.
4) The backstop is already there. CloseWindowsSafetyNet in <game folder>\aotr60\src\frame_ctl.cpp (around line 170) already logs and calls CamSwapEnd if g_swapActive is still set at C0. SCENE_RESTORE (0x44A271) is the existing restore call that every drawFrame exit runs.
   - Note: LW6_CAM_SCENE_END is not in the repo; it is a name from the proposed phase-6 design.
5) The description is also imprecise. The LW translator 0x8392A7 -> vt2C 0x49B799 only updates view state (+0xF8/+0xFC position, +0x11C angle, vt58). It does not write the W3D camera. So "the B draw rebuilds M_k" is not established by the cited evidence.

What stands: ending any LW camera swap right after the LW scene draw keeps audio, debug text and pick readers identical to stock. That is worth doing, but at low severity and for presentation fidelity only, not determinism.
CORRECTED REC: Lower the severity to low and drop the determinism claim. The eye tower's vt38 pick (0x7FB1E6 -> 0x49A7A1) writes into a stack local that is never read, so the camera cannot affect its logic-RNG use. 0x6110F8 (debug position text, only when +0x2D0 is set) and the audio listener (0x450A60 -> LW vt80 0x49A6B9) are presentation-only.

Phase-6 changes:
- End any A-side LW camera swap right after the LW scene draw (display vt20 0x49B618 at 0x449F45) and before propagate/audio (0x6324A1/0x6324F9), so that audio and the debug text read the stock camera. Keep SCENE_RESTORE (0x44A271) and the existing CloseWindowsSafetyNet at C0 (frame_ctl.cpp) as the backstops. A new assert is optional, because the safety net already logs a leaked swap.
- For the determinism gate, keep counting eye-tower logic-RNG calls (0x6D328E from 0x7FB28C/0x7FB2BB/0x7FB324) per stock frame. They must equal stock because of GATE_LWM (A-only run count), not because of the camera.

## VERIFY FINDING Cut rules for LW camera jumps -> confirmed
The finding mostly holds up against the binary (game.dat, checked with dis.sh, ghq and a raw vtable scan). It has two labelling errors and leaves out where the LW camera matrix is actually built.

Confirmed:
- moveTo, LW view vt9C (0xBDE918+0x9C = 0x6BFB88): stores the start pose through vt74/vt78, sets +0x7C = 1/frames, +0x74 = 0 and +0x78 = 1. It also stores the target at +0x60..+0x68, the angle at +0x84 and the zoom target clamped to [0,1] at +0x70.
- The flight step 0x6BF78E is reached only through LWM 0x6C0E4D (jmp at 0x6C0E86). Its only direct caller is LW view vt6C 0x49AAB8 at 0x49AAD4, so it runs once per A-render (GATE_GC_LWVIEW). Each call adds +0x7C to t (x10 when fast-forward is on: 0x63F122 and GD+0x88), eases t through 0x90AAEF on +0x28, then sets zoom via vt68, position via 0x6BF678 + vt58 and angle via 0x6BF657 + vt7C.
- 0x6BF96D computes the flight frame count: max(distance / LWM+0x1E0, |zoom difference| / LWM+0x1D8 * 0.0333), clamped to [0, LWM+0x1E4], times FPS30.
- 0x6BF878 is an instant pose set: vt58 0x49AA24 writes +0xF8/+0xFC/+0x100 (clamped to +0x124..+0x130 when TacticalView vt20 returns true), and vt7C 0x49A6A8 writes the angle at +0x11C.
- Setters 0x49A9A6 (vt68, zoom +0x134) and 0x49A6A8 (vt7C, angle +0x11C) have no smoothing of their own.
- (b) setSuspended vt50 0x49BE23: when suspending it copies LWM+0x34/+0x2C/+0x30 into +0x134/+0xF8/+0xFC and calls 0x49B4A5. When unsuspending it restores the camera value at +0x1AC through 0x533630 on +0xC0 and calls vt1C.
- (d) DelayedSplineCamera (vtable 0xC14674, slot +0x14 = 0x7FEE5B) is called from 0x6B96C4 <- TheLivingWorldLogic::update 0x6BE50E. That update is called at 0x632A8A only when sub == 1, so 5 Hz. Each call does frame+1 (+9 more under fast-forward), samples the spline and calls 0x6BF878. 0x6B96C4 only runs while view+0x14 == 1.
- Fade blend: 0x49AF11 lerps the target toward +0x1C/+0x20 and the angle toward 0 by the factor +0x198. vt6C advances +0x198 by [0xDCB840] per A-render (state 2 up, state 3 down), so the blend is continuous. When state 2 completes (0x49AC29) it calls vt4C, vt28(0) and vt84.
- Zoom inertia (0x49AB21/0x49AB3E: +0x134 += +0x138, then +0x138 *= LWM+0x1DC) runs per A-render.
- 0x625E20 has only two callers, 0x626662 and 0x925699, and both are battle-exit / return-to-map paths.

Errors in the finding:
- (a) 0x6BF878 does not write +0x1C/+0x20. 0x6B33FB writes them itself (0x6B3470/0x6B3481) after calling 0x6BF878 with (x, y, angle 0).
- (c) 0x6BF8BB is not activation. It calls vt28(0), which is setActive(false) (0x49D565 -> 0x6BFF7B clears +0x18 and calls vt50(0)), then vt4C(1,1), vt1C and teardown calls, and ends with vt18.

Missing from the finding:
- The LW camera matrix is not built in vt6C. It is rebuilt on every draw: vt20 0x49B618 -> 0x49B4A5 -> 0x49AF11, which sets the transform via 0xB26320 on camera +0xC0 and sets viewport/clip via 0x5337C0/0x533590. This path runs on B too, so B automatically shows M_k. The halfway pose has to be recorded and swapped around that build on the A draw.
- 0x49AF11 also passes an angle-derived matrix to [+0x188]->vt54. That object follows only the view state, not the swapped camera.

No other instant pose setter was found: 0x6BF878 has only the two callers above, and the only other pose-related LW-view virtual call near loads of 0xDE4958 is vt9C (moveTo) at 0x6BAFA3 (heuristic scan; 0x838D5D vt74 is a pose read).

The large teleport, 0x6B33FB, happens on battle exit. That path goes through GameEngine::reset, which flushes the camera history, so the distance threshold is only a fallback. The cut predicates themselves are sound: a non-contiguous history, a change in +0x14/+0x18/+0x19, an aspect or extent change, or a distance/angle limit.
CORRECTED REC: Implement the LW map like the tactical camera.
- Record the camera pose (+0xC0 transform and viewport) after 0x49B4A5 builds it inside the LW draw vt20 0x49B618, keyed by renderId and the view/camera pointers.
- On A-renders, present the halfway pose only around that draw and restore it at the end of the draw. Leave B unmodified: B rebuilds M_k from state that only changes on A.
- Treat the angle matrix passed to [+0x188]->vt54 the same way, or accept that it shows stock pose k.

Present M_k without interpolation when any of these holds:
- the A history is not contiguous (renderId, view or camera pointer changed);
- int[view+0x14], byte[view+0x18] (active) or byte[view+0x19] (suspended) differ between the two records;
- the aspect differs or an extent changes by more than 5%;
- the eye translation exceeds LwCamCutDist (start at 1500, then tune from counted cuts) or the rotation exceeds 45 degrees.

Correct the event list:
- (a) 0x6B33FB calls 0x6BF878 with (x, y, angle 0) and then writes +0x1C/+0x20 itself. It is reached only from the battle-exit paths 0x626662 and 0x925699, which go through reset and flush the history.
- (c) 0x6BF8BB is deactivation (vt28(0) clears +0x18 and calls vt50(0)), not activation.
- (d) DelayedSplineCamera's 0x6BF878 poses below the cut limit are halved on A at 5 Hz, which is acceptable because stock already shows them as 5 Hz steps.

Smooth paths that need no cut:
- moveTo (once per A-render through vt6C/0x6C0E4D, x10 under fast-forward);
- zoom inertia at 0x49AB21/0x49AB3E;
- the +0x198 fade blend.

## VERIFY FINDING SceneOpen_A misbehaves harmlessly on the LW map -> confirmed
The finding is real but only matters for the future. Today `BlockReason` (src/frame_ctl.cpp:103-116) refuses 60 mode when GL+0x110 is 8 or GL+0x125 is set, so `g_m60` stays 0 on the LW map. CAVE_SCENE_OPEN (stubs.asm:935-948) then takes the stock path and `SceneOpen_A` never runs there now. The problem appears as soon as phase 6 lifts that block. "Medium" overstates it: the only proven effect is a telemetry counter.

What I checked in the binary (dis 0x449CF0..0x449FB0):
- At 0x449D29, when GL+0x125 is set, drawFrame skips vtA0 updateViews and the call at 0x449D3B.
- The branches at 0x449D53, 0x449D61, 0x449D69 and 0x449D9E, plus the fall-through after the call at 0x449DA6, all reach 0x449DAB. That is the SCENE_OPEN site, so every non-load-screen drawFrame passes it, the LW one included. The LW display vt20 call at 0x449F45 comes later.
- `SceneOpen_A` (camera.cpp:244-301) first returns only on the load screen (GD+0xAF6 == 1). Otherwise it calls `OpenWindow(g_pw2Open)`. That saves GE+0x3C into `g_fracSaved`, writes `g_presFrac` there and sets `g_pwActive` to 1. `SceneRestore` undoes this through STUB_SCENE_RESTORE at 0x44A271.

No camera swap happens on the LW map, as the finding says:
- Camera records are written only by `S2_RecordMkAndOpen`. Its site 0x48C701 is inside W3DView::update (FUN_0048BCF2), which runs from updateViews.
- updateViews is skipped on the LW map, and the LW draw does not reach that site. FUN_0049B618 calls 0x51CCD0, 0x49A4FB, 0x49B4A5 and 0x518000, and draws with its own camera at +0xC0 through vt+0x54 in 0x49B4A5.
- So `g_rec[1].renderId` never equals `g_renderId`. With `g_featCamInterp` on and no swap active, `SceneOpen_A` increments `aNoHistory` on every A-render (or on the `!cam` branch if the tactical view has no camera). It never calls `BeginSwap`.
- `aNoHistory` is printed by telemetry.cpp:219, so the "stats polluted" claim holds.

Caveats:
1. "Harmless" is only shown for the camera part. For the fraction window I found no reader of GE+0x3C in 0x49B618 or 0x49B4A5. I did not audit the W3D scene render (0x51CCD0, 0x518000) or the UI/APT draw. While `g_pwActive` is 1 it also changes the C4 key (stubs.asm:252-262) and the C5 path. That is inert on the LW map only because RenderViews and drawables do not run there, which is consistent with the code above but not exhaustively proven.
2. `g_lwRec`, LW6_CAM_REC and LW6_CAM_SCENE_END do not exist in the repo (grep finds nothing). Those parts of the recommendation depend on the proposed LW-camera design, not on current code.
CORRECTED REC: Severity: low. It is only telemetry pollution plus a presentation window nothing needs, and none of it can happen until phase 6 lifts the LW block in `BlockReason`.

Fix for phase 6: at the top of `SceneOpen_A` (src/camera.cpp:244), after the load-screen check, return when TheGameLogic ([0xDE412C]) is non-null and byte[GL+0x125] != 0. Return before `OpenWindow`, not just before the camera part. On the LW map nothing consumes the half-step fraction: drawables and RenderViews do not run, and the LW draw 0x49B618 does not read GE+0x3C. Skipping the window keeps the A-render exactly stock render k, as PLAN 1.2 requires, and also stops the bogus `aNoHistory` increments. `SceneRestore` already does nothing when `g_pw2Open` is 0 and no swap is active, so no other change is needed. If you want certainty, check that the W3D scene render 0x51CCD0/0x518000 and the LW UI draw do not read GE+0x3C before relying on any LW fraction window.

If the proposed LW camera design is implemented (`g_lwRec`, LW-specific record and scene-end sites):
- `CameraReset()` should also invalidate `g_lwRec`.
- Phase-6 `BlockReason` should allow mode 8 / GL+0x125 only when those LW camera sites, and the other phase-6 gates, are installed.

Neither of these exists in the code today. They are design requirements, not fixes to current code.

## SITE {
 "id": "LW6_CAM_REC",
 "phase": "6",
 "group": "LW strategic map camera",
 "address": "0x49B77E",
 "length": 5,
 "original_hex": "e8 22 fd ff ff",
 "original_asm": [
  "0x49B77E: e8 22 fd ff ff  call 0x49b4a5"
 ],
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:STUB_LW6_CAM_REC>",
 "stub": "STUB_LW6_CAM_REC (call-boundary stub).\nEntry: [esp]=0x0049B783; ECX=ESI=LW view (vtbl 0xBDE918; 0x49B77C 'mov ecx,esi'); EBP=0x49B618 frame; x87 empty; no stack args.\n\n  cmp byte ptr [g_m60],0 / je stock\n  cmp byte ptr [g_inClientUpdate],0 / je stock\n  IS_MAIN eax / jne stock              ; EAX is dead at entry\n  call dword ptr [T_49B4A5]            ; original camera build: thiscall ECX=view, plain ret, x87 balanced\n  RUNCNT IDX_LW6_CAM_REC\n  SAVE_ALL\n  push esi                             ; LW view\n  call LwCamAfterBuild                 ; cdecl\n  add esp,4\n  RESTORE_ALL\n  ret                                  ; -> 0x49B783\nstock:\n  jmp dword ptr [T_49B4A5]             ; exact original call; the return address 0x49B783 is already on the stack\n\nLwCamAfterBuild(view):\n- cam = [view+0xC0]; return if NULL.\n\nA-render (!g_inB):\n- If lw[1].renderId != g_renderId, then lw[0] = lw[1].\n- lw[1] = {view, cam, xf = 48 bytes at cam+0x18, vp = 28 bytes at cam+0xD8 (min.x, min.y, max.x, max.y, aspect +0xE8, znear +0xEC, zfar +0xF0), st = int[view+0x14], act = byte[view+0x18], sus = byte[view+0x19], renderId = g_renderId, valid = 1}.\n- Return if !g_featCamInterp or g_swapActive.\n- Require lw[0].valid, lw[0].renderId == g_renderId-2, same cam, same view. Otherwise ++lwNoHist and return.\n- If st/act/sus differ: ++lwCut, return.\n- If xf and vp are identical: return.\n- mid = InterpolateCameraHalfway(lw[0], lw[1]) with LW limits:\n  - aspect must be equal;\n  - extents may change by at most 5%;\n  - near/far may differ: mid.znear = min, mid.zfar = max;\n  - eye distance <= LwCamCutDist (start at 1500, tune via telemetry);\n  - rotation <= 45 degrees.\n  On failure: ++lwCut, return.\n- BeginSwap(cam, mid.xf, mid.vp, refit=false); ++lwSwapA.\n\nB-render (g_inB, i.e. g_skipB):\n- r = lw[1]. Require r.valid, r.renderId == g_renderId-1, r.cam == cam. Otherwise ++lwBNoRec and return.\n- If cam+0x18 and cam+0xD8 equal r.xf/r.vp: ++lwBExact (expected case: the rebuild from unchanged state already equals M_k).\n- Otherwise: ++lwBMismatch; log view+0xF8..+0x138 once; if !g_swapActive, BeginSwap(cam, r.xf, r.vp, refit=false).\n\nBeginSwap(refit=false): save cam+0x18/+0xD8; cam->vt54(xf) (Set_Transform 0x533550); memcpy(cam+0xD8, vp, 28); byte[cam+0xFC]=0; g_swapActive=1; never call 0x47D37D.\n\n30 mode: the stock path is identical to the original call.",
 "resume_address": "0x49B783 (return address of the original call)",
 "live_after": "ESI = LW view (read by 0x49B783 'push [esi+0xC0]' and 0x49B789 'push [esi+0xC4]'). EBP = 0x49B618 frame (leave at 0x49B797). EBX/EDI hold drawFrame values: callee-saved and preserved. ESP balanced (0x49B4A5 has no stack args). Dead: EAX, ECX, EDX (0x49B783..0x49B78F reloads; 0x518000 is cdecl), EFLAGS, XMM (nothing in 0x49B783..0x49B798 or drawFrame 0x449F48.. reads XMM before writing it). x87 depth 0 (0x49B4A5 pairs every fld with fstp).",
 "branch_into_span_check": "Interior 0x49B77F..0x49B782: no target found by rg in listing.asm. A brute-force rel8 (EB/7x/E0-E3), rel32 (E8/E9) and jcc32 (0F 8x) scan over every executable section (.text, stxt774, stxt371, .mackt, .danetta, .angmar) found 0 hits, and the abs32 dword scan of the whole image found 0. The span start is the fall-through of 0x49B77C 'mov ecx,esi'. Bytes are identical in rotwk\\game.dat, aotr\\zGameDats\\delayfix.dat and rotwk\\game820.dat. No overlap with AotR hooks or existing sites; the nearest site is GATE_LWM 0x49AAD4.",
 "purpose": "Record point for the LW strategic-map camera. It sits after every A-only camera step (LookAt 0x6484B2, LW translator 0x6484BD, LW view vt6C 0x6484D9 incl. moveTo 0x6BF78E) and after the per-draw camera build 0x49B4A5, and before the LW scene render 0x49B78F.\n- A-render: record M_k and present the halfway camera lerp/slerp(M_{k-1}, M_k) for the LW scene only.\n- B-render: verify that the rebuilt camera equals M_k; swap to M_k only if it does not.",
 "evidence": "drawFrame LW branch: 0x449F2F..0x449F45 calls [0xDE4958]->vt20 = 0x49B618 when view+0x18 && !view+0x19.\n\n0x49B618 does: clear 0x51CCD0; derives +0x13C/+0xD8 from fade +0x198 and the livingworld.txt tables 0xDCB86C/0xDCB878; sets zoom alphas 0x49A4FB; scene vt18; 0x49B77E call 0x49B4A5; WW3D::Render 0x518000(scene [esi+0xC4], cam [esi+0xC0]) at 0x49B78F.\n\n0x49B4A5 does:\n- dist = lerp(+0xD8, +0xDC, f((z^2+z)/2)) and pitch = lerp(+0x13C, 0, f(z)) via 0x49A4D4;\n- cam Set_Clip_Planes 0x5337C0(10, dist*1.5+2000);\n- Set_View_Plane 0x533590(0.872665, -1);\n- 0x49AF11 (look-at matrix via 0xB26320 from pos +0xF8, angle +0x11C, zoom +0x134; writes target +0x110..+0x118 and the LM_SunRays transform +0x188);\n- cam->vt54 Set_Transform 0x533550 (copies to cam+0x18, zeroes +0xFC);\n- TheAudio vt58 0x450A08, which only sets the listener-dirty byte +0x6AB.\n\nCamera allocated by 0x534570 (CameraClass ctor, vtbl 0xBE8748) at 0x49D298. Set_Clip_Planes writes +0xEC/+0xF0.",
 "risks": "- Every A swap must be ended before drawFrame returns. The eye tower (0x7FB1DF vt38 pick at 0x7FB1E6, under the logic-RNG-consuming LWM vt28) and LWM 0x6110F8 read the LW camera in the next A vt6C, and the audio listener (TheAudio vt1A4 0x450A60 -> LW vt80 0x49A6B9) reads it in the A-only audio update. Both must see the real M_{k-1}/M_k. LW6_CAM_SCENE_END plus the SCENE_RESTORE backstop guarantee this.\n- Shadow refit must stay off.\n- On A frames, LM_SunRays height (+0x188 m[11]) and the zoom alphas (+0x18C/+0x190/+0x194) remain at S_k. Visual only.\n- Embedded APT-window draws (vt24 0x49BEFD) do not pass here.",
 "dll_vars": [
  {
   "name": "g_lwRec[2]",
   "ctype": "struct LwCamRec {uint8_t* view; uint8_t* cam; float xf[12]; uint32_t vp[7]; int32_t st; uint8_t act, sus; uint32_t renderId; bool valid;}",
   "note": "[1] = M_k of this A-render, [0] = M_{k-1}; cleared by CameraReset() on mode switch and RESET"
  },
  {
   "name": "g_swapActive / g_save",
   "ctype": "uint8_t / SwapSave",
   "note": "shared with camera.cpp; LW swaps use refit=false"
  },
  {
   "name": "g_featCamInterp",
   "ctype": "uint8_t",
   "note": "0 => A presents M_k (30 Hz camera steps, still exact speed)"
  },
  {
   "name": "LwCamCutDist",
   "ctype": "float",
   "note": "eye-translation cut, default 1500 world units; tune from lwCut telemetry (LW map Extent 4200x3450 in livingworld.ini)"
  },
  {
   "name": "g_lwStats",
   "ctype": "struct {lwSwapA, lwNoHist, lwCut, lwBExact, lwBNoRec, lwBMismatch, lwEnds}",
   "note": "lwBMismatch must stay 0; nonzero means a B-path pose mutation (gating hole)"
  }
 ]
}

## SITE {
 "id": "LW6_CAM_SCENE_END",
 "phase": "6",
 "group": "LW strategic map camera",
 "address": "0x49B78F",
 "length": 5,
 "original_hex": "e8 6c c8 07 00",
 "original_asm": [
  "0x49B78F: e8 6c c8 07 00  call 0x518000"
 ],
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:STUB_LW6_CAM_SCENE_END>",
 "stub": "STUB_LW6_CAM_SCENE_END (call-boundary stub).\nEntry: [esp]=0x0049B794; [esp+4]=SceneClass* (from 0x49B789 push [esi+0xC4]); [esp+8]=CameraClass* (from 0x49B783 push [esi+0xC0]). 0x518000 is cdecl: the caller pops 8 bytes at 0x49B794/0x49B795. It returns AL.\n\n  cmp byte ptr [g_swapActive],0 / je stock      ; 30 mode / no swap: exact original\n  IS_MAIN_KEEP_EAX / jne stock\n  push dword ptr [esp+8]        ; camera\n  push dword ptr [esp+8]        ; scene\n  call dword ptr [T_518000]     ; WW3D::Render(scene, camera) with the presented camera\n  add esp,8\n  SAVE_ALL\n  call LwCamSceneEnd            ; cdecl: if g_swapActive && g_save.cam == lw[1].cam, run CamSwapEnd() (Set_Transform(saved xf), raw view plane, +0xFC=0, no refit) and ++lwEnds\n  RESTORE_ALL                   ; keeps EAX (AL = render result)\n  ret                           ; -> 0x49B794 with the two original args still on the stack\nstock:\n  jmp dword ptr [T_518000]",
 "resume_address": "0x49B794 (return address of the original call; 'pop ecx; pop ecx' cleans the 2 args)",
 "live_after": "ESP layout: the two cdecl args stay on the stack for 0x49B794/0x49B795. EBP (leave at 0x49B797). ESI is restored from the stack at 0x49B796, so its current value is dead, but it is preserved anyway. EBX/EDI hold drawFrame values: preserved. EAX = AL render result: not consumed (0x49B618 is void and drawFrame 0x449F48 reloads EAX) but preserved. EFLAGS dead. x87 depth 0 (0x518000 does not touch the x87 stack). XMM dead.",
 "branch_into_span_check": "Interior 0x49B790..0x49B793: no target found by rg in listing.asm. The brute-force rel8/rel32/jcc32 scan over all executable sections found 0 hits, and the abs32 scan found 0. Bytes are identical in all three builds (game.dat, delayfix.dat, game820.dat). 0x518000 = WW3D::Render(scene, camera): it reads arg1 at [esp+0x158] and arg2 at [esp+0x15C] after its 0x154-byte frame, so re-pushing both args in the stub is exact.",
 "purpose": "Ends the LW camera swap (A: halfway camera; B fallback: M_k) right after the LW scene render, so only WW3D::Render sees the presented camera. The InGameUI/APT/mouse draws, the post-draw propagate (translator picking and region picks via 0x6C0E8D), the A-only audio update and the next A's eye tower all see the real camera, exactly as in stock. The existing SCENE_RESTORE 0x44A271 stays the backstop for any other drawFrame exit.",
 "evidence": "0x49B783..0x49B798 disassembly. The drawFrame LW branch after vt20 (0x449F48..0x449FA4, then 0x44A0AD..0x44A26D) draws only UI/mouse/overlays before Present and SCENE_RESTORE 0x44A271. Region and army visuals are 3D objects of the LW scene [view+0xC4] (livingworld.ini LivingWorldAnimObject/banners), so ending the swap before the UI loses nothing visible. CamSwapEnd in camera.cpp already restores via vt54 + raw view plane + +0xFC=0.",
 "risks": "- If a future audit finds a UI overlay that projects with the LW camera (LW vt3C 0x49A899), that overlay will show M_k on A frames: a half-step offset, visual only.\n- LwCamSceneEnd must not end a tactical swap. On the LW map none exists: updateViews is skipped at 0x449D29, so S1/S2 never run.",
 "dll_vars": [
  {
   "name": "g_swapActive",
   "ctype": "uint8_t",
   "note": "0 after this stub on every LW draw"
  },
  {
   "name": "g_save",
   "ctype": "SwapSave",
   "note": "shared with camera.cpp"
  },
  {
   "name": "g_lwStats.lwEnds",
   "ctype": "uint32_t"
  }
 ]
}