#include <gtest/gtest.h>

#include "midi_recall_timing.h"

namespace {

using iris1::MidiRecallTiming;

TEST(MidiRecallTimingTest, RunningTransportAppliesOnFallAfterWrap) {
    MidiRecallTiming timing;
    timing.request(true);

    timing.on_clock_rise(false);
    EXPECT_FALSE(timing.on_clock_fall(true));

    timing.on_clock_rise(true); // wrapped to step 1
    EXPECT_TRUE(timing.on_clock_fall(true)); // apply on boundary fall
    EXPECT_FALSE(timing.pending());
}

TEST(MidiRecallTimingTest, MissedBoundaryWaitsForNextWrap) {
    MidiRecallTiming timing;
    timing.request(true);

    timing.on_clock_rise(true); // first wrap reached
    EXPECT_FALSE(timing.on_clock_fall(false)); // preset not ready yet
    EXPECT_TRUE(timing.pending());

    timing.on_clock_rise(false);
    EXPECT_FALSE(timing.on_clock_fall(true)); // not boundary fall

    timing.on_clock_rise(true); // next wrap
    EXPECT_TRUE(timing.on_clock_fall(true));
    EXPECT_FALSE(timing.pending());
}

TEST(MidiRecallTimingTest, StoppedTransportHasNoDeferredBoundary) {
    MidiRecallTiming timing;
    timing.request(false);
    EXPECT_FALSE(timing.pending());
}

TEST(MidiRecallTimingTest, StoppingTransportAppliesImmediatelyWhenReady) {
    MidiRecallTiming timing;
    timing.request(true);
    EXPECT_TRUE(timing.pending());

    EXPECT_TRUE(timing.apply_if_stopped(false, true));
    EXPECT_FALSE(timing.pending());
}

} // namespace
