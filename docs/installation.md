# Installation

Add the driver to a CMake or ESP-IDF project. It is a header-only C++20 driver over CEVA's SH-2 library, which is
compiled C.

## Requirements

- A C++20 compiler (GCC 10+, Clang 12+ or a current MSVC) and a C99 compiler for the SH-2 library.
- The CEVA SH-2 library in the [`src/sh2`](https://github.com/ceva-dsp/sh2) submodule (Apache-2.0).
- No RTOS and no exceptions. The driver uses `std::function` for callbacks, which may allocate when you store a
  capturing lambda that does not fit its small buffer.

## Get the source

A git submodule keeps the driver pinned to a version you chose. Initialise `src/sh2` too: the build needs it.

```bash
git submodule add https://github.com/hardfoc/hf-bno08x-driver.git external/hf-bno08x-driver
git submodule update --init external/hf-bno08x-driver/src/sh2
```

Only `inc/`, `src/`, `cmake/` and `CMakeLists.txt` are needed to build. The examples have their own submodule
(`examples/esp32/scripts`); you do not need it to use the driver.

## CMake

```cmake
add_subdirectory(external/hf-bno08x-driver)
target_link_libraries(my_app PRIVATE hf_bno08x)   # or the alias hf::bno08x
```

The `hf_bno08x` target is a static library of the SH-2 sources and the DFU firmware stub. It carries the include
directories (`inc/`, `src/`, `src/dfu/`, `src/sh2/` and the generated version header) and requires C++20
(`cxx_std_20`), so your target does not need to set either.

| Option | Default | Effect |
| --- | --- | --- |
| `HF_BNO08X_ENABLE_WARNINGS` | `OFF` | `-Wall -Wextra -Wpedantic` on the library sources |
| `HF_BNO08X_BUILD_TESTS` | `OFF` | Builds the host unit tests in `tests/` ([Testing and CI](testing.md)) |

Use the driver from source with `add_subdirectory()`. The install rules copy only `inc/`, while `bno08x.hpp`
includes `src/bno08x.ipp` and the SH-2 headers, so an installed package does not compile yet.

## ESP-IDF

The ESP-IDF component wrapper is
[`examples/esp32/components/hf_bno08x/`](../examples/esp32/components/hf_bno08x/CMakeLists.txt). Copy that folder
into your project's `components/` (or point `EXTRA_COMPONENT_DIRS` at it) and set the `HF_BNO08X_ROOT` cache
variable to the driver checkout; the wrapper stops with a clear message when the path is wrong. Your component
then declares `REQUIRES hf_bno08x`. The wrapper compiles the same sources and requires the `driver` and
`freertos` components.

## How the build is organised

| Layer | File | Role |
| --- | --- | --- |
| Settings | [`cmake/hf_bno08x_build_settings.cmake`](../cmake/hf_bno08x_build_settings.cmake) | Sources, include directories, version: the single source of truth |
| Library | [`CMakeLists.txt`](../CMakeLists.txt) | Static library for host builds, install rules and CI |
| ESP-IDF | [`examples/esp32/components/hf_bno08x/CMakeLists.txt`](../examples/esp32/components/hf_bno08x/CMakeLists.txt) | IDF component wrapper over the same settings |

The version is set once in the settings file and written into `bno08x_version.h` at configure time. Read it at
run time:

```cpp
const char* v = GetBNO08xDriverVersion();   // "1.0.0"; also BNO085<MyI2c>::GetDriverVersion()
```

## Firmware image for updates

`src/dfu/firmware-bno.c` defines the image `Dfu()` uses by default. It is a 64-byte stub that the driver refuses
(an image must be at least 1024 bytes). For firmware updates, pass a vendor image at run time with
`DfuFromMemory()`, or replace this file with the vendor's. See [Firmware update](dfu.md).

## Next

[Quickstart](quickstart.md) wires the sensor and reads orientation; [Porting](porting.md) writes the communication
interface for your MCU.
