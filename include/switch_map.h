#pragma once

#include <cstdint>

namespace iris1::switches {

// Port C three‑way switch mapping (8 switches).
// Each switch uses two GPIOs to encode OFF / PROB / ON.
// From legacy firmware; update if wiring changes.
constexpr uint8_t kSwitchCount = 8;

// Logical switch index -> PC pin number
constexpr uint8_t kSwitchPin1Map[kSwitchCount] = { 9, 15, 5, 11, 1, 7, 3, 13 };
constexpr uint8_t kSwitchPin2Map[kSwitchCount] = { 8, 14, 4, 10, 0, 6, 2, 12 };

} // namespace iris1::switches
