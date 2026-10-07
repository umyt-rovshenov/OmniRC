# Wiring

Every connection in the transmitter, module by module. This is the document to follow while
building; [`HARDWARE.md`](HARDWARE.md) explains *why* the pins were chosen this way.

> **Work through [Pre-flight checks](#pre-flight-checks) before connecting anything.** Two of
> them prevent permanent damage.

## Bill of materials

| # | Part | Qty |
|---|---|---|
| 1 | ESP32-S3-WROOM-1 N16R8 development board, with IPEX antenna connector | 1 |
| 2 | nRF24L01+PA+LNA module with SMA antenna | 1 |
| 3 | nRF24 adapter board with AMS1117-3.3 regulator | 1 |
| 4 | 1.69" ST7789 TFT, 240x280, 8-pin SPI | 1 |
| 5 | MCP23017 I2C expander module | 1 |
| 6 | PS2-style thumbstick, 10K, with push button | 2 |
| 7 | 6x6x5 mm tactile switch | 8 |
| 8 | SS12D00G3 slide switch (1P2T) | 2 |
| 9 | KCD11 rocker switch, 15x10 mm, 6 A | 1 |
| 10 | 3.7 V charge and boost module, 5 V at 2 A | 1 |
| 11 | 103450 Li-ion cell, 2000 mAh | 2 |
| 12 | 2.4 GHz IPEX antenna, 3 dBi | 1 |
| 13 | Passive buzzer | 1 |
| 14 | 100 k resistor | 2 |
| 15 | 100 nF ceramic capacitor | 2 |
| 16 | 10 uF capacitor | 1 |

## Module pinouts

### ESP32-S3-WROOM-1 N16R8 board

Two USB-C ports. The UART port is used for flashing and the serial log; the native USB port is
reserved for the USB HID gamepad transport and goes to the panel.

**Do not use GPIO26–37.** They carry the flash and the octal PSRAM.

### Power module, LX-LCBST

| Pad | Connect to |
|---|---|
| `Type-C` | Charger input |
| `IN+` / `IN-` | Alternative charger input, instead of the Type-C socket. Use one or the other, not both |
| `B+` / `B-` | Battery pack positive and negative |
| `VO+` / `VO-` | 5 V output. `VO+` goes through the power switch to the 5 V rail, `VO-` to ground |
| trimmer | **Output voltage adjustment — see pre-flight check 1** |

The board exposes two `VO+`/`VO-` pairs. They are the same net, wired in parallel, so either
pair can be used.

The module charges linearly, which means the difference between the 5 V input and the cell
voltage is dissipated as heat on an 18 x 23.6 mm board. It getting warm during a charge is
expected behaviour, and the chip carries over-temperature protection.

Charge current is set by one 0603 resistor, following `R (kilohms) = 1200 / I (mA)`:

| Resistor | Charge current |
|---|---|
| 30 k | 50 mA |
| 5 k | 250 mA |
| 2 k | 580 mA |
| 1.2 k | 1000 mA |

The manufacturer recommends 0.37 C, which for the 4000 mAh pack is about 1.5 A. The module's
1 A maximum is below that, so the stock resistor needs no change and the pack charges in roughly
five hours.

The module provides soft start, reverse battery protection and over-temperature protection. It
does **not** provide over-discharge protection, which is why the firmware enforces its own
cut-off.

### nRF24 adapter board

The adapter carries an AMS1117-3.3 regulator, so it is fed from **5 V, not 3.3 V**: that
regulator needs more than about 4.4 V at its input and cannot work from a cell.

| Adapter pin | ESP32 GPIO |
|---|---|
| `VCC` | 5 V rail |
| `GND` | Ground |
| `CE` | 14 |
| `CSN` | 15 |
| `SCK` | 16 |
| `MOSI` | 17 |
| `MISO` | 18 |
| `IRQ` | 21 |

Solder a 10 uF capacitor and a 100 nF capacitor across the adapter's 3.3 V output, as physically
close to the radio's supply pins as possible. An unstable supply here is the single most common
cause of an nRF24 link that works on the bench and drops out in use.

### Display, ST7789 1.69"

The module's `SDA` pad is SPI MOSI and its `SCL` pad is SPI clock. There is no MISO.

| Display pin | ESP32 GPIO |
|---|---|
| `GND` | Ground |
| `VCC` | 3.3 V |
| `SCL` | 12 |
| `SDA` | 11 |
| `RES` | 8 |
| `DC` | 9 |
| `CS` | 10 |
| `BLK` | 13 |

### MCP23017 expander

| Module pin | Connect to |
|---|---|
| `VCC` | 3.3 V |
| `GND` | Ground |
| `SDA` | GPIO38 |
| `SCL` | GPIO39 |
| `ITA` | GPIO40 |
| `RESET` | Nothing. The module carries a 10 k pull-up to VCC |
| `A0`, `A1`, `A2` | Ground (I2C address 0x20) |
| `ITB`, `NC/SO`, `NC/CS` | Leave unconnected |

All sixteen I/O pins use the chip's internal pull-ups, so every button and switch simply
connects its pin to ground. No external resistors are needed.

The HW-839 module already carries everything the chip needs around it: 10 k pull-ups on `SDA`
and `SCL`, a 10 k pull-up on `RESET` so the chip is never held in reset, 10 k pull-downs on
`A0`–`A2` that fix the address at 0x20, and a 1 uF decoupling capacitor. Nothing external has to
be added for the expander to answer on the bus.

| Expander pin | Control |
|---|---|
| `A0` | Up |
| `A1` | Down |
| `A2` | Left |
| `A3` | Right |
| `A4` | OK |
| `A5` | Back |
| `A6` | F1, assignable in a profile |
| `A7` | F2, assignable in a profile |
| `B0` | Left stick push button |
| `B1` | Right stick push button |
| `B2` | Slide switch 1 |
| `B3` | Slide switch 2 |
| `B4`–`B7` | Free, expansion header |

**Tactile switches:** one leg to the expander pin, the other leg to ground. A 6x6 switch's two
legs on the same side are internally connected, so take the two legs from *opposite* sides.

**Slide switches:** centre pin to the expander input, one end pin to ground, the other end pin
unconnected. One position reads low, the other reads high through the internal pull-up.

### Thumbsticks

Supplied from the board's 3.3 V output, the same rail the ADC references. Powering them from
anywhere else would make the measured centre position drift as the battery discharges.

| Stick pin | Left stick | Right stick |
|---|---|---|
| `GND` | Ground | Ground |
| `+5V` | 3.3 V | 3.3 V |
| `VRx` | GPIO1 | GPIO4 |
| `VRy` | GPIO2 | GPIO5 |
| `SW` | MCP23017 `B0` | MCP23017 `B1` |

The pad is labelled `+5V` on most of these modules, but 3.3 V is what it must receive here.

### Buzzer

A piezo passive buzzer connects directly between GPIO41 and ground. A magnetic buzzer draws
30–50 mA, which is more than a GPIO should source: drive it through an NPN transistor such as an
S8050, with a 1 k resistor from GPIO41 to the base, the emitter to ground and the buzzer between
the collector and 3.3 V.

### Battery measurement

```
  B+ ──[100k]──┬──► GPIO6
               ├──[100k]──► GND
               └──[100nF]──► GND
```

This divider stays connected to the pack even when the transmitter is switched off. It draws
about 21 uA, which is a few mAh per month out of 4000 mAh. If the transmitter is going into
storage for several months, disconnect the pack.

## Battery pack: two cells in parallel

Two 2000 mAh cells wired **in parallel** make one 4000 mAh, 3.7 V pack. Runtime roughly doubles,
to about 13 hours on the nRF24 link.

> **Parallel, never in series.** Two cells in series produce 7.4 V. The charge module, the boost
> converter and every protection threshold in this design assume a single cell. Series wiring
> will destroy the electronics and can damage the cells.

```
  Cell A  +red ──┬────────► power module B+
  Cell B  +red ──┘

  Cell A  -black ──┬──────► power module B-
  Cell B  -black ──┘
```

### Balancing the cells before connecting them

Two cells joined in parallel immediately equalise their voltage. If they are not already close,
that equalisation happens as a very large unlimited current from the fuller cell into the
emptier one, which can damage both cells and melt the link between them.

1. Charge each cell separately, or let each rest until it is stable.
2. Measure both with a multimeter.
3. **The difference must be below 0.05 V.** Below 0.02 V is better. If it is larger, charge the
   lower cell on its own until they match.
4. Only then join them: positive to positive, negative to negative, with short leads.

Once joined they stay balanced on their own, because they are electrically one pack. They only
need to be separated again if one is replaced.

### Charging

A 4000 mAh pack changes the charge rate arithmetic: 2 A is 0.5 C, which is a normal and safe
rate. The caution that applied to a single cell no longer does, and the pack charges in roughly
two and a half hours.

## Power wiring

```
  Cell A ─┐
          ├──► power module  B+ / B-
  Cell B ─┘

  power module Type-C ◄── panel cutout, charging

  power module  +  ──► power switch ──► 5 V rail
  power module  -  ──────────────────► ground rail

  5 V rail ──► ESP32-S3 board "5V" pin
           ──► nRF24 adapter VCC

  ESP32 board 3V3 ──► display VCC
                  ──► MCP23017 VCC
                  ──► both thumbstick supply pins

  B+ ──► battery measurement divider ──► GPIO6
```

**The power switch sits on the module's 5 V output**, not on the battery. The pack stays
connected to the charger, so the transmitter charges whether it is on or off, and the module's
own standby draw is negligible.

**That switch must be rated for at least 2 A.** Peak draw approaches 1 A when the radio
transmits while the screen redraws. An SS12D00G3 is rated for about 0.5 A and will overheat and
degrade in this position; keep those two for the user-facing switches, where they only carry
microamps into the expander.

A KCD11 rocker is a good fit: two positions, generous contacts and a 6 A rating. A three
position toggle also works, wired with its centre pin to the module's `+` and only one of its
end pins to the 5 V rail, so one direction is on and the other two positions are off.

## Pre-flight checks

### 1. Set the output to 5.0 V before connecting anything

The power module's output voltage is set by a trimmer potentiometer, and its adjustment range is
**4.2 V to 28 V**. Anything above 5 V on this rail destroys the ESP32 board, the display and the
expander at once.

1. Connect the battery pack to `B+` / `B-`, with nothing on the output.
2. Measure between `+` and `-` with a multimeter.
3. Turn the trimmer until it reads 5.00 V, slowly; these trimmers are coarse.
4. Switch the pack off and on and confirm it still reads 5.00 V.
5. Once it reads 5.00 V, seal the trimmer so it cannot be knocked out of adjustment.

Only after it reads 5.00 V may anything else be connected.

### 2. External antenna before the radio ever transmits

If the ESP32 module is a WROOM-1**U**, it has no on-board antenna and the IPEX connector is the
only one. Transmitting without an antenna attached can damage the radio. Fit both antennas, the
IPEX one on the ESP32 and the SMA one on the nRF24, before powering up.

### 3. Board header reference

The ESP32 board is a YD-ESP32-S3 layout. Its headers, in physical order:

```
left   3V3  3V3  RST  4  5  6  7  15  16  17  18  8  3  46  9  10  11  12  13  14  5Vin  GND
right  GND  TX  RX  1  2  42  41  40  39  38  37  36  35  0  45  48  47  21  20  19  GND  GND
```

`5Vin` reaches the board's 5 V rail through diode D26, so the power module's output and USB
power cannot back-feed each other and no series diode is needed.

## Build order

Build in stages and test after each one. Finding a mistake with three wires connected is a
different experience from finding it with forty.

1. Set the power module output to 5.00 V and seal the trimmer. Verify it under load with a
   resistor if possible.
2. Balance and join the cells. Verify 5.00 V on the rail again.
3. Power only the ESP32 board. Flash the firmware and confirm the boot banner, including the
   PSRAM line.
4. Add the display. Confirm it lights and draws.
5. Add the MCP23017 and one button. Confirm the chip answers at 0x20.
6. Add the remaining buttons, the switches and both thumbsticks.
7. Add the nRF24 adapter with its capacitors, and the antennas.
8. Add the buzzer and the battery divider.
