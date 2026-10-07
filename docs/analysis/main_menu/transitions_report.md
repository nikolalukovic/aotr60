# Menu 60 FPS: transitions and robustness (area "transitions")

Scope: what has to hold so that the shell (main menu and its sub-screens) can run in 60 mode, and so that nothing from
shell 60 mode leaks into the next match. Other areas (APT/shell systems smoothness) are not covered here; the rules
below are what they must respect.

CONFIRMED = read in the binary/source in this pass. INFERRED = reasoned, not proven statically.

## 0. Base facts

- F1 CONFIRMED. AotR has no shell map: `ShellMapOn = No` (<game folder>\aotr\data\ini\gamedata.ini:11133,
  data\ini\object\gamedata.ini:3). Shell::showShellMap 0x75DE01 starts the map (MSG 0x1D + MSG_NEW_GAME(4)) only when
  GD+0xAF0 != 0 (checks at 0x75DE2D and 0x75DE76); otherwise it only sets Shell+0x52=1. So the main menu and all its sub-screens
  run in GameLogic mode 9 (no game). Mode 4 never occurs in AotR. There is no 3D scene behind the menu.
- F2 CONFIRMED. In mode 9 the stepper keeps running: GameLogic::update 0x62E4E8 sees !runnable && !frozen and takes the
  full path (no frame++); the stepper sets GC+0xC8=1 at 0x6325FB, so m_frame (GC+0x10) advances 30/s and the start
  rule (s==1 && GC+0xC8) is met once per tick. Logic frame GL+0x40 stays 0.
- F3 CONFIRMED. Every reset path resets m_frame: GameEngine::reset 0x635D11 -> 0x635D83 TheSubsystemList(0xDE3380)
  ->resetAll 0x5B4783 (vt+0x24 of every subsystem) -> GameClient::reset 0x64782E -> `mov [esi+0x10],ebx` (=0) at
  0x64784B. [0xDE4308] (stock "first 6 frames unlimited" base) is zeroed at 0x77945F (clearGameData) and 0x6315FB
  (startNewGame). Runtime log 2026-10-05 17:25:30 "60 FPS on ... m_frame 10" after >150 menu renders agrees.
- F4 CONFIRMED (frame_ctl.cpp OnEngineReset / LeaveSixty / OnPostRender). A reset inside an A-render sets g_m60=0,
  g_forceHalt is recomputed as 0, so that iteration's step is the stock step: X becomes "stock render k + stock step".
  A reset inside the Y stock step happens after the pair's B-render, so the pair is complete. A reset inside a
  B-render (not expected, see T1) also ends as "B + stock step". All three are logic-exact.
- F5 CONFIRMED (frame_state.cpp BeginIteration). 60 mode starts only at C0 with s==1 && GC+0xC8 && Block==null &&
  StartBlock==null; it stops only before an X iteration (pair boundary). StartBlockReason blocks while m_frame < 8.

## 1. Transitions

| # | Transition | Path (phase) | 60 in shell OK? | How the controller switches | Leak into next match |
|---|---|---|---|---|---|
| 1 | boot -> legal/logo movies -> main menu | Intro is a UI sequence pushed by GameClient::update (0x648482 sets GD+0xAF3, logo movies via Display vt10C "NewLineLogo"/"TolkienLogo" in 0x64838D; last step clears GD+0xAF3 at 0x6457EA). While GD+0xAF2/0xAF3 is set GameClient::update runs only Display draw/update (0x64849E intro branch). | Block (existing "intro" rule). Movies are wall-clock videos. | Starts at the first s==1 tick after AF2/AF3 clear and m_frame>=8. | none |
| 2 | main menu -> skirmish | AptSkirmish start 0x9286D7 (A-render, APT callback): clearGameData 0x9286F0 -> GameEngine::reset -> RESET hook -> LeaveSixty; InitRandom 0x928736; MSG_NEW_GAME 0x928807 (WotR: second clearGameData 0x92879B, MSG 0x1F 0x9287B5). CONFIRMED | 60 is off before the load. | Reset hook immediately (mid-A, exact per F4). Re-entry blocked: m_frame=0 after reset (F3) and MSG_NEW_GAME is processed at the next sub 1 (<= 6 steps), i.e. before m_frame can reach 8. | none (see section 3) |
| 3 | main menu -> campaign | 0x5EAB8F: clearGameData 0x5EABE8, InitRandom 0x5EAC4A, MSG_NEW_GAME 0x5EAC5A. CONFIRMED | same as 2 | same as 2 | none |
| 4 | main menu -> Living World campaign | 0x91B825: clearGameData 0x91B87E, then MSG 0x1F 0x91B8A9 -> logic sub 1: 0x779D36 -> 0x6BE09E -> 0x6B5286 setGameMode(8) + startNewGame(0) at 0x6B5347 (no reset in that chain). CONFIRMED | same as 2 | same as 2; the 0x6B5347 startNewGame is covered by the proposed NEWGAME_GUARD in any case. | none |
| 5 | main menu -> Create-a-Hero | 0x91A018 (A-render): appends MSG 0x1D (0x91A043) and MSG_NEW_GAME(7) (0x91A06C); InitRandom(0) at 0x91A05E in the render phase. CONFIRMED | Menu screen OK until the messages run; CaH itself blocked (mode 7, GD+0xD45). | 0x1D runs at the next sub 1 in the Y stock step -> RESET hook (pair complete, F4) -> startNewGame in the same logic call. | none. Note: the InitRandom at 0x91A05E happens on an A-render with g_m60=1 and is counted by telemetry as a "seed change outside logic in 60 mode" (expected, stock-equivalent). |
| 6 | main menu -> load saved game | AptSaveLoad close 0x818456 / 0x8184AA (A-render) -> GameEngine::reset -> loadGame 0x6DF4D1 -> 0x6DEB9A reset again -> xfer 0x82B8E4 -> startNewGame(1) at 0x82BEED. G5-13. | 60 off before the load. | Reset hook mid-A (exact). The load continues inside the same render with s carrying on (as stock: the click can land on any k=1..6 because X iterations occur at every s). | none; s phase at load is as random as in stock. |
| 7 | in-game -> quit to main menu | exit 0x625E36 queues MSG 0x1D -> logic sub 1 -> clearGameData -> RESET hook. | n/a (leaving a game) | LeaveSixty in the Y step. Shell 60 may start >= 8 frames later (F3). | none (section 3) |
| 8 | in-game -> load / restart / LW battle start/end | unchanged phase-2/6 paths (reset hook, mode-8 rules) | n/a | unchanged. The mode-9 gap inside LW battle start (clearGameData at sub 1, MSG_NEW_GAME processed at the next sub 1) cannot start shell 60: m_frame=0 for <8 frames. | none |
| 9 | options -> resolution / display mode | APT callback (A-render) -> W3D Set_Device_Resolution / Reset_Device 0x522000 (GAP_RESET marker g_gapReset) | yes | No mode change needed. Pacer: next 60 iteration is a gap (no catch-up, no debt). DisplayBlockReason re-evaluates only if hwnd/windowed/refresh/interval change; a resolution-only change keeps the cached refresh (fix R3). | none, if R3/R4 are applied |
| 10 | alt-tab / minimise (exclusive fullscreen) | Win32GameEngine::update 0x44181F parks in `while IsIconic: Sleep(5), serviceOS, TheLAN update` between iterations; on return Present -> D3DERR_DEVICELOST (stub sets g_gapDevLost) -> 0x52267B TestCooperativeLevel -> Reset_Device(1) 0x52268F or Sleep(200) 0x522696. | yes | X/Y parity survives (no C0 during the park). Pacer: lateness > 250 ms is a gap (pacer.cpp:240), device-lost/reset flags force a gap. | none |
| 11 | MpGameSetup with the War-of-the-Ring LivingWorldMap preview | AptMapPreview ctor 0x975659: LW view vt50(1) suspended, vt28(1) active; GL+0x125 stays 0; dtor 0x9753D3 calls vt28(0) when suspended (CONFIRMED). | Block in v1 (reason "Living World map preview"). INFERRED safe to allow later: LW client stepping is A-only and the window draw (vt24 0x49BEFD) is a per-render scene render; lw_present is gated on active && !suspended (lw_present.cpp:135), so the preview would only show 30 Hz motion twice. | Existing GameModeBlockReason LW branch: viewActive && mode != 8 -> blocked; after the screen closes viewActive=0 and the shell rule applies again. | none |
| 12 | LAN lobby / online | TheLAN [0xDE4394] created 0x847F3E (LanLobby init 0x847ED0), cleared 0x8471E8; AptOnline singleton [0xDEA36C] set 0x91E1D6, cleared 0x91DC4C; TheNetwork [0xDE4468] only during MP games. | Block (user scope is SP; lobbies are wall-clock networking, untested) | Next pair boundary after the screen object appears (at most one B-render at 60 with the lobby open, B-renders do no A-only work). | none |
| 13 | credits | Credits start (FUN_0091B5E9): Shell+0x5D=1 at 0x91B6CE, setFramesPerSecondLimit(100) at 0x91B6DC; credits end (separate routine): Shell+0x5D=0 at 0x91B796, setFramesPerSecondLimit(GD+0x28) at 0x91B7AA (CONFIRMED). | Block while GE+0xC != GD+0x28 (stock credits run at 100 FPS; 60 mode would pace 200 renders/s). | One B iteration may be paced at the 100-FPS interval before the pair boundary. | none |
| 14 | replay viewer | playbackFile 0x77F66B: clearGameData -> reset; mode 3 | menu yes, playback blocked (existing) | reset hook | none |
| 15 | quit to desktop from the menu | quit sets the engine quitting flag; the main loop ends after the current iteration. GameEngine/W3DDisplay dtor 0x449861 -> WW3D::Shutdown 0x517AA0 (SP_SHUTDOWN: cancel, g_disabled, timer thread quit). WW3D::Shutdown is otherwise only reached from W3DDisplay::init failure 0x4466B4 and WinMain 0x402D27 - not from a resolution change (CONFIRMED). | yes | Nothing to do; no render after shutdown. Split present never defers in mode 9 (StateGate kGateLwMap). | n/a |

## 2. Predicate rules (frame_ctl.cpp)

```
// GameModeBlockReason: first branch
if (mode != 8 && !viewActive && !GL+0x125) {
    if (mode == 0 || mode == 2 || mode == 6) return nullptr;
    if (mode == 9) return ShellBlockReason(ge, gd);          // NEW
    return "game mode (shell map, replay, Create-a-Hero or multiplayer)";
}
// LW branch: when mode == 9 and the LW view is active+suspended, return "Living World map preview" (v1: blocked)

const char* ShellBlockReason(ge, gd)                          // NEW, mode 9 only
{
    if (!g_cfg.menus)                                return "menus (Menus=0)";          // new INI key, default 1
    if (Ptr(0xDE4394))                               return "LAN lobby";                // TheLAN
    if (Ptr(0xDEA36C))                               return "online";                   // AptOnline singleton
    if (Field<int32_t>(ge,0xC) != Field<int32_t>(gd,0x28)) return "menu frame rate changed (credits)";
    return nullptr;
}
```

Everything else in BlockReason stays and applies to the shell too (TheNetwork, GL+0x114 in {1,2}, GL+0x9D, replay,
GD+0xBBD, GD+0xD45, intro GD+0xAF2/0xAF3, UseFPSLimit, script debugger, LOD, display). StartBlockReason is unchanged;
its `m_frame < 8` threshold is now load-bearing for transitions 2-8 (a reset is followed by <= 6 steps until the
queued MSG_NEW_GAME / 0x1F runs) and must not be lowered below 7. No UI-sequence block is needed in the shell: the
runner 0x80000F is A-only (GATE_CU_UISEQ) and the intro sequence is covered by AF2/AF3.

## 3. Leak audit (shell 60 -> next match)

Game state (must be stock):
- m_frame, [0xDE4308]: reset by the engine (F3). GE+0x38/0x40/0x44/0x48/0x4C..0x5C: reset 0x635DC7..0x635E10.
- GE+0x34 s: not reset by GameEngine::reset (G5 (5)); in stock its phase at a load depends on click time; shell 60
  has X iterations at every s, so the same set of phases occurs. No new state.
- W3D sync clock 0xDC7580: owed half flushed by LeaveSixty (FlushOwedSync) before anything else runs. Exact.
- Logic RNG [0xDA1CA4]: every new-game starter reseeds (0x6D3261 callers 0x5EAB8F, 0x612274, 0x9286D7, 0x91A018,
  0x75DE01, MP starters); boot seed = time() (0x6D34E6). Loads: only types 4/6 reseed (0x6DEC46 -> 0x6D321C(0)); no
  xfer of the seed besides the CRC path 0x6D320A (INFERRED: tactical saves do not restore it). So a load from the menu
  continues the RNG state left by the shell, as in stock. Requirement: shell B-renders must not consume the logic RNG
  (A-renders consume it at the stock 30/s). Verify with telemetry (V3).
- Commands: messages appended by APT (0x63244B) and GameClient::update (0x632498) are propagated in the same render
  by 0x7128C3 (0x6324A1), as stock. Messages appended after propagation (UI-sequence runner 0x6324A6 and later) are
  propagated by the B-render, i.e. one step earlier than stock. For shell transitions this only shifts when a new game
  starts (no determinism impact); noted for the in-game owners.

DLL state:
- Cleared by LeaveSixty/OnEngineReset: g_fs, owed sync, presentation windows, camera swap, LW restore, C5 table,
  camera history, split present, pacer schedule/window, telemetry tick window ("# reset"). CONFIRMED in code.
- NOT cleared today, must be scoped (fixes R5, R6): pacer fallback (g_fallbackUntil, FallbackPolicy::fallbacks_) and
  g_unknownPath (sticky, session-wide). A shell-triggered fallback or vt+0x188 path would otherwise disable 60 in the
  next match.
- Split present g_disabled (sticky) is reachable only via SpShutdown (exit) or a seed change across a deferred
  Present; split present never defers in mode 9 (StateGate kGateLwMap). No leak.

## 4. Hooks and code changes

- H1 NEWGAME_GUARD (new site, 0x6314CD GameLogic::startNewGame entry, see site table): if 60 mode is on, leave it
  (same body as OnEngineReset minus the trace marker), count and log `WARN new game without engine reset`. Covers
  any path that reaches startNewGame (callers 0x6B5347 LW map start, 0x779DEE MSG_NEW_GAME, 0x82BEED load) without the
  reset hook. Expected count 0; it is a backstop that keeps the "no 60 state survives into a new game" property
  independent of the m_frame<8 argument.
- R2 GAP_RESET_CAVE (existing site, stub change only): add `lock inc dword ptr [g_devResetGen]` (EFLAGS dead at
  0x522000). C0 consumes it at the next X boundary (or in 30 mode): CameraReset() and the LW present history reset, so
  an A-render never blends camera/view planes across a device reset (aspect change).
- R3 DisplayBlockReason signature: add BackBufferWidth/Height (0xDD2FF8/0xDD2FFC). A resolution-only change in
  exclusive fullscreen (refresh 0 = mode default) then re-queries the refresh once.
- R4 Device lost: optional block "device lost" while g_gapDevLost was set by the last Present and no successful
  Present followed (each lost-state render sleeps 200 ms at 0x522696; at 60 that is two sleeps per pair).
- R5 Pacer fallback scope: do not evaluate fallback windows while the shell rule is the reason 60 is allowed (or tag
  g_fallbackUntil with its context and let PacerFallbackActive(context) ignore the other context).
- R6 g_unknownPath scope: record the context of the bit; clear a shell-scoped bit in OnEngineReset/NEWGAME_GUARD. The
  stub always takes the stock path for the offending call, so re-arming is exact.
- R7 Telemetry: (a) TraceLogicCall writes nothing in mode 9 (and 4); compare_traces.py ignores segments whose logic
  frames never exceed 30 - otherwise a 60-FPS menu segment can pair with a 30-FPS menu segment of equal seed and
  report a false PASS (main() picks the first share60 >= 0.5 segment with a seed partner; 2-arg mode picks the
  longest segment). (b) separate per-tick site sums for the shell. (c) counters: resets inside a B-render (T1),
  seed changes on B-renders in mode 9 (V3), NEWGAME_GUARD hits.
- T1 cross-area rule for the APT/shell owners: code that can call clearGameData / GameEngine::reset / loadGame /
  startNewGame / setFramesPerSecondLimit / Reset_Device, create TheLAN/AptOnline, or append logic messages must run on
  A-renders only (today: APT 0x632449, WM 0x64863A, Shell 0x648891, UISEQ 0x6324A6 are A-only). If APT update is
  ever run on B-renders for smoothness, input/callback delivery must stay A-only. Even then F4 keeps it logic-exact.

## 5. Verification (runtime, Telemetry=1/2)

- V1 log per switch: reason strings "LAN lobby", "online", "credits", "Living World map preview", "first frames".
- V2 menu -> skirmish / campaign / LW / CaH / load / replay: "60 FPS off (game reset ...)" logged before any load
  stall; NEWGAME_GUARD hits = 0; match starts with "first frames of a game" then "60 FPS on".
- V3 in mode 9: 0 logic-seed changes on B-renders; tick invariant 12/6/6 with 0 errors.
- V4 quit to menu: shell 60 resumes >= 8 frames after the reset; LW quit leaves viewActive=0 (else reason
  "Living World map preview/transition" persists in the menu - INFERRED that the LW client reset deactivates it).
- V5 options resolution change in the menu and in a paused game: gap logged, display line re-logged with new WxH,
  no fallback.
- V6 alt-tab x10 in the menu at 4K exclusive fullscreen: no fallback, no stall-debt windows, parity intact.
- V7 determinism: skirmish replay recorded after menu time at 60 vs played at 30 (compare_traces with R7a).
