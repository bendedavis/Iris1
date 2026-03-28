#pragma once

namespace iris1 {

// Schedules when a loaded MIDI preset should be applied.
//
// Rule:
// - If transport is stopped, apply as soon as preset data is ready.
// - If transport is running, apply on the falling edge before step 1.
class MidiRecallTiming {
public:
    void reset();
    void request(bool transport_running);
    void on_clock_rise(bool wrapped_to_step1);
    bool on_clock_fall(bool preset_ready);
    bool apply_if_stopped(bool transport_running, bool preset_ready);
    bool pending() const;

private:
    bool wait_wrap_ = false;
    bool wait_fall_ = false;
};

} // namespace iris1
