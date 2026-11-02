# BNO08x ESP32-S3 examples

ESP-IDF applications that run the BNO08x driver on an ESP32-S3, one use case each. The full list, with what each
app shows, is on the documentation site: [Examples](https://hardfoc.github.io/hf-bno08x-driver/latest/examples/).

## Wiring

| BNO08x | ESP32-S3 GPIO | Notes |
| --- | --- | --- |
| SDA | 4 | I2C, 400 kHz; pull-ups usually on the breakout |
| SCL | 5 | |
| H_INTN | 17 | Active low, data ready |
| NRST | 16 | Active low reset |
| VDD, VDDIO | 3.3 V | |
| GND | GND | |
| UART-RVC TX | 20 (RX) | `rvc_mode` only; PS1 low, PS0 high on the sensor |
| UART TX, RX | 20 (RX), 21 (TX) | `uart_shtp_polling` only, 3 Mbit/s; PS1 high, PS0 low; needs the BNO08x crystal |
| SPI SCLK, MOSI, MISO, CS | 12, 11, 13, 10 | `spi_polling` only, mode 3 at 3 MHz; PS1 high |
| PS0/WAKE | 18 | `spi_polling` only: high through reset (selects SPI), then the active-low wake |

The I2C examples try address 0x4B (SA0 high) first, then 0x4A. GPIO 14 toggles as a progress indicator in the
test apps. Change the pins in [`main/esp32_bno08x_test_config.hpp`](main/esp32_bno08x_test_config.hpp) or in each
example's transport configuration.

## Build and flash

```bash
git submodule update --init ../../src/sh2 scripts   # the SH-2 library and the HardFOC ESP-IDF scripts
. $IDF_PATH/export.sh                                # ESP-IDF v5.5
./scripts/build_app.sh list                          # every app and build type
./scripts/build_app.sh driver_integration_test Release
./scripts/flash_app.sh flash_monitor driver_integration_test Release
```

The apps, their source files and the ESP-IDF versions CI builds them with are in [`app_config.yml`](app_config.yml).

## Test apps

`driver_integration_test` and `dfu_workflow_test` use the small framework in [`main/TestFramework.h`](main/TestFramework.h):
`RUN_TEST()`, `RUN_TEST_IN_TASK()` and `RUN_TEST_SECTION_IF_ENABLED()`, with a pass / fail summary on the serial
monitor at the end.

## If it does not work

| Symptom | Check |
| --- | --- |
| `APP_TYPE not defined` | Build through `./scripts/build_app.sh`, or pass `-DAPP_TYPE=... -DBUILD_TYPE=...` to `idf.py` |
| `Failed to read valid app types from app_config.yml` | `git submodule update --init scripts` |
| Probe fails at both addresses | Wiring, 3.3 V supply, PS0/PS1 low on the sensor |
| `Begin()` fails after a good probe | H_INTN on GPIO 17; NRST on GPIO 16 |

More: [Troubleshooting](https://hardfoc.github.io/hf-bno08x-driver/latest/troubleshooting/).
