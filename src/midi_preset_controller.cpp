#include "midi_preset_controller.h"

namespace iris1 {

namespace {
static inline bool slot_valid(std::uint8_t slot) {
    return slot < 16u;
}
} // namespace

MidiPresetAction MidiPresetController::on_byte(std::uint8_t byte) {
    // Realtime bytes can interleave and must not disturb running status.
    if (byte >= 0xF8u) {
        return {};
    }

    if (byte & 0x80u) {
        // Track channel voice running status only.
        if (byte >= 0x80u && byte <= 0xEFu) {
            status_ = byte;
            data_count_ = 0u;
        } else {
            // System common/exclusive clears running status.
            status_ = 0u;
            data_count_ = 0u;
        }
        return {};
    }

    if (status_ == 0u) {
        return {};
    }

    const std::uint8_t status_nibble = static_cast<std::uint8_t>(status_ & 0xF0u);

    if (status_nibble == 0xC0u) {
        // Program Change: one data byte.
        if (slot_valid(byte)) {
            if (cc16_value_ == 127u) {
                return {MidiPresetActionType::SaveSlot, byte};
            }
            return {MidiPresetActionType::RecallSlot, byte};
        }
        return {};
    }

    if (status_nibble == 0xB0u) {
        // Control Change: two data bytes.
        if (data_count_ == 0u) {
            data0_ = byte;
            data_count_ = 1u;
            return {};
        }

        data_count_ = 0u;
        if (data0_ != 16u) {
            return {};
        }

        cc16_value_ = byte;
        return {};
    }

    // Other channel messages: consume bytes conservatively to keep parser aligned.
    if (data_count_ == 0u) {
        data_count_ = 1u;
    } else {
        data_count_ = 0u;
    }
    return {};
}

} // namespace iris1
