#include "reset_policy.h"

namespace iris1 {

void ResetPolicy::reset() {
    mode_ = Mode::Auto;
    last_rise_tick_ = 0;
    expected_period_ticks_ = 0;
    auto_reset_queued_since_last_rise_ = false;
}

void ResetPolicy::on_clock_rise(std::uint32_t tick) {
    if (last_rise_tick_ != 0u && tick > last_rise_tick_) {
        expected_period_ticks_ = tick - last_rise_tick_;
    }
    last_rise_tick_ = tick;
    auto_reset_queued_since_last_rise_ = false;
}

void ResetPolicy::on_manual_reset_edge() {
    mode_ = Mode::Manual;
}

bool ResetPolicy::should_queue_auto_reset(std::uint32_t now_tick) {
    if (mode_ != Mode::Auto) {
        return false;
    }
    if (expected_period_ticks_ == 0u || last_rise_tick_ == 0u) {
        return false;
    }
    if (auto_reset_queued_since_last_rise_) {
        return false;
    }

    // Trigger only when clock gap is strictly more than 1.5 expected pulses.
    const std::uint32_t threshold = (expected_period_ticks_ * 3u) / 2u;
    if (now_tick - last_rise_tick_ > threshold) {
        auto_reset_queued_since_last_rise_ = true;
        return true;
    }
    return false;
}

bool ResetPolicy::manual_mode() const {
    return mode_ == Mode::Manual;
}

} // namespace iris1
