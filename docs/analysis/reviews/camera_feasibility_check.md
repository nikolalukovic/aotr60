# camera_feasibility_check

**Verdict: the §1.6 camera model works, with four fixes.** One skip site in W3DView::update replaces the per-integrator gate list. The A-render's camera step must run with the stock fraction. B-renders must show and pose with the camera from A's draw time, not the current one. A guard is needed on the fast-forward camera step. With those in place, the camera state at logic time is stock-exact and simpler to keep exact than with the state split. Static analysis only; no runtime check was done.

**Map of W3DView::update 0x48BCF2.** On entry ECX=EBX=view+0xB4. EDI=view from 0x48BE02 (reloaded from [ebp-0x1C] at 0x48C059).
- **B0 0x48BD25..0x48BDFD** (runs if terrain [0xDC78EC]+0x37D4 is set): scene lights 0x46F1DA, shadow refit 0x47D37D(cam), terrain vt+0x218 re-centre. Caches derived from the camera only; it does not change camera state.
- **B1 0x48BE02..0x48C068, follow/lock:**
  - follow factor 0xD99638 (0x48BE23 = −1.0, 0x48BEB5/0x48BEC2 ramp);
  - view+0x2354 = 0 at 0x48BE32;
  - vt70(0) → 0x4872A7 → message 0x452 at 0x48BE3D, and vt1A0 at 0x48BE80;
  - target from 0x676711 at 0x48BEF1, tether/lerp 0x48BF6E..0x48C001;
  - prev pos +0x23F4/+0x23F8 at 0x48C035/0x48C03E, pos +0xC..+0x14 at 0x48C04B, +0x76 at 0x48C050, +0x2408 at 0x48C05C.
- **B2:** updateCameraMovements call at 0x48C09A (gated by 0x441E23, GL+0x124, GL+0x125).
- **B3, legacy shake:** offsets +0x118/+0x11C (0x48C0F6/0x48C105); decay/flip +0x128/+0x120/+0x124 (0x48C12E/0x48C143/0x48C148) or zeroing (0x48C164..0x48C16E).
- **B4:** CameraShaker query 0x4655DD at 0x48C179, read only.
- **B5, height settle and terrain follow:**
  - writes +0x54, +0x50, +0x2408, +0x241C (0x48C24D/0x48C266/0x48C232/0x48C236);
  - zoom +0x3C at 0x48C399/0x48C3E0/0x48C3F8/0x48C494, +0x40 at 0x48C2EB;
  - 0x676711 again at 0x48C431.
- **B6, zone zoom:** +0x24C0, targets +0x9C/+0xA4, currents +0xA0/+0xA8 (0x48C6A4/0x48C6A9), limits +0x23E8/+0x23EC (0x48C6AE..0x48C6D1).
- **B7:** setCameraTransform at 0x48C6E2 when [ebp-0xD] or +0x2439 is set; terrain vt+0x22C(0) at 0x48C6FB.

**What the drawable pass 0x48C701..0x48C762 needs from the rest of the function:**
- Registers: only EBX, EDI and the EBP frame (the epilogue at 0x48C765 needs [ebp-0xC]). Locals [ebp-0x20]/[ebp-0x1C] are rewritten at 0x48C711/0x48C71C before use.
- Region: the frustum at cam+0x100 (built by 0x533B70) widened by the guard-band radius sqrt(view+0x78² + view+0x7C²) − 0.01. Only setGuardBandBias 0x48B5FC writes those two fields.
- So the pass depends on this call only through the camera transform set at 0x48C6E2.

**(1) Skip sites for B-renders.**
- **S1, one site skips the whole function including B0:** 0x48BD1B, 10 bytes `c6 45 f2 00 0f 84 dd 00 00 00` → `e9 cave` + 5×`90`.
  - Live at that point: EAX=[0xDC78EC], ESI=0, EBX, EBP. EDI is already pushed, so it is free to overwrite.
  - Cave: `mov byte[ebp-0xE],0`.
  - B path: `lea edi,[ebx-0xB4]`, swap the camera to M_k (fix 2), `jmp 0x48C701`.
  - Otherwise: save GE+0x3C and write the stock fraction (fix 1), then `test eax,eax`, `jz 0x48BE02`, `jmp 0x48BD25`.
- **S2:** 0x48C701, `a1 0c 1e dd 00` → `e9 cave2`. On the A-render it records M_k (cam+0x18, 48 bytes, plus cam+0xD8..+0xF0, with cam=[ebx+0x50]) and puts back the presentation fraction. Then `mov eax,[0xDD1E0C]` and `jmp 0x48C706`.
- **Jump to the original bytes rather than re-implementing the pass in a stub.** A stub would have to reproduce the x87 radius exactly (CRT sqrt 0xA3CF96, 24-bit precision).
- **S0, fast-forward guard:** 0x44B8D0, 8 bytes `8b 4d e4 e8 7b f0 03 00`. This path steps the camera (updateCameraMovements) and adds +33 to the sync on any render with m_frame%30≠0, B-renders included. On B, jump to 0x44BC5F.
- **Drop G2's split-step patches entirely:** C02–C08, C12 (1/60 shaker step), C13–C16, the IDIV doubling, the instant-move double step and the 16/17 ms dt. Under A-only stepping they would make the camera non-stock.

**(2) The W3D camera and where to swap it.**
- The camera is `[view+0x104]` (`[ebx+0x50]` relative to view+0xB4). Vtable 0xBE8748.
- **Set_Transform** is vt+0x54 = 0x533550: copies 48 bytes to cam+0x18..0x47 via 0x53B260, sets the identity flag at +0x74, notifies the scene, and sets +0xFC=0 (frustum invalid).
- **Projection:**
  - Set_View_Plane 0x533590 writes +0xD8..+0xE4 (+0xE8 aspect only when a vfov is given). It is called at 0x48B909 with hfov = view+0x6C (GD+0xEA8 in panorama) and at 0x48B93D with view+0x2364 in mode 4.
  - Set_Clip_Planes 0x5337C0 writes +0xEC/+0xF0; called at 0x48B837 (near [0xBD83D8]=10.0, far GD+0x950 × [0xBDD730]=1800).
  - Zoom is camera distance, so it is part of the transform. Update_Frustum is 0x533B70.
- **setCameraTransform 0x48B7B1 also notifies:**
  - terrain vt+0x218 (re-centre);
  - shadow manager 0x47D37D: camera copy via 0x534AD0, then fit via 0x47C276;
  - water 0x47F663 → 0x47EA2B;
  - NotifyCameraChange 0x603475 (external DLL, GetProcAddress);
  - TheAudio [0xDE42FC] vt+0x58 (listener).
- **Do not re-run setCameraTransform or buildCameraTransform for the swap.** They step the CameraShaker, the +0x138 decay and the fly transition, and they fire the notifications above.

| Role | Site | Original bytes | What it does |
|---|---|---|---|
| Swap start | 0x449DAB | `a1 2c 41 de 00` | After particles, trees and shroud; before shadow 0x449DF5, water 0x449E28/0x449E5D, RenderViews 0x449FE0 and RenderUI. Saves M_cur and the view plane, calls vt54(P), refits shadow with ECX=[0xDC7A38], push cam, call 0x47D37D (only if GD+0x62). |
| Water | none needed | | 0x47F1AC rebuilds the reflection camera from the passed camera at render time (0x47EA2B(cam,0)). |
| Restore | 0x44A23E | `8b 0d ac 3b de 00` | Every drawFrame exit passes here: Set_Transform(M_cur), raw view-plane bytes, +0xFC=0, shadow refit. This is before propagate, so logic-time readers see C_k′. |
| Guard | 0x4FD254 | `8b 0d 7c 44 de 00` | Motion-blur zoom-to lookAt inside postRender. lookAt reads the camera position at 0x48CF47, so the swap must end before this call. |
| Guard | 0x48B7B1 | `b8 25 3f b7 00` | If a swap is active, end it before any setCameraTransform. |

Caches that stay at C_k, all visual only: the terrain window, the drawable region (on A, drawables in the trailing sliver keep their B pose), the audio listener and NotifyCameraChange.

**Fix 2 — the plan's "record C_prev on B" is wrong.**
- Camera input applied after the draw makes B's current camera C_k′ instead of C_k. Sources: InGameUI key rotate/zoom/scroll (0x6A21DF..0x6A23CD) and the LookAt translator's middle-mouse rotate, wheel zoom and 0x452 handling during propagate 0x6324A1.
- If B shows C_k′, A_{k+1} = lerp(C_k′, C_{k+1}) ≈ B_k, giving 30 Hz judder. B's drawable region would also differ from stock render k's region, and the plan says logic reads that pass's output.
- Fix: record M_k at S2 on A. B swaps to M_k from the S1 B path through 0x44A23E. A presents lerp(M_{k−1}, M_k).
- While shake is active (view+0x128 > 0.01, or 0x4655DD true), present M_k with no interpolation. Otherwise the ± sign flip interpolates to about zero offset on A.

**(3) Coupling between camera and logic: confirmed stock-exact, with two exceptions.**
- **Flags:** +0x23D0, +0x23D4 and the vt78 inputs (+0x2354/+0x1DC/+0x204/+0x228/+0x254/+0x27C) are written only by steppers inside S1's skipped range or by setters called from logic or input.
- **Filter state:** motion-blur type 2, modes 7..12 makes vt78 return true. Its transitions in 0x4FCEAE happen only when the logic frame changes (`cmp [ebx+8],[GL+0x40]` at 0x4FD20F), so on A. End-pan mode 0xD decrements per render at 0x4FD1B4 (2x fast) but is not in 7..12, so it is visual only.
- **Message 0x452:** vt70 sites 0x4868ED, 0x486A06, 0x48975D, 0x489807, 0x4898B0, 0x489E9F and 0x48BE3D all sit inside the skip. They fire on A, are propagated by 0x7128C3 at 0x6324A1 (LookAt translator, then CommandList), and are consumed at sub 1. B emits nothing.
- **Exception — fraction:** 0x676711 reads GE+0x3C ([0xDE4324]+0x3C). On A that is (2k−1)/12, so in follow/lock mode the camera pose is not stock (flags are unaffected). Fix 1 runs B1–B7 with the B-value fraction.
- **Exception — fast-forward:** the step at 0x44B8D3 needs guard S0.
- More generally, any A-render reader of GE+0x3C that changes persistent state needs the same treatment.

**(4) Compared with the reviewers' state split.**
- **Not worse for exactness, and much simpler.** The state split would need to:
  - snapshot more than 9 KB of the view, including objects held by pointer at +0x2458 and +0x24C8;
  - snapshot the statics 0xD99638, 0xDC78D4 and 0xDD1CB8..0xDD1CC4;
  - suppress the 0x452 messages;
  - undo setCameraTransform's notifications. The external DLL notification (0x603475) and the audio listener cannot be undone.
- **Visually somewhat worse:**
  - camera latency is +16.7 ms over stock (the same as units);
  - interpolation is a straight line between frames (chord), so shakes need the rule above;
  - cuts need detection;
  - terrain, region and shadow coverage stay at C_k unless refit, which can show at screen edges during very fast motion.
