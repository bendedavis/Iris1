#include "sequencer.h"
#include "learning_prefs.h"

namespace iris1 {

namespace {
static uint32_t lfsr = 0xACE1u;
static constexpr uint8_t kStickyCooldownPulses = 4;
static inline uint8_t rand8() {
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
    return static_cast<uint8_t>(lfsr & 0xFF);
}

static inline uint8_t scale_prob(uint8_t base, uint8_t num, uint8_t den) {
    return static_cast<uint8_t>((static_cast<uint16_t>(base) * num) / den);
}

static inline uint8_t blend_role_toward_full(uint8_t role_prob, uint8_t control_prob) {
    // Keep the musical role weighting, but taper it out near the top
    // so ~90% knob doesn't suddenly feel sparse.
    if (control_prob <= 191u || role_prob >= control_prob) {
        return role_prob;
    }
    const uint16_t blend = static_cast<uint16_t>(control_prob - 191u); // 1..64
    const uint16_t delta = static_cast<uint16_t>(control_prob - role_prob);
    const uint16_t out = static_cast<uint16_t>(role_prob) + ((delta * blend) / 64u);
    return static_cast<uint8_t>((out > 255u) ? 255u : out);
}

static uint8_t apply_fill_probability_bias(
    uint8_t base_prob,
    uint8_t distance_from_anchor,
    uint32_t pulse_period_ticks,
    bool sparse_pattern
) {
    if (distance_from_anchor == 0u) {
        return base_prob;
    }

    uint32_t p = base_prob;

    // Musical spacing preference across the full cycle:
    // +1 often flams, +2 can crowd sparse rhythms, multiples of 3 are favored.
    if (distance_from_anchor == 1u) {
        p = (p * 1u) / 8u;
    } else if (distance_from_anchor == 2u) {
        p = (p * 1u) / 3u;
    } else if ((distance_from_anchor % 3u) == 0u) {
        p = (p * 7u) / 5u;
    } else {
        p = (p * 9u) / 10u;
    }

    // In sparse mode, further suppress near fills and favor farther fills.
    if (sparse_pattern) {
        if (distance_from_anchor == 1u) {
            p = (p * 1u) / 2u;
        } else if (distance_from_anchor == 2u) {
            p = (p * 2u) / 3u;
        } else if ((distance_from_anchor % 3u) == 0u) {
            p = (p * 6u) / 5u;
        }
    }

    // Tempo guardrails from incoming clock period (1 kHz ticks).
    // If very fast, avoid close fills that collapse into flams.
    if (pulse_period_ticks > 0u) {
        const uint32_t gap = static_cast<uint32_t>(distance_from_anchor) * pulse_period_ticks;
        if (gap < 45u) {
            p = 0u;
        } else if (gap < 70u) {
            p = (p * 1u) / 4u;
        } else if (gap < 95u && distance_from_anchor <= 2u) {
            p = (p * 1u) / 3u;
        }
    }

    if (p > 255u) {
        p = 255u;
    }
    return static_cast<uint8_t>(p);
}

static inline uint8_t clamp_u8(int32_t value) {
    if (value < 0) {
        return 0u;
    }
    if (value > 255) {
        return 255u;
    }
    return static_cast<uint8_t>(value);
}
}

Sequencer::Sequencer() {
    lfsr = 0xACE1u;
    reset_density_governor();
}

void Sequencer::reset_queued() {
    reset_queued_ = true;
    request_regen(true);
    precompute_from_reset();
}

void Sequencer::on_clock_fall(uint32_t tick) {
    last_fall_tick_ = tick;

    if (pending_regen_ || regen_on_next_fall_) {
        if (cycle_complete_pending_ && !last_cycle_bad_) {
            update_density_governor_from_cycle();
        }
        if (prefs_ && prev_cycle_valid_) {
            if (last_cycle_bad_) {
                apply_learning(false);
            } else if (cycle_complete_pending_) {
                apply_learning(true);
            }
        }
        regenerate_cycle();
        pending_regen_ = false;
        regen_on_next_fall_ = false;
        force_full_regen_ = false;
        cycle_complete_pending_ = false;
        last_cycle_bad_ = false;
    }

    decide_next_pulse();

    // Default low immediately after fall until midpoint.
    // If we don't yet have a valid half-period, fall back to immediate routing.
    if (half_period_ticks_ == 0) {
        clock_route_high_ = (next_step_state_ == StepState::On);
    } else {
        clock_route_high_ = false;
    }
}

bool Sequencer::on_clock_rise(uint32_t tick) {
    prev_rise_tick_ = last_rise_tick_;
    last_rise_tick_ = tick;

    // Compute half-period from rise-to-rise interval (assumes ~50% duty clock).
    if (prev_rise_tick_ > 0 && last_rise_tick_ > prev_rise_tick_) {
        const uint32_t period = last_rise_tick_ - prev_rise_tick_;
        half_period_ticks_ = period / 2;
    }

    bool advanced = false;

    // Apply queued reset on the next rise.
    if (reset_queued_) {
        step_ = 0;
        pulse_count_ = 0;
        last_pulse_index_ = 0;
        reset_queued_ = false;
    }

    const uint8_t cycle_pos = next_cycle_pos();

    // The pulse at next_cycle_pos() just occurred.
    const uint8_t pulse_index = static_cast<uint8_t>(cycle_pos % division_);
    last_pulse_index_ = pulse_index;
    if (sticky_cooldown_pulses_ > 0) {
        --sticky_cooldown_pulses_;
    }

    // Update step index from absolute pulse phase.
    step_ = static_cast<uint8_t>((cycle_pos / division_) % length_);
    current_step_state_ = next_step_state_;

    // Advance absolute pulse phase.
    ++pulse_count_;
    if (next_cycle_pos() == 0u) {
        regen_on_next_fall_ = true; // always regenerate each pass
        cycle_complete_pending_ = true;
        advanced = true;
    }

    return advanced;
}

void Sequencer::tick(uint32_t tick) {
    if (!clock_route_high_ && is_midpoint_reached(tick)) {
        clock_route_high_ = (next_step_state_ == StepState::On);
    }
}

bool Sequencer::clock_route_high() const {
    return clock_route_high_;
}

bool Sequencer::reset_pending() const {
    return reset_queued_;
}

uint8_t Sequencer::step_index() const {
    return step_;
}

uint8_t Sequencer::queued_step_index() const {
    if (division_ == 0u || length_ == 0u || cycle_len_ == 0u) {
        return 0u;
    }
    return static_cast<uint8_t>((next_cycle_pos() / division_) % length_);
}

void Sequencer::set_probability(uint8_t probability_0_255) {
    probability_ = probability_0_255;
    reset_density_governor();
    request_regen(true);
}

void Sequencer::set_length(uint8_t length_1_8) {
    if (length_1_8 < 1) length_1_8 = 1;
    if (length_1_8 > 8) length_1_8 = 8;
    length_ = length_1_8;
    reset_density_governor();
    sync_display_phase_from_last_pulse();
    request_regen(true);
}

void Sequencer::set_division(uint8_t division_1_4) {
    if (division_1_4 < 1) division_1_4 = 1;
    if (division_1_4 > 4) division_1_4 = 4;
    division_ = division_1_4;
    reset_density_governor();
    sync_display_phase_from_last_pulse();
    request_regen(true);
}

void Sequencer::set_step_mode(uint8_t step_index, StepMode mode) {
    if (step_index >= kMaxSteps) return;
    step_modes_[step_index] = mode;
    reset_density_governor();
    request_regen(true);
}

void Sequencer::set_learning_prefs(iris_common::LearningPrefs* prefs) {
    prefs_ = prefs;
}

uint8_t Sequencer::division() const {
    return division_;
}

uint8_t Sequencer::pulse_index() const {
    return last_pulse_index_;
}

void Sequencer::request_regen(bool full_reset) {
    pending_regen_ = true;
    if (full_reset) {
        force_full_regen_ = true;
        last_cycle_bad_ = true;
        sticky_cooldown_pulses_ = kStickyCooldownPulses;
    }
}

bool Sequencer::learning_dirty() const {
    return learning_dirty_;
}

void Sequencer::clear_learning_dirty() {
    learning_dirty_ = false;
}

void Sequencer::reset_density_governor() {
    global_opportunities_ = 0u;
    global_fires_ = 0u;
}

void Sequencer::update_density_governor_from_cycle() {
    for (uint8_t step = 0u; step < length_; ++step) {
        const StepMode mode = step_modes_[step];
        if (mode == StepMode::Off) {
            continue;
        }
        const uint8_t base = static_cast<uint8_t>(step * division_);
        for (uint8_t p = 0u; p < division_; ++p) {
            const bool eligible = (p == 0u) ? (mode == StepMode::Prob) : true;
            if (!eligible) {
                continue;
            }
            if (global_opportunities_ != 0xFFFFFFFFu) {
                ++global_opportunities_;
            }
            const uint8_t idx = static_cast<uint8_t>(base + p);
            if (cycle_[idx] != 0u) {
                if (global_fires_ != 0xFFFFFFFFu) {
                    ++global_fires_;
                }
            }
        }
    }
}

int32_t Sequencer::global_balance_scaled() const {
    const uint64_t desired = static_cast<uint64_t>(global_opportunities_) *
                             static_cast<uint64_t>(probability_);
    const uint64_t actual = static_cast<uint64_t>(global_fires_) * 255u;
    if (desired >= actual) {
        const uint64_t diff = desired - actual;
        return static_cast<int32_t>(diff > 0x7FFFFFFFu ? 0x7FFFFFFFu : diff);
    }
    const uint64_t diff = actual - desired;
    return -static_cast<int32_t>(diff > 0x7FFFFFFFu ? 0x7FFFFFFFu : diff);
}

uint8_t Sequencer::govern_probability(uint8_t pulse_index, uint8_t base_prob, bool primary_lane) const {
    (void)pulse_index;
    const int32_t balance = global_balance_scaled();
    int32_t out = base_prob;

    if (primary_lane) {
        if (balance > 0) {
            out += balance / 8;
        } else {
            out += balance / 12;
        }
    } else {
        if (balance > 0) {
            out += balance / 16;
        } else {
            out += balance / 20;
        }
    }

    return clamp_u8(out);
}

uint8_t Sequencer::govern_add_chance(uint8_t pulse_index, uint8_t base_chance, bool primary_lane) const {
    (void)pulse_index;
    const int32_t balance = global_balance_scaled();
    int32_t out = base_chance;

    if (primary_lane) {
        if (balance >= 255) {
            return 255u;
        }
        if (balance <= -255) {
            return 0u;
        }
        out += balance / 4;
    } else {
        if (balance >= 510) {
            return 255u;
        }
        if (balance <= -510) {
            return 0u;
        }
        out += balance / 8;
    }

    return clamp_u8(out);
}

uint8_t Sequencer::govern_remove_chance(uint8_t pulse_index, uint8_t base_chance, bool primary_lane) const {
    (void)pulse_index;
    const int32_t balance = global_balance_scaled();
    int32_t out = base_chance;

    if (primary_lane) {
        if (balance >= 255) {
            return 0u;
        }
        if (balance <= -255) {
            return 255u;
        }
        out -= balance / 4;
    } else {
        if (balance >= 510) {
            return 0u;
        }
        if (balance <= -510) {
            return 255u;
        }
        out -= balance / 8;
    }

    return clamp_u8(out);
}

void Sequencer::precompute_from_reset() {
    step_ = 0;
    last_pulse_index_ = 0;

    if (pending_regen_ || regen_on_next_fall_) {
        regenerate_cycle();
        pending_regen_ = false;
        regen_on_next_fall_ = false;
        force_full_regen_ = false;
        cycle_complete_pending_ = false;
        last_cycle_bad_ = false;
    }

    decide_next_pulse();
    clock_route_high_ = (next_step_state_ == StepState::On);
}

void Sequencer::regenerate_cycle() {
    cycle_len_ = static_cast<uint8_t>(length_ * division_);
    if (cycle_len_ == 0) cycle_len_ = 1;

    const uint32_t pulse_period_ticks =
        (last_rise_tick_ > prev_rise_tick_) ? (last_rise_tick_ - prev_rise_tick_) : 0u;

    const bool full_prob_global = (probability_ == 255u);
    const bool sticky = prev_cycle_valid_ && !force_full_regen_ && !full_prob_global && (sticky_cooldown_pulses_ == 0);
    const uint8_t sticky_prob = 178; // ~70% reuse
    const bool sparse_pattern = (probability_ < 224u);
    const bool dense_mode = (probability_ >= 224u);

    uint8_t candidate[kMaxCycle] = {0};
    uint8_t fill_prob[kMaxCycle] = {0};
    uint8_t fill_allowed[kMaxCycle] = {0};
    uint8_t anchor[kMaxCycle] = {0};
    uint8_t prob_anchor[kMaxCycle] = {0};
    uint8_t density_eligible[kMaxCycle] = {0};
    uint8_t density_mandatory[kMaxCycle] = {0};
    uint8_t sticky_reuse_allowed[kMaxCycle] = {0};

    for (uint8_t step = 0; step < length_; ++step) {
        const StepMode mode = step_modes_[step];

        // Role-based extra probability scaling (unless full probability).
        const uint8_t prob_use = probability_;
        uint8_t extra_prob = prob_use;
        const bool full_prob = (prob_use == 255u);
        if (!full_prob) {
            if (step == 0 || step == 4) {
                extra_prob = scale_prob(prob_use, 1, 4); // kick-like
            } else if (step == 2 || step == 6) {
                extra_prob = scale_prob(prob_use, 1, 2); // snare-like
            } else {
                extra_prob = scale_prob(prob_use, 4, 5); // hat-like (0.8)
            }
            extra_prob = blend_role_toward_full(extra_prob, prob_use);
        }

        const uint8_t base = static_cast<uint8_t>(step * division_);
        for (uint8_t p = 0; p < division_; ++p) {
            const uint8_t idx = static_cast<uint8_t>(base + p);
            const uint8_t idx32 = static_cast<uint8_t>((static_cast<uint16_t>(idx) * 32u) / cycle_len_);
            int16_t bias = 0;
            if (prefs_) {
                bias = static_cast<int16_t>(prefs_->pulse_bias[idx32]) - 128;
            }
            uint8_t on = 0;
            uint8_t prob_adj = prob_use;
            uint8_t extra_adj = extra_prob;
            if (bias != 0) {
                // Keep probability response closer to linear by applying
                // milder learning influence on primary and fill pulses.
                const int16_t p1 = static_cast<int16_t>(prob_use) + (bias / 4);
                const int16_t p2 = static_cast<int16_t>(extra_prob) + (bias / 3);
                prob_adj = static_cast<uint8_t>((p1 < 0) ? 0 : (p1 > 255 ? 255 : p1));
                extra_adj = static_cast<uint8_t>((p2 < 0) ? 0 : (p2 > 255 ? 255 : p2));
            }
            if (full_prob) {
                prob_adj = 255;
                extra_adj = 255;
            }

            if (mode == StepMode::Off) {
                candidate[idx] = 0;
                continue;
            }

            if (p == 0u) {
                if (mode == StepMode::On) {
                    on = 1u;
                    density_mandatory[idx] = 1u;
                } else {
                    density_eligible[idx] = 1u;
                    const uint8_t governed = govern_probability(idx, prob_adj, true);
                    on = (rand8() <= governed) ? 1u : 0u;
                }
                candidate[idx] = on;
                sticky_reuse_allowed[idx] = 0u;
                // Always use 100% steps as the primary rhythmic anchors.
                if (mode == StepMode::On) {
                    anchor[idx] = 1u;
                } else if (on != 0u) {
                    // Probabilistic anchors are used only as fallback when no
                    // explicit 100% anchors exist in the current pattern.
                    prob_anchor[idx] = 1u;
                }
            } else {
                density_eligible[idx] = 1u;
                sticky_reuse_allowed[idx] = 1u;
                if (full_prob) {
                    candidate[idx] = 1u;
                } else {
                    fill_prob[idx] = extra_adj;
                    fill_allowed[idx] = 1u;
                }
            }
        }
    }

    // If no intended pulse is active, fall back to cycle start as reference.
    bool have_anchor = false;
    for (uint8_t i = 0u; i < cycle_len_; ++i) {
        if (anchor[i] != 0u) {
            have_anchor = true;
            break;
        }
    }
    if (!have_anchor) {
        for (uint8_t i = 0u; i < cycle_len_; ++i) {
            if (prob_anchor[i] != 0u) {
                anchor[i] = 1u;
                have_anchor = true;
            }
        }
    }
    if (!have_anchor && cycle_len_ > 0u) {
        anchor[0] = 1u;
    }

    uint8_t dist_from_anchor[kMaxCycle] = {0};
    uint8_t preferred_slot[kMaxCycle] = {0};

    for (uint8_t idx = 0u; idx < cycle_len_; ++idx) {
        if (fill_allowed[idx] == 0u) {
            continue;
        }

        uint8_t best = cycle_len_;
        for (uint8_t a = 0u; a < cycle_len_; ++a) {
            if (anchor[a] == 0u) {
                continue;
            }
            const uint8_t d = static_cast<uint8_t>((idx + cycle_len_ - a) % cycle_len_);
            if (d == 0u) {
                continue;
            }
            if (d < best) {
                best = d;
            }
        }
        if (best == cycle_len_) {
            best = 1u;
        }

        dist_from_anchor[idx] = best;
        preferred_slot[idx] = ((best % 3u) == 0u) ? 1u : 0u;
    }

    // Fill preferred 3-step slots first, then other slots.
    for (uint8_t pass = 0u; pass < 2u; ++pass) {
        for (uint8_t idx = 0u; idx < cycle_len_; ++idx) {
            if (fill_allowed[idx] == 0u) {
                continue;
            }
            const bool want_preferred = (pass == 0u);
            if ((preferred_slot[idx] != 0u) != want_preferred) {
                continue;
            }

            uint32_t p = apply_fill_probability_bias(
                fill_prob[idx], dist_from_anchor[idx], pulse_period_ticks, sparse_pattern);

            // Keep the full pattern coherent: suppress tight clusters.
            const uint8_t prev = (idx == 0u) ? static_cast<uint8_t>(cycle_len_ - 1u)
                                             : static_cast<uint8_t>(idx - 1u);
            const uint8_t prev2 = (idx <= 1u) ? static_cast<uint8_t>(idx + cycle_len_ - 2u)
                                              : static_cast<uint8_t>(idx - 2u);
            const uint8_t next_anchor = static_cast<uint8_t>((idx + 1u) % cycle_len_);
            if (candidate[prev] != 0u) {
                p = dense_mode ? ((p * 2u) / 3u) : ((p * 1u) / 5u);
            } else if (candidate[prev2] != 0u) {
                p = dense_mode ? ((p * 5u) / 6u) : ((p * 2u) / 3u);
            }
            if (anchor[next_anchor] != 0u) {
                p = dense_mode ? ((p * 5u) / 6u) : ((p * 2u) / 3u);
            }

            if (p > 255u) {
                p = 255u;
            }
            p = govern_probability(idx, static_cast<uint8_t>(p), false);
            candidate[idx] = (rand8() <= p) ? 1u : 0u;
        }
    }

    // Tight-fill cohesion pass:
    // If we generate a close fill (+1/+2 from anchor), avoid isolated hits.
    // Either convert it into a short grouped roll near the end of the step
    // or suppress it if the pattern/probability doesn't support a roll.
    if (!full_prob_global && division_ >= 2u) {
        for (uint8_t step = 0u; step < length_; ++step) {
            const uint8_t base = static_cast<uint8_t>(step * division_);

            for (uint8_t p = 1u; p < division_; ++p) {
                const uint8_t idx = static_cast<uint8_t>(base + p);
                if (fill_allowed[idx] == 0u || candidate[idx] == 0u) {
                    continue;
                }
                if (dist_from_anchor[idx] > 2u) {
                    continue;
                }

                // Grouping considers fill neighbors inside the same step only.
                const bool prev_fill_on = (p > 1u) && (candidate[idx - 1u] != 0u);
                const bool next_fill_on =
                    ((p + 1u) < division_) && (candidate[static_cast<uint8_t>(idx + 1u)] != 0u);
                if (prev_fill_on || next_fill_on) {
                    continue;
                }

                const bool allow_roll = (division_ >= 3u) && (probability_ >= 192u);
                if (!allow_roll) {
                    // No roll context: remove isolated close hits to avoid flams.
                    candidate[idx] = 0u;
                    break;
                }

                // Default roll window is the tail of the step.
                uint8_t roll_start_p = (division_ >= 4u) ? 2u : 1u;
                // At slower clocks and very high probability, allow a longer roll.
                if ((division_ == 4u) && (probability_ >= 224u) && (pulse_period_ticks >= 120u)) {
                    roll_start_p = 1u;
                }

                uint8_t roll_hits = 0u;
                for (uint8_t rp = roll_start_p; rp < division_; ++rp) {
                    const uint8_t ridx = static_cast<uint8_t>(base + rp);
                    if (fill_allowed[ridx] == 0u) {
                        continue;
                    }

                    uint32_t roll_p = apply_fill_probability_bias(
                        fill_prob[ridx], dist_from_anchor[ridx], pulse_period_ticks, false);
                    roll_p = (roll_p * 6u) / 5u; // slight boost for contiguous roll feel
                    if (roll_p > 255u) {
                        roll_p = 255u;
                    }
                    roll_p = govern_probability(ridx, static_cast<uint8_t>(roll_p), false);

                    if (ridx == idx || rand8() <= roll_p) {
                        candidate[ridx] = 1u;
                        ++roll_hits;
                    } else {
                        candidate[ridx] = 0u;
                    }
                }

                // Ensure roll is not orphaned: guarantee at least 2 tail hits when possible.
                if (roll_hits < 2u && division_ >= 3u) {
                    const uint8_t last = static_cast<uint8_t>(base + division_ - 1u);
                    const uint8_t penult = static_cast<uint8_t>(base + division_ - 2u);
                    if (fill_allowed[last] != 0u && candidate[last] == 0u) {
                        candidate[last] = 1u;
                        ++roll_hits;
                    }
                    if (roll_hits < 2u && fill_allowed[penult] != 0u && candidate[penult] == 0u) {
                        candidate[penult] = 1u;
                        ++roll_hits;
                    }
                }

                // Clear early fill slots so the roll starts in a coherent spot.
                for (uint8_t rp = 1u; rp < roll_start_p; ++rp) {
                    const uint8_t ridx = static_cast<uint8_t>(base + rp);
                    if (fill_allowed[ridx] != 0u) {
                        candidate[ridx] = 0u;
                    }
                }

                break;
            }
        }
    }

    // Global density lock:
    // knob percent ~= percent of variable pulses ON.
    // Fixed 100% anchors stay on, but they do not consume the probability budget.
    if (!full_prob_global) {
        uint8_t eligible_count = 0u;
        uint8_t current_on = 0u;
        for (uint8_t i = 0u; i < cycle_len_; ++i) {
            if (density_eligible[i] == 0u) {
                continue;
            }
            ++eligible_count;
            if (candidate[i] != 0u) {
                ++current_on;
            }
        }

        if (eligible_count > 0u) {
            // Generate one cycle at a time, but randomize the rounding so small
            // eligible pools (like a single Prob step in divide 1) still obey
            // the requested long-term percentage.
            const uint16_t scaled_target = static_cast<uint16_t>(probability_) * eligible_count;
            uint8_t target_on = static_cast<uint8_t>(scaled_target / 255u);
            const uint8_t fractional = static_cast<uint8_t>(scaled_target % 255u);
            if ((target_on < eligible_count) && (fractional > 0u) && (rand8() < fractional)) {
                ++target_on;
            }

            // Global running count since the last pattern change:
            // if the pattern has fired less than it should have over time,
            // add extra pulses this cycle; if it has over-fired, hold back.
            const int32_t balance = global_balance_scaled();
            if (balance > 0) {
                uint8_t behind_hits = static_cast<uint8_t>(
                    (static_cast<uint32_t>(balance) / 255u) > 255u ? 255u
                                                                   : (static_cast<uint32_t>(balance) / 255u));
                if (probability_ <= 96u && balance >= 128) {
                    if (behind_hits == 0u) {
                        behind_hits = 1u;
                    }
                }
                const uint8_t room = static_cast<uint8_t>(eligible_count - target_on);
                target_on = static_cast<uint8_t>(target_on + ((behind_hits < room) ? behind_hits : room));
            } else if (balance < 0) {
                const uint32_t ahead_scaled = static_cast<uint32_t>(-balance);
                const uint8_t ahead_hits = static_cast<uint8_t>(
                    (ahead_scaled / 255u) > 255u ? 255u : (ahead_scaled / 255u));
                if (ahead_hits >= target_on) {
                    target_on = 0u;
                } else {
                    target_on = static_cast<uint8_t>(target_on - ahead_hits);
                }
            }

            // Add pulses until target is met (prefer musical slots first, still random).
            uint16_t guard = static_cast<uint16_t>(cycle_len_) * 32u;
            while ((current_on < target_on) && (guard-- > 0u)) {
                const uint8_t idx = static_cast<uint8_t>(rand8() % cycle_len_);
                if (density_eligible[idx] == 0u || density_mandatory[idx] != 0u || candidate[idx] != 0u) {
                    continue;
                }

                uint8_t chance = 180u;
                if (preferred_slot[idx] != 0u) {
                    chance = 255u;
                } else if (dist_from_anchor[idx] == 1u) {
                    chance = dense_mode ? 170u : 80u;
                } else if (dist_from_anchor[idx] == 2u) {
                    chance = dense_mode ? 210u : 140u;
                }
                chance = govern_add_chance(idx, chance, (idx % division_) == 0u);

                if (rand8() <= chance) {
                    candidate[idx] = 1u;
                    ++current_on;
                }
            }

            // Fallback add to guarantee exact target if probabilistic pass ran out.
            if (current_on < target_on) {
                const uint8_t start = static_cast<uint8_t>(rand8() % cycle_len_);
                for (uint8_t n = 0u; n < cycle_len_ && current_on < target_on; ++n) {
                    const uint8_t idx = static_cast<uint8_t>((start + n) % cycle_len_);
                    if (density_eligible[idx] != 0u && density_mandatory[idx] == 0u && candidate[idx] == 0u) {
                        const uint8_t chance = govern_add_chance(idx, 255u, (idx % division_) == 0u);
                        if (rand8() > chance) {
                            continue;
                        }
                        candidate[idx] = 1u;
                        ++current_on;
                    }
                }
            }

            // Remove pulses if over target (remove least musical first).
            guard = static_cast<uint16_t>(cycle_len_) * 32u;
            while ((current_on > target_on) && (guard-- > 0u)) {
                const uint8_t idx = static_cast<uint8_t>(rand8() % cycle_len_);
                if (density_eligible[idx] == 0u || density_mandatory[idx] != 0u || candidate[idx] == 0u) {
                    continue;
                }

                uint8_t chance = 150u;
                if (preferred_slot[idx] != 0u) {
                    chance = 40u;
                } else if (dist_from_anchor[idx] == 1u) {
                    chance = 255u;
                } else if (dist_from_anchor[idx] == 2u) {
                    chance = 210u;
                }
                chance = govern_remove_chance(idx, chance, (idx % division_) == 0u);

                if (rand8() <= chance) {
                    candidate[idx] = 0u;
                    --current_on;
                }
            }

            // Fallback remove to guarantee exact target.
            if (current_on > target_on) {
                const uint8_t start = static_cast<uint8_t>(rand8() % cycle_len_);
                for (uint8_t n = 0u; n < cycle_len_ && current_on > target_on; ++n) {
                    const uint8_t idx = static_cast<uint8_t>((start + n) % cycle_len_);
                    if (density_eligible[idx] != 0u && density_mandatory[idx] == 0u && candidate[idx] != 0u) {
                        const uint8_t chance = govern_remove_chance(idx, 255u, (idx % division_) == 0u);
                        if (rand8() > chance) {
                            continue;
                        }
                        candidate[idx] = 0u;
                        --current_on;
                    }
                }
            }
        }
    }

    // Apply sticky behavior after context-aware fill generation.
    for (uint8_t i = 0u; i < cycle_len_; ++i) {
        if (sticky && (sticky_reuse_allowed[i] != 0u) && (rand8() < sticky_prob)) {
            cycle_[i] = prev_cycle_[i];
        } else {
            cycle_[i] = candidate[i];
        }
    }

    // Save for sticky reuse on next cycle.
    for (uint8_t i = 0; i < cycle_len_; ++i) {
        prev_cycle_[i] = cycle_[i];
    }
    prev_cycle_valid_ = true;

    // Learning update handled on next falling edge (apply_learning).
}

uint8_t Sequencer::next_cycle_pos() const {
    if (cycle_len_ == 0u) {
        return 0u;
    }
    if (reset_queued_) {
        return 0u;
    }
    return static_cast<uint8_t>(pulse_count_ % cycle_len_);
}

void Sequencer::sync_display_phase_from_last_pulse() {
    const uint8_t effective_cycle_len = static_cast<uint8_t>(length_ * division_);
    if (division_ == 0u || length_ == 0u || effective_cycle_len == 0u || pulse_count_ == 0u) {
        step_ = 0u;
        last_pulse_index_ = 0u;
        return;
    }

    const uint8_t last_cycle_pos = static_cast<uint8_t>((pulse_count_ - 1u) % effective_cycle_len);
    last_pulse_index_ = static_cast<uint8_t>(last_cycle_pos % division_);
    step_ = static_cast<uint8_t>((last_cycle_pos / division_) % length_);
}

void Sequencer::apply_learning(bool positive) {
    if (!prefs_ || !prev_cycle_valid_) return;
    for (uint8_t i = 0; i < cycle_len_; ++i) {
        if (!prev_cycle_[i]) continue;
        const uint8_t idx32 = static_cast<uint8_t>((static_cast<uint16_t>(i) * 32u) / cycle_len_);
        uint8_t& b = prefs_->pulse_bias[idx32];
        if (positive) {
            if (b < 255) { ++b; learning_dirty_ = true; }
        } else {
            if (b > 0) { --b; learning_dirty_ = true; }
        }
    }
    if (learning_dirty_) {
        prefs_->update_checksum();
    }
}

void Sequencer::decide_next_pulse() {
    const uint8_t idx = next_cycle_pos();
    next_step_state_ = (cycle_[idx] != 0) ? StepState::On : StepState::Off;
}

bool Sequencer::is_midpoint_reached(uint32_t tick) const {
    if (half_period_ticks_ == 0) {
        return false;
    }
    return (tick - last_fall_tick_) >= half_period_ticks_;
}

} // namespace iris1
