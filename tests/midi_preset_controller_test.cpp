#include <gtest/gtest.h>

#include <vector>

#include "midi_preset_controller.h"

namespace {

using iris1::MidiPresetAction;
using iris1::MidiPresetActionType;
using iris1::MidiPresetController;

static std::vector<MidiPresetAction> feed(MidiPresetController& ctrl, std::initializer_list<std::uint8_t> bytes) {
    std::vector<MidiPresetAction> out;
    for (const std::uint8_t b : bytes) {
        const MidiPresetAction a = ctrl.on_byte(b);
        if (a.type != MidiPresetActionType::None) {
            out.push_back(a);
        }
    }
    return out;
}

TEST(MidiPresetControllerTest, ProgramChangeRecallsSlot) {
    MidiPresetController ctrl;
    const auto actions = feed(ctrl, {0xC0, 5});

    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0].type, MidiPresetActionType::RecallSlot);
    EXPECT_EQ(actions[0].slot, 5u);
}

TEST(MidiPresetControllerTest, ProgramChangeRunningStatusWorks) {
    MidiPresetController ctrl;
    const auto actions = feed(ctrl, {0xC0, 1, 2, 3});

    ASSERT_EQ(actions.size(), 3u);
    EXPECT_EQ(actions[0].slot, 1u);
    EXPECT_EQ(actions[1].slot, 2u);
    EXPECT_EQ(actions[2].slot, 3u);
    EXPECT_EQ(actions[0].type, MidiPresetActionType::RecallSlot);
    EXPECT_EQ(actions[1].type, MidiPresetActionType::RecallSlot);
    EXPECT_EQ(actions[2].type, MidiPresetActionType::RecallSlot);
}

TEST(MidiPresetControllerTest, ProgramChangeOnAnyChannelRecallsSlot) {
    MidiPresetController ctrl;
    const auto actions = feed(ctrl, {0xCF, 9}); // channel 16

    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0].type, MidiPresetActionType::RecallSlot);
    EXPECT_EQ(actions[0].slot, 9u);
}

TEST(MidiPresetControllerTest, CC16Value127MakesProgramChangesSavePersistently) {
    MidiPresetController ctrl;
    const auto actions = feed(ctrl, {
        0xB0, 16, 127, // save mode
        0xC0, 4,       // save slot 4
        0xC0, 5        // save slot 5 (still in save mode)
    });

    ASSERT_EQ(actions.size(), 2u);
    EXPECT_EQ(actions[0].type, MidiPresetActionType::SaveSlot);
    EXPECT_EQ(actions[0].slot, 4u);
    EXPECT_EQ(actions[1].type, MidiPresetActionType::SaveSlot);
    EXPECT_EQ(actions[1].slot, 5u);
}

TEST(MidiPresetControllerTest, CC16Non127MakesProgramChangesRecall) {
    MidiPresetController ctrl;
    const auto actions = feed(ctrl, {
        0xB0, 16, 64, // recall mode
        0xC0, 3,
        0xC0, 4
    });

    ASSERT_EQ(actions.size(), 2u);
    EXPECT_EQ(actions[0].type, MidiPresetActionType::RecallSlot);
    EXPECT_EQ(actions[0].slot, 3u);
    EXPECT_EQ(actions[1].type, MidiPresetActionType::RecallSlot);
    EXPECT_EQ(actions[1].slot, 4u);
}

TEST(MidiPresetControllerTest, ControlChangeOnAnyChannelSupportsSaveMode) {
    MidiPresetController ctrl;
    const auto actions = feed(ctrl, {
        0xB7, 16, 127, // CC16 save mode on channel 8
        0xC2, 6        // save on channel 3
    });

    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0].type, MidiPresetActionType::SaveSlot);
    EXPECT_EQ(actions[0].slot, 6u);
}

TEST(MidiPresetControllerTest, CC16ValueChangeFrom127ToOtherSwitchesToRecall) {
    MidiPresetController ctrl;
    const auto actions = feed(ctrl, {
        0xB0, 16, 127, // save mode
        0xC0, 2,       // save
        0xB0, 16, 40,  // recall mode
        0xC0, 2        // recall
    });

    ASSERT_EQ(actions.size(), 2u);
    EXPECT_EQ(actions[0].type, MidiPresetActionType::SaveSlot);
    EXPECT_EQ(actions[0].slot, 2u);
    EXPECT_EQ(actions[1].type, MidiPresetActionType::RecallSlot);
    EXPECT_EQ(actions[1].slot, 2u);
}

TEST(MidiPresetControllerTest, RealtimeByteDoesNotBreakRunningStatus) {
    MidiPresetController ctrl;
    const auto actions = feed(ctrl, {0xB0, 16, 127, 0xC0, 0xF8, 7});

    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0].type, MidiPresetActionType::SaveSlot);
    EXPECT_EQ(actions[0].slot, 7u);
}

TEST(MidiPresetControllerTest, SystemCommonClearsRunningStatus) {
    MidiPresetController ctrl;
    const auto actions = feed(ctrl, {0xC0, 0xF1, 7});

    EXPECT_TRUE(actions.empty());
}

TEST(MidiPresetControllerTest, InvalidProgramChangeSlotIgnoredAndCc16ModePreserved) {
    MidiPresetController ctrl;
    const auto actions = feed(ctrl, {
        0xB0, 16, 127, // save mode
        0xC0, 20,      // invalid slot, ignored
        0xC0, 4        // next valid PC still saves
    });

    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0].type, MidiPresetActionType::SaveSlot);
    EXPECT_EQ(actions[0].slot, 4u);
}

} // namespace
