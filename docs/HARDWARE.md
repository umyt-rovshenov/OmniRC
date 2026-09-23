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

```
  Li-ion 103450, 2000 mAh
      + ──► power module  BAT+
      - ──► power module  BAT-

  5 V / 2 A charge and boost module
      Type-C  ◄── charging input, use a 1 A limited source
      K pad   ──► momentary switch ──► OUT-        (power button)
      K pad   ──► ESP32 GPIO7                       (firmware sees the press)
      OUT+    ──► 5 V rail
      OUT-    ──► ground

  5 V rail ──► ESP32-S3 board "5V" pin
  5 V rail ──► nRF24 adapter board input (AMS1117-3.3) ──► nRF24 module
                   └─ 10 uF and 100 nF across the adapter's 3.3 V output,
                      as close to the radio's pins as possible

  ESP32 board 3V3 ──► display VCC
                  ──► MCP23017 VCC
                  ──► both thumbstick potentiometer supply pins

  BAT+ ──[100k]──┬──► GPIO6
                 ├──[100k]──► ground
                 └──[100nF]──► ground
```

**The sticks are supplied from the same 3.3 V rail as the ADC reference.** If they were fed from
anywhere else, the measured centre position would drift as the battery discharged.

**The radio has its own regulator.** The ESP32-S3 draws up to 350 mA in bursts; adding the
nRF24's 120 mA transmit peak on top of that is more than the board's own regulator can hold up,
and an unstable supply on an nRF24 shows up as random link dropouts. The adapter board needs
5 V in, because its AMS1117 cannot regulate from a 3.7 V cell.

### Budget

| State | Draw at 5 V | Estimated runtime |
|---|---|---|
| nRF24 link active, normal brightness | ~210 mA | ~6.5 h |
| Wi-Fi service mode | ~360 mA | ~3.8 h |
| Peak (ESP32 transmit + radio PA + display) | ~750 mA | module supplies 2 A |

Based on 2000 mAh at 3.7 V and 92.5% boost efficiency.

### Rules the power module imposes

1. **Never draw less than 80 mA while powered on.** The module shuts its output down when the
   load stays under 50 mA, so a conventional low-power idle state would look like a random
   shutdown. The display dims but never blanks, and the CPU does not drop below 160 MHz.
2. **Clean shutdown comes for free.** Selecting power off writes state to NVS, shuts down the
   display and radio, and enters deep sleep. The draw falls below the threshold and the module
   removes power. An EXT0 wake source on GPIO7 means a second press during that window brings
   the device back instead.
3. **Charge from a 1 A limited source.** The module will draw up to 2.4 A, which is 1.2 C for a
   2000 mAh cell and will shorten its life.
4. **Leave the charge voltage pad at 4.2 V.** The cell is a 4.2 V type; selecting 4.35 V will
   swell it.

## Checks before assembly

Verify these with a multimeter before soldering anything permanent:

- [ ] Charge terminates at 4.20 ±0.02 V at the battery terminals.
- [ ] Measure how long the power module takes to cut its output under a light load, and record
      it.
- [ ] Is there a diode between the ESP32 board's USB 5 V and its `5V` pin? Without one, the
      module's output and USB power fight each other when the HID port is in use; add a series
      SS34 on the module output if it is missing.
- [ ] Does the battery have a protection board under its tape? Firmware cut-off is in place
      either way, but a cell without protection deserves more caution.
- [ ] The nRF24 adapter outputs 3.3 V from a 5 V input.
- [ ] Is the module a WROOM-1 or a WROOM-1U? A 1U has no on-board antenna, and transmitting
      without one attached can damage it.
- [ ] Record the voltage each stick reads at its mechanical extremes on a 3.3 V supply.

## Enclosure

4 mm ABS cannot be bent cleanly, so the case is a layered stack: a base plate, spacer frames and
a front panel joined with M3 heat-set inserts.

Antennas go in opposite corners, ideally perpendicular to one another. The single-active-radio
rule already prevents the transmitters from fighting, but physical separation still improves
receive sensitivity. Keep the radio and its coaxial lead away from the display's SPI lines and
from the boost converter's inductor.
