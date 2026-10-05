#pragma once
#include <cstdint>
#include <string>
#include <vector>

// All-or-nothing code patching. Every patch states the exact original bytes it replaces; nothing is written
// unless every original matches. Patches are applied once (from DllMain) and never changed afterwards: runtime
// behaviour switches only through DLL globals the stubs read.

struct Patch {
    const char* id;
    uint32_t address;
    std::vector<uint8_t> original;
    std::vector<uint8_t> replacement; // same length as original
};

class PatchSet {
public:
    void Add(Patch patch);

    // `call target` (E8 rel32) at address, padded with NOPs to the original length.
    void AddCall(const char* id, uint32_t address, std::vector<uint8_t> original, const void* target);
    // `jmp target` (E9 rel32) at address, padded with NOPs to the original length.
    void AddJump(const char* id, uint32_t address, std::vector<uint8_t> original, const void* target);
    // Replaces a 4-byte absolute operand (disp32 / imm32 / pointer slot) at address with &variable.
    void AddAbsolute(const char* id, uint32_t address, uint32_t originalValue, const void* variable);

    // Checks every original against memory. On failure, describes the first mismatches in `error`.
    bool Verify(std::string* error) const;
    // Writes every replacement. Call only after Verify succeeded.
    bool Apply(std::string* error) const;

    size_t Size() const { return patches_.size(); }

private:
    std::vector<Patch> patches_;
};

std::vector<uint8_t> EncodeRel32(uint8_t opcode, uint32_t from, const void* to, size_t totalLength);
std::vector<uint8_t> HexBytes(const char* hex); // "8b 01 ff 50 28" -> bytes
