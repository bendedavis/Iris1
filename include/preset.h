#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace iris_common {

// Preset versioning to allow forward/backward compatible expansion.
constexpr std::uint8_t kPresetVersion = 1;
constexpr std::uint16_t kPresetMagic = 0x4952; // 'IR'

struct Preset {
    // Header
    std::uint16_t magic = kPresetMagic;
    std::uint8_t version = kPresetVersion;
    std::uint8_t size = sizeof(Preset);

    // Core fields (v1)
    std::uint8_t mute = 0;             // 0/1
    std::uint8_t probability = 128;    // 0..255
    std::uint8_t sequence_length = 8;  // 1..8 (expandable later)
    std::uint8_t division = 1;         // placeholder, define meaning later

    // Reserved for future expansion (keep size stable and aligned).
    std::uint8_t reserved[8] = {0};

    // Simple checksum (optional). If unused, leave 0.
    std::uint8_t checksum = 0;

    // Validation helper
    bool is_valid() const {
        return magic == kPresetMagic && version == kPresetVersion && size == sizeof(Preset) &&
               checksum == compute_checksum();
    }

    // Recompute checksum after modifying fields.
    void update_checksum() {
        checksum = compute_checksum();
    }

    // Compare meaningful fields (ignores checksum).
    bool equals_fields(const Preset& other) const {
        return magic == other.magic &&
               version == other.version &&
               size == other.size &&
               mute == other.mute &&
               probability == other.probability &&
               sequence_length == other.sequence_length &&
               division == other.division &&
               std::memcmp(reserved, other.reserved, sizeof(reserved)) == 0;
    }

private:
    std::uint8_t compute_checksum() const {
        // CRC-8 (poly 0x07), over bytes before checksum only.
        // This is robust even if the struct has trailing padding.
        const std::uint8_t* bytes = reinterpret_cast<const std::uint8_t*>(this);
        const std::size_t len = offsetof(Preset, checksum);
        std::uint8_t crc = 0x00;
        for (std::size_t i = 0; i < len; ++i) {
            crc ^= bytes[i];
            for (int b = 0; b < 8; ++b) {
                crc = (crc & 0x80) ? static_cast<std::uint8_t>((crc << 1) ^ 0x07) : static_cast<std::uint8_t>(crc << 1);
            }
        }
        return crc;
    }
};

static_assert(sizeof(Preset) <= 32, "Preset should remain compact for EEPROM");
static_assert(offsetof(Preset, checksum) < sizeof(Preset), "Preset checksum offset must be valid");

} // namespace iris_common
