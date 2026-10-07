# AREA modes

## SUMMARY

No new byte patches are needed for the shell. The only changes are in src/frame_ctl.cpp (plus a config key).

The AotR main menu has no shell map. ShellMapOn = No in both data\ini\gamedata.ini:11133 and rotwk INI.big. Shell::showShellMap 0x75DE01 sends MSG_NEW_GAME(4) only when GD+0xAF0 is set, so GL+0x110 = 4 never occurs in AotR.

Every shell screen runs in GL+0x110 = 9, which means no game is loaded:
- The stepper still runs subs 1..6.
- GameLogic::update sub 1 takes the non-runnable, non-frozen path, so GL+0x40 stays 0.
- GC+0xC8 is 1 after every stock step, so m_frame advances at 30/s.
- On screen: APT (MainMenu.apt and the other .apt screens), a static display overlay image (ShellMapLowLOD = InstallLoad.tga) and shell music.

The fix has four parts:
1. A mode-9 branch, ShellBlockReason, in GameModeBlockReason. It allows 60 FPS when all of these hold:
   - the intro is done;
   - the shell is active;
   - no Living World view is active;
   - no UI sequence is queued;
   - the credits are not open and the FPS limit is the stock one;
   - no LAN lobby is open;
   - no game-start message (0x1D/0x1E/0x1F) is pending in TheMessageStream or TheCommandList.
2. A shell exit guard in OnPostRender. After every 60-mode A-render that began in mode 9, it re-evaluates the predicate and leaves 60 mode at once, exactly as the reset hook does. This covers exits that bypass GameEngine::reset: starting a new Living World campaign (MSG 0x1F), the credits switching the FPS limit to 100, and display-mode changes.
3. StartBlockReason, the pacer and the reset hook stay as they are.
4. Create-a-Hero (mode 7), replays (3), multiplayer (1/5) and the shell-map mode (4) keep their 30 FPS reasons. The Living World map preview, credits, LAN/online screens, intro/legal movies and transitions also stay at 30, each with its own reason.

## DETAILS

## 1. Engine state per screen

Key: CONFIRMED = static disassembly or runtime log; INFERRED = deduced but not proven.

**Runtime evidence (CONFIRMED)** from %APPDATA%\Age of the Ring\aotr60\aotr60.log:
- 17:25:03.991 "60 FPS not available: game mode (menu or multiplayer)". So in the menu GL+0x110 is not in {0,2,6}, the Living World (LW) view is inactive and GL+0x125 = 0.
- The menu stall lines read "logic 0.0 (sub 2..6, frame 0)". So the stepper runs and GL+0x40 stays 0.

**Common to every shell screen:**
- GL+0x110 = 9 (ctor 0x6301C9; clearGameData 0x7793C5). There is no mode 4 because ShellMapOn = No (0x75DE2D..0x75DE3C, 0x75DEE2). [CONFIRMED]
- GL+0x114: 3 (not LW) or 0 after a single-player LW campaign. GL+0x9D = 0. GL+0x125 = 0: its writers are 0x6C0069 and 0x6DED03, and GameLogic::reset at 0x62D285 clears it. [CONFIRMED]
- GL+0x124 is 0 except while Display::playMovie 0x65D3F5 runs. That function has its own blocking loop, so C0 does not run during it. [CONFIRMED]
- TheNetwork [0xDE4468] = NULL. It is created only at a LAN or online game start (G5-02). [CONFIRMED]
- GD+0xAF2 / +0xAF3 = 0 after the intro. The 0x64576D UI step clears AF2 at 0x645793 and AF3 at 0x6457EA, after showShellMap(1,0) and showShell(1). [CONFIRMED]
- GD+0xD45 = 0 except in Create-a-Hero. GD+0xBBD = 0. GD+0x26 (UseFPSLimit) = 1 (aotr ini:11137; parse table 0xBFF600 gives offset 0x26). GD+0x28 = 30. [CONFIRMED]
- **TheShell is [0xDE7890]** (vtbl 0xC2C894; ctor 0x75DC29; created at 0x645F08 / 0x646DF6). Fields:
  - +0x5C = isShellActive: set to 1 at 0x75E4D4 (showShell 0x75E43C), set to 0 at 0x75DBEC (hideShell 0x75DBB9). [CONFIRMED]
  - +0x4C = screen count, max 16 (push 0x75D9F8). Screen stack at +0xC; top = [+8 + count*4] (0x75D9EB). [CONFIRMED]
  - +0x52 = "show the substitute background image": showShellMap sets it to !ShellMapOn at 0x75DE3C. [CONFIRMED]
  - +0x5D = credits / suspended: set at 0x91B6CE, cleared at 0x91B796. [CONFIRMED]
  - +0x6C = shell music enabled. [CONFIRMED]
- No shell map is loaded and no GameLogic game runs. The stepper still calls GameLogic::update for subs 1..6 and TheLivingWorldLogic::update at sub 1 (0x632A79..0x632A92). [CONFIRMED]
- GC+0xC8 is set to 1 at 0x6325FB on every non-halted iteration and is not cleared by the non-frozen sub-1 path. As a result, m_frame++ at 0x632423..0x632440 runs every stock iteration, and the start condition s==1 && C8 can be met in the shell. [CONFIRMED]
- Shell::update 0x75E1D3 is throttled to at least 32.33 ms of wall time ([0xC2C90C]). It is A-only through GATE_GC_SHELL. [CONFIRMED]

**Per screen:**
- **Startup, legal and intro movies.** AF2 = 1 from the ctor (0x64309B). The first-run UI sequence (queued in GameClient::update) runs:
  1. 0x645B8D / 0x64838D: NewLineLogo, TolkienLogo and Overall_Game_Intro via Display vt+0x10C; AF3 = 1 at 0x648482.
  2. 0x645BDF: TitleScreenLogo in a 5 s timeGetTime + Sleep(100) blocking loop. This is the 5202 ms render stall in the log.
  3. 0x64576D: clears both flags and shows the shell.

  GameClient::update takes the intro branch (0x648620..0x648634 → 0x6488BD). **Stays at 30** with reason "intro". [CONFIRMED]
- **Main menu** (MainMenu.apt, pushed by 0x75E43C when ShellMapOn = 0 and the stack is empty): mode 9, shell +0x5C = 1, count ≥ 1. **60.**
- **Options** (Options.apt 0x91ED91): mode 9. **60.** Applying a new resolution:
  - 0x91EF0F → 0x6455F3 sets GC+0xC9/0xCA. The APT handler runs inside the A-only APT update.
  - FUN_00645DAD (called after Shell::update at 0x6488A1, on the same A-render) deletes TheShell, calls Display vt+0x58 (mode set, device reset), writes GD+0x30/0x34 and recreates the shell.
  - The exit guard (part 3 of the spec) leaves 60 on that A-render.
- **Skirmish setup** (Skirmish.apt 0x9282E3): main-menu cases 7/8 call clearGameData 0x7792BC first, so the reset hook fires and 60 switches off briefly before coming back on. Mode 9. **60.**
- **Campaign menu / timeline** (CampaignMenu.apt 0x927EC6, TimeLine.apt 0x927898): mode 9. **60.**
- **Load / save game** (SaveLoad.apt 0x816655): mode 9. **60.** Loading goes through loadGame 0x6DEB9A, which calls GameEngine::reset, so the reset hook fires.
- **New Living World campaign:** main-menu cases 0xB..0xE → 0x91C108 sets the difficulty in LWL+0xEC and pushes UI-sequence steps → MSG 0x1F → 0x6BE09E → 0x6B5286, which calls setGameMode(8) and startNewGame. **GameEngine::reset is NOT called on this path.** It is handled by the exit guard. [CONFIRMED static]
- **War of the Ring setup (MpGameSetup) with the embedded LivingWorldMap window:**
  - AptMapPreview 0x975659 calls vt50(1) then vt28(1), so LW view +0x18 = 1, +0x19 = 1 and GL+0x125 = 0.
  - **Stays at 30** with reason "Living World map preview". Its vt24 draw 0x49BEFD has no presentation window, and the A/B scheme has not been audited for the preview.
- **Credits:** 0x91B5E9 creates TheCredits [0xDEBF50], sets shell +0x5D = 1 and calls setFramesPerSecondLimit(100) at 0x91B6DC. Exit is 0x91B6FD: deletes TheCredits, clears +0x5D at 0x91B796, restores GD+0x28 at 0x91B7AA. The credits scroll (TheCredits vt+0x28 in main-menu update state 4, 0x91C631) runs once per APT update. **Stays at 30** (a 100 FPS limit would make the pacer use T = 5 ms). [CONFIRMED]
- **LAN:** main-menu case 6 → UI sequence → clearGameData + LanLobby.apt 0x847152. TheLAN [0xDE4394] is created in 0x9131DB and cleared at 0x8471E8 in the LanLobby destructor 0x847189. **Stays at 30** with reason "LAN". [CONFIRMED creation and clear]
- **Online:** OnlineShell.apt 0x91D7A3 (screen pointer [0xDEA36C]; only a zero store was found, at 0x91E1D6). INFERRED; TheNetwork, GL+0x114 and GL+0x9D still catch real online games. Optional reason "online".
- **Create-a-Hero:** clearGameData (reset hook fires), then MSG_NEW_GAME(7) at 0x91A06C plus GD+0xD45 = 1 at 0x91A7A4. **Stays at 30** with reason "Create-a-Hero".
- **Replays:** mode 3. **Stays at 30.**
- **Loading screens:** the load screen 0x758084 has its own real-time loop inside startNewGame, so no C0 runs. The fade after the load (GL+0x9C) is in-game. Not affected.
- **Campaign movies:** played by the blocking playMovie, so no C0 runs. [CONFIRMED]

## 2. frame_ctl.cpp specification

**New constants (runtime.h):**
- kTheShell = 0xDE7890
- kTheCredits = 0xDEBF50
- kTheLan = 0xDE4394
- kTheMessageStream = 0xDE6398
- kTheCommandList = 0xDE639C
- optional: kOnlineShellScreen = 0xDEA36C (INFERRED)

**New config key:** `Menus = 1`, parsed like LivingWorldMap.

**GameStartPending()** (read-only, main thread). Walk both message lists:

```cpp
for (lst : {stream, cmdlist})
  for (m = [lst + 0xC], n = 0; m && n < 512; m = [m + 4], ++n)
    if ([m + 0x10] in {0x1D, 0x1E, 0x1F}) return true;
```

The layout is CONFIRMED by containsMessageOfType 0x710E2D (head +0xC, next +4, type +0x10) and by propagate 0x7128C3.

**ShellBlockReason(ge, gl, gd)**, checks in this order:

```cpp
if (GD+0xAF2 || GD+0xAF3)                return "intro";
if (!cfg.menus)                          return "menu (Menus=0)";
shell = Ptr(kTheShell);
if (!shell || !shell+0x5C || int(shell+0x4C) <= 0) return "menu transition";
view = Ptr(kLwView);
if (GL+0x125 || (view && view+0x18))
    return (view && view+0x19) ? "Living World map preview" : "Living World map transition";
if (Ptr(kUiSequenceQueue))               return "menu sequence";
if (Ptr(kTheCredits) || shell+0x5D)      return "credits";
if (int(GE+0xC) != int(GD+0x28))         return "FPS limit changed";
if (Ptr(kTheLan))                        return "LAN";
// optional: if (Ptr(kOnlineShellScreen)) return "online";
if (GameStartPending())                  return "game starting";
return nullptr;
```

Notes on these checks:
- Do NOT test LWL+0xB4. It is set only at 0x6B9033 and cleared only in the ctor 0x6BA054 and the destructor 0x6BA50E, so it stays set for the whole session after any LW campaign. TheLivingWorldLogic::update then still runs in the stock step only, which is safe.
- The LW view's +0x18 is cleared by clearGameData's vt28(0).

**GameModeBlockReason(ge, gl, gd):** start with:

```cpp
mode = GL+0x110;
if (mode == 9) return ShellBlockReason(...);
```

Keep the LW branch unchanged. Replace the catch-all "game mode (menu or multiplayer)" with:
- 1 or 5 → "multiplayer"
- 3 → "replay"
- 4 → "shell map"
- 7 → "Create-a-Hero"
- anything else → "game mode"

BlockReason passes ge and gd and keeps all its generic checks after this one.

**StartBlockReason:** unchanged.
- m_frame ≥ 8 is reachable because m_frame advances at 30/s in the shell. It is still needed because clearGameData zeroes [0xDE4308] at 0x77945F, and the limiter at 0x63A17B..0x63A188 runs unlimited until m_frame ≥ [0xDE4308] + 6.
- GL+0x9C / 0xA8 = 0 in the shell: 0xA8 is cleared by setGameMode, 0x9C by 0x62B385. INFERRED.
- TV+0x23D4 ≤ 1 (reset at 0x48B2B4). INFERRED.
- The switch-on still requires s==1 && C8 at C0, which is already satisfied in the shell.

**OnPostRender shell exit guard:**
- At C0, record `g_shellIter = (GL+0x110 == 9)`, the TheShell pointer, and the display signature (D3DPRESENT_PARAMETERS at 0xDD3014..0xDD302C).
- In OnPostRender, after CloseWindowsSafetyNet and before computing g_forceHalt:

```cpp
if (g_fs.m60 && g_fs.uiTick /* this render was A */ && g_shellIter) {
    r = BlockReason();
    if (!r && (Ptr(kTheShell) != shellAtC0 || displaySignatureChanged))
        r = "display mode change";
    if (r) { g_fs.Reset(); LeaveSixty(r); }
}
```

- These are the same semantics as OnEngineReset, which already switches off inside a render. The halt reads the live flag, so this iteration becomes stock render k plus the stock step. LeaveSixty flushes the owed sync, so the sync clock is exact at logic time; it also calls PacerOnModeChange(false), and the stock limiter continues from [0xDE4318].
- All shell state changes come from A-only code (APT 0x632449, GameClient::update 0x632498, UI sequence 0x6324A6, WindowManager), so checking after A-renders only is enough. A switch-off after a B-render would be pair-aligned anyway.
- Why the message stream is checked: a UI-sequence step that runs at 0x6324A6 appends to TheMessageStream after propagate 0x6324A1. The message would otherwise reach TheCommandList only on the B-render and be processed in the Y-step logic while 60 mode is still on.

**Pacer:** no change.
- The shell has GE+0xC = 30, so P = 33 and T = 16.5 ms.
- The credits enter at 100 FPS (0x91B6DC) happens inside the A-only APT callback; the guard switches off before the B iteration is paced.
- ScriptEngine reset at 0x6096B1 restores GD+0x28.
- In-game script FPS changes keep the existing behaviour.
- Split present stays off in the shell: its gate rejects mode 9 (split_present.cpp:611).

**Reset hook / LeaveSixty:** no change. It fires on:
- game → shell: quit is MSG 0x1D at sub 1 → clearGameData → GameEngine::reset. Log 17:31:26.886.
- shell → game for skirmish, campaign, tutorial, Create-a-Hero, replay and load.
- shell → shell: the clearGameData in skirmish-setup and LAN entry causes a ≤ 0.3 s 30 FPS blip.

After any switch-off, 60 cannot return before the sub-1 step that processes the pending message, because of s==1 && C8. The exit guard covers the one path without a reset (MSG 0x1F), plus credits and display changes.

**Title bar** (via UpdateTitle): "[AotR60: 60 FPS, smooth]" in the menu. Otherwise "30 FPS, …" with one of: intro, menu transition, menu sequence, credits, FPS limit changed, LAN, Living World map preview, Living World map transition, game starting, Create-a-Hero, replay, multiplayer.

Scratch dumps are in <analysis workspace>/menu\shell-states\: gc_update.c, shell_fns.c, mainmenu_update.c, ft.py.

## RISKS

1. **The menu may not look smoother.** Allowing 60 in the shell gives 60 presents/s, but the content still changes at 30 Hz. APT (TheAptPlayer update, GATE_CU_APT) and the mouse (GATE_GC_MOUSESEL) stay A-only, so APT animation steps and a software cursor would show each step twice. APT uses wall-clock dt: 0x624F3D..0x624F64 clamps it to ≤ 60 ms, and to ≥ 34 ms only on the one-shot flag +0x328. Real smoothness needs the APT/cursor areas' work; this predicate alone is only the safety layer.
2. **INFERRED items to confirm at runtime.** Log mode, shell +0x5C/+0x4C/+0x5D, LW view +0x18/+0x19, the UI-sequence head, GE+0xC, TheLAN, GL+0x9C/0xA8, TV+0x23D4 and m_frame on every block-reason change. The items:
   - GL+0x9C/0xA8 and TV+0x23D4 are clear in the shell after quitting a game.
   - m_frame is reset by GameEngine::reset.
   - [0xDEA36C] is non-null only while OnlineShell is open.
   - ScriptEngine reset restores the FPS limit on quit.
3. **Mid-pair switch-off from OnPostRender** is new code. It mirrors the existing OnEngineReset path, but needs a check that the iteration really completes as a stock one: the next tick check must be 30-mode exact and owed sync flushed once.
4. **The resolution change** (0x645DAD) deletes and recreates TheShell and resets the device inside an A-render. The guard switches off right after, but the A-render's draw on the new device still runs with 60-mode state (the same as this path already behaves in battles at 60 today). Test it.
5. If LWL+0xB4/+0xB5 remain set after quitting an LW campaign, TheLivingWorldLogic::update keeps running LW logic in the menu. This is stock behaviour: it runs only in the stock step, and its RNG use stays at stock cadence.
6. The skirmish-setup and LAN entries cause a short 30 FPS blip (reset hook) and an extra pair of mode log lines.

## RECOMMENDATION

Make these DLL-only changes. No new patch sites and no stub changes are needed.
1. Add ShellBlockReason and GameStartPending, and branch mode 9 into ShellBlockReason at the top of GameModeBlockReason, with refined reasons for modes 1/3/4/5/7.
2. Add the OnPostRender shell exit guard: after an A-render in an iteration that began in mode 9, re-run BlockReason and also check for a TheShell or display-parameter change. If anything blocks, call g_fs.Reset() and LeaveSixty(reason).
3. Add `Menus = 1` to aotr60.ini, and update the README, which still says "Menus … stay at the stock 30 FPS".
4. Keep StartBlockReason, the pacer and the reset hook unchanged.
5. Leave these at 30: Create-a-Hero, replays, multiplayer/LAN/online, credits, intro movies, the LW map preview in MpGameSetup and all menu-to-game transitions.

Test checklist with Telemetry=1:
- idle in the main menu: 0 tick errors, 12 renders per tick;
- options, including a resolution change;
- skirmish setup, then start a skirmish;
- quit back to the menu;
- credits in and out (the title must show 30 with reason "credits" during the credits);
- start a new Living World campaign (the log should show "60 FPS off (game starting/menu sequence)" before MSG 0x1F is processed);
- load a save from the menu;
- open Create-a-Hero and the LAN screen.

Coordinate with the APT and cursor areas so the menu content actually advances at 60.

## VERIFIER
The analysis is mostly right about how the engine behaves, and its safety predicate is a reasonable starting point. But it does not deliver what the user asked for, and some of its claims are wrong or unproven.

**What checks out:**
- The AotR shell runs in GL+0x110 = 9 with no shell map. ShellMapOn = No sits at GD+0xAF0 (parse table 0xC00510), and 0x75DE2D..0x75DEE2 sends MSG_NEW_GAME(4) only when AF0 is set.
- The stepper cycles subs 1..6 in the shell, so GC+0xC8 = 1 and m_frame advances there.
- The intro, credits, FPS-limit, message-list and LW-preview mechanics are as described.
- Split present already refuses mode 9 (split_present.cpp:611-613).

**The main problem: the goal is not met.** Nothing in this main menu changes more than 30 times per second:
- MainMenu.apt is a 30 fps timeline (Movie header at 0x500: 1024x768, MillisecondsPerFrame = 0x21 = 33 ms).
- Shell::update only runs after 32.33 ms of wall time ([0xC2C90C]).
- APT, Bink windows (0x814218, called only from the APT update 0x624EE1) and window transitions are A-only.
- The backdrop is a static image (ShellMapLowLOD, a MappedImage of InstallLoad.tga).
- The cursors are .cur files, so most likely an OS hardware cursor (INFERRED).

So the predicate alone gives 60 presents per second showing about 30 distinct images: the title bar says 60 but nothing looks smoother, and it adds risk. Running APT on every render would also not help, because its timeline frames are 33 ms.

**Errors in the spec:**
- TheShell+0x5D is not a credits flag. Display 0x65BE44 shows it means "hide the 3D views while a shell game (mode 4/7) runs". It is set by at least 5 sites (0x918B38 AptCampaignReview Continue, 0x91A45D, 0x91ACFE, 0x91B55C, 0x91B6CE). Only two clears were found (0x91A3C2, 0x91B796). In mode 9 nothing needs it cleared, so it can stay 1 and block 60 permanently with the wrong reason ("credits").
- The "display signature" range 0xDD3014..0xDD302C leaves out BackBufferWidth/Height at 0xDD2FF8/0xDD2FFC, so a pure resolution change is not seen.
- Comparing TheShell pointers to catch the shell being recreated is unreliable: 0x645DAD deletes and immediately reallocates an object of the same size, so the address can be reused.
- Smaller points:
  - TheLAN has another writer, 0x847F3E.
  - Create-a-Hero does not call clearGameData directly. 0x91A018 appends MSG 0x1D and then MSG 0x1E(7), so the reset hook fires later, in the sub-1 logic step.

**Unproven:**
- No one has shown that B-renders in mode 9 are idempotent: W3DDisplay::draw with no game, the display overlay slots at 0x65C42C (state 2), transition drawing, APT render.
- The mid-pair exit in OnPostRender (pacer, owed sync, stock limiter) has not been tested at runtime.

**Missing for a safe 60 FPS menu:**
- An audit of the mode-9 B-render.
- A decision on whether a 60 FPS menu brings any visible benefit at all.
- Without that benefit, the honest answer to the user is that the menu cannot look smoother than 30 FPS. That rules out shipping a cosmetic 60 that only changes the title bar.

- [confirmed] The AotR shell has no shell map; ShellMapOn=No means GL+0x110=4 never occurs and every shell screen is mode 9
  aotr data/ini/gamedata.ini:11133 says ShellMapOn = No. The GlobalData parse entry at 0xC00510 maps 'ShellMapOn' (0xC027E4) to offset 0xAF0. In Shell::showShellMap, 0x75DE76 cmp [GD+0xAF0] / je 0x75DEF2 skips the MSG 0x1E + 0x7111E5(4) at 0x75DEE2. Mode 9 is written by the GameLogic ctor at 0x6301C9 and by clearGameData 0x7792BC (decompiled: +0x110 = 9 right after GameEngine vt24). Runtime support: the quit at 17:31:27 shows only 230 ms of sub-1 logic, so no shell map was loaded. CONFIRMED statically; the 'mode 9 on every screen' part is INFERRED from those writers.

- [confirmed] In the shell the stepper runs subs 1..6, GC+0xC8 stays 1 after sub 1 (m_frame advances at 30/s) and GL+0x40 stays 0
  0x6325F8..0x6325FB sets C8=1 on the non-halted path. In GameLogic::update 0x62E4E8 sub 1, C8 is cleared only on the frozen path (TV vtD8/vt78/0x60342F); the non-runnable, non-frozen path falls through to 0x62E59A without touching C8. GL+0x40++ happens only when 0x625130 (runnable) is true. Runtime: the shell stall lines show subs 2, 4, 5 and 6 with 'frame 0' (telemetry.cpp:699 reads GL+0x40). The stepper 0x6325A0 would stay on sub 1 forever if C8 were 0 after sub 1.

- [refuted] MainMenu.apt and the shell show more than 30 Hz of content once 60 mode is allowed; the predicate is the safety layer and the APT/cursor areas supply the smoothness
  The analysis itself admits (risk 1) that nothing gets smoother, but its recommendation treats smoothness as reachable by other areas. Evidence that it is not:
- AotR MainMenu.apt Movie header (offset from MainMenu.const = 0x500): 17 frames, 1024x768, MillisecondsPerFrame 0x21 = 33 ms, so the timeline is 30 fps. APT update 0x624EE1 passes wall-clock dt (clamped to ≤60 ms) to 0xAE3150.
- Shell::update 0x75E1D3 runs screen updates only after ≥32.33 ms ([0xC2C90C] = 32.333).
- Bink windows update only through 0x814218, whose only caller is APT update 0x624EE1, so they are A-only.
- The backdrop is a static MappedImage (ShellMapLowLOD = InstallLoad.tga) placed via 0x65C42C.
- mouse.ini uses .cur cursors, most likely the OS hardware cursor (INFERRED).
Result: 60 presents per second with about 30 distinct images, i.e. no visible gain.

- [refuted] TheShell [0xDE7890] +0x5D is the credits/suspended flag (set 0x91B6CE, cleared 0x91B796); ShellBlockReason can return 'credits' when it is set
  Its reader 0x65BE44 skips view drawing only when isInShellGame (mode 4|7) && +0x5D, so it means 'hide the 3D views in a shell game'. Other setters found in listing.asm:
- 0x918B38 (AptCampaignReview 'Continue' callback 0x918B2F, registered at 0x918E24)
- 0x91A45D (0x91A3A9, CaH)
- 0x91ACFE (after showShellMap at 0x91ACC2)
- 0x91B55C (0x91B557, near the credits path)
- 0x91B6CE
Only two clears were found: 0x91A3C2 and 0x91B796. In mode 9 nothing requires it to be 0, so it can stay 1 after Create-a-Hero or a campaign review and block 60 for the rest of the session. Test TheCredits [0xDEBF50] instead; its only writers are 0x91B635 (create) and 0x91B72D (clear).

- [confirmed] Credits: setFramesPerSecondLimit(100) at 0x91B6DC, restore GD+0x28 at 0x91B7AA; GE+0xC != GD+0x28 detects it
  0x91B6DA push 0x64 / call [GE vt+0x48]. vt+0x48 = 0xBD8528 -> 0x66F0FD 'mov [ecx+0xC], eax'. Exit: 0x91B796 clears +0x5D, then 0x91B7A5 push [GD+0x28] / call vt+0x48 at 0x91B7AA. The pacer reads GE+0xC (pacer.cpp:193). The credits scroll is TheCredits vt28 in main-menu state 4 (0x91C631).

- [confirmed] Starting a new LW campaign (MSG 0x1F -> 0x6BE09E -> 0x6B5286) does not call GameEngine::reset
  In logicMessageDispatcher 0x779A3D, the 0x1F case calls 0x628650, then 0x6BE09E. 0x6BE09E calls 0x6B5286 -> 0x77948E(8,1,0) (sets mode, then hideShell) and startNewGame 0x6314CD. GameEngine::reset (0x44181A -> 0x635D11) is reached only through vt+0x24, which has 4 call sites: 0x6DEA25 loadGame, 0x7792BC clearGameData, 0x8182CF AptSaveLoad, 0x91C2FC main menu. None of them is on this path. No direct callers exist.

- [uncertain] Shell to game for skirmish/campaign/tutorial/Create-a-Hero/replay passes through clearGameData, so the reset hook fires
  The MSG 0x1E handler itself (0x77948E + 0x6314CD) does not reset. The senders mostly do:
- skirmish 0x9286D7 calls 0x7792BC first;
- campaign 0x5EAB8F calls it only on one branch;
- tutorial 0x91B825 and replay 0x77F66B call it conditionally.
Create-a-Hero 0x91A018 does not call it. It appends MSG 0x1D and then MSG 0x1E(7) (0x91A041 and 0x91A06C), so the reset fires only when the logic step processes 0x1D. Including 0x1D/0x1E/0x1F in GameStartPending covers this, but the blanket claim is overstated.

- [confirmed] Message list layout: head +0xC, next +4, type +0x10; TheMessageStream 0xDE6398, TheCommandList 0xDE639C
  0x710E2D: mov eax,[ecx+0xC]; loop cmp [eax+0x10],arg; mov eax,[eax+4]. GameLogic::update calls [0xDE639C] vt+0x44 (0x1D). Appends use [0xDE6398] vt+0x48 (0x75DEE2, 0x91A043). propagate 0x7128C3 walks the same +0xC/+4 chain.

- [confirmed] Intro flags: AF2 cleared at 0x645793, AF3 at 0x6457EA after showShellMap(1,0)/showShell(1); GameClient::update intro branch 0x648620..0x648634
  Disassembled 0x64576D..0x6457F4: 0x645793 mov [GD+0xAF2],bl. Then 0x6457CA showShellMap(1,0), 0x6457D7 showShell(1), shell+0x6C=1, and 0x6457EA mov [GD+0xAF3],bl. 0x648620/0x64862D jump to 0x6488BD. BlockReason already tests AF2/AF3 (frame_ctl.cpp:186), so the check in ShellBlockReason is redundant but harmless.

- [confirmed] Shell::update 0x75E1D3 is wall-clock throttled (≥32.33 ms) and A-only via GATE_GC_SHELL
  [0xC2C90C] = 32.3333f is compared with timeGetTime deltas, and sites.json GATE_GC_SHELL gates the call at 0x648893. Not mentioned in the analysis: Shell::update is also reached from the logic step through TheLivingWorldLogic::update (0x6BE52B), in stock steps only.

Risk the analysis missed: when the update is A-only at a nominal 33 ms spacing, an A-render that comes early because of pacer jitter or 1 ms timeGetTime rounding (a measured gap of 32 ms is below 32.33) skips that screen update, so the update arrives 66 ms after the previous one.

- [confirmed] LW map preview in MpGameSetup (AptMapPreview 0x975659 vt50(1), vt28(1)) keeps 60 off via LW view +0x18
  0x9756BC push 1 / call [eax+0x50]; 0x9756C9 push 1 / call [eax+0x28]. Its destructor 0x9753D3 calls vt28(0) when view+0x19 is set, and clearGameData calls vt28(0) when view+0x18 is set. So the block clears when the screen closes (INFERRED that vt28(0) clears +0x18).

- [refuted] OnPostRender exit guard: the display signature (D3DPRESENT_PARAMETERS 0xDD3014..0xDD302C) plus a TheShell pointer change detect resolution changes
  D3DPRESENT_PARAMETERS starts at 0xDD2FF8 (frame_ctl.cpp:49; telemetry.cpp:555 reads the width and height at 0xDD2FF8/0xDD2FFC). The proposed range covers only hwnd, Windowed, refresh and interval. A fullscreen-to-fullscreen resolution change keeps all four the same.

The TheShell pointer test is unreliable: 0x645DAD deletes TheShell (0x645DE4 stores 0) and allocates a same-size object right away (0x645F08), so the allocator can return the same address.

The same path (GC+0xC9 -> 0x645DAD at 0x6488A1 inside an A-render) already runs at 60 in battles through the in-game options, so switching 60 off here is not strictly needed.

- [uncertain] No new byte patches needed; DLL-only predicate change is sufficient for a safe 60 FPS menu
  For safety the predicate may be enough, because all state changes in the shell come from A-only code: APT 0x63244B, UI sequence 0x6324A6, WindowManager 0x648642 and Shell 0x648893 are gated.

But nothing shows that the mode-9 B-render is idempotent. These were audited only for battle and LW:
- W3DDisplay::draw 0x44B788 with no game loaded;
- Display overlay slots set via 0x65C42C (state field = 2) and how they fade;
- transition drawing;
- the TheSnowManager::update 'run' call at 0x648591;
- camera.cpp and C5 presentation windows on a TacticalView with no game.
For the user's actual goal (smooth), patches would not help either, because the content itself is 30 Hz.

- [confirmed] StartBlockReason prerequisites hold in the shell (GL+0x9C/0xA8 clear, m_frame ≥ 8 reachable)
  GL+0x9C is set only by startNewGame (0x63150C) and is cleared by 0x62B385 at 0x62B42F. 0x62B385 is called on every render from clientUpdate 0x63241E, so +0x9C does not survive into the shell (INFERRED, strong). m_frame advances in the shell (see the C8 check). TV+0x23D4 was not re-verified.

- [confirmed] TheLAN [0xDE4394] is created in 0x9131DB and cleared at 0x8471E8
  Both writers exist. There is also a third writer, 0x847F3E (inside 0x847ED0), which the analysis does not list. A non-null test still works as a LAN blocker.

## MISSED
1. **No visible benefit (the user's goal).** Every animated source in this menu is 30 Hz or wall-clock throttled:
   - the APT timeline (MainMenu.apt is 33 ms/frame; SkyrimMenu.apt in the AotR root is also 0x21 = 33 ms);
   - Shell::update (≥32.33 ms);
   - Bink windows (A-only through APT update 0x814218);
   - transitions (WindowManager, A-only);
   - a static backdrop and a probable hardware .cur cursor.

   A 60 FPS menu can only repeat frames. The recommendation should say plainly that the menu cannot be made visibly smoother without interpolating APT display lists, which is a large new project. Shipping 60-mode in the shell only for the title-bar label adds risk for no gain.

2. **The +0x5D credits check is wrong.** Use TheCredits [0xDEBF50] alone.

3. **Resolution-change detection.** If it is kept, it has to include BackBufferWidth/Height at 0xDD2FF8/0xDD2FFC, or GD+0x30/0x34, or GC+0xC9 sampled at C0. It should not rely on the TheShell pointer.

4. **No audit of the mode-9 B-render path.** Items to check:
   - W3DDisplay::draw / drawFrame when no game is loaded;
   - Display overlay slot state (0x65C42C sets state 2; the per-draw fade logic was not checked);
   - TheSnowManager::update, which runs on every render at 0x648591;
   - camera.cpp and C5 presentation windows on a gameless TacticalView;
   - deferred D3D work at 0x648650.

5. **Shell::update throttle versus A-only cadence.** Pacer jitter or 1 ms timeGetTime rounding can make it skip a 33 ms slot (a 66 ms gap in screen updates). Running it on every render, relying on its own wall-clock throttle, would avoid this.

6. **Performance.** At 4K no menu render-time data exists; menu transitions logged 55–108 ms renders. Frequent PacerFallback triggers in the shell could carry the 'performance fallback' block into the first seconds of a game.

7. **Logging.** UI-sequence and clearGameData blips on menu navigation (skirmish/LAN entry) will toggle 60↔30 and log a mode line each time. These should be measured.

8. **Create-a-Hero exit.** The reset is deferred to the logic-step MSG 0x1D (0x91A041), not run directly. The guard covers it only because 0x1D is in GameStartPending; that should be stated.

9. **Unverified details.** The APT runtime accumulates wall time per MillisecondsPerFrame (0xAE3150 internals) and GE vt+0x48 is the only writer of GE+0xC in the shell; both are INFERRED.

Scratch dumps are in <analysis workspace>/menu\verify_modes\ (gl_update.c, lmd.c, w3ddraw.c, f_*.c, ft.py).