#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

extern void __libc_init_array(void);
int main(void);

void Default_Handler(void) { while (1) {} }

void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void EXTI2_3_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void EXTI4_15_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void DMA1_Channel2_3_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void TIM14_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void USART1_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));

void _init(void) {}
void _fini(void) {}

static inline void enable_irq(void) {
    __asm volatile ("cpsie i" ::: "memory");
}

void Reset_Handler(void) {
    uint8_t* src = (uint8_t*)&_sidata;
    uint8_t* dst = (uint8_t*)&_sdata;
    while (dst < (uint8_t*)&_edata) { *dst++ = *src++; }
    dst = (uint8_t*)&_sbss;
    while (dst < (uint8_t*)&_ebss) { *dst++ = 0; }

    __libc_init_array();
    enable_irq();
    main();
    while (1) {}
}

__attribute__((section(".isr_vector")))
void (*const g_vectors[])(void) = {
    (void (*)(void))(&_estack),
    Reset_Handler,
    Default_Handler, // NMI
    HardFault_Handler,
    Default_Handler, // Reserved
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler, // SVC
    Default_Handler, // Reserved
    Default_Handler, // Reserved
    Default_Handler, // PendSV
    SysTick_Handler,
    // External IRQs
    Default_Handler, // WWDG
    Default_Handler, // PVD
    Default_Handler, // RTC
    Default_Handler, // FLASH
    Default_Handler, // RCC
    Default_Handler, // EXTI0_1
    EXTI2_3_IRQHandler,
    EXTI4_15_IRQHandler,
    Default_Handler, // TS
    Default_Handler, // DMA1_CH1
    DMA1_Channel2_3_IRQHandler,
    Default_Handler, // DMA1_CH4_5
    Default_Handler, // ADC1
    Default_Handler, // TIM1_BRK_UP_TRG_COM
    Default_Handler, // TIM1_CC
    Default_Handler, // TIM2
    Default_Handler, // TIM3
    Default_Handler, // TIM6
    Default_Handler, // TIM7
    TIM14_IRQHandler,
    Default_Handler, // TIM15
    Default_Handler, // TIM16
    Default_Handler, // TIM17
    Default_Handler, // I2C1
    Default_Handler, // I2C2
    Default_Handler, // SPI1
    Default_Handler, // SPI2
    USART1_IRQHandler,
    Default_Handler, // USART2
    Default_Handler, // USART3_4
    Default_Handler, // CEC_CAN
    Default_Handler, // USB
};
