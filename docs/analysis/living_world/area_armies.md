# AREA lw-armies

## SUMMARY
I traced how armies, settlements and region objects move and animate on the Living World strategic map. Armies are moved by client-side code once per render, not by the LW logic: the logic sets the destination at once, and the client army object then walks a fixed 20 world units per render along its waypoint list. The logic reads the client's moving flag and remaining waypoints to end the movement phases (turn phases 1/5), and save games store those waypoints. So army movement must keep exactly one step per A-render. Converting it to half-steps on both renders would change logic.

Today, with GATE_GC_LWVIEW and GATE_LWM in place, nothing in this area advances on B. The B-render's LW scene draw (0x49B618, plus army lines and W3D animations on the C3 clock) only redraws from current state. No LW code reads the GE+0x3C fraction or any time source; settlement and icon animations, region pulses and region effects are all frame counters stepped once per update.

**Design for smooth 60 FPS (presentation only, logic untouched):**
1. **Snapshot, new site LW_ICON_SNAP at 0x6C0E6B.** On the 60-mode A-render, just before the object update call 0x6C038B, the stub records the transform of every icon render object. It walks the live LW client hash at [0xDE4958]+0x98, takes each object's icon list (+0x2C..+0x30) and the two render objects per icon (+0x08 and +0x14). It only reads memory.
2. **Present and restore, new site LW_SCENE_PRESENT at 0x449F43.** On A-renders the stub wraps the LW scene draw call (vt20, 0x49B618). Before it, it re-walks the live hash and sets each changed render object to the midpoint between its snapshot and its current transform: translation always, the 3x3 part only if the change is small, nothing beyond 400 units. It uses the object's own Set_Transform (vt54). After the draw it restores the exact current matrices.

Result: B shows stock render k and A shows k-½, the same constant 16.7 ms delay as battle units. The same Open/Close window can carry the LW camera half-step so armies and camera don't jitter against each other. Both span byte strings match across game.dat, delayfix.dat and game820.dat. A brute-force branch scan, an abs32 scan and a listing grep found nothing branching into either span, and neither overlaps an AotR hook or an existing site.

**Other findings:**
- **Logic reads the LW camera (camera module):** each LW tick checks the zoom value (+0x134) and fly position. The zoom integrators at 0x49AB21/0x49AB3E must keep A-only, stock-exact steps, and only the presented picture can be interpolated.
- **Naming in PLAN and sites.json:** 0x6BF78E/0x6BFB88 ("moveTo") are the LW camera fly, not army movement. GATE_LWM's description in sites.json should be corrected.
- **Accepted 30 Hz steps:** colour and alpha animations (pulses, selection fades, region-effect intensity) stay in 30 Hz steps.

## COVERAGE
My only writes are scratch helper scripts in <analysis workspace>/lw_tmp\armies\.

**Traced statically (Ghidra dump plus capstone, against the three builds):**
- The full army chain: the logic order (0x71BE8F), the client army class (vtable 0xC4E2E0, ctor 0x7FFCDA, update 0x7FFC0A/0x7FFB25/0x7FFA06/0x7FFAA3), and the icon classes (0xC84B18 army, 0xC8C4A8 building, 0xC8EC90 default; base 0xC8C818).
- Icon per-render functions 0x7FCF6C/0x7FD785/0x7FDADE/0x7FE2EB/0x7FD3E5.
- The LW client object hash and its iteration helpers 0x5E0A39/0x6BFEFD/0x83697E.
- The three client object kinds: army, building (via 0x8E4A54) and a named map object (vtable 0xC88818, via 0x9029DD; purpose not identified).
- Pulses 0x7FC09D, logic readers of client state, army save/load, army lines, and the LW draw path and scene render.
- The LivingWorldMapInfo parse table (base LWM+0x14) and AotR livingworld.ini values.

**Not traced:**
- The LW manager lists at +0x23C/+0x248/+0x220: anim rays, clouds and markers with particle systems, created at 0x6113F6. They run inside the A-only LWM::update.
- The internals of the region-effect instance update (vtables not identified). Only the parse-time frame conversion and the update loop 0x8E646C were traced.
- The LW translator 0x8392A7: whether a drag/move-preview icon follows the cursor.
- The writers of LW client +0x24/+0x25. Their logic reader 0x6B5BD3 was traced; it turns them into commands 0x6BA/0x6BB.
- The embedded APT window, the eye-tower internals, and Palantir.

**Not done:** no runtime test, so the stub designs are unvalidated in game. The 400-unit cutoff and the 0.25 limit for averaging the 3x3 part are chosen heuristics.

## FINDING [high] Army movement on the LW map is a client-side per-render integrator whose results the LW logic reads and saves
Strategic-map armies are not moved by TheLivingWorldLogic::update (0x6BE50E) or by the LW manager. The logic sets the destination at once and fills the army's CLIENT waypoint list. The client army object then walks a fixed distance per client update (per render). The logic reads the client's moving flag and pending waypoints to decide when the movement phase (turn phases 1 and 5) ends, and save games store the client waypoints. So the visual movement cadence is logic-visible. This answers the open G1 question and CRITIC U4: ungated 60 FPS would move armies 2x and end movement phases early, which changes logic.
EVIDENCE: Chain: 0x6484D9 vt6C 0x49AAB8 -> 0x49AAD4 (GATE_LWM) -> 0x6C0E4D -> 0x6C0E6D call 0x6C038B -> [node+8]->vt1C at 0x6C03A9 -> army client vtbl 0xC4E2E0 slot 0x1C (0xC4E2FC) = 0x7FFCCA -> 0x7FFC0A.

0x7FFC0A:
- Runs only when 0x6B56CC is true: LWLogic+0xF4 in {1,5}, +0x154==+0x158, and !0x6B4019.
- Movement start: pops a waypoint with 0x93F7FC into +0x50/+0x54, sets +0x5C=1 and plays the start sound (0x7FF8C3).
- Each step 0x7FFB25: pos(+0x18,+0x1C) += unit(target-pos) * min(dist, step), where step is client +0x58 (0x7FFA06). The step is x10 ([0xBD83D8]) when 0x63F122([0xDE4334]) and GD+0x88 are set.
- When dist < 1.0, 0x7FFAA3 takes the next waypoint or sets +0x5C=0.
- Height comes from 0x6C019A, then vt14 0x93FCCF -> icon vt1C 0x7FD3E5 -> RO vt54.

Speed: client+0x58 = army+0x5C (0x7FFDFF..0x7FFE06) = template+0x44 (0x71BD26) = LWM+0xF8 DefaultArmyMoveSpeed (0x8E77DB; AotR livingworld.ini 20.0). That is per render, not per second.

The logic order 0x71BE8F (called from LW logic 0x6B31A6/0x6B322D) builds the waypoints into the client vector (0x71A4BD = client+0x3C, 0x93F8C9) and sets the army destination +0x3C/+0x40 instantly.

Logic reads:
- 0x6E1DEE (any army 0x71A4CE = client+0x5C) <- 0x6B5600 <- phase machine 0x6BE20A (phase 1/5 advance).
- 0x6E1E34 (0x71A87D, client waypoints non-empty) <- 0x6B5646.
- 0x71A8DC/0x71A9F6/0x93F6A8/0x93F6D0 (client waypoints) used by 0x6B45C7, 0x6BD97E and 0x71AEDF.

Save/load: army xfer 0x71BFD9 copies the client waypoints (0x71B4C8) into army+0x7C and xfers them (0x71BA5E); load 0x71B928 restores them with 0x93F8C9.

No GE/0xDE4324 or sync-time reads anywhere in this chain.
REC: Keep the army update exactly one step per A-render. GATE_GC_LWVIEW (0x6484D7) and GATE_LWM (0x49AAD4) already do this, and GATE_LWM must stay even if phase 6 lets the 0x49AAB8 camera tail run on B.

Do NOT turn army movement into half-steps on A and B (G1 G-04 'phase 2' style). Two half-steps re-normalise the direction and round differently, so the dist<1.0 arrival and min(dist,step) can flip by one render. That shifts +0x5C, the waypoint pops, phase timing and the saved waypoints.

Get smoothness only from presentation (LW_ICON_SNAP + LW_SCENE_PRESENT).

Also rename GATE_LWM's 'LW moveTo 0x6BF78E' to 'LW camera fly'.

## FINDING [high] No interpolation fraction exists for LW objects; DLL-side transform interpolation between consecutive A-render updates is required
LW armies, buildings and map objects are not Drawables. Their drawn pose is the W3D render-object transform (RO+0x18) written by Set_Transform during the client update. Nothing in LW code reads the stepper fraction GE+0x3C or any time source except fixed per-call steps. Smoothing therefore has to record the transforms before the A-update and present the midpoint during the A-render's LW scene draw, then restore them. B-renders show stock render k unchanged. This matches the battle design: A shows k-1/2, B shows k, with a constant 16.7 ms delay.
EVIDENCE: refs to 0xDE4324/0xDD1E0C/0xDD1E10/0xDC7A8C across 0x49A000-0x49E000, 0x610000-0x614500, 0x6B0000-0x6C1200, 0x71A000-0x71C200, 0x7EF000-0x800200, 0x838000-0x83A000, 0x93F000-0x940500, 0x973000-0x974000, 0x9D3000-0x9D5000: none (the 0x64576D hit is outside 0x645750's 29 bytes). Frame-based constants only: 0x7FEA48 ms*[0xD9F60C]*0.001, 0x6BF96D (camera). Icon position 0x7FD3E5: vt50, replace translation, vt54 on [icon+8] and [icon+0x14]. RO layout: transform +0x18 (0x53B260), scene +0x78, container +0x7C.
REC: Implement LW_ICON_SNAP (0x6C0E6B) and LW_SCENE_PRESENT (0x449F43) as specified in the sites: snapshot on A before 0x6C038B, and lerp translation (plus the 3x3 when the change is small) only inside the vt20 LW draw call, with an exact restore. Walk the live LW client hash both times; never keep RO pointers across renders. Gate the feature on g_featPresent, and clear g_lwSnapRender in the reset hook and on mode switches. Let the LW camera module share the same Open/Close window so armies and camera have the same delay.

## FINDING [medium] Cross-area: the LW logic reads LW camera zoom and position, so the 0x49AAB8 tail integrators are logic-visible
At every LW tick the logic asks the LW client whether the camera is fully zoomed (zoom +0x134 against [0xDCB884]). Only then does it process its pending-event list +0xCC. It also reads zoom and camera position when it schedules camera flies. The zoom integrator in the LW view update (zoom += velocity, velocity *= damping) therefore has to have stock values at logic time, the same rule as the tactical camera.
EVIDENCE: 0x6B96C4 (called from LW logic update 0x6BE50E) -> 0x6B96F8 call [eax+0x60] = 0x49A991: returns ([0xDCB884] >= [client+0x134]). It gates the vt10/vt14 processing of LWLogic+0xCC and the 0x6B5C0B deletes. 0x6BAEB6 calls vt64 0x85D271 (fld [ecx+0x134]), vt70 0x49A64F (+0x110..+0x118) and vt9C 0x6BFB88 (camera fly). Integrators: 0x49AB21 addss [esi+0x134] += [esi+0x138]; 0x49AB3E [esi+0x138] *= [LWM+0x1DC] (MouseWheelZoomDampenFactor 0.82), zeroed below 0.001.
REC: For the LW camera module: keep 0x49AB21/0x49AB3E and the camera fly 0x6BF78E stepping only on A-renders, stock-exact (no B half-steps). Interpolate only the presented camera inside the LW_SCENE_PRESENT window, for example by temporarily presenting lerp(+0x134) in 0x49B4A5 and restoring it before 0x449F48.

## FINDING [medium] Icon, settlement, pulse and region-effect animations are per-render frame counters; they must stay A-only, and only their transform part is smoothed
All of these advance once per LW client update and are already A-only, since the whole of vt6C is gated. They are presentation state: none were found read by logic.
- Icon state machines count render frames. The INI ArmySelectedFade*/ArmyHilightedFade* values are frames.
- The icon offset and scale animations change the RO transform, so LW_SCENE_PRESENT smooths them.
- Rotation 0x7FE2EB is idempotent: it rebuilds Rz(+0x94 + +0x8C) every call.
- Region/army/building flash pulses and region-effect intensity curves change material colour or alpha. They stay in 30 Hz steps.
EVIDENCE: 0x7FCF6C (icon vt0C):
- inc [esi+0x38] at 0x7FCFA1; state +0x4C; alpha +0x9C -> 0x50E244 / colour 0x50E040.
- Then calls vt10 0x7FD785 (offset * +0x74 -> vt1C), vt18 0x7FE2EB and vt14 0x7FDADE (scale integrator stored in the RO scale +0x48 and transform via vt174/vt54).
- INI frame values at LWM+0x1F0..+0x204 (parse table 0xBFAD88..0xBFADD8, base LWM+0x14).

Pulses: 0x6C0BD2 -> 0x7FC09D:
- +0x18 += 0.15 [0xBE53C8] at 0x7FC0DD, or += 0.2 [0xBDAD78] at 0x7FC163 clamped to pi/2.
- Value |cos| -> 0x7FBF2A -> 0x50E040 (walks materials).
- Registered for ARMY/BUILDING/DEFAULT icons by 0x97351B/0x9D36F5/0x9EE259 via 0x6C0C07.

Region effects:
- Control-point times are converted to frames at parse (0x909F2D/0x909FA5 -> 0x7FEA48).
- Updated by LWM::update 0x6121C5 -> [LWM+0x270] 0x7EFB7C -> 0x8E646C (vt18 active? -> vt14).
REC: Keep them all on A (no change needed: GATE_GC_LWVIEW/GATE_LWM). Accept 30 Hz steps for colour and alpha.

Optional, low priority: smooth the pulse by running 0x6C0BD2 on both renders with halved steps (operand redirects at 0x7FC0DD [0xBE53C8] and 0x7FC163 [0xBDAD78]). This needs a B-path call outside GATE_LWM. It is client-only state, but check first that 0xBE53C8/0xBDAD78 have no other readers. Do not try presentation swaps of material colours, because 0x50E040/0x50E244 rewrite mesh materials.

## FINDING [medium] No army or settlement state is advanced on B-renders in this area (given the existing gates); the LW scene draw is idempotent
On a 60-mode B-render the LW map path still runs drawFrame -> 0x449F45 -> 0x49B618. That draw only writes values derived from current state. It renders army lines from static per-order data that is shown while client waypoints remain, and W3D animations use the C3-split sync clock. So the 60 FPS LW map has no double-speed source for armies, settlements or icons as long as GATE_GC_LWVIEW, GATE_LWM and GATE_GC_LWUI stay A-only.
EVIDENCE: 0x49B618:
- [0xD9B03C]=1; clear 0x51CCD0.
- +0x13C/+0xD8 = f(+0x198, GD+0xD2E).
- 0x49A4FB alpha of +0x18C/+0x190/+0x194 = f(+0x134).
- Scene ambient vt18(+0xE0); camera 0x49B4A5 = f(+0x134,+0xD8,+0xDC,+0x13C, LWM+0x38).
- 0x518000 WW3D render.

LW scene vtbl 0xBDE848 slot 0x5C = 0x49C3DF -> 0x49C384 -> 0x49C215. It draws army lines from army+0x90/+0x94 (points built only at order time by 0x93F924 from 0x71AEDF/0x71B928/0x71BC70) while 0x71A87D is true.

The army update 0x7FFC0A is reachable only from vtable slot 0xC4E2FC, which is called only from 0x6C03A9. 0x6C038B is called only from 0x6C0E4D, and 0x6C0E4D only from 0x49AAD4.
REC: For phase 6 keep GATE_LWM as the guarantee: 0x6C0E4D, which covers pulses, object updates, LWM::update with the eye tower and Palantir, and the camera fly, never runs on B. Run 0x49B618 on B as is.

## FINDING [low] moveTo 0x6BF78E/0x6BFB88 are the LW camera fly, not army movement
PLAN phase 6 lists 'moveTo 0x6BF78E/0x6BFB88' next to army work. Both functions are the LW view's scripted camera fly. 0x6BFB88 (vt9C) is the setter, called by LW logic 0x6BAEB6. 0x6BF78E is the per-render step, a tail jump from 0x6C0E86 that runs while view+0x78 is set. The step uses progress += 1/frames, x10 under the same turbo predicate as armies, and interpolates zoom, position and angle through vt68/vt58/vt7C.
EVIDENCE: 0x6BFB88: +0x7C = 1/frames (param 4), +0x78=1, +0x74=0, targets +0x60.. and +0x70, from +0x6C (vt64) and +0x80 (vt78). 0x6BF78E: +0x74 += +0x7C (x10 if 0x63F122 && GD+0x88), clamped at 1 then +0x78=0; vt68(lerp zoom), 0x6BF678 position -> vt58, 0x6BF657 angle -> vt7C. Vtable 0xBDE918 slot 0x9C = 0x6BFB88; logic caller 0x6BAEB6 (vt9C call).
REC: Handle this in the LW camera work (A-only step, presentation interpolation), not in the army work. Correct the GATE_LWM description in sites.json.

## FINDING [low] Interpolation edge cases: teleports, spawns, shared render objects, pauses and LW battle transitions
Teleports are safe. The logic moves armies instantly with 0x71B39E (army+0x44 and client vt14), and this happens in the stock logic step, before the next A-render's snapshot, so they are never interpolated. Newly created icons have no snapshot entry, so they are drawn as-is. When paused, 0x6C0E4D returns before 0x6C0E6B, so no snapshot is taken and nothing is presented. During the LW-to-battle fade, 0x49AAB8 state 2 calls vt28(0) (setActive false), so the 0x449F39 check skips the draw. Snapshot records are never dereferenced: the Open step re-walks the live map.
EVIDENCE: 0x71B39E: army+0x44/+0x48 = pos; 0x6C019A height; [army+0x88]->vt14. Callers include LW logic 0x6B7229 and xfer 0x71B928. Pause check in 0x6C0E4D at 0x6C0E56 (0x90F92C). Draw guard at 0x449F35..0x449F41 (+0x18, +0x19). 0x49AAB8 state 2: vt4C(1,1), +0x198=0, vt28(0), vt84.
REC: Keep the 400-unit cutoff, the de-duplication of ROs in the restore list, and the renderId tag. Clear the snapshot tag in the reset hook (0x44181A) and on 30/60 switches. Telemetry: count presented ROs per A-render, and make sure LwPresentClose leaves g_lwRestoreN=0.

## VERIFY SITE LW_ICON_SNAP -> confirmed
All work was read-only. My only scratch file is lw_tmp/v_snap.py in the scratchpad.

Each claim was checked against the binary:
1. Bytes. pefile on a copy of game.dat at 0x6C0E6B gives 8b ce e8 19 f5 ff ff (mov ecx,esi at 0x6C0E6B; call 0x6C038B at 0x6C0E6D). Instruction boundaries: 0x6C0E66 call 0x6C0BD2 is before the span and 0x6C0E72 mov ecx,[0xDE3C08] is after it. The whole function 0x6C0E4D..0x6C0E8C is byte-identical in rotwk/game.dat, rotwk/game820.dat and aotr/zGameDats/delayfix.dat.
2. Branch into span. A brute-force scan of every section (E8/E9 rel32, 0F 8x rel32, EB/7x/E0-E3 rel8 in executable sections, abs32 dwords everywhere) found 0 hits into 0x6C0E6C..0x6C0E71. The only branches in 0x6C0E4D are jne/je 0x6C0E8B at 0x6C0E5D and 0x6C0E62.
3. Overlap. The span overlaps no existing site: sites.json has no address in 0x6C0xxx, and GATE_LWM is at 0x49AAD4. It overlaps no AotR hook.
4. 30-mode path. The stub does mov ecx,esi, then its flag checks, then jmp [k]=0x6C038B. That is the same as stock except that the return address is 0x6C0E70 (followed by two NOPs) instead of 0x6C0E72. 0x6C038B never reads its return address: it is push ebp / leave / ret with no args. EAX is dead (0x6C038B writes it at 0x6C0390 and 0x6C0E78 reloads it). EFLAGS are dead. ECX is restored. ESI, EBX, EDI and EBP are untouched or preserved by a cdecl callee. x87 depth is 0 and no XMM is live at this call boundary.
5. Per-render on the LW map. The chain is: 0x6C0E4D has one caller, 0x49AAD4, inside the LW view vt6C 0x49AAB8. That is called at 0x6484D9 in GameClient::update. Inside 0x6C0E4D, 0x6C038B runs when 0x90F92C([0xDE412C]) is false and [esi+0x18]!=0. In 60 mode both parents (GATE_GC_LWVIEW and GATE_LWM; LwmGate in stubs.asm) are A-only, so the site runs only on A-renders, inside clientUpdate on the main thread. That is exactly where the snapshot predicate (g_m60 && !g_inB && g_inClientUpdate && main tid && g_featPresent) is true. B-renders never reach it. All the DLL variables it uses already exist in src/runtime.h and runtime.cpp. 0x6C038B has one caller, 0x6C0E4D.
6. Snapshot data layout:
   - Hash map at view+0x98. 0x6C038B passes ecx+0x98 to 0x5E0A39, which reads buckets at [map+4, map+8). So buckets are [view+0x9C, view+0xA0).
   - Nodes. 0x6BFEFD follows [node]; 0x83697E steps to the next non-empty bucket. The object is [node+8], and 0x6C03A9 calls its vt1C.
   - Objects are inserted at 0x6C0622 into view+0x98 with key [obj+0x24].
   - Vtables. Slot 0x34 = 0x9A7ED6 (([ecx+0x30]-[ecx+0x2C])>>2) and slot 0x3C = 0x9A7EE0 ([[ecx+0x2C]+4i]). A whole-image vtable scan finds exactly three vtables with both slots: 0xC4E2E0, 0xC7F5B8 and 0xC88818. So the check matches no unrelated class.
   - Their vt1C (0x93FCA5, or 0x7FFCCA, which tail-jumps to it) loops over the icons and calls icon->vtC.
   - Icon setter 0x7FD3E5 uses RO=[icon+8] and reads the RO transform at +0x18..+0x44. 0x53B260 writes the Matrix3D at RO+0x18, and 0x53B323 tests the container at RO+0x7C.
   The snapshot is read-only, so it has no logic or determinism impact.

Two things are design-level and not part of this site's correctness. The purpose line says 0x6C038B is "the only code that moves" the icons. I could only confirm that 0x7FD3E5 is called from 0x7FF27A and 0x7FF2F0 (virtual, with no static callers). Whether other paths also write RO transforms matters for LW_SCENE_PRESENT, not for this site. Second, house style would name the jump-table constant through DEFTARGET 6C038B / T_6C038B instead of k_LwObjUpdTarget (cosmetic). The site JSON stands as proposed.
CORRECTED: 

## VERIFY SITE LW_SCENE_PRESENT -> confirmed
The computed task was read-only, so I only wrote scratch output.

I verified the site against the binary (ghw/bin/game.dat). Every check held.

Bytes and slot:
- The raw section-mapped read of 0x449F43 gives 8b 01 ff 50 20 (pefile isn't installed, so I mapped the sections by hand).
- Vtable 0xBDE918 slot +0x20 = 0x49B618.

Boundaries:
- 0x449F43 `mov eax,[ecx]` is the fall-through of `jne 0x449FA9` at 0x449F41. 0x449F45 `call [eax+0x20]` is followed by 0x449F48.
- Both are whole instructions, the span is 5 bytes, and there are no relative branches to re-encode.

Branches into the span:
- I scanned all of .text for rel8, jcxz/loop, rel32 call/jmp and jcc32 targeting 0x449F44..0x449F47. There were 0 hits.
- An abs32 scan of the whole file found 0 hits.
- The only listing.asm match is the instruction label 0x449F45 itself.
- No AotR hook is nearby. Existing drawFrame sites (C7_PARTMGR_DRAW 0x449D40, INT_TREES 0x449D55, SCENE_OPEN 0x449DAB, SUBT_DELETE 0x44A1FD, SCENE_RESTORE 0x44A271) are disjoint. No existing site in 0x49A000..0x49C500 or 0x518000 covers this draw. The LW gates (GATE_LWM 0x49AAD4, GATE_GC_LWXLAT/LWVIEW/LWUI/DISPUPD_LW) gate update calls, not this draw.

30-mode path:
- `mov eax,[ecx]; jmp [eax+0x20]` after the stub's call is exact. The return address 0x449F48 is already on the stack, and ECX is untouched.
- 0x49B618 ends in a plain RET (0x49B798) and takes no stack args.

B path:
- g_inB=1 on a B-render, so the stub takes the stock path and B renders exactly stock render k.
- The A predicate (g_m60 && !g_inB && g_inClientUpdate && main thread) matches the existing A-side convention. SCENE_OPEN uses g_inB, and runtime.h says g_inB is a 60-mode B-render's clientUpdate.

Live state:
- EDI=1 is set at 0x449EA0/0x449EA2 and used at 0x449F5C. EBX=0 and ESI=display are preserved by the cdecl helpers.
- EAX is reloaded at 0x449F48 and EFLAGS are reset by the cmp at 0x449F4D.
- XMM0/XMM1 are already clobbered by stock 0x49B618, and x87 is empty at entry.

Runs per render on the LW map:
- drawFrame reaches 0x449F2F..0x449F45 whenever [0xDE4958] is non-null, +0x18 is set and +0x19 is clear (0x449EBA, then 0x449F05 onward).
- 0x49B618 renders the LW scene through 0x518000 using scene +0xC4 and camera +0xC0.

Where the stock transforms come from:
- The army icon transform is written by 0x7FFC0A. It is reached via 0x7FFCCA, which is vtable 0xC4E2E0 slot +0x1C, and ends in an `[ecx]+0x14` call.
- Icon vtables 0xC4E0C8 and others have +0x10 = 0x7FD785, +0x14 = 0x7FDADE, +0x1C = 0x7FD3E5.
- I did not trace who calls 0x7FFCCA. The proposal says it runs from the LW manager 0x6C0E4D, which 0x49AAB8 calls under GATE_GC_LWVIEW/GATE_LWM on A only. None of these is reached from 0x49B618's callees (0x51CCD0, 0x49A4FB, 0x49B4A5, 0x518000, scene vt18), so swapped transforms are not overwritten mid-draw.

Set_Transform 0x53B260:
- It does a 48-byte copy, recomputes the +0x74 identity flag, clears bit 0x02 of +0x12, and calls scene->vt38(ro,3) only when +0x78 != 0 and +0x7C == 0.
- Restoring with an exact copy therefore leaves only a lazily recomputed derived state, which is deterministic.

Caveats, which don't change the verdict:
1. LwPresentOpen must stop swapping once g_lwRestoreN hits the 8192 capacity, or restoring will be incomplete.
2. It depends on LW_ICON_SNAP, which I did not verify here.
3. HLOD sub-object and animation recompute from the swapped matrix carries the same accepted risk as battle-unit presentation windows.
CORRECTED: {"id":"LW_SCENE_PRESENT","phase":"6","group":"LW armies/settlements presentation (phase 6)","address":"0x449F43","length":5,"original_hex":"8b 01 ff 50 20","kind":"call_gate","replacement_hex":"e8 <rel32:LwScenePresentStub>","resume_address":"0x449F48","note":"Confirmed as proposed. LwPresentOpen must stop swapping once g_lwRestoreN reaches the 8192 capacity, so every swapped RO is always restored."}

## VERIFY FINDING Army movement on the LW map is a client-side per-render integrator whose results -> confirmed
I checked every link in the chain against the binary (<analysis workspace>/ghw\bin\game.dat) with dis.sh, ghq and a raw read of the vtable.

Call chain:
- 0x49AAD4 calls 0x6C0E4D. 0x6C0E4D calls 0x6C0BD2 and then 0x6C038B at 0x6C0E6D. 0x6C038B walks the container at +0x98 and makes the call [node+8]->vt1C at 0x6C03A9.
- In vtable 0xC4E2E0, slot 0x1C (0xC4E2FC) holds 0x7FFCCA. 0x7FFCCA calls 0x7FFC0A and then tail-jumps to 0x93FCA5. Slot 0x14 (0xC4E2F4) holds 0x93FCCF.

0x7FFC0A:
- It runs only if 0x6B56CC is true. 0x6B56CC checks [ecx+0xF4] in {1,5}, !0x6B4019, and +0x154 == +0x158.
- If +0x5C == 0 and the vector at +0x3C is not empty, it pops a waypoint (0x93F7FC) into +0x50/+0x54, sets +0x5C=1 and calls 0x7FF8C3.
- While +0x5C is set it calls 0x7FFB25, then the height function 0x6C019A, then vt14.

0x7FFB25 and 0x7FFA06:
- 0x7FFB25 normalises (target - pos) when len^2 > 0.001. It scales that by 0x7FFA06's result, which is min(dist, +0x58), with +0x58 multiplied by 10 ([0xBD83D8] = 10.0f) when 0x63F122([0xDE4334]) and [0xDE4364]+0x88 are set.
- If the remaining distance is < 1.0 it calls 0x7FFAA3. 0x7FFAA3 pops the next waypoint or sets +0x5C = 0.
- vt14 0x93FCCF passes the new position to the sub-objects through their vt1C and copies it into +0x18 (movsd x3 at 0x93FD35).
- So the step is a fixed per-call distance with no time scaling and no GE/sync-clock read.

Speed copy: 0x7FFDFF..0x7FFE06 copies [[esi+0x38]+0x5C] into +0x58, as the finding says.

Logic reads:
- 0x71A4CE returns client(+0x88)->+0x5C. 0x71A87D tests whether the client vector at +0x3C is non-empty (0x71A4BD = client+0x3C).
- 0x6E1DEE and 0x6E1E34 loop over a player's armies with these. 0x6B5600 and 0x6B5646 loop over the players.
- Both are called from 0x6BE20A in the phase-1 branch (LAB_006be35e, which handles the phase advance). 0x6BE20A's caller is 0x6BE51A, the body of 0x6BE50E (TheLivingWorldLogic::update).
- So the logic phase advance really does depend on the client-side movement flag and the pending waypoints.

Save: the army xfer copies the client waypoints with FUN_0071b4c8(FUN_0071a4bd()), then xfers +0x7C with 0x71BA5E. The load branch calls 0x71B495. This matches the finding (decompiled.c around 666300).

Consequence: if the army walk ran on every render in 60 mode, armies would move twice as fast per logic frame. +0x5C would clear earlier and the phase-1/5 advance would come at a different logic frame, which is a real logic divergence.

The current design already prevents this:
- GATE_GC_LWVIEW (0x6484D7) skips the whole vt6C 0x49AAB8 on B.
- GATE_LWM (0x49AAD4) skips 0x6C0E4D on B, and its only caller is 0x49AAD4.
- Severity is therefore "high" only as a constraint on phase 6 (do not let 0x6C0E4D run on B). Nothing is broken today, and the mode controller refuses LW anyway.

The rename note is also correct. 0x6C0E86 tail-jumps to 0x6BF78E when view+0x78 is set. 0x6BF78E advances a 0..1 parameter (view+0x74, rate +0x7C, times 10 under the same fast flag) and calls the view's vt68 (zoom lerp), vt58 (position lerp) and vt7C (angle). That is the view's camera fly, not army moveTo.

One small point is not covered by the evidence: the claim that the logic order 0x71BE8F fills the waypoints and sets the destination at once. I did not re-trace it, but nothing in the conclusion depends on it.

CORRECTED REC: Keep the strategic army walk (0x7FFC0A, reached through 0x49AAB8, then 0x6C0E4D, 0x6C038B and vt1C 0x7FFCCA) at exactly one call per stock main-loop iteration, which means A-renders only.

- Today GATE_GC_LWVIEW (0x6484D7) and GATE_LWM (0x49AAD4) guarantee this.
- If phase 6 lets the 0x49AAB8 camera tail (0x49AAD9 onward) run on B, GATE_LWM must stay as an independent B-skip. 0x6C0E4D bundles the army walk, LWM::update (0x6C0E7A) and the camera fly 0x6BF78E, and none of these may run on B.

Do not split the walk into A and B half-steps:
- Arrival uses dist < 1.0 after min(dist, step), and the direction is re-normalised on each call.
- Two half-steps can change the render on which +0x5C clears or a waypoint pops.
- The logic reads both at phase 1/5 (0x6BE20A, through 0x6B5600/0x6B5646), and saves store the client waypoints (0x71BFD9, through 0x71B4C8).

Get smoothness only from presentation. On A-renders, show the icon or render-object position half a step back (between the previous and current +0x18/+0x1C) without writing any client army state. B shows the stock state.

Before enabling LW 60 mode, add a telemetry check:
- per-tick counts of 0x7FFC0A calls must equal the stock counts;
- the GATE_LWM skip count may be non-zero only on B.

In sites.json, rename GATE_LWM's "LW moveTo 0x6BF78E" to "LW view camera fly 0x6BF78E" (view+0x74 parameter; vt68/vt58/vt7C zoom, position and angle lerp).

## VERIFY FINDING No interpolation fraction exists for LW objects; DLL-side transform interpolatio -> confirmed
All scratch work is under <analysis workspace>\lw_tmp\verify.

The finding holds up against the binary.

1) Where LW army motion happens. GATE_LWM (0x49AAD4, A-only) is inside the LW client update vt6C 0x49AAB8 and calls LWM 0x6C0E4D. That function calls, in order:
- 0x6C0BD2: walks LWM+0xAC and calls 0x7FC09D, a pulse accumulator using cos(+0x18).
- 0x6C038B: walks the LWM+0x98 hash and calls [entry+8]->vt+0x1C.
- [0xDE3C08]->vt28.
- moveTo 0x6BF78E, only when +0x78 is set.

2) The army update path. For the army class (vtable 0xC4E2E0, slot +0x1C at 0xC4E2FC) vt+0x1C is 0x7FFCCA:
- It calls 0x7FFC0A, then the base update 0x93FCA5.
- 0x7FFC0A advances along the waypoint vector at +0x3C using 0x7FFB25.
- The step comes from 0x7FFA06: min(remaining distance, speed float[+0x58]), times 10 when 0x63F122 returns true and byte[[0xDE4364]+0x88] is set. That is a fixed per-call step, with no time or fraction input.
- The new position goes to vt+0x14 = 0x93FCCF. That loops over the sub-icons (vt34 count, vt3C get) and reaches 0x7FD3E5.
- 0x7FD3E5 reads the transform with vt50, replaces the translation, and calls vt54 (Set_Transform) on [icon+8] and [icon+0x14].
- So the drawn pose is the RO transform at +0x18, written once per A-only client update, and B repeats it. Without DLL help, LW armies move in 30 Hz steps at 60 FPS.

3) No fraction or time source in LW code:
- rg of listing.asm for 0xDE4324, 0xDD1E0C, 0xDD1E10 and 0xDC7A8C over 0x49A000-0x49E000, 0x645000-0x646000, 0x6B0000-0x6C1200, 0x7EF000-0x800200, 0x93F000-0x940500 and 0x973000-0x974000 finds only 0x64576D, 0x645B90, 0x645BEC and 0x645CB9.
- Those are GE vt5C calls in other functions. 0x645750 itself is the 29 bytes ending at 0x64576C.
- 0x7FEA48 is ms*FPS30*0.001, a frame conversion.
- The camera notes' exhaustive list of GE+0x3C readers (9 sites) has none in LW code.

4) The proposed spans don't exist in tools/sites.json yet, but they are viable:
- 0x6C0E6B: 'mov ecx,esi; call 0x6C038B', 7 bytes.
- 0x449F43: 'mov eax,[ecx]; call [eax+0x20]', 5 bytes. It sits after the [ecx+0x18]/[ecx+0x19] checks and is reached only on the LW draw path.
- A brute rel8/rel32/jcc32 scan of all executable sections plus an abs32 scan finds no branch into either span's interior.
- Neither overlaps an existing site (the nearest is GATE_LWM 0x49AAD4) or an AotR hook.

Caveats:
- Severity: this is a design requirement for smooth motion, not a defect. Without it the LW map runs at 60 FPS with 30 Hz army motion, the same cadence as stock and with no correctness or state risk. I would rate it medium rather than high.
- Coverage: the 0x6C0E6B snapshot covers only the LWM+0x98 objects. The 0x6C0BD2/0x7FC09D pulse objects and the attachments at [icon+0x1C] (0x5F2F2D) stay on 30 Hz steps; their position is set directly to k, with no lerp.
- Cuts: the ×10 fast-step branch and waypoint switches (0x7FFAA3) need a cut threshold.
CORRECTED REC: Treat this as a medium-priority design item for phase 6 smoothing, not a high-severity defect. The confirmed fact behind it: LW armies advance a fixed step per A-only client update. The chain is 0x6C038B -> vt+0x1C 0x7FFCCA -> 0x7FFC0A/0x7FFA06 -> 0x93FCCF -> 0x7FD3E5 -> RO vt+0x54 on [icon+8] and [icon+0x14]. Nothing in that chain reads a fraction, so smooth LW armies need DLL-side transform interpolation.

Implement it as two new sites; both spans check clean:
1. LW_ICON_SNAP, a call_gate or jmp_detour at 0x6C0E6B (7 bytes: 'mov ecx,esi; call 0x6C038B'). On A only (g_m60 && !g_inB && main thread && g_featPresent), walk the live LWM+0x98 hash before 0x6C038B. For each sub-icon RO at [icon+8] and [icon+0x14], record the 48-byte transform at RO+0x18, keyed by object identity and stamped with g_renderId.
2. LW_SCENE_PRESENT at 0x449F43 (5 bytes: 'mov eax,[ecx]; call [eax+0x20]'). On A, when the snapshot's renderId equals g_renderId, walk the hash again and match entries by key. For each, set the transform via vt+0x54 to: the translation midpoint, plus a normalized 3x3 interpolation when the rotation change is small. Then run the displaced vt+0x20 draw and restore the exact saved matrices via vt+0x54. On B and in 30 mode, run the stock bytes unchanged.

Safeguards:
- Skip interpolation on cuts: a translation jump larger than one normal step, which covers the ×10 fast-step branch at 0x7FFA60, waypoint switches in 0x7FFAA3, and teleports or spawns from LW logic.
- Never keep RO pointers past the current render.
- Clear the snapshot (g_lwSnapRender) in the reset hook, in C0 and on mode switches.

Accept as visual-only:
- The 0x6C0BD2/0x7FC09D pulse objects and the attachments at [icon+0x1C] (0x5F2F2D) stay at 30 Hz steps. You can widen coverage by snapshotting at 0x6C0E64 instead.

Pair this with LW camera interpolation (GATE_GC_LWXLAT, 0x8392A7) using the same open/close window, so the camera and the armies run with the same half-step delay on A. Until then, armies are smooth on screen but the camera still moves in 30 Hz steps.

## VERIFY FINDING Cross-area: the LW logic reads LW camera zoom and position, so the 0x49AAB8 tail -> confirmed
Note on scope: I only did read-only disassembly of the scratch copy of game.dat.

What I checked:
- **LW logic update.** The entry is 0x6BE50E: a SEH prologue that falls through into FUN_006be51a. Ghidra lists FUN_006be51a as the only caller of 0x6B96C4.
- **The zoom gate in 0x6B96C4.**
  - It loads ECX=[0xDE4958], the LW view object, the same receiver whose vt6C 0x49AAB8 is called at 0x6484D9.
  - It requires [ecx+0x14]==1, then calls [eax+0x60] at 0x6B96F8.
  - The callee 0x49A991 is `movss xmm0,[0xDCB884]; comiss xmm0,[ecx+0x134]; jb; inc eax`, which returns [0xDCB884] >= zoom.
  - If that returns false, it jumps to 0x6B97B8 and skips the walk of the vector at +0xCC..+0xD0. That walk is the 0x7FEAC6 check, the vt10 call at 0x6B9753, and so on. LWM+0x2C8 and +0xE8 are checked too.
  - So the zoom value decides whether logic processes the events at +0xCC. It is logic-visible.
- **0x6BAEB6.** It calls [eax+0x64] at 0x6BAEE2, [eax+0x70] at 0x6BAF6F and [edx+0x9C] at 0x6BAFA3, which is the camera-fly vt. That fits the claimed reads of zoom/position when it schedules a fly. I did not decode every vtable slot target.
- **The integrators in 0x49AAB8.**
  - 0x49AB21 `addss xmm0,[esi+0x134]` / store to +0x134, after a check that velocity +0x138 != 0.
  - 0x49AB3E `mulss xmm0,[esi+0x138]`, where xmm0 = [[0xDE3C08]+0x1DC], stored back to +0x138.
  - After that there is a compare against 0.001 at [0xBD88A0], with clamping.
  - This all matches the finding.

Why stock-exactness already holds:
- GATE_GC_LWVIEW (sites.json) makes the whole of vt6C 0x49AAB8 A-only.
- The integrators therefore step exactly once per stock render, on the A-render, before the Y stock step reads +0x134. Logic-time values equal stock today.
- The finding matters for phase 6. The GATE_LWM purpose text and the open question in the "other A-only gates" notes both expect the LW camera part of 0x49AAB8 to run on B, and that design would break this.

Not verified: the presentation-window addresses 0x49B4A5 and 0x449F48 in the recommendation.
CORRECTED REC: Treat LW camera zoom (+0x134), zoom velocity (+0x138) and camera position (+0x110..+0x118 via vt70) as logic-visible.

1. **Integrators.** The integrators at 0x49AB21 and 0x49AB3E, the 0.001 clamp after them, and the camera fly/moveTo (0x6BF78E/0x6BFB88) must step only on A-renders, with stock increments and no B half-steps.
   - Simplest: keep GATE_GC_LWVIEW gating the whole of vt6C (as now).
   - If phase 6 lets parts of 0x49AAB8 run on B, add g_skipB + main-thread patch sites that skip 0x49AB08..0x49AB79 (the zoom/velocity block) on B. GATE_LWM must keep 0x6C0E4D A-only.
2. **Smooth presentation.** Do it only inside the LW draw window (display vt20 0x49B618 called at 0x449F45):
   - Save the stock +0x134 (and position if interpolated).
   - Write the presented lerp value.
   - Restore it right after the vt20 call returns, before any logic step or the next clientUpdate.
3. **Checks still to do before using that window:**
   - Confirm that nothing inside the LW draw writes persistent state derived from +0x134/+0x110 and that no other code path calls 0x49A991/vt64 while the presented value is in place.
   - Confirm the exact entry and restore points (the 0x49B4A5 and 0x449F48 addresses suggested in the finding were not verified).
4. **Telemetry.** Add a test that compares +0x134/+0x138 at each LW logic tick against a 30-mode run.

## VERIFY FINDING Icon, settlement, pulse and region-effect animations are per-render frame counte -> confirmed
I checked every claim against game.dat with dis.sh, ghq.py and raw vtable scans.

**Gating chain:** The finding's claim that all of this is A-only holds.
- GATE_GC_LWVIEW gates the vt6C call to 0x49AAB8 at 0x6484D7.
- 0x49AAB8 calls 0x6C0E4D at 0x49AAD4, which GATE_LWM gates.
- 0x6C0E4D calls the pulse walker 0x6C0BD2 (it calls 0x7FC09D per entry), then 0x6C038B, then [0xDE3C08]->vt28.
- That vt28 is LWM::update 0x6121C5: the LWM vtable 0xBFB548 is set at 0x613C8C and 0x614187, and slot +0x28 at 0xBFB570 = 0x6121C5.
- 0x6121C5 calls 0x7EFB7C when [+0x270] is set. 0x7EFB7C is a tail jump to 0x8E646C, which calls vt18 and, when that returns true, vt14 (region effects). It then walks a list calling vt0C on each entry (icons).

**Icon vtables:** 0xC4E0C8, 0xC84B18 and 0xC8C4A8 have vt0C = 0x7FCF6C (0xC4E238 uses the 0x7FF50C wrapper). All four have vt10 = 0x7FD785, vt14 = 0x7FDADE and vt18 = 0x7FE2EB.

**0x7FCF6C:** Matches the finding.
- `inc [esi+0x38]` at 0x7FCFA1; state written to +0x4C.
- Alpha stored at +0x9C, then 0x50E244 (alpha) or 0x50E040 (colour).
- Then calls vt10, then vt18.

**Transform helpers:**
- 0x7FD785 scales the offset by +0x74 and applies it through vt1C.
- 0x7FDADE integrates the RO scale +0x48 toward +0x80, then calls RO vt174 and vt54.
- 0x7FE2EB rebuilds the rotation from +0x94 + +0x8C on every call, so it is idempotent.

**INI fade values are frames:** The parse table at 0xBFAD88..0xBFADD8 uses parser 0x42EC5E, a plain sscanf integer parse with no millisecond conversion. Its offsets 0x1DC..0x1F0 are relative to parse base LWM+0x14, i.e. LWM+0x1F0..0x204.

**Region-effect times:** 0x909F2D calls 0x7FEA48 at 0x909F5B. That function does fild of the FPS30 constant times the value, then ftol. So the times become 30 Hz frames.

**Pulse 0x7FC09D:**
- Adds 0.15 [0xBE53C8] at 0x7FC0DD, gated by 0x6B65F7 (a check of LW logic state that only reads it). If the check fails, +0x18 is reset to 0.
- Otherwise adds 0.2 [0xBDAD78] at 0x7FC163, clamped to 1.5708.
- Calls 0x7FBF2A at 0x7FC1FB.
- The icon state-change callbacks vt24 (0x7FD1E9) and vt28 (0x7FD275) only touch icon fields and the material colour/alpha.

**Logic reads:** Within what I traced, no logic writes show up. The only logic interaction is the read in 0x6B65F7.

**Minor issues in the finding:**
- LW_SCENE_PRESENT does not exist anywhere in the aotr60 repo. It is a proposed mechanism, so the claim that the transform part "is smoothed" depends on that future work.
- The optional pulse-smoothing idea conflicts with the design rule that a B-render must not advance persistent state. Running 0x6C0BD2 on B with halved steps would:
  - make the 0x6B65F7 reset and the pi/2 clamp behave at half-step granularity;
  - change float accumulation (0.075+0.075 instead of 0.15);
  - need a new B-path call site outside both vt6C and GATE_LWM.
- Its "other readers of 0xBE53C8/0xBDAD78" caveat does not matter for an operand redirect.
CORRECTED REC: No change is needed: keep the icon state machines (0x7FCF6C and its vt10/vt14/vt18), the flash pulses (0x6C0BD2 -> 0x7FC09D) and the region effects (0x6121C5 -> 0x7EFB7C -> 0x8E646C) A-only. The existing gates GATE_GC_LWVIEW (0x6484D7) and GATE_LWM (0x49AAD4) already do this.

Accept 30 Hz steps for material colour and alpha written through 0x50E244 and 0x50E040. Do not swap them on B, because those functions rewrite mesh materials.

If an LW scene presentation window (the proposed LW_SCENE_PRESENT) is added later, the icon offset, scale and rotation changes are RO-transform state, so it would smooth them for free.

Drop the optional pulse smoothing, or treat it as a deliberate exception to the design rule. Running 0x6C0BD2 on B with halved operands (redirects at 0x7FC0DD and 0x7FC163) would advance persistent pulse state on B. It would change the reset and clamp behaviour of 0x6B65F7 and the pi/2 clamp, and the float accumulation. It would also need a new B-only call outside both vt6C and GATE_LWM. All that for a purely cosmetic gain.

## VERIFY FINDING No army or settlement state is advanced on B-renders in this area (given the exi -> uncertain
This finding was not independently verified.
CORRECTED REC: No verdict on the finding. Its claims are untested: the B-render idempotence of 0x49B618, and GATE_LWM as the only route to the army update 0x7FFC0A through 0x6C0E4D, 0x6C038B, 0x6C03A9 and vtable slot 0xC4E2FC. Treat them as unconfirmed.

## SITE {
 "id": "LW_ICON_SNAP",
 "phase": "6",
 "group": "LW armies/settlements presentation (phase 6)",
 "address": "0x6C0E6B",
 "length": 7,
 "original_hex": "8b ce e8 19 f5 ff ff",
 "original_asm": [
  "0x6C0E6B: 8b ce           mov ecx, esi            ; ESI = LW view/client [0xDE4958] (this of 0x6C0E4D)",
  "0x6C0E6D: e8 19 f5 ff ff  call 0x6C038B           ; per-render update of every LW client object (armies, buildings, named map objects): [node+8]->vt1C at 0x6C03A9"
 ],
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:LwIconSnapStub> 90 90",
 "stub": "LwIconSnapStub (call boundary; entry [esp]=0x6C0E70, ESI=LW view, ECX/EAX/EDX/EFLAGS dead, x87 empty):\n  mov ecx, esi                                  ; displaced\n  cmp byte [g_m60],0           ; je .go\n  cmp byte [g_inB],0           ; jne .go\n  cmp byte [g_inClientUpdate],0; je .go\n  cmp byte [g_featPresent],0   ; je .go\n  mov eax, fs:[0x24] ; cmp eax,[g_mainTid] ; jne .go\n  push esi ; call LwIconSnapshot ; add esp,4      ; cdecl(view); C++ preserves EBX/ESI/EDI/EBP\n  mov ecx, esi\n.go:\n  jmp dword ptr [k_LwObjUpdTarget]              ; =0x006C038B; returns to 0x6C0E70 (2 NOPs) -> 0x6C0E72\nLwIconSnapshot(view): g_lwSnapRender=g_renderId; n=0. Walk the LIVE hash map at view+0x98: buckets [view+0x9C, view+0xA0) (same layout 0x5E0A39/0x6BFEFD/0x83697E use), node: [node+0]=next, [node+8]=client object. For each object with vt[0x34]==0x9A7ED6 && vt[0x3C]==0x9A7EE0 (true for army 0xC4E2E0, building 0xC7F5B8, map object 0xC88818): icons = [obj+0x2C..obj+0x30) (pointer vector); for each non-null icon, for RO in {[icon+0x08],[icon+0x14]} (the two ROs 0x7FD3E5/0x7FDADE write): skip if NULL or [RO+0x7C]!=0 (sub-object/container); store {RO, 48 bytes at RO+0x18 (Matrix3D 3x4, translation at +0x24/+0x34/+0x44)} into g_lwSnap and an open-address index keyed by RO (first occurrence wins). Guards: buckets<=65536, chain<=4096, icons<=64, entries<=8192 (overflow: g_lwSnapRender=0 -> no interpolation this render). Read-only; no engine calls. 30 mode: stock (mov ecx,esi; call 0x6C038B, plus 2 NOPs).",
 "resume_address": "0x6C0E72 (0x6C038B returns to 0x6C0E70; two NOPs)",
 "live_after": "ESI = LW view (0x6C0E7D cmp byte [esi+0x78]); EBX/EDI/EBP callee-saved (untouched / preserved by cdecl C++); [esp] = saved ESI of 0x6C0E4D then return 0x49AAD9. Dead: EAX (0x6C0E78 mov eax,[ecx]), ECX (0x6C0E72 mov ecx,[0xDE3C08]), EDX, EFLAGS. x87 depth 0, no XMM live (call boundary).",
 "branch_into_span_check": "Interior 0x6C0E6C..0x6C0E71: brute-force decode of every executable byte (E8/E9 rel32, 0F 80-8F rel32, EB/70-7F/E0-E3 rel8): 0 hits; abs32 dword scan of the whole image: 0 hits; rg listing.asm for 0x006c0e6[b-f]/0x006c0e7[01]: 0 hits. Only branches in 0x6C0E4D are je 0x6C0E8B at 0x6C0E5D/0x6C0E62 (outside). Bytes identical in game.dat, delayfix.dat, game820.dat. No overlap with AotR hooks or existing sites (GATE_LWM is 0x49AAD4).",
 "purpose": "Pre-update snapshot (state presented by the previous B-render, incl. any logic-step teleports) of every LW map icon render-object transform, taken on the 60-mode A-render immediately before the only code that moves/animates them (0x6C038B). Read-only; consumed by LW_SCENE_PRESENT in the same render. Does not touch any logic-visible client state.",
 "evidence": "0x6C0E4D (only caller 0x49AAD4, GATE_LWM) = pause/active check, 0x6C0BD2 (pulses), 0x6C038B (objects), LWM vt28 0x6121C5, camera fly tail 0x6C0E86. 0x6C038B: lea ecx,[map]; 0x5E0A39 begin; loop [node+8]->vt1C (0x6C03A9); 0x6BFEFD next (node=*node else 0x83697E next bucket). Objects inserted into view+0x98 by base ctor 0x93FDCA -> 0x6C0622 (key [obj+0x24]). Icon accessors 0x9A7ED6 ([ecx+0x30]-[ecx+0x2C])>>2, 0x9A7EE0 [[ecx+0x2C]+4i]. Icon setPosition 0x7FD3E5 and scale 0x7FDADE write RO [icon+8] and [icon+0x14] via vt54; RO transform at +0x18 (0x53B260), container +0x7C (0x53B323).",
 "risks": "None for logic (pure reads). Cost: one hash walk per A-render (hundreds of ROs). If an LW object class with different icon accessors exists it is skipped (vtable check), i.e. drawn stock.",
 "dll_vars": [
  {
   "name": "g_m60",
   "ctype": "uint8_t",
   "note": "existing"
  },
  {
   "name": "g_inB",
   "ctype": "uint8_t",
   "note": "existing; 0 on A"
  },
  {
   "name": "g_inClientUpdate",
   "ctype": "uint8_t",
   "note": "existing"
  },
  {
   "name": "g_mainTid",
   "ctype": "uint32_t",
   "note": "existing"
  },
  {
   "name": "g_featPresent",
   "ctype": "uint8_t",
   "note": "existing UnitInterpolation feature flag"
  },
  {
   "name": "g_renderId",
   "ctype": "uint32_t",
   "note": "existing, C0"
  },
  {
   "name": "k_LwObjUpdTarget",
   "ctype": "const uint32_t",
   "note": "0x006C038B in every mode"
  },
  {
   "name": "g_lwSnapRender",
   "ctype": "uint32_t",
   "note": "renderId of the snapshot; 0 = invalid; cleared by reset hook/mode switch"
  },
  {
   "name": "g_lwSnap[8192] {void* ro; float m[12];} + index",
   "ctype": "struct array",
   "note": "rebuilt every 60-mode A-render on the LW map"
  }
 ]
}

## SITE {
 "id": "LW_SCENE_PRESENT",
 "phase": "6",
 "group": "LW armies/settlements presentation (phase 6)",
 "address": "0x449F43",
 "length": 5,
 "original_hex": "8b 01 ff 50 20",
 "original_asm": [
  "0x449F43: 8b 01     mov eax, dword ptr [ecx]      ; ECX = [0xDE4958] LW client (non-null, +0x18 active, +0x19 clear: 0x449F35..0x449F41)",
  "0x449F45: ff 50 20  call dword ptr [eax + 0x20]  ; vtbl 0xBDE918 slot 0x20 = 0x49B618, LW scene draw (only call site of this slot)"
 ],
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:LwScenePresentStub>",
 "stub": "LwScenePresentStub (call boundary; entry [esp]=0x449F48, ECX=LW client, EBX=0, ESI=W3DDisplay, EDI=1 live; EAX/EDX/EFLAGS dead; x87 empty):\n  cmp byte [g_m60],0            ; je .stock\n  cmp byte [g_inB],0            ; jne .stock\n  cmp byte [g_inClientUpdate],0 ; je .stock\n  push eax ; mov eax,fs:[0x24] ; cmp eax,[g_mainTid] ; pop eax ; jne .stock\n  push ecx ; call LwPresentOpen ; pop ecx         ; cdecl, no args\n  mov eax,[ecx] ; call dword ptr [eax+0x20]       ; 0x49B618 draws the LW scene with the presented transforms (plain RET, no args)\n  call LwPresentClose                             ; cdecl, restores\n  ret                                             ; -> 0x449F48\n.stock:\n  mov eax,[ecx] ; jmp dword ptr [eax+0x20]        ; == original; 0x49B618 returns to 0x449F48\nLwPresentOpen(): return unless g_featPresent && g_lwSnapRender==g_renderId. g_lwRestoreN=0. Re-walk the LIVE hash exactly as LwIconSnapshot (so only live objects/ROs are touched). For each RO: e=lookup(RO); skip if none, if RO already in the restore list, or if memcmp(RO+0x18,e.m,48)==0. cur=RO+0x18. d=(cur[3]-e[3], cur[7]-e[7], cur[11]-e[11]); skip if |d|>400 world units (teleport/spawn; normal step is <= 20/render, 200 with the x10 turbo). Save {RO,cur} to g_lwRestore. P=cur; P[3]=(e[3]+cur[3])*0.5f; P[7]=...; P[11]=...; if max|cur[i]-e[i]| over the 3x3 (i in 0,1,2,4,5,6,8,9,10) <= 0.25f also average those (smooths 0x7FDADE scale/0x7FD785 offset anims). Call RO->vt54(&P) (thiscall Set_Transform; base 0x53B260, HLOD 0x574FC0 also propagates via vt208). Use SSE only; never touch x87 CW/MXCSR.\nLwPresentClose(): for i=g_lwRestoreN-1..0: RO->vt54(saved m) (exact 48-byte copy back); g_lwRestoreN=0. Other LW modules (e.g. LW camera half-step) can hook the same Open/Close pair so camera and icons share the 16.7 ms delay.",
 "resume_address": "0x449F48",
 "live_after": "EBX=0 (zero register for the rest of drawFrame), ESI=W3DDisplay*, EDI=1 (0x449F5C mov [eax+0x3c],edi), EBP frame incl. [ebp-0xD] (0x449F6A) and [ebp-4] EH state. Dead: EAX (0x449F48 mov eax,[0xDE4958]), ECX, EDX, EFLAGS (cmp at 0x449F4D). x87 depth 0; XMM0 (comiss at 0x449F18) not live.",
 "branch_into_span_check": "Interior 0x449F44..0x449F47: brute-force rel8/rel32/jcc32 decode of all executable bytes: 0 hits; abs32 scan of the whole image: 0 hits; rg listing.asm 0x00449f4[4-7]: 0 hits. Span start 0x449F43 is the fall-through of jne 0x449FA9 at 0x449F41. Bytes identical in game.dat, delayfix.dat, game820.dat. Disjoint from C7_PARTMGR_DRAW 0x449D40, INT_TREES 0x449D55, SCENE_OPEN 0x449DAB, SCENE_RESTORE 0x44A271; no AotR hook nearby.",
 "purpose": "Presentation-only half step for LW map armies (and building/map-object icon animations) on 60-mode A-renders: A shows lerp(previous presented, stock render k, 1/2), B shows stock render k unchanged, exactly like battle units. Every swapped transform is restored before 0x449F48, so picking (0x6C0A19/0x7FC05A), the next update (0x7FFB25 reads client +0x18, not the RO), saves and logic never see it.",
 "evidence": "drawFrame LW branch 0x449F2F..0x449F45 (updateViews skipped at 0x449D29); 0x49B618 = derived writes (+0x13C,+0xD8 from +0x198; alpha 0x49A4FB on +0x18C/+0x190/+0x194 from +0x134), camera 0x49B4A5 (pure from zoom), WW3D render 0x518000(scene +0xC4, camera +0xC0); LW scene Customized_Render 0x49C3DF (vtbl 0xBDE848 slot 0x5C) draws army lines 0x49C384/0x49C215 from static per-order points. Army icon translation written each A-render via 0x7FFC0A -> vt14 0x93FCCF -> icon vt1C 0x7FD3E5 -> RO vt54. RO Set_Transform 0x53B260: copies 48 bytes to +0x18, +0x74 identity flag, +0x12 &= ~2, scene->vt38(ro,3) (LW scene 0x540F90 case 3 = add-unique to list +0xBC via 0xA24490) - the same call stock makes per moving army per render.",
 "risks": "Set_Transform re-registers the RO in the LW scene update list (presentation state only; stock does the same each render for moving armies). Bounding volumes are recomputed lazily from the restored matrix (deterministic). Teleports below the 400-unit cutoff show one half-way frame. Colour/alpha animations (pulse 0x7FC09D, fades 0x7FCF6C alpha, region effects) are not interpolated and keep 30 Hz steps. Must be closed even when 0x49B618 draws nothing (Close always runs after the call).",
 "dll_vars": [
  {
   "name": "g_m60 / g_inB / g_inClientUpdate / g_mainTid / g_featPresent / g_renderId",
   "ctype": "existing"
  },
  {
   "name": "g_lwSnapRender, g_lwSnap",
   "ctype": "see LW_ICON_SNAP"
  },
  {
   "name": "g_lwRestore[8192] {void* ro; float m[12];}, g_lwRestoreN",
   "ctype": "struct array + uint32_t",
   "note": "non-empty only between Open and Close"
  },
  {
   "name": "k_LwInterpCutoff",
   "ctype": "float",
   "note": "400.0f world units"
  }
 ]
}