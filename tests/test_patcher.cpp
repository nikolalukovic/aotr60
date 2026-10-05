#include "test.h"

#include "patcher.h"

#include <cstring>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

struct CodePage {
    uint8_t* base;
    CodePage() : base(static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READ))) {}
    ~CodePage() { VirtualFree(base, 0, MEM_RELEASE); }
    uint32_t At(size_t offset) const { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(base + offset)); }
    void Fill(size_t offset, const std::vector<uint8_t>& bytes)
    {
        DWORD old;
        VirtualProtect(base, 4096, PAGE_EXECUTE_READWRITE, &old);
        std::memcpy(base + offset, bytes.data(), bytes.size());
        VirtualProtect(base, 4096, old, &old);
    }
};

} // namespace

TEST(hex_bytes_parses_spaced_hex)
{
    std::vector<uint8_t> b = HexBytes("8b 01 ff 50 28");
    CHECK_EQ(b.size(), size_t{5});
    CHECK_EQ(b[0], uint8_t{0x8B});
    CHECK_EQ(b[4], uint8_t{0x28});
    CHECK(HexBytes("").empty());
}

TEST(rel32_encoding_and_nop_padding)
{
    // call from 0x6325CF to 0x00401000 with a 6-byte span
    auto bytes = EncodeRel32(0xE8, 0x6325CF, reinterpret_cast<const void*>(uintptr_t{0x00401000}), 6);
    CHECK_EQ(bytes.size(), size_t{6});
    CHECK_EQ(bytes[0], uint8_t{0xE8});
    int32_t rel;
    std::memcpy(&rel, &bytes[1], 4);
    CHECK_EQ(static_cast<uint32_t>(0x6325CF + 5 + rel), 0x00401000u);
    CHECK_EQ(bytes[5], uint8_t{0x90});
}

TEST(patchset_is_all_or_nothing)
{
    CodePage page;
    page.Fill(0, HexBytes("8b 01 ff 50 28"));
    page.Fill(16, HexBytes("a1 0c f6 d9 00"));

    PatchSet set;
    set.Add({"good", page.At(0), HexBytes("8b 01 ff 50 28"), HexBytes("90 90 90 90 90")});
    set.Add({"bad", page.At(16), HexBytes("a1 0c f6 d9 01"), HexBytes("90 90 90 90 90")});
    std::string error;
    CHECK(!set.Verify(&error));
    CHECK(error.find("bad") != std::string::npos);
    CHECK(error.find("good") == std::string::npos);
    // Nothing applied: memory is unchanged.
    CHECK_EQ(page.base[0], uint8_t{0x8B});
}

TEST(patchset_detects_overlap)
{
    CodePage page;
    page.Fill(0, HexBytes("8b 01 ff 50 28 8b 01"));
    PatchSet set;
    set.Add({"a", page.At(0), HexBytes("8b 01 ff 50 28"), HexBytes("90 90 90 90 90")});
    set.Add({"b", page.At(3), HexBytes("50 28 8b 01"), HexBytes("90 90 90 90")});
    std::string error;
    CHECK(!set.Verify(&error));
    CHECK(error.find("overlap") != std::string::npos);
}

TEST(patchset_applies_call_and_absolute)
{
    CodePage page;
    page.Fill(0, HexBytes("ff 90 9c 00 00 00"));
    page.Fill(32, HexBytes("d8 0d 08 f6 d9 00"));
    static float variable = 0.0f;

    PatchSet set;
    set.AddCall("call", page.At(0), HexBytes("ff 90 9c 00 00 00"), reinterpret_cast<const void*>(page.At(100)));
    set.AddAbsolute("abs", page.At(34), 0x00D9F608, &variable);
    std::string error;
    CHECK(set.Verify(&error));
    CHECK(set.Apply(&error));
    CHECK_EQ(page.base[0], uint8_t{0xE8});
    CHECK_EQ(page.base[5], uint8_t{0x90});
    uint32_t operand;
    std::memcpy(&operand, page.base + 34, 4);
    CHECK_EQ(operand, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&variable)));
}
