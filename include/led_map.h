#pragma once

#include <cstdint>

namespace iris1::leds {

// Port B LED mapping (logical index -> PB pin number).
// From legacy firmware; adjust if board wiring changes.
constexpr uint8_t kLedCount = 16;
constexpr uint8_t kLedPinMap[kLedCount] = {
    9, 10, 11, 14, 13, 8, 12, 15, 0, 1, 2, 3, 4, 5, 6, 7
};

// Only 8 of the 16 LED positions are populated/used in this module.
// Fill these with the logical LED indices you want active.
constexpr uint8_t kLedUsedCount = 8;
constexpr uint8_t kLedUsed[kLedUsedCount] = {
    // TODO: Replace with the 8 logical LED indices actually populated.
    0, 1, 2, 3, 4, 5, 6, 7
};

} // namespace iris1::leds
