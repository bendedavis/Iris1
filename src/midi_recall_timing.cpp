#include "midi_recall_timing.h"

namespace iris1 {

void MidiRecallTiming::reset() {
    wait_wrap_ = false;
    wait_fall_ = false;
}

void MidiRecallTiming::request(bool transport_running) {
    if (!transport_running) {
        reset();
        return;
    }
    wait_wrap_ = true;
    wait_fall_ = false;
}

void MidiRecallTiming::on_clock_rise(bool wrapped_to_step1) {
    if (wait_wrap_ && wrapped_to_step1) {
        wait_wrap_ = false;
        wait_fall_ = true;
    }
}

bool MidiRecallTiming::on_clock_fall(bool preset_ready) {
    if (!wait_fall_) {
        return false;
    }

    if (preset_ready) {
        wait_fall_ = false;
        return true;
    }

    // Missed this boundary because preset was not ready yet.
    // Wait for the next wrap and its following fall.
    wait_fall_ = false;
    wait_wrap_ = true;
    return false;
}

bool MidiRecallTiming::apply_if_stopped(bool transport_running, bool preset_ready) {
    if (!preset_ready || transport_running || (!wait_wrap_ && !wait_fall_)) {
        return false;
    }
    reset();
    return true;
}

bool MidiRecallTiming::pending() const {
    return wait_wrap_ || wait_fall_;
}

} // namespace iris1
