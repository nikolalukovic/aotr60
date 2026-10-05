# G8_delayfix

## Summary

G8 result: delayfix.dat works and can run at 60 FPS, but the normal patch set must never be applied to it unchanged.

What delayfix does. Every per-render patch site of the design has the same bytes in game.dat, delayfix.dat and stock game820.dat. I checked about 110 sites against the 18 diff runs and none lies near them. The only overlap is 0x63252F, and its first 5 bytes are identical too. Only the stepper's tick structure differs.

1. isTick (FUN_0063252F). At 0x632535 delayfix has `f7 3d 00 a4 ec 00`, an idiv by [0xECA400], where normal has `f7 3d 08 f6 d9 00`, an idiv by LTR. [0xECA400] holds 8 in delayfix, so r = 30/8 = 3, which is below 6, and isTick returns s == 6/3 = 2.
2. Catch-up in vt98 (0x6329B0). At 0x632A9B delayfix has `b8 02 00 00 00 eb 19` (mov eax,2; jmp 0x632ABB) in place of `f7 3d 08 f6 d9 00 8b`. The loop now runs for every vt98 call: esi = 3, so only edi == 1 enters the loop.
   - vt98(1) does: GameLogic::update(1), then [0xDE4950]->vt28, then `inc [ebp+0x34]` (GE+0x34 = 2), the debug id, GameLogic::update(2), and the fraction call at 0x632AF0.
   - vt98(3..6) does GameLogic::update(s) plus a fraction recompute.
   - When paused, the early path at 0x632AF7 (GameLogic+0x124) runs no logic call.
   - If GameLogic itself fails sub 1, update(2) still runs. GC+0xC8 is written only inside the sub==1 block (0x62E5D1, 0x62EE3E).
3. Resulting tick:

| | normal | delayfix |
|---|---|---|
| Renders per tick | 6 | 5 |
| GameLogic::update calls per render | 1 each | 2, 1, 1, 1, 1 |
| m_frame per tick | 6 | 5 (+1 every running render) |
| GE+0x38 | 6 | still 6 (0x632565, 0x632615, 0x635DD0, 0x63CF15 divide by LTR) |
| Fractions shown | 1/6 .. 6/6 | 2/6, 3/6, 4/6, 5/6, 6/6 (uneven 2/6 step) |
| isTick true at | s == 1 | s == 2 |
| Tick length / logic rate | 198 ms / 5.05 Hz | 165 ms / 6.06 Hz |
| Failed tick attempts | 1 call per render | update(1) + vt28 + update(2) per render (none while paused) |

   The limiter and FramesPerSecondLimit are unchanged (30 in INI.big and in #aotr_patch202.big). So delayfix runs game time about 20% faster at the same FPS cap. AotR confirms the 5-renders-per-tick model itself: delayfix changes its Palantir counter threshold from 30 to 25 (0xED081C, `1e`->`19`), which is 5 ticks in both variants.

Other diffs. None of them touches timing or our patch sites.
- 0x6E5A10 `7e`->`eb`: removes the 100 cap on ScrollFactor (0x6E59A9, which feeds GlobalData+0xA9C/+0xAA0).
- 0x920B79 `75`->`eb`: the Options OnlineIp combo is never disabled (GameWindow::winEnable 0x7154B3). UI only.
- AIKINDOF parser bounds: 0x8ED4BF (float list: 0x15 normal / 0x11 delayfix and stock) and 0x8ED544 (name lookup: 0x15 / 0x12; stock 0x11). Load-time AI data only.
- 0x9A3AE0: normal has a stale AotR hook that jumps to 0xED3000. That address now holds data (the bytes `f4 be bf 00` decode as hlt), so it would crash if the vtable 0xC88358 method were ever called. Delayfix restores the stock bytes `51 53 56 8b f1`.
- 0xBDD378: camera clamp float, 5000.0 in normal vs 700.0 (stock value) in delayfix. Read by W3DView at 0x487090/0x4871DA/0x48C213/0x48D1BB; none of our camera fixes uses it.
- Inert data: 0xDB55C0/0xDB5608 (unreferenced name tables, "SPAM" vs "SUPPORT"), the 0xC79421 padding byte, and .angmar string and padding layout ("PATROL" moved).

Decision. The 60 FPS design can be applied to delayfix. It needs 10 frames per tick, not 12: 5 A/B pairs at 16/17 ms equal 165 ms, the same tick length as delayfix at 30. The logic sequence per tick stays vt98(1){1+2}, 3, 4, 5, 6, and m_frame advances 5 per tick as at 30 FPS.

Applying the normal C1 patch set as-is to delayfix would break logic:
- GE+0x34 gets bumped to 2 at the tick, then s=3 calls vt98(2), so sub 2 runs twice per tick.
- A tick becomes 11 frames (about 181.5 ms).
- The A/B order breaks (two A-frames in a row).

Recommended implementation ("route 2"): leave GE+0x34 in stock units and make every other stepper pass a forced "halted" pass. Hook 0x6325D5 (`84 db a1 88 43 de 00`). Detour isTick at 0x63252F (`a1 0c f6 d9 00`) to return 0 on B-renders. Set GE+0x3C in C0 as (2k-1)/(2R) on A-renders and 2k/(2R) on B-renders, with k = s - s_tick + 1, R = 7 - s_tick, and 1.0 when s > 6. The only per-variant parameter is s_tick (1 normal, 2 delayfix); the variant's own stepper code runs unmodified on A-frames.

If the synthesis C1 in-place stepper is kept instead ("route 1"), delayfix needs:
- N = 10: imm 06->0A at 0x632606 and 0x63264C.
- FracStub divisor 10, also at 0x632AF0 (`e8 7a fa ff ff`).
- SubStub map: odd s' gives sub = (s'==1 ? 1 : (s'+3)/2).
- RestoreStub sets s = 9 and frac = 1.0.
- NOP `ff 45 34` at 0x632AC0.
- Write [0xECA400] = 5 (from 8) so isTick becomes s' == 1.
- Switch modes inside the tick-path FracStub at 0x6326BE, before vt98(1).

Two bugs found in the synthesis C1/C3 that affect both variants:
1. The W3D sync half-step is armed on every B-frame, including while paused. Pause means GameLogic+0x124 makes every tick attempt fail. So syncAcc moves about 8 ms per render during pause and animations play at roughly 25% speed. Fix: arm only if the previous A stepper left GC+0xC8 == 1.
2. C1's RestoreStub leaves the fraction cycling between 11/12 and 12/12 during stalls such as pause. Fix: set frac = 1.0.

Detection.
- Check the host process. Hash .text, .danetta and .angmar at DllMain, before any game code runs:

| build | .text CRC32 | .danetta CRC32 | .angmar CRC32 |
|---|---|---|---|
| normal | 879A36B4 | 6DCD48D0 | 609926AF |
| delayfix | DEDA7CF9 | 63F5DCE6 | CCE952A4 |
| stock 2.01 unpacked (game820) | E74C1451 | none | none |

- Then verify the discriminator bytes, and the bytes at every patch site before writing anything.
- Refuse anything unknown.
- File SHA-256: normal cc08275d…, delayfix 035da15c….

## Design notes

1. Decision on delayfix: do not disable 60 FPS for it permanently. It is supportable with exact preservation:
- the logic sequence per tick, vt98(1){update(1), vt28, update(2)}, 3, 4, 5, 6;
- failed tick attempts at 30/s, each with the same calls;
- 5 m_frame steps per tick;
- 165 ms per tick, so the same game speed as delayfix at 30 FPS.

Delayfix at 30 FPS already runs game time about 20% faster than normal (5 renders per tick at the unchanged 33 ms limiter, FramesPerSecondLimit=30 in INI.big and #aotr_patch202.big). 60 mode must keep that speed, not "fix" it.

Ship delayfix support after its own runs of M1, M2, M3 and M9. Until then its build-table entry is 'known, 60 FPS not enabled': no patches, stock 30 FPS, logged.

2. Never apply the normal C1 patch set to delayfix. The catch-up bump (GE+0x34 = 2 at the tick) plus SubStub sub=(s+1)/2 calls vt98(2) at s=3, so sub 2 runs twice per tick and a tick becomes 11 frames. Strict variant detection is mandatory.

3. Recommended stepper implementation for both variants ("route 2"), which departs from synthesis C1. Keep GE+0x34 in stock units. A-frames run the original stepper bytes. B-frames are forced through the stepper's own 'halted' path (0x6325DE: GC+0xC8=0, Debug vt94, return) via a hook at 0x6325D5. isTick gets a 5-byte detour at 0x63252F that returns 0 on B-renders. C0 writes GE+0x3C before every render: A: (2k-1)/(2R), B: 2k/(2R), k = s - s_tick + 1, 1.0 when s > 6. The only per-variant parameter is s_tick: 1 for normal (R=6), 2 for delayfix (R=5).

What route 2 removes from C1:
- C1a, C1c, C1e, C1f and three of the C1b call-site patches;
- every delayfix-specific code write (0x632AC0, 0x632AF0, 0xECA400);
- runtime rewriting of imm bytes for the stepper.

It also gets failed tick attempts at 30/s for free and keeps the stepper's rate statistics in stock units. Its preconditions are verified: all fraction readers read +0x3C, every isTick caller is render-side inside clientUpdate, and no GE+0x34 reader via TheGameEngine exists outside the stepper. A getter reached through another pointer is still possible; M3 confirms.

If C1 is kept instead, delayfix needs:
- N = 10: imm 06->0A at 0x632606 and 0x63264C;
- FracStub divisor 10, also at 0x632AF0;
- sub map 1, 3, 4, 5, 6 (odd s': s'==1 ? 1 : (s'+3)/2);
- RestoreStub sets s = 9 and frac = 1.0;
- NOP 0x632AC0 (`ff 45 34`);
- [0xECA400] = 5;
- mode switch in the tick-path FracStub at 0x6326BE, before vt98(1).

4. Bug in the synthesis C1+C3 that affects both variants. When paused (GameLogic+0x124), vt98(1) fails every attempt, yet C1d arms g_lastStepB on each B-frame. SyncStub then advances syncAcc by 16/17 ms every other render, so model animation, UV and water run at about 25% speed during pause; stock freezes them. Fix: arm the half-step only if the previous A stepper left GC+0xC8 == 1 (route 2: read in BStub before the halted path clears it; C1: record b in C0).

Minor bug in C1e: after a failed attempt RestoreStub computes frac 11/12, so +0x3C cycles between 11/12 and 12/12 during stalls. Write 1.0.

5. Variant-independent facts the team can rely on:
- every per-render patch site in sections 2.2 and 3 is byte-identical in normal, delayfix and stock game820 (verified about 110 addresses against the 18 diff runs);
- all delayfix diffs outside the stepper are logic data, UI or camera-limit changes with no frame-rate interplay (ScrollFactor uncapped, camera clamp 700 vs 5000, AIKINDOF bounds, OnlineIp UI, stale 0x9A3AE0 hook);
- the AotR Palantir counter (30 vs 25 updates) stays correct per variant once the 0x6A23D7 Palantir gate (row 67, g_uiTick) is applied.

6. Detection procedure for the DLL:
- (a) Host check: ImageBase 0x400000, TimeDateStamp 0x460DA09E, EntryPoint RVA 0x23D082, SizeOfImage 0xAD4000, section table including .danetta at 0xECA000 and .angmar at 0xED3000.
- (b) CRC32 of .text, .danetta and .angmar at DllMain against the build table: normal 879A36B4 / 6DCD48D0 / 609926AF, delayfix DEDA7CF9 / 63F5DCE6 / CCE952A4.
- (c) Discriminator bytes cross-check: 0x632535, 0x632A9B, [0xECA400], 0xED0816.
- (d) Verify every patch site's original bytes, then patch. Never patch partially.
- (e) Log the CRCs of unknown builds and stay at stock 30 FPS.

File SHA-256 for launcher or installer tooling: normal cc08275d60ff8e3bfd4374c29d61304dea8336e6dd00ab8add88b1df95a705dc, delayfix 035da15c626af6cd1663d1886deea605464ec6776f2c5f1d24570c2be6d73e61, stock unpacked 2.01 (rotwk\game820.dat) bcf4c85ec72ecb9eb95a844fc0072434fdcef49a5b2c59abb77ac04af29c277e. Do not use CheckSum or the LAA bit.

7. Builds to anticipate:
- New AotR releases. The launcher embeds assets\game_dats\game.dat and delayfix.dat and copies the chosen one from aotr\zGameDats to rotwk\game.dat on launch. The changelog shows repeated re-builds for patch 2.02 compatibility, so expect new CRCs per AotR release: re-run the site verification and add table entries.
- Stock RotWK 2.01 unpacked (game820). All 60 FPS sites match, so it could become a supported 'vanilla' entry later. It lacks the AotR Palantir hook (0x6D57AF holds `a1 50 49 de 00`) and the AotR sections; refuse it for now.
- Patch 2.02 (v8.x/v9.x) game.dat and its own delay-fix game.dat. AotR states "compatible with 2.02 v9.0 (excluding delay fix)"; these are different binaries, refuse them.
- The retail packed 2.01 game.dat (encrypted .text): refuse.
- Third-party game.dat edits (FPS or speed hacks, widescreen code patches): the .text CRC mismatches, refuse.

## Items

### G8-01 — core stepper / isTick  [run_on_B_with_fix, high]
- **Site:** FUN_0063252F isTick; operand 0x632535 (normal f7 3d 08 f6 d9 00 / delayfix f7 3d 00 a4 ec 00); data 0xECA400 (normal 0, delayfix 8, only absolute ref is 0x632537)
- **What:** eax=30/[div]; if >=6 return s==1 else return s==6/eax. Delayfix: 30/8=3 -> returns GE+0x34==2, which is the state left by the catch-up bump after a tick.
- **Cadence:** Per call from 10 render-side callers: 0x461EBD (Miles update), 0x48E2D3, 0x4B51F4, 0x4B6BE6, 0x4ED6AB (trees), 0x64871C/0x6487F3 (GameClient::update drawable block), 0x6759BB (updateDrawable), 0x83BA12 (LookAt), 0x8A036E (counter extrapolation). All run inside clientUpdate, so once per render.
- **Reason:** At 60 FPS isTick must be true only at the A-render that follows the tick frame, as under model H. Route 2: detour returns 0 on B-renders, and the variant's own formula (s==1 normal, s==2 delayfix) stays valid on A-renders because GE+0x34 stays in stock units. Route 1 (10-step, catch-up bump NOPed): the tick leaves s'=1, so the formula must yield s==1; set [0xECA400]=5 (30/5=6 -> jge -> s==1).
- **Risk if wrong:** Route 1 without the fix: isTick becomes true at the B-render after the tick. The drawable block is skipped there (m_frame unchanged), so per-tick work (0xDE4BD0 vt18, FUN_0068D8F7/FUN_00678F54) never runs. Fraction readers (0x8A0365 etc.) also shift at the wrong render. Without the route-2 detour, isTick fires on both A and B renders and per-tick consumers (trees 0x4ED6AB, LookAt, audio) run twice.
- **Fix @ 0x63252F:** Route 2: e9 <IsTickStub>; stub: if (g_m60 && g_renderIsB) {xor eax,eax; ret} else {mov eax,[0xD9F60C]; jmp 0x632534} (orig: a1 0c f6 d9 00 (identical in normal, delayfix, stock))
- **Fix @ 0xECA400:** Route 1 only: write 5 while 60 mode is on, restore 8 when leaving (.danetta is writable, flags 0xE0000020; still use VirtualProtect) (orig: 08 00 00 00 (delayfix only; normal 00 00 00 00))

### G8-02 — core stepper / logic call pattern  [not_on_B_path, high]
- **Site:** vt98 0x6329B0, catch-up 0x632A95..0x632AEE; delayfix bytes at 0x632A9B b8 02 00 00 00 eb 19 (normal f7 3d 08 f6 d9 00 8b)
- **What:** After GameLogic::update(edi) (plus [0xDE4950]->vt28 when edi==1): esi=2+1=3; loop while ++edi<3: inc [ebp+0x34] (0x632AC0), debug id 0x6251A3(lf*10-1+s), GameLogic::update(edi=2); then 0x632AF0 call 0x63256F (fraction). Net effect: vt98(1) = update(1)+vt28+update(2) in one render; vt98(3..6) = update(s) + fraction recompute. Skipped when vt98(1) takes the pause early-out (bl = !GameLogic+0x124 == 0 -> 0x632AF7 GC+0xC8=0). If GameLogic fails sub 1 internally (0x1D check, 0x62E5D1), update(2) still runs, because GC+0xC8 is written only inside the sub==1 block (0x62E5D1/0x62E5EF/0x62EE3E).
- **Cadence:** Logic path, once per stepper call (per render at 30 FPS; per A-frame at 60). Per tick: 6 GameLogic::update calls in 5 renders.
- **Reason:** This is the logic call pattern that must stay bit-identical to delayfix at 30 FPS. In both routes it runs only on A-frames and stays unmodified (route 1 only NOPs the GE+0x34 bump, which logic never reads).
- **Risk if wrong:** Applying the normal C1 (12 steps, sub=(s+1)/2) to delayfix: the tick bumps s to 2, then s=3 calls vt98(2), so update(2) runs twice per tick (per-object pass FUN_006260E1, FUN_006F2364 path budget). Logic diverges, ticks take 11 frames (181.5 ms instead of 165 ms) and the A/B order breaks.
- **Fix @ 0x632AC0:** Route 1 only: 90 90 90 while 60 mode is on (restore when leaving). Route 2: untouched. (orig: ff 45 34 (inc dword [ebp+0x34]; same bytes in all builds, unreachable in normal))
- **Fix @ 0x632AF0:** Route 1 only: call FracStub (falls through to 0x63256F when !g_m60); otherwise it overwrites the 60-mode fraction after every A-frame with s/6. Route 2: untouched (C0 rewrites +0x3C before each render). (orig: e8 7a fa ff ff (call 0x63256F, ECX=engine))

### G8-03 — core stepper / tick structure  [skip_on_B, high]
- **Site:** GameEngine::update 0x6325A0 (bytes identical in all builds) + G8-01/G8-02
- **What:** Delayfix at 30 FPS: stepper sets GC+0xC8=1 every running update (0x6325FB) -> m_frame +1 per render. Per tick: render after tick shows s=2 (frac 2/6), then s=3,4,5,6 (vt98(3..6)), tick at s=7 -> 5 renders, 5 m_frames per tick. GE+0x38 stays 6, so the instance-cache expiry m_frame+6 (0x671720) and the /+0x38 normalisers keep their 6-frame assumption (a delayfix quirk). Limiter unchanged (P=33 ms, 0x63A1AD): 165 ms per tick = 6.06 Hz vs 198 ms = 5.05 Hz in normal, about 20% faster game time.
- **Cadence:** Once per GameEngine::update
- **Reason:** 60 FPS on delayfix needs 10 frames per tick (5 A/B pairs x 33 ms = 165 ms, same as delayfix at 30). Route 2: B-frames run the stepper's own 'halted' path (GC+0xC8=0, Debug vt94, no s++, no logic). A-frames run the unmodified variant stepper. Failed tick attempts then happen only on A-frames, 30/s, matching stock.
- **Risk if wrong:** 12 frames per tick on delayfix: game 20% slower than delayfix at 30. Any extra or missing vt98 call: logic divergence. m_frame per tick other than 5: delayfix saves and visual timing change.
- **Fix @ 0x6325D5:** Route 2: e8 <BStub> 90 90. BStub: mov eax,[0xDE4388]; if g_m60 && !bl && !g_lastStepWasB: g_armHalf=(byte[eax+0xC8]==1); g_lastStepWasB=1; bl=1 (take the halted path 0x6325DE: GC+0xC8=0, Debug vt94, epilogue). Else g_lastStepWasB=0. End with test bl,bl; ret (ZF survives the ret; EAX must hold TheGameClient). (orig: 84 db a1 88 43 de 00 (test bl,bl; mov eax,[0xDE4388]); followed by 74 1a at 0x6325DC)

### G8-04 — core stepper (alternative to G8-03 route 2)  [skip_on_B, high]
- **Site:** Route 1 (synthesis C1 in place) parameterised for delayfix: 0x632606, 0x63264C, 0x632642, 0x6326BE, 0x6326E4, 0x632AF0, 0x6326DF, 0x6326EB, 0x632AC0, 0xECA400
- **What:** Sub-frame counter s' 1..10. The tick happens at s'>10 (tick path sets s'=1 and calls vt98(1) directly at 0x6326C6). The catch-up runs update(2) without bumping GE+0x34. Odd s' = A, even = B.
- **Cadence:** Once per GameEngine::update
- **Reason:** Only needed if the team keeps C1. Delayfix-specific deltas against the normal C1: N=10, a different sub map, NOP the bump, redirect the 4th fraction call, isTick divisor. The mode switch must happen in the tick-path FracStub at 0x6326BE, before vt98(1), so the bump state matches the mode for the whole tick.
- **Risk if wrong:** Wrong map: missing or doubled sub-steps. Missing 0x632AF0 redirect: A-render fractions become s'/6. Missing ECA400 fix: see G8-01. Switching mode after vt98(1): one tick with A/B parity off by one.
- **Fix @ 0x632606:** 0A in delayfix 60 mode (cosmetic +0x38 refresh; the value stays 6) (orig: 06 (imm of 83 f9 06))
- **Fix @ 0x63264C:** 0A in delayfix 60 mode (tick when s'>10) (orig: 06 (imm of 83 f8 06))
- **Fix @ 0x632642 / 0x6326BE / 0x6326E4 / 0x632AF0:** call FracStub: g_m60 ? +0x3C = min(s',10)/10 : jmp 0x63256F (0x6326BE stub also runs the mode controller) (orig: e8 28 ff ff ff / e8 ac fe ff ff / e8 86 fe ff ff / e8 7a fa ff ff)
- **Fix @ 0x6326EB:** e8 <SubStub> 90x6; delayfix: odd s' -> vt98(s'==1 ? 1 : (s'+3)/2) (gives 1,3,4,5,6); even -> GC+0xC8=0, arm the sync half-step only if the previous A succeeded (orig: 8b 16 50 8b ce ff 92 98 00 00 00)
- **Fix @ 0x6326DF:** e8 <RestoreStub> 90x5; s' = 9 (N-1) in 60 mode, then +0x3C = 1.0 (not 9/10) (orig: 8b ce 89 7e 34 e8 86 fe ff ff)
- **Fix @ 0x632AC0:** 90 90 90 in 60 mode (orig: ff 45 34)
- **Fix @ 0xECA400:** 05000000 in 60 mode (orig: 08000000)

### G8-05 — interpolation fraction  [run_on_B_with_fix, medium]
- **Site:** C0 render hook 0x6325CF (ff 90 9c 00 00 00) - fraction write, route 2
- **What:** All 9 fraction readers read [TheGameEngine 0xDE4324]+0x3C (verified 0x48E3C4, 0x4B5241, 0x4B6C21, 0x4B6F86, 0x67175A, 0x67668D, 0x67673E, 0x6767D1, 0x8A0382). No code outside the GameEngine methods reads GE+0x34 through the 0xDE4324 global (scan: only 0x860D43 reads +0x38). So C0 can set +0x3C right before clientUpdate.
- **Cadence:** Once per render
- **Reason:** k = s - s_tick + 1, R = 7 - s_tick (normal s_tick=1, R=6; delayfix s_tick=2, R=5). A-render: (2k-1)/(2R); B-render: 2k/(2R); s>6 (failed tick or pause) or k<1: 1.0. Normal gives 1/12..12/12 (same as the C1 design); delayfix gives 1/10..10/10, evenly spaced and smoother than delayfix at 30 FPS (2/6..6/6 with a 2/6 jump).
- **Risk if wrong:** Wrong R or s_tick: units jump at tick boundaries (visual only, logic unaffected).
- **Fix @ 0x6325CF:** (already C0) e8 <C0Stub> 90; additionally [ecx+0x3C] = frac(s=[ecx+0x34], g_lastStepWasB, variant.s_tick) when g_m60 (orig: ff 90 9c 00 00 00)

### G8-06 — W3D sync clock  [run_on_B_with_fix, high]
- **Site:** C3 SyncStub 0x44B911 + C1d/BStub arming (cross-variant)
- **What:** C1d arms g_lastStepB on every B-frame. While paused (GameLogic+0x124: vt98(1) fails every attempt, 0x632AFC GC+0xC8=0) the C1 cycle is s=13 fail -> 11 -> 12 (B, arms) -> 13... SyncStub then adds 16/17 ms every other render: syncAcc runs at about 25% of real time and model animation, UV and water advance during pause. Stock freezes them (d=0).
- **Cadence:** Once per render
- **Reason:** Arm the B half-step only if the previous A stepper advanced m_frame (GC+0xC8==1 seen by C0 before that render, or by BStub before overwriting it).
- **Risk if wrong:** Visible animation and water motion during pause, cinematic freezes and network stalls (both variants).
- **Fix @ 0x6326EB (C1 SubStub) or 0x6325D5 (route-2 BStub):** g_lastStepB = g_prevStepAdvanced (C1: value of GC+0xC8 read in C0; route 2: byte[eax+0xC8]==1 read in BStub before the halted path clears it) (orig: see G8-03/G8-04)

### G8-07 — UI / AotR hook  [skip_on_B, medium]
- **Site:** AotR Palantir counter 0xED0800 (cmp [0xED0000],imm at 0xED0816: normal 1e, delayfix 19); path 0x6A23D7 -> 0x6D7B33 -> 0x6D577C -> jmp 0xED0B00 (0x6D57AF) -> jmp 0xED0800 (0xED0B54)
- **What:** Counts Palantir updates and runs its scan every 30 (normal) or 25 (delayfix) updates, i.e. every 5 logic ticks in each variant.
- **Cadence:** Per render (Palantir update)
- **Reason:** With the row-67 gate at 0x6A23D7 (g_uiTick) the Palantir update stays at 30/s, so the counter fires at the same wall-clock rate as each variant at 30 FPS. No AotR bytes touched.
- **Risk if wrong:** Ungated: scan and hero-bar counters run 2x (harmless scan, 2x UI counters).
- **Fix @ 0x6A23D7:** gate with g_uiTick (unchanged from synthesis row 67) (orig: 8b 0d 70 4a de 00 8b 01 ff 50 28 (identical in all builds))

### G8-08 — input / options  [n/a, high]
- **Site:** 0x6E5A10 in FUN_006E59A9 (ScrollFactor getter; callers 0x641E0C/0x641E1A -> GlobalData+0xA9C/+0xAA0, 0x920A06)
- **What:** Normal clamps the ScrollFactor option to <=100 (x0.02); delayfix replaces jle with jmp, so there is no upper cap.
- **Cadence:** Event (options load/apply)
- **Reason:** The 60 FPS scroll fix (row 58, halve the per-render offset at 0x83B8A0) is proportional, so it is exact for any factor.
- **Risk if wrong:** None for 60 FPS.

### G8-09 — camera  [n/a, high]
- **Site:** 0xBDD378 float (normal 00 40 9c 45 = 5000.0; delayfix/stock 00 00 2f 44 = 700.0); readers 0x487090/0x48709F (FUN_00486F76), 0x4871DA/0x4871E9, 0x48C213 (W3DView::update 0x48BCF2), 0x48D1BB/0x48D1CA (FUN_0048D14D)
- **What:** Clamp value in W3DView camera height/position code. AotR raised it; delayfix still has the stock value.
- **Cadence:** Per render (W3DView::update)
- **Reason:** A static clamp, not an integrator. None of the planned camera operands (0x48BE98, 0x48BEA2, 0x48BFCB, 0x48BFE8, 0x48C36A/0x48C3B0, 0x48C553..0x48C60B) refers to it.
- **Risk if wrong:** None for 60 FPS; camera limits differ between variants by AotR design.

### G8-10 — logic data (INI load)  [n/a, high]
- **Site:** AIKINDOF parser: 0x8ED4BF (cmp esi: normal 0x15, delayfix 0x11 = stock), 0x8ED544 (normal 0x15, delayfix 0x12, stock 0x11), 0x8ED5D9 (0x12 both AotR), table ptr 0x8ED516 -> 0xED3012 (both AotR; 18 names INFANTRY..CREEP_STRUCTURE)
- **What:** Bounds of AIKINDOF float lists and name lookup; delayfix accepts at most 17 floats.
- **Cadence:** Load time
- **Reason:** Logic data, not frame timing. The variants are not save/replay compatible with each other regardless of 60 FPS.
- **Risk if wrong:** None for 60 FPS.

### G8-11 — shell UI  [n/a, high]
- **Site:** 0x920B79 in FUN_009205C4 (Options::OnlineIp branch)
- **What:** Normal: if !byte[edi+0x283], GameWindow::winEnable(0x7154B3) disables the IP combo. Delayfix: jne -> jmp, never disables it.
- **Cadence:** Event
- **Reason:** UI only.
- **Risk if wrong:** None.

### G8-12 — AotR hook (stale)  [n/a, medium]
- **Site:** 0x9A3AE0 (thunk 0x9A3AD5 in vtable 0xC88358)
- **What:** Normal: jmp 0xED3000. That address now holds the .angmar pointer table (f4 be bf 00 = hlt), so the hook would fault if called. The method is a stream/string read (stock body 0x9A3ADD..0x9A3B50). Delayfix restores the stock bytes 51 53 56 8b f1.
- **Cadence:** Unknown (apparently never called)
- **Reason:** Not related to frame rate; mention to AotR.
- **Risk if wrong:** None for 60 FPS.

### G8-13 — data  [n/a, high]
- **Site:** Inert diffs: 0xDB55C0/0xDB5608 (last entry of stock name tables 0xDB5580/0xDB55C8, unreferenced in AotR: 0xED3141 'SPAM' vs 0xC15A1C 'SUPPORT'), 0xC79421 (.rdata padding 04/00, unreferenced), .angmar 0xED3026 ('PATROL' pointer 0xED314C vs 0xED3146), 0xED305D/0xED3097/0xED30A6/0xED3146 (padding and string layout)
- **What:** Leftovers of string and table edits; no code references found (absolute scan).
- **Cadence:** n/a
- **Reason:** Usable only as discriminator bytes.
- **Risk if wrong:** None.

### G8-14 — loader / safety  [n/a, high]
- **Site:** Build identification (DllMain of the dinput8 proxy, before game.dat runs)
- **What:** Identify the build and refuse anything unknown. Only game.dat imports dinput8 (checked Worldbuilder.exe, both lotrbfme2ep1.exe, LotRIcon.exe), but still verify the host. No .reloc and fixed base 0x400000; the IAT is in .rdata (0xBD0000..0xBD0BE0), so in-memory .text equals the file. .danetta holds runtime variables (0xED0000 counter), so hash it only at DllMain or mask them. Ignore the LAA bit (Characteristics normal 0x12F, stock 0x10F) and CheckSum (0xADC2F6 not recomputed by AotR).
- **Cadence:** Once at process start
- **Reason:** The two AotR variants share TimeDateStamp, CheckSum, SizeOfImage and section layout with each other. The timestamp equals stock's. Only content hashes or discriminator bytes tell them apart.
- **Risk if wrong:** Patching delayfix with the normal parameters doubles sub 2 and changes speed (G8-02). Patching an unknown build: crash or silent logic change.
- **Fix @ PE header:** require all (AotR) (orig: ImageBase 0x400000; TimeDateStamp 0x460DA09E; EntryPoint RVA 0x23D082; SizeOfImage 0xAD4000 (AotR) / 0xACA000 (stock); sections 12 (AotR: ... .mackt, .danetta VA 0xECA000 VS 0x8192, .angmar VA 0xED3000 VS 0x1000) / 10 (stock))
- **Fix @ .text 0x401000 VS 0x7CF000 (zlib CRC32):** build table key; unknown -> refuse and log the CRC (orig: normal 879A36B4; delayfix DEDA7CF9; stock game820 E74C1451)
- **Fix @ .danetta 0xECA000 VS 0x8192 / .angmar 0xED3000 VS 0x1000 (CRC32 at DllMain):** must match the same entry (orig: normal 6DCD48D0 / 609926AF; delayfix 63F5DCE6 / CCE952A4)
- **Fix @ 0x632535 (6 bytes):** discriminator (orig: normal/stock f7 3d 08 f6 d9 00; delayfix f7 3d 00 a4 ec 00)
- **Fix @ 0x632A9B (7 bytes):** discriminator (orig: normal/stock f7 3d 08 f6 d9 00 8b; delayfix b8 02 00 00 00 eb 19)
- **Fix @ 0xECA400 (dword):** discriminator (orig: normal 0; delayfix 8)
- **Fix @ 0xED0816 (7 bytes):** discriminator (orig: normal 83 3d 00 00 ed 00 1e; delayfix 83 3d 00 00 ed 00 19)
- **Fix @ 0x6E5A10 / 0x920B79 / 0x8ED4BF / 0x8ED544 / 0x9A3AE0 / 0xBDD378 / 0xDB55C0:** secondary discriminators (orig: normal 7e / 75 / 15 / 15 / e9 1b f5 52 00 / 00 40 9c 45 / 41 31 ed 00; delayfix eb / eb / 11 / 12 / 51 53 56 8b f1 / 00 00 2f 44 / 1c 5a c1 00)
- **Fix @ 0x6D57AF (5 bytes):** AotR vs vanilla discriminator (orig: AotR e9 4c b3 7f 00; stock a1 50 49 de 00)
- **Fix @ every patch site:** verify all sites first, then write all; never patch partially; on any mismatch stay in stock 30 FPS (orig: identical in all 3 builds: 0x6325CF ff 90 9c 00 00 00; 0x6325D5 84 db a1 88 43 de 00 74 1a; 0x63252F a1 0c f6 d9 00; 0x632604 83 f9 06; 0x632642 e8 28 ff ff ff; 0x63264A 83 f8 06; 0x6326BE e8 ac fe ff ff; 0x6326C3 8b 06 53 ff 90 98 00 00 00; 0x6326DF 8b ce 89 7e 34 e8 86 fe ff ff; 0x6326EB 8b 16 50 8b ce ff 92 98 00 00 00; 0x632631 6b c9 0a; 0x632AC0 ff 45 34; 0x632AF0 e8 7a fa ff ff; 0x63A1AD e8 f2 2d 40 00; 0x63A1F8 89 3d 18 43 de 00; 0x44B911 a1 8c 7a dc 00 0f af c6 01 05 80 75 dc 00; 0x44B98C 83 c6 e2; 0x44B9C1 83 f9 1d; 0x6765D5 8b 0d 88 43 de 00 8b 01 53 ff 50 7c; 0x67173B 8b 0d 88 43 de 00 8b 01 ff 50 7c; 0x671774 8b 0d 88 43 de 00 8b 01 83 c4 10 ff 50 7c; 0x67BDC0 55 8b ec 51 57 8b f9; 0x449D40 8b 0d 44 37 de 00 8b 01 ff 50 28; 0x6A23D7 8b 0d 70 4a de 00 8b 01 ff 50 28; 0x449D55 e8 a1 e7 01 00; 0x83B8A0 8d 55 f0 52 ff 50 5c; 0x5DD986/0x5DD997 f3 0f 5c 05 20 f6 d9 00; 0x450ABC 55 8b ec 51 56 8b f1; 0x6A2012 e8 8d af 39 00; 0x4FDB4A 83 46 74 21; 0x4C78CE e8 c6 cd fe ff; 0x4CF2BB/0x4CF3CF f3 0f 2a 05 08 f6 d9 00; 0x71FD5D/0x9314DE 8b 0d 88 43 de 00 8b 01 ff 50 7c; 0x5DB4B4 8b 46 04 01 46 08; 0x5033E5 d9 05 6c fc bd 00; 0x452C9F f3 0f 10 05 20 f6 d9 00)

### G8-15 — mode control  [n/a, high]
- **Site:** Mode controller for delayfix
- **What:** When g_m60 may switch.
- **Cadence:** At tick boundaries
- **Reason:** Route 2: per-tick wall time is the same in both modes (6 or 5 A-frames x 33 ms, as 30 FPS or as A+B pairs), so switching at any A boundary keeps game speed. Prefer the update right after a successful tick (C0 sees GC+0xC8==1 and s==s_tick), then BStub makes the next stepper a B. Route 1 (delayfix): switch only in the tick-path FracStub (0x6326BE) before vt98(1), writing imm 0A/06, ECA400 5/8 and the 0x632AC0 NOP/restore together; when leaving, vt98(1) bumps s to 2 as in stock delayfix.
- **Risk if wrong:** Route 1 switching elsewhere: one mis-paired tick (extra or missing frame, wrong fraction or isTick for that tick).

## Open questions

- Confirm the delayfix speed at runtime with M1: GameLogic+0x40 rate should be about 6.06/s at 30 FPS in single player (and 5.05/s for normal). Then confirm that 60 mode keeps exactly that rate.
- Route 2 relies on GE+0x34 never being read outside the GameEngine stepper, isTick and fraction functions. The listing scan through the TheGameEngine global (0xDE4324) found only a +0x38 reader at 0x860D43. A getter reached through another pointer cannot be fully excluded, so confirm with the M3 trace (log GE+0x34 reads with a hardware breakpoint).
- Does AotR's AI INI data use more than 17 AIKINDOF float values? delayfix.dat throws at the 18th because of cmp 0x11 at 0x8ED4BF. This is an AotR-side issue, but it could make delayfix fail to start with current data.
- In normal game.dat the stale hook at 0x9A3AE0 jumps into .angmar data. Is the vtable 0xC88358 method (thunk 0x9A3AD5) ever called? If yes, normal crashes there independent of our DLL.
- The AotR hook at 0x69413A (in FUN_00693D0C, caller 0x6D165E, Palantir/UI area; reads drawable +0x3DC through 0xED0300) is missing from synthesis R10. Its cadence under B-renders is unaudited and applies to both variants.
- Delayfix bundles two logic sub-steps plus a render into the tick A-frame. Under deadline pacing, measure with M4 that the A+B pair stays at or below 33 ms in large battles. At 30 FPS that frame had 33 ms on its own.
- Should delayfix in single player be offered at all? It changes game speed by about 20% compared with normal AotR. That is a product decision for the user and AotR, outside the 60 FPS scope; 60 mode just preserves whichever variant is installed.
