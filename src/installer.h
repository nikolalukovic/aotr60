#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "patcher.h"
#include "sites.gen.h"

// Turns the generated site table (tools/sites.json) into concrete patches: every <rel32:NAME> becomes the
// displacement from the end of its 4 bytes to the DLL symbol, every <abs32:NAME> the symbol's address.

// Replacement bytes of one site, or empty with `error` set when a symbol is unknown.
std::vector<uint8_t> ResolveReplacement(const sites::Site& site, std::string* error);

// Sites -> patch set (no memory access). Phase-4b sites (half-step effect forms) and the split-present
// checkpoints only when requested.
bool BuildSitePatches(PatchSet* patches, std::string* error, bool includePhase4b = false,
                      bool includeSplitPresent = false);

// Build, verify every original against the running game, then write all patches (all or nothing).
bool InstallSites(std::string* error, size_t* count, bool includeSplitPresent = false);
