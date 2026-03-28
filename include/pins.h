#pragma once

// Iris 1 v2 pin map (STM32F030R8T6)

// Notes
// - PA13/PA14 reserved for SWD
// - BOOT0 pulled down (always boot from flash)
// - NRST pulled up, not available on programmer

namespace iris1::pins {

// Analog
constexpr int kKnobAdcPin = 0; // PA0
constexpr float kKnobRefVolts = 3.0f;
constexpr float kAdcRefVolts = 3.3f;

// Inputs
constexpr int kSeqResetInPin = 2; // PA2 (active high, queued to next clock)
constexpr int kMuteButtonPin = 3; // PA3
constexpr int kClockInPin = 8;    // PA8
constexpr int kMidiInPin = 10;    // PA10 (USART RX)

// Outputs
constexpr int kClockRouteSelPin = 11; // PA11 (analog mux select, high routes clock)
constexpr int kMuteLedPin = 12;       // PA12

// SPI1 (EEPROM)
constexpr int kSpiNssPin = 4;  // PA4 (SPI1_NSS, hardware NSS)
constexpr int kSpiSckPin = 5;  // PA5
constexpr int kSpiMisoPin = 6; // PA6
constexpr int kSpiMosiPin = 7; // PA7

// SWD reserved
constexpr int kSwdIoPin = 13;  // PA13
constexpr int kSwdClkPin = 14; // PA14

} // namespace iris1::pins
