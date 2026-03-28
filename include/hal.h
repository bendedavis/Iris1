#pragma once

#include <cstdint>

namespace iris_common::hal {

void init_systick_1khz(std::uint32_t sysclk_hz);
void on_systick();
std::uint32_t tick_ms();
void init_exti_pa2_pa8();
bool exti_pending_pa2();
bool exti_pending_pa8();
void exti_clear_pa2();
void exti_clear_pa8();
void init_spi1_eeprom();
void init_adc1_dma_pa0();
std::uint16_t adc_latest();
void adc_poll_tick();
void init_switches_pc();
bool gpio_read_pc(std::uint8_t pin);
void init_mute_button_pa3();
void init_usart1_midi_rx_pa10();
bool usart1_rx_ready();
std::uint8_t usart1_read_byte();
void usart1_clear_errors();
bool gpio_read_pa3();
bool gpio_read_pa2();
bool gpio_read_pa8();
void init_clock_reset_inputs();
void init_clock_route_pa11();
void set_clock_route(bool on);

// Mute LED PWM (high-resolution timer PWM on PA12).
void init_mute_led_pwm();
void set_mute_led_pwm(std::uint16_t duty_0_1000);
void set_mute_led_gpio(bool on);
void on_tim14_irq();

// Clock setup
void init_clocks_hse_pll_48mhz();

// SPI1 polling transfer (non-blocking state machine)
bool spi1_poll_busy();
void spi1_poll_start(const std::uint8_t* tx, std::uint8_t* rx, std::uint16_t len);
void spi1_poll_tick();

// SPI1 + DMA helpers (still used elsewhere if needed)
bool spi1_dma_busy();
void spi1_dma_start_tx(const std::uint8_t* tx, std::uint16_t len);
void spi1_dma_start_txrx(const std::uint8_t* tx, std::uint8_t* rx, std::uint16_t len);
void spi1_dma_start_rx(std::uint8_t* rx, std::uint16_t len);
void on_spi1_dma_irq();

} // namespace iris_common::hal
