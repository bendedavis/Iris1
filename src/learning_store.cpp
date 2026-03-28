#include "learning_store.h"

namespace iris_common {

LearningStore::LearningStore(std::uint16_t base_addr) : base_addr_(base_addr) {}

bool LearningStore::begin_load() {
    if (eeprom_.busy()) return false;
    done_ = false;
    loading_ = true;
    saving_ = false;
    return eeprom_.begin_read(base_addr_, reinterpret_cast<std::uint8_t*>(&prefs_), sizeof(LearningPrefs));
}

bool LearningStore::begin_save(const LearningPrefs& prefs) {
    if (eeprom_.busy()) return false;
    done_ = false;
    loading_ = false;
    saving_ = true;
    prefs_ = prefs;
    prefs_.update_checksum();
    return eeprom_.begin_write(base_addr_, reinterpret_cast<const std::uint8_t*>(&prefs_), sizeof(LearningPrefs));
}

void LearningStore::tick() {
    eeprom_.tick();
    if (!eeprom_.busy() && (loading_ || saving_)) {
        done_ = true;
        loading_ = false;
        saving_ = false;
    }
}

bool LearningStore::busy() const {
    return eeprom_.busy();
}

bool LearningStore::done() const {
    return done_;
}

void LearningStore::finalize_load() {
    if (!prefs_.is_valid()) {
        prefs_ = LearningPrefs{};
        prefs_.update_checksum();
    }
}

const LearningPrefs& LearningStore::prefs() const {
    return prefs_;
}

LearningPrefs& LearningStore::prefs_mut() {
    return prefs_;
}

} // namespace iris_common
