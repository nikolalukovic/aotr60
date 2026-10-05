#include "build_id.h"

#include "crc32.h"

#include <cstring>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

constexpr uint32_t kTimeDateStamp = 0x460DA09E;
constexpr uint32_t kEntryPointRva = 0x0063D082; // VA 0xA3D082, see PLAN §7

struct KnownBuild {
    GameBuild build;
    uint32_t textCrc;
    uint32_t danettaCrc;
    uint32_t angmarCrc;
};

// CRCs of the loaded image: .angmar has VirtualSize 0x1000 but only 0x200 raw bytes, so the loader zero-fills the rest.
constexpr KnownBuild kKnownBuilds[] = {
    {GameBuild::AotrNormal, 0x879A36B4, 0x6DCD48D0, 0x88C16692},
    {GameBuild::AotrDelayfix, 0xDEDA7CF9, 0x63F5DCE6, 0x40B06044},
};

uint32_t SectionCrc(const uint8_t* imageBase, const IMAGE_SECTION_HEADER& section)
{
    return Crc32(imageBase + section.VirtualAddress, section.Misc.VirtualSize);
}

bool NameIs(const IMAGE_SECTION_HEADER& section, const char* name)
{
    char buf[IMAGE_SIZEOF_SHORT_NAME + 1] = {};
    std::memcpy(buf, section.Name, IMAGE_SIZEOF_SHORT_NAME);
    return std::strcmp(buf, name) == 0;
}

} // namespace

BuildInfo IdentifyBuild(const uint8_t* imageBase)
{
    BuildInfo info;
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return info;
    }
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(imageBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        return info;
    }
    info.headersValid = true;
    info.timeDateStamp = nt->FileHeader.TimeDateStamp;
    info.entryPointRva = nt->OptionalHeader.AddressOfEntryPoint;

    const IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const IMAGE_SECTION_HEADER& s = sections[i];
        if (NameIs(s, ".text")) {
            info.textCrc = SectionCrc(imageBase, s);
        }
        else if (NameIs(s, ".danetta")) {
            info.danettaCrc = SectionCrc(imageBase, s);
        }
        else if (NameIs(s, ".angmar")) {
            info.angmarCrc = SectionCrc(imageBase, s);
        }
    }

    if (info.timeDateStamp != kTimeDateStamp || info.entryPointRva != kEntryPointRva) {
        return info;
    }
    for (const KnownBuild& known : kKnownBuilds) {
        if (info.textCrc == known.textCrc && info.danettaCrc == known.danettaCrc &&
            info.angmarCrc == known.angmarCrc) {
            info.build = known.build;
            break;
        }
    }
    return info;
}

const char* BuildName(GameBuild build)
{
    switch (build) {
    case GameBuild::AotrNormal:
        return "AotR normal game.dat";
    case GameBuild::AotrDelayfix:
        return "AotR delayfix (PvP mode) game.dat";
    default:
        return "unknown build";
    }
}
