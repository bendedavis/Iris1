#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

int main(void);
void EXTI4_15_IRQHandler(void);

void Default_Handler(void) { while (1) {} }

void Reset_Handler(void) {
    uint8_t* src = (uint8_t*)&_sidata;
    uint8_t* dst = (uint8_t*)&_sdata;
    while (dst < (uint8_t*)&_edata) { *dst++ = *src++; }
    dst = (uint8_t*)&_sbss;
    while (dst < (uint8_t*)&_ebss) { *dst++ = 0; }
    main();
    while (1) {}
}

__attribute__((section(".isr_vector")))
void (*const g_vectors[])(void) = {
    (void (*)(void))(&_estack),
    Reset_Handler,
    Default_Handler, // NMI
    Default_Handler, // HardFault
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
    Default_Handler, // SysTick
    // External IRQs (unused in bootloader)
    Default_Handler, // WWDG
    Default_Handler, // PVD
    Default_Handler, // RTC
    Default_Handler, // FLASH
    Default_Handler, // RCC
    Default_Handler, // EXTI0_1
    Default_Handler, // EXTI2_3
    EXTI4_15_IRQHandler, // EXTI4_15
    Default_Handler, // TS
    Default_Handler, // DMA1_CH1
    Default_Handler, // DMA1_CH2_3
    Default_Handler, // DMA1_CH4_5
    Default_Handler, // ADC1
    Default_Handler, // TIM1_BRK_UP_TRG_COM
    Default_Handler, // TIM1_CC
    Default_Handler, // TIM2
    Default_Handler, // TIM3
    Default_Handler, // TIM6
    Default_Handler, // TIM7
    Default_Handler, // TIM14
    Default_Handler, // TIM15
    Default_Handler, // TIM16
    Default_Handler, // TIM17
    Default_Handler, // I2C1
    Default_Handler, // I2C2
    Default_Handler, // SPI1
    Default_Handler, // SPI2
    Default_Handler, // USART1
    Default_Handler, // USART2
    Default_Handler, // USART3_4
    Default_Handler, // CEC_CAN
    Default_Handler, // USB
};
