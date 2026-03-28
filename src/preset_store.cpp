#include "preset_store.h"
#include <cstring>

namespace iris_common {

PresetStore::PresetStore(std::uint16_t base_addr) : base_addr_(base_addr) {}

std::uint8_t PresetStore::crc8(const std::uint8_t* data, std::uint16_t len) {
    std::uint8_t crc = 0x00;
    for (std::uint16_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x80u) ? static_cast<std::uint8_t>((crc << 1) ^ 0x07u)
                                : static_cast<std::uint8_t>(crc << 1);
        }
    }
    return crc;
}

void PresetStore::encode_wire(const Preset& preset) {
    std::memset(write_wire_, 0, sizeof(write_wire_));
    write_wire_[kMagicLoOff] = static_cast<std::uint8_t>(preset.magic & 0xFFu);
    write_wire_[kMagicHiOff] = static_cast<std::uint8_t>((preset.magic >> 8) & 0xFFu);
    write_wire_[kVersionOff] = preset.version;
    write_wire_[kSizeOff] = preset.size;
    write_wire_[kMuteOff] = preset.mute;
    write_wire_[kProbOff] = preset.probability;
    write_wire_[kSeqLenOff] = preset.sequence_length;
    write_wire_[kDivOff] = preset.division;
    std::memcpy(&write_wire_[kReservedOff], preset.reserved, sizeof(preset.reserved));
    write_wire_[kChecksumOff] = crc8(write_wire_, kChecksumOff);
}

void PresetStore::decode_wire() {
    preset_.magic = static_cast<std::uint16_t>(read_wire_[kMagicLoOff]) |
                    static_cast<std::uint16_t>(static_cast<std::uint16_t>(read_wire_[kMagicHiOff]) << 8);
    preset_.version = read_wire_[kVersionOff];
    preset_.size = read_wire_[kSizeOff];
    preset_.mute = read_wire_[kMuteOff];
    preset_.probability = read_wire_[kProbOff];
    preset_.sequence_length = read_wire_[kSeqLenOff];
    preset_.division = read_wire_[kDivOff];
    std::memcpy(preset_.reserved, &read_wire_[kReservedOff], sizeof(preset_.reserved));
    preset_.checksum = read_wire_[kChecksumOff];
}

bool PresetStore::begin_load() {
    if (eeprom_.busy()) return false;
    done_ = false;
    loading_ = true;
    saving_ = false;
    verifying_ = false;
    verify_ok_ = false;
    return eeprom_.begin_read(base_addr_, read_wire_, kWireSize);
}

bool PresetStore::begin_save(const Preset& preset) {
    if (eeprom_.busy()) return false;
    done_ = false;
    loading_ = false;
    saving_ = true;
    verifying_ = false;
    verify_ok_ = false;
    preset_ = preset;
    preset_.update_checksum();
    encode_wire(preset_);
    return eeprom_.begin_write(base_addr_, write_wire_, kWireSize);
}

void PresetStore::tick() {
    eeprom_.tick();
    if (!eeprom_.busy()) {
        if (saving_ && !verifying_) {
            // Start readback verify.
            verifying_ = true;
            verify_ok_ = false;
            if (!eeprom_.begin_read(base_addr_, verify_wire_, kWireSize)) {
                verifying_ = false;
                saving_ = false;
                done_ = true;
            }
            return;
        }
        if (verifying_) {
            verify_ok_ = (std::memcmp(verify_wire_, write_wire_, kWireSize) == 0);
            verifying_ = false;
            saving_ = false;
            done_ = true;
        } else if (loading_) {
            decode_wire();
            done_ = true;
            loading_ = false;
        }
    }
}

bool PresetStore::busy() const {
    return eeprom_.busy();
}

bool PresetStore::done() const {
    return done_;
}

bool PresetStore::verify_ok() const {
    return verify_ok_;
}

std::uint8_t PresetStore::last_status() const {
    return eeprom_.last_status();
}

void PresetStore::finalize_load(std::uint8_t knob_probability_0_255) {
    if (!preset_.is_valid()) {
        preset_ = Preset{};
        preset_.probability = knob_probability_0_255;
        preset_.update_checksum();
        last_valid_ = false;
    } else {
        last_valid_ = true;
    }
}

bool PresetStore::last_load_valid() const {
    return last_valid_;
}

const Preset& PresetStore::preset() const {
    return preset_;
}

Preset& PresetStore::preset_mut() {
    return preset_;
}

void PresetStore::set_base_addr(std::uint16_t base_addr) {
    base_addr_ = base_addr;
}

} // namespace iris_common
