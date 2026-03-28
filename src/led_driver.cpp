#include "led_driver.h"
#include "led_map.h"
#include "stm32f0_regs.h"

namespace iris1::leds {

static std::uint16_t g_duty[8] = {0};
static std::uint16_t g_acc[8] = {0};

void init_step_leds_pwm() {
    using namespace iris_common::stm32f0;

    // Enable GPIOB clock.
    RCCREG->AHBENR |= RCC_AHBENR_GPIOBEN;

    // Configure PB pins as output based on mapping for steps 0..7.
    for (std::uint8_t i = 0; i < 8; ++i) {
        const std::uint8_t pin = kLedPinMap[i];
        GPIOB->MODER &= ~(0x3u << (pin * 2));
        GPIOB->MODER |=  (0x1u << (pin * 2));
    }
}

void set_step_duty(std::uint8_t index, std::uint16_t duty_0_1000) {
    if (index >= 8) return;
    if (duty_0_1000 > 1000) duty_0_1000 = 1000;
    g_duty[index] = duty_0_1000;
}

void set_length_display(std::uint8_t length) {
    if (length > 8) length = 8;
    for (std::uint8_t i = 0; i < 8; ++i) {
        g_duty[i] = (i < length) ? 1000 : 0;
    }
}

void on_tim14_irq() {
    using namespace iris_common::stm32f0;

    for (std::uint8_t i = 0; i < 8; ++i) {
        g_acc[i] = static_cast<std::uint16_t>(g_acc[i] + g_duty[i]);
        const bool on = (g_acc[i] >= 1000);
        if (on) {
            g_acc[i] = static_cast<std::uint16_t>(g_acc[i] - 1000);
        }
        const std::uint8_t pin = kLedPinMap[i];
        if (on) {
            GPIOB->BSRR = (1u << pin);
        } else {
            GPIOB->BSRR = (1u << (pin + 16));
        }
    }
}

} // namespace iris1::leds
