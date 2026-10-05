#pragma once
#include <cstddef>
#include <cstdint>

// zlib-compatible CRC-32 (polynomial 0xEDB88320). Used to identify the game build.
uint32_t Crc32(const void* data, size_t size, uint32_t crc = 0);
