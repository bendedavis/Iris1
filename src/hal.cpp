#include "hal.h"
#include "stm32f0_regs.h"

namespace iris_common::hal {

static volatile bool g_spi1_dma_busy = false;
static volatile bool g_spi1_dma_tx_done = false;
static volatile bool g_spi1_dma_rx_done = false;
static std::uint8_t g_spi1_dummy_tx = 0xFF;
static std::uint8_t g_spi1_dummy_rx = 0x00;
static volatile std::uint16_t g_adc_last = 0;
static volatile std::uint16_t g_adc_dma_buf[1] = {0};

static volatile bool g_mute_pwm_enabled = false;
static volatile std::uint16_t g_mute_pwm_duty = 0; // 0..1000
static volatile std::uint16_t g_mute_pwm_acc = 0;

static const std::uint8_t* g_spi1_poll_tx = nullptr;
static std::uint8_t* g_spi1_poll_rx = nullptr;
static std::uint16_t g_spi1_poll_len = 0;
static std::uint16_t g_spi1_poll_idx = 0;
static bool g_spi1_poll_busy = false;
static bool g_spi1_poll_inflight = false;
static bool g_spi1_poll_done_pending = false;
static volatile std::uint32_t g_ms_tick = 0;

void init_systick_1khz(std::uint32_t sysclk_hz) {
    using namespace iris_common::stm32f0;
    const std::uint32_t reload = (sysclk_hz / 1000u) - 1u;
    SYSTICK->LOAD = reload;
    SYSTICK->VAL = 0u;
    SYSTICK->CTRL = SYSTICK_CTRL_CLKSOURCE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_ENABLE;
}

void on_systick() {
    ++g_ms_tick;
}

std::uint32_t tick_ms() {
    return g_ms_tick;
}

void init_exti_pa2_pa8() {
    using namespace iris_common::stm32f0;

    // Enable GPIOA and SYSCFG clocks.
    RCCREG->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCCREG->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    // EXTI lines default to Port A, so no EXTICR changes required.

    // Configure EXTI for PA2 (reset): rising edge only.
    EXTIREG->IMR |= EXTI_LINE_2;
    EXTIREG->RTSR |= EXTI_LINE_2;
    EXTIREG->FTSR &= ~EXTI_LINE_2;

    // Configure EXTI for PA8 (clock): rising and falling edges.
    EXTIREG->IMR |= EXTI_LINE_8;
    EXTIREG->RTSR |= EXTI_LINE_8;
    EXTIREG->FTSR |= EXTI_LINE_8;

    // Enable NVIC interrupts.
    nvic_enable_irq(EXTI2_3_IRQn);
    nvic_enable_irq(EXTI4_15_IRQn);
}

void init_spi1_eeprom() {
    using namespace iris_common::stm32f0;

    // Enable GPIOA, SPI1, DMA1 clocks.
    RCCREG->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_DMA1EN;
    RCCREG->APB2ENR |= RCC_APB2ENR_SPI1EN;

    // GPIOA PA5-PA7 to AF mode (AF0 for SPI1). PA4 will be GPIO for CS.
    GPIOA->MODER &= ~(0xFCu << (4 * 2));
    GPIOA->MODER |=  (0xA8u << (4 * 2));

    // AFRL: AF0 => 0, clear bits for safety (PA5-PA7).
    GPIOA->AFRL &= ~(0xFFFu << (5 * 4));

    // PA4 as GPIO output for CS (active low).
    GPIOA->MODER &= ~(0x3u << (4 * 2));
    GPIOA->MODER |=  (0x1u << (4 * 2));
    GPIOA->BSRR = (1u << 4); // CS high

    // Configure SPI1: master, mode 0, 8-bit, MSB first.
    // Prescaler: /64 => 750 kHz at 48 MHz SYSCLK (safe for M95080).
    SPI1->CR1 = 0;
    SPI1->CR1 |= SPI_CR1_MSTR;
    SPI1->CR1 |= (0x5u << SPI_CR1_BR_Pos); // BR=101 => /64
    SPI1->CR1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA | SPI_CR1_LSBFIRST);
    SPI1->CR1 |= SPI_CR1_SSM | SPI_CR1_SSI; // software NSS

    // DS=8-bit frame, FRXTH=8-bit RXNE event threshold.
    SPI1->CR2 = SPI_CR2_DS_8BIT | SPI_CR2_FRXTH;

    SPI1->CR1 |= SPI_CR1_SPE;

    // Enable DMA1 channel 2/3 IRQ.
    nvic_enable_irq(DMA1_Channel2_3_IRQn);
}

void init_adc1_dma_pa0() {
    using namespace iris_common::stm32f0;

    // Enable GPIOA, DMA1 and ADC clocks.
    RCCREG->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_DMA1EN;
    RCCREG->APB2ENR |= RCC_APB2ENR_ADC1EN;

    // Enable HSI14 for ADC clock (async).
    RCCREG->CR2 &= ~RCC_CR2_HSI14DIS;
    RCCREG->CR2 |= RCC_CR2_HSI14ON;
    // Wait for HSI14 ready with timeout to avoid boot hang.
    for (std::uint32_t wait = 0; wait < 200000u; ++wait) {
        if (RCCREG->CR2 & RCC_CR2_HSI14RDY) {
            break;
        }
    }

    // PA0 to analog mode (MODER = 11b).
    GPIOA->MODER &= ~(0x3u << (0 * 2));
    GPIOA->MODER |=  (0x3u << (0 * 2));
    // Ensure no pull-up/down on PA0.
    GPIOA->PUPDR &= ~(0x3u << (0 * 2));

    // Calibrate ADC (must be disabled).
    if (ADC1->CR & ADC_CR_ADEN) {
        ADC1->CR &= ~ADC_CR_ADEN;
        for (std::uint32_t wait = 0; wait < 200000u; ++wait) {
            if (!(ADC1->CR & ADC_CR_ADEN)) break;
        }
    }
    ADC1->CR |= ADC_CR_ADCAL;
    for (std::uint32_t wait = 0; wait < 200000u; ++wait) {
        if (!(ADC1->CR & ADC_CR_ADCAL)) break;
    }

    // Configure ADC clock mode: asynchronous HSI14 (CKMODE = 00).
    ADC1->CFGR2 &= ~0x3u;

    // Configure ADC: continuous conversion with DMA (circular).
    ADC1->CFGR1 = ADC_CFGR1_CONT | ADC_CFGR1_DMAEN | ADC_CFGR1_DMACFG;

    // Sample time: max for stability.
    ADC1->SMPR = ADC_SMPR_239_5;

    // Select channel 0 (PA0).
    ADC1->CHSELR = (1u << 0);

    // Configure DMA1 Channel1 for ADC1 (circular, 16-bit).
    DMA1->CH[0].CCR &= ~DMA_CCR_EN;
    DMA1->CH[0].CPAR = reinterpret_cast<std::uint32_t>(&ADC1->DR);
    DMA1->CH[0].CMAR = reinterpret_cast<std::uint32_t>(g_adc_dma_buf);
    DMA1->CH[0].CNDTR = 1;
    DMA1->CH[0].CCR = DMA_CCR_CIRC | DMA_CCR_PSIZE_16 | DMA_CCR_MSIZE_16;
    DMA1->CH[0].CCR |= DMA_CCR_EN;

    // Clear EOC/OVR and set ADRDY flag.
    ADC1->ISR = (1u << 2) | (1u << 4); // EOC | OVR
    ADC1->ISR |= ADC_ISR_ADRDY;

    // Enable ADC and start conversions.
    ADC1->CR |= ADC_CR_ADEN;
    for (std::uint32_t wait = 0; wait < 200000u; ++wait) {
        if (ADC1->ISR & ADC_ISR_ADRDY) break;
    }
    ADC1->CR |= ADC_CR_ADSTART;
}

void init_mute_led_pwm() {
    using namespace iris_common::stm32f0;

    // Enable GPIOA and TIM14 clocks.
    RCCREG->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCCREG->APB1ENR |= RCC_APB1ENR_TIM14EN;

    // PA12 as GPIO output (software PWM toggles).
    GPIOA->MODER &= ~(0x3u << (12 * 2));
    GPIOA->MODER |=  (0x1u << (12 * 2));

    // TIM14 setup: update interrupt at 8 kHz.
    // 48 MHz / (PSC+1) / (ARR+1) = 8 kHz -> PSC=47, ARR=124.
    TIM14->PSC = 47;
    TIM14->ARR = 124;
    TIM14->EGR = TIM_EGR_UG;
    TIM14->DIER |= TIM_DIER_UIE;
    TIM14->CR1 |= TIM_CR1_CEN;

    nvic_enable_irq(TIM14_IRQn);

    g_mute_pwm_enabled = false;
    g_mute_pwm_duty = 0;
    g_mute_pwm_acc = 0;
}

void init_clock_reset_inputs() {
    using namespace iris_common::stm32f0;

    RCCREG->AHBENR |= RCC_AHBENR_GPIOAEN;

    // PA8 input, no pull.
    GPIOA->MODER &= ~(0x3u << (8 * 2));
    GPIOA->PUPDR &= ~(0x3u << (8 * 2));

    // PA2 input with weak pull-down to discourage stray high pulses on the
    // reset jack path when nothing valid is driving it.
    GPIOA->MODER &= ~(0x3u << (2 * 2));
    GPIOA->PUPDR &= ~(0x3u << (2 * 2));
    GPIOA->PUPDR |=  (0x2u << (2 * 2));

    // Bootloader may leave EXTI clock/reset lines enabled before jump-to-app.
    // Iris1 uses PA8/PA2 polling, so force EXTI off to avoid double edge events.
    const std::uint32_t lines = EXTI_LINE_2 | EXTI_LINE_8;
    EXTIREG->IMR &= ~lines;
    EXTIREG->RTSR &= ~lines;
    EXTIREG->FTSR &= ~lines;
    EXTIREG->PR = lines;
    nvic_disable_irq(EXTI2_3_IRQn);
    nvic_disable_irq(EXTI4_15_IRQn);
}

void init_clock_route_pa11() {
    using namespace iris_common::stm32f0;

    RCCREG->AHBENR |= RCC_AHBENR_GPIOAEN;
    // PA11 output
    GPIOA->MODER &= ~(0x3u << (11 * 2));
    GPIOA->MODER |=  (0x1u << (11 * 2));
    // default low (GND to output)
    GPIOA->BSRR = (1u << (11 + 16));
}

void set_clock_route(bool on) {
    using namespace iris_common::stm32f0;
    if (on) {
        GPIOA->BSRR = (1u << 11);
    } else {
        GPIOA->BSRR = (1u << (11 + 16));
    }
}

void init_switches_pc() {
    using namespace iris_common::stm32f0;

    // Enable GPIOC clock.
    RCCREG->AHBENR |= RCC_AHBENR_GPIOCEN;

    // Configure PC0..PC15 as inputs with pull-ups.
    // MODER: 00b for input.
    GPIOC->MODER = 0x00000000u;
    // PUPDR: 01b pull-up for all pins.
    GPIOC->PUPDR = 0x55555555u;
}

void init_mute_button_pa3() {
    using namespace iris_common::stm32f0;

    RCCREG->AHBENR |= RCC_AHBENR_GPIOAEN;
    // PA3 input
    GPIOA->MODER &= ~(0x3u << (3 * 2));
    // Pull-up on PA3
    GPIOA->PUPDR &= ~(0x3u << (3 * 2));
    GPIOA->PUPDR |=  (0x1u << (3 * 2));
}

void init_usart1_midi_rx_pa10() {
    using namespace iris_common::stm32f0;

    RCCREG->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCCREG->APB2ENR |= RCC_APB2ENR_USART1EN;

    // PA10 -> USART1_RX (AF1), no pull.
    GPIOA->MODER &= ~(0x3u << (10 * 2));
    GPIOA->MODER |=  (0x2u << (10 * 2)); // AF mode
    GPIOA->AFRH &= ~(0xFu << ((10 - 8) * 4));
    GPIOA->AFRH |=  (0x1u << ((10 - 8) * 4)); // AF1
    GPIOA->PUPDR &= ~(0x3u << (10 * 2));

    // MIDI baud: 31250 at 48 MHz -> BRR = 1536.
    USART1->CR1 = 0u;
    USART1->CR2 = 0u;
    USART1->CR3 = 0u;
    USART1->BRR = 1536u;
    USART1->ICR = USART_ICR_FECF | USART_ICR_NCF | USART_ICR_ORECF;
    USART1->CR1 = USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    nvic_enable_irq(USART1_IRQn);
}

void init_clocks_hse_pll_48mhz() {
    using namespace iris_common::stm32f0;

    // Enable HSE
    RCCREG->CR |= RCC_CR_HSEON;
    bool hse_ready = false;
    for (std::uint32_t wait = 0; wait < 200000u; ++wait) {
        if (RCCREG->CR & RCC_CR_HSERDY) {
            hse_ready = true;
            break;
        }
    }
    if (!hse_ready) {
        // Fallback to HSI if HSE never stabilizes.
        return;
    }

    // Configure PLL: source HSE, prediv=1, mul=3 => 16MHz * 3 = 48MHz
    RCCREG->CFGR &= ~(0xF << RCC_CFGR_PLLMUL_Pos);
    RCCREG->CFGR |= (1u << RCC_CFGR_PLLMUL_Pos); // 0010: x3 (per RM)
    RCCREG->CFGR |= RCC_CFGR_PLLSRC_HSE_PREDIV;
    RCCREG->CFGR2 &= ~(0xF << RCC_CFGR2_PREDIV_Pos); // PREDIV=1

    // Enable PLL
    RCCREG->CR |= RCC_CR_PLLON;
    for (std::uint32_t wait = 0; wait < 200000u; ++wait) {
        if (RCCREG->CR & RCC_CR_PLLRDY) {
            break;
        }
    }

    // Switch SYSCLK to PLL
    RCCREG->CFGR &= ~(0x3u << RCC_CFGR_SW_Pos);
    RCCREG->CFGR |= RCC_CFGR_SW_PLL;
    for (std::uint32_t wait = 0; wait < 200000u; ++wait) {
        if ((RCCREG->CFGR & (0x3u << RCC_CFGR_SWS_Pos)) == RCC_CFGR_SWS_PLL) {
            break;
        }
    }
}

void set_mute_led_pwm(std::uint16_t duty_0_1000) {
    if (duty_0_1000 > 1000) duty_0_1000 = 1000;
    g_mute_pwm_duty = duty_0_1000;
    g_mute_pwm_enabled = true;
}

void set_mute_led_gpio(bool on) {
    using namespace iris_common::stm32f0;
    g_mute_pwm_enabled = false;
    if (on) {
        GPIOA->BSRR = (1u << 12);
    } else {
        GPIOA->BSRR = (1u << (12 + 16));
    }
}

void on_tim14_irq() {
    using namespace iris_common::stm32f0;

    if (TIM14->SR & TIM_SR_UIF) {
        TIM14->SR &= ~TIM_SR_UIF;

        if (!g_mute_pwm_enabled) {
            return;
        }

        // Sigma-delta style PWM for high-resolution duty at fixed tick rate.
        g_mute_pwm_acc = static_cast<std::uint16_t>(g_mute_pwm_acc + g_mute_pwm_duty);
        if (g_mute_pwm_acc >= 1000) {
            g_mute_pwm_acc = static_cast<std::uint16_t>(g_mute_pwm_acc - 1000);
            GPIOA->BSRR = (1u << 12);
        } else {
            GPIOA->BSRR = (1u << (12 + 16));
        }
    }
}

std::uint16_t adc_latest() {
    g_adc_last = g_adc_dma_buf[0];
    return g_adc_last;
}

void adc_poll_tick() {
    // DMA continuously updates g_adc_dma_buf; nothing to do here.
}

bool exti_pending_pa2() {
    using namespace iris_common::stm32f0;
    return (EXTIREG->PR & EXTI_LINE_2) != 0u;
}

bool exti_pending_pa8() {
    using namespace iris_common::stm32f0;
    return (EXTIREG->PR & EXTI_LINE_8) != 0u;
}

void exti_clear_pa2() {
    using namespace iris_common::stm32f0;
    EXTIREG->PR = EXTI_LINE_2;
}

void exti_clear_pa8() {
    using namespace iris_common::stm32f0;
    EXTIREG->PR = EXTI_LINE_8;
}

bool gpio_read_pa2() {
    using namespace iris_common::stm32f0;
    return (GPIOA->IDR & (1u << 2)) != 0u;
}

bool gpio_read_pa8() {
    using namespace iris_common::stm32f0;
    return (GPIOA->IDR & (1u << 8)) != 0u;
}

bool gpio_read_pa3() {
    using namespace iris_common::stm32f0;
    return (GPIOA->IDR & (1u << 3)) != 0u;
}

bool gpio_read_pc(std::uint8_t pin) {
    using namespace iris_common::stm32f0;
    return (GPIOC->IDR & (1u << pin)) != 0u;
}

bool usart1_rx_ready() {
    using namespace iris_common::stm32f0;
    return (USART1->ISR & USART_ISR_RXNE) != 0u;
}

std::uint8_t usart1_read_byte() {
    using namespace iris_common::stm32f0;
    return static_cast<std::uint8_t>(USART1->RDR & 0xFFu);
}

void usart1_clear_errors() {
    using namespace iris_common::stm32f0;
    const std::uint32_t isr = USART1->ISR;
    if (isr & (USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)) {
        USART1->ICR = USART_ICR_FECF | USART_ICR_NCF | USART_ICR_ORECF;
    }
}

bool spi1_dma_busy() {
    return g_spi1_dma_busy;
}

bool spi1_poll_busy() {
    return g_spi1_poll_busy;
}

void spi1_poll_start(const std::uint8_t* tx, std::uint8_t* rx, std::uint16_t len) {
    if (g_spi1_poll_busy || len == 0) return;
    // Assert CS (active low) on PA4.
    stm32f0::GPIOA->BSRR = (1u << (4 + 16));
    g_spi1_poll_tx = tx;
    g_spi1_poll_rx = rx;
    g_spi1_poll_len = len;
    g_spi1_poll_idx = 0;
    g_spi1_poll_busy = true;
    g_spi1_poll_inflight = false;
    g_spi1_poll_done_pending = false;
}

void spi1_poll_tick() {
    using namespace iris_common::stm32f0;
    if (!g_spi1_poll_busy) return;

    if (!g_spi1_poll_inflight && (SPI1->SR & SPI_SR_TXE)) {
        const std::uint8_t out = g_spi1_poll_tx ? g_spi1_poll_tx[g_spi1_poll_idx] : 0xFF;
        *reinterpret_cast<volatile std::uint8_t*>(&SPI1->DR) = out;
        g_spi1_poll_inflight = true;
    }

    if (g_spi1_poll_inflight && (SPI1->SR & SPI_SR_RXNE)) {
        const std::uint8_t in = *reinterpret_cast<volatile std::uint8_t*>(&SPI1->DR);
        if (g_spi1_poll_rx) {
            g_spi1_poll_rx[g_spi1_poll_idx] = in;
        }
        g_spi1_poll_idx++;
        g_spi1_poll_inflight = false;
        if (g_spi1_poll_idx >= g_spi1_poll_len) {
            // Wait for SPI not busy before deasserting CS.
            g_spi1_poll_done_pending = true;
        }
    }

    if (g_spi1_poll_done_pending && !(SPI1->SR & stm32f0::SPI_SR_BSY)) {
        g_spi1_poll_done_pending = false;
        g_spi1_poll_busy = false;
        // Deassert CS (active low) on PA4.
        stm32f0::GPIOA->BSRR = (1u << 4);
    }
}

void spi1_dma_start_tx(const std::uint8_t* tx, std::uint16_t len) {
    using namespace iris_common::stm32f0;
    if (!tx || len == 0) return;

    g_spi1_dma_busy = true;
    g_spi1_dma_tx_done = false;
    g_spi1_dma_rx_done = false;

    // Disable channels before config.
    DMA1->CH[1].CCR &= ~DMA_CCR_EN; // CH2 (RX)
    DMA1->CH[2].CCR &= ~DMA_CCR_EN; // CH3 (TX)

    // RX channel: sink to dummy byte (avoid overrun).
    DMA1->CH[1].CPAR = reinterpret_cast<std::uint32_t>(&SPI1->DR);
    DMA1->CH[1].CMAR = reinterpret_cast<std::uint32_t>(&g_spi1_dummy_rx);
    DMA1->CH[1].CNDTR = len;
    DMA1->CH[1].CCR = DMA_CCR_TCIE | DMA_CCR_PSIZE_8 | DMA_CCR_MSIZE_8; // MINC off

    // TX channel
    DMA1->CH[2].CPAR = reinterpret_cast<std::uint32_t>(&SPI1->DR);
    DMA1->CH[2].CMAR = reinterpret_cast<std::uint32_t>(tx);
    DMA1->CH[2].CNDTR = len;
    DMA1->CH[2].CCR = DMA_CCR_TCIE | DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_8 | DMA_CCR_MSIZE_8;

    // Enable SPI DMA
    SPI1->CR2 |= (SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN);

    // Enable channels
    DMA1->CH[1].CCR |= DMA_CCR_EN;
    DMA1->CH[2].CCR |= DMA_CCR_EN;
}

void spi1_dma_start_txrx(const std::uint8_t* tx, std::uint8_t* rx, std::uint16_t len) {
    using namespace iris_common::stm32f0;
    if (!rx || len == 0) return;

    g_spi1_dma_busy = true;
    g_spi1_dma_tx_done = false;
    g_spi1_dma_rx_done = false;

    const std::uint8_t* tx_buf = tx ? tx : &g_spi1_dummy_tx;
    const bool tx_minc = (tx != nullptr);

    // Disable channels before config.
    DMA1->CH[1].CCR &= ~DMA_CCR_EN; // CH2 (RX)
    DMA1->CH[2].CCR &= ~DMA_CCR_EN; // CH3 (TX)

    // RX channel
    DMA1->CH[1].CPAR = reinterpret_cast<std::uint32_t>(&SPI1->DR);
    DMA1->CH[1].CMAR = reinterpret_cast<std::uint32_t>(rx);
    DMA1->CH[1].CNDTR = len;
    DMA1->CH[1].CCR = DMA_CCR_TCIE | DMA_CCR_MINC | DMA_CCR_PSIZE_8 | DMA_CCR_MSIZE_8;

    // TX channel
    DMA1->CH[2].CPAR = reinterpret_cast<std::uint32_t>(&SPI1->DR);
    DMA1->CH[2].CMAR = reinterpret_cast<std::uint32_t>(tx_buf);
    DMA1->CH[2].CNDTR = len;
    DMA1->CH[2].CCR = DMA_CCR_TCIE | DMA_CCR_DIR | (tx_minc ? DMA_CCR_MINC : 0u) | DMA_CCR_PSIZE_8 | DMA_CCR_MSIZE_8;

    // Enable SPI DMA
    SPI1->CR2 |= (SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN);

    // Enable channels
    DMA1->CH[1].CCR |= DMA_CCR_EN;
    DMA1->CH[2].CCR |= DMA_CCR_EN;
}

void spi1_dma_start_rx(std::uint8_t* rx, std::uint16_t len) {
    using namespace iris_common::stm32f0;
    if (!rx || len == 0) return;

    g_spi1_dma_busy = true;
    g_spi1_dma_tx_done = false;
    g_spi1_dma_rx_done = false;

    // Disable channels before config.
    DMA1->CH[1].CCR &= ~DMA_CCR_EN; // CH2 (RX)
    DMA1->CH[2].CCR &= ~DMA_CCR_EN; // CH3 (TX)

    // RX channel
    DMA1->CH[1].CPAR = reinterpret_cast<std::uint32_t>(&SPI1->DR);
    DMA1->CH[1].CMAR = reinterpret_cast<std::uint32_t>(rx);
    DMA1->CH[1].CNDTR = len;
    DMA1->CH[1].CCR = DMA_CCR_TCIE | DMA_CCR_MINC | DMA_CCR_PSIZE_8 | DMA_CCR_MSIZE_8;

    // TX channel uses constant 0xFF, MINC off.
    DMA1->CH[2].CPAR = reinterpret_cast<std::uint32_t>(&SPI1->DR);
    DMA1->CH[2].CMAR = reinterpret_cast<std::uint32_t>(&g_spi1_dummy_tx);
    DMA1->CH[2].CNDTR = len;
    DMA1->CH[2].CCR = DMA_CCR_TCIE | DMA_CCR_DIR | DMA_CCR_PSIZE_8 | DMA_CCR_MSIZE_8;

    // Enable SPI DMA
    SPI1->CR2 |= (SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN);

    // Enable channels
    DMA1->CH[1].CCR |= DMA_CCR_EN;
    DMA1->CH[2].CCR |= DMA_CCR_EN;
}

void on_spi1_dma_irq() {
    using namespace iris_common::stm32f0;

    // Check channel 2 (RX) TC flag and clear.
    if (DMA1->ISR & (1u << 5)) { // TCIF2
        DMA1->IFCR = (1u << 5);
        g_spi1_dma_rx_done = true;
    }

    // Check channel 3 (TX) TC flag and clear.
    if (DMA1->ISR & (1u << 9)) { // TCIF3
        DMA1->IFCR = (1u << 9);
        g_spi1_dma_tx_done = true;
    }

    if (g_spi1_dma_tx_done && g_spi1_dma_rx_done) {
        g_spi1_dma_busy = false;
        g_spi1_dma_tx_done = false;
        g_spi1_dma_rx_done = false;
    }
}

} // namespace iris_common::hal
