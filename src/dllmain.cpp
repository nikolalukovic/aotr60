// AotR60 entry point. Runs inside the game process before the game's own code: identifies the game build and,
// for a known build, installs the 60 FPS patches. Anything unexpected leaves the game untouched (stock 30 FPS);
// the DLL then only forwards DirectInput8Create.

#include "build_id.h"
#include "config.h"
#include "log.h"
#include "paths.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstring>
#include <string>

namespace {

constexpr const char kVersion[] = "AotR60 0.1 (phase 0: passthrough)";
constexpr uintptr_t kGameImageBase = 0x00400000;

Config LoadConfig(const std::wstring& dataDir)
{
    std::wstring path = dataDir + L"\\aotr60.ini";
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        HANDLE created = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
                                     nullptr);
        if (created != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(created, kDefaultConfigText, static_cast<DWORD>(std::strlen(kDefaultConfigText)), &written,
                      nullptr);
            CloseHandle(created);
            Log("config: wrote default %ls", path.c_str());
        }
        return Config{};
    }
    std::string text;
    char buf[4096];
    DWORD read;
    while (ReadFile(file, buf, sizeof(buf), &read, nullptr) && read > 0) {
        text.append(buf, read);
    }
    CloseHandle(file);
    std::string warnings;
    Config cfg = ParseConfig(text, &warnings);
    if (!warnings.empty()) {
        Log("config: %s", warnings.c_str());
    }
    return cfg;
}

void Startup()
{
    std::wstring dataDir = DataDirectory();
    if (!dataDir.empty()) {
        LogOpen(dataDir + L"\\aotr60.log");
    }
    std::wstring exe = ExecutablePath();
    Log("%s loaded into %ls", kVersion, exe.c_str());

    Config cfg = dataDir.empty() ? Config{} : LoadConfig(dataDir);
    Log("config: Enabled=%d Pacing=%s Telemetry=%d Fallback=%d", cfg.enabled ? 1 : 0,
        cfg.pacing == Pacing::Stock ? "stock" : "nominal", cfg.telemetry, cfg.fallback ? 1 : 0);

    auto base = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
    if (reinterpret_cast<uintptr_t>(base) != kGameImageBase) {
        Log("host: image base %p is not the game's 0x400000; forwarding only", base);
        return;
    }
    BuildInfo info = IdentifyBuild(base);
    Log("host: TimeDateStamp=0x%08X EntryPointRva=0x%06X CRC .text=%08X .danetta=%08X .angmar=%08X -> %s",
        info.timeDateStamp, info.entryPointRva, info.textCrc, info.danettaCrc, info.angmarCrc,
        BuildName(info.build));

    switch (info.build) {
    case GameBuild::AotrNormal:
        Log("host: known build; 60 FPS patches arrive in phase 2 - running stock 30 FPS");
        break;
    case GameBuild::AotrDelayfix:
        Log("host: known build, 60 FPS not supported for delayfix (launcher PvP mode) - running stock 30 FPS");
        break;
    default:
        Log("host: unknown build - running stock 30 FPS, nothing patched");
        break;
    }
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        Startup();
    }
    return TRUE;
}
