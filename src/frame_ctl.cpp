// C0 pre/post-render bookkeeping, the 30/60 mode controller and the engine-reset hook (PLAN §1.3, §1.8).

#include "config.h"
#include "frame_state.h"
#include "log.h"
#include "pacer.h"
#include "runtime.h"
#include "split_present.h"

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
// D3DPRESENT_PARAMETERS live at 0xDD2FF8 (hDeviceWindow +0x1C, Windowed +0x20, FullScreen_RefreshRateInHz +0x30,
// PresentationInterval +0x34). Applies to windowed mode too (DWM / DXVK FIFO).
// Evaluated only when those parameters change (device creation, reset, mode switch), never periodically: this runs
// on the game thread before every render, and display queries made there while the game was fullscreen coincided
// with a hitch about once a second. A nonzero FullScreen_RefreshRateInHz is used as is (the engine leaves it 0, so
// in practice the OS is queried once per parameter change).
const char* DisplayBlockReason()
{
    struct Signature {
        uint32_t interval;
        uint32_t windowed;
        uint32_t refresh;
        HWND hwnd;
        bool operator==(const Signature&) const = default;
    };
    static Signature last{};
    static bool valid = false;
    static const char* cached = nullptr;
    Signature now{Read<uint32_t>(0xDD302C), Read<uint32_t>(0xDD3018), Read<uint32_t>(0xDD3028), Read<HWND>(0xDD3014)};
    if (valid && now == last) {
        return cached;
    }
    valid = true;
    last = now;
    cached = nullptr;
    if (now.interval == 0x80000000u) {
        Log("display: no vsync (presentation interval immediate)");
        return cached; // D3DPRESENT_INTERVAL_IMMEDIATE: no vsync
    }
    uint32_t divisor = now.interval == 2 ? 2 : now.interval == 4 ? 3 : now.interval == 8 ? 4 : 1;
    uint32_t hz = 0;
    double queryMs = 0.0;
    if (!now.windowed && now.refresh > 1) {
        hz = now.refresh;
    }
    else {
        LARGE_INTEGER f, t0, t1;
        QueryPerformanceFrequency(&f);
        QueryPerformanceCounter(&t0);
        HMONITOR monitor = MonitorFromWindow(now.hwnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        if (monitor && GetMonitorInfoW(monitor, &info) &&
            EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode)) {
            hz = mode.dmDisplayFrequency;
        }
        QueryPerformanceCounter(&t1);
        queryMs = static_cast<double>(t1.QuadPart - t0.QuadPart) * 1000.0 / static_cast<double>(f.QuadPart);
    }
    if (hz > 1 && hz / divisor < 59) {
        cached = "display refresh below 59 Hz with vsync";
    }
    Log("display: vsync (presentation interval 0x%X), %s, refresh %u Hz%s (query %.2f ms)", now.interval,
        now.windowed ? "windowed" : "fullscreen", hz, cached ? " - 60 FPS not possible" : "", queryMs);
    return cached;
}

// Game mode GL+0x110 (0 campaign / LW battle, 2 skirmish / War of the Ring battle, 6 tutorial, 8 Living World map)
// and the Living World strategic map (phase 6). The map is identified by mode 8 together with the LW view and LW
// logic state, not by GL+0x125: the battle-return handler's GameLogic::reset (0x6BF21B) clears GL+0x125 while the map
// stays shown, and the shell's map preview has GL+0x125 = 0 with the LW view active but suspended. Activation, fades,
// battle transitions and UI sequences on the map stay in 30 mode.
const char* GameModeBlockReason(uint8_t* gl)
{
    uint32_t mode = Field<uint32_t>(gl, 0x110);
    uint8_t* view = Ptr(kLwView);
    uint8_t* lwl = Ptr(kLwLogic);
    bool viewActive = view && Field<uint8_t>(view, 0x18);
    if (mode != 8 && !viewActive && !Field<uint8_t>(gl, 0x125)) {
        switch (mode) {
        case 0:
        case 2:
        case 6:
            return nullptr;
        case 9:
            return "main menu"; // AotR's menus: APT at 30 fps over a still image, nothing to gain at 60
        case 1:
        case 5:
            return "multiplayer";
        case 3:
            return "replay";
        case 7:
            return "Create-a-Hero";
        default:
            return "game mode";
        }
    }
    if (!g_cfg.livingWorldMap) {
        return "Living World strategic map (LivingWorldMap=0)";
    }
    if (mode != 8) {
        return "Living World map transition"; // battle fade-in before the mode returns to 8, shell map preview
    }
    if (!view || !lwl) {
        return "Living World map not ready";
    }
    if (!Field<uint8_t>(lwl, 0xB4) || !Field<uint8_t>(lwl, 0xB5)) {
        return "Living World logic not running";
    }
    if (!viewActive || Field<uint8_t>(view, 0x19)) {
        return "Living World map not shown";
    }
    int32_t state = Field<int32_t>(view, 0x14);
    if (state == 2 || state == 3) {
        return "Living World map fade";
    }
    if (Field<uint8_t>(view, 0x24) || Field<uint8_t>(view, 0x25) || Field<uint8_t>(lwl, 0x176) ||
        Field<uint8_t>(lwl, 0x177)) {
        return "Living World battle transition";
    }
    if (Ptr(kUiSequenceQueue)) {
        return "UI sequence running";
    }
    return nullptr;
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
    if (const char* mode = GameModeBlockReason(gl)) {
        return mode;
    }
    uint32_t mp = Field<uint32_t>(gl, 0x114);
    if (mp == 1 || mp == 2) {
        return "multiplayer session";
    }
    if (Field<uint8_t>(gl, 0x9D)) {
        return "multiplayer start";
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
    if (g_lwRestoreN) {
        Log("WARN %s: Living World objects still drawn halfway - restoring", where);
        ++g_lwStats.iconLateCloses;
        LwPresentClose();
    }
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
    SplitPresentReset(kCancelMode);
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
        swprintf(status, 200, L"[AotR60: 60 FPS%ls%ls] ", g_featPresent ? L", smooth" : L"",
                 SplitPresentActive() ? L", split" : L"");
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
//   Ctrl+Shift+F9   split present (heavy logic steps) on/off
//   Ctrl+Shift+F8   uniform camera scroll on/off
void PollHotkeys()
{
    if (HotkeyPressed(VK_F11, g_hotkeyDown)) {
        g_enabled = !g_enabled;
        Log("hotkey: 60 FPS %s", g_enabled ? "switched on" : "switched off");
    }
    static bool scrollDown = false;
    if (HotkeyPressed(VK_F8, scrollDown)) {
        UniformScrollToggle();
    }
    static bool splitDown = false;
    if (HotkeyPressed(VK_F9, splitDown) && SplitPresentInstalled()) {
        SplitPresentToggleUser();
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
    UniformScrollInit(cfg.uniformScroll);
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
    TelemetryIterationBegin();
    SplitPresentOnC0();
    CloseWindowsSafetyNet("C0");
    PollHotkeys();
    UniformScrollOnC0();

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
        SplitPresentReset(kCancelMode);
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
    PacerOnC0(g_fs.m60 && !g_fs.inB, s);
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
    TelemetryOnPostRender();
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
    SplitPresentReset(kCancelEngineReset);
    UniformScrollOnReset();
    TelemetryOnReset();
}
