# Changelog

All notable changes to this driver are listed here. Versions follow
[Semantic Versioning](https://semver.org/). Entries marked **BREAKING** need source changes in
code that used the affected API; together they call for a major version bump at release.

## Unreleased

The build settings carry version 1.0.0; no release has been tagged yet.

### Breaking

- `Dfu()` has no default image any more: it defaulted to the compiled-in `firmware` object from
  `src/dfu/firmware-bno.c`, a 64-byte "Dummy Firmware Image" placeholder. Callers pass the image
  (`Dfu(image)`, `DfuWithOptions()`, `DfuFromMemory()`); `imu.Dfu()` no longer compiles. **BREAKING**.
- Every DFU call refuses the placeholder and any copy of it (new `IsPlaceholderFirmware()`), an `HcBin_t` with a
  missing function (it was called through the null pointer), an image shorter than 1024 bytes (empty included),
  a wrong `FW-Format` and an unknown part number with `SH2_ERR_BAD_PARAM`. These checks now run before the driver
  changes anything: a refused image leaves a running SH-2 session open (it used to be closed first) and sends
  nothing on the bus.

### Added

- SPI and UART-SHTP reference ports for ESP-IDF, `examples/esp32/main/esp32_bno08x_spi_bus.hpp` (mode 3 at
  3 MHz, header-first reads in one chip select, PS0/WAKE handling) and `esp32_bno08x_uart_bus.hpp` (3 Mbit/s,
  buffer status query, 100 µs host byte spacing), with the example apps `spi_polling` and `uart_shtp_polling`.
- `inc/bno08x_shtp_uart.hpp`: portable UART-SHTP framing (flags, protocol IDs, byte stuffing, buffer status) for
  any UART port, with host tests.
- Every ESP32 example app is now built by CI (`ci_enabled: true` in `app_config.yml`).
- Host unit tests (`-DHF_BNO08X_BUILD_TESTS=ON`) against a mock transport: UART-RVC parsing with the
  datasheet's example frame, interface and state guards, reset and bootloader pin sequences, DFU image checks.
  A CI workflow runs them with GCC and Clang.

### Fixed

- `SelectInterface()` drove PS1/PS0 swapped for the two UART modes: `UART` selected UART-RVC and `UARTRVC`
  selected UART-SHTP. It now follows datasheet figure 1-5 (PS1/PS0 = 01 UART-RVC, 10 UART); I2C and SPI were
  already right. **BREAKING** for boards that called it with the swapped value to work around the bug.
- `HardwareReset()` waited 50 ms after releasing NRST, shorter than the hub's start-up time. It now waits
  `RESET_STARTUP_MS` = 94 ms: t1 (internal initialization, at least 90 ms) plus t2 (internal configuration,
  about 4 ms), datasheet section 6.5.3, figure 6-8. `EnterBootloader()` and `ExitBootloaderAndReboot()` use it
  too, so each takes 44 ms longer.
- A program with more than one source file calling `DfuFromMemory()` / `RunDfuFromMemory()` failed to link with
  Apple clang (duplicate thread-local wrapper symbols for `MemoryFirmware`'s state). The adapter keeps its
  per-thread state in function-local statics now.
- Firmware update over I2C never reached the bootloader: the driver kept talking to the application address
  (`0x4A` / `0x4B`) while the bootloader answers at `0x28` / `0x29` (datasheet section 1.4). I2C ports can now
  provide the optional `GetI2cAddress()` / `SetI2cAddress()` hooks; `Dfu()`, `DfuWithOptions()`,
  `DfuFromMemory()` and `RunDfuFromMemory()` switch the port to `BootloaderI2cAddress()` of its address (same SA0
  bit) for the transfer and restore it on every path. A failed switch returns `SH2_ERR_IO`; an address that is not
  a BNO08x address returns `SH2_ERR_BAD_PARAM`. Ports without the hooks behave as before. The ESP32 I2C port
  and the documented port implement them.
- A second `BNO085` object's `Begin()` silently reset the SH-2 session of the first: CEVA's SH-2 library keeps one
  global session, and `sh2_open()` clears it before it fails. `Begin()` now refuses while another object's session
  is open, with the new error `BNO08X_ERR_SH2_SESSION_IN_USE` (-100), and touches neither the library nor the bus;
  `Close()` and the destructor free the session, and `Sh2SessionInUse()` reports it. UART-RVC and DFU are not
  affected.
- `cmake --install` copied only `inc/`, while `bno08x.hpp` includes `bno08x.ipp`, `dfu/HcBin.h`, the SH-2
  headers and (through the `.ipp`) `dfu/MemoryFirmware.hpp`, so an installed package did not compile. The install
  now ships all of them, and the header includes `bno08x.ipp` through the include path instead of
  `../src/bno08x.ipp`. The installed target is exported as the documented `hf::bno08x` (was `hf::hf_bno08x`,
  kept as an alias by the package config). A new ctest, `hf_bno08x_install_consumer`, builds a `find_package()`
  consumer against a fresh install.

### Changed

- The port's `GpioSet()` now returns `bool` (`true` = pin driven or not wired, `false` = GPIO error), and the
  driver passes it on: `HardwareReset()`, `SetBootPin()`, `SetWakePin()` and `SelectInterface()` return `bool`
  instead of `void`, and `EnterBootloader()` / `ExitBootloaderAndReboot()` fail with `SH2_ERR_IO` and close the
  port. `HardwareReset()` stops at a failed RSTN write instead of waiting. Ports with a `void` `GpioSet()` still
  compile (detected at compile time) and are treated as never failing; any other return type is a compile error,
  so an SDK status code is not read as a bool. Calls that ignore the result compile as before; code that takes
  the address of one of the four member functions needs the new type. The ESP32 example ports return `false` on
  an ESP-IDF error.

- Documentation moved to the HardFOC MkDocs platform: new page set (quickstart, hardware, architecture, API
  guide, porting, UART-RVC mode, firmware update, testing), code snippets compiled by the host build from
  `tests/docs/`, Doxygen under `/api/`, versioned with mike. The Jekyll site files and the doxygen-awesome
  submodule are gone.
