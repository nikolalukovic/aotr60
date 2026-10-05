// C0 pre/post-render bookkeeping, the 30/60 mode controller and the engine-reset hook (PLAN §1.3, §1.8).

#include "config.h"
#include "frame_state.h"
#include "log.h"
#include "runtime.h"

#include <cwchar>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace game;

namespace {

Config g_cfg;
bool g_installed = false; // every patch site was written (DllMain)
bool g_enabled = false; // Enabled from aotr60.ini; Ctrl+Shift+F11 toggles it while the game runs
bool g_hotkeyDown = false;
FrameState g_fs;
const char* g_lastBlock = nullptr; // last reason 60 mode was not allowed (logged on change)
bool g_lodNeutral = false;
uint8_t* g_lodChecked = nullptr;

// GameLOD.ini values that logic reads must be neutral: DebrisSkipMask 0, SlowDeathScale 1.0 for every level
// (docs/analysis/audits/lod_neutrality.md §6). Re-checked whenever the LOD manager object changes.
bool LodNeutral()
{
    uint8_t* m = Ptr(kGameLod);
    if (!m) {
        return false;
    }
    if (m != g_lodChecked) {
        bool ok = true;
        for (uint32_t i = 0; i < 5; ++i) {
            ok = ok && Field<uint32_t>(m, 0x1D0 + 16 * i) == 0 && Field<uint32_t>(m, 0x1D4 + 16 * i) == 0x3F800000u;
        }
        g_lodNeutral = ok;
        g_lodChecked = m;
        Log("lod: GameLOD values %s", ok ? "neutral" : "NOT neutral - 60 FPS disabled");
    }
    return g_lodNeutral && Field<uint32_t>(m, 0x1798) == 0 && Field<uint32_t>(m, 0x179C) == 0x3F800000u;
}

// PLAN §1.7: no 60 mode while Presents are tied to a display refreshing below 59 Hz (vsync on). The engine's own
// D3DPRESENT_PARAMETERS live at 0xDD2FF8 (hDeviceWindow +0x1C, PresentationInterval +0x34). Applies to windowed
// mode too (DWM / DXVK FIFO). Cached; re-evaluated about once per second.
const char* DisplayBlockReason()
{
    static DWORD lastCheck = 0;
    static bool valid = false;
    static const char* cached = nullptr;
    DWORD now = GetTickCount();
    if (valid && now - lastCheck < 1000) {
        return cached;
    }
    valid = true;
    lastCheck = now;
    cached = nullptr;
    uint32_t interval = Read<uint32_t>(0xDD302C);
    if (interval == 0x80000000u) {
        return cached; // D3DPRESENT_INTERVAL_IMMEDIATE: no vsync
    }
    uint32_t divisor = interval == 2 ? 2 : interval == 4 ? 3 : interval == 8 ? 4 : 1;
    HWND hwnd = Read<HWND>(0xDD3014);
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (!monitor || !GetMonitorInfoW(monitor, &info) ||
        !EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode)) {
        return cached;
    }
    uint32_t hz = mode.dmDisplayFrequency;
    if (hz > 1 && hz / divisor < 59) {
        cached = "display refresh below 59 Hz with vsync";
    }
    return cached;
}

// Conditions for staying in 60 mode (PLAN §1.8). Returns nullptr when allowed, else the reason.
const char* BlockReason()
{
    if (!g_installed) {
        return "patches not installed";
    }
    if (!g_enabled) {
        return "switched off (aotr60.ini Enabled=0 or Ctrl+Shift+F11)";
    }
    uint8_t* ge = Ptr(kTheGameEngine);
    uint8_t* gl = Ptr(kTheGameLogic);
    uint8_t* gc = Ptr(kTheGameClient);
    uint8_t* gd = Ptr(kGlobalData);
    uint8_t* tv = Ptr(kTacticalView);
    if (!ge || !gl || !gc || !gd || !tv) {
        return "engine not ready";
    }
    if (Ptr(kTheNetwork)) {
        return "network game";
    }
    uint32_t mode = Field<uint32_t>(gl, 0x110);
    if (mode != 0 && mode != 2 && mode != 6) {
        return "game mode (menu, multiplayer or Living World map)";
    }
    uint32_t mp = Field<uint32_t>(gl, 0x114);
    if (mp == 1 || mp == 2) {
        return "multiplayer session";
    }
    if (Field<uint8_t>(gl, 0x9D)) {
        return "multiplayer start";
    }
    if (Field<uint8_t>(gl, 0x125)) {
        return "Living World strategic map";
    }
    uint8_t* rec = Ptr(kRecorder);
    if (rec && Field<uint32_t>(rec, 0x1C) == 1) {
        return "replay playback";
    }
    if (Field<uint8_t>(gd, 0xBBD)) {
        return "fast-forward";
    }
    if (Field<uint8_t>(gd, 0xD45)) {
        return "Create-a-Hero";
    }
    if (Field<uint8_t>(gd, 0xAF2) || Field<uint8_t>(gd, 0xAF3)) {
        return "intro";
    }
    if (!Field<uint8_t>(gd, 0x26)) {
        return "UseFPSLimit off";
    }
    if (Ptr(kScriptDebugDll)) {
        return "script debugger";
    }
    if (g_unknownPath) {
        return "unexpected call path (see log)";
    }
    if (PacerFallbackActive()) {
        return "performance fallback";
    }
    if (!LodNeutral()) {
        return "dynamic LOD not neutral";
    }
    if (const char* display = DisplayBlockReason()) {
        return display;
    }
    return nullptr;
}

// Extra conditions for switching on (not for staying on).
const char* StartBlockReason()
{
    uint8_t* gl = Ptr(kTheGameLogic);
    uint8_t* gc = Ptr(kTheGameClient);
    uint8_t* tv = Ptr(kTacticalView);
    if (Field<uint32_t>(gc, 0x10) < 8) {
        return "first frames of a game";
    }
    // Both are bools; GL+0xA9..0xAB is uninitialised padding.
    if (Field<uint8_t>(gl, 0x9C) || Field<uint8_t>(gl, 0xA8)) {
        return "fade-in";
    }
    if (Field<int32_t>(tv, 0x23D4) > 1) {
        return "camera time multiplier";
    }
    return nullptr;
}

void CloseWindowsSafetyNet(const char* where)
{
    if (g_swapActive) {
        Log("WARN %s: camera swap still active - restoring", where);
        CamSwapEnd();
    }
    if (g_pw1Open || g_pw2Open) {
        Log("WARN %s: presentation window still open - restoring the stock fraction", where);
        if (uint8_t* ge = Ptr(kTheGameEngine)) {
            SetField<float>(ge, 0x3C, g_fracSaved);
        }
        g_pw1Open = 0;
        g_pw2Open = 0;
    }
    g_pwActive = 0;
}

void FlushOwedSync()
{
    int32_t owed = g_syncOwed;
    if (owed != 0) {
        Write<int32_t>(kSyncAccumulator, Read<int32_t>(kSyncAccumulator) + owed);
        g_syncOwed = 0;
    }
}

// Every 60->30 transition (mode controller or reset).
void LeaveSixty(const char* reason)
{
    FlushOwedSync();
    CloseWindowsSafetyNet("leave 60");
    g_m60 = 0;
    g_uiTick = 1;
    g_inB = 0;
    g_skipB = 0;
    g_forceHalt = 0;
    SetIntegratorVariablesForB(false);
    CameraReset();
    C5Reset();
    g_mDrawRid = 0xFFFFFFFFu;
    PacerOnModeChange(false);
    TelemetryOnModeChange(false, reason);
}

// Current state as a prefix of the game window's title (visible in windowed mode; helps testing the hotkeys).
void UpdateTitle(const char* block)
{
    static HWND window = nullptr;
    static wchar_t original[256] = {};
    static wchar_t shown[512] = {};
    if ((g_renderId & 7) != 0) {
        return;
    }
    HWND h = Read<HWND>(0xDD3014); // D3DPRESENT_PARAMETERS.hDeviceWindow
    if (!h) {
        return;
    }
    if (h != window) {
        window = h;
        GetWindowTextW(h, original, 256);
        shown[0] = 0;
    }
    wchar_t status[200];
    if (g_m60) {
        swprintf(status, 200, L"[AotR60: 60 FPS%ls] ", g_featPresent ? L", smooth" : L"");
    }
    else if (int left = PacerFallbackSecondsLeft()) {
        swprintf(status, 200, L"[AotR60: 30 FPS, performance fallback %d s] ", left);
    }
    else {
        wchar_t reason[120] = L"starting";
        if (block) {
            size_t i = 0;
            for (; block[i] && i < 119; ++i) {
                reason[i] = static_cast<wchar_t>(static_cast<unsigned char>(block[i]));
            }
            reason[i] = 0;
        }
        swprintf(status, 200, L"[AotR60: 30 FPS, %ls] ", reason);
    }
    wchar_t title[512];
    swprintf(title, 512, L"%ls%ls", status, original);
    if (wcscmp(title, shown) != 0) {
        SetWindowTextW(h, title);
        wcscpy_s(shown, title);
    }
}

bool HotkeyPressed(int key, bool& wasDown)
{
    bool down = (GetAsyncKeyState(key) & 0x8000) && (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                (GetAsyncKeyState(VK_SHIFT) & 0x8000);
    bool pressed = down && !wasDown;
    wasDown = down;
    if (!pressed) {
        return false;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

// Test hotkeys in the game window, polled at C0 (between renders, so no presentation window is open):
//   Ctrl+Shift+F11  60 FPS on/off (the mode controller applies it at the next pair boundary)
//   Ctrl+Shift+F10  unit + camera interpolation on/off
void PollHotkeys()
{
    if (HotkeyPressed(VK_F11, g_hotkeyDown)) {
        g_enabled = !g_enabled;
        Log("hotkey: 60 FPS %s", g_enabled ? "switched on" : "switched off");
    }
    static bool interpDown = false;
    if (HotkeyPressed(VK_F10, interpDown)) {
        bool on = !g_featPresent;
        g_featPresent = on ? 1 : 0;
        g_featCamInterp = on ? 1 : 0;
        Log("hotkey: unit and camera interpolation %s", on ? "switched on" : "switched off");
    }
}

} // namespace

void FrameControlInit(const Config& cfg, bool installed)
{
    g_cfg = cfg;
    g_installed = installed;
    g_enabled = cfg.enabled;
    g_featPresent = cfg.unitInterpolation ? 1 : 0;
    // The interpolated camera only matches units drawn at the same half step (PLAN §1.2).
    g_featCamInterp = (cfg.cameraInterpolation && cfg.unitInterpolation) ? 1 : 0;
    if (cfg.cameraInterpolation && !cfg.unitInterpolation) {
        Log("config: CameraInterpolation ignored without UnitInterpolation");
    }
}

extern "C" void __cdecl OnPreRender(uint8_t* engine)
{
    if (!g_mainTid) {
        g_mainTid = GetCurrentThreadId();
        Log("frame: main thread %lu, first C0", static_cast<unsigned long>(g_mainTid));
    }
    if (!OnMainThread()) {
        // Never expected: C0 is the main loop. Treat the render as stock.
        g_anomaly |= 0x80000000u;
        return;
    }
    CloseWindowsSafetyNet("C0");
    PollHotkeys();

    const char* block = BlockReason();
    const char* startBlock = block ? block : StartBlockReason();
    const char* shown = g_fs.m60 ? block : startBlock;
    if (shown != g_lastBlock) {
        if (shown && g_enabled) {
            Log("mode: 60 FPS %s: %s", g_fs.m60 ? "stopping" : "not available", shown);
        }
        g_lastBlock = shown;
    }

    uint8_t* gc = Ptr(kTheGameClient);
    int s = Field<int32_t>(engine, 0x34);
    bool c8 = gc && Field<uint8_t>(gc, 0xC8);
    bool was60 = g_fs.m60 != 0;
    g_fs.BeginIteration(block == nullptr, startBlock == nullptr, s, c8);
    if (was60 && !g_fs.m60) {
        LeaveSixty(block ? block : "mode controller");
    }
    else if (!was60 && g_fs.m60) {
        g_syncOwed = 0;
        g_syncHalfPar = 0;
        CameraReset();
        C5Reset();
        g_mDrawRid = 0xFFFFFFFFu;
        PacerOnModeChange(true);
        TelemetryOnModeChange(true, "conditions met");
    }

    g_renderId = g_fs.renderId;
    g_m60 = g_fs.m60;
    g_uiTick = g_fs.uiTick;
    g_inB = g_fs.inB;
    if (g_fs.m60 && g_fs.uiTick) {
        g_presFrac = FrameState::PresentationFraction(s);
    }
    g_inClientUpdate = 1;
    g_skipB = (g_fs.m60 && g_fs.inB) ? 1 : 0;
    if (g_skipB) {
        SetIntegratorVariablesForB(true);
    }
    UpdateTitle(startBlock);
    TelemetryOnPreRender(s);
}

extern "C" void __cdecl OnPostRender()
{
    if (!OnMainThread()) {
        return;
    }
    if (g_skipB) {
        SetIntegratorVariablesForB(false);
    }
    g_skipB = 0;
    g_inClientUpdate = 0;
    CloseWindowsSafetyNet("post-render");
    // The reset hook may have switched 60 mode off during this render; it then reset g_fs as well, so the
    // iteration completes as a stock one.
    g_forceHalt = (g_m60 && g_fs.ForceHalt()) ? 1 : 0;
    // Outside the render everything sees stock values.
    g_uiTick = 1;
    g_inB = 0;
}

extern "C" void __cdecl OnEngineReset()
{
    bool was60 = g_fs.m60 != 0;
    g_fs.Reset();
    if (was60 || g_m60) {
        LeaveSixty("game reset (new game, load, exit)");
    }
    else {
        FlushOwedSync();
        CameraReset();
        C5Reset();
    }
    g_lodChecked = nullptr; // GameLOD may be re-read for the next game
    TelemetryOnReset();
}
