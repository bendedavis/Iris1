#include "app.h"
#include "sequencer.h"
#include "pins.h"
#include "preset_store.h"
#include "hal.h"
#include "led_driver.h"
#include "led_map.h"
#include "switch_map.h"
#include "learning_store.h"
#include "midi_preset_controller.h"
#include "midi_recall_timing.h"
#include "reset_policy.h"
#include "stm32f0_regs.h"

#include <cstdint>

namespace iris1::app {

namespace {
constexpr std::uint32_t kAutosaveIntervalTicks = 2000; // 2s @ 1 kHz
constexpr std::uint32_t kLearningSaveIntervalTicks = 10000; // 10s @ 1 kHz
constexpr bool kEnableLearningStore = false; // slot-0 stabilization mode
constexpr std::uint16_t kAutosaveAddr = 0x0000;
constexpr std::uint16_t kPresetSize = 32;
constexpr std::uint16_t kPresetCount = 16;
constexpr std::uint16_t kLearningAddr = static_cast<std::uint16_t>(kAutosaveAddr + kPresetSize * (kPresetCount + 1)); // autosave + 16 presets
constexpr std::uint16_t kPresetRegionBytes = static_cast<std::uint16_t>(kPresetSize * (kPresetCount + 1));
constexpr std::uint16_t preset_slot_addr(std::uint16_t slot_0_to_16) {
    return static_cast<std::uint16_t>(kAutosaveAddr + kPresetSize * slot_0_to_16);
}

static_assert(kAutosaveAddr == 0x0000, "Autosave must be slot 0 at EEPROM base");
static_assert(preset_slot_addr(0) == 0x0000, "Slot 0 address mismatch");
static_assert(preset_slot_addr(1) == 0x0020, "Slot 1 address mismatch");
static_assert(preset_slot_addr(16) == 0x0200, "Slot 16 address mismatch");
static_assert(kLearningAddr == 0x0220, "Learning area must start after slot 16");
static_assert(kLearningAddr == kPresetRegionBytes, "Preset region size/address mismatch");
static_assert(kLearningAddr + sizeof(iris_common::LearningPrefs) <= iris_common::EepromM95080::kSizeBytes,
              "EEPROM map exceeds M95080 capacity");
constexpr std::uint32_t kLengthModeHoldTicks = 1000; // 1s
constexpr std::uint32_t kFullResetHoldTicks = 6000; // 6s
constexpr std::uint32_t kTransportStopTicks = 1500; // no clock for 1.5s => stopped

static Sequencer g_sequencer;
static iris_common::PresetStore g_preset_store(kAutosaveAddr);
static iris_common::PresetStore g_midi_store(kAutosaveAddr);
static iris_common::LearningStore g_learning_store(kLearningAddr);
static iris_common::Preset g_current_preset;
static iris_common::Preset g_last_autosave;
static iris_common::Preset g_pending_autosave;
static bool g_autosave_pending = false;
static bool g_force_autosave = false;
static bool g_preset_save_queued = false;
static iris_common::Preset g_queued_preset;
static bool g_learning_dirty = false;
static bool g_learning_save_queued = false;
static iris_common::LearningPrefs g_queued_learning;
static std::uint32_t g_last_learning_save_tick = 0;
static bool g_learning_load_started = false;
static bool g_learning_loaded = false;
static volatile uint32_t g_tick = 0;
static std::uint32_t g_last_autosave_tick = 0;
static bool g_loaded = false;
// ADC polling disabled for isolation.

enum class UiMode : std::uint8_t { Normal, LengthSet };
static UiMode g_mode = UiMode::Normal;

static bool g_mute_btn_prev = false;
static std::uint32_t g_mute_hold_start = 0;
static bool g_longpress_active = false;

static std::uint8_t g_division_page = 1;

// Mute LED fade
static std::uint16_t g_fade_phase = 0; // computed from g_tick

// Step LED fade state
static std::uint32_t g_prev_rise_tick = 0;
static std::uint32_t g_last_rise_tick = 0;
static std::uint32_t g_last_period_ticks = 0;
static bool g_prob_update_armed = true;
static std::uint16_t g_prob_hold_adc = 0;
static std::uint8_t g_prob_slot = 0; // quantized 0..31, stable against ADC jitter
static bool g_length_update_armed = true;
static std::uint16_t g_length_hold_adc = 0;
static std::uint8_t g_length_slot = 0;
static std::uint32_t g_length_last_move_tick = 0;
static std::uint16_t g_length_adc_filt = 0;
static std::uint8_t g_length_candidate_slot = 0xFFu;
static std::uint8_t g_length_candidate_count = 0u;
static volatile bool g_clk_rise_flag = false;
static volatile bool g_clk_fall_flag = false;
static volatile bool g_reset_rise_flag = false;
static volatile std::uint32_t g_midi_rx_irq_count = 0;
static volatile std::uint32_t g_midi_rx_byte_count = 0;
static volatile std::uint32_t g_midi_rx_error_count = 0;
static volatile std::uint32_t g_midi_rx_error_flags = 0;
static volatile std::uint8_t g_midi_last_byte = 0;
static volatile std::uint32_t g_midi_action_count = 0;
static volatile std::uint32_t g_midi_recall_action_count = 0;
static volatile std::uint32_t g_midi_save_action_count = 0;
static volatile std::uint8_t g_midi_last_action_type = 0;
static volatile std::uint8_t g_midi_last_action_slot = 0xFFu;
static bool g_reset_raw_prev = false;
static bool g_reset_filtered_high = false;
static std::uint8_t g_reset_high_samples = 0;
static volatile std::uint32_t g_reset_raw_rise_count = 0;
static volatile std::uint32_t g_reset_filtered_rise_count = 0;
static bool g_autosave_watch_init = false;
static std::uint8_t g_watch_mute = 0;
static std::uint8_t g_watch_prob = 0;
static std::uint8_t g_watch_length = 8;
static std::uint8_t g_watch_division = 1;
static std::uint32_t g_last_control_change_tick = 0;
// (ADC debug mode removed; length mode uses 1s hold)

static constexpr std::uint16_t kProbKnobHysteresis = 16; // ADC counts
static constexpr std::uint16_t kProbResumeHysteresis = 48; // require a deliberate move after leaving select mode
static constexpr std::uint16_t kLengthKnobHysteresis = 64; // ADC counts
static constexpr std::uint32_t kLengthModeExitTicks = 4000; // 4s inactivity
static constexpr std::uint8_t kLengthSlotStableTicks = 12; // ~12 ms stable before latching a new slot
static constexpr std::uint16_t kMuteFadePeriodTicks = 2000; // 1s up, 1s down at 1kHz
static constexpr std::uint16_t kMuteFadeHalfTicks = 1000;
static constexpr std::uint8_t kResetFilterSamples = 8; // 1.0 ms at 8 kHz

static StepMode g_pending_modes[8] = {
    StepMode::Prob, StepMode::Prob, StepMode::Prob, StepMode::Prob,
    StepMode::Prob, StepMode::Prob, StepMode::Prob, StepMode::Prob
};
static bool g_pending_modes_valid = false;
static MidiPresetController g_midi_controller;
static ResetPolicy g_reset_policy;

enum class MidiStoreOp : std::uint8_t { None, RecallLoad, SaveSlot };
static MidiStoreOp g_midi_store_op = MidiStoreOp::None;
static bool g_midi_recall_queued = false;
static std::uint8_t g_midi_recall_slot = 0;
static bool g_midi_save_queued = false;
static std::uint8_t g_midi_save_slot = 0;
static bool g_midi_recall_ready = false;
static iris_common::Preset g_midi_loaded_preset;
static MidiRecallTiming g_midi_recall_timing;

static bool autosave_fields_differ(const iris_common::Preset& a, const iris_common::Preset& b) {
    return a.mute != b.mute ||
           a.probability != b.probability ||
           a.sequence_length != b.sequence_length ||
           a.division != b.division;
}

static void refresh_autosave_watch_snapshot() {
    g_watch_mute = g_current_preset.mute;
    g_watch_prob = g_current_preset.probability;
    g_watch_length = g_current_preset.sequence_length;
    g_watch_division = g_current_preset.division;
    g_autosave_watch_init = true;
}

static void track_control_activity() {
    if (!g_autosave_watch_init) {
        refresh_autosave_watch_snapshot();
        g_last_control_change_tick = g_tick;
        return;
    }
    if (g_watch_mute != g_current_preset.mute ||
        g_watch_prob != g_current_preset.probability ||
        g_watch_length != g_current_preset.sequence_length ||
        g_watch_division != g_current_preset.division) {
        refresh_autosave_watch_snapshot();
        g_last_control_change_tick = g_tick;
    }
}

static bool request_preset_save(const iris_common::Preset& preset) {
    // Prioritize slot-0 autosave and avoid sharing the SPI EEPROM bus concurrently.
    if (g_preset_store.busy() || g_autosave_pending || g_learning_store.busy() ||
        g_midi_store.busy() || g_midi_store_op != MidiStoreOp::None ||
        g_midi_recall_queued || g_midi_save_queued) {
        g_queued_preset = preset;
        g_preset_save_queued = true;
        return false;
    }
    if (!g_preset_store.begin_save(preset)) {
        g_queued_preset = preset;
        g_preset_save_queued = true;
        return false;
    }
    g_pending_autosave = preset;
    g_autosave_pending = true;
    g_preset_save_queued = false;
    return true;
}

static void queue_learning_save(const iris_common::LearningPrefs& prefs) {
    g_queued_learning = prefs;
    g_learning_save_queued = true;
}

static void service_preset_save_completion() {
    if (!g_autosave_pending || g_preset_store.busy()) {
        return;
    }
    if (!g_preset_store.done()) {
        return;
    }
    if (g_preset_store.verify_ok()) {
        g_last_autosave = g_pending_autosave;
        g_force_autosave = false;
    } else {
        // Keep trying on the periodic autosave cadence.
        g_force_autosave = true;
    }
    g_autosave_pending = false;
}

static void service_eeprom_write_queue() {
    if (g_preset_store.busy() || g_learning_store.busy() || g_autosave_pending ||
        g_midi_store.busy() || g_midi_store_op != MidiStoreOp::None ||
        g_midi_recall_queued || g_midi_save_queued) {
        return;
    }

    // Slot 0 autosave has priority.
    if (g_preset_save_queued) {
        if (g_preset_store.begin_save(g_queued_preset)) {
            g_pending_autosave = g_queued_preset;
            g_autosave_pending = true;
            g_preset_save_queued = false;
            g_last_autosave_tick = g_tick;
        }
        return;
    }

    // Learning writes are lower priority and only run after learning store is loaded.
    if (g_learning_save_queued && g_learning_loaded) {
        if (g_learning_store.begin_save(g_queued_learning)) {
            g_learning_save_queued = false;
        }
    }
}

static std::uint16_t knob_adc_raw() {
    const std::uint32_t raw = iris_common::hal::adc_latest();
    std::uint32_t scaled = (raw * 33u) / 30u;
    if (scaled > 4095u) scaled = 4095u;
    return static_cast<std::uint16_t>(scaled);
}

static std::uint8_t knob_probability_slot() {
    const std::uint16_t adc = knob_adc_raw();
    std::uint32_t slot_u32 = (static_cast<std::uint32_t>(adc) * 32u) / 4096u;
    if (slot_u32 > 31u) slot_u32 = 31u;
    return static_cast<std::uint8_t>(slot_u32);
}

static std::uint8_t slot_to_probability_255(std::uint8_t slot_0_31) {
    // Two-piece response:
    // - 0..25%: sparse expo for fine low-density dialing
    // - 25..50%: more natural climb into the midpoint
    // - 50..100%: gentler expo so midpoint is obvious but full scale arrives later
    static constexpr std::uint8_t kProbabilityCurve[32] = {
        0u,   2u,   5u,   10u,  16u,  23u,  31u,  40u,
        50u,  61u,  73u,  85u,  97u,  108u, 118u, 128u,
        129u, 132u, 136u, 141u, 147u, 154u, 161u, 170u,
        178u, 188u, 198u, 208u, 219u, 230u, 242u, 255u
    };
    if (slot_0_31 > 31u) {
        slot_0_31 = 31u;
    }
    return kProbabilityCurve[slot_0_31];
}

static void hold_probability_until_new_move() {
    g_prob_update_armed = false;
    g_prob_hold_adc = knob_adc_raw();
}

static void arm_probability_tracking() {
    g_prob_update_armed = true;
    g_prob_hold_adc = knob_adc_raw();
}

static void reset_length_knob_tracking() {
    g_length_update_armed = false;
    g_length_hold_adc = knob_adc_raw();
    g_length_adc_filt = 0u;
    g_length_candidate_slot = 0xFFu;
    g_length_candidate_count = 0u;
}

static bool midi_slot_valid(std::uint8_t slot_0_15) {
    return slot_0_15 < kPresetCount;
}

static std::uint16_t midi_slot_addr(std::uint8_t slot_0_15) {
    // MIDI slots 0..15 map to EEPROM presets 1..16 (slot 0 is autosave).
    return preset_slot_addr(static_cast<std::uint16_t>(slot_0_15 + 1u));
}

static bool transport_running() {
    if (g_last_rise_tick == 0u) {
        return false;
    }
    return (g_tick - g_last_rise_tick) < kTransportStopTicks;
}

static bool manual_queued_step_display_ready() {
    if (!g_reset_policy.manual_mode()) {
        return false;
    }

    // For manual-reset display parking, react on the expected next pulse time
    // instead of waiting for the coarse 1.5s transport-stopped timeout.
    if (g_last_period_ticks != 0u && g_last_rise_tick != 0u) {
        return (g_tick - g_last_rise_tick) >= g_last_period_ticks;
    }

    return !transport_running();
}

static bool show_step1_ready_indicator() {
    if (g_sequencer.reset_pending()) {
        return true;
    }

    // In manual reset mode, a stopped clock should leave the last step display parked
    // until an explicit reset jack pulse arrives.
    if (g_reset_policy.manual_mode()) {
        return false;
    }

    return !transport_running();
}

static void apply_recalled_preset(const iris_common::Preset& preset) {
    if (!preset.is_valid()) {
        return;
    }

    g_current_preset = preset;
    g_current_preset.update_checksum();
    g_sequencer.set_probability(g_current_preset.probability);
    g_sequencer.set_length(g_current_preset.sequence_length);
    g_sequencer.set_division(g_current_preset.division);

    g_prob_slot = static_cast<std::uint8_t>(
        (static_cast<std::uint16_t>(g_current_preset.probability) * 31u + 127u) / 255u);
    g_length_slot = static_cast<std::uint8_t>(
        (g_current_preset.division - 1u) * 8u + (g_current_preset.sequence_length - 1u));
    g_division_page = g_current_preset.division;

    // Re-arm knob capture from current position to avoid sudden takeover.
    hold_probability_until_new_move();
    reset_length_knob_tracking();

    refresh_autosave_watch_snapshot();
    g_last_control_change_tick = g_tick;
}

static void queue_midi_recall(std::uint8_t slot_0_15) {
    if (!midi_slot_valid(slot_0_15)) {
        return;
    }
    g_midi_recall_slot = slot_0_15;
    g_midi_recall_queued = true;
}

static void queue_midi_save(std::uint8_t slot_0_15) {
    if (!midi_slot_valid(slot_0_15)) {
        return;
    }
    g_midi_save_slot = slot_0_15;
    g_midi_save_queued = true;
}

static void service_midi_store() {
    g_midi_store.tick();

    if (g_midi_store_op != MidiStoreOp::None && !g_midi_store.busy() && g_midi_store.done()) {
        if (g_midi_store_op == MidiStoreOp::RecallLoad) {
            g_midi_store.finalize_load(slot_to_probability_255(knob_probability_slot()));
            if (g_midi_store.last_load_valid()) {
                g_midi_loaded_preset = g_midi_store.preset();
                g_midi_recall_ready = true;
                if (!g_midi_recall_timing.pending()) {
                    apply_recalled_preset(g_midi_store.preset());
                    g_midi_recall_ready = false;
                }
            }
        }
        g_midi_store_op = MidiStoreOp::None;
    }

    if (g_midi_recall_timing.apply_if_stopped(transport_running(), g_midi_recall_ready)) {
        apply_recalled_preset(g_midi_loaded_preset);
        g_midi_recall_ready = false;
    }

    if (g_midi_store_op != MidiStoreOp::None || g_midi_store.busy()) {
        return;
    }

    // MIDI recall/save has priority over autosave activity.
    if (g_preset_store.busy() || g_autosave_pending || g_learning_store.busy()) {
        return;
    }

    if (g_midi_recall_queued) {
        g_midi_store.set_base_addr(midi_slot_addr(g_midi_recall_slot));
        if (g_midi_store.begin_load()) {
            g_midi_store_op = MidiStoreOp::RecallLoad;
            g_midi_recall_ready = false;
            g_midi_recall_timing.request(transport_running());
            g_midi_recall_queued = false;
        }
        return;
    }

    if (g_midi_save_queued) {
        g_midi_store.set_base_addr(midi_slot_addr(g_midi_save_slot));
        if (g_midi_store.begin_save(g_current_preset)) {
            g_midi_store_op = MidiStoreOp::SaveSlot;
            g_midi_save_queued = false;
        }
    }
}

static void handle_midi_action(const MidiPresetAction& action) {
    if (action.type != MidiPresetActionType::None) {
        ++g_midi_action_count;
        g_midi_last_action_type = static_cast<std::uint8_t>(action.type);
        g_midi_last_action_slot = action.slot;
    }
    if (action.type == MidiPresetActionType::RecallSlot) {
        ++g_midi_recall_action_count;
        queue_midi_recall(action.slot);
    } else if (action.type == MidiPresetActionType::SaveSlot) {
        ++g_midi_save_action_count;
        queue_midi_save(action.slot);
    }
}

static void apply_probability_slot(std::uint8_t new_slot) {
    g_prob_slot = new_slot;
    const std::uint8_t prob = slot_to_probability_255(g_prob_slot);
    if (prob != g_current_preset.probability) {
        g_current_preset.probability = prob;
        g_sequencer.set_probability(prob);

        // Update learning histogram (8 bins).
        if (kEnableLearningStore && g_learning_store.prefs().is_valid()) {
            const uint8_t bin = static_cast<uint8_t>(new_slot / 4); // 0..7
            iris_common::LearningPrefs& prefs = g_learning_store.prefs_mut();
            if (prefs.prob_hist[bin] < 255) {
                prefs.prob_hist[bin]++;
                prefs.update_checksum();
                g_learning_dirty = true;
            }
        }
    }
}

static void update_probability_from_knob() {
    const std::uint16_t adc = knob_adc_raw();
    const std::uint8_t new_slot = knob_probability_slot();
    const uint16_t delta = (adc > g_prob_hold_adc) ? (adc - g_prob_hold_adc) : (g_prob_hold_adc - adc);
    if (!g_prob_update_armed) {
        if (delta <= kProbResumeHysteresis) {
            return;
        }
        g_prob_update_armed = true;
        g_prob_hold_adc = adc;
        if (new_slot != g_prob_slot) {
            apply_probability_slot(new_slot);
        }
        return;
    }

    if (delta <= kProbKnobHysteresis) {
        return;
    }

    if (new_slot == g_prob_slot) {
        g_prob_hold_adc = adc;
        return;
    }

    g_prob_hold_adc = adc;
    apply_probability_slot(new_slot);
}

static void set_length_single_display(std::uint8_t length);

static void update_length_mode_from_knob() {
    const std::uint16_t raw = knob_adc_raw();
    if (g_length_adc_filt == 0) {
        g_length_adc_filt = raw;
    }
    // Stronger IIR filter plus slot stability keeps the select page from
    // rattling between adjacent values near a boundary.
    g_length_adc_filt = static_cast<std::uint16_t>((g_length_adc_filt * 7u + raw) / 8u);
    const std::uint16_t adc = g_length_adc_filt;
    const uint16_t delta = (adc > g_length_hold_adc) ? (adc - g_length_hold_adc) : (g_length_hold_adc - adc);
    if (!g_length_update_armed) {
        if (delta > kLengthKnobHysteresis) {
            g_length_update_armed = true;
            g_length_hold_adc = adc;
        } else {
            return;
        }
    }

    // Map full ADC range into 32 slots.
    std::uint32_t slot_u32 = (static_cast<std::uint32_t>(adc) * 32u) / 4096u;
    if (slot_u32 > 31u) slot_u32 = 31u;
    const std::uint8_t new_slot = static_cast<std::uint8_t>(slot_u32);

    if (new_slot == g_length_slot) {
        g_length_candidate_slot = 0xFFu;
        g_length_candidate_count = 0u;
        return;
    }

    if (g_length_candidate_slot != new_slot) {
        g_length_candidate_slot = new_slot;
        g_length_candidate_count = 1u;
        return;
    }
    if (g_length_candidate_count < 0xFFu) {
        ++g_length_candidate_count;
    }
    if (g_length_candidate_count < kLengthSlotStableTicks) {
        return;
    }

    g_length_slot = new_slot;
    g_length_hold_adc = adc;
    g_length_candidate_slot = 0xFFu;
    g_length_candidate_count = 0u;
    g_length_last_move_tick = g_tick;

    const std::uint8_t slot = g_length_slot;
    const std::uint8_t division = static_cast<std::uint8_t>((slot / 8) + 1);
    const std::uint8_t length = static_cast<std::uint8_t>((slot % 8) + 1);

    if (division != g_division_page) {
        g_division_page = division;
    }

    g_current_preset.division = division;
    g_current_preset.sequence_length = length;
    g_sequencer.set_division(division);
    g_sequencer.set_length(length);

    set_length_single_display(length);
}

static void update_mute_led_normal() {
    const bool muted = (g_current_preset.mute != 0);
    if (muted) {
        iris_common::hal::set_mute_led_gpio(true);
        return;
    }

    // Show actual output status when unmuted:
    // output high only when route is enabled and clock input is high.
    const bool output_high = g_sequencer.clock_route_high() && iris_common::hal::gpio_read_pa8();
    iris_common::hal::set_mute_led_gpio(output_high);
}

static void update_mute_led_fade() {
    // 1s up, 1s down triangle with darker bias (quadratic curve).
    g_fade_phase = static_cast<std::uint16_t>(g_tick % kMuteFadePeriodTicks);
    const std::uint16_t lin = (g_fade_phase < kMuteFadeHalfTicks)
        ? g_fade_phase
        : static_cast<std::uint16_t>(kMuteFadePeriodTicks - g_fade_phase);
    // Apply quadratic to emphasize darker range.
    const std::uint16_t duty = static_cast<std::uint16_t>((static_cast<std::uint32_t>(lin) * lin) / 1500u);
    iris_common::hal::set_mute_led_pwm(duty);
}

static void update_step_led_fade() {
    if (show_step1_ready_indicator()) {
        // Step 1 is next when reset is queued or transport appears stopped.
        for (std::uint8_t i = 0; i < 8; ++i) {
            iris1::leds::set_step_duty(i, (i == 0u) ? 1000u : 0u);
        }
        return;
    }

    const std::uint8_t step = g_sequencer.step_index();
    const std::uint8_t division = g_sequencer.division();
    const std::uint8_t pulse = g_sequencer.pulse_index();
    std::uint16_t duty = 0;

    if (division <= 1) {
        duty = 1000;
    } else if (pulse == 0u) {
        // First pulse of the stage is always full bright.
        duty = 1000;
    } else {
        const std::uint32_t period = g_last_period_ticks;
        const std::uint32_t fade_pulses = static_cast<std::uint32_t>(division - 1u);
        if (period == 0u) {
            // Fallback if timing is not established yet: fade by pulse index only.
            const std::uint32_t remaining_steps =
                (pulse >= division) ? 0u : static_cast<std::uint32_t>(division - pulse);
            duty = static_cast<std::uint16_t>((remaining_steps * 1000u) / fade_pulses);
        } else {
            // Linear fade starts at top of pulse 2 and reaches 0 by end of last pulse.
            std::uint32_t elapsed_in_pulse = 0u;
            if (g_last_rise_tick > 0u && g_tick > g_last_rise_tick) {
                elapsed_in_pulse = g_tick - g_last_rise_tick;
                if (elapsed_in_pulse > period) {
                    elapsed_in_pulse = period;
                }
            }
            const std::uint32_t fade_total = fade_pulses * period;
            const std::uint32_t fade_elapsed =
                static_cast<std::uint32_t>(pulse - 1u) * period + elapsed_in_pulse;
            if (fade_elapsed >= fade_total) {
                duty = 0;
            } else {
                duty = static_cast<std::uint16_t>(((fade_total - fade_elapsed) * 1000u) / fade_total);
            }
        }
    }

    if (manual_queued_step_display_ready() && duty == 0u) {
        const std::uint8_t queued_step = g_sequencer.queued_step_index();
        for (std::uint8_t i = 0; i < 8; ++i) {
            iris1::leds::set_step_duty(i, (i == queued_step) ? 300u : 0u);
        }
        return;
    }

    for (std::uint8_t i = 0; i < 8; ++i) {
        iris1::leds::set_step_duty(i, (i == step) ? duty : 0);
    }
}

static void set_length_single_display(std::uint8_t length) {
    if (length < 1) length = 1;
    if (length > 8) length = 8;
    const std::uint8_t index = static_cast<std::uint8_t>(length - 1);
    for (std::uint8_t i = 0; i < 8; ++i) {
        iris1::leds::set_step_duty(i, (i == index) ? 1000 : 0);
    }
}


static void read_switch_modes(StepMode out_modes[8]) {
    for (int i = 0; i < 8; ++i) {
        const uint8_t pin1 = iris1::switches::kSwitchPin1Map[i];
        const uint8_t pin2 = iris1::switches::kSwitchPin2Map[i];

        const bool p1 = iris_common::hal::gpio_read_pc(pin1);
        const bool p2 = iris_common::hal::gpio_read_pc(pin2);

        // Assumes pull-ups: low means active.
        // Mapping: pin1 low => Off, pin2 low => On, both high => Prob.
        if (!p1 && p2) {
            out_modes[i] = StepMode::Off;
        } else if (p1 && !p2) {
            out_modes[i] = StepMode::On;
        } else {
            out_modes[i] = StepMode::Prob;
        }
    }
}

static void apply_switch_modes(const StepMode modes[8]) {
    for (int i = 0; i < 8; ++i) {
        g_sequencer.set_step_mode(static_cast<std::uint8_t>(i), modes[i]);
    }
}

} // namespace

void init() {
    // Decide next step on boot.
    g_sequencer.reset_queued();
    g_reset_policy.reset();
    const bool reset_now = iris_common::hal::gpio_read_pa2();
    g_reset_raw_prev = reset_now;
    g_reset_filtered_high = reset_now;
    g_reset_high_samples = reset_now ? kResetFilterSamples : 0u;
    g_reset_rise_flag = false;
    g_reset_raw_rise_count = 0u;
    g_reset_filtered_rise_count = 0u;

    // Always load autosave on start.
    g_preset_store.begin_load();
    if (kEnableLearningStore) {
        g_learning_load_started = false;
        g_learning_loaded = false;
    } else {
        g_learning_load_started = true;
        g_learning_loaded = true;
    }
    g_preset_save_queued = false;
    g_learning_save_queued = false;
    g_prob_slot = 0;
    g_midi_controller = MidiPresetController{};
    g_midi_recall_queued = false;
    g_midi_save_queued = false;
    g_midi_recall_ready = false;
    g_midi_recall_timing.reset();
    g_midi_store_op = MidiStoreOp::None;
    g_midi_rx_irq_count = 0u;
    g_midi_rx_byte_count = 0u;
    g_midi_rx_error_count = 0u;
    g_midi_rx_error_flags = 0u;
    g_midi_last_byte = 0u;
    g_midi_action_count = 0u;
    g_midi_recall_action_count = 0u;
    g_midi_save_action_count = 0u;
    g_midi_last_action_type = 0u;
    g_midi_last_action_slot = 0xFFu;
}

void tick(uint32_t tick) {
    // Step 1: minimal tick (sequencer + route only).
    g_sequencer.tick(tick);

    // Drive PA11 (analog mux): low = GND, high = clock to output.
    const bool muted = (g_current_preset.mute != 0);
    iris_common::hal::set_clock_route(g_sequencer.clock_route_high() && !muted);

    // EEPROM state machine.
    g_preset_store.tick();
    if (kEnableLearningStore) {
        g_learning_store.tick();
    }
    service_midi_store();
    service_preset_save_completion();
    service_eeprom_write_queue();

    // ADC polling disabled for isolation (suspect of blackout).

    if (!g_loaded && g_preset_store.done()) {
        g_preset_store.finalize_load(slot_to_probability_255(knob_probability_slot()));
        g_current_preset = g_preset_store.preset();
        g_last_autosave = g_current_preset;
        g_loaded = true;
        g_prob_slot = static_cast<std::uint8_t>((static_cast<std::uint16_t>(g_current_preset.probability) * 31u + 127u) / 255u);
        g_last_autosave_tick = g_tick;
        g_last_control_change_tick = g_tick;
        refresh_autosave_watch_snapshot();
        g_division_page = g_current_preset.division;
        g_sequencer.set_division(g_current_preset.division);
        g_sequencer.set_length(g_current_preset.sequence_length);
        g_length_slot = static_cast<std::uint8_t>((g_current_preset.division - 1) * 8 + (g_current_preset.sequence_length - 1));
        arm_probability_tracking();
        reset_length_knob_tracking();

        // Boot prefill: use the live switch positions immediately so step 1
        // is ready before the first incoming clock pulse.
        StepMode boot_modes[8];
        read_switch_modes(boot_modes);
        apply_switch_modes(boot_modes);
        g_sequencer.set_probability(g_current_preset.probability);
        g_sequencer.reset_queued();

        // If autosave was invalid, write defaults immediately.
        if (!g_preset_store.last_load_valid() && !g_preset_store.busy()) {
            g_force_autosave = true;
            request_preset_save(g_current_preset);
        }
    }

    if (kEnableLearningStore && g_loaded && !g_learning_load_started && !g_preset_store.busy() && !g_autosave_pending) {
        if (g_learning_store.begin_load()) {
            g_learning_load_started = true;
        }
    }

    if (kEnableLearningStore && g_learning_load_started && !g_learning_loaded && g_learning_store.done()) {
        g_learning_store.finalize_load();
        g_sequencer.set_learning_prefs(&g_learning_store.prefs_mut());
        g_learning_loaded = true;
    }

    // Reset edge is filtered in the 8 kHz polling path to ignore crosstalk spikes.
    if (g_reset_rise_flag) {
        g_reset_rise_flag = false;
        g_reset_policy.on_manual_reset_edge();
        on_reset_edge(g_tick);
    }

    // High-rate clock edges captured in TIM14 ISR.
    // Handle reset first so a valid reset pulse can affect the very next clock.
    if (g_clk_rise_flag) {
        g_clk_rise_flag = false;
        on_clock_rise(g_tick);
    }
    if (g_clk_fall_flag) {
        g_clk_fall_flag = false;
        on_clock_fall(g_tick);
    }

    if (g_reset_policy.should_queue_auto_reset(g_tick)) {
        on_reset_edge(g_tick);
    }

    // UI: hold mute for 3s to enter length mode; release to exit.
    const bool mute_pressed = !iris_common::hal::gpio_read_pa3(); // active-low with pull-up

    if (mute_pressed && !g_mute_btn_prev) {
        g_mute_hold_start = g_tick;
        g_longpress_active = false;
    }

    // Full reset after 6s hold (uses preset defaults).
    if (mute_pressed && (g_tick - g_mute_hold_start) >= kFullResetHoldTicks && !g_longpress_active) {
        g_longpress_active = true;
        g_current_preset = iris_common::Preset{};
        g_current_preset.update_checksum();
        g_prob_slot = static_cast<std::uint8_t>((static_cast<std::uint16_t>(g_current_preset.probability) * 31u + 127u) / 255u);
        g_division_page = g_current_preset.division;
        g_sequencer.set_division(g_current_preset.division);
        g_sequencer.set_length(g_current_preset.sequence_length);
        g_sequencer.set_probability(g_current_preset.probability);
        g_length_slot = static_cast<std::uint8_t>((g_current_preset.division - 1) * 8 + (g_current_preset.sequence_length - 1));
        reset_length_knob_tracking();
        arm_probability_tracking();

        if (!g_preset_store.busy()) {
            g_force_autosave = true;
            request_preset_save(g_current_preset);
            g_last_autosave_tick = g_tick;
        }

        if (kEnableLearningStore && !g_learning_store.busy()) {
            iris_common::LearningPrefs cleared{};
            cleared.update_checksum();
            queue_learning_save(cleared);
            g_learning_dirty = false;
        }
    }

    if (g_mode == UiMode::Normal) {
        if (mute_pressed && (g_tick - g_mute_hold_start) >= kLengthModeHoldTicks) {
            g_mode = UiMode::LengthSet;
            hold_probability_until_new_move();
            g_longpress_active = true;
            reset_length_knob_tracking();
            g_length_last_move_tick = g_tick;
        }
        // Apply any queued switch changes from length mode.
        if (g_pending_modes_valid) {
            apply_switch_modes(g_pending_modes);
            g_pending_modes_valid = false;
        } else {
            // Normal mode: read switches and apply immediately.
            StepMode modes[8];
            read_switch_modes(modes);
            apply_switch_modes(modes);
        }
        update_probability_from_knob();
        if (mute_pressed && (g_tick - g_mute_hold_start) < kLengthModeHoldTicks) {
            // During hold-to-enter, suppress clock flicker.
            iris_common::hal::set_mute_led_gpio(false);
        } else {
            update_mute_led_normal();
        }
        update_step_led_fade();
    } else {
        // Length set mode
        update_length_mode_from_knob();
        // Always show length value in this mode, even if knob hasn't moved yet.
        set_length_single_display(g_current_preset.sequence_length);
        update_mute_led_fade();

        // Capture switch changes to apply after exit.
        StepMode modes[8];
        read_switch_modes(modes);
        for (int i = 0; i < 8; ++i) {
            g_pending_modes[i] = modes[i];
        }
        g_pending_modes_valid = true;

        // Exit length mode after inactivity.
        if ((g_tick - g_length_last_move_tick) >= kLengthModeExitTicks) {
            g_mode = UiMode::Normal;
            iris_common::hal::set_mute_led_gpio(false);
            hold_probability_until_new_move();
            g_longpress_active = false;
            reset_length_knob_tracking();
        }
    }

    // Short press toggles mute (only if not long-pressing).
    if (!mute_pressed && g_mute_btn_prev && !g_longpress_active) {
        g_current_preset.mute = g_current_preset.mute ? 0 : 1;
    }

    g_mute_btn_prev = mute_pressed;
    track_control_activity();

    // Autosave only after user controls are inactive for the window.
    const bool inactive_for_window = (g_tick - g_last_control_change_tick) >= kAutosaveIntervalTicks;
    const bool save_retry_window = (g_tick - g_last_autosave_tick) >= kAutosaveIntervalTicks;
    if (g_loaded && inactive_for_window && save_retry_window) {
        const bool changed = autosave_fields_differ(g_current_preset, g_last_autosave);
        if (!g_preset_store.busy() && !g_autosave_pending && (changed || g_force_autosave)) {
            if (request_preset_save(g_current_preset)) {
                g_last_autosave_tick = g_tick;
            }
        }
    }

    // Learning prefs save every 10 seconds if dirty.
    if (kEnableLearningStore && g_tick - g_last_learning_save_tick >= kLearningSaveIntervalTicks) {
        g_last_learning_save_tick = g_tick;
        if (g_sequencer.learning_dirty()) {
            g_learning_dirty = true;
            g_sequencer.clear_learning_dirty();
        }
        // Keep slot-0 autosave as highest priority on the shared EEPROM bus.
        if (g_learning_dirty && g_learning_loaded &&
            !g_learning_store.busy() && !g_preset_store.busy() && !g_autosave_pending) {
            g_learning_store.begin_save(g_learning_store.prefs());
            g_learning_dirty = false;
        } else if (g_learning_dirty && g_learning_loaded) {
            queue_learning_save(g_learning_store.prefs());
            g_learning_dirty = false;
        }
    }

}

void on_clock_fall(uint32_t tick) {
    if (g_midi_recall_timing.on_clock_fall(g_midi_recall_ready) && g_midi_recall_ready) {
        apply_recalled_preset(g_midi_loaded_preset);
        g_midi_recall_ready = false;
    }
    g_sequencer.on_clock_fall(tick);
}

void on_clock_rise(uint32_t tick) {
    g_prev_rise_tick = g_last_rise_tick;
    g_last_rise_tick = tick;
    g_reset_policy.on_clock_rise(tick);

    if (g_prev_rise_tick != 0 && g_last_rise_tick > g_prev_rise_tick) {
        g_last_period_ticks = g_last_rise_tick - g_prev_rise_tick;
    }

    const bool advanced = g_sequencer.on_clock_rise(tick);
    g_midi_recall_timing.on_clock_rise(advanced);
}

void on_reset_edge(uint32_t tick) {
    (void)tick;
    g_sequencer.reset_queued();
}

void on_systick() {
    ++g_tick;
}

uint32_t now_tick() {
    return g_tick;
}

void on_fast_tick() {
    static bool clk_prev = false;
    const bool clk_now = iris_common::hal::gpio_read_pa8();
    if (clk_now && !clk_prev) {
        g_clk_rise_flag = true;
    } else if (!clk_now && clk_prev) {
        g_clk_fall_flag = true;
    }
    clk_prev = clk_now;

    const bool reset_now = iris_common::hal::gpio_read_pa2();
    if (reset_now && !g_reset_raw_prev) {
        ++g_reset_raw_rise_count;
    }
    g_reset_raw_prev = reset_now;

    if (reset_now) {
        if (g_reset_high_samples < kResetFilterSamples) {
            ++g_reset_high_samples;
        }
    } else {
        g_reset_high_samples = 0u;
    }

    const bool reset_filtered_now = (g_reset_high_samples >= kResetFilterSamples);
    if (reset_filtered_now && !g_reset_filtered_high) {
        g_reset_rise_flag = true;
        ++g_reset_filtered_rise_count;
    }
    g_reset_filtered_high = reset_filtered_now;
}

void on_midi_irq(std::uint32_t isr) {
    (void)isr;
    ++g_midi_rx_irq_count;
}

void on_midi_byte(std::uint8_t byte) {
    ++g_midi_rx_byte_count;
    g_midi_last_byte = byte;
    const MidiPresetAction action = g_midi_controller.on_byte(byte);
    handle_midi_action(action);
}

void on_midi_error(std::uint32_t flags) {
    ++g_midi_rx_error_count;
    g_midi_rx_error_flags |= flags;
}

} // namespace iris1::app
