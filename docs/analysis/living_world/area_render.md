# AREA lw-render

## SUMMARY
I found no per-draw integrators in the Living World strategic-map draw path, so it needs no new A-only gate sites. I propose three phase-6 sites: one smooths the LW camera, two are optional cost savings. I modified or deleted no files.

What I traced, and what I found:

1. **The LW branch of drawFrame 0x449CF8 changes no persistent state.** The LW draw vt20 0x49B618 clears the target, then sets +0x13C and +0xD8 from the fade +0x198, sets the sub-object alphas 0x49A4FB→0x50E244 and the scene vt18. It then rebuilds the LW camera with 0x49B4A5/0x49AF11 and renders via 0x518000. All of these values are recomputed every draw from the LW client fields.

2. **The LW scene's custom render 0x49C3DF (scene vtable 0xBDE848) is also stateless.** Its per-render increments are only:
   - a visibility stamp (scene +0x104 in 0x542250);
   - D3D state statistics (0xDD34D8/0xDD34A4).

   Its other calls are all stateless or already covered:
   - the shadow flag 0x499D6D→0x4F43A6;
   - the debug-draw flush ([0xDE4418]+0x2C vt58/vt5C);
   - the army path lines 0x49C384→0x49C215 (pure getters, plus a W3D segmented line whose UV scroll follows sync time);
   - particle render vt48/vt40 (0x44C3E2/0x44C84A);
   - standard W3D render objects only. There is no LW-specific render-object class: a vtable scan found none with Render or On_Frame_Update in LW code. LW objects enter the scene through LW client vt44 0x49A707 from 0x7EFD28, 0x7FF120, 0x612EDB, 0x61314A and 0x9D437C, all created by the asset manager.

   The On_Frame_Update slots already patched (INT_LIGHTPULSE 0xBDC00C, INT_TERRAIN_TILEUPD 0xBE4794) and the other g_skipB render-object sites are keyed on the slot or the instruction, so they work identically in this scene.

3. **All per-call LW visual integrators sit in the A-only update paths.** These are the gates GATE_GC_LWXLAT, GATE_GC_LWVIEW, GATE_LWM, GATE_GC_LWUI, GATE_CU_APT and GATE_PALANTIR. Examples:
   - zoom 0x49AB21 and fade 0x49ABB4/0x49ABFD;
   - rotate 0x49B8E6 and scroll via vt58 at 0x49B8AF;
   - moveTo 0x6BF78E;
   - the flash phase 0x7FC09D (+0x18 += 0xBE53C8/0xBDAD78).

   None of the LW object code (0x6Bxxxx, 0x7Fxxxx, 0x61xxxx) is reachable from the draw roots, apart from pure getters.

4. **Banner, token and emitter animation is correct on B-renders.** They follow the W3D sync clock, and C3 splits it correctly on the LW map: the stepper sets GC+0xC8=1 on every non-halted step (0x6325F8..0x6325FB), so m_frame advances at 30/s even though LW GameLogic sets it only at sub 1.

5. **The shared parts of the branch are already covered.** RenderUI, 0x6A536C, the subtitles (the LW logic-running path at 0x44A1D3), the video overlay layers 0x65D03B (stateless) and 0x65CF05 are handled by existing sites. The APT draw 0x62212F produces no B-render update, because vt28 clears +0x328 at 0x624F6C right after its setters 0x622911/0x622959. Its embedded-window flag and hide bookkeeping is the same on A and B, since the display list changes only in the A-only APT update.

6. **The LW map embedded in an APT window adds nothing.** Its window draw 0x975441 runs the windowed LW draw vt24 0x49BEFD. That draw is stateless and uses a fixed camera (vt50(1) sets the position and zoom from mapinfo at 0x49BED1). The embedded video window draws 0x9D8BE1 and 0x92B447 are stateless; frames advance only in 0x92B5BB through the A-only APT update.

The proposed sites:

- **LW_SCENE_PRESENT (0x449F43)** is the real phase-6 need. It is a presentation window: on A-renders the LW camera inputs are set halfway between the previous and current camera for the vt20 draw, and restored at the existing SCENE_RESTORE.
  - Without it, A and B show the identical LW picture.
  - Those inputs are position +0xF8/+0xFC/+0x100, angle +0x11C (clamped, not wrapped) and zoom +0x134.
  - The restore also re-runs 0x49B4A5 so the camera is left exactly at the stock C_k.
- **LW_B_SKIP_SHROUD (0x449D63) and LW_B_SKIP_SHADOW_WATER (0x449DBD)** skip, on LW-branch B-renders only, the shroud, shadow-map and water-reflection render-to-texture passes. The LW branch never consumes these.
  - In stock they already run every render on the LW map, because the terrain heightmap pointer DC78EC+0x37C0 is only ever replaced, never cleared.
  - At 60 FPS they would double, so these sites would recover that cost; measure first.

## COVERAGE
Fully traced (capstone, Ghidra decompile, static reachability):
- drawFrame 0x449CF8: both branches and the shared tail. W3DDisplay::draw 0x44B788 up to drawFrame.
- LW draw vt20 0x49B618, 0x49B4A5, 0x49AF11, 0x49A4FB→0x50E244.
- The LW scene class (vtable 0xBDE848, Customized_Render 0x49C3DF; vt6C 0x542250).
- Register/Add 0x540F90/0x540F30, and the On_Frame_Update list mechanism (shared with RTS3DScene 0x46FF57).
- Army lines 0x49C384/0x49C215 plus getters; SegmentedLine 0x55E810/0x55E9F0.
- Shadow path 0x499D6D→0x4F43A6, including the RMW candidates in its closure.
- Particle render vt40/vt48 (vtable 0xBDAC10).
- LW client vtable 0xBDE918: 0x49A707, 0x49A725, 0x49AA24, 0x49A9A6, 0x49A6A8, 0x49BE23, 0x49BEFD, 0x49AAB8, 0x49B799 (field writers).
- LW object adders: 0x7EFD28, 0x7FF120, 0x612EDB, 0x61314A.
- Flash update 0x7FC09D; moveTo 0x6BF78E/0x6BFB88.
- APT draw 0x62212F with 0x81424C/0x814278. Embedded window factory 0x8142D2, LW window 0x975659/0x975441/0x97547F, Bink window 0x9D8BE1/0x92B447/0x92B5BB.
- Display video draw 0x65D03B/0x65C529 and Display::update 0x65C1EA.
- 3D-in-UI clock 0x4A89BD (real-time).
- RMW scan over the LW code ranges 0x49A000-0x49D700, 0x6B2000-0x6C2000, 0x7EF000-0x805000, 0x610000-0x617000, 0x9D3000-0x9DA000, 0x975000-0x977000, 0x838000-0x83A000. Every hit lies off the draw closure except the false positives listed.
- Span checks (brute rel8/rel32/jcc32 plus abs32 plus listing.asm) for all three spans. Byte identity checked in delayfix.dat and game820.dat.

Not traced or not proven:
- Internals of the standard W3D render objects (HLod/Mesh Render), beyond relying on the earlier phase-4a sweep and on these classes being shared with battles. HLod and Mesh vtables were not identified by my scan.
- APT renderer internals 0xAE0AB0/0xAE09F0, and InGameUI draw 0x48EA29 / vt158 0x69B7BE internals (shared with battles). Only their reach into LW code was checked: iterators only.
- Whether the shroud objects DC78EC+0x3878/+0x387C, DC7A38 and the water polygons are live on the LW map (runtime state). Cost gains are unmeasured.
- Where the banner-scale and army-selection fade integrators (mapinfo BannerScaleSpeed +0xCC, ArmySelectedFade* +0x1DC..+0x1F0) live. They are not reachable from any draw root, so they sit in the A-only update paths, but their exact location was not pinned.
- Mode-controller enabling (BlockReason for GL+0x110==8 / GL+0x125) and the LW input or camera stepping gates are outside this area.
- No in-game verification.

## FINDING [medium] LW draw path has no per-draw integrators; no new A-only gate sites needed in LW-specific draw code
Every function the LW branch runs per render recomputes its outputs from LW client state, which is stepped only on A-renders. So a B-render reproduces stock render k exactly, apart from sync-clock animation, which C3 already splits correctly.

The draw-side counters that do change per render are harmless:
- the scene visibility stamp +0x104 (0x542250), compared only for equality;
- D3D statistics counters 0xDD34D8/0xDD34A4;
- DAT_00D9B03C=1, a shadow flag set and cleared within the render;
- TheAudio's listener dirty flag (vt58 0x450A08 sets +0x6AB=1).

Shared paths that the LW branch also runs are already covered by existing sites: C7_PARTMGR_DRAW (0x5F5123 updates only +0x98 systems on the LW map), INT_TREES, INT_UIPART, INT_OVL_FADE, INT_SUBT_* (the LW logic-running path at 0x44A1D3), and the slot or instruction-level g_skipB sites in W3D render objects (INT_LIGHTPULSE, INT_TERRAIN_TILEUPD, INT_OUTLINE_*, INT_OVL_*).

The RMW candidates in the closure were checked and are false positives: 0x46DB66 is an RGB→HSV conversion, 0x4F3626 sets then adds, 0x5065D7 and 0x506E00 reset then accumulate, 0x551B96 builds a local vector of FX parameters.
EVIDENCE: - drawFrame 0x449CF8: LW branch 0x449F2F..0x449FA4 and tail 0x44A0AD..0x44A22E.
- Decompiles: 0x49B618, 0x49B4A5, 0x49AF11, 0x49C3DF (LW scene vt5C), 0x49C384/0x49C215, 0x542250.
- Capstone reachability from 0x49B618, 0x49BEFD, 0x49C3DF, 0x975441 and 0x49C384 (495 functions), intersected with an RMW scan of the LW code ranges.
- Render-object vtable scan: none has Render or On_Frame_Update in LW code. LW objects reach the scene only through LW vt44 0x49A707.
- Stepper 0x6325F8..0x6325FB sets GC+0xC8=1 on every non-halted step, so C3 sync works on the LW map.
REC: Mark the LW draw allow-listed for B-renders as is. Spend the phase-6 effort on enabling the mode controller and on the camera presentation (LW_SCENE_PRESENT). Verify with telemetry on the LW map: gated-call rates at 30.3/s and the logic-RNG trace per LW tick.

## FINDING [high] Without a presentation window the LW map at 60 FPS shows each camera picture twice
The LW camera is built every draw from the client fields:
- position +0xF8/+0xFC/+0x100;
- angle +0x11C;
- zoom +0x134;
- fade +0x198.

All their writers are A-only gated: pan and rotate (0x49B799, vt58 at 0x49B8AF and 0x49B8E6), zoom velocity 0x49AB21, fade 0x49ABB4/0x49ABFD, and moveTo 0x6BF78E. The A- and B-renders therefore draw the identical LW picture, so scrolling, zooming and fly-to look like 30 Hz even at 60 renders/s.
EVIDENCE: - 0x49B618 → 0x49B4A5 → 0x49AF11 (inputs listed above).
- The setters vt58 0x49AA24, vt68 0x49A9A6, vt7C 0x49A6A8 are called only from the gated paths: GATE_GC_LWXLAT 0x6484BD, GATE_GC_LWVIEW 0x6484D7, GATE_LWM 0x49AAD4.
- The angle is clamped, not wrapped (0x49B8F9..0x49B927), so a plain lerp is correct.
REC: Add LW_SCENE_PRESENT (0x449F43) with its restore in the existing SceneRestore. Coordinate with the owner of the LW camera input area so that only one mechanism, A-only stepping plus presentation, is used.

## FINDING [low] Tactical render-to-texture pre-passes run every render on the LW map and would double
In stock, drawFrame runs these passes on every render on the LW map, but the LW branch never consumes them:
- the shroud passes (0x472EF4/0x473CAD);
- the shadow-map pass (0x47D5C9);
- the water-reflection pass (0x47F25A).

The heightmap pointer DC78EC+0x37C0 is only ever replaced on map load, so they run on the last loaded map. In 60 mode each would run twice per stock frame.
EVIDENCE: - The only writers of +0x37C0 are 0x46CF3A and 0x46D107 inside 0x46CF01 (sole caller 0x4E1366).
- The LW draw closure contains no references to DC7A38 or DC7A40. RenderViews and its shadow hooks (0x449FA9..0x44A00D) are skipped in the LW branch.
REC: Optional sites LW_B_SKIP_SHROUD (0x449D63) and LW_B_SKIP_SHADOW_WATER (0x449DBD) skip these passes only on LW-branch B-renders. Measure the B-render cost on the LW map first.

## FINDING [low] SCENE_OPEN fires on every LW A-render without camera history
On the LW map, W3DView::update is not called (0x449D29), so S2 never records M_k.

SceneOpen_A still opens the presentation window: the fraction becomes (2k-1)/12 and the C4 key is set. It then increments aNoHistory on every A-render. This is harmless:
- the only fraction or C4 consumer in the LW draw closure is 0x6765B9;
- it is reached through projected shadows 0x4F42BF only for render objects carrying Drawable user data;
- LW objects carry none.
EVIDENCE: camera.cpp SceneOpen_A (requires r1.renderId==g_renderId). The reachability path 0x49C3DF→0x499D6D→0x4F43A6→0x4F42BF→0x6765B9 reads [0xDE4324]+0x3C.
REC: When the LW branch will be taken ([0xDE4958]+0x18 && !+0x19), return early from SceneOpen_A after the fraction window, or count it separately, so that camera telemetry stays meaningful.

## FINDING [low] APT and embedded-window draw side is A/B-consistent
The APT draw 0x62212F runs the APT update only if +0x328 is set, and that flag is effectively never set at draw time. Its per-render window bookkeeping is idempotent across A and B:
- 0x81424C clears the drawn flag +0x2C;
- 0x814278 hides windows that were not drawn.

The embedded LivingWorldMap window draw 0x975441 → vt24 0x49BEFD uses a fixed APT camera and is stateless. The Bink window draw 0x9D8BE1 → 0x92B447 is stateless; frames advance only in 0x92B5BB via the A-only APT update 0x814218. The window's one-shot +0x2A0 → 0x840024 runs once regardless of A or B.
EVIDENCE: - 0x624F6C clears +0x328 inside vt28 0x624EE1. The setters 0x622911/0x622959 call vt28 immediately.
- Window vtables 0xC84E48 and 0xC84DCC; video object vtable 0xC7ED74.
REC: No site needed. Keep GATE_CU_APT.

## Scratch files
My only writes were scratch files under lw_tmp in the analysis workspace:
- helpers: bx.py, fields.py, rovt.py, rovt2.py, path.py;
- data dumps: sites_dump.txt, drawframe.txt, w3ddraw.txt, vt24.txt, lw_rmw.json, reach_lwdraw.json, reach_ui.json, de3c08_funcs.txt, rovts.txt.

They are not needed after this report.

## VERIFY SITE LW_SCENE_PRESENT -> confirmed
Verification of LW_SCENE_PRESENT at 0x449F43 (read from a copy of game.dat):
- Bytes. The raw file at the section-mapped offset is "8b 01 ff 50 20", decoded as mov eax,[ecx] (0x449F43) and call [eax+0x20] (0x449F45). The vtable 0xBDE918 has 0x49B618 at slot +0x20. 0x49B618 is __fastcall/thiscall with no stack args and ends with a plain ret at 0x49B798, so tail-calling it via jmp [eax+0x20] with [esp]=0x449F48 behaves exactly like the original call.
- Boundaries. The span ends at 0x449F48 (mov eax,[0xDE4958]), so it lies exactly on instruction boundaries and is 5 bytes. The preceding guards at 0x449F35..0x449F41 (ecx!=0, byte[+0x18]!=0, byte[+0x19]==0) branch to 0x449FA9, outside the span.
- Branch into span. I brute-scanned every executable section for rel8 / loop / jcz / E8 / E9 / 0F8x targets in 0x449F44..0x449F47 and found 0 hits. A whole-file abs32 scan for 0x449F43..0x449F47 also found 0 hits.
- live_after holds. 0x449F48 reloads EAX, 0x449F5F reloads ECX, and 0x449F4D sets EFLAGS. EBX=0, ESI and EDI are used later at 0x449F5C, 0x449F6A, 0x449F8D and 0x44A0D0. The C++ helper is cdecl, so EBX/ESI/EDI/EBP are callee-saved. EDX is clobbered by the callee 0x49B618 anyway. Pushing ECX twice and adding 4 to ESP keeps ECX and leaves the stack exact.
- 30-mode, B and non-main-thread paths run byte-for-byte "mov eax,[ecx]; jmp [eax+0x20]", i.e. the original. The predicate (g_m60 && !g_inB && g_inClientUpdate && main thread) is equivalent to an A-render inside the OnPreRender/OnPostRender bracket (frame_ctl.cpp:354-372; g_skipB = m60 && inB). It mirrors the A test SCENE_OPEN uses.
- Runs per render on the LW map. The drawFrame LW branch 0x449F2F..0x449FA4 calls vt20 on every drawFrame where the LW client is active. It is not covered by any existing site: sites.json has nothing in 0x449E00..0x44A300 except SCENE_OPEN 0x449DAB, SUBT_DELETE 0x44A1FD (6 bytes) and SCENE_RESTORE 0x44A271. The only entry in 0x49A000..0x49C000 is GATE_LWM 0x49AAD4. None of the AotR hooks are nearby.
- Restore window. Every drawFrame exit reaches SCENE_RESTORE at 0x44A271:
  - the LW path jumps to 0x44A0AD and falls through;
  - the jmp at 0x449F00 goes to 0x44A26B;
  - the jne instructions at 0x44A24B, 0x44A25A and 0x44A269 go to 0x44A26D;
  - there is no other ret before the epilogue.
- Recompute on restore. 0x49B4A5 (fastcall, ECX=lw) rebuilds the look-at +0x110..+0x118 through 0x49AF11 (also the +0x188 vt54 transform), the camera transform (+0xC0 vt54) and the view plane/clip range. It reads only +0xF8..+0x10C, +0x11C, +0x134, +0x198, +0x1C/+0x20, +0xD8/+0xDC and +0x13C, and those last two depend only on the fade, which is not interpolated. Its audio call vt58 is 0x450A08, which just does "mov byte [ecx+0x6AB],1; ret", so it is idempotent.
- Angle. +0x11C is clamped to [-1,1] at 0x49B8F9..0x49B927 and never wrapped, so a linear midpoint is valid.

Minor notes, none of which block the site:
(a) 0x49B618 also pushes zoom-derived values to the +0x18C/+0x190/+0x194 objects through 0x49A4FB. After restore these keep the halfway values until the B draw rewrites them with stock values. That is presentation-only and is not restored by 0x49B4A5. If nothing between the A and B draws reads them, that is acceptable; LwPresentRestore could also redo those three calls.
(b) The site stays inert until phase 6 lifts BlockReason's LW refusal (frame_ctl.cpp:103/114).
(c) The site changes the SCENE_RESTORE stub's fast path, which is an existing site, so that edit must be made together with this one.
CORRECTED: none (site confirmed as proposed).

## VERIFY SITE LW_B_SKIP_SHROUD -> confirmed
The only file I created is the scratch script <analysis workspace>/lw_tmp/chk_shroud.py.

Results of the checks:

1. **Bytes.** Read with pefile: `39 98 c0 37 00 00 74 40` at 0x449D63. The bytes are identical in the the analysed copy of game.dat, rotwk\game.dat, rotwk\game820.dat, aotr\zGameDats\game.dat and delayfix.dat. The LW-branch test bytes at 0x449F2F are also identical in all five. Capstone shows `cmp [eax+0x37c0],ebx` (6 bytes) and `je 0x449dab` (2 bytes). The span is 8 bytes on instruction boundaries, and `e9 rel32 90 90 90` fits.

2. **Branch into the span.**
   - A brute rel8/loop/jcc8, rel32 call/jmp and jcc32 scan of the executable sections finds nothing that targets 0x449D64..0x449D6A.
   - A whole-image abs32 scan finds nothing either, and listing.asm has no references.
   - No overlap with existing sites: C7_PARTMGR_DRAW is 0x449D40+11, INT_TREES is 0x449D55+5, and SCENE_OPEN is 0x449DAB+5, so it is adjacent, not overlapping. There is no AotR hook nearby, and no site in 0x472C00..0x474000 (the shroud passes are not gated today).

3. **30-mode / A path.** It re-executes the displaced cmp and je exactly, with the same EAX and EBX and identical targets. RUNCNT/SKIPCNT are `inc dword [g_siteRun/Skip+idx*4]`, so they only touch flags, and that happens before the cmp. The stub uses only ECX.

4. **Liveness.**
   - **At 0x449D6B:** EAX is reloaded (`mov eax,[esi]`) and ECX is reloaded (`mov ecx,esi`).
   - **ECX/EDI dead at 0x449DAB (the strongest argument):** stock already reaches 0x449DAB with ECX holding a different value on each path:
     - =0 via 0x449D53,
     - = the 0x4684FB return via 0x449D61,
     - clobbered by the 0x473CAD call,
     - =0 via 0x449D9E.

     EDI is still the caller's value (never assigned in 0x449D02..0x449D53) on the 0x449D53 and 0x449D61 paths, and the view on the 0x449D9E path. So neither register can be live there.
   - **The one EDI use downstream:** `mov [eax+0x3c],edi` at 0x449F5C is preceded on every path by `xor edi,edi; inc edi` at 0x449EA0.
   - **SCENE_OPEN** reloads EAX (`mov eax,[0xDE412C]`) and its own live_after says EDI is dead.
   - Flags are dead at both resume points, there is no x87 or XMM use, and fs:[0x24] is the TEB thread ID, the same check STUB_TREES uses.

5. **B path.** The stub's predicate copies the stock LW-branch test byte for byte: [0xDE4958] non-null, +0x18 != 0, +0x19 == 0 (0x449F2F..0x449F41).
   - When that holds, drawFrame reaches only the LW draw vt20 at 0x449F45 (then UI, or a skip when +0x14==2), the load-screen path (0x449EC3) or the no-render exit 0x44A230. RenderViews (0x449FE0, display vt98) is never reached, so the tactical shroud texture goes unused by the tactical render path.
   - Skipping jumps to 0x449DAB, which is SCENE_OPEN's patched start, entered exactly as stock enters it.
   - Even if the LW scene sampled the shroud texture, B would see the texture A uploaded one render earlier. W3DShroud::interpolateFogLevels (0x472CF2) runs on timeGetTime with a static prevTime at 0xDC7900, so this is the same as a stock frame-rate difference and is visual-only.
   - 0x472EF4 only interpolates the fog bytes, then locks, memcpys, unlocks and releases the shroud texture (0x516090/0x516110/0x576550), so skipping it leaves no other state behind.

6. **It really runs per render on the LW map.** drawFrame skips updateViews when GL+0x125 is set (0x449D29) but falls through to particles, trees and this check.
   - [0xDC78EC] is created in W3DDisplay init (0x491512, also 0x465F52 and 0x46C349) and cleared only in the display destructor (0x491A34), so it is non-null throughout play.
   - +0x3878 (the W3DShroud from 0x472BEC) is always created in the constructor 0x46C1F3. +0x387C exists only when GD+0xC6A is set.
   - +0x37C0 stays non-null after the first map load, normally the shell map. So in stock, 0x472EF4 runs on every LW render and is not covered by any existing gate.

**Evidence correction (text only, the mechanics are unaffected):** the +0x37C0 writers are not only 0x46CF3A and 0x46D107.
   - The constructor 0x46C325 sets it to 0.
   - The virtual REF_PTR_SET 0x4E080C (callers=0, vtable) writes it via EDI=&+0x37C0 at 0x4E0836, but skips the write when the new pointer is null.
   - 0x46CF3A gets a non-null value: its only caller is 0x4E1366, which dereferences [edi+8] right after.

So "never cleared after a map load" still holds. The corrected JSON below updates only the evidence and purpose text; every functional field is unchanged.
CORRECTED: {
 "id": "LW_B_SKIP_SHROUD",
 "phase": "6",
 "group": "LW draw cost (optional)",
 "address": "0x449D63",
 "length": 8,
 "original_hex": "39 98 c0 37 00 00 74 40",
 "original_asm": [
  "0x449d63: 39 98 c0 37 00 00  cmp dword ptr [eax+0x37c0], ebx   ; EAX=[0xDC78EC] terrain render object (non-null, checked at 0x449D5F), EBX=0",
  "0x449d69: 74 40  je 0x449dab"
 ],
 "kind": "jmp_detour",
 "replacement_hex": "e9 <rel32:CAVE_LW_B_SHROUD> 90 90 90",
 "stub": "CAVE_LW_B_SHROUD (pure asm, DLL globals only):\n  cmp byte [g_skipB],0 ; je .stock\n  mov ecx,fs:[0x24] ; cmp ecx,[g_mainTid] ; jne .stock   ; ECX is dead on every continuation\n  mov ecx,[0xDE4958] ; test ecx,ecx ; je .stock\n  cmp byte [ecx+0x18],0 ; je .stock\n  cmp byte [ecx+0x19],0 ; jne .stock     ; same test as stock 0x449F2F..0x449F41: the LW branch (0x449F45) will draw this frame, not RenderViews\n  SKIPCNT ; jmp dword [T_449DAB]         ; B-render: skip the shroud render-to-texture pre-pass 0x449D6B..0x449DA6 (vt90 getView + 0x472EF4 + 0x473CAD)\n.stock:\n  RUNCNT\n  cmp dword [eax+0x37C0],ebx             ; displaced\n  je .t\n  jmp dword [T_449D6B]\n.t: jmp dword [T_449DAB]                 ; displaced 'je 0x449DAB'. This target is the SCENE_OPEN span start, entered exactly as in stock.\n\n30 mode and A-renders: identical to the original two instructions.",
 "resume_address": "0x449D6B (fall-through) / 0x449DAB (je target and skip)",
 "live_after": "- At 0x449D6B: ESI=display and EBX=0 are live. EAX is reloaded by 'mov eax,[esi]'. ECX is reloaded ('mov ecx,esi'). EDX and EDI are dead; EDI is assigned at 0x449D75.\n- At 0x449DAB: EBX=0, ESI and EBP are live. EAX is reloaded by SCENE_OPEN's displaced 'mov eax,[0xDE412C]'.\n- ECX, EDX and EDI are dead at 0x449DAB. Stock already reaches it with differing values: ECX=0 via 0x449D53, the 0x4684FB return via 0x449D61, clobbered via the 0x473CAD call. EDI is the caller's value via 0x449D53/0x449D61 and the view via 0x449D9E. The only later EDI read (0x449F5C) is preceded by 'xor edi,edi; inc edi' at 0x449EA0.\n- EFLAGS are dead at both. x87 depth 0, XMM dead.",
 "branch_into_span_check": "- Brute rel8/loop/jcc8, rel32 call/jmp, jcc32 scan of the executable sections and a whole-image abs32 scan for 0x449D64..0x449D6A: 0 hits. listing.asm has no references.\n- Bytes are identical (site and the 0x449F2F LW test) in rotwk\\game.dat, rotwk\\game820.dat, aotr\\zGameDats\\game.dat and delayfix.dat.\n- No overlap: C7_PARTMGR_DRAW 0x449D40+11, INT_TREES 0x449D55+5, SCENE_OPEN 0x449DAB+5. No AotR hook nearby, and no existing site on 0x472EF4/0x473CAD.",
 "purpose": "Per-render cost on the LW map that should not double.\n\nThe shroud render-to-texture passes 0x472EF4/0x473CAD interpolate the tactical fog (real time, 0x472CF2 timeGetTime prevTime at 0xDC7900) and upload it to the shroud texture (lock 0x516090, memcpy, unlock 0x516110, release 0x576550).\n\n[0xDC78EC] lives from W3DDisplay init (0x491512) to the display destructor (0x491A34). +0x3878 (W3DShroud, 0x472BEC) is always created in the constructor 0x46C1F3. +0x37C0 is non-null after the first map load (normally the shell map) and is never set back to null. In stock these passes therefore keep running every render on the LW map, using the previous map.\n\nWhen the stock LW test (DE4958+0x18 && !+0x19) holds, drawFrame goes to the LW draw vt20 0x449F45, the load-screen path or the no-render exit. It never reaches RenderViews (0x449FE0), so the tactical shroud texture goes unused by the tactical render path.\n\nA-renders keep stock; B-renders skip work whose result is discarded. Even if the LW scene sampled the texture, B would see A's upload from one render earlier: a real-time, visual-only difference.",
 "evidence": "- drawFrame 0x449D4B..0x449DAA, LW test 0x449F2F..0x449F51, RenderViews 0x449FE0 only on the 0x449FA9 path.\n- +0x37C0 writers: ctor 0x46C325 (=0); 0x46CF3A (map-load 0x46CF01, sole caller 0x4E1366, which dereferences the new pointer, so it is non-null); 0x46D107; virtual REF_PTR_SET 0x4E080C/0x4E0836, which skips the write when the new pointer is null. None clears it after a load.\n- 0x472EF4 decompile: fog interpolate 0x472CF2 + texture lock/copy/unlock only.\n- G2 notes: the shroud is real-time (0x472CF2/0x473AAB).",
 "risks": "- Optional. Gain unmeasured: check with telemetry (B-render ms on the LW map) before adding.\n- +0x387C exists only when GD+0xC6A is set; +0x3878 always exists.\n- If a future change makes the LW branch consume the shroud texture, this site must go. Even then the difference is visual-only (one A upload, about 16 ms old)."
}

## VERIFY SITE LW_B_SKIP_SHADOW_WATER -> needs_changes
My only writes were two scratch scripts in <analysis workspace>\lw_tmp (v_449dbd.py, clos.py).

The site is sound except for the counter on the 30-mode path. Corrected JSON below.

What I checked:
1. **Bytes.** pefile on ghw\bin\game.dat (SHA cc08275d..., identical to rotwk\game.dat) gives 39 1d 38 7a dc 00 74 43 at 0x449DBD. That is cmp [0xDC7A38],ebx followed by je 0x449E08, as claimed. Both instructions are 8 bytes and start on instruction boundaries.
2. **Branch into the span.** I scanned all executable sections for rel8, rel32 and jcc32 branches. Nothing lands in 0x449DBE..0x449DC4. listing.asm has only 'JZ 0x00449dbd' at 0x449DB2, which targets the span start and is allowed. The image-wide abs32 scan found 0x5F31C0 and 0x5F31CE. Both are rel32 operand bytes of 'call 0xA3CF84' at 0x5F31BF and 'call 0xA3CF90' at 0x5F31CD, so they are false positives.
3. **Overlap.** No AotR hook or existing site overlaps the span. The nearby sites are C7_PARTMGR_DRAW 0x449D40, INT_TREES 0x449D55, SCENE_OPEN 0x449DAB..0x449DAF, SUBT_DELETE 0x44A1FD and SCENE_RESTORE 0x44A271. No site in sites.json covers 0x47D5C9, 0x47F25A or 0x47F695.
4. **It runs on the LW map.**
   - drawFrame takes the LW path at 0x449D29: GL+0x125 set skips updateViews, then it reaches 0x449D40 and 0x449DAB.
   - At 0x449DB4 the only way out is GL+0x6D, which is the map-loading flag: set at 0x62A176 in FUN_0062A11A (only caller is startNewGame), cleared at 0x6315BF in startNewGame and at 0x6300B3 in the constructor. It is 0 on a settled LW map, so 0x449DBD runs on every LW render.
   - The stub's LW test (byte [DE4958]+0x18 set, +0x19 clear) is the same as the LW-branch test at 0x449F2F..0x449F41.
5. **Resume points.**
   - EBX=0 (from 0x449D12) and ESI=display are untouched.
   - ECX is dead at all three resume points (written at 0x449DDA and 0x449E0A; only pushed as a placeholder at 0x449E98). So the stub may clobber ECX, including on the stock path of a tactical B-render.
   - EAX is reloaded at 0x449DC5, 0x449E08 and 0x449E70. EDI is dead: stock already reaches 0x449E70 with an arbitrary EDI from the jne at 0x449DB7, and EDI is rewritten at 0x449EA0 or 0x44A0ED or popped in the epilogue.
   - Flags are dead at all three points. x87 is empty.
6. **Code skipped on B.** The skipped range 0x449DC5..0x449E6B only renders the shadow map (0x47D5C9) and the water reflection (0x47F695/0x47F25A). The display vt+0x90 call is a pure getter (vt 0xBD9C28+0x90 = 0x7B0F25: mov eax,[ecx+0x1C]; ret).
   - A direct-call closure from 0x49B618 and 0x49C3DF (641 functions) never references 0xDC7A38 or 0xDC7A40. Virtual calls are not resolved by this check.
   - Even if a stale texture were used, B shows the same LW camera as A, so it would not be visible. The skip only drops these two passes to stock cadence (A renders only).
7. **What needs changing.**
   - **Counter on the stock path:** the stub runs RUNCNT before the displaced cmp, so in 30 mode and on A-renders it does extra work (a counter write and a flags change). That breaks the rule that the 30-mode path is byte-exact.
   - **Undefined macros:** SKIPCNT and RUNCNT are not defined anywhere in sites.json, the src files or PLAN.md.
   - **Fix:** remove the counter from the stock path. Keep at most one counter increment on the skip path, where flags are dead. Use plain rel32 jumps instead of the T_ table.
   - The gain is still unmeasured and the site remains optional.
CORRECTED: {
 "id": "LW_B_SKIP_SHADOW_WATER",
 "phase": "6",
 "group": "LW draw cost (optional)",
 "address": "0x449DBD",
 "length": 8,
 "original_hex": "39 1d 38 7a dc 00 74 43",
 "original_asm": [
  "0x449DBD: 39 1d 38 7a dc 00  cmp dword ptr [0xdc7a38], ebx   ; shadow-map manager",
  "0x449DC3: 74 43              je 0x449e08"
 ],
 "kind": "jmp_detour",
 "replacement_hex": "e9 <rel32:CAVE_LW_B_SHADOW_WATER> 90 90 90",
 "stub": "CAVE_LW_B_SHADOW_WATER (pure asm, mid-stream).\nEntry = original state at 0x449DBD: EBX=0, ESI=W3DDisplay this, EAX=GL or 0 (dead), ECX/EDX/EDI dead, EBP=drawFrame frame, x87 empty, flags dead.\n  cmp byte [g_skipB],0 ; je .stock                 ; 30 mode and A-renders: straight to the original\n  mov ecx,fs:[0x24] ; cmp ecx,[g_mainTid] ; jne .stock\n  mov ecx,[0x00DE4958] ; test ecx,ecx ; je .stock  ; same LW-branch test as 0x449F2F..0x449F41\n  cmp byte [ecx+0x18],0 ; je .stock\n  cmp byte [ecx+0x19],0 ; jne .stock\n  inc dword [g_cntLwShadowWaterSkip]               ; optional telemetry, only on the skip path; flags are dead at 0x449E70\n  jmp 0x00449E70                                   ; rel32. 60-mode B-render on the LW branch: skip UpdateShadowMap (0x449DC5..0x449E03, 0x47D5C9) and UpdateWaterReflection (0x449E08..0x449E6B: vt+0x90 getter 0x7B0F25, 0x47F695, 0x47F25A)\n.stock:\n  cmp dword [0x00DC7A38],ebx                       ; displaced, same encoding 39 1d 38 7a dc 00\n  je 0x00449E08                                    ; displaced 'je 0x449E08', re-encoded as rel32 jcc\n  jmp 0x00449DC5                                   ; rel32\n30 mode and A-renders (g_skipB==0): only the g_skipB compare runs before the original cmp/je, which recomputes the flags, so registers, memory and control flow are identical to stock. Tactical 60 B-renders (no active LW client) reach .stock with ECX clobbered; ECX is dead there (written at 0x449DDA or 0x449E0A before any read).",
 "resume_address": "0x449DC5 (fall-through) / 0x449E08 (je target) / 0x449E70 (B skip)",
 "live_after": "- EBX=0 and ESI=display are live at all three resume points and are preserved.\n- EAX is dead: reloaded at 0x449DC5, 0x449E0A and 0x449E70.\n- ECX is dead: written at 0x449DDA or 0x449E0A, and on the skip path only pushed as a placeholder at 0x449E98 and overwritten by the fstp.\n- EDI is dead: written at 0x449E1A, 0x449EA0 or 0x44A0ED, or popped in the epilogue. Stock already reaches 0x449E70 with an arbitrary EDI via the jne at 0x449DB7.\n- EDX is dead.\n- EFLAGS are dead at 0x449E70; at 0x449DC5 and 0x449E08 they are recomputed by the displaced cmp.\n- x87 depth 0. XMM dead.",
 "branch_into_span_check": "- pefile scan of all executable sections (rel8, rel32, jcc32): no target in 0x449DBE..0x449DC4.\n- listing.asm: only 'JZ 0x00449dbd' at 0x449DB2, which targets the span start (allowed, the e9 is there).\n- Image-wide abs32 scan: 0x5F31C0 and 0x5F31CE are the rel32 operand bytes of 'call 0xA3CF84' at 0x5F31BF and 'call 0xA3CF90' at 0x5F31CD, i.e. false positives.\n- No overlap with SCENE_OPEN (0x449DAB..0x449DAF), INT_TREES (0x449D55), C7_PARTMGR_DRAW (0x449D40), SUBT_DELETE (0x44A1FD), SCENE_RESTORE (0x44A271) or any AotR hook.",
 "purpose": "Avoid running the tactical shadow-map pass (DC7A38, 0x47D5C9) and the water-reflection pass (DC7A40, 0x47F25A) twice per stock render on the LW map. Their consumers are RenderViews and the shadow apply calls 0x47BA74/0x47AF59 at 0x449FD7/0x449FFA, which the LW branch (0x449F43 -> vt+0x20 0x49B618) skips. On A-renders and in 30 mode they run as stock, so on the LW map they keep the stock cadence.",
 "evidence": "- drawFrame 0x449CF8: 0x449D29 skips updateViews when GL+0x125 is set; 0x449DB4 exits only on GL+0x6D (map-loading flag: set 0x62A176 in FUN_0062A11A, cleared 0x6315BF in startNewGame and 0x6300B3 in the constructor), so 0x449DBD runs on every settled LW render.\n- LW branch test 0x449F2F..0x449F41 matches the stub predicate.\n- Display vt+0x90 = 0x7B0F25 'mov eax,[ecx+0x1C]; ret' (pure getter).\n- Direct-call closure from 0x49B618 and 0x49C3DF (641 functions) has no reference to 0xDC7A38 or 0xDC7A40. Virtual calls are not resolved.\n- Writers of 0xDC7A38 are only 0x499F18 and 0x49A128 (creation and destruction).",
 "risks": "- Optional. Gain unmeasured; the scene may be nearly empty on the LW map. Measure with g_cntLwShadowWaterSkip and frame time before keeping it.\n- Virtual calls inside the LW draw were not resolved. If any of them sampled the shadow or reflection texture, B would show A's texture, rendered with the same LW camera, so the difference is not visible.\n- The shadow dirty-state machine (+0x28, 0xD9952C) may defer a rebuild by one render; the next A-render recomputes it."
}

## VERIFY FINDING LW draw path has no per-draw integrators; no new A-only gate sites needed in LW- -> confirmed
I checked the finding against game.dat using Ghidra decompiles, capstone and a static callgraph closure. Its core claim holds. One side claim is wrong but changes nothing.

1. The LW draw is recomputed on every render.
   - 0x49B618 and its viewport twin 0x49BEFD write only +0x13C, +0xD8 and the LW display state behind them. Each value is a lerp of constants (0xDCB86C..0xDCB880) by +0x198.
   - Both call 0x49A4FB, which sets FX opacity through 0x50E244 from +0x134. 0x50E244 recurses into sub-objects and sets values; it does not accumulate.
   - 0x49B4A5 and 0x49AF11 rebuild the camera into locals and into +0x110..+0x118 from +0xF8..+0x10C, +0x1C/+0x20, +0x11C and +0x198.
   - Every writer of +0x134 and +0x198 is outside the draw: 0x49AAB8 (the LW view tail, A-only through GATE_GC_LWVIEW), 0x49BB8B, 0x49BE23, 0x49C185 and 0x49C79C (init).
   - The tail calls of the LW scene render vt5C 0x49C3DF go to display+0x2C vt58/vt5C. That object is built by ctor 0x474A6A with vtable 0xBDC5C0, and both slots are no-ops (0x9F3A3C `ret 4`, 0x63F3BF `ret`).
   - The particle manager calls there (vt48 0x44C3E2 sets +0xA8=1; vt40 0x44C84A is the particle draw) are shared with battle.
   - The terrain path 0x4F43A6 is entered only if [0xDD1718]!=0, and it is shared code already covered by the INT_OVL_*, INT_RIVER_* and INT_TERRAIN_TILEUPD sites.

2. The per-render On_Frame_Update loop is covered.
   - The LW scene (vtable 0xBDE848) runs the same update-list loop as RTS3DScene 0x46FE84: list +0x78/+0x7C, vt34.
   - The existing on-frame-update sites are ptr_slot patches on the class vtables: INT_LIGHTPULSE at 0xBDC00C and INT_TERRAIN_TILEUPD at 0xBE4794. They therefore fire in any scene, including the LW scene.
   - A vtable scan found no other non-trivial On_Frame_Update in LW code. The only others are 0x5A1060 and 0x5A5E60, which are shared, non-LW and idempotent.

3. Callgraph closure.
   - The static closure from 0x49B618, 0x49BEFD, 0x49C3DF, 0x49C384 and 0x975441 has 507 functions.
   - Its LW-range members are only these: 0x499D6D, 0x499EAF, 0x49A4D4, 0x49A4FB, 0x49AF11, 0x49B4A5, 0x49B618, 0x49BEFD, 0x49C215, 0x49C384, 0x49C3DF, 0x6B3174, 0x6B5DE0, 0x6B5E90 and 0x975441. All are readers or idempotent.
   - 0x975441 makes its one-shot call (FUN_00840024) once, latched by +0x2A0.
   - The 0x67xxxx and 0x68xxxx functions in the closure are reached only through the shared terrain path 0x4F43A6.

4. The other claims check out.
   - D3D statistics counters 0xDD34D8/0xDD34A4: confirmed.
   - Audio vt58 0x450A08 is `mov byte [ecx+0x6AB],1`: confirmed.
   - The stepper sets GC+0xC8 at 0x6325F8..0x6325FB (`xor ebx,ebx; inc ebx; mov [eax+0xC8],bl`) on every non-halted step: confirmed.
   - The LW branch's UI block 0x449F57..0x449FA4 is the same InGameUI/WindowManager vt30/vt158 sequence as the battle path 0x44A04E..0x44A096. The tail from 0x44A0AD is shared, and 0x65CF05 is caught by the INT_OVL_FADE func_detour.

5. One inaccuracy. DAT_00D9B03C is not "set and cleared within the render".
   - 0x49B657 and 0x49C063 only store 1. It is cleared only in LW init 0x49C7B7, and saved/restored around 0x462048..0x46207F.
   - It is still harmless, because the store is idempotent.

6. Residual risk not covered by this static proof: LW-only GameWindow or APT draw callbacks that run inside the shared UI draw block on the LW map. The planned telemetry run should cover them.

CORRECTED REC: Keep the LW draw (display vt20 0x49B618 / 0x49BEFD, LW scene vt5C 0x49C3DF, 0x49C384/0x49C215, APT window 0x975441) allow-listed for B-renders as it is. No new A-only gate sites are needed in LW-specific draw code. Correct the side note: DAT_00D9B03C is an idempotent set to 1 that is never cleared within the render, not a set-then-clear.

Put the phase-6 effort into two things:
1. **Mode controller.** Let BlockReason accept GL+0x125 / mode 8.
2. **Camera presentation (LW_SCENE_PRESENT).** On A-renders, present the LW camera inputs that the draw reads half a step back: +0x134 zoom, +0x198, +0x1C/+0x20, +0xF8..+0x10C and +0x11C. These are written only by A-only LW view code (0x49AAB8 and friends), so restore them after the draw.

Verify on the LW map with telemetry:
- The GATE_GC_LWVIEW, GATE_GC_LWUI, GATE_GC_LWXLAT, GATE_GC_DISPUPD_LW and GATE_LWM run rates are 30.3/s.
- The logic-RNG trace per LW tick matches stock.
- The INT_LIGHTPULSE and INT_TERRAIN_TILEUPD skip counters on the LW map are plausible.
- LW-only HUD window and APT animations visibly run at stock speed. This is the one path the static proof did not close: window/APT draw callbacks that run on the LW map inside the shared InGameUI/WindowManager draw block.

## VERIFY FINDING Without a presentation window the LW map at 60 FPS shows each camera picture twi -> confirmed
I checked this against game.dat with capstone, listing.asm and a vtable dump, and the finding holds.

1. The LW draw only reads the camera fields. 0x449F45 calls display vt20 0x49B618 (vtable 0xBDE918). That function:
   - derives +0x13C and +0xD8 from fade +0x198 and global tables;
   - sets object alphas through 0x49A4FB from zoom +0x134;
   - calls 0x49B4A5, which builds the matrix through 0x49AF11 from +0x134/+0xD8/+0xDC/+0x13C, sets the camera transform on +0xC0 (vt54) and clip planes (0x5337C0), then renders with 0x518000.
   Every write in the draw is derived from the inputs again on each draw. None of them moves +0xF8/+0xFC/+0x100, +0x11C, +0x134 or +0x198.

2. Every writer of those fields sits outside the draw. Vtable 0xBDE918 slots:
   - +0x2C = 0x49B799 (pan/rotate; it calls vt58 at 0x49B8AF and writes +0x11C at 0x49B8E6..0x49B927);
   - +0x54 = 0x49A9CF (zoom velocity +0x138);
   - +0x58 = 0x49AA24 (sets +0xF8..+0x100 and clamps it to bounds);
   - +0x68 = 0x49A9A6 (+0x134, clamped to 0..1);
   - +0x7C = 0x49A6A8 (+0x11C);
   - +0x9C = 0x6BFB88 (moveTo set-up only);
   - +0x6C = 0x49AAB8 (zoom integration at 0x49AB21, fade at 0x49ABB4/0x49ABFD).
   Who calls them:
   - FUN_008392A7 calls vt2C at 0x8395FF and 0x839663 and vt54 at 0x8396A0. Its only caller is 0x6484BD (GATE_GC_LWXLAT).
   - vt6C is called at 0x6484D9 (GATE_GC_LWVIEW).
   - moveTo 0x6BF78E calls vt68, vt58 and vt7C. It is reached only by the tail-jump at 0x6C0E86 from 0x6C0E4D, whose only caller is 0x49AAD4 (GATE_LWM).
   The evidence list is incomplete: vt5C→vt54 is also reached from 0x838E5A (called by 0x646198 and 0x6461AE), and vt9C from 0x838D3A, 0x7FEB59 and 0x90498E. None of these is in the draw, so the conclusion still holds.

3. A and B therefore draw the same LW picture. Between the A draw and the B draw only the halted step runs, and it does no logic. In the B-render clientUpdate everything except the draw is gated. Scroll, zoom and fly-to will advance only on A-renders and then show twice, so camera motion looks like 30 Hz at 60 renders per second. 60 mode is blocked on the LW map today (BlockReason checks GL+0x125 and mode 8), so this is a gap phase 6 must close, not a live bug. G5-06's "LW draw vt20 run on B as is" covers no camera presentation.

4. The angle claim is right: 0x49B8F9..0x49B927 clamps +0x11C to plus or minus [0xD99B60] and does not wrap it. vt58 clamps the position to a box (+0x124..+0x130) and vt68 clamps zoom to 0..1. All clamp regions are convex, so a halfway lerp stays valid.

5. The proposed site is sound:
   - 0x449F43 holds `8B 01 FF 50 20` (`mov eax,[ecx]` + `call [eax+0x20]`), 5 bytes on instruction boundaries.
   - Nothing branches into 0x449F44..0x449F47 (listing.asm and a raw rel8/rel32/jcc32 scan of the executable sections).
   - EAX is reloaded at 0x449F48.
   - It matches no AotR hook and no existing site.
   - Every drawFrame exit passes SCENE_RESTORE at 0x44A271, as sites.json documents, so the LW path is covered by the existing restore.
CORRECTED REC: Add a phase-6 call_gate LW_SCENE_PRESENT at 0x449F43. It replaces `8B 01 FF 50 20` (`mov eax,[ecx]; call [eax+0x20]`, ECX = [0xDE4958], ret 0x449F48, EAX dead afterwards). The 30-mode path and the B path run the exact original `mov eax,[ecx]; call [eax+0x20]`.

On an A-render in 60 mode, on the main thread:
1. Record M_k = LW client fields {+0xF8, +0xFC, +0x100, +0x11C, +0x134, +0x198} plus a renderId, the same way the tactical g_rec does.
2. If M_{k-1} came from the previous A-render (renderId − 2) and the move is not a cut, save the live fields and write lerp(M_{k-1}, M_k, 0.5).
   - A cut is any of: moveTo with zero duration, setActive/load, return from a battle, a change in client state +0x14, or a jump above a threshold.
   - The lerp is valid because the position is box-clamped, the angle is clamped (not wrapped) and zoom is clamped to 0..1.
3. Call vt20, then restore the fields in the existing SceneRestore at 0x44A271, which every drawFrame exit passes.

The B-render draws the restored C_k unchanged, and its draw recomputes the derived +0x13C, +0xD8, alphas and +0xC0 camera transform. That way the picking in the next A-render's clientUpdate sees stock values. Flush the LW history on every mode switch, reset or BlockReason change.

Keep all field writers A-only:
- GATE_GC_LWXLAT (0x8392A7 → vt2C/vt54);
- GATE_GC_LWVIEW (vt6C: zoom 0x49AB21, fade 0x49ABB4/0x49ABFD);
- GATE_LWM (moveTo 0x6BF78E → vt68/vt58/vt7C).

Add no per-draw halving of these integrators. Use one mechanism only: A-only stepping plus A-side presentation, coordinated with whoever owns the LW camera input. The evidence should also list the vt5C path (0x838E5A from 0x646198/0x6461AE) and the vt9C moveTo set-up callers (0x838D3A, 0x7FEB59, 0x90498E). They are outside the draw, so the conclusion is unchanged.

## SITE {
 "id": "LW_SCENE_PRESENT",
 "phase": "6",
 "group": "LW draw presentation",
 "address": "0x449F43",
 "length": 5,
 "original_hex": "8b 01 ff 50 20",
 "original_asm": [
  "0x449f43: 8b 01  mov eax, dword ptr [ecx]   ; ECX = LW client [0xDE4958] (vtable 0xBDE918)",
  "0x449f45: ff 50 20  call dword ptr [eax+0x20]   ; LW scene draw 0x49B618 (thiscall, no args, plain ret at 0x49B798)"
 ],
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:LW_PRESENT_STUB>",
 "stub": "LW_PRESENT_STUB (call-boundary stub; pure asm plus one cdecl C++ call). Entry: [esp]=0x449F48. ECX = LW client, already checked by 0x449F35..0x449F41: non-null, byte[+0x18]!=0, byte[+0x19]==0. EBX=0, ESI=W3DDisplay, EDI=1, x87 empty.\n  cmp byte [g_m60],0            ; je .draw\n  cmp byte [g_inB],0            ; jne .draw   (B-render presents stock C_k)\n  cmp byte [g_inClientUpdate],0 ; je .draw\n  mov eax,fs:[0x24] ; cmp eax,[g_mainTid] ; jne .draw   ; EAX dead, the displaced mov reloads it\n  push ecx ; push ecx ; call LwPresentBegin_A ; add esp,4 ; pop ecx\n.draw:\n  mov eax,[ecx]                 ; displaced\n  jmp dword [eax+0x20]          ; displaced call as a tail call: 0x49B618 returns to 0x449F48 with the stock stack\n\nC++ LwPresentBegin_A(lw), A-renders only:\n- Take S = {float +0xF8,+0xFC,+0x100 (position, set by vt58 0x49AA24); +0x11C (angle, vt7C 0x49A6A8, clamped by 0x49B8F9..0x49B927, never wrapped); +0x134 (zoom 0..1, vt68 0x49A9A6 and 0x49AB21); +0x198 (fade); dword +0x14 (state); +0x104..+0x10C (zoomed-out centre)}.\n- Interpolate only if ALL hold: g_featCamInterp; rec.valid; rec.lw==lw; rec.renderId==g_renderId-2; S.state==rec.state; state not 2 or 3; both fades==0; the centres are equal; |dpos| <= cut (suggest 400 world units); |dzoom| <= 0.5.\n- Then g_lwSaved=S, write pos=(rec+S)/2, angle=(rec+S)/2, zoom=(rec+S)/2 into the client, and set g_lwSwapActive=1.\n- Always set rec=S, rec.lw=lw, rec.renderId=g_renderId, rec.valid=1.\n\nEnd of the window is the existing SCENE_RESTORE (0x44A271), which every drawFrame exit passes:\n- STUB_SCENE_RESTORE's fast path gets 'cmp byte [g_lwSwapActive],0 / jne WORK'.\n- SceneRestore() calls LwPresentRestore(). If g_lwSwapActive and on the main thread, it writes g_lwSaved back to +0xF8/+0xFC/+0x100/+0x11C/+0x134.\n- It then calls thiscall 0x49B4A5(ECX=lw). That recomputes the look-at +0x110..+0x118 (0x49AF11), the LW camera +0xC0 transform, view plane and clip range, and the cloud-border +0x188 transform, i.e. exactly the state stock render k leaves (+0x13C/+0xD8 are unchanged because the fade is never interpolated). It also sets TheAudio's idempotent dirty flag (vt58 0x450A08).\n- Finally g_lwSwapActive=0.\n\nInvalidate rec in the same places as camera.cpp g_rec (mode switch, RESET hook).\n\n30 mode, B-render, non-main thread: byte-for-byte the original 'mov eax,[ecx]; call [eax+0x20]'.",
 "resume_address": "0x449F48",
 "live_after": "- 0x449F48 'mov eax,[0xDE4958]' reloads EAX. ECX and EDX are dead (0x449F5F reloads ECX). EFLAGS are dead (cmp at 0x449F4D).\n- Live and preserved: EBX=0 (bl compares at 0x449F6A, stores at 0x449F8D/0x44A0A5), ESI=W3DDisplay this, EDI=1 (stored at 0x449F5C, pushed at 0x44A0D0), the EBP frame.\n- x87 depth 0. XMM dead: xmm0 was last used by the comiss at 0x449F18.",
 "branch_into_span_check": "- Brute rel8/rel32/jcc32 decode of all executable bytes plus a whole-image abs32 scan for 0x449F43..0x449F47: 0 hits (none even to 0x449F43). listing.asm has no operand references.\n- These ranges are byte-identical in rotwk\\game.dat, aotr\\zGameDats\\delayfix.dat and rotwk\\game820.dat: 0x449F20..0x449F5F, 0x49B4A5..0x49B617, 0x49B618..0x49B798, 0x49AF11 (+0x200), 0x44A260..0x44A27F.\n- No AotR hook and no existing site within 0x449F20..0x449F60. The nearest are SCENE_OPEN 0x449DAB and SCENE_RESTORE 0x44A271.",
 "purpose": "Smooth LW strategic-map camera at 60 FPS.\n\nThe LW scene draw rebuilds its camera purely from client fields every render: 0x49B618 → 0x49B4A5 → 0x49AF11 → camera vt54. Every writer of those fields is A-only:\n- pan and rotate: GATE_GC_LWXLAT 0x6484BD → 0x8392A7 → vt2C 0x49B799;\n- zoom velocity and fade: GATE_GC_LWVIEW 0x6484D7 → vt6C 0x49AAB8;\n- moveTo: GATE_LWM 0x49AAD4 → 0x6C0E4D → 0x6BF78E (vt68/vt58/vt7C).\n\nSo without this site the A- and B-renders show the identical LW picture. With it, A presents C_{k-½} and B presents stock C_k, the same constant 16.7 ms delay as the tactical camera (PLAN §1.6).",
 "evidence": "- drawFrame disassembly 0x449F2F..0x449FA4 (LW branch).\n- Decompiles of 0x49B618, 0x49B4A5 and 0x49AF11. The camera inputs are +0xF8..+0x100, +0x104..+0x10C, +0x11C, +0x134, +0x198, +0x1C/+0x20 and mapinfo [0xDE3C08].\n- Setters: vt58 0x49AA24, vt68 0x49A9A6, vt7C 0x49A6A8. Integrators: 0x49AB21, 0x49ABB4, 0x49ABFD, 0x49B8E6, 0x6BF78E.\n- The fields read by the draw are written only by those setters/integrators and by the draw's own derived outputs (+0x13C, +0xD8, +0x110..+0x118).\n- camera.cpp SceneOpen_A/SceneRestore pattern; sites.json SCENE_RESTORE stub.",
 "risks": "- During the rest of the A-render's drawFrame (RenderUI, APT, InGameUI), LW client accessors and the LW camera return the halfway pose. That is consistent with the picture. Picking and input run in clientUpdate before the draw, so they always see C_k.\n- Fade transitions (states 2/3) and cuts present 30 Hz steps.\n- If drawFrame unwinds by exception, the restore is skipped, the same exposure as the existing tactical camera swap.\n- Requires the three A-only LW stepping gates to stay active.\n- Coordinate with the owner of the LW camera input area so that only one presentation mechanism is used.",
 "dll_vars": [
  {
   "name": "g_lwSwapActive",
   "ctype": "uint8_t",
   "note": "30: 0; 60 A: 1 from LwPresentBegin_A to SceneRestore; 60 B: 0"
  },
  {
   "name": "g_lwRec",
   "ctype": "struct LwCamRec {void* lw; uint32_t renderId; float pos[3], angle, zoom, fade, center[3]; uint32_t state; bool valid;}",
   "note": "previous A-render's stock LW camera inputs"
  },
  {
   "name": "g_lwSaved",
   "ctype": "struct LwCamRec",
   "note": "stock inputs saved while the halfway pose is applied"
  }
 ]
}

## SITE {
 "id": "LW_B_SKIP_SHROUD",
 "phase": "6",
 "group": "LW draw cost (optional)",
 "address": "0x449D63",
 "length": 8,
 "original_hex": "39 98 c0 37 00 00 74 40",
 "original_asm": [
  "0x449d63: 39 98 c0 37 00 00  cmp dword ptr [eax+0x37c0], ebx   ; EAX=[0xDC78EC] terrain render object (non-null, checked at 0x449D5F), EBX=0",
  "0x449d69: 74 40  je 0x449dab"
 ],
 "kind": "jmp_detour",
 "replacement_hex": "e9 <rel32:CAVE_LW_B_SHROUD> 90 90 90",
 "stub": "CAVE_LW_B_SHROUD (pure asm, DLL globals only):\n  cmp byte [g_skipB],0 ; je .stock\n  mov ecx,fs:[0x24] ; cmp ecx,[g_mainTid] ; jne .stock   ; ECX is dead on every continuation\n  mov ecx,[0xDE4958] ; test ecx,ecx ; je .stock\n  cmp byte [ecx+0x18],0 ; je .stock\n  cmp byte [ecx+0x19],0 ; jne .stock     ; the LW branch (0x449F45) will draw this frame\n  SKIPCNT ; jmp dword [T_449DAB]         ; B-render: skip the shroud render-to-texture pre-pass 0x449D6B..0x449DA6 (vt90 getView + 0x472EF4 + 0x473CAD)\n.stock:\n  RUNCNT\n  cmp dword [eax+0x37C0],ebx             ; displaced\n  je .t\n  jmp dword [T_449D6B]\n.t: jmp dword [T_449DAB]                 ; displaced 'je 0x449DAB'. This target is the SCENE_OPEN span start, entered exactly as in stock.\n\n30 mode and A-renders: identical to the original two instructions.",
 "resume_address": "0x449D6B (fall-through) / 0x449DAB (je target and skip)",
 "live_after": "- At 0x449D6B: ESI=display and EBX=0 are live. EAX is reloaded by 'mov eax,[esi]'. ECX is reloaded ('mov ecx,esi'). EDX and EDI are dead; EDI is assigned at 0x449D75.\n- At 0x449DAB: EBX=0 and ESI are live. EAX is reloaded by SCENE_OPEN's displaced 'mov eax,[0xDE412C]'. ECX, EDX and EDI are dead: ECX is next written at 0x449DDA or 0x449E0A, EDI at 0x449E1A or 0x449EA0.\n- EFLAGS are dead at both. x87 depth 0, XMM dead.",
 "branch_into_span_check": "- Brute rel8/rel32/jcc32 and whole-image abs32 scan for 0x449D63..0x449D6A: 0 hits. The span start 0x449D63 is not a branch target either.\n- listing.asm has no references.\n- Bytes are identical in the three builds (0x449F20 block checked; the 0x449Dxx block is the same function).\n- No overlap: INT_TREES ends at 0x449D59 and SCENE_OPEN starts at 0x449DAB.",
 "purpose": "Per-render cost on the LW map that should not double.\n\nThe shroud render-to-texture passes 0x472EF4/0x473CAD render the tactical shroud using the tactical view's camera.\n\nThe pointer DC78EC+0x37C0 is written only at 0x46CF3A/0x46D107, inside map-load 0x46CF01 (caller 0x4E1366). It is never cleared, so in stock these passes keep running every render on the LW map using the previous map. The LW branch never consumes them: it never calls RenderViews, and the LW scene render closure has no shroud reference.\n\nA-renders keep stock; B-renders skip work whose result is discarded.",
 "evidence": "- drawFrame 0x449D4B..0x449DAA.\n- G2 notes: the shroud is real-time (0x472CF2/0x473AAB).\n- listing.asm writers of +0x37C0.\n- Static reachability from 0x49B618/0x49C3DF/0x49BEFD/0x975441: the only DC78EC readers are in shadow-projection code, which reads heights, never the shroud.",
 "risks": "- Optional. Gain unmeasured: check with telemetry (B-render ms on the LW map) before adding.\n- Whether +0x3878/+0x387C are non-null on the LW map was not traced.\n- If a future change makes the LW branch consume the shroud texture, this site must go."
}

## SITE {
 "id": "LW_B_SKIP_SHADOW_WATER",
 "phase": "6",
 "group": "LW draw cost (optional)",
 "address": "0x449DBD",
 "length": 8,
 "original_hex": "39 1d 38 7a dc 00 74 43",
 "original_asm": [
  "0x449dbd: 39 1d 38 7a dc 00  cmp dword ptr [0xdc7a38], ebx   ; shadow-map manager",
  "0x449dc3: 74 43  je 0x449e08"
 ],
 "kind": "jmp_detour",
 "replacement_hex": "e9 <rel32:CAVE_LW_B_SHADOW_WATER> 90 90 90",
 "stub": "CAVE_LW_B_SHADOW_WATER (pure asm):\n  cmp byte [g_skipB],0 ; je .stock\n  mov ecx,fs:[0x24] ; cmp ecx,[g_mainTid] ; jne .stock\n  mov ecx,[0xDE4958] ; test ecx,ecx ; je .stock\n  cmp byte [ecx+0x18],0 ; je .stock\n  cmp byte [ecx+0x19],0 ; jne .stock\n  SKIPCNT ; jmp dword [T_449E70]          ; B-render, LW branch: skip UpdateShadowMap (0x449DCF..0x449E03, 0x47D5C9) and UpdateWaterReflection (0x449E08..0x449E6B, display vt90 getter + 0x47F695/0x47F25A)\n.stock:\n  RUNCNT\n  cmp dword [0xDC7A38],ebx                ; displaced\n  je .t\n  jmp dword [T_449DC5]\n.t: jmp dword [T_449E08]                  ; displaced 'je 0x449E08'\n\n30 mode and A-renders: identical to the original.",
 "resume_address": "0x449DC5 (fall-through) / 0x449E08 (je target) / 0x449E70 (skip)",
 "live_after": "- EBX=0 and ESI=display are live at all three resume points.\n- EAX is dead at all three: reloaded at 0x449DC5, 0x449E08 and 0x449E70.\n- ECX is dead: written at 0x449DDA, 0x449E0A, or only pushed as a slot at 0x449E98 and overwritten by the fstp.\n- EDI is dead: written at 0x449E1A or 0x449EA0. EDX is dead.\n- EFLAGS are dead. x87 depth 0. XMM dead.",
 "branch_into_span_check": "- 0x449DBD itself is a branch target (je at 0x449DB2); that is allowed because the e9 is at the span start.\n- Interior 0x449DBE..0x449DC4: no rel8/rel32/jcc32 hits. The two abs32 hits, 0x449DBE at 0x5F31CE and 0x449DC0 at 0x5F31C0, are the rel32 operand bytes of 'call 0xA3CF90/0xA3CF84' in 0x5F31B0, i.e. false positives.\n- listing.asm shows only 'JZ 0x00449dbd' at 0x449DB2.\n- No overlap with SCENE_OPEN (0x449DAB..0x449DAF).",
 "purpose": "Per-render cost on the LW map that should not double.\n\nThe tactical shadow-map pass (DC7A38, 0x47D5C9) and the water-reflection pass (DC7A40, 0x47F25A) are applied only around RenderViews (0x47BA74/0x47AF59 at 0x449FD7/0x449FFA), which the LW branch does not call.\n\nThe LW scene draw closure never references DC7A38 or DC7A40.",
 "evidence": "- drawFrame 0x449DAB..0x449E6B and 0x449FA9..0x44A00D.\n- refs of 0xDC7A38: drawFrame, W3DView 0x48B7B1/0x48BCF2, shadow-manager internals 0x499xxx and 0x91A3A9. None of these are in the LW draw closure (reach from 0x49B618/0x49C3DF).\n- 0x47F695 needs water polygon triggers (0x70A5B0), probably none on the LW map.",
 "risks": "- Optional. Gain unmeasured; the RTS3DScene is probably empty on the LW map.\n- The shadow-map dirty state machine (+0x28, 0xD9952C) may defer a rebuild by one render. That is invisible because the LW branch does not use it, and the next A-render recomputes it."
}