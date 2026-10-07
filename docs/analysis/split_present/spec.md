# decision
DECISION: implement (C) in its hybrid form, (H): checkpoint presentation armed by a timer thread. (T) is NO-GO for this iteration.

What the user's request gets. A second thread is used, but only as a precise timer. At the B target time it sets a one-byte "due" flag. The main thread then presents the already-rendered B from the next cheap checkpoint inside the logic step. All D3D calls stay on the main thread, under the game's own DX mutex.

Why not (T), safety first:
1. (T) needs D3DCREATE_MULTITHREADED.
   - The main thread makes D3D calls while the helper would be inside Present:
     - End_Scene tail Releases, 0x5226AF..0x5227C5;
     - End_Render tail: 16x SetTexture plus Releases, 0x51F690 -> 0x51C600;
     - logic CreateVertexBuffer 0x539290 and EvictManagedResources, on a CONFIRMED static path from 0x62E4E8;
     - resource and D3DX calls that the [0xDD3474]-based count misses.
   - The flag is fixed per device at 0x524112/0x5241B6/0x524222. 30 mode would then also run on an MT device, so it is no longer byte-exact.
   - Its per-call cost is unmeasured.
2. DXVK 2.6.2 holds the device lock for the whole Present, including acquireNextImage and SyncFrameLatency (d3d9_swapchain.cpp:112/911, dxvk_presenter.cpp:77-134). Main-thread D3D calls during logic would spin for the Present duration, which is unmeasured (presentMax was never logged in a heavy session). The sim sweep shows 5 ms of contention per overlap halves the gain and 10 ms erases it (mt/verify_sim/out_chk3.txt).
3. There is an unbreakable deadlock class. Swapchain re-creation would run on the helper (OUT_OF_DATE on alt-tab or mode change) while DXVK's WM_ACTIVATEAPP hook takes the device lock on the window thread (d3d9_device.cpp:8715). A message-aware join cannot break it, because the main thread is spinning, not waiting.
4. A message pump is reachable inside GameLogic::update (0x84EEBA -> 0x84EEFA, 0x98B12C -> 0x98B16C). Native d3d9 in exclusive fullscreen may send messages from Present.

Why (H) is enough, on benefit:
- C with checkpoint spacing g <= 4 ms and the 'wait' end rule is within about +/-1 ms of T in every load (out_cgran.txt).
- My rerun (mt/lead/out_lead.txt) gives C2w+E+M2 (checkpoints every 2 ms, early release, proportional repay) as equal to Tm-med3+E+M2 (presenter thread, same rules):
  - 'ai' IMMEDIATE p99 P0..P4: 21.1/26.5/31.2/33.0/37.3 vs 21.0/26.2/30.7/32.6/37.3.
- With early release only about 1-6 B/s land inside logic (out_chk4.txt). The rest are presented by the main thread in the end drain anyway.
- The timer thread fixes the refuted slow-path overhead. The fast path is a 1-byte compare (about 1 ns), and the slow path runs only once B is due. There is no 45 ns QPC call per hit, and no 0.9 ms of sub-2 overhead.

Staging, each behind its own ini switch:
- S0: M2 proportional repay in AotR60_Pacer. Independent of threading. It is the actual speed fix: old -3.20 % -> -0.08 % at P4. INFERRED from the sim.
- S1: split-present core. Handoff, 8 checkpoints, timer thread, drains and nets, predictor.
- S2: predicted early release E of the Y iteration. It requires S1. It recovers the P0 regression of S1 alone and gives most of the p2p gain.

Profile and stress modes ship with S1. A midpoint threshold was simulated and rejected: C2w-thr2 is no better (out_lead.txt).

When to reopen (T): only if the profile session shows unsplittable logic gaps over 6 ms that dominate, AND a measured in-logic Present plus lock-hold time under 1 ms, AND a debug device/resource vtable shim shows zero main-thread D3D calls during the overlap window.

Files (lead area, <analysis workspace>/hitch\mt\lead\):
- sites_sp.json: the full sites.json-ready entries, with evidence, dll_vars, branch checks and risks. sites_sp_min.json is the condensed copy in sites_json.
- gen_sites.py
- mtsim_lead.py, run_lead.py, out_lead.txt

All 13 new spans had their original bytes re-read from a copy of game.dat and match (CONFIRMED). All 13 were checked against the sync and checkpoint verify passes, and their boundaries were confirmed with ./dis.sh.

# sites_json
[{"id":"SP_CP_MOD","phase":"sp","group":"split-present/checkpoint","address":"0x62EA97","length":7,"original_hex":"8d 4b 10 8b 01 ff 10","kind":"call_gate","replacement_hex":"e8 <rel32:CP_MOD_STUB> 90 90","resume_address":"0x62EA9C (nop nop) -> 0x62EA9E","stub":"CP_MOD_STUB: cmp byte ptr [g_cpDue],0 / jne slow / go: lea ecx,[ebx+10h] / mov eax,[ecx] / jmp dword ptr [eax] (callee returns to 0x62EA9C = 90 90, then 0x62EA9E). slow: CP_SLOW IDX_CP_MOD = SAVE_ALL; fxsave [g_cpFx] (static, 16-aligned, 512 B, main thread only); push IDX_CP_MOD; call AotR60_Checkpoint (cdecl); add esp,4; fxrstor [g_cpFx]; RESTORE_ALL; jmp go.","live_after":"Entry: EBX=sleepy entry, ESI=GL, EDI=bucket vector, EBP frame; EAX/ECX written by the span, EDX/EFLAGS dead, x87 empty (call boundary). After: EAX=callee result (cmp eax,1 at 0x62EA9E), EBX/ESI/EDI/EBP preserved. Only difference to stock: the return slot holds 0x62EA9C instead of 0x62EA9E (VTGATE precedent).","purpose":"Checkpoint before every awake module update: subs 3/4 (bucket 0 halves, AIUpdate) and sub 5 (buckets 1-2). Dominant checkpoint; gap = one module update (INFERRED <0.05 ms typical, <=1 ms worst).","branch_into_span_check":"verify_cp.py (mt/checkpoints + re-run mt/checkpoints_verify): linear+function-local capstone sweep, brute rel8/rel32/jcc32, abs32 scan, overlap vs sites.json main/wip, lw_final_sites.json, AotR hooks: 0 hits; 0x62EA97 is target of je 0x62EA8C (span start). Re-run tools/verify_sites.py."},{"id":"SP_CP_PATH","phase":"sp","group":"split-present/checkpoint","address":"0x6EC0D1","length":7,"original_hex":"56 8d 91 00 08 00 00","kind":"func_detour","replacement_hex":"e9 <rel32:CP_PATH_CAVE> 90 90","resume_address":"0x6EC0D8","stub":"CP_PATH_CAVE: cmp byte ptr [g_cpDue],0 / jne slow / go: push esi / lea edx,[ecx+800h] / jmp dword ptr [T_6EC0D8]. slow: CP_SLOW IDX_CP_PATH (as CP_MOD) then jmp go (slow path runs before push esi: stack as at entry).","live_after":"Entry: ECX=queue, [esp]=return to 0x6F24B3/0x6F2509, EAX/EDX dead (written 0x6EC0D2/0x6EC0D8), EFLAGS dead (function entry), x87 empty. At 0x6EC0D8: ESI pushed, EDX=ECX+0x800 as stock.","purpose":"Checkpoint before each path search (pathfinder 0x6F2364 at the start of every sub via 0x62E69F and again in sub 5 via TheAI 0x6FEC66). Gap = one A* search (INFERRED 1-4 ms, unsplittable).","branch_into_span_check":"only refs are call 0x6EC0D1 at 0x6F24AE/0x6F2504 (span start); interior 0 hits (verify_cp.py)."},{"id":"SP_CP_PLAYER","phase":"sp","group":"split-present/checkpoint","address":"0x6A84E5","length":5,"original_hex":"e8 7f 6d 00 00","kind":"call_gate","replacement_hex":"e8 <rel32:CP_PLAYER_STUB>","resume_address":"0x6A84EA","stub":"CP_PLAYER_STUB: cmp byte ptr [g_cpDue],0 / jne slow / go: jmp dword ptr [T_6AF269] (tail jump: callee sees return address 0x6A84EA, identical to stock). slow: CP_SLOW IDX_CP_PLAYER; jmp go.","live_after":"Entry: ECX=Player* (0x6A84E3), ESI=slot ptr, EDI=count, EFLAGS dead (dec edi/jne at 0x6A84ED), x87 empty. Return address identical to stock.","purpose":"Sub-5 checkpoint before each of the 20 Player::update calls (AIPlayer::update 0x8F9705 incl. 5-tick 0x8F8F68 and 10-tick 0x8F3EFE).","branch_into_span_check":"loop target 0x6A84E3 outside span; interior 0 hits (verify_cp.py)."},{"id":"SP_CP_SKAI","phase":"sp","group":"split-present/checkpoint","address":"0x6A96F3","length":5,"original_hex":"e8 fe 46 24 00","kind":"call_gate","replacement_hex":"e8 <rel32:CP_SKAI_STUB>","resume_address":"0x6A96F8","stub":"CP_SKAI_STUB: cmp byte ptr [g_cpDue],0 / jne slow / go: jmp dword ptr [T_8EDDF6] (return address 0x6A96F8 identical to stock). slow: CP_SLOW IDX_CP_SKAI; jmp go.","live_after":"Entry: ECX=entry (0x6A96F1), EDI=iterator, ESI=&mgr+0xA4C, EFLAGS dead (add edi,4/cmp edi,[esi] after), x87 empty.","purpose":"Sub-5 checkpoint per SkirmishAIManager entry (splits the manager tail).","branch_into_span_check":"loop target 0x6A96F1 outside span; interior 0 hits (verify_cp.py)."},{"id":"SP_CP_SCRIPT","phase":"sp","group":"split-present/checkpoint","address":"0x60A3BA","length":5,"original_hex":"e8 9d fd ff ff","kind":"call_gate","replacement_hex":"e8 <rel32:CP_SCRIPT_STUB>","resume_address":"0x60A3BF","stub":"CP_SCRIPT_STUB: cmp byte ptr [g_cpDue],0 / jne slow / go: jmp dword ptr [T_60A15C] (return address 0x60A3BF and the two pushed args identical to stock). slow: CP_SLOW IDX_CP_SCRIPT; jmp go.","live_after":"Entry: ECX=ScriptEngine, 2 args pushed (0x60A3B6/0x60A3B7), ESI=list node, EDI=context, EBX=engine, EFLAGS dead, x87 empty.","purpose":"Sub-1 checkpoint before each script evaluation (list walker 0x60A377). Reachable outside GameLogic::update too: AotR60_Checkpoint ignores hits unless g_inLogic.","branch_into_span_check":"no branch to 0x60A3BA; interior 0 hits (verify_cp.py)."},{"id":"SP_CP_OBJ1","phase":"sp","group":"split-present/checkpoint","address":"0x62E912","length":5,"original_hex":"e8 fc f6 0d 00","kind":"call_gate","replacement_hex":"e8 <rel32:CP_OBJ1_STUB>","resume_address":"0x62E917","stub":"CP_OBJ1_STUB: cmp byte ptr [g_cpDue],0 / jne slow / go: jmp dword ptr [T_70E013] (return address 0x62E917 identical to stock). slow: CP_SLOW IDX_CP_OBJ1; jmp go.","live_after":"Entry: ECX=Object (0x62E910), EBX=object, ESI=GL, EFLAGS dead, x87 empty. After: EAX=getter result (test eax,eax 0x62E917).","purpose":"Sub-1 checkpoint per object in the first object loop (bounds the Lua/post-script segment).","branch_into_span_check":"loop target 0x62E910 outside span; interior 0 hits (verify_cp.py)."},{"id":"SP_CP_PART","phase":"sp","group":"split-present/checkpoint","address":"0xA3B564","length":6,"original_hex":"8b b7 20 01 00 00","kind":"call_gate","replacement_hex":"e8 <rel32:CP_PART_STUB> 90","resume_address":"0xA3B569 (nop) -> 0xA3B56A","stub":"CP_PART_STUB: cmp byte ptr [g_cpDue],0 / jne slow / go: mov esi,[edi+120h] / ret (returns to 0xA3B569 nop -> 0xA3B56A test esi,esi). slow: CP_SLOW IDX_CP_PART; jmp go.","live_after":"Entry: EDI=PartitionManager, EAX/ECX/EDX dead, EFLAGS dead (test esi,esi follows), x87 empty (no x87 in function). After: ESI=[EDI+0x120] as stock.","purpose":"Sub-2 checkpoint per object re-celled by the partition manager (vtable 0xC95180 vt+0x28 -> 0xA39020 -> 0xA3B4E0, CONFIRMED).","branch_into_span_check":"0xA3B564 is target of je 0xA3B552 (span start); interior 0 hits (verify_cp.py)."},{"id":"SP_CP_COLL","phase":"sp","group":"split-present/checkpoint","address":"0xB6D11E","length":5,"original_hex":"8b 4e 04 8b 16","kind":"call_gate","replacement_hex":"e8 <rel32:CP_COLL_STUB>","resume_address":"0xB6D123","stub":"CP_COLL_STUB: cmp byte ptr [g_cpDue],0 / jne slow / go: mov ecx,[esi+4] / mov edx,[esi] / ret (returns to 0xB6D123). slow: CP_SLOW IDX_CP_COLL; jmp go.","live_after":"Entry: ESI=pair, EBX=CollisionManager, EAX/EBP/EDI dead (rewritten 0xB6D12E/0xB6D129/0xB6D12B), EFLAGS dead (next reader test ecx,ecx 0xB6D146 after calls), x87 empty (temporaries 0xB6D15C..0xB6D17C popped). After: ECX=[esi+4], EDX=[esi] as stock.","purpose":"Sub-2 checkpoint per active collision pair (vtable 0xD0BF08 vt+0x28 -> 0xB6C420 -> 0xB6E6B0 -> 0xB6D060, CONFIRMED). Highest hit count (INFERRED up to ~15k/tick).","branch_into_span_check":"fall-through of je 0xB6D118, not a branch target; interior 0 hits (verify_cp.py)."},{"id":"SP_STEP_DRAIN","phase":"sp","group":"split-present/drain","address":"0x441822","length":5,"original_hex":"e8 79 0d 1f 00","kind":"call_gate","replacement_hex":"e8 <rel32:STEP_DRAIN_STUB>","resume_address":"0x441827","stub":"STEP_DRAIN_STUB: call dword ptr [T_6325A0] (ECX=ESI=engine unchanged) / cmp dword ptr [g_ppState],0 / jne drain / ret / drain: SAVE_ALL; push SP_DRAIN_STEP; call AotR60_SpDrain (cdecl, main thread: wait until min(target, next pacer deadline) then present; cancel if iconic/not foreground); add esp,4; RESTORE_ALL; ret. 30 mode: g_ppState==0 -> call + cmp + ret.","live_after":"At 0x441827: ESI=engine, EBX/EDI/EBP as stock, EAX overwritten (mov eax,[0xDC3C64]), ECX/EDX dead, EFLAGS dead (test eax,eax), x87 empty. GameEngine::update runs with its frame 4 bytes lower than stock (VTGATE precedent).","purpose":"Primary end drain: the deferred B is presented (or cancelled) before the IsIconic loop 0x441827, the pump (tail jmp 0x4418C2 -> 0x441A7D), the pacer 0x63A196, the streamer 0x6325B0 and the next C0. A pending Present never outlives one GameEngine::update call.","branch_into_span_check":"0x441823..0x441826: listing operand scan 0 hits; raw sweep only false positive 0x5FB6C8->0x441824 (inside call 0xa3cef0 at 0x5FB6C7); entry 0x44181F only via vtable slot 0xBD8508; no overlap with RESET 0x44181A..0x44181E."},{"id":"SP_PUMP_NET","phase":"sp","group":"split-present/drain","address":"0x441A7D","length":5,"original_hex":"b8 56 11 b7 00","kind":"func_detour","replacement_hex":"e9 <rel32:PUMP_NET_CAVE>","resume_address":"0x441A82","stub":"PUMP_NET_CAVE: cmp dword ptr [g_ppState],0 / jne net / stock: mov eax,0B71156h / jmp dword ptr [T_441A82] / net: SAVE_ALL; push SP_NET_PUMP; call AotR60_SpNet (main thread: present now unless iconic/not foreground -> cancel; other thread: cancel); add esp,4; RESTORE_ALL; jmp stock.","live_after":"At 0x441A82: EAX=0xB71156 (consumed by SEH helper 0xA3CEF0), other GPRs/ESP as at entry, EFLAGS dead, x87 empty.","purpose":"Safety net: serviceWindowsOS is reachable inside GameLogic::update (0x84EEBA -> 0x84EEFA; 0x98B12C -> 0x98B16C). No window message is dispatched while a deferred B is pending.","branch_into_span_check":"entered via vtable 0xBD853C and tail jmp 0x4418C2; interior only the false-positive pointer 0x5FB46D (sync_verify)."},{"id":"SP_TCL_NET","phase":"sp","group":"split-present/drain","address":"0x516C40","length":5,"original_hex":"a1 74 34 dd 00","kind":"func_detour","replacement_hex":"e9 <rel32:TCL_NET_CAVE>","resume_address":"0x516C45","stub":"TCL_NET_CAVE: cmp dword ptr [g_ppState],0 / jne net / stock: mov eax,ds:[0DD3474h] / jmp dword ptr [T_516C45] / net: SAVE_ALL; push SP_NET_TCL; call AotR60_SpNet; add esp,4; RESTORE_ALL; jmp stock.","live_after":"At 0x516C45: EAX=device pointer, other GPRs/ESP as at entry, EFLAGS dead (sub esp,0xC0), x87 empty.","purpose":"Safety net: B presented before TestCooperativeLevel/new frame on every drawFrame path (second drawFrame in the same render; load-screen draw cancels).","branch_into_span_check":"callers 0x447645/0x449D17 (span start); raw-sweep hit 0x516C3D->0x516C41 is inside jmp 0x53e660 at 0x516C3B (false positive)."},{"id":"SP_BEGIN_RENDER_NET","phase":"sp","group":"split-present/drain","address":"0x517B30","length":10,"original_hex":"83 ec 28 80 3d 14 1e dd 00 00","kind":"func_detour","replacement_hex":"e9 <rel32:BEGIN_RENDER_NET_CAVE> 90 90 90 90 90","resume_address":"0x517B3A","stub":"BEGIN_RENDER_NET_CAVE: cmp dword ptr [g_ppState],0 / jne net / stock: sub esp,28h / cmp byte ptr ds:[0DD1E14h],0 / jmp dword ptr [T_517B3A] / net: SAVE_ALL; push SP_NET_BEGIN_RENDER; call AotR60_SpNet; add esp,4; RESTORE_ALL; jmp stock (displaced cmp is the last flag writer).","live_after":"At 0x517B3A: ZF from the displaced cmp (je 0x517C50), ESP=entry-0x28, all GPRs as at entry, x87 empty.","purpose":"Safety net: Present before any Clear/BeginScene on every Begin_Render path (0x449EA5, RTT 0x47D6F6/0x47D781/0x47F218, 0x4437B3, 0x44765F).","branch_into_span_check":"direct callers all target the span start; 0x517B31..0x517B39 0 hits."},{"id":"SP_SHUTDOWN","phase":"sp","group":"split-present/drain","address":"0x517AA0","length":7,"original_hex":"6a ff 68 b8 8d b7 00","kind":"func_detour","replacement_hex":"e9 <rel32:SHUTDOWN_CAVE> 90 90","resume_address":"0x517AA7","stub":"SHUTDOWN_CAVE: SAVE_ALL; call AotR60_SpShutdown (cancel pending, g_spDisabled=1, signal timer thread to exit, never wait); RESTORE_ALL; push -1; push 0B78DB8h; jmp dword ptr [T_517AA7].","live_after":"At 0x517AA7: ECX=param_1 (fastcall), other GPRs as at entry, two pushes on the stack, EFLAGS dead, x87 empty.","purpose":"No deferred Present after DX8Wrapper::Shutdown 0x5257A0 releases the device (0x52121B Release, 0x52121F clears [0xDD3474]); stops the timer thread.","branch_into_span_check":"callers 0x402D27, 0x4466B4, 0x4499F5 (span start); interior 0 hits."},{"id":"PRESENT (modified stub, same bytes)","phase":"2a","group":"pacing/present","address":"0x522644","length":15,"original_hex":"a1 74 34 dd 00 8b 10 53 53 53 53 50 ff 52 44","kind":"call_gate","replacement_hex":"e8 <rel32:PRESENT_STUB> 90 90 90 90 90 90 90 90 90 90","resume_address":"0x522653","stub":"PRESENT_STUB: cmp byte ptr [g_m60],0 / je present / call AotR60_PresentSkip (now 0=present, 1=skip, 2=defer) / test eax,eax / jz present / cmp eax,2 / je defer / SKIPCNT IDX_PRESENT / xor eax,eax / ret / defer: DEFERCNT IDX_PRESENT (new g_siteDefer) / xor eax,eax (S_OK -> stock success path [0xDD34C4]++, [0xDD1F38]=0, stats-only) / ret / present: unchanged.","live_after":"unchanged (EAX=HRESULT, EBX=0).","purpose":"Handoff: a gated 60-mode B Present becomes PENDING and returns S_OK.","branch_into_span_check":"unchanged"},{"id":"GAP_RESET (modified stub, same bytes)","phase":"2a","group":"pacing/present","address":"0x522000","length":6,"original_hex":"64 a1 00 00 00 00","kind":"func_detour","replacement_hex":"e9 <rel32:GAP_RESET_CAVE> 90","resume_address":"0x522006","stub":"GAP_RESET_CAVE: mov byte ptr [g_gapReset],1 / cmp dword ptr [g_ppState],0 / je stock / SAVE_ALL; push SP_CANCEL_RESET; call AotR60_SpCancel; add esp,4; RESTORE_ALL / stock: mov eax,fs:[0] / jmp dword ptr [T_522006]","live_after":"unchanged (EAX=fs:[0], EFLAGS dead).","purpose":"Reset_Device (any caller or thread) drops a pending B: no Present on a reset swapchain.","branch_into_span_check":"unchanged"}]

# design
Everything below is new code unless it names an existing file. Proposed files: src/split_present.cpp/.h. Edited files: pacer.cpp, frame_ctl.cpp, telemetry.cpp, config.*, stubs.asm, installer.cpp and sites.json.

## A. Config, installation, hotkey

1. Ini keys. All four are added to kDefaultConfigText (config.cpp:6).
   - `SplitPresent = 0|1|2|3`
     - 0: off. The new sites are not installed.
     - 1: on.
     - 2: profile. Sites installed, nothing deferred, checkpoint profiling.
     - 3: stress. Every gated B is due immediately, which forces a present at the first checkpoint. Use it for determinism tests only.
   - `SplitPresentEarly = 0|1`: stage S2, early release.
   - `RepayProportional = 0|1`: stage S0, M2.
   - `SplitPresentNative = 0|1`: allow the feature on native d3d9. Default 0.

2. Installation.
   - Give the new sites `"phase":"sp"`. installer.cpp:47 already skips Phase4b, so add the same filter: skip Phase::Sp unless cfg.splitPresent != 0.
   - With SplitPresent=0 the patched image is byte-identical to today's build. The PRESENT and GAP_RESET stub changes are DLL-side only, with the same bytes. In 30 mode they add one cmp each.

3. Hotkey Ctrl+Shift+F9, in PollHotkeys (frame_ctl.cpp:342; F10 and F11 are taken).
   - It toggles g_spUser and logs "hotkey: split present on/off".
   - It is applied at C0, where the state is always IDLE.
   - Add ", split" to the title tag (frame_ctl.cpp:297).
   - Every telemetry window carries the sp flag, so A/B segments can be separated.

4. g_spActive = installed && cfg==1|3 && g_spUser && !g_spDisabled && wrapperOk && timerThreadOk.
   - wrapperOk is checked at the first C0. GetModuleHandleW(L"d3d9.dll") must be loaded from the game exe's directory (DXVK), or SplitPresentNative=1 must be set. Log either outcome.

## B. Globals (runtime.h/.cpp)

- `volatile LONG g_ppState`: 0 IDLE, 1 PENDING, 2 PRESENTING. Changed only by InterlockedCompareExchange.
- `volatile uint8_t g_cpDue`: the only byte the checkpoint fast path reads.
- `volatile uint8_t g_inLogic`: set by LogicUpdateWrapper on the main thread.
- `uint8_t g_spDisabled`: sticky for the session.
- `uint8_t g_iterWasA`: set at C0 to g_fs.m60 && !g_fs.inB.
- `int g_c0Sub`: s at C0.
- `alignas(16) uint8_t g_cpFx[512]` and `g_spFx[512]`.
- The pending job, main-thread only:

  ```
  struct PendingB { uint32_t gen; int64_t tHandoff, target; bool paced; int sNext; uint32_t fNext; } g_pb;
  ```

- Timer-thread handoff: `volatile int64_t g_armTarget; volatile LONG g_armGen; HANDLE g_spArm` (auto-reset event); `volatile LONG g_spQuit`.
- Pacer additions: `int64_t g_borrow`, `g_pairTicks`, `g_lastA` (QPC of the last A Present call).
- Predictor tables:
  - `int64_t hist[10][7][3]; uint8_t n[10][7];`
  - aPre[7]: release to A PresentSkip entry, EWMA 1/8, keyed by g_c0Sub.
  - bEst: release to B handoff, EWMA 1/8.
  - tailEst: logic end to pacer entry minus drain wait, EWMA 1/8.

## C. Timer thread (the only extra thread; it never touches D3D, game memory or locks)

1. Creation.
   - Create it lazily on the main thread at the first C0 where SplitPresent is 1 or 3: CreateEventW (auto-reset) plus CreateThread.
   - SetThreadPriority(THREAD_PRIORITY_HIGHEST). Set an optional description.
   - Its own CreateWaitableTimerExW(CREATE_WAITABLE_TIMER_HIGH_RESOLUTION), falling back to a normal timer.
   - If any step fails, set g_spDisabled and log. Never create it in DllMain.

2. Loop.

   ```
   for(;;){ WaitForSingleObject(g_spArm, INFINITE); if(g_spQuit) return;
     gen=g_armGen; tgt=g_armTarget;
     while(QPC < tgt-0.7ms){ if(g_armGen!=gen||g_ppState!=1) goto next; HR timer slice <=2 ms (same pattern as pacer.cpp WaitUntil) }
     while(QPC < tgt){ if(g_armGen!=gen||g_ppState!=1) goto next; YieldProcessor(); }
     if(g_armGen==gen && g_ppState==1){ g_cpDue=1; record wake lateness QPC-tgt (Interlocked max/sum) }
   next:; }
   ```

   Spinning costs at most 0.7 ms per deferral, about 30/s with S2.

3. A stale due flag (helper sets it for an old generation after the main thread consumed the job) is harmless: the slow path sees state != PENDING, clears g_cpDue and returns.

4. Exit.
   - SHUTDOWN_CAVE sets g_spQuit and SetEvent, without waiting.
   - DLL_PROCESS_DETACH does nothing. The thread holds no locks, so ExitProcess killing it is safe.

## D. Handoff (AotR60_PresentSkip, pacer.cpp:298; return 0 present, 1 skip, 2 defer)

The order for a B-render:
1. Off the main thread: if g_ppState != 0, SpCancel(OFF_MAIN). Then return 0, as today.
2. Existing M>1 forced skip, which returns 1.
3. paced and relPresent statistics (unchanged; they measure the handoff, i.e. render time). bEst is updated with now - g_releaseTime.
4. Existing FIFO late-skip, which returns 1.
5. If g_spActive and every gate below holds, compute the target. If target > now + 0.3 ms, defer:
   - g_pb = {++gen, now, target, paced, sNext, fNext}
   - CAS 0 -> 1, g_cpDue = 0
   - g_armTarget = target; InterlockedExchange(&g_armGen, gen); SetEvent(g_spArm)
   - RecordPresentSpacing is NOT called yet; g_presentCall is not set
   - ++deferred; return 2 (no spacing wait: logic starts at once)
6. Otherwise fall through to the existing spacing wait and present.
   - Under S2, the cap `latest` becomes g_releaseTime + g_borrowUsed + half*6/10, so a B released early but not deferred is not shown early.

Gates (all must hold; each failure is counted by reason):
- paced
- g_ppState == 0
- GL=[0xDE412C] != null
- GL+0x110 in {0,2,6} (battle, not the LW map 8)
- [GL+0x124] == 0 (not paused; also excludes the paused redraw loop 0x44BC13..0x44BC20)
- [0xDC7568] <= 0 and [[0xDE4364]+0xEA0] == 0: no frame save or tile capture reading the back buffer after Present (0x44BC28..0x44BC54, 0x44BAB9, CONFIRMED)
- hwnd=[0xDD3014]: GetForegroundWindow()==hwnd && !IsIconic(hwnd)
- IMMEDIATE ([0xDD302C]==0x80000000) or g_presentsSinceBlock >= 2 (FIFO queue has room, so a Present inside logic does not block; on a 60 Hz display this keeps the feature effectively off)
- now >= g_spCooldownUntil. The cooldown is 2 s after any gap, DEVICELOST, Reset, mode change or anomaly cancel.
- [GL+0x125] == 0

Target rule (all values QPC):
- tNormal: if g_owed==0 && g_lastPresent, then g_lastPresent + half - f/4000; else now. Without S2 it is also capped at g_releaseTime + half*6/10, which is the old rule (pacer.cpp:339-343).
- Prediction of the next logic call, read from game state, not a call counter:
  - s=[[0xDE4324]+0x34], F=[GL+0x40]
  - s<=5 gives (sub s+1, frame F)
  - s==6 gives (sub 1, F+1)
  - s>=7: (sub 1, F+1) if not paused (paused is already excluded by the gate)
  - Slot = (frame % 10, sub).
- L-hat = median of the 3 stored samples (min of 2 if only 2, the value if 1). If the slot is empty, target = tNormal (the plain T0 effect).
- endPred = now + L-hat + tailEst
- dlNext = g_deadline + (g_pairTicks - g_interval) + g_borrow - expPay
  - expPay = min(g_owed, M2 ? max(I/8, owed/6) : I/8), where I = g_pairTicks - g_interval
- A-hat_next = max(endPred, dlNext) + aPre[sNext], with 9 ms as the fallback for aPre.
- tMid = (g_lastA + A-hat_next)/2
- target = min(max(tNormal, tMid), now + 60 ms)
- No bias. The simulated midpoint-threshold variant is not used (out_lead.txt).

## E. Presentation paths (all on the main thread; only cancels may run elsewhere)

### SpPresentNow(reason)

1. CAS 1 -> 2, else return. Then g_cpDue = 0.
2. Mutex.
   - Checkpoint reasons: if [0xDD34C8]==GetCurrentThreadId() && [0xDD34CC]>0 (main thread inside a bracketed D3D sequence), revert to state 1, set g_cpDue = 1 so the next checkpoint retries, ++mutexOwnedSkips, return.
   - Then try-lock `bool(__cdecl*)(DWORD)0x51EF50(0)`. CONFIRMED: WFSO on [0xDD1FD8]; returns al=0 only on WAIT_TIMEOUT; on success sets owner/count under CS 0xDD1F80. On failure revert as above.
   - Drains and nets: 0x51EF50(2) in a loop up to 100 ms total. On failure SpCancel(MUTEX_TIMEOUT), set g_spDisabled, and log [0xDD34C8].
3. dev = [0xDD3474]. If null, cancel.
4. _fxsave(g_spFx); gle = GetLastError(); seed0 = [0xDA1CA4]; read CW/MXCSR.
5. tCall = QPC; hr = dev->vt[0x44](dev,0,0,0,0); tRet = QPC.
6. Restore and check:
   - compare CW/MXCSR (fpChanged++);
   - _fxrstor;
   - SetLastError(gle);
   - [0xDA1CA4] != seed0 -> seedChanged++ and g_spDisabled = 1.
7. Unlock with `void(*)()0x5208D0()` (CONFIRMED: releases via ReleaseMutex at 0x520920).
8. Pacer bookkeeping, as AotR60_PresentDone does today:
   - RecordPresentSpacing(tCall, g_pb.paced); g_lastPresent = tCall
   - presentCallTicks += tRet - tCall
   - took > f/1000 ? g_presentsSinceBlock = 0 : ++
9. Results.
   - hr == D3DERR_DEVICELOST (0x88760868): g_gapDevLost = 1 and cooldown. Stock recovery runs on the main thread at the next A Present (End_Scene 0x522674 TCL/Reset/Sleep) or at 0x516C40. This is the same "one render late" property the skip path already has.
   - Other hr < 0: count, and log the first 5.
10. If g_inLogic, g_cpPresentInCall += tRet - tCall.
11. Record lateness tCall - target. Set state 0.

### Who calls it

- **AotR60_Checkpoint(idx)**, via the CP_SLOW asm in the 8 sites, which already did SAVE_ALL plus fxsave.
  - Return (clearing g_cpDue if state != 1) unless OnMainThread() && g_inLogic && g_ppState == 1.
  - Then SpPresentNow(CP_BASE + idx).
  - In profile mode (SplitPresent=2): only record (site, sub, QPC) and return; see Telemetry.
- **AotR60_SpDrain(STEP)**, from STEP_DRAIN 0x441822 after GameEngine::update, i.e. after the logic step and before IsIconic, the pump, the pacer and the streamer.
  - If iconic or not foreground: cancel.
  - Otherwise w = min(g_pb.target, dlNext recomputed now, g_pb.tHandoff + 60 ms).
  - Plain WaitUntil(w): pacer.cpp:73, exported as PacerWaitUntil. No message dispatch, the same kind of wait as today's spacing wait. Then SpPresentNow.
  - g_drainWaitTicks is recorded so tailEst excludes it.
- **AotR60_SpNet(reason)**, from PUMP_NET, TCL_NET, BEGIN_RENDER_NET and the PRESENT_STUB re-entry (a second Present while pending).
  - On the main thread: present now, or cancel if iconic.
  - Off the main thread: cancel.
  - Always counted. In steady state these must read 0, except PUMP_NET, which should be about 0.
- **OnPreRender (C0) net**: right after the g_mainTid init (frame_ctl.cpp:374), if g_ppState != 0, call SpNet(C0) and ++anomaly. This covers an exception unwinding past STEP_DRAIN.
- **SpCancel(reason)**, from any thread: CAS 1 -> 0; g_cpDue = 0; count; cooldown. Callers:
  - GAP_RESET_CAVE (Reset_Device, every caller)
  - LeaveSixty (frame_ctl.cpp:264)
  - OnEngineReset (frame_ctl.cpp:451)
  - PacerOnModeChange
  - SHUTDOWN_CAVE
  - PresentSkip off the main thread

  A cancelled B is simply never shown, which is legal (the existing skip path does the same). When a non-main thread holds the DX mutex, the main thread cannot be in state 2, because state 2 is held under the mutex. So a cancel never races a running Present.

## F. LogicUpdateWrapper (telemetry.cpp:598)

- On the main thread: g_inLogic = 1 before the call and 0 after. Reset g_cpPresentInCall = 0 before.
- Always take t0 when g_spActive, not only when telemetry is on.
- d_corr = d - g_cpPresentInCall. Use it for the predictor and for logic telemetry, so Present blocking is never learned as logic time.
- Record g_logicEndQpc.
- Learn (F' = [GL+0x40] after the call) only when all of these hold:
  - (sub >= 2) || (sub == 1 && F' == F + 1); this excludes frozen and failed attempts, and paused attempts never reach the wrapper;
  - F' >= 25 (pathfinder x100 budget, 0x6F247F);
  - F' != 2 (one-off block 0x62E640);
  - [GL+0x125] == 0;
  - no gap or reset flag pending.
- Outliers: if the slot has 2 or more samples and d_corr > 3*median + 5 ms, store 3*median + 5 ms and count a clip.
- Flush all tables on PacerOnModeChange and OnEngineReset.

## G. Pacer interaction (pacer.cpp)

- **S0 (M2)**: the on-time branch (pacer.cpp:247-252) becomes `pay = min(owed, max(I/8, owed/6), -late)` when RepayProportional is set.
- **S2 (E)**:
  - `g_deadline += g_interval + g_borrow; g_borrow = 0;` (pacer.cpp:217). Gap, late and mode-change paths reset g_borrow.
  - In the on-time branch, after the pay and before WaitUntil, when g_spActive && cfg.splitEarly && g_iterWasA && the predictor has a value for the coming Y step:
    - over = bEst + L-hat + tailEst - g_interval
    - if over > 0: shift = min(over, g_deadline - now); g_deadline -= shift; g_borrow = shift; ++earlyReleases
  - Only predicted-overrun Ys are released early. Light ticks are unchanged; Eall is not used.
- **A-render** (PresentSkip with !g_inB):
  - spacing target = min(old target, g_lastA + g_pairTicks - f/2000), so a late deferred B never pushes A past its cadence;
  - set g_lastA = now at the A present;
  - update aPre[g_c0Sub] with now - g_releaseTime (paced only, clip at 3x).
- Late-skip, forced skip, fallback windows (g_windowBRenders) and the gap logic are unchanged. presentsSinceBlock also gets deferred-Present samples. tailEst is updated at pacer entry for Y iterations.

## H. FPU/SSE and determinism

- Checkpoints sit only at call boundaries or places where the x87 stack is empty (CONFIRMED per site).
- CP_SLOW = SAVE_ALL (flags, GPRs, XMM0-7, cld) + fxsave/fxrstor (x87 CW/SW/TW, MXCSR).
- SpPresentNow also saves and restores LastError and checks the RNG seed [0xDA1CA4].
- The timer thread's FP state is per-thread and irrelevant.
- No stack snapshot: refuted (it can read past StackLimit, and SAVE_ALL writes below ESP anyway). Stack-content independence is proven by compare_traces (Tests).

## I. Device lost, alt-tab, mode switch, exit

- Covered by these gates and nets: foreground and !IsIconic at the handoff and at the drain; cooldown after g_gapDevLost / g_gapReset / pacer gap; Reset_Device cancel; pump net; no Present after WW3D::Shutdown.
- The main thread never calls TCL, Reset or Sleep(200) in our code.
- Native d3d9 is off by default.

# telemetry
One play session with Telemetry=1 should verify everything. Add a "split" block to each telemetry window line in aotr60.log, and the same fields as columns in aotr60_rates.csv. Tag every window with sp=0/1 from the hotkey state so the A/B segments separate.

## Counters (per window, and also totalled at exit)

**Handoff**
- B handoffs.
- deferred, and immediate (target <= now + 0.3 ms).
- notDeferred, broken down by reason: off, unpaced, fifoFull, forcedSkip, lateSkip, notForeground, iconic, paused, lwMap, capture, cooldown, busy.

**Where each deferred B was presented**
- cp[8] per site.
- stepDrainWaited, and stepDrainLate (target already passed).
- Nets: pumpNet, tclNet, beginRenderNet, presentStubNet, c0Net.
- Cancels: reset, engineReset, mode, iconic, shutdown, offMain, mutexTimeout.
- Each net and cancel is logged with renderId the first 10 times.

**Timing quality**
- Lateness of the actual Present call against target: p50 and max, plus a histogram with buckets <0.5, <1, <2, <4, >=4 ms.
- Timer-thread wake lateness: avg and max.
- In-logic Presents: count, sum and max duration (the B Present inside logic, which includes DXVK blocking).
- presentMax: existing field, telemetry.cpp:357-362.
- Present blocks (took > 1 ms).

**Cost and safety**
- slowPathEntries.
- mutexOwnedSkips and tryLockFails.
- fpStateChanged and seedChanged: both must be 0.
- DEVICELOST from a deferred Present.
- logicExt: present time inside each heavy step, as max and avg. This is the logic time with the corrected d_corr against the raw value.

**Predictor**
- Per sub: mean |L-hat − d_corr|, empty-slot predictions, outlier clips, flushes.
- aPre and bEst / tailEst current values.

**Early release (S2)**
- earlyReleases and their ticks.
- borrowedButNotDeferred: count of Ys released early whose B then failed a gate. These must be rare.
- owed and debt (existing).

**Displayed cadence**
Computed from actual Present call times for A and for deferred or immediate B:
- max interval;
- count per second of intervals > 24.75 ms and > 34 ms;
- max A→B and B→A per window;
- per-tick hold max (the visible hitch metric).

Under FIFO these are submit times, not scanout times. The existing late-skip and presentsSinceBlock data put them in context.

## Profile mode (SplitPresent=2, nothing deferred)

1. On every 10th logic frame (F%10==0), LogicUpdateWrapper sets g_cpDue=1 at entry and clears it at exit.
2. AotR60_Checkpoint then records, per (sub, site): the hit count, plus the maximum gap between consecutive checkpoint QPCs, including call start → first hit and last hit → call end.
3. Output per window:
   - the heaviest sub, and d_corr max/avg per sub;
   - per-site hits;
   - the max gap per sub with the site pair around it, giving where the largest gaps fall.

This settles the INFERRED checkpoint spacing. Acceptance: in the heavy sub, the p99 of the per-tick max gap is ≤ 4 ms. Otherwise enable the verified optional sites, chosen from where the gaps fall:
- ALT_RECRUIT 0x8F9545
- ALT_DESTROY 0x62A3F1
- ALT_OBJ1END 0x62EC9D
- ALT_SAP 0xB6D8C4
- ALT_MGR_MID 0x62EBB3
- ALT_SKAI0 0x6A96CD

## Stress mode (SplitPresent=3)

Every gated B is presented at the first checkpoint, which is the maximum perturbation. This mode is for Telemetry=2 determinism traces only.

## Measurements to reopen (T) later

These come for free from the counters above:
- in-logic Present duration (as an upper bound for lock hold);
- the number of main-thread D3D calls per render, via the delta of [0xDD34D8].

# expected
All figures are model output from the calibrated simulator (mtsim, tsim V3 calibration, ±3 ms on L), so all are INFERRED. Sources: my rerun mt/lead/out_lead.txt, plus mt/simulation/out_final.txt and out_cgran.txt. Each figure is 3 seeds × 60 s, P0..P4. Old = committed pacer. S1 = C2w: checkpoints every 2 ms, wait rule, med3 predictor. S1+S2+S0 = C2w+E+M2.

**User's case (L ≈ 31 ms heavy sub-step)**
- Old: A_k→B ≈ 13 ms, then a B→A_{k+1} hold of ≈ 40.6 ms.
- Midpoint deferral: ≈ 26.8 / 26.8 ms. The arithmetic was confirmed by the sync verdict.
- On a 120 Hz FIFO display: holds of 41.7/50 ms become 25/33.3 ms.

**IMMEDIATE, 'ai' load (+8 ms every 5th tick, +15 ms more every 10th)**

| | gap p99 (ms) | frames >34 ms per s | p2p error (ms) | speed at P4 |
|---|---|---|---|---|
| old | 22.7 / 33.5 / 41.1 / 47.1 / 56.4 | 0.52 / 0.56 / 1.49 / 4.55 / 4.88 | 2.6 / 9.4 / 17.2 / 21.4 / 30.5 | 0.968 |
| S1 | 25.6 / 30.2 / 34.0 / 36.2 / 39.7 | 0 / 0 / 0.63 / 0.88 / 1.26 | | |
| S1+S2+S0 | 21.1 / 26.5 / 31.2 / 33.0 / 37.3 | 0 / 0 / 0.02 / 0.40 / 0.95 | 2.5 / 2.8 / 7.7 / 11.8 / 20.8 | 1.000 |

- The speed fix comes from M2: old+M2 alone gives 0.9992.
- S1 alone regresses P0: p99 22.7 → 25.6, and frames >24.75 ms go 0.52 → 0.76 per second. S2 removes the regression: 21.1 and 0.00. Ship S1 and S2 together.

**IMMEDIATE, 'cal' load (heavy sub i.i.d., cv 0.3)**

| | gap p99 (ms) | frames >34 ms per s | p2p error (ms) |
|---|---|---|---|
| old | 19.9 / 30.9 / 40.3 / 47.7 / 59.2 | 0.02 / 0.35 / 1.75 / 2.82 / 4.37 | |
| S1 | 19.2 / 25.2 / 31.0 / 35.0 / 41.9 | | |
| S1+S2+S0 | 19.2 / 23.0 / 28.6 / 33.3 / 39.4 | 0.01 / 0.05 / 0.24 / 0.55 / 1.28 | 2.4 / 2.9 / 8.1 / 11.2 / 20.0 |

**FIFO 120 Hz, 3 images (the test setup)**

| | gap p99 (ms) | frames >34 ms per s |
|---|---|---|
| 'ai', old | 25.0 / 33.3 / 41.7 / 50.0 / 58.3 | 0.23 / 0.53 / 0.95 / 2.72 / 4.81 |
| 'ai', S1+S2+S0 | 25.0 / 25.0 / 33.3 / 33.3 / 33.3 | 0 / 0 / 0.01 / 0.09 / 0.57 |
| 'cal', old | — | 0.02 / 0.16 / 1.08 / 2.09 / 3.62 |
| 'cal', S1+S2+S0 | 25.0 / 25.0 / 27.8 / 33.3 / 41.7 | 0.01 / 0.01 / 0.14 / 0.29 / 0.89 |

**What does not improve**
- Frames >24.75 ms per second at P4 stay at about 8-10 per second, because one ~56 ms hold becomes two ~28 ms holds. Read >34 ms per second as the hitch metric.
- On a 60 Hz FIFO display the 3-deep queue already absorbs the holds. The FIFO-full gate keeps the feature mostly idle there.
- Stock spikes of 100 ms or more and unpredictable outliers are not reduced.
- Theoretical floor: the larger of the two holds is at least (A_k→A_{k+1})/2. With S2 that is about 37 ms when L ≈ 59 ms (P4 plus the 10th-tick AI work).

**Checkpoint spacing**
- g = 1-4 ms comes within ±1.3 ms of an ideal presenter thread.
- g = 8 ms degrades P0/P1: p2p goes to 4.3 / 8.9 ms.
- No checkpoints at all is worse than old ('cal' P2..P4 p99 39.1 / 45.0 / 56.2). The profile session must therefore confirm the gap distribution before the gains above apply.

**Overhead**
- Fast path: about 1 ns per hit, at most about 40k hits per tick, so ≤ 0.05 ms per tick.
- Slow path: only once B is due, normally 1 hit per deferral, plus retries while the DX mutex is owned.
- Timer thread: ≤ 0.7 ms spin per deferral on another core.
- Present inside logic: about 0.15-1 ms when the FIFO queue has room. This is the gate condition, and it is measured.

# risks_and_tests
## Risks (most serious first)

1. **Checkpoint coverage (INFERRED).** These stretches have no checkpoint inside them, so B can be presented up to that long after its target:
   - one A* search, 1-4 ms;
   - a path-zone rebuild 0x93A530;
   - a collision full rebuild 0xB6CC30;
   - a single long script or AI recruit search;
   - the sub-5 manager block.

   **Mitigation:**
   - Ship profile mode first. If the data justifies it, enable the 6 verified optional sites.
   - Lateness is bounded by STEP_DRAIN: B is never shown after the next pacer deadline. The worst case is therefore the old behaviour, not worse.

2. **Present inside logic.**
   - Same-thread window messages. DXVK's steady-state Present only calls GetClientRect (CONFIRMED for the DXVK layer). Swapchain recreate on OUT_OF_DATE now happens inside logic instead of inside the render.
     - Mitigation: foreground and not-iconic gates, a 2 s cooldown after any gap, reset or device loss, and native d3d9 off by default.
     - Residual: an alt-tab between handoff and present. The pump net and the Reset cancel cover the game-side paths.
   - Blocking Present. Gated by g_presentsSinceBlock >= 2; the logicExt telemetry measures it.

3. **Determinism.**
   - Covered: x87 CW/MXCSR, flags, GPRs, XMM and LastError are saved, and the RNG seed is checked.
   - Not covered: stale stack below ESP differs from stock. It already differs through LogicUpdateWrapper and other wrappers, but stack independence is unproven. Wall-clock reads in logic (ScriptEngine timeGetTime 0x60CD8E/0x60CE34, pathfinder QPC 0x6F255E debug-gated) look harmless (INFERRED).
   - Mitigation: the compare_traces gate below, including stress mode.

4. **D3D ordering.** Logic D3D calls (CreateVertexBuffer, Lock, Release) now run between B's EndScene and its Present, on the same thread under the DX mutex. That is legal in D3D9.
   - Back-buffer readers after Present (frame save 0x44BC54, tile capture 0x44BAC7) are gated.
   - Second drawFrames are covered by TCL_NET / BEGIN_RENDER_NET.
   - Unaudited virtual paths could still issue back-buffer D3D calls from logic (SetRenderTarget/Clear on the back buffer; INFERRED none). The nets catch WW3D paths only.

5. **S2 early release.** A predicted-heavy Y's B-render starts up to `shift` ms early.
   - Logic is frame-stepped and does not depend on release time (INFERRED). Real-time cosmetic client effects in B would be stamped earlier but presented at the target (cosmetic only, INFERRED).
   - If the B then fails a gate, the cap shifts by the borrow, so B is not shown early. The borrowedButNotDeferred counter tracks this.

6. **Predictor.**
   - Slot drift is removed by indexing on game state (GE+0x34, GL+0x40).
   - Spike replay is limited by med3 plus the outlier clip.
   - A misprediction only moves the target, and STEP_DRAIN bounds it.
   - Residual: the ~2-4 s worse-than-old window for >24.75 ms frames after a disruption (sim verdict).

7. **Timer thread.** Under CPU saturation it can wake late. The B is then presented late, bounded as in risk 1. Wake lateness is logged.

8. **30-mode exactness.**
   - SplitPresent=0 leaves the image unchanged.
   - With the sites installed:
     - in 30 mode every stub executes the original instructions plus one compare;
     - CP_MOD stores return value 0x62EA9C instead of 0x62EA9E;
     - GameEngine::update runs 4 bytes deeper (STEP_DRAIN);
     - both follow the VTGATE precedent.

9. **DX mutex.**
   - A 0x51EF50 timeout of 100 ms or more disables the feature.
   - Lock order: we take only the game DX mutex and never hold it while waiting on anything else.

## Test plan (in order; each step gates the next)

**T0 — build and static.**
- tools/verify_sites.py passes with the 13 new sites. Port verify_cp.py's brute rel8/rel32 and abs32 checks into it.
- With SplitPresent=0, the PatchSet is identical to the current build: same site count and bytes.
- gen_sites.py and sites.gen.inc regenerate cleanly.

**T1 — determinism (Telemetry=2, same replay or save).**
- Run compare_traces.py on:
  - 30 mode;
  - 60 mode with SplitPresent=0;
  - 60 / SP=1 / Early=1 / M2=1;
  - 60 / SP=3 stress.
- All logic traces must be identical, and fpStateChanged = seedChanged = 0.
- Repeat SP=3 with the profile counters to confirm that every checkpoint site fired at least once.

**T2 — profile session (SP=2).** 10 minutes of a typical large skirmish at 4K, 120 Hz FIFO, exclusive fullscreen. Read off:
- the heavy sub and the per-site counts;
- the max gap per sub.

Acceptance: p99 of the per-tick max gap ≤ 4 ms in the heavy sub; otherwise add the named optional sites and repeat T1.

**T3 — A/B play session.** SP=1, Early=1, RepayProportional=1, Telemetry=1, same setup, at least 15 minutes into the heavy phase. Toggle Ctrl+Shift+F9 about every 60 s. Pass criteria, heavy windows with sp=1 vs sp=0:

| Metric | Pass criterion |
|---|---|
| Frames >34 ms per second | down at least 60 % |
| Max per-tick hold | down at least 10 ms |
| c0Net, tclNet, beginRenderNet, presentStubNet, offMain cancels | 0 |
| pumpNet | ≈ 0 |
| In-logic Present max | < 2 ms |
| logicExt avg | < 0.3 ms |
| Mutex skips | rare |
| Speed ratio in rates.csv | unchanged or improved |
| Fallbacks | not more frequent |
| User's subjective report | the once-per-second hitch is gone or much weaker |

**T4 — stress, while in a heavy battle.**
- Alt-tab out and back several times.
- Minimise and restore.
- Display-mode change, if the game offers one.
- Save, then load.
- Quit to menu, and exit the game.
- Pause and unpause.
- A scripted camera time-multiplier cinematic (forced-skip path).
- Screenshot / frame-save key (capture gate).
- LW campaign battle and the LW strategic map (no deferral on the map).
- Windowed mode (BackBufferCount 1, latency 2).
- A 60 Hz display or refresh (FIFO-full gate keeps it idle).
- RTSS / overlay off and on.

Expected: no hang, no wrong frame shown; cancels only at resets and loads; cooldowns visible in the counters.

**T5 — native d3d9.** Temporarily rename rotwk\d3d9.dll.
- Check that the log reports split-present disabled (native).
- Optionally set SplitPresentNative=1 and repeat the T4 alt-tab subset, to decide whether native may ever be enabled.

**T6 — rollout.** Default SplitPresent=0 until T1-T4 pass. Then default SplitPresent=1, Early=1, RepayProportional=1. Keep the hotkey for user A/B and future regressions.