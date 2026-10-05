# G7_aotr_hooks

## Summary

G7 result: I disassembled every byte of AotR code in .danetta (0xECA000-0xED2200) and .angmar (0xED3000-0xED3200). I also byte-diffed game.dat against stock game820.dat across the whole image, and diffed game.dat against delayfix.dat.

**Hook count.** game.dat has 14 jmp/call hooks plus 2 changed registry-string pushes. The task list was incomplete. Not listed but present:
- 0x52CC7F: push 0xEA7690
- 0x69413A: jmp 0xED0300
- 0x79DCE0: jmp 0xED1900
- 0x88D061: jmp 0xED1B00
- 0x8A11A3: jmp 0xED1C00
- Table-pointer change at 0x8ED516 (0xDB55C8 -> 0xED3012) and a second byte at 0x73BDD7.

**What AotR adds.** Almost everything is the Palantir statistics feature: resources per minute, K/D, spent on army, favourite unit, faction name, and the "Cammy" bars. It is fed by logic-event hooks: object create/destroy, ScoreKeeper money earned, refunds, and logic reset. The two other functional changes are:
- Damage-modifier stacking (0x5D8A64/0x5D8AF1 -> 0xECA000): logic math, multiplicative 1-(1-a)(1-b) instead of additive.
- Registry-key redirection: init only.

**Timing.** No AotR code references any timing global, GameClient (0xDE4388), m_frame, timeGetTime/QPC, the W3D sync clock or GE+0x34..0x5C. A systematic operand scan finds exactly these external globals: 0xDE3C14, 0xDE3F0C (TheAptPlayer), 0xDE412C (TheGameLogic; reads +0x40 only), 0xDE4928 (ThePlayerList), 0xDE4950, 0xDE7744 (TheControlBar +0x218 = observed player). Called helpers: APT set-variable 0x625071, APT call-ActionScript 0x62279C, string helpers, and the stock functions they wrap.

**Logic state.** No AotR code writes logic state. Outside stock behaviour it writes only AotR-private tables (0xECD000-0xED2200, unsaved) and APT variables. 0xECA000 is part of deterministic damage math called only from logic.

**The one per-render hook.** 0x6D57AF -> 0xED0B00 sits inside the Palantir update. It is reached from InGameUI::update (0x6A23DF) and also from LivingWorldManager::update (0x61226F). It holds the only frame-count-dependent AotR state: a per-call counter [0xED0000] that samples per-player income every 30 calls (0xED0816 `cmp 0x1E`) into a 7-slot ring. The displayed RPM is the change over 6 samples = 180 Palantir updates, about 5.94 s at stock. Run 2x, this window halves, so RPM shows about 50% and refreshes twice as often. ActionScript call volume (SetStatsStatus, SetStatsMode, 2N x SetCammyBar) also doubles. This contradicts the synthesis row 86 "harmless scan".

**Effect of the 60 FPS design.**
- Logic and event hooks: unchanged, as long as C1 keeps the GameLogic::update call pattern.
- Palantir hook: identical to stock if the Palantir update runs only on g_uiTick renders (skip on B-renders). The planned 0x6A23D7 gate covers the battle HUD path but not the Living World path 0x61226F. Gating the callee thunk 0x5039C1 (W3DAptPalantir vt+0x28, `e9 6d 41 1d 00`) covers both.

**Patch-site overlap.** All 22 planned patch sites are byte-identical to stock in game.dat and delayfix.dat, and match the synthesis original bytes. No overlap.

**delayfix.dat must be detected.** It changes stepper semantics:
- 0x632537: isTick divisor becomes [0xECA400]=8, so isTick is true at s==2.
- 0x632A9B `b8 02 00 00 00 eb 19`: vt98(1) runs logic sub 1 and 2 in one frame and increments GameEngine+0x34.
- 0xED081C: Palantir sample every 25 calls; together with the 5-frame tick this suggests a 25 FPS target (inference).
This is incompatible with C1 as designed: logic sub 2 would run twice per tick. The 60 mode must be disabled on it or redesigned for it.

**Dead debug hook.** 0x9A3AE0 jumps to 0xED3000, which is data (`f4 be bf 00`) and decodes to HLT. If ever executed it would crash at any FPS. It is removed in delayfix and is irrelevant to timing.

## Design notes

**AotR fits the B-render allow-list principle by default.** Its only per-render code is inside the Palantir update, a UI subsystem that is not on the allow-list. Everything else is logic- or event-driven and never on the render path. The "skip unknown per-render work on B-renders" default keeps AotR stock-correct.

**Where to gate the Palantir.** Put the skip at the callee: W3DAptPalantir vt+0x28 thunk 0x5039C1 (`e9 6d 41 1d 00` -> stub `if (g_m60 && !g_uiTick) ret; jmp 0x6D7B33`), or vtable slot 0xBE5770. One site then covers both callers:
- InGameUI::update 0x6A23DF (battle HUD);
- TheLivingWorldManager::update 0x61226F (Living World map, also reached from W3DDisplay vt+0x188 0x444CD4).
The planned caller gate at 0x6A23D7 alone misses the Living World path. If caller gates are kept, add 0x612267.

**Do not patch AotR bytes:**
- 0x6D57AF..0x6D57B3;
- the 30-call sampler imm at 0xED081C (delayfix tunes it to 25);
- any .danetta/.angmar code.

**Do not put DLL code caves in AotR sections.** 0xECA200..0xECA20F holds constants and scratch, 0xECA400 is used by delayfix, and 0xECD000..0xED2200 plus 0xED0000..0xED000C are runtime tables and counters. Use VirtualAlloc'd memory.

**Corrections to the synthesis:**
1. Row 86 / R10 call the AotR counter a "harmless scan". It is the resources-per-minute statistic: a 30-call sample period and a 6-sample window, 180 Palantir updates. Run 2x, the displayed RPM is about 50% and refreshes 2x. Gating is required, not optional.
2. The R10 hook list is incomplete. It misses 0x52CC7F, 0x69413A, 0x79DCE0, 0x88D061 and 0x8A11A3 (code) and 0x8ED516 / 0x73BDD7 (data). All are init, logic or event cadence except the Palantir hook.
3. 0x9A3AE0 is not a working hook. Its target 0xED3000 is data that decodes to HLT; delayfix removes it.

**Delayfix.dat must be detected at startup.** Check 0x632A9B for `b8 02 00 00 00 eb 19` or 0x632537 for `00 a4 ec 00`; if present, disable 60 mode. Its vt98 runs logic sub 1 and 2 in one frame and increments GameEngine+0x34, and its isTick fires at s==2. Both break C1's 12-sub-frame schedule (sub 2 would run twice per tick) and the A/B classification. Supporting delayfix would need its own schedule: 5 logic frames per tick, probably at a 25 FPS base.

**Optional load-balancing (contradicts nothing, but changes phase).** The AotR Palantir work is not trivial: about 4 + 2N ActionScript calls plus 3 string-formatted APT SetVariables per update. Under model H it runs on A-renders, which already carry the logic sub-step (R3 budget). Running all 30 Hz UI work on B-renders instead (g_uiTick = !b while running, still alternating when paused) keeps exactly one UI update per render pair. That gives the same counts and the same logic state, since B-frames do not change logic. Two m_frame cases to check:
- m_frame%N users (ControlBar flash) still see each m_frame value once.
- The m_frame-based ControlBar fix in row 68 becomes unnecessary if ControlBar is likewise skipped on one phase.
Measure with M4 before choosing.

**Saves and logic are unaffected.** AotR keeps no saved state; its tables are reset at GameLogic reset (0xED1600), except the sampler counter [0xED0000], which harmlessly carries over. The logic-side hooks (0xECA000 damage stacking, object create/destroy, ScoreKeeper, refunds) need nothing beyond the C1 guarantee of exactly one GameLogic::update(sub) for sub 1..6 per tick and stock failed-tick attempts.

## Items

### AOTR-01 — AotR Palantir statistics (UI/APT)  [skip_on_B, high]
- **Site:** 0x6D57AF jmp 0xED0B00 (replaces `a1 50 49 de 00` mov eax,[0xDE4950]) in FUN_006D577C <- AptPalantir::update 0x6D7647 <- thunk 0x6D7B33 <- W3DAptPalantir vt+0x28 = 0x5039C1 (vtable 0xBE5748, slot 0xBE5770; object created at 0x48ECAF/0x503EF9, stored in 0xDE4A70) <- callers InGameUI::update 0x6A23DF and LivingWorldManager::update 0x61226F (0x6121C5, vtable 0xBFB548 slot 0xBFB570). Body: 0xED0B00 -> 0xED0400, 0xED0500/0xED0580(->0xED0F30/0xED0F70), 0xED1E00(->0xED1E5D/0xED1F32), 0xED0800(->0xED0200) -> back to 0x6D57B4
- **What:** On every Palantir update the hook does the following.
- K/D for the observed player (TheControlBar+0x218): sum of 20 dwords at player+0x3FC, x100, divided by player+0x450 (min 1). Written via 0xED0400 as an "x.yz" string to APT var 'APT:PalantirKD' (0x625071).
- 0xED0500: calls ActionScript SetStatsStatus('250','75','0xBE1414',[0xED05FB]) via 0x62279C.
- 0xED0580: calls SetStatsMode('0' or '1'), chosen by whether the player template name starts 'Obse' or 'Civi'.
- 0xED1E00: for up to 8 players in 0xED2000 calls SetCammyBar and SetCammyBar2 with permille shares of the RPM delta (+0xC) and spent-on-army (+0x34).
- 0xED0800: `inc [0xED0000]`; at 30 (`cmp dword [0xED0000],0x1E` at 0xED0816) it resets and, for each player in ThePlayerList, writes the income counter [0xECF800+i*0x40+0x24] into a 7-slot ring. Delta = current minus the sample 6 periods ago, stored at +0x20 and mirrored to 0xED2000[k]+0xC. If the observed player's delta changed ([0xED000C]), it sets 'APT:PalantirRPM' via 0xED0200.
- Finally re-executes mov eax,[0xDE4950] (0xED0989) and jumps to 0x6D57B4.
- **Cadence:** Per render, about 30.3 per second at stock, whenever the Palantir is active (FUN_006D4609) and its mode has not just changed (+0xE8 check in 0x6D7647). The InGameUI::update call at 0x6A23D7..0x6A23DF is unconditional on that path (verified at 0x6A23CD..0x6A23E2). It is also reached from TheLivingWorldManager->update (0x61226F), which is called from the LW map view 0x6C0E7A (via 0x49AAD4) and from W3DDisplay vt+0x188 0x444CD4 (0x444CDC); no static caller of vt+0x188 was found. [0xED0000] is the only call-count-dependent state in all AotR code.
- **Reason:** This is a UI subsystem, not in the allow-list, and its RPM statistic is defined in calls. Stock: 30-call sample period x 6 = 180 calls, about 5.94 s at 30.3 FPS. At 60 calls per second the window becomes about 2.97 s, so the displayed RPM is about half and refreshes 2x. The Cammy bars use ratios, so their scale is unaffected, but they get noisier and twice the ActionScript calls (about 4 + 2N per call). Running the whole Palantir update only on g_uiTick renders (A-renders, alternating while paused) reproduces stock exactly. It also covers the stock Palantir counters 0x6D72DF/0x6D57B4. AotR bytes need no change.
- **Risk if wrong:** Without the skip: Palantir RPM about 50% too low, 2x APT/ActionScript CPU on every render (worse for the frame-time budget, R3), and stock Palantir timers 2x. If the gate is only at 0x6A23D7, the Living World path 0x61226F still runs 2x. If someone patches the AotR counter (0xED0816 imm) instead, it breaks delayfix (0x19) and AotR updates.
- **Fix @ 0x5039C1:** Preferred: jmp to a stub doing `if (g_m60 && !g_uiTick) ret; else jmp 0x6D7B33`. The function takes no stack args (0x6D7647 ends `leave; ret`); preserve ECX. This covers both callers (0x6A23DF, 0x61226F). (orig: e9 6d 41 1d 00 (jmp 0x6D7B33; W3DAptPalantir vt+0x28 thunk, identical in game.dat/game820/delayfix))
- **Fix @ 0x6A23D7:** Planned caller gate (row 67). Fine for battles, but needs the companion site below for Living World. (orig: 8b 0d 70 4a de 00 8b 01 ff 50 28)
- **Fix @ 0x612267:** Alternative companion gate for the Living World path (same g_uiTick condition), if 0x5039C1 is not used. (orig: 8b 0d 70 4a de 00 8b 01 ff 50 28)

### AOTR-02 — AotR damage-modifier stacking (logic)  [not_on_B_path, high]
- **Site:** 0x5D8A64 `e8 97 15 8f 00` and 0x5D8AF1 `e8 0a 15 8f 00` (call 0xECA000; stock called 0x68C818) inside FUN_005D893C (armor/damage-type multiplier using damage-type names at 0xD9DA08 'FORCE'...). Called by body-module vtable methods 0x8C1C51/0x8C2FC1/0x8C3FA3 (vtables 0xC71ED0.., 0xC72434.., 0xC72678)
- **What:** Reimplements 0x68C818 -> 0x804F39:
- obj = 0x68C4A6(); iterates the obj+0x20..+0x24 modifier list (16-byte entries).
- Skips entries expired against the logic frame [TheGameLogic+0x40] vs entry+8, and those rejected by 0x804D27.
- Queries 0x6144BF on [0xDE3C14].
- Combines results multiplicatively, total = 1-(1-total)(1-v), using 1.0 at 0xECA200 and 0.0 at 0xECA204, instead of the stock additive sum.
- Saves and restores xmm1/xmm2 in scratch at 0xECA208/0xECA20C.
- Returns a bool and writes the float out-param.
- **Cadence:** Logic: per damage/armor evaluation inside GameLogic::update (body modules). Event-driven, never on the render path.
- **Reason:** Pure, deterministic logic math. Reads only the logic frame and modifier lists; its only global writes are the save/restore scratch. Its result is unchanged if the logic call pattern is unchanged (C1d/C1e).
- **Risk if wrong:** None from rendering. Like all logic, any extra or reordered GameLogic::update call would change damage results.

### AOTR-03 — AotR stats reset on GameLogic reset  [not_on_B_path, high]
- **Site:** 0x629D11 jmp 0xED1600 (replaces `83 66 40 00 83 a6 ac 00 00 00 00`) in FUN_00629D02 (callers 0x62A11A, 0x62CE75, 0x62D232); returns to 0x629D1C
- **What:** Executes the stock `and [esi+0x40],0; and [esi+0xAC],0` (logic frame reset). It then:
- zeros AotR tables 0xECFC00-0xECFC80 (spent on army), 0xECD000-0xECF000 (favourite counts), 0xECF800-0xECFC00 (income rings) and 0xED2000-0xED2200 (bar rows), plus the string buffer at 0xED1318;
- sets 'APT:PalantirNameFaction' to empty and 'APT:PalantirFavourite' to 'None' via 0x625071.
It does NOT reset [0xED0000], [0xED0004] or [0xED000C].
- **Cadence:** Once per map start, reset or load (event).
- **Reason:** Runs only on logic reset.
- **Risk if wrong:** None.

### AOTR-04 — AotR registry key (section 6 strings)  [n/a, high]
- **Site:** 0x638D49 (byte diff at 0x638D4A) in FUN_00638D22 (GameEngine init path, caller 0x63AD4F), and 0x52CC7E (byte diff at 0x52CC7F) in FUN_0052C9B0 (caller 0x446330 = W3DDisplay vt+0x04 init): `push 0xEA7690` instead of `push 0xBE8368`
- **What:** RegOpenKeyExW(HKLM, L"SOFTWARE\\WOW6432Node\\AotR\\Standalone\\The Battle for Middle-earth II\\lotrbfme2.exe") instead of the stock App Paths key (0xBE8368). The second added string at 0xEA7770 ("...\\The Lord of the Rings, The Rise of the Witch-king") has no reference in the binary.
- **Cadence:** Once at init.
- **Reason:** Init only, no timing.
- **Risk if wrong:** None.

### AOTR-05 — AotR spent-on-army / favourite unit (logic event)  [not_on_B_path, high]
- **Site:** 0x69413A jmp 0xED0300 (replaces `8d 88 dc 03 00 00` lea ecx,[eax+0x3DC]) in FUN_00693D0C (object initialisation; caller 0x6D165E, reached from GameLogic paths including GameLogic::xfer 0x6308F3); helper 0xED1400; returns to 0x694140 (call 0x79F0E1)
- **What:** For a created object: edx = template (esi+4), eax = player. Name filter: template name (template+0x64 string) must not contain 'Port'. Flags: [tmpl+0x113]&4 or !([tmpl+0x108]&0x80). Cost: word [tmpl+0x5EA].
- 0xED1400: per-player template build count (0xECD000, 16 players x 61 slots); updates the favourite and, for the observed player, 'APT:PalantirFavourite' (UTF-16 to UTF-8).
- Adds the cost to per-player spent-on-army (0xECFC00); for the observed player calls 0xED0600 -> 'APT:PalantirSpentOnArmy'.
- Re-executes lea ecx,[eax+0x3DC].
- **Cadence:** Logic event (object creation; also during save-game load).
- **Reason:** Only AotR tables and APT variables are written. Count and cost come from logic.
- **Risk if wrong:** None for timing. Stats are already not saved in stock AotR.

### AOTR-06 — AotR spent-on-army decrement (logic event)  [not_on_B_path, high]
- **Site:** 0x69A760 jmp 0xED0A00 (replaces `b8 e0 8e b8 00` mov eax,0xB88EE0 = SEH prologue of the Object destructor, body FUN_0069A765 which sets Object vtables 0xC123D8..); caller 0x69AB8B; returns to 0x69A765
- **What:** On object destruction, takes the owner player via obj+0x31C -> +0x30 -> +8. It skips the object if 'Port' is in the name, if the KindOf filter fails, or if obj+0x27C -> +0x258 vt+0x114(0) >= 2. Otherwise it subtracts the template cost (word +0x5EA) from the 0xECFC00 entry, clamped at 0, and updates 'APT:PalantirSpentOnArmy' for the observed player. Then it re-executes mov eax,0xB88EE0.
- **Cadence:** Logic event (object deletion).
- **Reason:** Writes only AotR-private table and APT data; called from logic deletion.
- **Risk if wrong:** None.

### AOTR-07 — AotR observer player switch (UI event)  [not_on_B_path, high]
- **Site:** 0x6D40FA and 0x6D410A jmp 0xED0D00 (replace `call 0x6A8D2B`) in APT callbacks 0x6D40F2 'AptPalantir::OnBttnObserveNextPlayer' (registered at 0x6D6DF4) and 0x6D4102 'AptPalantir::OnBttnObservePriorPlayer' (registered at 0x6D6E46); 0xED0D00 ends `popal; ret 4`
- **What:** Calls the stock 0x6A8D2B(dir), which cycles the observed player and sets TheControlBar+0x218. It then rebuilds:
- 'APT:PalantirNameFaction': player name + ' (' + faction name from 0xED1100/0xED1200 + ')', or 'Observer';
- 'APT:PalantirFavourite' from 0xECD000;
- 'APT:PalantirSpentOnArmy' (0xED0600);
- the RPM value via 0xED1D00 (writes [0xED000C], calls 0xED0200).
- **Cadence:** UI event (button click dispatched by the APT player).
- **Reason:** Event-driven, so the render count does not change how often it runs.
- **Risk if wrong:** None.

### AOTR-08 — AotR Palantir callback registration  [n/a, high]
- **Site:** 0x6D7267 jmp 0xED0700 (replaces `8d bb d0 00 00 00` lea edi,[ebx+0xD0]) in AptPalantir init FUN_006D67EA (caller ctor 0x6D73C2); returns to 0x6D726D. Callback body 0xED05D0
- **What:** Registers the extra APT callback 'AptPalantir::OnBttnHideStats' (string at 0xED07DC) -> 0xED05D0, using the same 0xA02F80/0x92B062 pattern as stock. 0xED05D0 toggles the byte at [0xED05FB] ('0'/'1') and calls 0xED0500 (SetStatsStatus, then SetStatsMode).
- **Cadence:** Registration once per Palantir construction; the callback runs on a button event.
- **Reason:** Init plus event only.
- **Risk if wrong:** None.

### AOTR-09 — AotR income counter (logic event)  [not_on_B_path, high]
- **Site:** 0x79DCE0 jmp 0xED1900 (replaces `01 81 14 01 00 00` add [ecx+0x114],eax) in FUN_0079DCCB (ScoreKeeper money-earned, executes only if TheGameLogic+0x98; called from Money::deposit 0x7B18B8); returns to 0x79DCE6
- **What:** Executes the stock add. It then finds the player (ecx-0x3DC) in ThePlayerList 0xDE4928+0x18[i] and adds the deposited amount (saved EAX) to [0xECF824+i*0x40]. This is the cumulative income that the Palantir RPM sampler (AOTR-01) reads.
- **Cadence:** Logic event (money deposit).
- **Reason:** Logic-driven accumulator. The sampler that reads it is the only rate-sensitive part, handled in AOTR-01.
- **Risk if wrong:** None.

### AOTR-10 — AotR income correction for refunds (logic event)  [not_on_B_path, high]
- **Site:** Refund hooks: 0x88D061 jmp 0xED1B00 (replaces `8b 4c 24 10 f7 dd`, FUN_0088CFFE, caller 0x88D1AC); 0x8A11A3 jmp 0xED1C00 (replaces `56 8d 4b e0 e8 46 fb ff ff`, FUN_008A1140, vtable slot 0xC67DC0); 0x8A144D jmp 0xED1A00 (replaces `8d 87 dc 03 00 00`, FUN_008A13EE, vtable slot 0xC67DD8)
- **What:** Around stock Money::deposit refunds (0x7B18B8 with the ScoreKeeper at player+0x3DC), these subtract the refunded amount (ebp or [esi+0x28]) from the player's income counter [0xECF824+i*0x40]. This keeps cancelled production or construction from counting as income. Each then re-executes the displaced instructions.
- **Cadence:** Logic event (production or construction cancel).
- **Reason:** Only the AotR table is written; the trigger is logic.
- **Risk if wrong:** None.

### AOTR-11 — AotR debug leftover (.angmar)  [n/a, medium]
- **Site:** 0x9A3AE0 `e9 1b f5 52 00` jmp 0xED3000 (replaces `51 53 56 8b f1`) in FUN_009A3ADD, entered via thunk 0x9A3AD5 (vtable 0xC88344 slot +0x14; class with hero/army description strings near 0x9A3C58); removed in delayfix.dat
- **What:** 0xED3000 holds data (`f4 be bf 00` = pointer 0xBFBEF4), which executes as HLT, a privileged instruction, so the process would crash. The apparent intended stub at 0xED3008 is `pushal; pushfd; int3; popfd; popal; jmp 0x9A3AE8`, which is itself broken: it does not replay the displaced pushes and jumps mid-instruction. The rest of .angmar is data: the AI unit-category name table at 0xED3012 and side names.
- **Cadence:** Presumably never executed in normal play (it would crash at any FPS).
- **Reason:** Not timing-related and not on any path the design touches.
- **Risk if wrong:** If that vtable slot is ever reached, game.dat crashes regardless of the 60 FPS mod. Do not route any DLL code through it.

### AOTR-12 — AotR faction / AI category tables  [n/a, high]
- **Site:** Data/INI enum changes: 0x73BDD3/0x73BDD7 (FUN_0073BDA3 side-name lookup bound 9->0xE); 0xDA3ADC.. side-name array 0xDA3AC0 extended (Dunland, Guldur, Rhun, Mirkwood, Dale, Rohan, Arnor), overwriting the adjacent stock arrays; AI unit-category parsers 0x8ED4BF/0x8ED544 (0x11->0x15), 0x8ED5D9 (0x11->0x12), 0x8ED516 table pointer 0xDB55C8->0xED3012; 0xDB55C0/0xDB5608 SUPPORT->'SPAM' (0xED3141); 0xC79404.. strings SHIP_SUICIDE->FLYERS, SHIP_TRANSPORT->MONSTER, EXPLORABLE_AREA->SLAYER
- **What:** INI-parse name tables for sides and AI unit categories.
- **Cadence:** INI load and lookups during logic.
- **Reason:** No timing content.
- **Risk if wrong:** None.

### AOTR-13 — AotR render/camera constants  [run_on_B_as_is, high]
- **Site:** 0xBD88BC float 1.3333->1.7778 (readers 0x4451A1 and 0x44520F in W3DDisplay vt+0x5C/0x60); 0xBDD378 float 700->5000 (readers 0x487090/0x48709F in FUN_00486F76, 0x4871D9/0x4871E9 in FUN_0048713E, 0x48C213 in W3DView::update 0x48BCF2, 0x48D1BA/0x48D1CA in FUN_0048D14D)
- **What:** 0xBD88BC: the aspect-ratio constant changed from 4:3 to 16:9. 0xBDD378: the clamp on terrain height under the camera (comiss/movaps min) raised from 700 to 5000.
- **Cadence:** Read per render in W3DView::update and display code, but these are limits, not per-call rates.
- **Reason:** Geometric clamps do not depend on frame rate. 0x48C213..0x48C21A lies inside W3DView::update between the planned camera fix sites (0x48C10F..0x48C14D shake detour, 0x48C36A/0x48C3B0 settle) and does not overlap them.
- **Risk if wrong:** None.

### AOTR-14 — AotR delay-fix variant: stepper semantics  [n/a, high]
- **Site:** delayfix.dat only: 0x632537 (FUN_0063252F isTick: `idiv [0xD9F608]` -> `idiv [0xECA400]`, with [0xECA400]=8); 0x632A9B `b8 02 00 00 00 eb 19` (vt98 0x6329B0 catch-up loop forced on); 0xED081C 0x1E->0x19; 0x6E5A10 jle->jmp (FUN_006E59A9 'ScrollFactor' cap of 100 removed); 0x920B79 jne->jmp (options FUN_009205C4 skips call 0x7154B3); 0x9A3AE0 restored; 0xBDD379, 0xDB55C0/0xDB5608, 0xC79421 and parts of .angmar reverted or reshuffled
- **What:** - vt98(s==1) now runs GameLogic::update(1), then `inc [GameEngine+0x34]` and GameLogic::update(2) in the same frame. A tick therefore takes 5 render frames: [1+2], 3, 4, 5, 6.
- isTick becomes s==2: 30/8=3, then 6/3=2.
- The Palantir sampler fires every 25 calls.
- Together these imply the delay-fix build targets 25 FPS for 5.0 Hz logic (inference: the FPS limit itself is INI data, not in the exe).
- **Cadence:** Stepper, every frame.
- **Reason:** Incompatible with C1 as designed:
- SubStub calling vt98(1) on s=1 would run sub 1 and sub 2 and bump the 12-step counter in +0x34 to 2.
- The next A-frame (s=3) calls vt98(2), so sub 2 runs twice per tick and logic diverges.
- FUN_0063252F would report isTick on s==2, which is a B-frame under 12 sub-frames.
- The 16/17 ms limiter math also assumes 6 frames per tick.
- **Risk if wrong:** Duplicated logic sub-steps (desync, saves no longer bit-identical), wrong interpolation shift, and a different game speed on delayfix installs.
- **Fix @ 0x632A9B:** Detection only: if the delayfix bytes are present (or 0x632537 reads `00 a4 ec 00`), force g_m60=0. Alternatively, build a separate schedule for this variant; that is not designed yet. (orig: game.dat: f7 3d 08 f6 d9 00 8b; delayfix: b8 02 00 00 00 eb 19)

### AOTR-15 — Overlap and interaction check  [n/a, high]
- **Site:** All 22 planned patch sites: 0x6325CF, 0x632606(0x632604), 0x632642, 0x63264C(0x63264A), 0x6326BE, 0x6326DF, 0x6326E4, 0x6326EB, 0x63A1AD, 0x63A1F8, 0x44B911, 0x6765D5, 0x67173B, 0x671774, 0x67BDC0, 0x44B98E, 0x44B9C3, 0x449D40, 0x444CF2, 0x449D55, 0x6A23D7, 0x83B8A0 (plus 0x632631)
- **What:** Bytes at every site are identical in game.dat, stock game820.dat and delayfix.dat, and match the synthesis originals. Examples: 0x6325CF `ff 90 9c 00 00 00`, 0x44B911 `a1 8c 7a dc 00 0f af c6 01 05 80 75 dc 00`, 0x444CF2 `8b 0d 44 37 de 00 8b 01 ff 60 28`, 0x6A23D7 `8b 0d 70 4a de 00 8b 01 ff 50 28`, 0x83B8A0 `8d 55 f0 52 ff 50 5c`. No AotR change lies inside or adjacent to any site; the nearest AotR code bytes are 0x629D11 and 0x6D57AF.

Interactions (not overlaps):
1. The 0x6A23D7 gate also gates AotR's Palantir hook (AOTR-01).
2. 0x444CF2 is in 0x444CD4 (W3DDisplay vt+0x188), which first calls TheLivingWorldManager->update (0x444CDC). That leads to 0x61226F, the Palantir update and the AotR hook, which the C7 change does not gate.
3. In delayfix.dat the stepper sites interact with the AotR changes at 0x632537 and 0x632A9B (AOTR-14).
- **Cadence:** n/a
- **Reason:** Verified by byte comparison of the three binaries.
- **Risk if wrong:** n/a

## Open questions

- W3DDisplay vt+0x188 = 0x444CD4 calls TheLivingWorldManager->update and then the FX-particle tail-jump at 0x444CF2. I found no static caller. Who calls it, and does it run in normal skirmish or campaign battles? If it does, the Palantir update (and the AotR sampler) already runs twice per render in stock. A callee gate at 0x5039C1 preserves that per-A-render count; caller gates must cover both paths. Verify with M5 call counters on 0x6D577C, 0x6121C5 and 0x444CD4.
- Is the Palantir active (FUN_006D4609 true) on the Living World strategic map? This decides whether the 0x61226F path actually reaches the AotR hook. Also, is g_m60 meant to be on in LW map mode at all? Living World mode is still an overall coverage gap.
- The APT side (Palantir .apt/ActionScript in AotR's .big files) is not in the exe. Does it scale the raw 6-sample income delta to per-minute, which fixes the exact displayed magnitude? And are SetStatsStatus/SetStatsMode/SetCammyBar idempotent, or does each call restart a tween? Gating to stock cadence makes this moot, but it matters if anyone considers running the Palantir update per render.
- Which FramesPerSecondLimit does AotR's delay-fix distribution ship? The 0x19 sampler and 5-frame tick suggest 25. Should the 60 FPS mod support delayfix installs or refuse them?
- Is 0x9A3ADD (vtable 0xC88344 slot +0x14, via thunk 0x9A3AD5) ever reached in game.dat? If yes it crashes (HLT at 0xED3000) regardless of FPS. This is only relevant for crash triage while testing the mod.
- AotR's 0x69413A hook also fires during save-game load (GameLogic::xfer 0x6308F3 -> 0x6D165E -> 0x693D0C), inflating 'spent on army' after load. This is stock AotR behaviour, unaffected by 60 FPS; listed only so testers do not attribute it to the mod.
