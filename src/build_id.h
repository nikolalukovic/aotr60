#pragma once
#include <cstdint>

// Identification of the game executable the proxy was loaded into.
// Only known builds are ever patched; everything else runs untouched (stock 30 FPS).

enum class GameBuild {
    Unknown,
    AotrNormal,    // rotwk\game.dat as installed by the AotR launcher
    AotrDelayfix,  // aotr\zGameDats\delayfix.dat (launcher "PvP mode"); known but not patched
};

struct BuildInfo {
    GameBuild build = GameBuild::Unknown;
    uint32_t timeDateStamp = 0;
    uint32_t entryPointRva = 0;
    uint32_t textCrc = 0;
    uint32_t danettaCrc = 0;
    uint32_t angmarCrc = 0;
    bool headersValid = false;
};

// imageBase points at a PE image mapped with section alignment (as the loader maps it).
// CRCs are zlib CRC-32 over [VA, VA + VirtualSize) of .text, .danetta and .angmar.
BuildInfo IdentifyBuild(const uint8_t* imageBase);

const char* BuildName(GameBuild build);
