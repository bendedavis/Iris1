#pragma once

#include <cstdint>

#include "preset.h"
#include "eeprom_m95080.h"

namespace iris_common {

class PresetStore {
public:
    explicit PresetStore(std::uint16_t base_addr = 0x0000);

    // Nonblocking load/save API.
    bool begin_load();
    bool begin_save(const Preset& preset);

    // Call from main loop to advance EEPROM state.
    void tick();

    bool busy() const;
    bool done() const;
    bool verify_ok() const;
    std::uint8_t last_status() const;

    // Call after load completes.
    // If EEPROM data is invalid, this applies defaults and uses knob probability.
    void finalize_load(std::uint8_t knob_probability_0_255);
    bool last_load_valid() const;

    const Preset& preset() const;
    Preset& preset_mut();
    void set_base_addr(std::uint16_t base_addr);

private:
    static constexpr std::uint16_t kWireSize = 32;
    static constexpr std::uint8_t kMagicLoOff = 0;
    static constexpr std::uint8_t kMagicHiOff = 1;
    static constexpr std::uint8_t kVersionOff = 2;
    static constexpr std::uint8_t kSizeOff = 3;
    static constexpr std::uint8_t kMuteOff = 4;
    static constexpr std::uint8_t kProbOff = 5;
    static constexpr std::uint8_t kSeqLenOff = 6;
    static constexpr std::uint8_t kDivOff = 7;
    static constexpr std::uint8_t kReservedOff = 8;
    static constexpr std::uint8_t kChecksumOff = 16;

    static std::uint8_t crc8(const std::uint8_t* data, std::uint16_t len);
    void encode_wire(const Preset& preset);
    void decode_wire();

    std::uint16_t base_addr_ = 0x0000;

    EepromM95080 eeprom_;
    Preset preset_;

    bool done_ = false;
    bool loading_ = false;
    bool saving_ = false;
    bool verifying_ = false;
    bool last_valid_ = true;
    bool verify_ok_ = false;
    std::uint8_t write_wire_[kWireSize] = {0};
    std::uint8_t read_wire_[kWireSize] = {0};
    std::uint8_t verify_wire_[kWireSize] = {0};
};

} // namespace iris_common
