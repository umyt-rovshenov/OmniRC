# Hardware

Pinout, wiring and the power design decisions behind them.

## Pin constraints on the ESP32-S3-WROOM-1 N16R8

**GPIO26–37 are unavailable.** They carry the quad SPI flash and, because the R8 variant uses
octal SPI PSRAM, GPIO33–37 as well. Nothing may be connected to them.

**Use with care:** GPIO0 (boot), GPIO3 (JTAG strapping), GPIO45 and GPIO46 (strapping),
GPIO19/20 (native USB), GPIO43/44 (UART0).

**ADC:** only ADC1 (GPIO1–10) is usable. ADC2 shares hardware with the Wi-Fi radio and returns
garbage whenever Wi-Fi is active, so it is not used anywhere in this design.

**Deep sleep wake:** only GPIO0–21 are RTC capable, which is why the power button sits at GPIO7.

## Pin map

### Analog — all on ADC1

| GPIO | Signal | Notes |
|---|---|---|
| 1 | `STICK_L_X` | ADC1_CH0 |
| 2 | `STICK_L_Y` | ADC1_CH1 |
| 4 | `STICK_R_X` | ADC1_CH3 |
| 5 | `STICK_R_Y` | ADC1_CH4 |
| 6 | `VBAT_SENSE` | ADC1_CH5, 100k/100k divider with a 100 nF cap to ground |

GPIO3 is deliberately left unused: it is a strapping pin, and an analog voltage sitting at
mid-rail during boot makes its state ambiguous.

### Display — SPI2, 40 MHz

| GPIO | Signal |
|---|---|
| 8 | `TFT_RST` |
| 9 | `TFT_DC` |
| 10 | `TFT_CS` |
| 11 | `TFT_MOSI` |
| 12 | `TFT_SCLK` |
| 13 | `TFT_BL` (LEDC PWM backlight) |

The module's `SDA` pad is MOSI and its `SCL` pad is SCLK. There is no MISO; the panel is
write-only.

### nRF24L01+PA+LNA — SPI3, 8 MHz

| GPIO | Signal |
|---|---|
| 14 | `NRF_CE` |
| 15 | `NRF_CSN` |
| 16 | `NRF_SCK` |
| 17 | `NRF_MOSI` |
| 18 | `NRF_MISO` |
| 21 | `NRF_IRQ` |

The radio gets its own SPI bus so that a 40 MHz screen redraw cannot stall a 100 Hz control loop.

### I2C and system

| GPIO | Signal | Notes |
|---|---|---|
| 38 | `I2C_SDA` | MCP23017, 400 kHz |
| 39 | `I2C_SCL` | MCP23017 |
| 40 | `MCP_INT` | MCP23017 INTA, interrupt driven reads |
| 41 | `BUZZER` | LEDC PWM |
| 7 | `PWR_BTN_SENSE` | RTC pin, EXT0 wake, tied to the power module's `K` pad |
| 48 | `STATUS_LED` | On-board addressable LED |
| 19, 20 | Native USB | Panel mounted USB-C, used by the USB HID transport |
| 43, 44 | UART0 | Flashing and serial log over the other USB-C port |
| 42, 47 | free | Expansion header |

22 pins used, 3 GPIO plus 4 expander pins free.

## MCP23017

I2C address `0x20` (A0, A1 and A2 tied to ground). Every input uses the chip's internal 100 k
pull-up via the `GPPU` register, with buttons switching to ground, so no external resistors are
needed. The chip runs in mirrored interrupt mode: INTA alone reports activity on both ports.

| Pin | Signal | Pin | Signal |
|---|---|---|---|
| A0 | `BTN_UP` | B0 | `BTN_LEFT_STICK` |
| A1 | `BTN_DOWN` | B1 | `BTN_RIGHT_STICK` |
| A2 | `BTN_LEFT` | B2 | `SWITCH_1` |
| A3 | `BTN_RIGHT` | B3 | `SWITCH_2` |
| A4 | `BTN_OK` | B4–B7 | free, expansion |
| A5 | `BTN_BACK` | | |
| A6 | `BTN_F1` | | |
| A7 | `BTN_F2` | | |

## Power

Connection-by-connection wiring is in [`WIRING.md`](WIRING.md). This section records why the
supply is arranged the way it is.

**The pack is two 103450 cells in parallel**, 4000 mAh at 3.7 V. Parallel, never series: every
threshold in this design, and the charge module itself, assumes a single cell.

A 4000 mAh pack also settles the charge rate question. The module's 2 A is 0.5 C here, a normal
rate, where the same current into a single 2000 mAh cell would have been 1.2 C and would have
shortened its life.

**The sticks are supplied from the same 3.3 V rail as the ADC reference.** If they were fed from
anywhere else, the measured centre position would drift as the battery discharged.

**The radio has its own regulator.** The ESP32-S3 draws up to 350 mA in bursts; adding the
nRF24's 120 mA transmit peak on top of that is more than the board's own regulator can hold up,
and an unstable supply on an nRF24 shows up as random link dropouts. The adapter board is fed
from 5 V, because its AMS1117 cannot regulate from a cell.

**The power switch is on the module's 5 V output, not on the battery.** The pack stays connected
to the charger, so the transmitter charges whether it is on or off. That switch carries the full
system current and must be rated for at least 2 A.

### Budget

| State | Draw at 5 V | Estimated runtime |
|---|---|---|
| nRF24 link active, normal brightness | ~210 mA | ~13 h |
| Wi-Fi service mode | ~360 mA | ~7.5 h |
| Peak (ESP32 transmit + radio PA + display) | ~750 mA | module supplies 2 A |

Based on 4000 mAh at 3.7 V and roughly 90% boost efficiency.

### Battery protection

Protection is enforced in firmware regardless of what the pack or the module provide: warn at
3.5 V, force a clean shutdown at 3.3 V. The pack is measured through a divider on `B+`, which
stays live even when the transmitter is switched off.

## Enclosure

4 mm ABS cannot be bent cleanly, so the case is a layered stack: a base plate, spacer frames and
a front panel joined with M3 heat-set inserts.

Antennas go in opposite corners, ideally perpendicular to one another. The single-active-radio
rule already prevents the transmitters from fighting, but physical separation still improves
receive sensitivity. Keep the radio and its coaxial lead away from the display's SPI lines and
from the boost converter's inductor.
