#pragma once

#include <cstdint>

namespace iris_common::stm32f0 {

struct GPIO {
    volatile std::uint32_t MODER;
    volatile std::uint32_t OTYPER;
    volatile std::uint32_t OSPEEDR;
    volatile std::uint32_t PUPDR;
    volatile std::uint32_t IDR;
    volatile std::uint32_t ODR;
    volatile std::uint32_t BSRR;
    volatile std::uint32_t LCKR;
    volatile std::uint32_t AFRL;
    volatile std::uint32_t AFRH;
};

struct RCC {
    volatile std::uint32_t CR;
    volatile std::uint32_t CFGR;
    volatile std::uint32_t CIR;
    volatile std::uint32_t APB2RSTR;
    volatile std::uint32_t APB1RSTR;
    volatile std::uint32_t AHBENR;
    volatile std::uint32_t APB2ENR;
    volatile std::uint32_t APB1ENR;
    volatile std::uint32_t BDCR;
    volatile std::uint32_t CSR;
    volatile std::uint32_t AHBRSTR;
    volatile std::uint32_t CFGR2;
    volatile std::uint32_t CFGR3;
    volatile std::uint32_t CR2;
};

struct SYSCFG {
    volatile std::uint32_t CFGR1;
    volatile std::uint32_t CFGR2;
    volatile std::uint32_t EXTICR1;
    volatile std::uint32_t EXTICR2;
    volatile std::uint32_t EXTICR3;
    volatile std::uint32_t EXTICR4;
};

struct EXTI {
    volatile std::uint32_t IMR;
    volatile std::uint32_t EMR;
    volatile std::uint32_t RTSR;
    volatile std::uint32_t FTSR;
    volatile std::uint32_t SWIER;
    volatile std::uint32_t PR;
};

struct SPI {
    volatile std::uint32_t CR1;
    volatile std::uint32_t CR2;
    volatile std::uint32_t SR;
    volatile std::uint32_t DR;
    volatile std::uint32_t CRCPR;
    volatile std::uint32_t RXCRCR;
    volatile std::uint32_t TXCRCR;
    volatile std::uint32_t I2SCFGR;
    volatile std::uint32_t I2SPR;
};

struct DMAChannel {
    volatile std::uint32_t CCR;
    volatile std::uint32_t CNDTR;
    volatile std::uint32_t CPAR;
    volatile std::uint32_t CMAR;
};

struct DMA {
    volatile std::uint32_t ISR;
    volatile std::uint32_t IFCR;
    DMAChannel CH[7];
};

struct ADC {
    volatile std::uint32_t ISR;
    volatile std::uint32_t IER;
    volatile std::uint32_t CR;
    volatile std::uint32_t CFGR1;
    volatile std::uint32_t CFGR2;
    volatile std::uint32_t SMPR;
    volatile std::uint32_t RESERVED0;
    volatile std::uint32_t RESERVED1;
    volatile std::uint32_t TR;
    volatile std::uint32_t RESERVED2;
    volatile std::uint32_t CHSELR;
    volatile std::uint32_t RESERVED3[5];
    volatile std::uint32_t DR;
};

struct ADC_Common {
    volatile std::uint32_t CSR;
    volatile std::uint32_t CCR;
    volatile std::uint32_t CDR;
};

struct TIM {
    volatile std::uint32_t CR1;
    volatile std::uint32_t CR2;
    volatile std::uint32_t SMCR;
    volatile std::uint32_t DIER;
    volatile std::uint32_t SR;
    volatile std::uint32_t EGR;
    volatile std::uint32_t CCMR1;
    volatile std::uint32_t CCMR2;
    volatile std::uint32_t CCER;
    volatile std::uint32_t CNT;
    volatile std::uint32_t PSC;
    volatile std::uint32_t ARR;
};

struct USART {
    volatile std::uint32_t CR1;
    volatile std::uint32_t CR2;
    volatile std::uint32_t CR3;
    volatile std::uint32_t BRR;
    volatile std::uint32_t GTPR;
    volatile std::uint32_t RTOR;
    volatile std::uint32_t RQR;
    volatile std::uint32_t ISR;
    volatile std::uint32_t ICR;
    volatile std::uint32_t RDR;
    volatile std::uint32_t TDR;
};

struct SysTickRegs {
    volatile std::uint32_t CTRL;
    volatile std::uint32_t LOAD;
    volatile std::uint32_t VAL;
    volatile std::uint32_t CALIB;
};

constexpr std::uintptr_t GPIOA_BASE = 0x48000000u;
constexpr std::uintptr_t GPIOB_BASE = 0x48000400u;
constexpr std::uintptr_t GPIOC_BASE = 0x48000800u;
constexpr std::uintptr_t RCC_BASE = 0x40021000u;
constexpr std::uintptr_t SYSCFG_BASE = 0x40010000u;
constexpr std::uintptr_t EXTI_BASE = 0x40010400u;
constexpr std::uintptr_t SPI1_BASE = 0x40013000u;
constexpr std::uintptr_t ADC_BASE = 0x40012400u;
constexpr std::uintptr_t ADC_COMMON_BASE = ADC_BASE + 0x308u;
constexpr std::uintptr_t DMA1_BASE = 0x40020000u;
constexpr std::uintptr_t TIM14_BASE = 0x40002000u; // APBPERIPH_BASE + 0x2000
constexpr std::uintptr_t USART1_BASE = 0x40013800u;
constexpr std::uintptr_t SYSTICK_BASE = 0xE000E010u;
constexpr std::uintptr_t NVIC_ISER_BASE = 0xE000E100u;
constexpr std::uintptr_t NVIC_ICER_BASE = 0xE000E180u;

inline GPIO* const GPIOA = reinterpret_cast<GPIO*>(GPIOA_BASE);
inline GPIO* const GPIOB = reinterpret_cast<GPIO*>(GPIOB_BASE);
inline GPIO* const GPIOC = reinterpret_cast<GPIO*>(GPIOC_BASE);
inline RCC* const RCCREG = reinterpret_cast<RCC*>(RCC_BASE);
inline SYSCFG* const SYSCFGREG = reinterpret_cast<SYSCFG*>(SYSCFG_BASE);
inline EXTI* const EXTIREG = reinterpret_cast<EXTI*>(EXTI_BASE);
inline SPI* const SPI1 = reinterpret_cast<SPI*>(SPI1_BASE);
inline ADC* const ADC1 = reinterpret_cast<ADC*>(ADC_BASE);
inline ADC_Common* const ADC_COMMON = reinterpret_cast<ADC_Common*>(ADC_COMMON_BASE);
inline DMA* const DMA1 = reinterpret_cast<DMA*>(DMA1_BASE);
inline TIM* const TIM14 = reinterpret_cast<TIM*>(TIM14_BASE);
inline USART* const USART1 = reinterpret_cast<USART*>(USART1_BASE);
inline SysTickRegs* const SYSTICK = reinterpret_cast<SysTickRegs*>(SYSTICK_BASE);

// RCC bits
constexpr std::uint32_t RCC_AHBENR_GPIOAEN = (1u << 17);
constexpr std::uint32_t RCC_AHBENR_GPIOBEN = (1u << 18);
constexpr std::uint32_t RCC_AHBENR_GPIOCEN = (1u << 19);
constexpr std::uint32_t RCC_AHBENR_DMA1EN = (1u << 0);
constexpr std::uint32_t RCC_APB2ENR_SYSCFGEN = (1u << 0);
constexpr std::uint32_t RCC_APB2ENR_SPI1EN = (1u << 12);
constexpr std::uint32_t RCC_APB2ENR_ADC1EN = (1u << 9);
constexpr std::uint32_t RCC_APB2ENR_USART1EN = (1u << 14);
constexpr std::uint32_t RCC_APB1ENR_TIM14EN = (1u << 8); // TIM14 clock enable

// RCC CR bits
constexpr std::uint32_t RCC_CR_HSION = (1u << 0);
constexpr std::uint32_t RCC_CR_HSIRDY = (1u << 1);
constexpr std::uint32_t RCC_CR_HSEON = (1u << 16);
constexpr std::uint32_t RCC_CR_HSERDY = (1u << 17);
constexpr std::uint32_t RCC_CR_PLLON = (1u << 24);
constexpr std::uint32_t RCC_CR_PLLRDY = (1u << 25);

// RCC CR2 bits (ADC clock: HSI14)
constexpr std::uint32_t RCC_CR2_HSI14ON = (1u << 0);
constexpr std::uint32_t RCC_CR2_HSI14RDY = (1u << 1);
constexpr std::uint32_t RCC_CR2_HSI14DIS = (1u << 2);

// RCC CFGR bits
constexpr std::uint32_t RCC_CFGR_SW_Pos = 0;
constexpr std::uint32_t RCC_CFGR_SWS_Pos = 2;
constexpr std::uint32_t RCC_CFGR_SW_PLL = (0x2u << RCC_CFGR_SW_Pos);
constexpr std::uint32_t RCC_CFGR_SWS_PLL = (0x2u << RCC_CFGR_SWS_Pos);
constexpr std::uint32_t RCC_CFGR_PLLSRC_HSE_PREDIV = (1u << 16);
constexpr std::uint32_t RCC_CFGR_PLLMUL_Pos = 18;

// RCC CFGR2 bits (PREDIV)
constexpr std::uint32_t RCC_CFGR2_PREDIV_Pos = 0;

// SysTick CTRL bits
constexpr std::uint32_t SYSTICK_CTRL_ENABLE = (1u << 0);
constexpr std::uint32_t SYSTICK_CTRL_TICKINT = (1u << 1);
constexpr std::uint32_t SYSTICK_CTRL_CLKSOURCE = (1u << 2);

// EXTI lines
constexpr std::uint32_t EXTI_LINE_2 = (1u << 2);
constexpr std::uint32_t EXTI_LINE_8 = (1u << 8);

// SPI CR1 bits
constexpr std::uint32_t SPI_CR1_CPHA = (1u << 0);
constexpr std::uint32_t SPI_CR1_CPOL = (1u << 1);
constexpr std::uint32_t SPI_CR1_MSTR = (1u << 2);
constexpr std::uint32_t SPI_CR1_BR_Pos = 3;
constexpr std::uint32_t SPI_CR1_SPE = (1u << 6);
constexpr std::uint32_t SPI_CR1_LSBFIRST = (1u << 7);
constexpr std::uint32_t SPI_CR1_SSI = (1u << 8);
constexpr std::uint32_t SPI_CR1_SSM = (1u << 9);

// SPI CR2 bits
constexpr std::uint32_t SPI_CR2_SSOE = (1u << 2);
constexpr std::uint32_t SPI_CR2_RXDMAEN = (1u << 0);
constexpr std::uint32_t SPI_CR2_TXDMAEN = (1u << 1);
constexpr std::uint32_t SPI_CR2_FRXTH = (1u << 12);
constexpr std::uint32_t SPI_CR2_DS_Pos = 8;
constexpr std::uint32_t SPI_CR2_DS_8BIT = (0x7u << SPI_CR2_DS_Pos);

// SPI SR bits
constexpr std::uint32_t SPI_SR_RXNE = (1u << 0);
constexpr std::uint32_t SPI_SR_TXE = (1u << 1);
constexpr std::uint32_t SPI_SR_BSY = (1u << 7);

// DMA CCR bits
constexpr std::uint32_t DMA_CCR_EN = (1u << 0);
constexpr std::uint32_t DMA_CCR_TCIE = (1u << 1);
constexpr std::uint32_t DMA_CCR_DIR = (1u << 4);
constexpr std::uint32_t DMA_CCR_CIRC = (1u << 5);
constexpr std::uint32_t DMA_CCR_MINC = (1u << 7);
constexpr std::uint32_t DMA_CCR_PSIZE_8 = (0u << 8);
constexpr std::uint32_t DMA_CCR_MSIZE_8 = (0u << 10);
constexpr std::uint32_t DMA_CCR_PSIZE_16 = (1u << 8);
constexpr std::uint32_t DMA_CCR_MSIZE_16 = (1u << 10);

// ADC bits (F0)
constexpr std::uint32_t ADC_CR_ADEN = (1u << 0);
constexpr std::uint32_t ADC_CR_ADSTART = (1u << 2);
constexpr std::uint32_t ADC_CR_ADCAL = (1u << 31);
constexpr std::uint32_t ADC_ISR_ADRDY = (1u << 0);

// ADC CFGR1 bits
constexpr std::uint32_t ADC_CFGR1_DMAEN = (1u << 0);
constexpr std::uint32_t ADC_CFGR1_DMACFG = (1u << 1);
constexpr std::uint32_t ADC_CFGR1_CONT = (1u << 13);

// ADC SMPR bits (single field)
constexpr std::uint32_t ADC_SMPR_239_5 = 0x7u;

// TIM bits
constexpr std::uint32_t TIM_CR1_CEN = (1u << 0);
constexpr std::uint32_t TIM_DIER_UIE = (1u << 0);
constexpr std::uint32_t TIM_SR_UIF = (1u << 0);
constexpr std::uint32_t TIM_EGR_UG = (1u << 0);

// USART bits (STM32F0)
constexpr std::uint32_t USART_CR1_UE = (1u << 0);
constexpr std::uint32_t USART_CR1_RE = (1u << 2);
constexpr std::uint32_t USART_CR1_TE = (1u << 3);
constexpr std::uint32_t USART_CR1_RXNEIE = (1u << 5);

constexpr std::uint32_t USART_ISR_FE = (1u << 1);
constexpr std::uint32_t USART_ISR_NE = (1u << 2);
constexpr std::uint32_t USART_ISR_ORE = (1u << 3);
constexpr std::uint32_t USART_ISR_RXNE = (1u << 5);

constexpr std::uint32_t USART_ICR_FECF = (1u << 1);
constexpr std::uint32_t USART_ICR_NCF = (1u << 2);
constexpr std::uint32_t USART_ICR_ORECF = (1u << 3);

// IRQ numbers (STM32F030x8)
constexpr int EXTI2_3_IRQn = 6;
constexpr int EXTI4_15_IRQn = 7;
constexpr int DMA1_Channel2_3_IRQn = 10;
constexpr int TIM14_IRQn = 19;
constexpr int USART1_IRQn = 27;

inline void nvic_enable_irq(int irqn) {
    volatile std::uint32_t* iser = reinterpret_cast<volatile std::uint32_t*>(NVIC_ISER_BASE);
    iser[0] = (1u << static_cast<std::uint32_t>(irqn));
}

inline void nvic_disable_irq(int irqn) {
    volatile std::uint32_t* icer = reinterpret_cast<volatile std::uint32_t*>(NVIC_ICER_BASE);
    icer[0] = (1u << static_cast<std::uint32_t>(irqn));
}

} // namespace iris_common::stm32f0
