# Testing and CI

How the driver is checked: host unit tests against a mock transport, compiled documentation, ESP32 builds and
static analysis, on every pull request.

## Host unit tests

The tests build the real driver and the CEVA SH-2 library against a mock transport that records every control-pin
change and delay and serves queued bytes to `Read()`. Time is virtual, so tests check the exact reset and
bootloader sequences.

```bash
git submodule update --init src/sh2
cmake -S . -B build-host -DHF_BNO08X_BUILD_TESTS=ON
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

| File | Covers |
| --- | --- |
| [`tests/test_rvc.cpp`](../tests/test_rvc.cpp) | UART-RVC parsing: the datasheet's example frame, resynchronisation, checksum rejection, open and close |
| [`tests/test_driver.cpp`](../tests/test_driver.cpp) | Interface and state guards, error codes, `HardwareReset()` and `EnterBootloader()` pin sequences, DFU image checks |
| [`tests/test_dfu_i2c.cpp`](../tests/test_dfu_i2c.cpp) | DFU over I2C at the bootloader address for both SA0 levels, restore on success and failure, address errors |
| [`tests/test_memory_firmware.cpp`](../tests/test_memory_firmware.cpp) | In-memory DFU image checks from a second source file (the `MemoryFirmware` adapter links from more than one) |
| [`tests/test_gpio_status.cpp`](../tests/test_gpio_status.cpp) | GPIO errors from `GpioSet()` reaching the pin calls and the bootloader helpers; a legacy `void GpioSet()` port |
| [`tests/mock_bno08x.hpp`](../tests/mock_bno08x.hpp) | The mock transport; `GpioSet()` can fail per pin |

The tests do not model the SH-2 protocol, so a successful `Begin()` and report decoding are covered on hardware
only.

## Documentation snippets

Every C++ block on this site is included from [`tests/docs/`](../tests/docs/my_i2c.hpp), which the host build
compiles (target `hf_bno08x_doc_snippets`) with the driver instantiated over the documented ports. A change to
the API that breaks a page breaks the build first.

## CI workflows

| Workflow | What it checks |
| --- | --- |
| Host unit tests | GCC and Clang build and run the tests and the doc snippets |
| ESP32 examples build | The CI-enabled apps in `examples/esp32/app_config.yml`, for each listed ESP-IDF version and build type |
| C++ lint, C++ analysis | clang-format, clang-tidy and cppcheck |
| Docs | Strict site build, figure and generated-table drift, Doxygen, link check, Lighthouse budget |
| Markdown lint, YAML lint, link check | Repository text files |

The workflows call the shared, versioned workflows of
[hf-general-ci-tools](https://github.com/hardfoc/hf-general-ci-tools) and
[hf-espidf-ci-tools](https://github.com/hardfoc/hf-espidf-ci-tools).

## On hardware

The [ESP32 examples](examples.md) are the hardware test suite. `driver_integration_test` runs the API checks and
prints a pass / fail summary over the serial monitor; `dfu_workflow_test` covers the firmware-update helpers.
