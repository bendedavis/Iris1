#pragma once

#include <cstdint>

namespace iris1::leds {

// Initialize step LEDs on Port B for software PWM.
void init_step_leds_pwm();

// Set duty (0..1000) for step LED index 0..7.
void set_step_duty(std::uint8_t index, std::uint16_t duty_0_1000);

// Convenience: set first N LEDs full bright, rest off.
void set_length_display(std::uint8_t length);

// Called from TIM14 IRQ to update PWM outputs.
void on_tim14_irq();

} // namespace iris1::leds
