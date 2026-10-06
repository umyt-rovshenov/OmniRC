# Controllar

A profile driven, multi-protocol universal RC transmitter built around an ESP32-S3.

Most hobby transmitters are welded to one radio and one vehicle. Controllar is built the other
way round: the hardware is fixed, and everything about *what* it controls lives in a profile you
can edit on the device itself. Switching from a rover to a quadruped to a USB gamepad is a menu
selection, not a firmware rebuild.

> **Status: early development.** The protocol library and host test suite are in place. Drivers,
> user interface and radio transports are being built phase by phase — see the roadmap below.

## What makes it different

**Profiles, not firmware.** A profile names a transport, its radio settings, a failsafe policy,
and a mapping from each physical control to a numbered channel. Adding a new vehicle means
writing a JSON profile, not editing C++.

```json
{
  "id": "rover-01",
  "name": "4WD Rover",
  "transport": { "type": "nrf24", "channel": 108, "address": "RVR01", "rate": "1Mbps" },
  "rateHz": 50,
  "failsafe": { "timeoutMs": 500, "action": "neutral" },
  "channels": [
    { "id": 0, "name": "Steering",  "source": "axis.rightX", "min": -500, "max": 500, "deadzone": 40, "expo": 0.30 },
    { "id": 1, "name": "Throttle",  "source": "axis.leftY",  "min": 0,    "max": 1000, "deadzone": 60, "invert": true },
    { "id": 2, "name": "Cam Servo", "source": "axis.rightY", "min": 0,    "max": 180 },
    { "id": 3, "name": "Arm",       "source": "switch.sw1",  "type": "bool" }
  ]
}
```

On the vehicle, that profile is just numbered channels:

```cpp
steering.write(rx.channel(0));
throttle.set(rx.channel(1));
camera.write(rx.channel(2));
```

**One radio at a time, on purpose.** An nRF24L01+PA+LNA transmitting at +20 dBm will desensitise
an ESP32's own 2.4 GHz receiver sitting centimetres away. Rather than pretend otherwise,
Controllar activates exactly one transport and fully shuts down the others' stacks. A transmitter
talks to one vehicle at a time anyway.

**Failsafe is not optional.** Every frame carries a sequence number, and the receiver library
applies a configured safe state when frames stop arriving. This is part of the protocol, not
something each project reimplements.

## Planned transports

| Transport | Use |
|---|---|
| nRF24L01+PA+LNA | Long range control with telemetry in the ACK payload |
| ESP-NOW | Low latency control between ESP32 devices, no access point needed |
| Wi-Fi | Service mode: configuration web interface and over-the-air updates |
| BLE HID | Acts as a standard gamepad for phones, tablets and single board computers |
| USB HID | Acts as a standard gamepad over the ESP32-S3's native USB |

## Hardware

| Part | Role |
|---|---|
| ESP32-S3-WROOM-1 N16R8 | Dual core MCU, 16 MB flash, 8 MB octal PSRAM, native USB |
| nRF24L01+PA+LNA with SMA antenna | Long range control link |
| nRF24 adapter board (AMS1117-3.3) | Clean, dedicated 3.3 V supply for the radio |
| 1.69" ST7789 TFT, 240x280, SPI | On-device configuration UI |
| 2x thumbstick (10K, with push button) | 4 analog axes, 2 buttons |
| MCP23017 | I2C expander for 8 buttons, 2 stick buttons and 2 slide switches |
| 5 V / 2 A charge and boost module | Battery charging, load sharing and the power button |
| 2x 103450 2000 mAh Li-ion, in parallel | 4000 mAh, ~13 hours on the nRF24 link |

Build instructions, every connection and the pre-flight checks: [`docs/WIRING.md`](docs/WIRING.md).
Pin choices and the reasoning behind them: [`docs/HARDWARE.md`](docs/HARDWARE.md).

## Repository layout

```
src/                      transmitter firmware entry point
lib/                      transmitter libraries (core, hal, input, ui, transports, web)
shared/ControllarProtocol portable wire protocol, shared with receivers
receiver/                 receiver library and example vehicle firmware
test/                     host unit tests, no hardware required
docs/                     hardware, protocol and profile documentation
```

Anything that does not touch hardware is built and tested on the host, so the protocol,
calibration and channel mapping logic can be developed and debugged without a board attached.

## Building

```bash
pio test -e native       # host unit tests
pio run -e transmitter   # build the firmware
pio run -e transmitter -t upload
```

See [`docs/BUILDING.md`](docs/BUILDING.md) for toolchain setup and formatting rules.

## Roadmap

- [x] **P0** Project skeleton, protocol library, host test suite, CI
- [ ] **P1** Hardware abstraction: display, expander, ADC, battery, power button
- [ ] **P2** Input pipeline: calibration, filtering, channel mapping
- [ ] **P3** nRF24 transport, receiver library, first real link
- [ ] **P4** LVGL user interface and profile storage
- [ ] **P5** ESP-NOW transport and telemetry
- [ ] **P6** USB HID and BLE HID
- [ ] **P7** Configuration web interface and OTA updates
- [ ] **P8** Receiver examples, documentation, enclosure drawings
- [ ] **P9** Hardening: failsafe testing, soak testing, power measurements

## License

MIT. See [`LICENSE`](LICENSE).
