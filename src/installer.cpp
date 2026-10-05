#include "installer.h"

#include "symbols.h"

#include <cstring>

std::vector<uint8_t> ResolveReplacement(const sites::Site& site, std::string* error)
{
    std::vector<uint8_t> bytes;
    bytes.reserve(site.length);
    for (uint32_t i = 0; i < site.tokenCount; ++i) {
        const sites::Token& t = site.replacement[i];
        if (t.type == sites::TokenType::Byte) {
            bytes.push_back(t.value);
            continue;
        }
        const void* symbol = FindSymbol(t.symbol);
        if (!symbol) {
            if (error) {
                *error += std::string(site.id) + ": unknown symbol " + t.symbol + "\n";
            }
            return {};
        }
        uint32_t target = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(symbol));
        uint32_t value = target;
        if (t.type == sites::TokenType::Rel32) {
            uint32_t next = site.address + static_cast<uint32_t>(bytes.size()) + 4;
            value = target - next;
        }
        uint8_t raw[4];
        std::memcpy(raw, &value, 4);
        bytes.insert(bytes.end(), raw, raw + 4);
    }
    if (bytes.size() != site.length) {
        if (error) {
            *error += std::string(site.id) + ": replacement length differs from the original span\n";
        }
        return {};
    }
    return bytes;
}

bool BuildSitePatches(PatchSet* patches, std::string* error, bool includePhase4b)
{
    bool ok = true;
    for (uint32_t i = 0; i < sites::kSiteCount; ++i) {
        const sites::Site& site = sites::kSites[i];
        if (site.phase == sites::Phase::Phase4b && !includePhase4b) {
            continue;
        }
        std::vector<uint8_t> replacement = ResolveReplacement(site, error);
        if (replacement.empty()) {
            ok = false;
            continue;
        }
        patches->Add({site.id, site.address, std::vector<uint8_t>(site.original, site.original + site.length),
                      std::move(replacement)});
    }
    return ok;
}

bool InstallSites(std::string* error, size_t* count)
{
    PatchSet patches;
    if (!BuildSitePatches(&patches, error)) {
        return false;
    }
    if (!patches.Verify(error)) {
        return false;
    }
    if (!patches.Apply(error)) {
        return false;
    }
    if (count) {
        *count = patches.Size();
    }
    return true;
}
