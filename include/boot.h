#pragma once

#include <cstdint>

namespace iris_common::boot {

// Boot/app flash split used by Iris modules.
constexpr std::uint32_t kFlashBase = 0x08000000u;
constexpr std::uint32_t kBootloaderBytes = 0x00002000u; // 8 KB
constexpr std::uint32_t kAppBase = kFlashBase + kBootloaderBytes;
constexpr std::uint32_t kAppMaxBytes = 0x0000E000u; // 56 KB

// Cortex-M0 on STM32F0 does not support full VTOR remap in the same way as M3/M4.
// Bootloader jumps by copying vectors to SRAM and remapping SRAM to address 0.
inline void relocate_vector_table() {
    // Implement in platform init (requires CMSIS headers).
}

} // namespace iris_common::boot
