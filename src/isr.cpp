// ISR stubs (replace with real handlers)
#include "app.h"
#include "hal.h"
#include "led_driver.h"
#include "led_map.h"
#include "stm32f0_regs.h"

extern "C" {

void HardFault_Handler(void) {
    using namespace iris_common::stm32f0;
    // Minimal hardfault indicator: force LED1/LED2 on and spin.
    RCCREG->AHBENR |= RCC_AHBENR_GPIOBEN;
    const std::uint8_t pin0 = iris1::leds::kLedPinMap[0];
    const std::uint8_t pin1 = iris1::leds::kLedPinMap[1];
    GPIOB->MODER &= ~(0x3u << (pin0 * 2));
    GPIOB->MODER |=  (0x1u << (pin0 * 2));
    GPIOB->MODER &= ~(0x3u << (pin1 * 2));
    GPIOB->MODER |=  (0x1u << (pin1 * 2));
    GPIOB->BSRR = (1u << pin0);
    GPIOB->BSRR = (1u << pin1);
    while (true) { }
}

void SysTick_Handler(void) {
    iris_common::hal::on_systick();
    iris1::app::on_systick();
}

void EXTI2_3_IRQHandler(void) {
    if (iris_common::hal::exti_pending_pa2()) {
        if (iris_common::hal::gpio_read_pa2()) {
            iris1::app::on_reset_edge(iris1::app::now_tick());
        }
        iris_common::hal::exti_clear_pa2();
    }
}

void EXTI4_15_IRQHandler(void) {
    if (iris_common::hal::exti_pending_pa8()) {
        if (iris_common::hal::gpio_read_pa8()) {
            iris1::app::on_clock_rise(iris1::app::now_tick());
        } else {
            iris1::app::on_clock_fall(iris1::app::now_tick());
        }
        iris_common::hal::exti_clear_pa8();
    }
}

void USART1_IRQHandler(void) {
    using namespace iris_common::stm32f0;

    iris1::app::on_midi_irq(USART1->ISR);
    while (iris_common::hal::usart1_rx_ready()) {
        const std::uint8_t b = iris_common::hal::usart1_read_byte();
        iris1::app::on_midi_byte(b);
    }

    const std::uint32_t err = USART1->ISR & (USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE);
    if (err != 0u) {
        iris1::app::on_midi_error(err);
    }
    iris_common::hal::usart1_clear_errors();
}

void DMA1_Channel2_3_IRQHandler(void) {
    // SPI1 DMA completion (TX/RX). Handle channel 2/3 IRQs.
    iris_common::hal::on_spi1_dma_irq();
}

void TIM14_IRQHandler(void) {
    iris_common::hal::on_tim14_irq();
    iris1::leds::on_tim14_irq();
    iris1::app::on_fast_tick();
}

}
