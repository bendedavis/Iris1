#include "led_map.h"
#include "hal.h"
#include "led_driver.h"
#include "app.h"
#include "stm32f0_regs.h"

int main() {
    // Known-good init sequence (no tick loop).
    iris_common::hal::init_clocks_hse_pll_48mhz();
    iris_common::hal::init_systick_1khz(48000000u);
    iris1::leds::init_step_leds_pwm();
    iris_common::hal::init_adc1_dma_pa0();
    iris_common::hal::init_spi1_eeprom();
    iris_common::hal::init_clock_reset_inputs();
    iris_common::hal::init_clock_route_pa11();
    iris_common::hal::init_mute_button_pa3();
    iris_common::hal::init_usart1_midi_rx_pa10();
    iris_common::hal::init_switches_pc();
    iris_common::hal::init_mute_led_pwm();
    iris1::app::init();

    // Let the app drive step LEDs.

    while (true) {
        iris1::app::tick(iris1::app::now_tick());
    }
}
