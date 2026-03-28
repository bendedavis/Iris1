#pragma once

#include <cstdint>

#include "learning_prefs.h"
#include "eeprom_m95080.h"

namespace iris_common {

class LearningStore {
public:
    explicit LearningStore(std::uint16_t base_addr);

    bool begin_load();
    bool begin_save(const LearningPrefs& prefs);

    void tick();

    bool busy() const;
    bool done() const;

    void finalize_load();

    const LearningPrefs& prefs() const;
    LearningPrefs& prefs_mut();

private:
    std::uint16_t base_addr_ = 0;
    EepromM95080 eeprom_;
    LearningPrefs prefs_;
    bool done_ = false;
    bool loading_ = false;
    bool saving_ = false;
};

} // namespace iris_common
