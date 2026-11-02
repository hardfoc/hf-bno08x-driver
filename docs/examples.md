# Examples

Complete ESP32-S3 applications that exercise the driver on a BNO08x breakout, one use case each.

They run on an ESP32-S3 DevKit wired to the sensor over I2C, except `spi_polling` (SPI), `uart_shtp_polling`
(UART-SHTP) and `rvc_mode` (UART-RVC); the pins are in
[`esp32_bno08x_test_config.hpp`](../examples/esp32/main/esp32_bno08x_test_config.hpp) and in each port's
configuration. The ports are [`esp32_bno08x_bus.hpp`](../examples/esp32/main/esp32_bno08x_bus.hpp) (I2C),
[`esp32_bno08x_spi_bus.hpp`](../examples/esp32/main/esp32_bno08x_spi_bus.hpp) (SPI),
[`esp32_bno08x_uart_bus.hpp`](../examples/esp32/main/esp32_bno08x_uart_bus.hpp) (UART-SHTP) and
[`esp32_uart_rvc_bus.hpp`](../examples/esp32/main/esp32_uart_rvc_bus.hpp) (UART-RVC). CI builds the apps marked
"yes" on each pull request.

## The apps

<!-- generated: examples -->
| App | What it shows | Source | CI |
| --- | --- | --- | --- |
| `driver_integration_test` :material-star:{ title="featured" } | Comprehensive driver integration test suite (no hardware required) | [`driver_integration_test.cpp`](../examples/esp32/main/driver_integration_test.cpp) | yes |
| `basic_polling` :material-star:{ title="featured" } | Basic polling mode example with orientation and linear acceleration | [`basic_polling_example.cpp`](../examples/esp32/main/basic_polling_example.cpp) | yes |
| `event_driven_callback` :material-star:{ title="featured" } | Event-driven callback example with step counter and tap detector | [`event_driven_callback_example.cpp`](../examples/esp32/main/event_driven_callback_example.cpp) | yes |
| `full_features` :material-star:{ title="featured" } | Comprehensive example demonstrating all BNO08x features | [`full_features_example.cpp`](../examples/esp32/main/full_features_example.cpp) | yes |
| `dfu_update` :material-star:{ title="featured" } | Device Firmware Update (DFU) example demonstrating BNO08x firmware update API | [`dfu_example.cpp`](../examples/esp32/main/dfu_example.cpp) | yes |
| `dfu_workflow_test` :material-star:{ title="featured" } | Comprehensive DFU workflow integration test (state + helpers + in-memory DFU) | [`dfu_workflow_test.cpp`](../examples/esp32/main/dfu_workflow_test.cpp) | yes |
| `rvc_mode` :material-star:{ title="featured" } | UART-RVC mode: heading, pitch, roll and acceleration streamed at 100 Hz | [`rvc_mode_example.cpp`](../examples/esp32/main/rvc_mode_example.cpp) | yes |
| `spi_polling` :material-star:{ title="featured" } | SH-2 over SPI: rotation vector and gyroscope, polled (SPI reference port) | [`spi_polling_example.cpp`](../examples/esp32/main/spi_polling_example.cpp) | yes |
| `uart_shtp_polling` :material-star:{ title="featured" } | SH-2 over UART (UART-SHTP): rotation vector and linear acceleration, polled | [`uart_shtp_polling_example.cpp`](../examples/esp32/main/uart_shtp_polling_example.cpp) | yes |

Build and flash one (from `examples/esp32`, ESP-IDF environment loaded):

```bash
./scripts/build_app.sh driver_integration_test Release
./scripts/flash_app.sh flash_monitor driver_integration_test Release
```

Source of this table: [`examples/esp32/app_config.yml`](../examples/esp32/app_config.yml).
<!-- /generated -->

## Set up once

```bash
git submodule update --init src/sh2 examples/esp32/scripts   # the SH-2 library and the HardFOC ESP-IDF scripts
cd examples/esp32
. $IDF_PATH/export.sh                                        # ESP-IDF v5.5
./scripts/build_app.sh list                                  # every app and build type
```

The scripts read [`app_config.yml`](../examples/esp32/app_config.yml) (the same file this page's table comes
from) and build each app as its own firmware image.

## What to read in an example

| You want to | Read |
| --- | --- |
| Bring up I2C: reset, probe both addresses, `Begin()`, poll orientation | `basic_polling_example.cpp` |
| React to steps and taps with a callback | `event_driven_callback_example.cpp` |
| Run many reports at once, callbacks and polling together | `full_features_example.cpp` |
| Run SH-2 over SPI (mode 3, WAKE, H_INTN) | `spi_polling_example.cpp`, `esp32_bno08x_spi_bus.hpp` |
| Run SH-2 over UART (SHTP framing at 3 Mbit/s) | `uart_shtp_polling_example.cpp`, `esp32_bno08x_uart_bus.hpp` |
| Stream heading over UART-RVC | `rvc_mode_example.cpp` ([UART-RVC mode](rvc.md)) |
| Update the hub's firmware | `dfu_example.cpp`, `dfu_workflow_test.cpp` ([Firmware update](dfu.md)) |
| See every API call checked, including error paths | `driver_integration_test.cpp` |

The [example folder's README](../examples/esp32/README.md) has the wiring and the expected serial output.
