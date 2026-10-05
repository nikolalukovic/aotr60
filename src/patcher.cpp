#include "patcher.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

std::vector<uint8_t> EncodeRel32(uint8_t opcode, uint32_t from, const void* to, size_t totalLength)
{
    std::vector<uint8_t> bytes(totalLength, 0x90);
    int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(to) - (from + 5));
    bytes[0] = opcode;
    std::memcpy(&bytes[1], &rel, 4);
    return bytes;
}

std::vector<uint8_t> HexBytes(const char* hex)
{
    std::vector<uint8_t> bytes;
    while (*hex) {
        while (*hex == ' ') {
            ++hex;
        }
        if (!*hex) {
            break;
        }
        char* end = nullptr;
        bytes.push_back(static_cast<uint8_t>(std::strtoul(hex, &end, 16)));
        hex = end;
    }
    return bytes;
}

void PatchSet::Add(Patch patch)
{
    patches_.push_back(std::move(patch));
}

void PatchSet::AddCall(const char* id, uint32_t address, std::vector<uint8_t> original, const void* target)
{
    size_t length = original.size();
    Add({id, address, std::move(original), EncodeRel32(0xE8, address, target, length)});
}

void PatchSet::AddJump(const char* id, uint32_t address, std::vector<uint8_t> original, const void* target)
{
    size_t length = original.size();
    Add({id, address, std::move(original), EncodeRel32(0xE9, address, target, length)});
}

void PatchSet::AddAbsolute(const char* id, uint32_t address, uint32_t originalValue, const void* variable)
{
    std::vector<uint8_t> original(4);
    std::vector<uint8_t> replacement(4);
    uint32_t value = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(variable));
    std::memcpy(original.data(), &originalValue, 4);
    std::memcpy(replacement.data(), &value, 4);
    Add({id, address, std::move(original), std::move(replacement)});
}

bool PatchSet::Verify(std::string* error) const
{
    bool ok = true;
    for (const Patch& p : patches_) {
        if (p.original.size() != p.replacement.size() || p.original.empty()) {
            ok = false;
            if (error) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%s @%08X: replacement length mismatch\n", p.id, p.address);
                *error += buf;
            }
            continue;
        }
        const void* code = reinterpret_cast<const void*>(static_cast<uintptr_t>(p.address));
        if (std::memcmp(code, p.original.data(), p.original.size()) != 0) {
            ok = false;
            if (error) {
                char buf[160];
                std::snprintf(buf, sizeof(buf), "%s @%08X: original bytes differ\n", p.id, p.address);
                *error += buf;
            }
        }
    }
    // Overlapping patches would corrupt each other.
    for (size_t i = 0; i < patches_.size(); ++i) {
        for (size_t j = i + 1; j < patches_.size(); ++j) {
            const Patch& a = patches_[i];
            const Patch& b = patches_[j];
            if (a.address < b.address + b.original.size() && b.address < a.address + a.original.size()) {
                ok = false;
                if (error) {
                    char buf[160];
                    std::snprintf(buf, sizeof(buf), "%s and %s overlap\n", a.id, b.id);
                    *error += buf;
                }
            }
        }
    }
    return ok;
}

bool PatchSet::Apply(std::string* error) const
{
    // Phase 1: make every span writable. Nothing is written unless all of them are.
    std::vector<DWORD> oldProtect(patches_.size(), 0);
    for (size_t i = 0; i < patches_.size(); ++i) {
        const Patch& p = patches_[i];
        void* code = reinterpret_cast<void*>(static_cast<uintptr_t>(p.address));
        if (!VirtualProtect(code, p.replacement.size(), PAGE_EXECUTE_READWRITE, &oldProtect[i])) {
            if (error) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%s @%08X: VirtualProtect failed (%lu)\n", p.id, p.address,
                              GetLastError());
                *error += buf;
            }
            // Restore in reverse order so shared pages end with their original protection.
            for (size_t j = i; j-- > 0;) {
                DWORD unused;
                VirtualProtect(reinterpret_cast<void*>(static_cast<uintptr_t>(patches_[j].address)),
                               patches_[j].replacement.size(), oldProtect[j], &unused);
            }
            return false;
        }
    }
    // Phase 2: write everything, then restore the protections (reverse order) and flush.
    for (const Patch& p : patches_) {
        std::memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(p.address)), p.replacement.data(),
                    p.replacement.size());
    }
    for (size_t j = patches_.size(); j-- > 0;) {
        const Patch& p = patches_[j];
        void* code = reinterpret_cast<void*>(static_cast<uintptr_t>(p.address));
        DWORD unused;
        VirtualProtect(code, p.replacement.size(), oldProtect[j], &unused);
        FlushInstructionCache(GetCurrentProcess(), code, p.replacement.size());
    }
    return true;
}
