# G9_bootstrap

## Summary

I looked at G9 (DLL bootstrap and process facts) read-only, in game.dat, delayfix.dat, game820.dat, the rotwk stub lotrbfme2ep1.exe and AotR_Launcher.exe. AotR_Launcher.exe turned out to be a PyInstaller bundle that contains its own Python source.

(a) dinput8.dll is a static import of game.dat with exactly one function, DirectInput8Create. Its IAT slot is 0xBD0078 and the thunk is 0xA3EC90. There is one caller, 0x4985FF, inside FUN_004985DC (DirectInputKeyboard::init, vtable 0xBDE5B0). It creates only GUID_SysKeyboard with IID_IDirectInput8A: buffered, 256 entries, cooperative level NONEXCLUSIVE|FOREGROUND. The mouse does not use DirectInput.
- No other PE in rotwk\ imports or loads dinput8. dinput8 is not a KnownDLL, so a proxy placed in rotwk\ wins the DLL search.
- The real 32-bit DLL is C:\Windows\SysWOW64\dinput8.dll. It exports DirectInput8Create plus 5 COM/joystick exports.

(b) Launch chain:
- The real single-player launcher is AotR_Launcher.exe, not aotr\lotrbfme2ep1.exe. That one is a Qt "GameRanger command-line flag tool" used for multiplayer.
- AotR_Launcher runs QProcess with working directory = the rotwk path, program = rotwk\lotrbfme2ep1.exe, args ["-mod", "<game folder>\aotr"]. Its log confirms this. With windowed mode on, it inserts "-win -fullscreen -xpos 0 -ypos 0 -xres W -yres H" before -mod.
- The EA stub reads "RUN = . game.dat" and appends " "+argv[i]. It calls CreateProcessA(NULL, "game.dat -mod <game folder>\aotr", inherit=TRUE, flags 0, cwd=NULL). So game.dat inherits cwd = rotwk.
- game.dat's WinMain (0x4027F7) also sets the working directory to the exe folder when lotrsec.big is not found in it.
- The stub and game.dat then do a handshake: a mutex/event named from gi.dat, thread message 0xBEEF, a file mapping, with 60 s and 10 s timeouts.

(c) Threading and patch safety:
- The main loop runs on the process's initial thread (the same thread that runs DllMain): entry 0xA3D082 → WinMain 0x4027F7 → GameMain 0x6443B0 → TheGameEngine(0xDE4324) vt38/vt3C.
- Worker threads exist only for audio (Miles), asset streaming, the W3D mouse cursor, a display movie/load-screen thread (draws APT through TheDisplay vt+0x18C under the WW3D lock, never W3DDisplay::draw), networking/HTTP and the crash dialog. None of them run GameLogic, GameClient::update or any of the planned patch sites.
- No integrity checks on .text: SecuROM is stripped (entry point is the normal C runtime start 0xA3D082, versus 0xEC506E inside stxt371 in the original game.other). Nothing in .text references stxt774/stxt371, and I found no CRC over code. The WinVerifyTrust result is irrelevant. The AotR code caves do not read code.
- AotR's code patches do not overlap any planned patch site. The delayfix variant changes the stepper next door (0x632537 and 0x632A9B).
- DllMain is safe for code patches. It is not safe for data that the exe's static initializers set (that table at 0xD8AC84 runs after DllMain).

(d) Settings:
- Resolution comes from %APPDATA%\Age of the Ring\Options.ini (the folder name is the registry value UserDataLeafName under the GameRegPath key in gi.dat).
- Windowed mode comes only from -win (sets GlobalData+0x2C) or "Windowed" in GameData.ini.
- No FPS setting is exposed to the user: no Options.ini key, no command-line switch. UseFPSLimit (+0x26) and FramesPerSecondLimit (+0x28) are set only in aotr\data\ini\gamedata.ini lines 11137-11138 (Yes / 30). Only scripts, the credits screen and MSG_NEW_GAME change the limit at runtime.
- Presentation interval is DEFAULT (vsync; 0x524B5C). DXVK runs it as exclusive fullscreen with FIFO present.

(e) Timer resolution:
- timeBeginPeriod(1) is called at 0x63A507 (GameEngine constructor, argument 1 pushed at 0x63A4C5). It is also called from a static initializer (0xBCADA2) and WW3D::Init (0x517A49). So timeGetTime has 1 ms resolution for the whole process.
- QueryPerformanceCounter/Frequency are imported. The game uses them only for profiling.

Additional findings that matter for the DLL:
- The AotR launcher's file scanner will report rotwk\dinput8.dll, and any ini/log next to it, as unknown files ("Game files modified...") and delete them if the user accepts PATCH.
- The launcher swaps rotwk\game.dat between game.dat and delayfix.dat (PvP mode). The DLL must detect the variant by bytes.
- AotR set the large-address-aware flag on game.dat (pointers can be above 2 GB).
- The game forces the x87 FPU to 24-bit precision (FUN_00440809).
- Keyboard update runs once per render and counts key-repeat in frames, so it must stay at 30 Hz (skip on B-renders).

## Design notes

1. Bootstrap recipe (all facts verified above):
- Build rotwk\dinput8.dll (x86, /arch:SSE2, static CRT is fine) exporting the undecorated name DirectInput8Create via a .def file. Forwarding the other 5 system exports is optional.
- The forwarder lazily loads the real DLL on its first call with LoadLibraryExW(L"dinput8.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32), or with GetSystemDirectoryW()+"\\dinput8.dll" (WOW64 redirects to SysWOW64). It caches GetProcAddress and passes all 5 arguments through. The first call happens on the main thread at keyboard init (0x4985FF), outside the loader lock.
- DllMain(PROCESS_ATTACH), in order:
  1. DisableThreadLibraryCalls.
  2. Host check: GetModuleHandle(NULL)==0x400000, the PE TimeDateStamp is 0x460DA09E, and the bytes at 0x632537 are `08 f6 d9 00` and at 0x632A9B are `f7 3d 08 f6 d9 00`. This rejects delayfix (PvP mode) and stock game820 (whose AotR-only sites differ, e.g. 0x9A3AE0/0x629D11).
  3. Verify the original bytes at every planned patch site (all-or-nothing).
  4. VirtualProtect, write, FlushInstructionCache.
  5. Record the main thread id.
- Do not write statically-initialized data in DllMain (static init runs afterwards). Defer such writes to the first C0 stub call.
- If the host check fails, stay a pure forwarder: the game runs at stock 30 FPS.

2. Files:
- The DLL must be in rotwk\ (the folder of game.dat).
- The AotR launcher will flag it as unknown and delete it if the user accepts PATCH. Its own ini and log should therefore not go in rotwk\. Recommended location: %APPDATA%\<UserDataLeafName>\, i.e. %APPDATA%\Age of the Ring\, with UserDataLeafName read from HKLM\SOFTWARE\WOW6432Node\AotR\Standalone\The Lord of the Rings, The Rise of the Witch-king (the GameRegPath in gi.dat). Fallback: rotwk\zzCammyFiles\, which the scanner skips.
- Locate everything with GetModuleFileNameW(hinstDLL); never use cwd in DllMain.
- The README should say a launcher PATCH removes the mod, and that PvP mode (delayfix game.dat) disables it.

3. Multiplayer: GameRanger multiplayer goes through aotr\lotrbfme2ep1.exe (a flag tool) and then the same stub, so the DLL also loads in multiplayer. The runtime check that TheNetwork ([0xDE4468]) is null, plus the delayfix byte check, keeps multiplayer at 30 FPS.

4. Input (supports the allow-list principle): keyboard update and createStreamMessages (0x6485D8/0x6485E3) run every render and count key-repeat in frames, so they must be skipped on B-renders. The mouse is Win32 and W3DMouse. The DirectX-mode cursor thread moves the cursor in real time, but mouse update (TheMouse 0xDE36E0) is also per render. I did not check whether its click and drag logic counts frames; default it to skip-on-B as well.

5. Pacing:
- timeGetTime is already at 1 ms. Use int64 QPC math in the DLL, because x87 is forced to 24-bit precision by FUN_00440809.
- Present uses vsync (DEFAULT interval, DXVK FIFO). On 60 Hz displays this paces 60 FPS exactly and preserves the stock game speed (stock is also vsync-snapped to 30.0 FPS, a 200 ms tick). On displays above 60 Hz, expect judder from 16/17 ms targets snapping to refresh multiples. Consider pacing to present completion or documenting 60/120 Hz.
- The C2 deadline-pacing change must count Present's blocking time.

6. Threads: no worker thread reaches any planned patch site. The load-screen and movie thread renders APT under the WW3D lock while the main thread loads a map. The stubs do not need locks, but asserting the main thread is cheap insurance.

Contradictions to the brief:
- The task statement says "AotR's aotr\lotrbfme2ep1.exe (Qt) starts it with ' -mod "..."'". That exe is a GameRanger multiplayer flag tool. The single-player launcher is <game folder>\AotR_Launcher.exe (PyInstaller/PyQt6, x64). It starts rotwk\lotrbfme2ep1.exe with cwd=rotwk and the separate arguments -mod <game folder>\aotr.
- The stub's CreateProcess uses bInheritHandles=TRUE, not FALSE.

## Items

### G9-01 — input / DLL bootstrap  [n/a, high]
- **Site:** IAT 0xBD0078 (DINPUT8.DLL!DirectInput8Create, only import from this DLL); thunk 0xA3EC90 `jmp [0xBD0078]`; sole call 0x4985FF in FUN_004985DC (DirectInputKeyboard::init, reached via vtable 0xBDE5B0 slot+4 = 0x4986B4 -> jmp 0x4985DC)
- **What:** DirectInput8Create(hInst=[0xDC3C60], 0x800, IID_IDirectInput8A {bf798030-483a-4da2-aa99-5d64ed369700} @0xCF82B8, kbd+0xE20, NULL); then CreateDevice(GUID_SysKeyboard {6f1d2b61-...} @0xCF8148), SetDataFormat(c_dfDIKeyboard @0xC95474: size 24, 256 objects), SetCooperativeLevel(hwnd=[0xDC3C64], 6 = NONEXCLUSIVE|FOREGROUND), SetProperty(DIPROP_BUFFERSIZE=0x100), Acquire. On failure it calls FUN_00498477. This is the only DirectInput use: the mouse (vtable 0xBDE5F4, W3DMouse with a WWLib ThreadClass cursor thread) has no DirectInput8Create or CreateDevice path.
- **Cadence:** event: once, when TheKeyboard (0xDE4334) is initialised during client init, on the main thread. This is the first time the proxy's forwarder is called.
- **Reason:** Bootstrap fact. The proxy only needs to forward DirectInput8Create (stdcall, 5 arguments, undecorated export name via .def). game.dat imports nothing else from dinput8. Forwarding the other 5 system exports (DllCanUnloadNow, DllGetClassObject, DllRegisterServer, DllUnregisterServer, GetdfDIJoystick) is optional hygiene.
- **Risk if wrong:** If the forwarder fails or returns an error, the keyboard silently does not work (FUN_00498477 path). A wrong calling convention or decoration means the import does not resolve and the loader refuses to start game.dat.

### G9-02 — DLL bootstrap  [n/a, high]
- **Site:** rotwk\*.dll/*.exe import tables (pefile scan); HKLM\...\Session Manager\KnownDLLs; C:\Windows\SysWOW64\dinput8.dll
- **What:** Static importers of dinput8 in rotwk\: only game.dat. d3d9.dll (DXVK 2.6.2) imports api-ms-win-crt-* and setupapi. d3dx9_27.dll (MS 9.08.299) imports only core DLLs. mss32 imports winmm. p2xdll imports perlcrt. patchw32 imports ole32 and version. dbghelp imports rpcrt4. debugwindowlite imports comctl32, shlwapi, oleaut32, oleacc. gdiplus imports ole32 (and gdiplus is a KnownDLL, so the rotwk copy is unused). The lotrbfme2ep1.exe stub and Worldbuilder.exe do not import or name dinput8. No binkw32.dll is present. game.dat loads D3D9.DLL itself with LoadLibraryA (FUN_00524FD0, strings at 0xBE7204/0xBE71F4), so the DXVK d3d9.dll in rotwk is used. dinput8.dll is not in KnownDLLs, so rotwk\dinput8.dll (the application directory = game.dat's folder) is loaded instead of the system copy. The system 32-bit dinput8 is C:\Windows\SysWOW64\dinput8.dll; its exports are ordinals 1-6: DirectInput8Create, DllCanUnloadNow, DllGetClassObject, DllRegisterServer, DllUnregisterServer, GetdfDIJoystick.
- **Cadence:** event: process load
- **Reason:** No conflicting dinput8 importer or second proxy exists. Load the real DLL with LoadLibraryExW(L"dinput8.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32), or with GetSystemDirectoryW()+"\\dinput8.dll" (WOW64 redirects system32 to SysWOW64 in a 32-bit process). This never loads the proxy itself.
- **Risk if wrong:** If the proxy loads itself recursively, the stack overflows or the keyboard is missing.

### G9-03 — process launch  [n/a, high]
- **Site:** AotR_Launcher.exe views\home_view.py launch_game (lines 709-735, decompressed in memory from the PyInstaller archive); %APPDATA%\Age of the Ring\Launcher Files\app.log line 1926; rotwk\lotrbfme2ep1.exe 0x40AE70 (RUN parser), 0x409490 (argv append loop), 0x40ACB0/0x40AD58 (CreateProcessA)
- **What:** 1) AotR_Launcher (x64 PyQt6) calls game_process.setWorkingDirectory(rotwk_path) and start(rotwk\lotrbfme2ep1.exe, ['-mod', aotr_path]). In windowed mode it inserts ['-win','-fullscreen','-xpos','0','-ypos','0','-xres',W,'-yres',H] at index 1; with script debug it appends '-scriptdebug2'. The log shows ['<game folder>\\rotwk\\lotrbfme2ep1.exe', '-mod', '<game folder>\\aotr'].
2) The EA stub parses the lcf key 'RUN = . game.dat' into struct fields +0 dir='.', +0x100 exe='game.dat', +0x200 = rest of line. 0x409490 appends ' '+argv[i] for i>=1 to +0x200, using CRT argv (quotes stripped, not re-quoted).
3) 0x40AD58 calls CreateProcessA(NULL, exe+args, NULL, NULL, bInherit=TRUE, flags=0, env=NULL, cwd=NULL). game.dat therefore gets the command line 'game.dat -mod <game folder>\aotr' (argv[0] relative and unquoted) and inherits the stub's cwd, which is rotwk\ when started by AotR_Launcher.
4) game.dat WinMain 0x4027F7: if FindFirstFileW(L"lotrsec.big") fails in cwd, it calls GetModuleFileNameW(NULL), strips the file name and calls SetCurrentDirectoryW(exe dir). It parses -win (DAT_00DC3C68=1), -fullscreen (=0), -xpos and -ypos, then creates the single-instance mutex E99E8455-....
5) aotr\lotrbfme2ep1.exe is a Qt 'GameRanger command-line flag tool' (strings 'Game executable:', 'Mod file:', 'Extra flags:', ' -mod "' at 0xA75A04) used for GameRanger multiplayer, not the single-player launcher.
- **Cadence:** event: process start
- **Reason:** The DLL must not rely on cwd in DllMain, because cwd is inherited and is only fixed by WinMain later. Locate files with GetModuleFileNameW(hinstDLL); the DLL folder is the same as game.dat's folder (rotwk\).
- **Risk if wrong:** If paths are relative to cwd, a launch from GameRanger or a shortcut with a different cwd cannot find the ini or log.

### G9-04 — process launch / launcher handshake  [n/a, medium]
- **Site:** game.dat WinMain 0x4027F7 → FUN_0063F68D (CreateMutexA(name from gi.dat via FUN_00AAA900), ERROR_ALREADY_EXISTS=launched by stub) → FUN_0063F6BF (OpenEventA loop ≤60 s, then PeekMessage for 0xBEEF ≤10 s, MapViewOfFileEx(lParam) → 0xDE4338); stub side 0x40B650 (CreateMutexA), 0x40B1A0 (decodes game2.dat, ergc/volume-serial key, file mapping)
- **What:** After CreateProcess, the stub hands its key/launch data to game.dat through a named mutex/event and a thread message 0xBEEF carrying a file-mapping handle. The message is posted to game.dat's main thread. If game.dat is started without the stub (mutex absent), this step is skipped.
- **Cadence:** event: once, in WinMain, after DllMain has returned
- **Reason:** DllMain must stay short and must not pump or consume the main thread's message queue (no PeekMessage/GetMessage in DllMain). The timeouts are generous (60 s/10 s), so normal patching time is irrelevant.
- **Risk if wrong:** If DllMain blocks for a long time or pumps messages, the handshake fails and the launch data is lost (the stub reports an error or the game starts without it).

### G9-05 — deployment / AotR launcher  [n/a, high]
- **Site:** AotR_Launcher app\patcher.py scan_game_files; app\data_manager.py _set_ignore_paths lines 245-286; %APPDATA%\Age of the Ring\Launcher Files\checksum\remote\rotwk.json
- **What:** The launcher walks the rotwk, aotr and bfme2 install folders with os.walk; the only folder it skips is any subfolder named 'zzCammyFiles'. Any file missing from the remote checksum list is marked for removal ('Delete: <file>') and sets needs_patch, which shows 'Game files modified...' unless the file is in ignore_patch_files. The rotwk ignore list contains only game.dat, profile-high.csv, d3d9.dll, dxvk.conf, game.dat.dxvk-cache, game.dat_d3d9.log and d3dx9_27.dll. The remote rotwk.json has no dinput8.dll. If the user accepts PATCH, patch_files() calls os.remove on the flagged files. Launching is not blocked. Other launcher behaviour:
- The PvP toggle copies aotr\zGameDats\{game,delayfix}.dat over rotwk\game.dat.
- The DXVK toggle copies or deletes rotwk\d3d9.dll and dxvk.conf.
- The launcher sets the AppCompat layer '~ WINXPSP3' on both lotrbfme2ep1.exe files. game.dat may also carry a per-user (HKCU) compatibility layer.
- **Cadence:** event: every launcher start (scan) / PATCH click
- **Reason:** rotwk\dinput8.dll will always be reported, and is deleted if the user patches. Keep the DLL's ini and log out of rotwk\. Options: the user data folder %APPDATA%\Age of the Ring\ (not scanned), or rotwk\zzCammyFiles\ (excluded from the scan). Document that a PATCH removes the mod.
- **Risk if wrong:** Users lose the mod silently after accepting a launcher patch, or are confused by a permanent 'Game files modified' banner.

### G9-06 — binary variant detection  [n/a, high]
- **Site:** game.dat vs delayfix.dat diff: 0x632537 (game `08 f6 d9 00` = [0xD9F608] LTR; delayfix `00 a4 ec 00` = [0xECA400]=8), 0x632A9B (game `f7 3d 08 f6 d9 00` idiv [LTR]; delayfix `b8 02 00 00 00 eb 19`), 0x9A3AE0 (game `e9 1b f5 52 00`; delayfix restored `51 53 56 8b f1`), 0x6E5A10 7e→eb, 0x920B79 75→eb, 0x8ED4BF/0x8ED544 table bytes, 0xBDD378 float 5000.0→700.0, 0xC79421, 0xDB55C0/0xDB5608 ptr, .danetta/.angmar bytes
- **What:** Both variants have the same PE header (TimeDateStamp 0x460DA09E, CheckSum 0xADC2F6, same size 11347456), so they can only be told apart by bytes. In delayfix:
- FUN_0063252F (isTick) divides by 8, so isTick becomes s==2.
- step 0x6329B0 has a catch-up loop with esi=3 that runs sub 1 and sub 2 in the same frame and increments GE+0x34.
The stepper cycle therefore becomes 5 frames, unlike the stock model the design relies on. The launcher text says 'PvP mode uses the delayfix game.dat ... not recommended for single-player'. game820.dat (stock, no AotR) differs in many places, and its Characteristics are 0x10F (no large-address-aware flag).
- **Cadence:** event: DllMain host check
- **Reason:** The DLL must check all original bytes at every patch site, plus these signatures, before patching. On delayfix or unknown builds it should stay a pure forwarder (30 FPS). The planned patch bytes themselves are identical in both variants, but the stepper semantics differ.
- **Risk if wrong:** Running 60-FPS mode on delayfix breaks the 12-sub-frame mapping: wrong tick cadence, game speed changes, logic call pattern changes.

### G9-07 — process memory layout  [n/a, high]
- **Site:** PE headers (pefile): game.dat ImageBase 0x400000, no .reloc (Characteristics 0x12F, RELOCS_STRIPPED), DllCharacteristics 0, EP 0xA3D082, import dir 0xEC6000 (.mackt_M), IAT 0xBD0000.. in .rdata (flags 0xC0000040 RW), .text 0x401000-0xBCFFFF flags 0x60000020 (RX), stxt774 0xEBF000 / stxt371 0xEC2000 / .mackt_M 0xEC6000 / .danetta 0xECA000 RWX, .angmar 0xED3000 RX; security dir offset 0xBA2350 lies beyond end of file (stale)
- **What:** Facts about the image layout:
- Fixed base 0x400000 with no relocations, so all absolute addresses are valid as-is.
- The large-address-aware flag (0x20) is set in game.dat (AotR 4GB patch) but not in game820. Heap pointers can be at or above 0x80000000.
- No ASLR and no NX-compat flags.
- .text is read/execute only: writes need VirtualProtect(PAGE_EXECUTE_READWRITE) followed by FlushInstructionCache.
- The IAT section is writable.
- The original SecuROM build is rotwk\game.other (EP 0xEC506E in stxt371, .text entropy 8.0). game.dat is that build unpacked: EP is the C runtime start 0xA3D082 and the import table was rebuilt in .mackt_M.
- stxt774/stxt371 are dead SecuROM code; .danetta/.angmar are AotR code caves and data.
- **Cadence:** n/a
- **Reason:** Trampolines: use VirtualAlloc(PAGE_EXECUTE_READWRITE) memory or DLL code; a rel32 call/jmp reaches anywhere in 32-bit space. Do not borrow stxt*/.danetta/.angmar. DLL-side tables keyed by game pointers must use unsigned uintptr_t.
- **Risk if wrong:** Signed pointer comparisons break above 2 GB, and writing into AotR caves corrupts AotR features.

### G9-08 — integrity checks  [n/a, high]
- **Site:** WinVerifyTrust FUN_006443D4 (called by FUN_00644CC2); Debug-lib FUN_0043DBA0 (VirtualProtect, msvcrt.dll IAT hook of RaiseException); IsDebuggerPresent/CheckRemoteDebuggerPresent 0x43DD20/0x43C964; listing scan for immediates 0x401000/0xBD0000/0x7CF000/SizeOfImage; AotR caves 0xECA000, 0xED0300, 0xED0700, 0xED0A00, 0xED0B00, 0xED0D00, 0xED1600, 0xED1900, 0xED1A00, 0xED1B00, 0xED1C00
- **What:** No self-checksum of .text was found.
- WinVerifyTrust(argv[0]) returns 1 or 2, never 0, so FUN_00644CC2 always takes the VERSION-string branch; the signature result does not matter.
- VirtualProtect is used only to hook msvcrt.dll's IAT (Debug library).
- The debugger checks only drive debug output.
- exeCRC/iniCRC are GameSpy/LAN report keys (0x79120E/0x78EF3B) for multiplayer matching; CRC strings are logic and network CRCs over game state.
- Nothing in .text references stxt774/stxt371.
- The AotR caves only branch back into .text; their 'mov eax,imm' values are function pointers, not code reads.
- AotR's .text patch sites are 0x52CC7F and 0x638D4A (registry path pushes to 0xEA7690), 0x5D8A65/0x5D8AF2 (call 0xECA000), 0x629D11, 0x69413A, 0x69A760, 0x6D40FA, 0x6D410A, 0x6D57AF, 0x6D7267, 0x73BDD3, 0x79DCE0, 0x88D061, 0x8A11A3, 0x8A144D, 0x8ED4BF/516/544/5D9 and 0x9A3AE0. None of them overlaps a planned patch site (0x6325CF..0x6326F5, 0x63A1AD, 0x63A1F8, 0x44B911, 0x44B98E, 0x44B9C3, 0x449D40, 0x444CF2, 0x6765D5, 0x67173B, 0x671774, 0x67BDC0).
- Oddity: 0x9A3AE0 jumps to 0xED3000, which holds data (f4 = HLT). FUN_009A3ADD has no direct callers, so it is presumably never reached.
- **Cadence:** n/a
- **Reason:** Patching code in memory is safe from integrity checks. The multiplayer exe CRC (if any) is over the file on disk, which is never modified.
- **Risk if wrong:** An undiscovered checksum would crash or desync. That is unlikely given the evidence; the runtime test M2 (logic hash) would catch logic-side effects.

### G9-09 — patch timing / threading  [n/a, high]
- **Site:** Loader order: static import dinput8 → proxy DllMain(PROCESS_ATTACH) on the initial thread, before exe entry 0xA3D082 → CRT _initterm (static-initializer table entries e.g. 0xD8AC84 → 0xBCADA0; static init of 0xDC7A8C at 0xBC146C/0xBC1478) → WinMain 0x4027F7 → GameMain 0x6443B0 (`TheGameEngine=[0xDE4324]=FUN_00401CF3(); vt38 init(argc,argv); jmp vt3C execute`)
- **What:** DllMain runs on the same initial thread that later runs WinMain, GameMain and GameEngine::execute 0x639CF8 (the main loop). game.dat's IAT is already resolved and .text is mapped. No game code has run, the C runtime static initializers have not run, and no game worker thread exists yet.
- **Cadence:** event: once
- **Reason:** Apply all code patches in DllMain: VirtualProtect, byte checks, write, FlushInstructionCache. Record GetCurrentThreadId() as the game thread so stubs and runtime immediate toggles (C1a/C1c/C6) can assert they run on it. Do not write in DllMain to data that the static initializers set (e.g. 0xDC7A8C), because it would be overwritten. Defer any such data write to the first stub invocation. Do not LoadLibrary the real dinput8 in DllMain (loader lock); resolve it lazily on the first DirectInput8Create call (main thread, keyboard init).
- **Risk if wrong:** A data patch made in DllMain is silently reverted by static init. LoadLibrary under the loader lock can deadlock with other DllMains.

### G9-10 — threading  [not_on_B_path, medium]
- **Site:** CreateThread/_beginthread(ex) sites: 0x4611D3→0x45E97D/0x45E6FF (Miles audio service loop, AIL_ms_count+Sleep); 0x4A7F61 (from MilesAudioManager ctor 0x45C675)→0x4A7E67 (audio file loader, Sleep(1)); 0x4A82C3→0xB5390E (audio worker, created from 0x460D96 Miles init); 0xA32B90 (from W3DDisplay init 0x446330)→0xA38AD0 _beginthread 0xA36390 (asset-stream manager; main thread services it via [0xDEF548]->vt28 at 0x6325A8); 0xA241A0 (WWLib ThreadClass, W3DMouse vtable 0xBDE5F4 slot+4) from 0x498ADB/0x499A56 (cursor redraw-mode thread); 0x65D604→0x65CE28 (display movie / load-screen thread, started via 0x65D6CB(1) from load-screen 0x81C79C mode 5, and 0x65D773 mode 3); 0xA7E220/0xA7E750/0xA92A00/0xA99260/0xADE110 (GameSpy/network); 0x9CB890 (WinInet HTTP); 0x43D1C0 (Debug crash/assert dialog); 0x632778 (debug '_EA_RTS_HEADLESS' child processes)
- **What:** Only the main thread runs GameEngine::update, clientUpdate, GameLogic::update, W3DDisplay::draw and the limiter. The display worker thread (0x65CE28) calls TheDisplay vt+0x124 = 0x65CC2A and vt+0x18C = 0x4475E4. Under the WW3D lock (FUN_0051EEC0/FUN_005208D0) these render the APT load screen ([0xDE3F0C]->vt30) or movie frames, call TheWindowManager vt28 and FUN_00440809, and sleep 33 ms in mode 5. It never reaches 0x44B788, 0x632409 or 0x6325A0.
- **Cadence:** real time (worker threads)
- **Reason:** No worker thread executes any planned patch site, so stubs can assume single-threaded execution. The load-screen thread draws only while the main thread is loading a map, when the stepper and limiter are not running.
- **Risk if wrong:** A shared stub reached from two threads would race on renderId/pending state. The evidence says this does not happen.

### G9-11 — input (keyboard)  [skip_on_B, high]
- **Site:** TheKeyboard [0xDE4334] update vt28 = 0x4985CB → jmp 0x63F667 (counter +0xE1C++, read DirectInput 0x63F61B, state/repeat 0x63F4E2 → 0x63F473); createStreamMessages vt3C = 0x63F19A (MSG_RAW_KEY_DOWN 0x15 / UP 0x16 into TheMessageStream 0xDE6398). Call sites: 0x6485D8 + 0x6485E3 in GameClient::update 0x64849E (per render, outside the m_frame-gated drawable block); 0x6324BC in clientUpdate (only when focus regained, FUN_0080000F bit0); 0x65D581 (main-thread movie loop)
- **What:** Key auto-repeat counts keyboard updates, not time: 0x63F473 repeats a held key once (counter - downFrame) >= 11 (`cmp ... 0xb`). It then resets every timestamp to counter and sets the repeating key to counter-12, so after the 11-update delay the key repeats on every update. Stock: about 367 ms delay, then 30 repeats/s.
- **Cadence:** per render (60/s under model H if not gated)
- **Reason:** If keyboard update and createStreamMessages run on B-renders, the key-repeat delay halves to about 183 ms and the repeat rate doubles to 60/s (held hotkeys, chat, held build or queue keys). Running them on A-renders only keeps stock behaviour. The DirectInput buffer (256 events) holds input between A-renders; latency stays as in stock.
- **Risk if wrong:** Held keys repeat twice as fast: unit queuing via held hotkeys, text entry and camera hotkey repeats change. Logic determinism is unaffected, but the input stream changes, which can matter for replays.
- **Fix @ 0x6485CC..0x6485E5 (`8b 0d 34 43 de 00 85 c9 74 10 8b 01 ff 50 28 8b 0d 34 43 de 00 8b 01 ff 50 3c`):** Gate the whole block on g_uiTick: run on A-renders, and on every other render while paused or frozen. Covered automatically if the B-render allow-list skips GameClient::update's per-render subsystem calls. (orig: if (TheKeyboard) { TheKeyboard->vt28(); TheKeyboard->vt3C(); })

### G9-12 — FPU state  [n/a, high]
- **Site:** FUN_00440809 (`_fpreset(); _controlfp((_statusfp() & 0xFFFEFCFF) | 0x20000, 0x30300)`), called from GameLogic::update on every call and from 0x65CC2A
- **What:** The game sets the x87 FPU to 24-bit precision (_PC_24) and round-to-nearest. Engine float code (e.g. the limiter 0x63A19C fild/fmul/fdivr, then _ftol2) runs in this mode.
- **Cadence:** per logic call (re-asserted)
- **Reason:** The DLL must be built with SSE2 float math (the MSVC x86 default is /arch:SSE2; do not use /arch:IA32 or long double) and do its pacing in integer QPC ticks. Otherwise its double math is silently 24-bit (about 7 significant digits), which breaks QPC-based deadlines. Stubs must preserve the x87 control word and MXCSR, and keep the x87 stack balanced (e.g. the C2 LimStub that replaces the call to _ftol2 at 0x63A1AD must pop ST0 exactly as _ftol2 does).
- **Risk if wrong:** Pacing drift or jitter from lost precision; FPU stack corruption causes NaNs in game math.

### G9-13 — settings  [n/a, high]
- **Site:** Options.ini reader FUN_006E56F3 (string 0xC1B1C8); UserDataLeafName FUN_0064174E/FUN_0064148E (registry GameRegPath from rotwk\gi.dat = SOFTWARE\WOW6432Node\AotR\Standalone\The Lord of the Rings, The Rise of the Witch-king; value 'Age of the Ring'); keys at 0xBF97xx..0xBF9A38 and 0xC1B150..0xC1B254; GameData field table 0xBFF580.. {Windowed +0x2C, XResolution +0x30, YResolution +0x34, UseFPSLimit +0x26, FramesPerSecondLimit +0x28}; command-line table 0xC35DA8..0xC35E27
- **What:** Settings sources:
- User data: %APPDATA%\Age of the Ring\Options.ini. The file holds Resolution, StaticGameLOD/IdealStaticGameLOD, ScrollFactor, AudioLOD, volumes and similar. It has no FPS, vsync or windowed key, and none exists among the option strings.
- Windowed mode: only the -win switch (handler 0x7B9FC6 sets GlobalData+0x2C=1) or 'Windowed' in GameData.ini. WinMain's own -win/-fullscreen pair sets the window style flag DAT_00DC3C68.
- Resolution: Options.ini, or -xres/-yres.
- Command-line switches: -noshellmap, -mod, -noaudio, -xres, -yres, -win, -scriptDebug2, -scriptDebugLite, -fullVersion, -preferLocalFiles, -Watchdog, -noWatchdog, -rif, -file, -resumeGame, -randomSeed. None relates to FPS.
- FPS limit: only UseFPSLimit=Yes and FramesPerSecondLimit=30 in aotr\data\ini\gamedata.ini lines 11137-11138 (managed and checksummed by the AotR launcher). At runtime only these change it: GameEngine::init 0x63C841, ScriptEngine reset 0x6096B1, MSG_NEW_GAME 0x779DCF (forces UseFPSLimit=1), script SET_FPS_LIMIT 0x7CCF20, credits 0x91B6DC (100, restored at 0x91B7AA).
- AotR launcher settings (aotr_settings.ini) hold windowed_mode, dxvk and pvp_mode.
- -Watchdog sets 0xDE87B9=1 (handler 0x7B9F36). It is off by default, so FUN_00631D04 per frame stays inactive.
- **Cadence:** event: startup / options save
- **Reason:** No user-facing setting fights the DLL. Keep FramesPerSecondLimit=30 (the design halves the period in LimStub) and do not edit gamedata.ini, because the launcher would flag and revert it. The DLL's own on/off switch belongs in its own ini.
- **Risk if wrong:** Editing AotR INIs gets them reverted or flagged by the launcher patcher.

### G9-14 — presentation / pacing  [n/a, medium]
- **Site:** DX8Wrapper::Set_Render_Device FUN_005249A0: 0x524B5C `mov [0xDD302C],ebp` (ebp=0 from 0x5249ED) = D3DPRESENT_PARAMETERS.PresentationInterval (0xDD2FF8+0x34), 0x524B62 refresh rate = 0; FUN_00522460 (swap-interval setter, no direct callers); rotwk\game.dat_d3d9.log (Windowed false, Present mode VK_PRESENT_MODE_FIFO_KHR, 3 images); dxvk.conf sets only memory and buffer-caching options (no present or latency options)
- **What:** The game requests D3DPRESENT_INTERVAL_DEFAULT, which behaves like vsync. DXVK presents with FIFO. In this setup the game runs exclusive fullscreen with the default refresh rate.
- **Cadence:** per render (Present)
- **Reason:** On a 60 Hz display, vsync paces 60 FPS at exactly 16.67 ms; a pair takes 33.33 ms and a tick 200 ms. That matches stock under vsync, where 33 ms limiter frames snap to 33.33 ms, so game speed is identical. On displays above 60 Hz, FIFO rounds the 16/17 ms limiter targets to refresh multiples, causing judder. Pace on present boundaries, or recommend 60/120 Hz. The deadline-pacing change C2 must tolerate Present blocking.
- **Risk if wrong:** Uneven frame pacing on 144 Hz panels. With FIFO the pair average cannot exceed the stock rate, so game speed does not change.

### G9-15 — timer resolution  [n/a, high]
- **Site:** timeBeginPeriod (IAT 0xBD091C) calls: 0x63A507 (GameEngine ctor 0x63A49C, argument 1 pushed at 0x63A4C5), 0xBCADA2 (static initializer registered at 0xD8AC84, pairs with atexit 0xBCF4D0 → timeEndPeriod at 0xBCF4D2), 0x517A49 (WW3D::Init, from 0x446330), 0xA2EA02 (WWLib timer ctor, from 0x449861); timeEndPeriod (IAT 0xBD0924): 0x63D14F (GameEngine dtor), 0x517AC6 (WW3D::Shutdown, from WinMain), 0xBCF4D2
- **What:** A 1 ms timer period is active from C runtime static initialization (before WinMain) until exit, so timeGetTime and Sleep have about 1 ms granularity. The engine limiter 0x63A196..0x63A1F8 is: timeGetTime; P=_ftol2(1000/(fps*[0xD9F498])) = 33; busy-wait Sleep(0) + timeGetTime until now-last >= P; last = now.
- **Cadence:** event: startup/shutdown
- **Reason:** The DLL does not need its own timeBeginPeriod. A 16/17 ms alternation is representable with timeGetTime. For tighter pacing use QPC (G9-16). From general Windows knowledge (not verified here): on Windows 11, timer-resolution requests may be ignored while the game's window is minimized or fully hidden. That only matters in the minimized Sleep(5) loop.
- **Risk if wrong:** Without 1 ms resolution, Sleep-based pacing would overshoot by up to 15.6 ms. The evidence shows that does not happen here.

### G9-16 — timing source  [n/a, high]
- **Site:** QueryPerformanceCounter IAT 0xBD02E8, QueryPerformanceFrequency 0xBD02EC; game uses: 0x5F5123 (particle manager, only if GlobalData+0x9C2 profiling), 0x6F2364 (path servicing, only if GlobalData+0x11C0), 0x62C159 (benchmark FPS), 0x5FA2CE, 0x5B86A0, 0x65D86D.. (display timing stats), 0x442927/0x44293E, 0xA1EF40/0xA339B0/0xAD96B0 (libraries)
- **What:** QPC and QPF are available (kernel32, always succeed on XP and later; 10 MHz on Windows 10/11). The game's own QPC use is limited to profiling and benchmarks behind GlobalData flags. Logic never depends on it in the retail configuration.
- **Cadence:** n/a
- **Reason:** The DLL can call QPC directly for deadline pacing, using int64 tick arithmetic. Optionally use CreateWaitableTimerExW(CREATE_WAITABLE_TIMER_HIGH_RESOLUTION) for sub-ms waits, then a short spin. No IAT hook is needed.
- **Risk if wrong:** None for availability.

## Open questions

- The GameRanger flag tool (aotr\lotrbfme2ep1.exe, mingw Qt4): which executable it starts (rotwk stub or game.dat), with which working directory and quoting. Not verified; only matters for multiplayer, which stays at 30 FPS.
- The stub appends CRT argv entries without re-quoting (0x409490 loop), so an install path containing spaces would probably split '-mod <path>'. Confirm at runtime (e.g. Process Explorer command line) if the DLL ever parses the command line.
- Whether the '~ WINXPSP3' AppCompat layer on rotwk\lotrbfme2ep1.exe propagates to game.dat through __COMPAT_LAYER, and whether any of its shims affect timers, LoadLibrary or VirtualProtect. game.dat may also carry per-user (HKCU) compatibility layers.
- Windows 11 may ignore timer-resolution requests while the game window is minimized or occluded. Check whether this affects the paused or minimized paths of the limiter.
- Mouse per-render update (TheMouse 0xDE36E0 vt28 and createStreamMessages): not analysed for frame-counted double-click, drag or hold thresholds. Defaulting it to skip_on_B is safe but should be confirmed.
- Whether any AotR campaign or Living World map script uses SET_FPS_LIMIT (0x7CCF20). The C2 LimStub handles any P generically, but cutscene behaviour should be checked.
- Whether AotR_Launcher's self-update (Inno Setup) or version-update path removes unknown files in rotwk\ automatically, without the user accepting PATCH. I only verified the user-triggered PATCH path.
- Display behaviour above 60 Hz with DXVK FIFO and the 16/17 ms limiter: measure frame-time variance. Consider DXVK d3d9.maxFrameRate=60 as a user-side option (dxvk.conf is ignore-listed but overwritten when the DXVK toggle changes).
