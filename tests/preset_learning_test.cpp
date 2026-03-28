#include <gtest/gtest.h>

#include "preset.h"
#include "learning_prefs.h"

namespace {

TEST(PresetTest, DefaultPresetIsValidAfterChecksumUpdate) {
    iris_common::Preset p;
    p.update_checksum();
    EXPECT_TRUE(p.is_valid());
}

TEST(PresetTest, FieldMutationInvalidatesChecksum) {
    iris_common::Preset p;
    p.update_checksum();
    ASSERT_TRUE(p.is_valid());

    p.probability = static_cast<std::uint8_t>(p.probability + 1u);
    EXPECT_FALSE(p.is_valid());
}

TEST(PresetTest, EqualsFieldsIgnoresChecksum) {
    iris_common::Preset a;
    a.update_checksum();

    iris_common::Preset b = a;
    b.checksum ^= 0x5Au;

    EXPECT_TRUE(a.equals_fields(b));
    EXPECT_FALSE(b.is_valid());
}

TEST(PresetTest, HeaderMismatchIsInvalid) {
    iris_common::Preset p;
    p.update_checksum();
    ASSERT_TRUE(p.is_valid());

    p.version = 99u;
    p.update_checksum();
    EXPECT_FALSE(p.is_valid());
}

TEST(LearningPrefsTest, DefaultLearningPrefsValidAfterChecksumUpdate) {
    iris_common::LearningPrefs prefs;
    prefs.update_checksum();
    EXPECT_TRUE(prefs.is_valid());
}

TEST(LearningPrefsTest, BiasChangeInvalidatesChecksumUntilUpdated) {
    iris_common::LearningPrefs prefs;
    prefs.update_checksum();
    ASSERT_TRUE(prefs.is_valid());

    prefs.pulse_bias[7] = 200u;
    EXPECT_FALSE(prefs.is_valid());

    prefs.update_checksum();
    EXPECT_TRUE(prefs.is_valid());
}

} // namespace
