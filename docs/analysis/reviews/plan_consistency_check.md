# plan_consistency_check

The plan's addresses, bytes and build table hold up, with one exception: the entry-point value in §7 is wrong. The plan also has several real internal gaps, most importantly how each patch behaves in 30 mode and what happens when 60 mode switches off in the middle of a pair.

**Checked against `game.dat` (gdis, pefile)**
- `0x6325CF` `ff 90 9c 00 00 00` is the clientUpdate call, with ECX=ESI=engine and BL=halt flag from `0x603452`.
- `0x6325D5` `84 db a1 88 43 de 00` matches; the halted branch `0x6325DE` jumps to `0x632705`.
- `0x63252F` `a1 0c f6 d9 00` matches; isTick returns `s==1`.
- `0x63A196` `ff 15 20 09 bd 00` matches; exits `0x63A1F5` (restores ESI from `[ebp-0x20]`) and `0x63A21F` are correct.
- `0x522644` is the 15-byte Present span ending at `0x522653` (`a1 74 34 dd 00 8b 10 53 53 53 53 50 ff 52 44`).
- `0x44B964` `ff 0d b4 8c d9 00` matches. `0x44B911` is 14 bytes (`a1 8c 7a dc 00 0f af c6 01 05 80 75 dc 00`).
- `0x44181A` `e9 f2 44 1f 00` jumps to `0x635D11`. That function loads `Menus/BlankWindow.wnd`, so it is GameEngine::reset; its vtable slot is at `0xBD8504`.
- `0x6765D5` is 12 bytes with a `push ebx` inside it. `0x67173B` and `0x671774` are the 11-byte getFrame calls.
- `0x6D328E` and `0x6D332C` prologues match the plan.
- Section CRCs and SHA-256 values for the normal and delayfix builds match §7. The discriminator bytes at `0x632535`/`0x632A9B` differ between the two builds as the plan expects.

**Defects, most important first**
1. **§7 EntryPoint RVA `0x23D082` is wrong.** The real AddressOfEntryPoint is **`0x63D082`** (the CRT entry at VA `0xA3D082`; G9 says the same). The wrong value came from G8. If the host check compares it, the DLL rejects the genuine build and stays a plain forwarder.
2. **30-mode behaviour of the patches is never defined, and there is no `g_m60`-style flag in the plan.**
   - The halt patch fires "iff `g_uiTick==1`".
   - In 30 mode (`Enabled=0`, fallback, not-60 modes) every render must have `g_uiTick=1` so the A-only gates run.
   - As written, the halt would then fire on every iteration and freeze the game.
   - Since code bytes never change after DllMain, every patch needs an explicit "60 mode off = stock" branch: halt, C3, isTick, C4/C5, Present-skip, LOD sampling, the M>1 hook and the pacer.
3. **"On/off only at a pair boundary" (§1.8) conflicts with the reset hook.**
   - Loads run inside the A-render: the APT load at `0x818456`/`0x6E047A` reaches loadGame, and `0x6DEB9A` calls GameEngine::reset (G5-13).
   - New games start from `0x62B385`, called at `0x63241E` inside clientUpdate. That call is also on the B allow-list.
   - So reset fires in the middle of a pair. The plan must say what happens then: the halt reads the live mode flag, owed sync is dropped, `g_uiTick` is reset.
   - For the same reason, the `g_inClientUpdate` claim about the load-screen thread `0x65CE28` is false: the flag is 1 while that thread draws. Correctness there relies on mode-off plus a main-thread check.
4. **The coverage rule is overstated.** Per-iteration work outside the three classified functions now runs 60 times a second and is unclassified:
   - `[0xDEF548]`->vt28 asset streamer (`0x6325B0`);
   - `0x604189` and `0x603452`;
   - Debug vt94 on both paths (`0x6325ED` and `0x6326FE`);
   - the IsIconic/Sleep(5) wrapper `0x44181F`;
   - the GameEngine::execute loop calls (watchdog `0x631D04`).

   The critic's U8 flagged these as "believed harmless, not measured". They should be listed in `sites.json` and counted in telemetry.
5. **"Use the immediate 33, not `[0xDC7A8C]`" (§1.5) is left over from the IDIV-doubling design.** Nothing changes `[0xDC7A8C]` now. It has a static initializer and a setter (vtable slot `0xBDA7AC` → `0x44BF5C`), so the immediate value can only make sync differ from stock. Use `owed = [0xDC7A8C]·d − h`. The phase-2a check "sync Δ = 33·Δm_frame" should follow.
6. **"A-render = stock render" contradicts the fraction and sync definitions.**
   - C0 writes the fraction (2k−1)/12 to `GE+0x3C` before clientUpdate. GameClient::update, the drawable block `0x648705` and every A-side transform-cache user therefore see a non-stock fraction from phase 2b on.
   - Sync during the A draw is S_{k−1}+h, not S_k.
   - So "all persistent client state stepped exactly as stock" (§1.2) is not true. Either write the A fraction only around W3DDisplay::draw, or narrow the invariant to logic-visible state.
   - `k` is also used for two things: the render index ("stock render k", S_k) and the sub-frame ((2k−1)/12, 1..6). The plan should define `k = s − s_tick + 1`.
7. **C4 re-keying changes cache behaviour during pause and frozen time.**
   - `g_renderId` goes up on every render. While `m_frame` is constant, stock hits the cache but 60 mode recomputes with the current fraction.
   - Drawable::xfer is a consumer of the cache getter, and saves are made while paused (G5-13).
   - Fix: bump the key only when the stock key would change, or on a B-render after an advancing A-render (the same rule as owed sync).
8. **Phase order: the camera checks sit in the wrong phase.** B-render camera gating lands in 2a. The logic-visible camera checks (vt78, `+0x23D0`/`+0x23D4`/`+0x23CC`, `TheAI+0x1C`, `0x452` waypoint messages, key-hold scroll rate) only appear in the phase-3 Check. They belong in 2a; phase 3 only adds picture interpolation.
9. **Checks that cannot be measured as written:**
   - §4.3 "Every diff … fails if it is 0": an identical diff has 0 lines, which is the pass case. It should be "fail if either trace has 0 records".
   - §4.2 "battle ≥ stock − 0.3 %" needs a battle load that repeats in both modes. Replay playback is excluded from 60 mode, so name a fixed no-input save and duration.
   - Phase 1 "counting pass-through proves the byte contracts": a pass-through never takes the alternate exits (forced halt, `0x522653`, `0x44B9E3`, `0x63A1F5`/`0x63A21F`).
   - The Telemetry=1 seed check reports RNG use the client already does in stock (BuildAssistant, ThingFactory, eye tower per G4). It must be compared against the stock baseline, not treated as an error.
   - "Exact half-steps" (§1.5, phase 4b) are impossible for INC counters such as `0x4FB599` `ff 05 ec 1b dd 00` (inc `[0xDD1BEC]`) and for ×0.8 at `0x67C4C7`, unless counters and thresholds are doubled.
   - "Logic never reads" these effects is unproven for view filters: vt78 `0x486352` returns early-true when filter `+0x110==2` (G2-C18).
10. **Undefined or stale terms:**
    - `g_inB` (§6) is never defined.
    - The GL/GD/GE/TV bases are not given, and `+0x23D0` has no base at all.
    - `mult` in T is the network multiplier `[0xD9F498]`, which is easily confused with camera M.
    - "G2's list … etc." leaves the camera gate set outside the plan.
    - The C1/C2/C6 numbering gaps are left over from older designs.
    - "LW battle" versus "WotR battle" is ambiguous.
    - InGameUI::update has no address in the plan.
    - §1.2 says the only B-render exceptions are in §1.5, but §1.4 adds `0x62B385`, `0x532D6F`, GC vt90, `0x80000F`, `0x7128C3` and snow.
11. **M>1 wording.** The 29 ms wait happens once per drawn frame (every M−1 pairs), not "per pair". The B-render jump to `0x44B9E3` also skips the `[edi+0x115]` check at `0x44B995`.

**User requirements the plan does not clearly cover**
- **DXVK is never mentioned.** `rotwk\d3d9.dll` (DXVK 2.6.2) and `dxvk.conf` are tracked in git. The Present-skip and FIFO math assume DXVK FIFO, and the launcher's DXVK toggle rewrites `dxvk.conf`. `d3d9.maxFrameRate` must not be set to 60 (the G6 gap report warns it fights the pacer; G9 suggests the opposite). Add DXVK on/off to the §4 test matrix.
- **Git tracking is incomplete.**
  - Config and logs in `%APPDATA%\Age of the Ring\aotr60\` and the "backups outside rotwk\" from `install.ps1` are outside the repo. Also, no `dinput8.dll` exists in rotwk, so it is unclear what gets backed up. A default config template should be committed.
  - "status shows only `rotwk/dinput8.dll`" conflicts with "commit the DLL at milestones": after that commit the status should be empty.
- **Launcher.** `dinput8.dll` is not on the rotwk ignore list, so the launcher reports "Game files modified" on every start, and accepting PATCH deletes the DLL (G9). The plan should document this.
- **Fixed 60 FPS** means 60.6 renders/s and 60 Presents/s on a 60 Hz display, with a fallback to 30. The plan states this; the user should approve it explicitly.

No files were created or changed.
