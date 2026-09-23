# Building

## Requirements

- [PlatformIO Core](https://platformio.org/install/cli) 6.2 or newer, or the PlatformIO IDE
  extension for VS Code
- Python 3.9 or newer
- A C++17 host compiler for the unit tests (Clang or GCC; both ship with the usual developer
  tools on macOS and Linux)

The ESP32 toolchain is downloaded automatically on the first firmware build.

## Environments

| Environment | Target | Purpose |
|---|---|---|
| `transmitter` | ESP32-S3-WROOM-1 N16R8 | The firmware |
| `native` | Host machine | Unit tests for everything that does not touch hardware |

## Unit tests

```bash
pio test -e native
```

These run on your computer, with no board attached. The protocol, sequence tracking, calibration
curves and channel mapping all live behind this gate, so most development can happen before the
hardware even arrives.

## Firmware

```bash
pio run -e transmitter                 # build
pio run -e transmitter -t upload       # build and flash
pio device monitor                     # serial log, 115200 baud
```

Logging goes to UART0 on GPIO43/44, which is the board's UART USB-C port. The other port is the
ESP32-S3's native USB peripheral and is reserved for the USB HID gamepad transport, so the two
never fight over the same interface.

### Board configuration

The N16R8 module pairs quad SPI flash with octal SPI PSRAM, which needs an explicit memory type:

```ini
board_build.arduino.memory_type = qio_opi
board_build.psram_type = opi
board_upload.flash_size = 16MB
```

If the boot banner reports no PSRAM, these flags do not match your board.

> **Before powering a board whose module is a WROOM-1U**, attach the external antenna.
> Transmitting without one can damage the radio.

## Formatting

Formatting is enforced in CI with a pinned `clang-format`, so local checkouts and CI agree
exactly:

```bash
pip install clang-format==23.1.1
find src lib shared test -type f \( -name '*.cpp' -o -name '*.h' \) -print0 \
  | xargs -0 clang-format -i
```

## Continuous integration

Every push and pull request runs three jobs: host unit tests, a firmware build, and a formatting
check. All three must pass before a change is merged.
