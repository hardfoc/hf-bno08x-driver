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

### Changed

- Documentation moved to the HardFOC MkDocs platform: new page set (quickstart, hardware, architecture, API
  guide, porting, UART-RVC mode, firmware update, testing), code snippets compiled by the host build from
  `tests/docs/`, Doxygen under `/api/`, versioned with mike. The Jekyll site files and the doxygen-awesome
  submodule are gone.
