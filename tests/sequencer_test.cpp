#include <gtest/gtest.h>
#include <cstdint>

#include "sequencer.h"

namespace {

using iris1::Sequencer;
using iris1::StepMode;

static void make_single_step_on(Sequencer& seq) {
    seq.set_length(1);
    seq.set_division(1);
    seq.set_probability(255);
    seq.set_step_mode(0, StepMode::On);
}

static std::uint8_t count_cycle_on_pulses(Sequencer& seq, std::uint8_t pulse_count) {
    std::uint8_t on_count = 0u;

    seq.on_clock_fall(0);
    if (seq.clock_route_high()) {
        ++on_count;
    }

    for (std::uint8_t i = 1u; i < pulse_count; ++i) {
        seq.on_clock_rise(0);
        seq.on_clock_fall(0);
        if (seq.clock_route_high()) {
            ++on_count;
        }
    }

    return on_count;
}

static std::uint8_t max_consecutive_misses_for_step(Sequencer& seq, std::uint8_t pulse_count, std::uint8_t target_step) {
    std::uint8_t miss_run = 0u;
    std::uint8_t max_miss_run = 0u;
    std::uint32_t t = 0u;

    auto sample = [&]() {
        if (seq.queued_step_index() != target_step) {
            return;
        }
        seq.tick(t + 100u);
        if (seq.clock_route_high()) {
            miss_run = 0u;
            return;
        }
        ++miss_run;
        if (miss_run > max_miss_run) {
            max_miss_run = miss_run;
        }
    };

    seq.on_clock_fall(t);
    sample();

    for (std::uint8_t i = 1u; i < pulse_count; ++i) {
        t += 10u;
        seq.on_clock_rise(t);
        t += 1u;
        seq.on_clock_fall(t);
        sample();
    }

    return max_miss_run;
}

TEST(SequencerTest, RoutesImmediatelyWhenPeriodUnknown) {
    Sequencer seq;
    make_single_step_on(seq);

    seq.on_clock_fall(0);
    EXPECT_TRUE(seq.clock_route_high());
}

TEST(SequencerTest, RoutesAtMidpointWhenPeriodKnown) {
    Sequencer seq;
    make_single_step_on(seq);

    // First cycle before period estimate is available.
    seq.on_clock_fall(0);
    EXPECT_TRUE(seq.clock_route_high());
    seq.on_clock_rise(10);

    // Still no period yet (need two rises).
    seq.on_clock_fall(20);
    EXPECT_TRUE(seq.clock_route_high());
    seq.on_clock_rise(30); // rise-to-rise period=20 => half=10

    // Now routing should wait until midpoint after fall.
    seq.on_clock_fall(40);
    EXPECT_FALSE(seq.clock_route_high());
    seq.tick(49);
    EXPECT_FALSE(seq.clock_route_high());
    seq.tick(50);
    EXPECT_TRUE(seq.clock_route_high());
}

TEST(SequencerTest, DivisionAndStepProgressionAreStable) {
    Sequencer seq;
    seq.set_length(2);
    seq.set_division(2);
    seq.set_probability(255);
    seq.set_step_mode(0, StepMode::On);
    seq.set_step_mode(1, StepMode::On);

    seq.on_clock_fall(0);

    EXPECT_FALSE(seq.on_clock_rise(10));
    EXPECT_EQ(seq.step_index(), 0u);
    EXPECT_EQ(seq.pulse_index(), 0u);

    EXPECT_FALSE(seq.on_clock_rise(20));
    EXPECT_EQ(seq.step_index(), 0u);
    EXPECT_EQ(seq.pulse_index(), 1u);

    EXPECT_FALSE(seq.on_clock_rise(30));
    EXPECT_EQ(seq.step_index(), 1u);
    EXPECT_EQ(seq.pulse_index(), 0u);

    EXPECT_TRUE(seq.on_clock_rise(40));
    EXPECT_EQ(seq.step_index(), 1u);
    EXPECT_EQ(seq.pulse_index(), 1u);
}

TEST(SequencerTest, QueuedStepIndexTracksNextStageAcrossDivisionBoundary) {
    Sequencer seq;
    seq.set_length(2);
    seq.set_division(4);
    seq.set_probability(255);
    seq.set_step_mode(0, StepMode::On);
    seq.set_step_mode(1, StepMode::On);

    seq.on_clock_fall(0);
    EXPECT_EQ(seq.queued_step_index(), 0u);

    seq.on_clock_rise(10);
    EXPECT_EQ(seq.queued_step_index(), 0u);

    seq.on_clock_rise(20);
    EXPECT_EQ(seq.queued_step_index(), 0u);

    seq.on_clock_rise(30);
    EXPECT_EQ(seq.queued_step_index(), 0u);

    seq.on_clock_rise(40);
    EXPECT_EQ(seq.queued_step_index(), 1u);
}

TEST(SequencerTest, DivisionChangePreservesAbsolutePhaseWhenRestored) {
    Sequencer seq;
    seq.set_length(8);
    seq.set_division(1);
    seq.set_probability(255);
    for (std::uint8_t i = 0; i < 8; ++i) {
        seq.set_step_mode(i, StepMode::On);
    }

    std::uint32_t t = 0;
    seq.on_clock_fall(t);
    for (int i = 0; i < 5; ++i) {
        t += 10;
        seq.on_clock_rise(t);
        t += 1;
        seq.on_clock_fall(t);
    }

    EXPECT_EQ(seq.queued_step_index(), 5u);

    seq.set_division(4);
    t += 1;
    seq.on_clock_fall(t);
    EXPECT_EQ(seq.queued_step_index(), 1u);

    seq.set_division(1);
    t += 1;
    seq.on_clock_fall(t);
    EXPECT_EQ(seq.queued_step_index(), 5u);
}

TEST(SequencerTest, MandatoryOnStepsDoNotConsumeProbabilityBudgetAtDivideOne) {
    Sequencer seq;
    seq.set_length(8);
    seq.set_division(1);
    seq.set_probability(128);
    seq.set_step_mode(0, StepMode::On);
    seq.set_step_mode(1, StepMode::On);
    seq.set_step_mode(2, StepMode::Prob);
    seq.set_step_mode(3, StepMode::Prob);
    seq.set_step_mode(4, StepMode::Off);
    seq.set_step_mode(5, StepMode::Off);
    seq.set_step_mode(6, StepMode::Off);
    seq.set_step_mode(7, StepMode::Off);

    // Two fixed anchors plus half of the two probabilistic slots => 3 total ON pulses.
    EXPECT_EQ(count_cycle_on_pulses(seq, 8u), 3u);
}

TEST(SequencerTest, SingleDivideOneProbStepHitsHalfTheTimeAtHalfProbability) {
    Sequencer seq;
    seq.set_length(8);
    seq.set_division(1);
    seq.set_probability(128);
    seq.set_step_mode(0, StepMode::On);
    seq.set_step_mode(1, StepMode::On);
    seq.set_step_mode(2, StepMode::Prob);
    seq.set_step_mode(3, StepMode::Off);
    seq.set_step_mode(4, StepMode::Off);
    seq.set_step_mode(5, StepMode::Off);
    seq.set_step_mode(6, StepMode::Off);
    seq.set_step_mode(7, StepMode::Off);

    // Eight 8-step passes = 64 pulses:
    // 16 mandatory pulses from the two fixed anchors, plus a governed
    // probabilistic lane that should stay very close to 50% over time.
    const std::uint8_t total = count_cycle_on_pulses(seq, 64u);
    EXPECT_GE(total, 20u);
    EXPECT_LE(total, 21u);
}

TEST(SequencerTest, SparseDivideOneProbabilityDistributesAcrossWindow) {
    Sequencer seq;
    seq.set_length(8);
    seq.set_division(1);
    seq.set_probability(32);
    seq.set_step_mode(0, StepMode::On);
    seq.set_step_mode(1, StepMode::On);
    seq.set_step_mode(2, StepMode::Prob);
    seq.set_step_mode(3, StepMode::Prob);
    seq.set_step_mode(4, StepMode::Off);
    seq.set_step_mode(5, StepMode::Off);
    seq.set_step_mode(6, StepMode::Off);
    seq.set_step_mode(7, StepMode::Off);

    // Four cycles:
    // 8 mandatory pulses from the two fixed anchors, plus two sparse
    // probabilistic hits from the seeded one-pass generator.
    EXPECT_EQ(count_cycle_on_pulses(seq, 32u), 10u);
}

TEST(SequencerTest, SparsePrimaryProbLaneGetsDroughtGuard) {
    Sequencer seq;
    seq.set_length(8);
    seq.set_division(1);
    seq.set_probability(32);
    seq.set_step_mode(0, StepMode::On);
    seq.set_step_mode(1, StepMode::On);
    seq.set_step_mode(2, StepMode::Prob);
    seq.set_step_mode(3, StepMode::Off);
    seq.set_step_mode(4, StepMode::Off);
    seq.set_step_mode(5, StepMode::Off);
    seq.set_step_mode(6, StepMode::Off);
    seq.set_step_mode(7, StepMode::Off);

    // With global self-correction enabled, low-probability main-step lanes may
    // still be sparse, but they should not stay dead indefinitely.
    EXPECT_LE(max_consecutive_misses_for_step(seq, 160u, 2u), 9u);
}

TEST(SequencerTest, DividedDensityUsesFullCycleButExcludesFixedAnchors) {
    Sequencer seq;
    seq.set_length(8);
    seq.set_division(4);
    seq.set_probability(128);
    seq.set_step_mode(0, StepMode::On);
    for (std::uint8_t i = 1; i < 8; ++i) {
        seq.set_step_mode(i, StepMode::Prob);
    }

    // 32 pulse positions total, with 1 fixed anchor outside the probability budget.
    // One seeded cycle yields 1 mandatory + 16 probabilistic hits = 17 total.
    EXPECT_EQ(count_cycle_on_pulses(seq, 32u), 17u);
}

TEST(SequencerTest, LengthChangePreservesAbsolutePhaseWhenRestored) {
    Sequencer seq;
    seq.set_length(8);
    seq.set_division(1);
    seq.set_probability(255);
    for (std::uint8_t i = 0; i < 8; ++i) {
        seq.set_step_mode(i, StepMode::On);
    }

    std::uint32_t t = 0;
    seq.on_clock_fall(t);
    for (int i = 0; i < 10; ++i) {
        t += 10;
        seq.on_clock_rise(t);
        t += 1;
        seq.on_clock_fall(t);
    }

    EXPECT_EQ(seq.queued_step_index(), 2u);

    seq.set_length(7);
    t += 1;
    seq.on_clock_fall(t);
    EXPECT_EQ(seq.queued_step_index(), 3u);

    seq.set_length(8);
    t += 1;
    seq.on_clock_fall(t);
    EXPECT_EQ(seq.queued_step_index(), 2u);
}

TEST(SequencerTest, DivisionClampsToFourAndPulseWraps) {
    Sequencer seq;
    seq.set_length(1);
    seq.set_division(99);
    seq.set_probability(255);
    seq.set_step_mode(0, StepMode::On);

    EXPECT_EQ(seq.division(), 4u);

    seq.on_clock_fall(0);
    seq.on_clock_rise(10);
    EXPECT_EQ(seq.pulse_index(), 0u);

    seq.on_clock_rise(20);
    EXPECT_EQ(seq.pulse_index(), 1u);

    seq.on_clock_rise(30);
    EXPECT_EQ(seq.pulse_index(), 2u);

    seq.on_clock_rise(40);
    EXPECT_EQ(seq.pulse_index(), 3u);

    seq.on_clock_rise(50);
    EXPECT_EQ(seq.pulse_index(), 0u);
}

TEST(SequencerTest, LengthClampToOneKeepsStepAtZero) {
    Sequencer seq;
    seq.set_length(0);
    seq.set_division(1);
    seq.set_probability(255);
    seq.set_step_mode(0, StepMode::On);

    seq.on_clock_fall(0);
    for (std::uint32_t i = 1; i <= 6; ++i) {
        seq.on_clock_rise(i * 10);
        EXPECT_EQ(seq.step_index(), 0u);
    }
}

TEST(SequencerTest, ResetPendingClearsOnNextClockRise) {
    Sequencer seq;
    seq.reset_queued();
    EXPECT_TRUE(seq.reset_pending());

    seq.on_clock_rise(10);
    EXPECT_FALSE(seq.reset_pending());
    EXPECT_EQ(seq.step_index(), 0u);
}

TEST(SequencerTest, ResetQueuedPrefillsFirstStepImmediately) {
    Sequencer seq;
    seq.set_length(1);
    seq.set_division(1);
    seq.set_probability(255);
    seq.set_step_mode(0, StepMode::On);

    seq.reset_queued();

    EXPECT_TRUE(seq.reset_pending());
    EXPECT_TRUE(seq.clock_route_high());
}

TEST(SequencerTest, ResetQueuedUsesCurrentStepOneState) {
    Sequencer seq;
    seq.set_length(2);
    seq.set_division(1);
    seq.set_probability(255);
    seq.set_step_mode(0, StepMode::Off);
    seq.set_step_mode(1, StepMode::On);

    seq.reset_queued();
    EXPECT_FALSE(seq.clock_route_high());

    seq.set_step_mode(0, StepMode::On);
    seq.set_step_mode(1, StepMode::Off);
    seq.reset_queued();
    EXPECT_TRUE(seq.clock_route_high());
}

TEST(SequencerTest, ResetQueuedKeepsPrefilledCycleForFollowingStep) {
    Sequencer seq;
    seq.set_length(2);
    seq.set_division(1);
    seq.set_probability(255);
    seq.set_step_mode(0, StepMode::On);
    seq.set_step_mode(1, StepMode::Off);

    seq.reset_queued();
    EXPECT_TRUE(seq.clock_route_high());

    seq.on_clock_rise(10);
    seq.on_clock_fall(20);
    EXPECT_FALSE(seq.clock_route_high());
}

TEST(SequencerTest, FullProbabilityKeepsDividedFillsOn) {
    Sequencer seq;
    seq.set_length(1);
    seq.set_division(4);
    seq.set_probability(255);
    seq.set_step_mode(0, StepMode::On);

    std::uint32_t t = 0;
    seq.on_clock_fall(t);
    EXPECT_TRUE(seq.clock_route_high());

    for (int i = 0; i < 12; ++i) {
        t += 10;
        seq.on_clock_rise(t);
        t += 1;
        seq.on_clock_fall(t);
        seq.tick(t + 5);
        EXPECT_TRUE(seq.clock_route_high());
    }
}

} // namespace
