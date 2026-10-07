# AREA checkpoints

## SUMMARY

Variant (C), presenting from checkpoints inside the logic step, can be built with 8 hooks. All 8 are verified against rotwk\game.dat: original bytes match, spans sit on instruction boundaries, no branch, abs32 value or brute-force rel8/rel32 decode lands inside any span, and none overlaps tools/sites.json (main or wip), lw_final_sites.json or AotR's hooks.

The 8 hooks:
- **CP_MOD** at 0x62EA97: before every module update in the bucket loop. Covers subs 3, 4, 5 and 6 (AIUpdate halves, plus physics, weapons and the other non-AI modules).
- **CP_PATH** at 0x6EC0D1: hook on the queue-pop function's entry, once per path the pathfinder solves, on every sub and twice in sub 5.
- **CP_PLAYER** at 0x6A84E5: before each of the 20 Player::update calls (AI players) in sub 5.
- **CP_SKAI** at 0x6A96F3: once per skirmish-AI manager entry in sub 5.
- **CP_SCRIPT** at 0x60A3BA: once per script the ScriptEngine runs, in sub 1.
- **CP_OBJ1** at 0x62E912: once per object in sub 1's first object loop.
- **CP_PART** at 0xA3B564: once per moved object the partition manager processes, in sub 2.
- **CP_COLL** at 0xB6D11E: once per active collision pair before its collision callbacks, in sub 2.

Five are plain 5-byte `call rel32` replacements (CP_PATH, which hooks a function entry, is one of the other three). Their stubs jump on to the original target, so the callee sees the same return address and stack as stock. On every hit the fast path adds only `cmp byte [g_presentPending],0 / jne slow`. Flags are dead after every span, and the x87 stack is empty at every site (CONFIRMED).

Hit counts and gaps (INFERRED, assuming about 3000-5000 objects and a 25-35 ms sub 5):
- CP_MOD runs about 3k-12k times in sub 5 and 1k-2.5k times in each of subs 3 and 4. The most for any site in one tick is about 16k.
- CP_COLL can reach about 15k per tick in dense melee.
- The gap inside the dominant module loop is usually under 0.05 ms, and at most about 1 ms (one module update).
- Worst gaps with the 8 sites are about 2-5 ms. They occur in these segments:
  - one long path search, or a path-zone rebuild;
  - the sub-5 manager block between the last Player::update and the first skirmish-AI entry;
  - sub 1 from the last script to the object loop (Lua and the other subsystems);
  - the sub-1 tail object loop;
  - the sub-2 collision sort, or its full-rebuild branch.
- Six optional sites, all verified, cut these gaps to about 2 ms. The exceptions are single path searches and the zone or collision rebuilds, which cannot be split.

The predictor works as follows (stepper code CONFIRMED, see details):
- At a B-render, s = [GE+0x34] and F = [GL+0x40].
- If s ≤ 5, the next call is sub s+1 of frame F.
- If s ≥ 6, the next call is a sub-1 attempt. It runs only if [GL+0x124] (paused) is 0, and it advances to frame F+1 unless time is frozen.
- Learn from a call only when sub ≥ 2, or when the sub-1 call changed GL+0x40. Key the result by (GL+0x40 % 10, sub).
- Paused attempts never reach LogicUpdateWrapper. Frozen attempts reach it with the frame unchanged, and must be excluded.

## DETAILS

## 1. Where the time of GameLogic::update 0x62E4E8 goes, per sub

The order of calls is CONFIRMED from the disassembly in hitch\gl_update.asm and the callee disassembly. The costs are INFERRED: there is no runtime profile.

The 'Gap' column is the longest stretch with no checkpoint, for the 8 core sites.

| Segment (in order) | Checkpoint | Gap without checkpoint |
|---|---|---|
| **Every sub, start** | | |
| Prologue; sub-1 frame logic 0x62E50C..0x62E59A; 0x5FF9D3 | | |
| Pathfinder 0x6F2364 via 0x62E69F, which first runs the path-zone update 0x93A530 (event-driven) | CP_PATH before each path | 1 path: 15k-25k cells max (gamedata.ini), about 1-4 ms. Zone rebuild: several ms, rare |
| 0x440809 (_controlfp PC24/RC-nearest on every sub) | | |
| **Sub 1** | | |
| ScriptEngine vt28 0x60CC67: prework (0x60399C, timers, [0xDE87D8]/[0xDE8844] vt28) | | about 0.1-2 ms |
| Per-side loop calling 0x60A377 for each script list (also 0x60BCE5 recursion). The engine's own "slow script >10 ms" log at 0x60CDxx shows this part can be heavy | CP_SCRIPT per script | |
| Post-loop 0x6A8541 / 0x862535 / 0x60C441, Lua vt28, TerrainLogic, VictorySystem, recorder / FireLogic / Weather, 0x820EF0, 0x81BE85 (every 5 ticks), command list 0x779A3D | | about 0.3-4 ms; Lua is unknown |
| Per-object loop 0x62E910 (0x70E013 getter + 0x674B1F) | CP_OBJ1 per object | |
| Sub-1 tail per-object loop 0x62EC5D (4 small calls per object) | | 0.3-2 ms → ALT_OBJ1END |
| **Sub 2** | | |
| Partition 0xA3B4E0, per dirty object | CP_PART | |
| Collision 0xB6E6B0 → 0xB6CF90 (sweeps all pairs, cheap per pair) + 0xB6D8A0 (sort-and-sweep update per moved object) | | 0.3-3 ms → ALT_SAP. The full-rebuild branch 0xB6CC30/0xB6CD60/0xB6C930 cannot be split |
| 0xB6D060 pair callbacks (collide-module vt+0x20 / vt+0x24) | CP_COLL per active pair | |
| Per-object loop 0x62E95F (0x6260E1, no callees) | | about 0.1-0.5 ms |
| **Subs 3 and 4** | | |
| Bucket 0 (all AIUpdate classes), half each | CP_MOD per awake module | one AIUpdate (target scans), typically under 0.3 ms |
| Compaction 0x62EADF..0x62EB4F, then 0x629DA6 | | under 0.2 ms |
| **Sub 5** | | |
| Buckets 1 and 2 (163 classes) | CP_MOD | one module, typically 1-20 µs, worst about 0.5-1 ms |
| TheAI 0x6FEC63: second pathfinder pass | CP_PATH | |
| ThePlayerList 0x6A84DB: 20 × Player::update 0x6AF269 → AIPlayer::update 0x8F9705 (0x8F833B when the player is registered in TheSkirmishAIManager; otherwise vt40..4C + 0x8F8F68 every 5 ticks; 0x8F3EFE every 10 ticks → 0x8F9487 recruit searches 0x7A41D4) | CP_PLAYER | one player, about 0.05-5 ms on the 10-tick boundary → ALT_RECRUIT |
| 0x629DA6, shroud [0xDE4358], taint, BuildAssistant, [0xDE3BE8], processDestroyList 0x62A2C9, WeaponStore, LocomotorStore, VictoryConditions, DelayedXP, 0x80F4D3, SkirmishAIManager loop 1 (0x8E3CF3) | | 0.3-3 ms → ALT_DESTROY / ALT_MGR_MID |
| SkirmishAIManager loop 2 (0x8EDDF6) | CP_SKAI per entry | |
| Mineshaft, TeamFactory, tail | | under 0.5 ms; end of step |
| **Sub 6** | | |
| Bucket 3 is empty: light | | |

Estimated share of a heavy sub-5 step (INFERRED): modules about 50-65 %, players and AI about 10-25 %, managers about 5-15 %, pathfinder about 2-10 %.

## 2. Stub template and slow path

**Fast path.** The stub is `cmp byte ptr [g_presentPending],0 / jne slow`, then the original instructions. For a call it ends with a tail `jmp dword ptr [T_target]` or `jmp [eax]`; for a non-call span it ends with `ret`.
- With a tail jump the return-stack buffer stays balanced and no extra stack is used.
- A pure 5-byte call replacement gives the callee the same return address as stock. The exception is CP_MOD, where the return value is 0x62EA9C instead of 0x62EA9E, as with the existing VTGATE stubs.

**Slow path (`CP_SLOW idx`).**
1. Uses the existing SAVE_ALL macro (pushfd, pushad, XMM0-7, cld), then calls `cdecl AotR60_Checkpoint(idx)`, then RESTORE_ALL.
2. Checks the main thread (fs:[24h] == g_mainTid), then `QPC >= g_cpTarget`.
3. If due, it clears g_presentPending before presenting, so any re-entry takes the fast path.
4. It saves x87 and SSE state (fxsave, i.e. x87 CW and MXCSR) and LastError. Optionally it snapshots about 32 KB of stack below ESP.
5. It calls `[0xDD3474] vt+0x44 (dev, 0, 0, 0, 0)`; the B Present's arguments are all NULL (EBX=0, stubs.asm:785-792).
6. It restores everything saved in step 4.
7. DEVICELOST sets g_gapDevLost only. The next A-render's own Present then runs the stock 0x522674 handling; Reset_Device or Sleep(200) never runs inside logic.

**Cost and fallbacks.**
- Before the target time each slow hit costs about 45 ns, about 0.3 ms per heavy step at most. An optional asm countdown can check only every 8th hit at CP_MOD / CP_COLL / CP_PART / CP_OBJ1.
- At the end of the step (LogicUpdateWrapper), present if due. Otherwise the pacer presents at the target time during its wait. C0 always presents before the next render.
- Because a wrong deferral is nearly free, (C) can defer every B and also replace the spacing wait. That wait currently sits before logic: ../hitching/synthesis.md cause 2(a).

**Cost of the fast path.** About 2-3 ns per hit. At the worst case of about 40k hits per tick (all sites) that is about 0.1 ms per tick.

## 3. Optional sites (each a 5-byte call, verified with 0 errors)

| Site | Address | Original | What it splits |
|---|---|---|---|
| ALT_SAP | 0xB6D8C4 | `e8 07 ef ff ff` (call 0xB6C7D0) | per moved collision object |
| ALT_DESTROY | 0x62A3F1 | `e8 53 ad ff ff` (call 0x625149) | per destroyed object |
| ALT_OBJ1END | 0x62EC9D | `e8 14 92 06 00` (call 0x697EB6) | sub-1 tail loop |
| ALT_RECRUIT | 0x8F9545 | `e8 8a ac ea ff` (call 0x7A41D4) | per AI recruit search |
| ALT_SKAI0 | 0x6A96CD | `e8 21 a6 23 00` | first SkirmishAIManager loop |
| ALT_MGR_MID | 0x62EBB3 | `e8 11 b7 ff ff` (call processDestroyList) | sub-5 manager block, once per tick |

Brute decodes into ALT_RECRUIT (from 0x8F9543) and ALT_SKAI0 (from 0x6A96C4) are misaligned false positives that start inside other instructions.

## 4. Predictor: which sub and which logic frame comes next (CONFIRMED from 0x6325A0, 0x6329B0, 0x62E4E8)

**The stepper.**
- The Y step does `s' = [GE+0x34] + 1` (0x632622/0x632625).
- If s' ≤ 6, it calls GE::step(s') at 0x6326F0. GE::step (0x6329C7 → 0x632A82) calls GL::update(s') unconditionally.
- If s' ≥ 7, it sets s = 1 and calls GE::step(1) at 0x6326C6.
  - In SP, GE::step(1) skips GL::update when [GL+0x124] (paused, 0x90F92C) is set. In that case it sets GC+0xC8 = 0 at 0x632AF7; there is no network gating, because 60 mode requires [0xDE4468] == NULL.
  - Inside GL::update(1), the frame advances (`inc [GL+0x40]` at 0x62E577) only when 0x625130 is true (GL+0x44 && !GL+0xA8 && !GL+0x9D) and time is not frozen.
  - Frozen means (TV vtD8 && !TV vt78) || script-debug, which is 0 in retail.
  - If frozen and CommandList [0xDE639C] vt44(0x1D) is false, it returns at 0x62E5EA with GC+0xC8 = 0.
  - If frozen and that message is present, it runs a full update without frame++. This is rare; exclude it.
- When GC+0xC8 = 0, the stepper restores s = s' (0x6326E1). So s ≥ 7 only after failed attempts.
- Subs 2-6 always do real work, even if pause starts mid-tick.

**Rule at the B-render** (anywhere in the Y iteration before the step), with s = [[0xDE4324]+0x34] and F = [[0xDE412C]+0x40]:
- s ≤ 5: next call is (sub s+1, frame F).
- s == 6: next call is (sub 1, frame F+1).
- s ≥ 7: a failed attempt just happened. Predict "no logic" while [GL+0x124] is set. Otherwise predict (sub 1, F+1); this happens after unpause.

**Learning (in LogicUpdateWrapper).**
- Keep a sample only if sub ≥ 2, or if GL+0x40 after the call equals the value before plus 1.
- Store it under key ((GL+0x40 after) % 10, sub).
- Paused attempts never reach the wrapper. Frozen attempts reach it with the frame unchanged, which is why the existing g_logicCalls % 60 index drifts.
- Do not learn while GL+0x40 < 5·LTR = 25: the pathfinder budget is ×100 there (0x6F247F..0x6F2487). Also skip GL+0x40 == 2 (the one-off block at 0x62E640).
- Flush the table on reset, load or mode switch.

**Periodicity (INFERRED).**
- 0x8F8F68 (LTR = 5 ticks) and 0x8F3EFE (2·LTR = 10 ticks) count down once per sub-5 call. 0x8F8F68 counts only while its queue is non-empty, so its phase against frame%10 can drift.
- Script delays recur every 5·d ticks (0x60A15C).
- Use a per-key EWMA (α = 1/4), the value 10 ticks back clipped to 2·EWMA + 2 ms (against spike replay), and exclude outliers above 3·EWMA + 5 ms.

## 5. Files and evidence

- **Verifier:** <analysis workspace>/hitch\mt\checkpoints\verify_cp.py
- **Output:** verify_cp_out.json and verify_out.txt in the same folder; wip_sites.json is a copy of the wip branch's sites.json.
- **What it checks:**
  - pefile bytes;
  - a function-local capstone decode from each function start, plus a linear sweep of every executable section;
  - direct branch targets;
  - brute rel8, rel32 and jcc32 decodes at every byte;
  - every 4-byte value in the image matching an interior address;
  - overlap with sites.json main and wip, lw_final_sites.json and AOTR_HOOKS.
- **Result for the 8 core sites:** 0 errors, 0 notes.

## RISKS

1. **Unknown time distribution.** No runtime profile exists, so the gap and hit estimates are INFERRED. Build the stubs with a profile mode first: target = +inf, every hit takes the slow path and logs (site, QPC) to a ring buffer. Run it on one tick in ten, then read the per-site counts and the max gap per sub from the next test session before relying on the ~2 ms bound.
2. **Present must not change state that logic reads.**
   - Logic starts with the x87 CW / MXCSR / LastError left by the B-render, not by Present. This is identical only if Present preserves them. The pathfinder runs before 0x440809 resets the CW on every sub.
   - Mitigation: compare CW and MXCSR across real Presents in AotR60_PresentDone and disable deferral on any difference; restore everything after the checkpoint Present.
   - Stale stack below ESP changes after a slow path. LogicUpdateWrapper already shifts the logic stack and traces stay identical, but a 32 KB snapshot and restore around Present is cheap insurance.
3. **Device loss and window changes mid-logic.** Never run Reset or Sleep(200) inside logic: only set g_gapDevLost. The next A Present repeats DEVICELOST and goes through the stock handling. If DXVK ever sent the window proc a message during Present (swapchain rebuild on alt-tab), game code would run mid-logic. Skip deferral when not foreground or after any reset or gap flag.
4. **D3D calls during logic** (resource create, lock, free when objects are made or destroyed) now happen between the B EndScene and its Present. This is legal in D3D9; it may only delay submission. A screenshot or back-buffer read requested in a B iteration must force an immediate present.
5. **FIFO blocking.** A checkpoint Present that blocks on a full queue makes the logic step longer by that time. Measured late_skips = 0 suggests Present does not block, but log the blocking time per checkpoint Present.
6. **Segments that cannot be split.** These remain gaps of 1-5 ms: single A* searches, path-zone rebuilds (0x93A530), the collision full rebuild, and single long scripts or AI recruit searches.
7. **Predictor drift.** Frozen-time attempts, pause, the first 25 ticks (pathfinder ×100), and the 5/10-tick AI counters, which drift when their queues empty. With (C) a misprediction only moves the target time, because the Present still happens at a checkpoint, at the end of the step, in the pacer or at C0.
8. **Fast-path cost:** about 2-3 ns per hit, up to about 40k hits per tick with all sites. Slow-path cost before the target is about 45 ns per hit (QPC); a countdown filter is optional.
9. **30-mode exactness.** g_presentPending is set only for 60-mode B Presents. CP_MOD writes a different return-address value (0x62EA9C) into its stack slot, as VTGATE stubs do. Determinism must be re-proven with compare_traces in both modes plus a forced slow-path stress mode.

## RECOMMENDATION

Variant (C) is feasible with the 8 core sites and is lower risk than the presenter thread (T): no second thread touches D3D, and no D3DCREATE_MULTITHREADED change is needed.

1. Add the 8 sites to tools/sites.json. They are 5 pure calls, 1 func_detour (CP_PATH) and 2 short call_gates (CP_PART, CP_COLL). Add verify_cp.py's brute-force rel8/rel32 and abs32 checks to verify_sites.py.
2. Implement CP_SLOW and AotR60_Checkpoint with these rules:
   - Clear g_presentPending before calling Present.
   - Save and restore x87/SSE state (fxsave/fxrstor) and LastError; optionally snapshot the stack.
   - On DEVICELOST only set the gap flag.
   - Check that the code runs on the main thread.
3. Present the deferred B through this chain:
   - the first checkpoint at or after the target time;
   - otherwise the end of the step in LogicUpdateWrapper, if the target is due;
   - otherwise the pacer at the target time;
   - in any case C0 before the next render.
   With this chain (C) can defer every B and drop the spacing wait that now sits before logic.
4. Ship the profile mode first. Use one session to measure per-site counts and gaps per sub, then enable only the alternates that the data justifies (ALT_DESTROY, ALT_OBJ1END, ALT_SAP and ALT_RECRUIT first).
5. Change the predictor key to (GL+0x40 % 10, sub), using the rule at the B-render from [GE+0x34], [GL+0x40] and [GL+0x124]. Exclude frozen sub-1 calls (frame unchanged), frames below 25 and frame 2, and flush the table on reset or mode switch. Use EWMA plus the clipped value from 10 ticks back.
6. Gate release on compare_traces identity in 30 mode, in 60 mode, and in a stress mode that runs the slow path and presents at the first checkpoint on every hit.

## SITE {
 "id": "CP_MOD",
 "address": "0x62EA97",
 "length": 7,
 "original_hex": "8d 4b 10 8b 01 ff 10",
 "original_asm": "0x62ea97: lea ecx,[ebx+0x10]; 0x62ea9a: mov eax,[ecx]; 0x62ea9c: call dword ptr [eax]   ; UpdateModule::update() in the GameLogic::update bucket loop",
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:CP_MOD_STUB> 90 90",
 "stub": "CP_MOD_STUB:\n- `cmp byte ptr [g_presentPending],0 / jne slow`\n- go: `lea ecx,[ebx+10h]; mov eax,[ecx]; jmp dword ptr [eax]`. The callee returns to 0x62EA9C (90 90), then 0x62EA9E.\n- slow: `CP_SLOW IDX_CP_MOD; jmp go`.",
 "live_after": "- **At entry:** EBX = sleepy entry (+8 object, +0x10 interface, +0x14 wake frame), ESI = GL, EDI = bucket vector, EBP frame. EAX and ECX are written by the span; EDX and EFLAGS are dead; x87 is empty (call boundary).\n- **After:** EAX = sleep frames returned by the callee (0x62EA9E cmp eax,1), and EBX/ESI/EDI/EBP are preserved, all as stock.\n- **Difference from stock:** the return slot holds the value 0x62EA9C instead of 0x62EA9E.",
 "branch_into_span_check": "- 0x62EA97 is the target of `je` at 0x62EA8C (span start, allowed).\n- Interior 0x62EA98..0x62EA9D: no direct branch (linear sweep of all executable sections), no brute rel8/rel32/jcc32 decode, and no abs32 value anywhere in the image.\n- No overlap with sites.json main/wip, lw_final_sites.json or AOTR_HOOKS.",
 "purpose": "- Main checkpoint, before every awake module update in subs 3/4 (bucket 0, AIUpdate halves) and sub 5 (buckets 1-2: physics, weapons, horde contain and others). Sub 6 is empty.\n- Hits (INFERRED, large battle): about 3k-12k in sub 5 and about 1k-2.5k in each of subs 3 and 4; at most about 16k per tick.\n- Gap: one module update, typically under 0.05 ms, worst about 0.5-1 ms."
}

## SITE {
 "id": "CP_PATH",
 "address": "0x6EC0D1",
 "length": 7,
 "original_hex": "56 8d 91 00 08 00 00",
 "original_asm": "0x6ec0d1: push esi; 0x6ec0d2: lea edx,[ecx+0x800]   ; pathfinder queue pop, called once per path from 0x6F24AE / 0x6F2504 inside 0x6F2364",
 "kind": "func_detour",
 "replacement_hex": "e9 <rel32:CP_PATH_CAVE> 90 90",
 "stub": "CP_PATH_CAVE:\n- `cmp byte ptr [g_presentPending],0 / jne slow`\n- go: `push esi; lea edx,[ecx+800h]; jmp dword ptr [T_6EC0D8]`\n- slow: `CP_SLOW IDX_CP_PATH; jmp go`. The slow path runs before `push esi`, so the stack is as at entry.",
 "live_after": "- **At entry:** ECX = queue (this+0x1C9E8 or +0x1C1E0); [esp] = return to 0x6F24B3 or 0x6F2509; EDX and EAX are dead (written at 0x6EC0D2 and 0x6EC0D8); EFLAGS dead (function entry); x87 empty.\n- **At 0x6EC0D8:** ESI pushed, EDX = ECX+0x800, exactly as stock.",
 "branch_into_span_check": "- The only references are `call 0x6EC0D1` at 0x6F24AE and 0x6F2504, both to the span start.\n- Interior 0x6EC0D2..0x6EC0D7: no direct, brute or abs32 hit.\n- No overlap.",
 "purpose": "- Checkpoint before each path search, on every sub and twice in sub 5 (TheAI 0x6FEC66).\n- Hits: paths solved, up to about 100 per call (4000-cell budget), about 700 per tick or fewer.\n- Gap: one A* search (15k-25k cells max), about 1-4 ms (INFERRED); it cannot be split. The zone update 0x93A530 runs before the first pop."
}

## SITE {
 "id": "CP_PLAYER",
 "address": "0x6A84E5",
 "length": 5,
 "original_hex": "e8 7f 6d 00 00",
 "original_asm": "0x6a84e5: call 0x6af269   ; Player::update in ThePlayerList::update loop (20 slots)",
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:CP_PLAYER_STUB>",
 "stub": "CP_PLAYER_STUB:\n- `cmp byte ptr [g_presentPending],0 / jne slow`\n- go: `jmp dword ptr [T_6AF269]`. The return address is 0x6A84EA, identical to stock.\n- slow: `CP_SLOW IDX_CP_PLAYER; jmp go`.",
 "live_after": "- **At entry:** ECX = Player* (from mov ecx,[esi] at 0x6A84E3); ESI = slot pointer; EDI = count; EFLAGS dead; x87 empty.\n- **After:** ESI/EDI preserved by the callee; the next flag reader is `dec edi / jne` at 0x6A84ED.",
 "branch_into_span_check": "- The loop target 0x6A84E3 is outside the span. 0x6A84E5 itself is not a branch target.\n- Interior 0x6A84E6..0x6A84E9: no direct, brute or abs32 hit.\n- No overlap.",
 "purpose": "- Sub-5 checkpoint before each Player::update, including AIPlayer::update 0x8F9705 and its 5-tick (0x8F8F68) and 10-tick (0x8F3EFE → 0x8F9487) routines.\n- Hits: 20 per tick.\n- Gap: one player, about 0.05-5 ms (INFERRED); ALT_RECRUIT splits it."
}

## SITE {
 "id": "CP_SKAI",
 "address": "0x6A96F3",
 "length": 5,
 "original_hex": "e8 fe 46 24 00",
 "original_asm": "0x6a96f3: call 0x8eddf6   ; TheSkirmishAIManager (0x6A96A0, sub 5 via 0x62EBF7) per-AI entry update",
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:CP_SKAI_STUB>",
 "stub": "CP_SKAI_STUB:\n- `cmp byte ptr [g_presentPending],0 / jne slow`\n- go: `jmp dword ptr [T_8EDDF6]`. The return address is 0x6A96F8, identical to stock.\n- slow: `CP_SLOW IDX_CP_SKAI; jmp go`.",
 "live_after": "- **At entry:** ECX = entry (mov ecx,[edi] at 0x6A96F1); EDI = iterator; ESI = &mgr+0xA4C; EFLAGS dead; x87 empty.\n- **After:** `add edi,4 / cmp edi,[esi]` sets the flags.",
 "branch_into_span_check": "- The loop target 0x6A96F1 is outside the span.\n- Interior 0x6A96F4..0x6A96F7: no direct, brute or abs32 hit.\n- No overlap.",
 "purpose": "- Sub-5 checkpoint per skirmish-AI entry (0x8EDDF6 → 0x6C7946 per item and others).\n- Hits: number of AI players or fewer, about 8 or fewer per tick.\n- It splits the manager tail: the gap from the last Player::update up to here is the manager block, about 0.3-3 ms (INFERRED)."
}

## SITE {
 "id": "CP_SCRIPT",
 "address": "0x60A3BA",
 "length": 5,
 "original_hex": "e8 9d fd ff ff",
 "original_asm": "0x60a3ba: call 0x60a15c   ; execute one script, in ScriptEngine list walker 0x60A377 (reached from ScriptEngine::update 0x60CC67 per side, 0x60BCE5 group recursion, 0x60BD42, 0x60BFBD)",
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:CP_SCRIPT_STUB>",
 "stub": "CP_SCRIPT_STUB:\n- `cmp byte ptr [g_presentPending],0 / jne slow`\n- go: `jmp dword ptr [T_60A15C]`. The return address is 0x60A3BF and the two pushed arguments sit at [esp+4] / [esp+8], identical to stock.\n- slow: `CP_SLOW IDX_CP_SCRIPT; jmp go`.",
 "live_after": "- **At entry:** ECX = ScriptEngine (mov ecx,ebx); two arguments pushed at 0x60A3B6/0x60A3B7; ESI = list node; EDI = context; EBX = engine; EFLAGS dead; x87 empty.\n- **After:** `mov esi,[esi] / test esi,esi`.",
 "branch_into_span_check": "- No branch to 0x60A3BA.\n- Interior 0x60A3BB..0x60A3BE: no direct, brute or abs32 hit.\n- No overlap.",
 "purpose": "- Sub-1 checkpoint before each script evaluation (map and skirmish AI scripts). Scripts can be slow: the engine itself logs \"slow script\" above 10 ms per frame.\n- Hits: active scripts per tick, about 100-3000 (INFERRED).\n- Gap: one script, typically under 0.5 ms."
}

## SITE {
 "id": "CP_OBJ1",
 "address": "0x62E912",
 "length": 5,
 "original_hex": "e8 fc f6 0d 00",
 "original_asm": "0x62e912: call 0x70e013   ; per-object loop 0x62E910 in GameLogic::update sub 1 (getter mov eax,[ecx+0x84]; followed by 0x674B1F when non-null)",
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:CP_OBJ1_STUB>",
 "stub": "CP_OBJ1_STUB:\n- `cmp byte ptr [g_presentPending],0 / jne slow`\n- go: `jmp dword ptr [T_70E013]`. The return address is 0x62E917, identical to stock.\n- slow: `CP_SLOW IDX_CP_OBJ1; jmp go`.",
 "live_after": "- **At entry:** ECX = Object (mov ecx,ebx at 0x62E910); EBX = object; ESI = GL; EFLAGS dead; x87 empty.\n- **After:** EAX = getter result, read by `test eax,eax` at 0x62E917.",
 "branch_into_span_check": "- The loop target 0x62E910 (jne at 0x62E933) is outside the span.\n- Interior 0x62E913..0x62E916: no direct, brute or abs32 hit.\n- No overlap.",
 "purpose": "- Sub-1 checkpoint per object, after ScriptEngine, Lua, terrain, victory, recorder and the lists.\n- Hits: all objects, about 3k-5k per tick.\n- It bounds the Lua / post-script segment (about 0.3-4 ms, INFERRED).\n- The sub-1 tail loop 0x62EC5D after it (about 0.3-2 ms) is covered by ALT_OBJ1END or by the end of the step."
}

## SITE {
 "id": "CP_PART",
 "address": "0xA3B564",
 "length": 6,
 "original_hex": "8b b7 20 01 00 00",
 "original_asm": "0xa3b564: mov esi,[edi+0x120]   ; next dirty object, in PartitionManager::update 0xA3B4E0 (sub 2 via 0x62E943)",
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:CP_PART_STUB> 90",
 "stub": "CP_PART_STUB:\n- `cmp byte ptr [g_presentPending],0 / jne slow`\n- go: `mov esi,[edi+120h]; ret`. It returns to 0xA3B569 (nop), then 0xA3B56A `test esi,esi`.\n- slow: `CP_SLOW IDX_CP_PART; jmp go`.",
 "live_after": "- **At entry:** EDI = PartitionManager; EAX/ECX/EDX dead (each is rewritten at 0xA3B4F1 / 0xA3B4FE / 0xA3B506 before any read, and the function returns void); EFLAGS dead (`test esi,esi` follows); x87 empty (no x87 in the function).\n- **After:** ESI = [EDI+0x120], as stock.",
 "branch_into_span_check": "- 0xA3B564 is the target of `je` at 0xA3B552 (span start, allowed).\n- Interior 0xA3B565..0xA3B569: no direct, brute or abs32 hit.\n- No overlap.",
 "purpose": "- Sub-2 checkpoint per moved object re-celled by the partition manager.\n- Hits: moved objects, about 1k-4k per tick.\n- Gap: one object, under 0.05 ms."
}

## SITE {
 "id": "CP_COLL",
 "address": "0xB6D11E",
 "length": 5,
 "original_hex": "8b 4e 04 8b 16",
 "original_asm": "0xb6d11e: mov ecx,[esi+4]; 0xb6d121: mov edx,[esi]   ; start of the active-pair path in collision callbacks 0xB6D060 (onCollide vt+0x24 both ways)",
 "kind": "call_gate",
 "replacement_hex": "e8 <rel32:CP_COLL_STUB>",
 "stub": "CP_COLL_STUB:\n- `cmp byte ptr [g_presentPending],0 / jne slow`\n- go: `mov ecx,[esi+4]; mov edx,[esi]; ret`. It returns to 0xB6D123.\n- slow: `CP_SLOW IDX_CP_COLL; jmp go`.",
 "live_after": "- **At entry:** ESI = pair, EBX = CollisionManager; EAX/EBP/EDI dead (rewritten at 0xB6D12E / 0xB6D129 / 0xB6D12B); EFLAGS dead (next reader `test ecx,ecx` at 0xB6D146 comes after calls); x87 empty (the x87 temporaries at 0xB6D15C..0xB6D17C are always popped).\n- **After:** ECX = [esi+4], EDX = [esi], as stock.",
 "branch_into_span_check": "- Nothing branches to 0xB6D11E; it is the fall-through of `je` at 0xB6D118.\n- Interior 0xB6D11F..0xB6D122: no direct, brute or abs32 hit.\n- No overlap.",
 "purpose": "- Sub-2 checkpoint per active collision pair, before its collide-module callbacks.\n- Hits: active pairs, about 0.5k-15k per tick in dense melee (INFERRED; the most of any site, still about 30 µs of fast-path cost).\n- The sort-and-sweep update before it, 0xB6D8A0, is covered by ALT_SAP."
}

## VERIFIER OVERALL
I checked the checkpoints (C) analysis against the binary, the repo, the DXVK source copy and the simulators. Its patch engineering holds up. I re-ran verify_cp.py from my own folder: all 8 core sites have 0 errors and 0 notes, and the 6 optional sites show only the misaligned brute-force false positives it already names. My own disassembly confirms the bytes, instruction boundaries, live registers, dead flags and the order of work inside GameLogic::update. I also closed a gap that ../hitching/area_engine.md left open: sub 2 really does run the partition manager and the collision manager (CONFIRMED through their constructors' vtables). Re-running the simulator cgran.py reproduces out_cgran.txt exactly.

The design is feasible and lower risk than the presenter thread (T). One reason is that logic does call D3D (CreateVertexBuffer; CONFIRMED on a static path), so (T) would race on a device that was not created MULTITHREADED.

Six things are wrong or unproven:
1. **Slow-path cost.** The "about 0.3 ms per heavy step at most" figure fails when every B is deferred. Then every hit before the target takes the 45 ns slow path, and sub 2 in dense melee alone reaches about 0.9 ms. A sensitivity run shows that 0.5 to 1 ms of extra logic per step erases (C)'s advantage over (T).
2. **Stack snapshot.** Copying 32 KB below ESP can read reserved, uncommitted stack and crash. It also does not keep the stack identical to stock, because the slow path itself writes below ESP on every hit.
3. **"End of step" location.** LogicUpdateWrapper is not the end of the step. GameEngine::step runs TheLivingWorldLogic's update after GameLogic::update(1) returns.
4. **Predictor gaps.** The predictor rules miss two code paths: an early return guarded by [GL+0x125], and a full update that runs without the frame counter advancing when 0x625130 returns false.
5. **Integration with the pacer.** The analysis does not say how deferral fits the existing B-Present rules in src/pacer.cpp: the forced skip under the camera time multiplier, the FIFO late skip, the spacing wait, RecordPresentSpacing and PresentDone. It also does not cover what happens to a pending Present at Reset_Device (0x522000).
6. **Profile data.** All hit counts and gap sizes are guesses. There is still no per-sub profile, and ../hitching/synthesis.md says the same.

- [confirmed] Original bytes, lengths and instruction boundaries of the 8 core sites (CP_MOD 0x62EA97 '8d 4b 10 8b 01 ff 10', CP_PATH 0x6EC0D1, CP_PLAYER 0x6A84E5, CP_SKAI 0x6A96F3, CP_SCRIPT 0x60A3BA, CP_OBJ1 0x62E912, CP_PART 0xA3B564, CP_COLL 0xB6D11E) match game.dat; no branch, abs32 value or brute decode lands in any span interior; no overlap with sites.json (main/wip), lw_final_sites.json or the AotR hooks
  I re-ran a copy of verify_cp.py (hitch\mt\checkpoints_verify\verify_cp_out.json): 0 errors and 0 notes for all 8 core sites. Branches to the span starts are 0x62EA8C, 0x6F24AE/0x6F2504 and 0xA3B552 only. My own disassembly (./dis.sh) agrees. Only the optional sites ALT_RECRUIT and ALT_SKAI0 get notes: rel8 decodes from 0x8F9543 and 0x6A96C4, which start inside other instructions, so they are false positives as stated. The script also lists ALT_SUB2MID (from 0x62E9C0, inside `cmp edi,[ebp-0x20]` at 0x62E9BE), which is likewise a false positive.

- [confirmed] live_after, dead flags and empty x87 at every site; CP_MOD returns to 0x62EA9C (nop nop) and then 0x62EA9E; CP_PART/CP_COLL call+ret stubs are exact
  0x62EA9E `cmp eax,1` rewrites the flags. At 0xA3B56A `test esi,esi` reads only ESI. 0xB6D123..0xB6D13E are movs, leas and calls before the next flag reader. CP_PATH is a function entry reached only from 0x6F24AE/0x6F2504. The x87 temporaries at 0xB6D15C..0xB6D17C all end in fstp. The fast paths write only the registers the original span wrote. The slow path's SAVE_ALL/RESTORE_ALL (stubs.asm:59-86) restores the flags (pushfd/popfd) and DF.

- [refuted] Five of the sites are plain 5-byte call rel32 replacements; recommendation 1 says '5 pure calls, 1 func_detour, 2 short call_gates'
  Only four sites replace a direct call: CP_PLAYER, CP_SKAI, CP_SCRIPT and CP_OBJ1. CP_MOD is a 7-byte span ending in an indirect `call [eax]`, and its return slot changes to 0x62EA9C. CP_PART (6 bytes) and CP_COLL (5 bytes) replace movs. This is cosmetic, but the sites.json entries must describe them correctly. Using call_gate for non-call spans does follow existing practice (HALT, C0_PRERENDER).

- [confirmed] Bucket mapping: subs 3/4 = bucket 0 halves, sub 5 = buckets 1-2, sub 6 = bucket 3 (empty)
  0x62E988..0x62E9BE: sub 3/4 → edi=0, end=1. Sub 5 → edi=1, end=3. Sub 6 → edi=3, end=4. Halving is at 0x62E9D4 (sub 4 starts at count/2) and 0x62EA09 (sub 3 stops at count/2). 'Bucket 3 is empty' is INFERRED only.

- [confirmed] Sub 2 runs the partition manager 0xA3B4E0 (CP_PART) and the collision manager 0xB6E6B0 → 0xB6D060 (CP_COLL) via 0x62E943/0x62E94E
  No vtable points at 0xA3B4E0 or 0xB6E6B0 directly, so I traced the constructors. 0x62CEB3 stores the object built by ctor 0xA39120 in [0xDE4354]; that ctor writes vtable 0xC95180 at 0xA3915D, and vt+0x28 = 0xA39020 (`mov ecx,[ecx+0x10]; jmp 0xA3B4E0`). 0x62CF4C stores the object built by ctor 0xB6C490 in [0xDE4360]; it writes vtable 0xD0BF08 at 0xB6C4CD, and vt+0x28 = 0xB6C420 → call 0xB6E6B0 → 0xB6D060. This settles the point ../hitching/area_engine.md:307 had marked as unverified.

- [confirmed] The pathfinder 0x6F2364 runs at the start of every sub (0x62E69F) and a second time in sub 5 through TheAI (0x6FEC66); CP_PATH fires once per queued path; the budget is x100 while frame < 5·LTR
  The only references to 0x6F2364 are 0x62E69F and 0x6FEC66. TheAI::update 0x6FEC63 calls the pathfinder and then tail-jumps to [0xDE4928] vt28, which is ThePlayerList 0x6A84DB (20 slots, `push 0x14`). The queue pop 0x6EC0D1 is called only at 0x6F24AE and 0x6F2504. The budget code is 0x6F246E..0x6F2487 (`imul [0xD9F608],5`; `imul esi,100`). The hit counts are inconsistent: 'about 100 paths per call at a 4000-cell budget' does not fit '15k-25k cells per path'. Neither is measured.

- [confirmed] Stepper/predictor rules (0x632622, 0x6326C6/0x6326F0, 0x6326E1, 0x632AF7, 0x62E577, 0x62E5EA, pause [GL+0x124] via 0x90F92C)
  All addresses and their semantics match. The rule 'frozen = (TV vtD8 && !vt78) || 0x60342F()' matches 0x62E520..0x62E554. The analysis misses three paths:
(a) At 0x62E5AC, when [GL+0x125] is set and GL+0x110 is not 1 or 5, sub 1 sets GC+0xC8=1 and returns without work, possibly after frame++. Subs 2-6 then also do no work, so 'subs 2-6 always do real work' is not absolute.
(b) When not frozen but 0x625130 is false, a full update runs without frame++. That is not only the 'frozen' case.
(c) GE::step loops over GL::update again at 0x632AC0..0x632AEC when [0xD9F60C]/[0xD9F608] < 6. With stock values this is inert.

- [refuted] At the end of the step, LogicUpdateWrapper can present the B frame if due
  LogicUpdateWrapper (telemetry.cpp:598) wraps only GL::update (vt+0x34). GE::step then calls [0xDE4950] vt28 (TheLivingWorldLogic) at 0x632A8A after sub 1. The Y iteration also continues at 0x6326F6 ([0xDC62C0] vt94) before the pacer. So 'end of step' is earlier than the true end; this work is a gap with no checkpoint. It is harmless for correctness but wrong for timing.

- [confirmed] The B Present's arguments are all NULL (EBX=0, stubs.asm:785-792); Present = device vt+0x44 after EndScene vt+0xA8
  `xor ebx,ebx` at 0x5225F6. EndScene `call [ecx+0xA8]` is at 0x522628, then 0x5766A0, then Present at 0x522650. The pushes are at stubs.asm:787-790.

- [uncertain] DXVK Present does not change x87 CW/MXCSR; Present does not block; DEVICELOST is handled by setting a flag
  The scratchpad DXVK source copy calls SetupFPU only in the device constructor (d3d9_device.cpp:91-92), so Present leaves the CW alone. That copy matches the 2.6+-era installed d3d9.dll by its Reflex/latencySleep strings, but the exact version is not proven. Present can block in two places: SyncFrameLatency (d3d9_swapchain.cpp:911/1128, waits for frameId - maxFrameLatency) and acquireNextImage. The measured late_skips = 0 and an empty dxvk.conf (no latencySleep, no frame limiter) make blocking unlikely but do not rule it out. Present returns D3DERR_DEVICELOST when IsDeviceLost() is true (d3d9_swapchain.cpp:116). EndScene calls ConsiderFlush(ImplicitStrongHint) (d3d9_device.cpp:1807), so the GPU work for B is usually submitted at EndScene and the deferred Present only adds the blit. That is good for (C) but should be measured.

- [confirmed] D3D calls happen during logic and are legal between EndScene and Present
  reach.py from 0x62E4E8 (direct calls only) finds 0x779A3D → … → 0x539290, which calls CreateVertexBuffer (vt+0x68) and, on failure, 0x51DFB0 EvictManagedResources (vt+0x14). These calls are legal for (C). They also prove that (T) needs D3DCREATE_MULTITHREADED. Virtual-call paths (drawables, shroud [0xDE4358]) are not audited for SetRenderTarget, Clear or Draw on the back buffer.

- [refuted] The slow path costs about 45 ns per hit before the target, about 0.3 ms per heavy step at most, so (C) can defer every B
  With every B deferred, every hit before the target is slow. Sub 2 in dense melee (CP_COLL about 15k + CP_PART about 4k, the analysis's own numbers) costs about 19k × 45 ns ≈ 0.86 ms. Sub 5 with half of 12k CP_MOD hits costs about 0.27 ms; the other subs are similar. My sensitivity run (checkpoints_verify\cgran_ovh.py, extra logic time per C step) gives, for FIFO cal P3: g=2 >34ms/s 0.22→0.27→0.38 and p2p 9.8→9.8→13.6 ms at +0/0.5/1.0 ms. T has 0.28 and 9.7. So 0.5-1 ms of overhead removes (C)'s edge. The fix is an inline rdtsc compare against a TSC target in the fast path (about 25 cycles) instead of SAVE_ALL+QPC.

- [refuted] A 32 KB stack snapshot around the checkpoint Present is cheap insurance, and LogicUpdateWrapper's stack shift shows stale stack does not matter
  (1) Reading 32 KB below ESP can touch MEM_RESERVE pages below the guard page and raise an access violation (main stack is 1 MB reserve, 4 KB commit). The copy must be clamped to TEB StackLimit fs:[8]. (2) SAVE_ALL (164 bytes) plus the C frame write below ESP on every slow hit, even when nothing is presented. So a snapshot taken only around Present cannot keep the stale stack identical to stock. (3) Both 30 and 60 mode run through LogicUpdateWrapper, so compare_traces has never compared against a run without the wrapper. Stack independence is UNPROVEN, though likely, since reading stale stack would be undefined behaviour.

- [confirmed] Simulation basis: checkpoint granularity gives (C) ≈ (T)
  `python cgran.py` re-run (2.5 s) reproduces simulation\out_cgran.txt exactly, e.g. fifo cal C g=2: 25.0/25.0/33.3/41.7 gap p99, T: 25.0/33.3/33.3/41.7. The model has limits: checkpoints are uniform every g ms with random phase, with no slow-path overhead and a present_cost of 0.15 ms. The real distribution (dense sub-ms in module loops, 2-5 ms holes) is INFERRED, so g=4..8 is the honest bracket.

- [uncertain] Wall-clock time does not affect logic (implicit in 'bit-identical')
  This was not in the analysis. Logic does read wall-clock time. ScriptEngine reads timeGetTime at 0x60CD8E and 0x60CE34; a script slower than 10 ms only triggers a sprintf into a local buffer at 0x60CE5B, so there is no effect. The pathfinder calls QPC at 0x6F255E and 0x6F258F, gated by GameData+0x11C0; when it fires it only sets a debug tint via 0x6ED163, and CP_PATH sits outside the timed region. 0x62C159 calls QPC only when [0xDE4148] is set. A checkpoint Present lengthens these measurements. They look harmless today, but this rests on a static graph only; virtual paths are not checked.

## VERIFIER MISSED
Not covered by the analysis, and needed before a safe implementation:
1. **Integration with the existing B-Present rules in src/pacer.cpp:298-371.**
   - The forced skip under the camera time multiplier (g_mDrawAM) and the FIFO late-skip rule must be decided at the B PRESENT_STUB.
   - The deferred present must still feed RecordPresentSpacing, g_presentCall/AotR60_PresentDone (presentsSinceBlock) and the relPresent statistics.
   - The stub must return S_OK so the stock code at 0x52265A increments [0xDD34C4] and clears [0xDD1F38], as the existing skip path does.
2. **Reset and device loss while a present is pending.** GAP_RESET_CAVE (0x522000 Reset_Device), a quit or load out of the main loop, and a switch to LW mode must each flush or cancel g_presentPending. Otherwise Present runs after Reset on a swapchain that was recreated.
3. **Predictor contamination.** LogicUpdateWrapper durations will include the checkpoint Present time and the slow-path overhead. These must be measured and subtracted before learning, the same pitfall as "Present blocking learned as render time".
4. **Determinism guards.** Snapshot the logic RNG seed ([kLogicRngSeed]) before and after each checkpoint Present and disable deferral on any change. Do the same for the CW/MXCSR comparison. Check that no wndproc ran: DXVK's SetWindowPos in d3d9_window.cpp:75 and wsi_window_win32.cpp can send messages synchronously to the game's window procedure.
5. **Fast-path design.** Use an inline TSC compare, with the QPC/TSC ratio calibrated at startup, instead of SAVE_ALL+QPC on every hit before the target. Use a static 16-byte-aligned fxsave buffer (main thread only).
6. **Stack snapshot bounds.** If a snapshot is kept, clamp it to fs:[8] (StackLimit).
7. **Measurement first.** No per-sub profile exists yet, and ../hitching/synthesis.md:27 says the heavy sub is "not measured". Profile mode should first log which sub is heavy, then per-site counts and gaps.
8. **D3D audit of the virtual paths reachable from logic.** Check for SetRenderTarget, Clear, Draw*, StretchRect, GetBackBuffer and Begin/EndScene, using a debug vtable hook that counts device calls between B EndScene and the deferred Present.
9. **Gap after GL::update(1).** The TheLivingWorldLogic vt28 call at 0x632A8A and the rest of the Y iteration after 0x6326F6 have no checkpoint and sit outside LogicUpdateWrapper.
10. **Pathfinder figures.** GameData+0x11E8 (cells per frame) and +0x11C0 (debug timing) should be checked in AotR's INI; the "4000-cell budget" and "15-25k cells per path" figures contradict each other.

Files are in <analysis workspace>/hitch\mt\checkpoints_verify\:
- verify_cp_out.json (re-run of the site checks)
- glupd.txt (disassembly of GameLogic::update)
- cgran_rerun.txt (simulator re-run)
- cgran_ovh.py, mtsim_ovh.py and ovh_out.txt (overhead sensitivity run)
- timefuncs.txt (timer-reading functions)