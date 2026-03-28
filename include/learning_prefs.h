#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace iris_common {

constexpr std::uint8_t kLearningVersion = 1;
constexpr std::uint16_t kLearningMagic = 0x4C52; // 'LR'

struct LearningPrefs {
    std::uint16_t magic = kLearningMagic;
    std::uint8_t version = kLearningVersion;
    std::uint8_t size = sizeof(LearningPrefs);

    // Biases (0..255, 128 = neutral)
    // 32 pulse slots (length 8 * division 4)
    std::uint8_t pulse_bias[32] = {
        128,128,128,128,128,128,128,128,
        128,128,128,128,128,128,128,128,
        128,128,128,128,128,128,128,128,
        128,128,128,128,128,128,128,128
    };
    std::uint8_t prob_hist[8] = {0};

    std::uint8_t reserved[8] = {0};

    std::uint8_t checksum = 0;

    bool is_valid() const {
        return magic == kLearningMagic && version == kLearningVersion && size == sizeof(LearningPrefs) &&
               checksum == compute_checksum();
    }

    void update_checksum() {
        checksum = compute_checksum();
    }

private:
    std::uint8_t compute_checksum() const {
        // CRC-8 over bytes before checksum only (padding-safe).
        const std::uint8_t* bytes = reinterpret_cast<const std::uint8_t*>(this);
        const std::size_t len = offsetof(LearningPrefs, checksum);
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

static_assert(sizeof(LearningPrefs) <= 64, "LearningPrefs should remain compact");
static_assert(offsetof(LearningPrefs, checksum) < sizeof(LearningPrefs), "LearningPrefs checksum offset must be valid");

} // namespace iris_common
