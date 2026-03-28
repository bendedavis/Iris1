#include <gtest/gtest.h>

#include "reset_policy.h"

namespace {

using iris1::ResetPolicy;

TEST(ResetPolicyTest, StartsInAutoMode) {
    ResetPolicy policy;
    policy.reset();
    EXPECT_FALSE(policy.manual_mode());
}

TEST(ResetPolicyTest, AutoResetTriggersAfterMoreThanOnePointFivePulses) {
    ResetPolicy policy;
    policy.reset();

    // Learn expected period = 100 ticks.
    policy.on_clock_rise(100);
    policy.on_clock_rise(200);

    // Exactly 1.5 periods (150 ticks after last rise) should not trigger yet.
    EXPECT_FALSE(policy.should_queue_auto_reset(350));
    // More than 1.5 periods should trigger once.
    EXPECT_TRUE(policy.should_queue_auto_reset(351));
    EXPECT_FALSE(policy.should_queue_auto_reset(400));
}

TEST(ResetPolicyTest, AutoResetRearmsOnNextClockRise) {
    ResetPolicy policy;
    policy.reset();

    policy.on_clock_rise(100);
    policy.on_clock_rise(200);
    EXPECT_TRUE(policy.should_queue_auto_reset(351));

    // New clock edge re-arms auto reset.
    policy.on_clock_rise(500);
    EXPECT_FALSE(policy.should_queue_auto_reset(649)); // <= 1.5 * 300
    EXPECT_TRUE(policy.should_queue_auto_reset(951));  // > 1.5 * 300
}

TEST(ResetPolicyTest, ManualResetEdgeDisablesAutoResetForeverUntilReset) {
    ResetPolicy policy;
    policy.reset();

    policy.on_clock_rise(100);
    policy.on_clock_rise(200);
    policy.on_manual_reset_edge();

    EXPECT_TRUE(policy.manual_mode());
    EXPECT_FALSE(policy.should_queue_auto_reset(10000));
}

} // namespace
