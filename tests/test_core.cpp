#include "test.h"

#include "build_id.h"
#include "config.h"
#include "crc32.h"

#include <cstring>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

TEST(crc32_matches_zlib_check_value)
{
    CHECK_EQ(Crc32("123456789", 9), 0xCBF43926u);
    CHECK_EQ(Crc32("", 0), 0u);
    // Chained computation equals one-shot.
    CHECK_EQ(Crc32("6789", 4, Crc32("12345", 5)), 0xCBF43926u);
}

TEST(config_defaults_when_empty)
{
    Config cfg = ParseConfig("");
    CHECK(cfg.enabled);
    CHECK(cfg.pacing == Pacing::Stock);
    CHECK_EQ(cfg.telemetry, 0);
    CHECK(cfg.fallback);
    CHECK(!cfg.allowDelayfix);
    CHECK(!cfg.allowLivingWorldMap);
}

TEST(config_default_template_parses_to_defaults)
{
    std::string warnings;
    Config cfg = ParseConfig(kDefaultConfigText, &warnings);
    CHECK(warnings.empty());
    CHECK(cfg.enabled);
    CHECK(cfg.pacing == Pacing::Stock);
    CHECK_EQ(cfg.telemetry, 0);
}

TEST(config_parses_values_comments_and_case)
{
    std::string warnings;
    Config cfg = ParseConfig("[AotR60]\r\n enabled = no ; comment\r\nPACING=Nominal\r\nTelemetry = 2\r\n"
                             "# Fallback = 0\r\nAllowLivingWorldMap = yes\r\nUnknownKey = 5\r\n",
                             &warnings);
    CHECK(!cfg.enabled);
    CHECK(cfg.pacing == Pacing::Nominal);
    CHECK_EQ(cfg.telemetry, 1); // 2 is not implemented yet and maps to 1
    CHECK(cfg.fallback);
    CHECK(cfg.allowLivingWorldMap);
    CHECK(warnings.find("Telemetry=2") != std::string::npos);
}

TEST(config_parses_interpolation_switches)
{
    std::string warnings;
    Config cfg = ParseConfig("UnitInterpolation = 1\nCameraInterpolation = on\n", &warnings);
    CHECK(cfg.unitInterpolation);
    CHECK(cfg.cameraInterpolation);
    CHECK(warnings.empty());
}

TEST(config_reports_invalid_values_and_keeps_defaults)
{
    std::string warnings;
    Config cfg = ParseConfig("Enabled = maybe\nTelemetry = 7\nPacing = fast\n", &warnings);
    CHECK(cfg.enabled);
    CHECK_EQ(cfg.telemetry, 0);
    CHECK(cfg.pacing == Pacing::Stock);
    CHECK(warnings.find("Enabled") != std::string::npos);
    CHECK(warnings.find("Telemetry") != std::string::npos);
    CHECK(warnings.find("Pacing") != std::string::npos);
}

namespace {

// Maps an executable with section layout (as the loader would) without running it.
BuildInfo IdentifyFile(const wchar_t* relativePath)
{
    std::wstring path = std::wstring(L"" AOTR60_GAME_ROOT) + relativePath;
    HMODULE module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE);
    if (!module) {
        std::printf("  cannot map %ls (error %lu)\n", path.c_str(), GetLastError());
        return {};
    }
    auto base = reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(module) & ~uintptr_t{3});
    BuildInfo info = IdentifyBuild(base);
    FreeLibrary(module);
    return info;
}

} // namespace

TEST(build_id_recognises_aotr_normal_game_dat)
{
    BuildInfo info = IdentifyFile(L"\\rotwk\\game.dat");
    CHECK(info.headersValid);
    CHECK_EQ(info.timeDateStamp, 0x460DA09Eu);
    CHECK_EQ(info.entryPointRva, 0x63D082u);
    CHECK_EQ(info.textCrc, 0x879A36B4u);
    CHECK_EQ(info.danettaCrc, 0x6DCD48D0u);
    CHECK_EQ(info.angmarCrc, 0x88C16692u); // loaded image: VirtualSize 0x1000, raw 0x200, zero-filled
    CHECK(info.build == GameBuild::AotrNormal);
}

TEST(build_id_recognises_delayfix)
{
    BuildInfo info = IdentifyFile(L"\\aotr\\zGameDats\\delayfix.dat");
    CHECK(info.build == GameBuild::AotrDelayfix);
}

TEST(build_id_rejects_stock_unpacked_2_01)
{
    BuildInfo info = IdentifyFile(L"\\rotwk\\game820.dat");
    CHECK(info.headersValid);
    CHECK(info.build == GameBuild::Unknown);
}
