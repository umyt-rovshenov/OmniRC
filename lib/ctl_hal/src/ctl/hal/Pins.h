#pragma once

/// Pin assignments for the OmniRC transmitter.
///
/// This header is the single source of truth. docs/HARDWARE.md explains why
/// each pin was chosen and docs/WIRING.md says what connects to it; neither
/// may disagree with this file.
///
/// Three constraints shaped the layout and must be respected by any change:
///
///  - GPIO26 to GPIO37 are unavailable. They carry the quad SPI flash and, on
///    the N16R8 part, the octal SPI PSRAM.
///  - Analog inputs must sit on ADC1 (GPIO1 to GPIO10). ADC2 shares hardware
///    with the radio and returns garbage whenever Wi-Fi is active.
///  - Anything that has to wake the chip from deep sleep must be an RTC pin,
///    which means GPIO0 to GPIO21.
namespace ctl {
namespace hal {
namespace pins {

// --- Analog inputs, all on ADC1 -------------------------------------------
// GPIO3 is deliberately skipped: it is a strapping pin, and an analog voltage
// sitting at mid-rail during boot makes its state ambiguous.
constexpr int kStickLeftX = 1;    ///< ADC1_CH0
constexpr int kStickLeftY = 2;    ///< ADC1_CH1
constexpr int kStickRightX = 4;   ///< ADC1_CH3
constexpr int kStickRightY = 5;   ///< ADC1_CH4
constexpr int kBatterySense = 6;  ///< ADC1_CH5, behind a 100k/100k divider

// --- Power button ----------------------------------------------------------
// Must stay within GPIO0..21 so it can serve as a deep sleep wake source.
constexpr int kPowerButton = 7;

// --- Display, SPI2 ---------------------------------------------------------
// The panel is write-only, so no MISO line is wired.
constexpr int kTftRst = 8;
constexpr int kTftDc = 9;
constexpr int kTftCs = 10;
constexpr int kTftMosi = 11;
constexpr int kTftSclk = 12;
constexpr int kTftBacklight = 13;

// --- nRF24L01+PA+LNA, SPI3 -------------------------------------------------
// A separate bus from the display, so a 40 MHz redraw cannot stall the
// control loop.
constexpr int kNrfCe = 14;
constexpr int kNrfCsn = 15;
constexpr int kNrfSck = 16;
constexpr int kNrfMosi = 17;
constexpr int kNrfMiso = 18;
constexpr int kNrfIrq = 21;

// --- I2C and system --------------------------------------------------------
constexpr int kI2cSda = 38;
constexpr int kI2cScl = 39;
constexpr int kExpanderInt = 40;  ///< MCP23017 INTA, mirrored to cover both ports
constexpr int kBuzzer = 41;
constexpr int kStatusLed = 48;  ///< Addressable LED on the board

// --- Reserved --------------------------------------------------------------
// GPIO19 and GPIO20 are the native USB peripheral, used by the USB HID
// transport. GPIO43 and GPIO44 are UART0, which carries the serial log.
// GPIO42 and GPIO47 are free for expansion.

}  // namespace pins
}  // namespace hal
}  // namespace ctl
