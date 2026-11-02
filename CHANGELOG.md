# Changelog

All notable changes to this driver are listed here. Versions follow
[Semantic Versioning](https://semver.org/). Entries marked **BREAKING** need source changes in
code that used the affected API; together they call for a major version bump at release.

## Unreleased

The build settings carry version 1.0.0; no release has been tagged yet.

### Added

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

### Changed

- Documentation moved to the HardFOC MkDocs platform: new page set (quickstart, hardware, architecture, API
  guide, porting, UART-RVC mode, firmware update, testing), code snippets compiled by the host build from
  `tests/docs/`, Doxygen under `/api/`, versioned with mike. The Jekyll site files and the doxygen-awesome
  submodule are gone.
