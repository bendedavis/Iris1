#pragma once

#include <cstdint>

namespace iris1::app {

// Called at boot after clocks are configured.
void init();

// Called from main loop.
void tick(uint32_t tick);

// ISR event hooks.
void on_clock_fall(uint32_t tick);
void on_clock_rise(uint32_t tick);
void on_reset_edge(uint32_t tick); // active‑high, queued

// Simple 1 kHz timebase (incremented from SysTick ISR).
void on_systick();
uint32_t now_tick();

// High-rate clock sampling (called from TIM14 IRQ).
void on_fast_tick();
void on_midi_irq(std::uint32_t isr);
void on_midi_byte(std::uint8_t byte);
void on_midi_error(std::uint32_t flags);

} // namespace iris1::app
