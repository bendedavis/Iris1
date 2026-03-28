#pragma once

#include <cstdint>

namespace iris1 {

class ResetPolicy {
public:
    void reset();
    void on_clock_rise(std::uint32_t tick);
    void on_manual_reset_edge();
    bool should_queue_auto_reset(std::uint32_t now_tick);
    bool manual_mode() const;

private:
    enum class Mode : std::uint8_t {
        Auto = 0,
        Manual = 1,
    };

    Mode mode_ = Mode::Auto;
    std::uint32_t last_rise_tick_ = 0;
    std::uint32_t expected_period_ticks_ = 0;
    bool auto_reset_queued_since_last_rise_ = false;
};

} // namespace iris1
