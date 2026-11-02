# Installation

This guide covers how to obtain and integrate the BNO08x driver library into your project.

## Prerequisites

Before installing the driver, ensure you have:

- **C++11 Compiler**: GCC 4.8+, Clang 3.3+, or MSVC 2015+
- **Build System**: Make, CMake, or ESP-IDF (depending on your platform)
- **Platform SDK**: ESP-IDF, STM32 HAL, Arduino, or your platform's I²C/SPI/UART driver

## Obtaining the Source

### Option 1: Git Clone (with submodules)

```bash
git clone --recurse-submodules https://github.com/n3b3x/hf-bno08x-driver.git
cd hf-bno08x-driver
```

**Important**: The `--recurse-submodules` flag is required because the driver includes the CEVA SH-2 library as a git submodule.

### Option 2: Copy Files

Copy the following layout into your project:

```
inc/
  ├── bno08x.hpp
  └── bno08x_comm_interface.hpp
src/
  ├── bno08x.cpp              (driver implementation; includes RVC and DFU logic)
  ├── sh2/                    (CEVA SH-2 library submodule — required)
  └── dfu/                    (firmware stubs and MemoryFirmware.hpp for DFU)
```

RVC and DFU are built into the driver; no separate RVC or DFU modules are required.

## Integration Methods

### Using CMake

Add the driver as a subdirectory in your `CMakeLists.txt`:

```cmake
add_subdirectory(external/hf-bno08x-driver)
target_link_libraries(your_target PRIVATE hf_bno08x)
target_include_directories(your_target PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/hf-bno08x-driver/inc
)
```

### Using ESP-IDF Component

The driver can be used as an ESP-IDF component. Add it to your `components` directory:

```cmake
# In your main CMakeLists.txt
idf_component_register(
    SRCS "your_code.cpp"
    INCLUDE_DIRS "."
    REQUIRES hf_bno08x
)
```

### Manual Integration

1. Copy the driver files to your project
2. Add the `inc/` directory to your include path
3. Include the header:
   ```cpp
   #include "bno08x.hpp"
   ```
4. Compile with C++11 support:
   ```bash
   g++ -std=c++11 -I inc/ your_code.cpp src/bno08x.cpp
   ```

## Verification

To verify the installation:

1. Include the header in a test file:
   ```cpp
   #include "bno08x.hpp"
   ```

2. Compile a simple test:
   ```bash
   g++ -std=c++11 -I inc/ -c src/bno08x.cpp -o test.o
   ```

3. If compilation succeeds, the library is properly installed.

## Next Steps

- Follow the [Quick Start](quickstart.md) guide to create your first application
- Review [Hardware Setup](hardware_setup.md) for wiring instructions
- Check [Platform Integration](platform_integration.md) to implement the communication interface

---

**Navigation**
⬅️ [Back to Index](index.md) | [Next: Quick Start ➡️](quickstart.md)

