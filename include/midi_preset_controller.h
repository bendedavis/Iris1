#pragma once

#include <cstdint>

namespace iris1 {

enum class MidiPresetActionType : std::uint8_t {
    None = 0,
    RecallSlot,
    SaveSlot,
};

struct MidiPresetAction {
    MidiPresetActionType type = MidiPresetActionType::None;
    std::uint8_t slot = 0; // 0..15 when action type is RecallSlot/SaveSlot
};

// Parses MIDI channel messages and emits preset intents.
//
// Rules (on any MIDI channel):
// - Tracks the latest CC16 value.
// - Program Change 0..15:
//   - if latest CC16 == 127: save slot
//   - otherwise: recall slot
// - Realtime bytes (>= 0xF8) are ignored without affecting parser state.
class MidiPresetController {
public:
    MidiPresetAction on_byte(std::uint8_t byte);

private:
    std::uint8_t status_ = 0;
    std::uint8_t data0_ = 0;
    std::uint8_t data_count_ = 0;
    std::uint8_t cc16_value_ = 0;
};

} // namespace iris1
