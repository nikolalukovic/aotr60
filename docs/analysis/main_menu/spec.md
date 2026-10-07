# decision
DECISION (lead): The safe set of shell screens CAN run at 60 FPS. Running them at 60 makes nothing visibly smoother, so I am specifying it as an opt-in `Menus` switch, default 0. The user should get the honest answer below before the default is flipped.

WHY NOTHING WILL LOOK SMOOTHER (CONFIRMED by 3 independent verifiers):
1. AotR has no 3D shell map. ShellMapOn = No (aotr\data\ini\gamedata.ini:11133; GD+0xAF0 checked at 0x75DE2D/0x75DE76), so every menu screen runs in GL+0x110 = 9 with no game loaded.
2. Every on-screen source changes at 30 Hz or slower:
   - APT movies are authored at 33 ms/frame: MainMenu.apt, SkyrimMenu.apt, the 19 AotR .apt files and 79 of the 81 rotwk\apt files. MenuFrameAndBg runs at 83 ms and libSnow at 40 ms.
   - The APT update (GATE_CU_APT), Shell::update (throttled to at least 32.333 ms at 0x75E210, then GATE_GC_SHELL), window transitions (GATE_GC_WM), video service (0x814218 is only reached from the A-only APT update; GATE_GC_DISPUPD) and input (GATE_GC_KBDSEL/MOUSESEL) are all A-only.
   - The backdrop is a static MappedImage (ShellMapLowLOD = InstallLoad.tga, set via 0x65C42C).
   - The cursor is the OS cursor (redraw mode 0, GD+0x9C6 = 1 at 0x642E38; SetCursor at 0x4413E3).
   - The only per-render source is the texture mappers of the Background.apt View3D model SFE_MenuBkgrnd. I CONFIRMED that <game folder>\aotr\MainMenu.apt contains "SetBackground fadein", so the model is probably shown. Its rotate mappers turn at 0.02-0.03 rev/s, about 0.25 degrees per 33 ms, which is imperceptible.
3. So in 60 mode each B-render is a pixel copy of its A-render. The title bar would say 60, the screen would look like stock 30, and GPU load at 4K doubles.
4. Input latency does not improve either, because input and the APT reaction are A-only.

The only route to a menu that really looks 60 FPS is render-time interpolation of the APT display list: character matrices and colour transforms between APT frames k-1 and k, plus rules for cuts and removed characters. That would be "phase 7b", a large reverse-engineering project, and it is NOT part of this spec. It needs the user's go-ahead.

WHAT TO BUILD NOW (phase 7a, safety layer, low risk; several pieces also harden battles):
- frame_ctl.cpp: an allow-list ShellBlockReason for mode 9, wired into GameModeBlockReason, with refined reasons for the other modes.
- New key `Menus` (default 0).
- Two new verified sites:
  - GATE_GC_DISPMODE at 0x6488A1 keeps the display-mode change A-only.
  - NEWGAME_GUARD at 0x6314CD leaves 60 mode at startNewGame if no reset ran.
- DLL fixes:
  - add BackBufferWidth/Height to the display signature;
  - scope the pacer fallback and g_unknownPath to the menu;
  - telemetry: trace filter, mode number in log lines, shell counters.

ALLOWED AT 60 (with Menus = 1): main menu (MainMenu.apt), Options (including Apply), Skirmish setup, Campaign menu / Timeline, Save/Load, and other plain SP shell screens in mode 9 that pass every check.

DROPPED FROM THE ANALYSES:
- Shell+0x5D used as a credits flag. Refuted: it means "hide 3D views in a shell game" and has 5 setters, so it can stay 1.
- The OnPostRender mid-pair exit guard. NEWGAME_GUARD, the reset hook and the pair-boundary re-check cover the same cases without new mid-pair code.
- Display signature range 0xDD3014..0xDD302C and the TheShell pointer comparison. Both refuted.
- R2 camera flush via GAP_RESET. camera_math.cpp:70-79 already treats an aspect, near or far change as a cut, and the proposed flush would act too late.
- SHELL_THROTTLE_60. It deviates from stock cadence; it is replaced by a telemetry counter.
- A render-count start rule and a "dm_frame = 0 accepted" telemetry exception. Refuted: GC+0xC8 is set at 0x6325FB, so m_frame advances at 30/s in the shell and StartBlockReason works unchanged.

WHAT TO TELL THE USER (suggested wording): "I can switch the menu to 60 FPS safely (Menus=1), but it will not look any different: every menu animation in AotR is authored at 30 frames per second, the background is a still image and the cursor is the Windows cursor. 60 FPS would only show each picture twice and double the GPU load at 4K. Making the menu actually look smoother would need a new, large project that interpolates the Flash (APT) menu animations. Do you want the switch on anyway, or should I investigate the interpolation project?"

# predicate_code
// ===== src/runtime.h, namespace game: new constants =====
constexpr uintptr_t kTheShell = 0xDE7890;          // Shell, vtbl 0xC2C894: +0x4C screen count, +0x5C shell shown (0x75E4D4 / 0x75DBEC)
constexpr uintptr_t kTheCredits = 0xDEBF50;        // CreditsManager; only writers 0x91B635 (create) and 0x91B72D (clear)
constexpr uintptr_t kTheLan = 0xDE4394;            // TheLAN; writers 0x847F3E, 0x913217, clear 0x8471E8
constexpr uintptr_t kLanLobbyScreen = 0xDE8D90;    // INFERRED: LanLobby screen, set 0x848D29, cleared 0x84722F
constexpr uintptr_t kOnlineShell = 0xDEA36C;       // INFERRED: AptOnline screen, set 0x91E1D6, cleared 0x91DC4C
constexpr uintptr_t kTheDisplay = 0xDE4418;        // +0x38: full-screen video buffer (INFERRED from drawFrame's video tail)
constexpr uintptr_t kTheMessageStream = 0xDE6398;
constexpr uintptr_t kTheCommandList = 0xDE639C;
constexpr uintptr_t kShellLastUpdateMs = 0xDE7894; // Shell::update throttle timestamp (0x75E1FC, 0x75E248)

// ===== src/config.h / config.cpp =====
// Config: add   bool menus = false;   // phase 7a: 60 FPS in the single-player main menu (no visible change, see README)
// ParseConfig:  else if (EqualsNoCase(key, "Menus")) { ok = ParseBool(value, cfg.menus); }
// kDefaultConfigText: add after LivingWorldMap:
//   "# 60 FPS in the main menu. Safe, but the menu looks the same: its animations are authored at 30 fps.\n"
//   "Menus = 0\n"

// ===== src/frame_ctl.cpp (anonymous namespace) =====

// Messages that start or tear down a game. Layout CONFIRMED by containsMessageOfType 0x710E2D
// (head +0xC, next +4, type +0x10) and by propagate 0x7128C3.
// 0x1D clear game data, 0x1E new game, 0x1F new Living World game, 0x20 campaign hop (0x91B254).
bool ListHasGameStart(uintptr_t global)
{
    uint8_t* list = Ptr(global);
    if (!list) {
        return false;
    }
    uint8_t* m = Field<uint8_t*>(list, 0xC);
    for (int n = 0; m && n < 512; ++n, m = Field<uint8_t*>(m, 4)) {
        uint32_t t = Field<uint32_t>(m, 0x10);
        if (t >= 0x1D && t <= 0x20) {
            return true;
        }
    }
    return false;
}

bool GameStartPending()
{
    return ListHasGameStart(kTheMessageStream) || ListHasGameStart(kTheCommandList);
}

// Mode 9 (no game loaded) = every AotR shell screen, because ShellMapOn = No. Allow-list: plain single-player
// shell screens only. Each check blocks; blocking is always the safe direction.
const char* ShellBlockReason(uint8_t* ge, uint8_t* gc, uint8_t* gd)
{
    if (Field<uint8_t>(gd, 0xAF2) || Field<uint8_t>(gd, 0xAF3)) {
        return "intro";                                  // cleared at 0x645793 / 0x6457EA
    }
    if (!g_cfg.menus) {
        return "main menu (Menus=0)";
    }
    uint8_t* shell = Ptr(kTheShell);
    if (!shell || !Field<uint8_t>(shell, 0x5C) || Field<int32_t>(shell, 0x4C) <= 0) {
        return "menu transition";                        // shell hidden or screen stack empty
    }
    if (Field<uint8_t>(gd, 0xAF6)) {
        return "loading screen";                         // set 0x62A221, cleared 0x628F87
    }
    if (Ptr(kUiSequenceQueue)) {
        return "menu sequence";                          // intro steps, LW/LAN entry sequences
    }
    if (Ptr(kTheCredits)) {
        return "credits";                                // the credits run at FPS limit 100 (0x91B6DC)
    }
    if (Field<int32_t>(ge, 0xC) != Field<int32_t>(gd, 0x28)) {
        return "FPS limit changed";                      // credits exit lag, or a MSG_NEW_GAME arg3 leftover
    }
    if (Ptr(kTheLan) || Ptr(kLanLobbyScreen)) {
        return "LAN";
    }
    if (Ptr(kOnlineShell)) {
        return "online";
    }
    uint8_t* disp = Ptr(kTheDisplay);
    if (disp && Field<uint32_t>(disp, 0x38)) {
        return "movie";
    }
    if (Field<uint8_t>(gc, 0xC9)) {
        return "display mode change";                    // pending 0x645DAD (set 0x6455F3 / 0x645622)
    }
    if (GameStartPending()) {
        return "game starting";
    }
    return nullptr;
}

// GameModeBlockReason: new signature (ge, gl, gc, gd). Only the first branch changes, plus one line after it.
const char* GameModeBlockReason(uint8_t* ge, uint8_t* gl, uint8_t* gc, uint8_t* gd)
{
    uint32_t mode = Field<uint32_t>(gl, 0x110);
    uint8_t* view = Ptr(kLwView);
    uint8_t* lwl = Ptr(kLwLogic);
    bool viewActive = view && Field<uint8_t>(view, 0x18);
    if (mode != 8 && !viewActive && !Field<uint8_t>(gl, 0x125)) {
        switch (mode) {
        case 0: case 2: case 6: return nullptr;
        case 9: return ShellBlockReason(ge, gc, gd);
        case 1: case 5: return "multiplayer";
        case 3: return "replay";
        case 4: return "shell map";                      // never reached with AotR's INI; not audited
        case 7: return "Create-a-Hero";
        default: return "game mode";
        }
    }
    if (mode == 9 && view && Field<uint8_t>(view, 0x19)) {
        return "Living World map preview";               // MpGameSetup's AptMapPreview 0x975659: active + suspended
    }
    // ... the rest is unchanged: LivingWorldMap=0 / "Living World map transition" / LW map checks ...
}

// BlockReason: the call becomes  if (const char* m = GameModeBlockReason(ge, gl, gc, gd)) return m;
// All later generic checks stay (0x114, 0x9D, replay, BBD, D45, AF2/AF3, UseFPSLimit, script debugger,
// g_unknownPath, fallback, LOD, display).
// StartBlockReason: UNCHANGED. GC+0x10 < 8 is load-bearing: after a reset the next sub-1 step comes within 6 steps,
// so the threshold must stay at 7 or more.

// DisplayBlockReason: add BackBufferWidth/Height to the signature (R3):
//   struct Signature { uint32_t width, height, interval, windowed, refresh; HWND hwnd; ... };
//   Signature now{Read<uint32_t>(0xDD2FF8), Read<uint32_t>(0xDD2FFC), Read<uint32_t>(0xDD302C),
//                 Read<uint32_t>(0xDD3018), Read<uint32_t>(0xDD3028), Read<HWND>(0xDD3014)};
//   and print "%ux%u" in the "display:" log line.

// OnPreRender log line: add the game mode, which proves mode 9 at runtime:
//   Log("mode: 60 FPS %s: %s (game mode %u)", ..., gl ? Field<uint32_t>(gl, 0x110) : 0xFFFFFFFFu);

// NEWGAME_GUARD target (extern "C", frame_ctl.cpp):
extern "C" volatile uint32_t g_newGameGuardHits = 0;
extern "C" void __cdecl AotR60_OnNewGameGuard(uint32_t returnAddress)
{
    if (!OnMainThread()) {
        return;
    }
    ++g_newGameGuardHits;
    Log("WARN startNewGame with 60 FPS on and no engine reset (return 0x%X) - leaving 60 FPS", returnAddress);
    g_fs.Reset();
    LeaveSixty("new game without engine reset");
    g_lodChecked = nullptr;
    SplitPresentReset(kCancelEngineReset);
}

# sites_json
[
 {
  "id": "GATE_GC_DISPMODE",
  "phase": "7",
  "group": "GameClient::update allow-list gates (0x64849E)",
  "address": "0x6488A1",
  "length": 5,
  "original_hex": "e8 07 d5 ff ff",
  "original_asm": [
   "006488A1: e8 07 d5 ff ff  call 0x645dad  ; pending display-mode change, reached only if GC+0xC9 (preceded by cmp byte [ebx+0xC9],0 / je 0x6488A6 / mov ecx,ebx)"
  ],
  "kind": "call_gate",
  "replacement_hex": "e8 <rel32:Gate_GC_DISPMODE>",
  "stub": "Gate_GC_DISPMODE (CALLGATE Gate_GC_DISPMODE, IDX_GATE_GC_DISPMODE, 645DAD): cmp byte ptr [g_m60],0 / je .run / cmp byte ptr [g_uiTick],0 / je .skip / .run: jmp dword ptr [g_tgt_GC_DISPMODE] (=0x00645DAD, thiscall ECX=EBX=TheGameClient, no stack args, plain RET at 0x645F64; [esp]=0x6488A6) / .skip: ret (GC+0xC9 stays set; the next A-render services it at the same site). 30: .run == original. 60/A: .run. 60/B: .skip.",
  "resume_address": "0x6488A6",
  "live_after": "Nothing from the span: 0x6488A6 reloads ECX=[0xDE4830] and 0x6488AC reloads EAX=[ecx]. EBX=TheGameClient (used at 0x6488B1) is callee-saved, as are ESI/EDI/EBP. EFLAGS dead. x87 depth 0. No XMM live.",
  "branch_into_span_check": "Interior 0x6488A2..0x6488A5: 0 hits (brute rel8/rel32/jcc32 decode over executable sections plus a whole-image abs32 scan, re-run independently by the verifier). 0x6488A6 is the je target from 0x64888F/0x64889D (span end, allowed). The adjacent GATE_GC_IGUI starts at 0x6488A6, so no overlap. Bytes identical in game.dat, zGameDats\\delayfix.dat and game820.dat.",
  "purpose": "Keep the display-mode change A-only. 0x645DAD deletes and recreates TheShell, resets the D3D device and reloads MainMenu.apt. Requests come from Options apply 0x91EF0F->0x6455F3 and the AptMainMenu revert 0x91B2A7->0x645622. Without the gate, a request raised after A's check (e.g. during propagate 0x6324A1) runs inside the next B-render. This applies to in-game Options at 60 as well.",
  "evidence": "menu/ui report + verifier: setters of GC+0xC9 are 0x64560B and 0x645640 only; ordering draw 0x648869 < Shell 0x648891 < check 0x648896 < propagate 0x6324A1.",
  "dll_vars": [
   {"name": "g_m60", "ctype": "uint8_t", "value_30": "0", "value_60_A": "1", "value_60_B": "1"},
   {"name": "g_uiTick", "ctype": "uint8_t", "value_30": "1", "value_60_A": "1", "value_60_B": "0"},
   {"name": "g_tgt_GC_DISPMODE", "ctype": "const uint32_t", "value_30": "0x00645DAD", "value_60_A": "0x00645DAD", "value_60_B": "0x00645DAD"}
  ],
  "risks": "Insurance only: most requests are already serviced on the same A-render. A request raised during the B half is delayed by one render (16.5 ms). Coverage entry 'GameClient::update 0x64849E / 0x6488A1' changes cls 'run' -> 'gate', with note 'display-mode change A-only (GATE_GC_DISPMODE)'."
 },
 {
  "id": "NEWGAME_GUARD",
  "phase": "7",
  "group": "core",
  "address": "0x6314CD",
  "length": 5,
  "original_hex": "b8 67 45 b8 00",
  "original_asm": [
   "006314CD: b8 67 45 b8 00  mov eax, 0xb84567  ; GameLogic::startNewGame(bool loadingSaveGame) SEH handler; next 0x6314D2: e8 19 ba 40 00 call 0xa3cef0 (_EH_prolog)"
  ],
  "kind": "func_detour",
  "replacement_hex": "e9 <rel32:NEWGAME_GUARD_CAVE>",
  "stub": "NEWGAME_GUARD_CAVE PROC\n    cmp byte ptr [g_m60], 0\n    je stock                       ; 30 mode: no C++ call\n    mov eax, [esp]                 ; return address: 0x6B534C (LW map start), 0x779DF3 (MSG_NEW_GAME), 0x82BEF2 (load)\n    SAVE_ALL\n    push eax\n    call AotR60_OnNewGameGuard     ; cdecl(uint32_t): main thread only; g_fs.Reset(); LeaveSixty(); SplitPresentReset(kCancelEngineReset)\n    add esp, 4\n    RESTORE_ALL\nstock:\n    mov eax, 0B84567h              ; displaced\n    jmp dword ptr [T_6314D2]       ; call _EH_prolog with the stock stack\nNEWGAME_GUARD_CAVE ENDP",
  "resume_address": "0x6314D2",
  "live_after": "At 0x6314D2: EAX=0xB84567 (consumed by _EH_prolog), ECX=this (TheGameLogic), [esp]=return address, [esp+4]=loadingSaveGame. EBX/ESI/EDI/EBP as at entry. EFLAGS dead (_EH_prolog does not read them). x87 empty and XMM caller-saved at a call boundary. SAVE_ALL/RESTORE_ALL preserve everything around the C++ call; EAX is overwritten by the displaced instruction anyway.",
  "branch_into_span_check": "Interior 0x6314CE..0x6314D1: 0 hits (unaligned rel8/rel32 sweep of the executable sections plus an abs32 scan; <analysis workspace>\\menu\\transitions\\span_check.py; confirmed by the verifier). The entry is reached only by calls at 0x6B5347, 0x779DEE and 0x82BEED. Bytes identical in game.dat, delayfix.dat and game820.dat.",
  "purpose": "Backstop: 60 mode can never be on while a new game or save load starts. It covers startNewGame callers with no engine reset of their own (LW map start 0x6B5286 via MSG 0x1F: setGameMode(8) 0x6B533A + startNewGame 0x6B5347), any path missed by GameStartPending, and future paths. Expected hits: 0. Every checked SP menu sender calls clearGameData first, and it is a no-op if the RESET hook already ran.",
  "evidence": "menu/transitions report + verifier; menu/modes verifier (GameEngine::reset only via vt+0x24 from 0x6DEA25, 0x7792BC, 0x8182CF, 0x91C2FC).",
  "dll_vars": [
   {"name": "g_m60", "ctype": "uint8_t", "value_30": "0", "value_60_A": "1", "value_60_B": "1"},
   {"name": "g_newGameGuardHits", "ctype": "volatile uint32_t", "value_30": "unchanged", "value_60_A": "+1 per hit", "value_60_B": "+1 per hit", "note": "telemetry summary, logged per caller"}
  ],
  "risks": "If it fires inside an A-render, the rest of that iteration runs as stock render k plus a stock step, the same as OnEngineReset (frame_ctl.cpp OnPostRender recomputes g_forceHalt from g_m60 = 0). If it fires inside the Y step, the pair is already complete."
 }
]

# code_changes
1. **src/config.h/.cpp, aotr60.ini template, README.md**
   - Add the key `Menus` (bool, default 0) next to LivingWorldMap.
   - README: replace "Menus ... stay at the stock 30 FPS" with: "Menus=1 lets the single-player main menu run at 60 FPS. It is safe but looks the same, because AotR's menu animations are authored at 30 fps. LAN, online, credits, Create-a-Hero, replays and the War of the Ring map preview always stay at 30."
   - docs/PLAN.md phase 7: record 7a (this spec) and 7b (APT display-list interpolation, not started).

2. **src/runtime.h**: add the constants listed in predicate_code (kTheShell, kTheCredits, kTheLan, kLanLobbyScreen, kOnlineShell, kTheDisplay, kTheMessageStream, kTheCommandList, kShellLastUpdateMs). Also declare:
   - `extern "C" volatile uint32_t g_newGameGuardHits;`
   - `void PacerOnShellState(bool shellIter);`
   - `void PacerOnEngineReset();`

3. **src/frame_ctl.cpp**
   - Add ListHasGameStart, GameStartPending and ShellBlockReason.
   - Change GameModeBlockReason to take (ge, gl, gc, gd) and update its call in BlockReason.
   - Add the "Living World map preview" reason.
   - Add the game mode to the "mode:" log line.
   - Add width and height to the DisplayBlockReason signature.
   - Add AotR60_OnNewGameGuard.
   - Do NOT add an OnPostRender exit guard; StartBlockReason and the reset hook stay unchanged.
   - **R6, menu-scoped g_unknownPath.** In OnPreRender keep `static uint32_t s_upSeen`. When g_unknownPath turns nonzero while s_upSeen is 0, store `g_upFromShell = (GL+0x110 == 9)` and log it. In OnEngineReset, when g_upFromShell is set:
     - `InterlockedExchange(&g_unknownPath, 0)`;
     - `g_upFromShell = false`; `s_upSeen = 0`;
     - log "unexpected-path flag from the menu cleared".
     A flag raised in a battle stays sticky, as today.
   - Call PacerOnShellState(mode == 9) at every C0, before PacerOnC0.
   - Call PacerOnEngineReset() from OnEngineReset.

4. **src/pacer.cpp / pacer_policy.cpp (R5).** No timing change: the shell has GE+0xC = 30, so P = 33 and T = 16.5 ms.
   - Remember whether the active fallback (g_fallbackUntil) was triggered while PacerOnShellState(true) was in effect.
   - PacerOnEngineReset(): if that fallback was triggered in the shell, clear g_fallbackUntil, reset the FallbackPolicy streak counters, restore the backoff count to its value before the shell fallback, and log "menu fallback cleared". Also clear the policy's window and streak counters on every reset that leaves the shell (the last C0 was mode 9), so menu windows never combine with battle windows.

5. **src/stubs/stubs.asm, tools/sites.json, sites.gen.* (via gen_sites.py), coverage**
   - `CALLGATE Gate_GC_DISPMODE, IDX_GATE_GC_DISPMODE, 645DAD`, with the g_tgt/T_645DAD entry generated like GATE_GC_POPUPS.
   - The NEWGAME_GUARD_CAVE proc, plus the T_6314D2 target.
   - Add both sites to sites.json. Change coverage entry 0x6488A1 from cls "run" to "gate".
   - Regenerate kSiteCount and the telemetry arrays, then run tools/verify_sites.py.
   - Reconcile "144 installed vs 148 in sites.json" from the user's last log before counting: it must read 150 of 150 after the change, or the same difference as before with a known reason.

6. **src/telemetry.cpp, tools/compare_traces.py (R7)**
   - (a) TraceLogicCall: return early when GL+0x110 is 4 or 9. compare_traces.py: drop segments whose maximum logic frame (GL+0x40) is 0. Otherwise a 60-FPS menu segment can pair with a 30-FPS one and print a false PASS.
   - (b) Shell counters while m60 is on and the iteration began in mode 9, all printed in the periodic summary and at exit:
     - shellA: number of such A-renders.
     - shellUpdSkips: A-renders where [kShellLastUpdateMs] did not change across the render, which means the 32.333 ms throttle dropped that Shell update.
     - The largest gap between Shell updates, in ms.
     - bSeedChanges: B-renders in mode 9 that changed kLogicRngSeed (0xDA1CA4); expected 0.
     - newGameGuardHits, broken down by return address.
   - (c) On every change of block reason while GL+0x110 = 9, log one line with these values, so the INFERRED pointers get confirmed at runtime:
     - Shell+0x5C, +0x4C and +0x5D;
     - LW view +0x18 and +0x19;
     - the UI-sequence head;
     - GE+0xC and GD+0x28;
     - TheLAN, LanLobby, Online, TheCredits;
     - Display+0x38, GD+0xAF6, GC+0xC9;
     - m_frame, GL+0x9C/0xA8, TV+0x23D4.

7. **No change** to camera.cpp, c5_physics.cpp, lw_present.cpp or split_present.cpp:
   - split present already returns kGateLwMap for mode 9 (split_present.cpp:611-613);
   - the camera is static, so BeginSwap is a no-op (camera.cpp:92-95) and A takes the "did not move" exit (:294);
   - the drawable pass at 0x48C701 is empty.
   - The GAP_RESET stub is unchanged (R2 dropped).

8. **UpdateTitle**: no code change. The new reasons show automatically, for example "[AotR60: 30 FPS, credits]" or "[AotR60: 60 FPS, smooth]".

# stays_30
These stay at 30, and each shows its reason in the title bar and log:
- **Intro, legal and logo movies** ("intro"). GD+0xAF2/AF3 are set by the first-run UI sequence (0x645B8D/0x64838D, AF3 at 0x648482). TitleScreenLogo is a 5 s blocking loop at 0x645BDF.
- **Credits** ("credits", then "FPS limit changed" until GD+0x28 is restored at 0x91B7AA). The credits set GE+0xC = 100 at 0x91B6DC, so the pacer would run 5 ms halves (200 renders/s). Stock already renders the credits at 100 FPS.
- **LAN lobby** ("LAN": TheLAN 0xDE4394 or LanLobby screen 0xDE8D90) and **online screens** ("online": 0xDEA36C). The project is single-player only, and TheNetwork is NULL in lobbies, so the existing "network game" check would not catch them.
- **War of the Ring setup with the embedded Living World map preview** ("Living World map preview"). AptMapPreview 0x975659 activates and suspends the LW view. Its vt24 draw 0x49BEFD has no presentation window and has not been audited. lw_present requires the view to be unsuspended, so 60 would only repeat 30 Hz frames.
- **Create-a-Hero** (mode 7, GD+0xD45), **replays** (mode 3 / recorder), **multiplayer** (modes 1/5, GL+0x114, GL+0x9D), and **mode 4 shell map** (never reached with AotR's INI and not audited).
- **Transitions:**
  - "menu transition": shell hidden or the stack empty, e.g. while 0x645DAD rebuilds the shell.
  - "menu sequence": UI-sequence queue non-empty, e.g. LW and LAN entry.
  - "game starting": MSG 0x1D..0x20 queued.
  - "loading screen": GD+0xAF6.
  - "movie": Display+0x38.
  - "display mode change": GC+0xC9.
  - The 8-render m_frame guard after every reset, which gives about 0.27 s of 30 FPS after quitting to the menu and after the clearGameData that skirmish setup and LAN entry run.
- **Loading screens and campaign movies.** They run their own blocking loops (load screen 0x758084 inside startNewGame, Display::playMovie 0x65D3F5), so C0 never runs during them. The behaviour is stock.
- **Stale states that pin the menu at 30 (safe, logged):**
  - TV+0x23D4 > 1 after quitting mid-cinematic ("camera time multiplier");
  - GL+0x9C/0xA8 after quitting during a fade-in ("fade-in");
  - a non-default GE+0xC from MSG_NEW_GAME arg3 ("FPS limit changed");
  - an LW view still active after quitting a Living World campaign ("Living World map transition").
- **Everything, when Menus = 0** (the default): "main menu (Menus=0)".

# test_checklist
Set Telemetry=1 and Menus=1 in %APPDATA%\Age of the Ring\aotr60\aotr60.ini. Then play normally and send aotr60.log. In fullscreen, read the title via the log's "mode:" lines.

1. **Start the game.** During the logo and intro movies the log shows "intro". About 0.3 s after the main menu appears it shows "60 FPS on" with "(game mode 9)".
   Look at the menu: button hover, the logo fade-in and the background should look and behave exactly as at 30. Nothing should run faster:
   - button fades;
   - the background (slowly rotating light effects);
   - shell music tempo;
   - the cursor;
   - menu-to-menu slide transitions.
2. **Leave the main menu idle for 60 s.**
   - Expect 0 tick-check errors, shellUpdSkips at or near 0 (report the number), bSeedChanges = 0 and no "performance fallback".
   - Check the GPU load or fan in the menu (expect up to twice the stock load).
3. **Options.**
   - Open it, change a setting, apply.
   - Then change the resolution, apply, and confirm or revert. The log should show the "display:" line with the new WxH, and 60 should resume within about 1 s. Look for a black screen, a stuck image or a crash.
4. **Skirmish.**
   - Open skirmish setup. A short 30 blip is expected.
   - Start a skirmish. "60 FPS off (game reset ...)" must appear before the loading screen, with 0 NEWGAME_GUARD hits.
   - Play 1 minute, then quit to the menu. The menu returns to 60.
5. **Campaign.** Do this once each for the good and the evil campaign: start a campaign mission from the campaign menu or timeline. Same check as in step 4.
6. **Tutorial.** If AotR has it, start one. Same check.
7. **Load.** Load a save from the main menu. Same check, and the game must continue correctly.
8. **New Living World campaign.**
   - Start one. The log must show 30 ("menu sequence", "game starting" or "Living World map transition") before the map loads. Report any "WARN startNewGame with 60 FPS on" line (NEWGAME_GUARD hit).
   - Play a bit, then quit to the menu.
9. **War of the Ring setup screen.** It must show 30 with "Living World map preview". Back out: the menu returns to 60.
10. **Credits.** They must show 30 ("credits"). Scroll speed and music must match stock. Exit: back to 60.
11. **Create-a-Hero.** Open and close it. It must show 30 ("Create-a-Hero" or "game starting"), and the hero must save correctly.
12. **LAN screen.** Open it and back out. It must show "LAN" while open. Also visit the online screen if AotR offers it ("online").
13. **Alt-Tab.** Alt-Tab out and back 5 times in the menu. Expect no fallback, no crash and correct rendering.
14. **Determinism.** With Telemetry=2: spend 2 minutes in the menu at 60, then play the same replay or skirmish seed as a 30-FPS reference. compare_traces must not report a menu-only PASS (menu segments filtered out) and must compare the battle segments.
15. **Default off.** Repeat step 1 with Menus=0. It must show "main menu (Menus=0)", and battles at 60 must behave exactly as before. This also covers the new GATE_GC_DISPMODE site: apply a resolution change from the in-game Options during a 60-FPS battle.

# risks
1. **No visible benefit** (CONFIRMED). The menu at 60 shows each 30-Hz picture twice, so it looks like stock while using up to twice the GPU at 4K. This is why `Menus` defaults to 0 and the user must be told before it is enabled. Real smoothness needs phase 7b (APT display-list interpolation): large, unexplored, and requiring reverse-engineering of the APT renderer.
2. **INFERRED singletons and fields.** LanLobby 0xDE8D90, AptOnline 0xDEA36C and Display+0x38 are INFERRED.
   - A false negative would let a lobby run at 60. The A-only gates still apply, but it would break the single-player-only constraint.
   - A false positive (for example Display+0x38 always nonzero in the menu) would pin the menu at 30. That is safe but makes the feature useless.
   - The step-(c) telemetry line confirms or refutes each one on the first test.
3. **B-render idempotence in mode 9 was not audited per screen.** Verified so far:
   - the shell render callbacks and the image layers have 0 writes;
   - the APT render sync swap is absolute and idempotent;
   - snow (0x648591) is idle with frozen sync.
   Not verified: MainMenu/shell .apt specifics, the W3D texture-mapper internals, and the one-shot '_Init' AS calls from the render callback 0x814BEC (they can fire about 16 ms earlier on a B-render, never twice). The bSeedChanges counter is the runtime check for these.
4. **B-render propagate runs translators on messages that were appended after propagate on A** (UI-sequence runner 0x6324A6, keyboard drain 0x6324BC). This is existing battle behaviour, newly exercised in the menu. A game start or reset raised this way is caught by the reset hook or NEWGAME_GUARD (logic stays exact), but other translator side effects on B have not been audited.
5. **Shell::update throttle.** Shell::update is A-only, and its 32.333 ms timeGetTime throttle can drop an update when the A-to-A spacing measures 32 ms (pacer repayment or early release), leaving a 66 ms gap in menu-screen state steps. The effect is slower only, never 2x. It is measured by shellUpdSkips; if the counter is high, revisit the dropped SHELL_THROTTLE_60 site (26 ms threshold, only while in 60 mode).
6. **Credits entry happens inside an A-render.** GE+0xC becomes 100 before the next pair boundary, so one B half is paced with P = 10 ms (5 ms early). This is harmless and expected once per credits entry.
7. **Resolution change inside an A-render.** 0x645DAD runs a device reset and the shell rebuild. The following B-render draws on the new device in 60 mode, which is the same as the in-game Options path at 60 today. Rebuild guards: "menu transition" while the stack is empty, then "display mode change" via GC+0xC9. Needs the test in step 3.
8. **NEWGAME_GUARD, R5/R6 scoping and the trace filter are new code.**
   - The guard runs C++ from inside startNewGame. It must stay main-thread only and must not touch game state beyond LeaveSixty.
   - The fallback scoping must not hide a real in-battle fallback: only fallbacks triggered while the C0 mode was 9 get cleared.
9. **Short 30-FPS blips** occur on skirmish-setup and LAN entry, where clearGameData resets m_frame, and after every quit to the menu. Each blip adds two mode lines to the log; this is cosmetic.
10. **The m_frame < 8 start guard is load-bearing for the transitions.** It must never be lowered below 7.
11. **Site-count bookkeeping.** The last log reported 144 installed sites against 148 in sites.json. Reconcile this before adding 2 more, or the "all sites installed" gate (g_installed) may be misread.