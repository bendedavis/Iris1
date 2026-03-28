#pragma once

#include <cstdint>

namespace iris_common { struct LearningPrefs; }

namespace iris1 {

enum class StepState : uint8_t {
    Off = 0,
    On = 1,
};

enum class StepMode : uint8_t {
    Off = 0,
    Prob = 1,
    On = 2,
};

// Handles step decisions and timing for PA11 pre‑route.
class Sequencer {
public:
    Sequencer();

    // Call on boot or when a reset is requested.
    void reset_queued();

    // Call on clock falling edge.
    void on_clock_fall(uint32_t tick);

    // Call on clock rising edge. Returns true if step advanced.
    bool on_clock_rise(uint32_t tick);

    // Call periodically to update mid‑period actions.
    void tick(uint32_t tick);

    // Returns true if PA11 should be high (clock routed to output).
    bool clock_route_high() const;

    // True when a reset is queued and will be applied on the next clock rise.
    bool reset_pending() const;

    // Returns current step (0‑based).
    uint8_t step_index() const;
    uint8_t queued_step_index() const;

    // Probability control (0..255).
    void set_probability(uint8_t probability_0_255);

    void set_length(uint8_t length_1_8);
    void set_division(uint8_t division_1_4);

    void set_step_mode(uint8_t step_index, StepMode mode);
    void set_learning_prefs(iris_common::LearningPrefs* prefs);

    uint8_t division() const;
    uint8_t pulse_index() const; // 0‑based within step

    // Mark that a new cycle should be generated on the next falling edge.
    void request_regen(bool full_reset);
    bool learning_dirty() const;
    void clear_learning_dirty();

private:
    void precompute_from_reset();
    void regenerate_cycle();
    void apply_learning(bool positive);
    void reset_density_governor();
    void update_density_governor_from_cycle();
    int32_t global_balance_scaled() const;
    uint8_t govern_probability(uint8_t pulse_index, uint8_t base_prob, bool primary_lane) const;
    uint8_t govern_add_chance(uint8_t pulse_index, uint8_t base_chance, bool primary_lane) const;
    uint8_t govern_remove_chance(uint8_t pulse_index, uint8_t base_chance, bool primary_lane) const;
    void decide_next_pulse();
    bool is_midpoint_reached(uint32_t tick) const;
    uint8_t next_cycle_pos() const;
    void sync_display_phase_from_last_pulse();

    static constexpr uint8_t kMaxSteps = 8;
    static constexpr uint8_t kMaxDivision = 4;
    static constexpr uint8_t kMaxCycle = kMaxSteps * kMaxDivision;

    uint8_t step_ = 0;
    StepState current_step_state_ = StepState::Off;
    StepState next_step_state_ = StepState::Off;

    bool reset_queued_ = false;
    bool clock_route_high_ = false;

    // Timing
    uint32_t last_fall_tick_ = 0;
    uint32_t last_rise_tick_ = 0;
    uint32_t prev_rise_tick_ = 0;
    uint32_t half_period_ticks_ = 0;

    uint8_t probability_ = 128; // 0..255
    uint8_t length_ = 8;
    uint8_t division_ = 1;

    StepMode step_modes_[kMaxSteps] = {
        StepMode::Prob, StepMode::Prob, StepMode::Prob, StepMode::Prob,
        StepMode::Prob, StepMode::Prob, StepMode::Prob, StepMode::Prob
    };

    // Cycle buffer
    uint8_t cycle_[kMaxCycle] = {0};
    uint8_t prev_cycle_[kMaxCycle] = {0};
    uint8_t cycle_len_ = 8;
    uint32_t pulse_count_ = 0; // absolute next-pulse count; resets only on reset
    uint8_t last_pulse_index_ = 0;
    uint32_t global_opportunities_ = 0u;
    uint32_t global_fires_ = 0u;

    bool pending_regen_ = true;
    bool regen_on_next_fall_ = true;
    bool force_full_regen_ = false;
    uint8_t sticky_cooldown_pulses_ = 0;
    bool prev_cycle_valid_ = false;
    bool learning_dirty_ = false;
    bool cycle_complete_pending_ = false;
    bool last_cycle_bad_ = false;
    iris_common::LearningPrefs* prefs_ = nullptr;
};

} // namespace iris1
