#include "test.h"

#include "installer.h"
#include "symbols.h"

#include <cstring>
#include <set>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

constexpr uint32_t kImageBase = 0x400000;

struct MappedImage {
    HMODULE module = nullptr;
    const uint8_t* base = nullptr;
    explicit MappedImage(const wchar_t* relativePath)
    {
        std::wstring path = std::wstring(L"" AOTR60_GAME_ROOT) + relativePath;
        module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE);
        if (module) {
            base = reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(module) & ~uintptr_t{3});
        }
    }
    ~MappedImage()
    {
        if (module) {
            FreeLibrary(module);
        }
    }
    const uint8_t* At(uint32_t va) const { return base + (va - kImageBase); }
};

} // namespace

TEST(sites_every_placeholder_resolves)
{
    std::string error;
    PatchSet patches;
    CHECK(BuildSitePatches(&patches, &error, true));
    if (!error.empty()) {
        std::printf("  %s", error.c_str());
    }
    CHECK_EQ(patches.Size(), static_cast<size_t>(sites::kSiteCount));
}

TEST(sites_rel32_operands_reach_their_symbols)
{
    for (uint32_t i = 0; i < sites::kSiteCount; ++i) {
        const sites::Site& site = sites::kSites[i];
        std::vector<uint8_t> bytes = ResolveReplacement(site, nullptr);
        CHECK_EQ(bytes.size(), static_cast<size_t>(site.length));
        uint32_t offset = 0;
        for (uint32_t t = 0; t < site.tokenCount; ++t) {
            const sites::Token& token = site.replacement[t];
            if (token.type == sites::TokenType::Byte) {
                CHECK_EQ(bytes[offset], token.value);
                offset += 1;
                continue;
            }
            uint32_t value;
            std::memcpy(&value, &bytes[offset], 4);
            uint32_t symbol = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(FindSymbol(token.symbol)));
            if (token.type == sites::TokenType::Rel32) {
                CHECK_EQ(site.address + offset + 4 + value, symbol);
                CHECK(offset >= 1); // always the operand of an e8/e9 (or 0f 8x) branch
            }
            else {
                CHECK_EQ(value, symbol);
            }
            offset += 4;
        }
    }
}

TEST(sites_symbol_table_has_no_unused_entries)
{
    std::set<std::string> used;
    for (uint32_t i = 0; i < sites::kSiteCount; ++i) {
        for (uint32_t t = 0; t < sites::kSites[i].tokenCount; ++t) {
            if (sites::kSites[i].replacement[t].symbol) {
                used.insert(sites::kSites[i].replacement[t].symbol);
            }
        }
    }
    size_t count = 0;
    const Symbol* all = AllSymbols(&count);
    std::set<const void*> addresses;
    for (size_t i = 0; i < count; ++i) {
        if (!used.count(all[i].name)) {
            std::printf("  unused symbol %s\n", all[i].name);
            CHECK(false);
        }
        CHECK(all[i].address != nullptr);
        CHECK(addresses.insert(all[i].address).second); // no two names for one stub
    }
}

TEST(sites_originals_match_game_dat)
{
    MappedImage image(L"\\rotwk\\game.dat");
    CHECK(image.base != nullptr);
    if (!image.base) {
        return;
    }
    for (uint32_t i = 0; i < sites::kSiteCount; ++i) {
        const sites::Site& site = sites::kSites[i];
        if (std::memcmp(image.At(site.address), site.original, site.length) != 0) {
            std::printf("  %s @%08X: original bytes differ from game.dat\n", site.id, site.address);
            CHECK(false);
        }
    }
}

TEST(sites_do_not_overlap)
{
    for (uint32_t i = 0; i < sites::kSiteCount; ++i) {
        for (uint32_t j = i + 1; j < sites::kSiteCount; ++j) {
            const sites::Site& a = sites::kSites[i];
            const sites::Site& b = sites::kSites[j];
            bool overlap = a.address < b.address + b.length && b.address < a.address + a.length;
            if (overlap) {
                std::printf("  %s overlaps %s\n", a.id, b.id);
            }
            CHECK(!overlap);
        }
    }
}

TEST(sites_integrator_constants_match_game_dat)
{
    // runtime.cpp copies these at startup; the defaults must equal them so 30 mode is exact either way.
    MappedImage image(L"\\rotwk\\game.dat");
    if (!image.base) {
        CHECK(false);
        return;
    }
    auto f = [&](uint32_t va) {
        float v;
        std::memcpy(&v, image.At(va), 4);
        return v;
    };
    CHECK(f(0xBDAD70) == 0.6f);
    CHECK(f(0xBDC540) == 0.03f);
    CHECK(f(0xBE5228) == 0.0225f);
    CHECK(f(0xBDC320) == 0.02f);
    CHECK(f(0xBDD760) == 0.05f);
    CHECK(f(0xBDE8D8) == 0.8f);
    double d;
    std::memcpy(&d, image.At(0xBE5220), 8);
    CHECK(d == 0.005);
    CHECK(f(0xBD1908) == 1.0f);
    CHECK(f(0xD9A3A0) == 0.005f);
    CHECK(f(0xBDC1FC) == 1.0f / 60.0f);
    CHECK(f(0xBD88C8) == 1.0f / 12.0f);
    int32_t ltr;
    std::memcpy(&ltr, image.At(0xD9F608), 4);
    CHECK_EQ(ltr, 5);
}
