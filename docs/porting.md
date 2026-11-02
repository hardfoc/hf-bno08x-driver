# Platform Integration Guide

This guide explains how to implement the CRTP-based communication interface for the BNO08x driver on your platform.

## Understanding CRTP (Curiously Recurring Template Pattern)

The BNO08x driver uses **CRTP** (Curiously Recurring Template Pattern) for hardware abstraction. This design choice provides several critical benefits for embedded systems:

### Why CRTP Instead of Virtual Functions?

#### 1. **Zero Runtime Overhead**
- **Virtual functions**: Require a vtable lookup (indirect call) = ~5-10 CPU cycles overhead per call
- **CRTP**: Direct function calls = 0 overhead, compiler can inline
- **Impact**: In time-critical IMU data processing, this matters significantly

#### 2. **Compile-Time Polymorphism**
- **Virtual functions**: Runtime dispatch - the compiler cannot optimize across the abstraction boundary
- **CRTP**: Compile-time dispatch - full optimization, dead code elimination, constant propagation
- **Impact**: Smaller code size, faster execution

#### 3. **Memory Efficiency**
- **Virtual functions**: Each object needs a vtable pointer (4-8 bytes)
- **CRTP**: No vtable pointer needed
- **Impact**: Critical in memory-constrained systems

#### 4. **Type Safety**
- **Virtual functions**: Runtime errors if method not implemented
- **CRTP**: Compile-time errors if method not implemented
- **Impact**: Catch bugs at compile time, not in the field

### How CRTP Works

```cpp
// Base template class (from bno08x_comm_interface.hpp)
template <typename Derived>
class CommInterface {
public:
    bool Open() {
        // Cast 'this' to Derived* and call the derived implementation
        return static_cast<Derived*>(this)->Open();
    }

    int Read(uint8_t* data, uint32_t length) {
        return static_cast<Derived*>(this)->Read(data, length);
    }

    // ... other methods
};

// Your implementation
class MyComm : public bno08x::CommInterface<MyComm> {
public:
    // This method is called directly (no virtual overhead)
    bool Open() {
        // Your platform-specific initialization code
        return true;
    }

    int Read(uint8_t* data, uint32_t length) {
        // Your platform-specific read code
        return length;
    }
};
```

## Interface Definition

The BNO08x driver requires you to implement the following interface:

```cpp
template <typename Derived>
class CommInterface {
public:
    // Required methods (implement all of these)
    BNO085Interface GetInterfaceType();  // I2C, SPI, UART, or UARTRVC — driver uses this to allow Begin(), BeginRvc(), or Dfu()
    bool Open();
    void Close();
    int Write(const uint8_t* data, uint32_t length);
    int Read(uint8_t* data, uint32_t length);
    bool DataAvailable();
    void Delay(uint32_t ms);
    uint32_t GetTimeUs();

    // Optional methods (implement if pins are wired)
    void SetReset(bool state);
    void SetBoot(bool state);
    void SetWake(bool state);
    void SetPS0(bool state);
    void SetPS1(bool state);
};
```

## Implementation Steps

### Step 1: Create Your Implementation Class

```cpp
#include "bno08x_comm_interface.hpp"

class MyPlatformComm : public bno08x::CommInterface<MyPlatformComm> {
private:
    // Your platform-specific members
    i2c_handle_t i2c_handle_;  // Example for I²C

public:
    // Constructor
    MyPlatformComm(i2c_handle_t handle) : i2c_handle_(handle) {}

    // Implement required methods
    bool Open() {
        // Your initialization code
        return true;
    }

    void Close() {
        // Your cleanup code
    }

    int Write(const uint8_t* data, uint32_t length) {
        // Your write code
        return length;
    }

    int Read(uint8_t* data, uint32_t length) {
        // Your read code
        return length;
    }

    bool DataAvailable() {
        // Check if data is available (e.g., via interrupt pin)
        return true;
    }

    void Delay(uint32_t ms) {
        // Your delay implementation
    }

    uint32_t GetTimeUs() {
        // Return current time in microseconds
        return 0;
    }
};
```

### Step 2: Platform-Specific Examples

#### ESP32 (ESP-IDF) - I²C

```cpp
#include "driver/i2c.h"
#include "bno08x_comm_interface.hpp"

class Esp32I2CComm : public bno08x::CommInterface<Esp32I2CComm> {
private:
    i2c_port_t i2c_port_;
    uint8_t device_addr_;

public:
    Esp32I2CComm(i2c_port_t port, uint8_t addr)
        : i2c_port_(port), device_addr_(addr) {}

    bool Open() {
        i2c_config_t conf = {};
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = GPIO_NUM_4;
        conf.scl_io_num = GPIO_NUM_5;
        conf.master.clk_speed = 400000;
        i2c_param_config(i2c_port_, &conf);
        i2c_driver_install(i2c_port_, conf.mode, 0, 0, 0);
        return true;
    }

    void Close() {
        i2c_driver_delete(i2c_port_);
    }

    int Write(const uint8_t* data, uint32_t length) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (device_addr_ << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write(cmd, data, length, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(i2c_port_, cmd, pdMS_TO_TICKS(1000));
        i2c_cmd_link_delete(cmd);
        return (ret == ESP_OK) ? length : -1;
    }

    int Read(uint8_t* data, uint32_t length) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (device_addr_ << 1) | I2C_MASTER_READ, true);
        if (length > 1) {
            i2c_master_read(cmd, data, length - 1, I2C_MASTER_ACK);
        }
        i2c_master_read_byte(cmd, data + length - 1, I2C_MASTER_NACK);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(i2c_port_, cmd, pdMS_TO_TICKS(1000));
        i2c_cmd_link_delete(cmd);
        return (ret == ESP_OK) ? length : -1;
    }

    bool DataAvailable() {
        // Check interrupt pin or always return true
        return true;
    }

    void Delay(uint32_t ms) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    uint32_t GetTimeUs() {
        return esp_timer_get_time();
    }
};
```

#### STM32 (HAL) - I²C

```cpp
#include "stm32f4xx_hal.h"
#include "bno08x_comm_interface.hpp"

extern I2C_HandleTypeDef hi2c1;

class STM32I2CComm : public bno08x::CommInterface<STM32I2CComm> {
private:
    uint16_t device_addr_;

public:
    STM32I2CComm(uint16_t addr) : device_addr_(addr << 1) {}

    bool Open() {
        return HAL_I2C_IsDeviceReady(&hi2c1, device_addr_, 3, 100) == HAL_OK;
    }

    void Close() {
        // Nothing to do
    }

    int Write(const uint8_t* data, uint32_t length) {
        HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c1, device_addr_,
                                                          const_cast<uint8_t*>(data),
                                                          length, 100);
        return (status == HAL_OK) ? length : -1;
    }

    int Read(uint8_t* data, uint32_t length) {
        HAL_StatusTypeDef status = HAL_I2C_Master_Receive(&hi2c1, device_addr_,
                                                          data, length, 100);
        return (status == HAL_OK) ? length : -1;
    }

    bool DataAvailable() {
        return true;
    }

    void Delay(uint32_t ms) {
        HAL_Delay(ms);
    }

    uint32_t GetTimeUs() {
        return HAL_GetTick() * 1000;
    }
};
```

#### Arduino - I²C

```cpp
#include <Wire.h>
#include "bno08x_comm_interface.hpp"

class ArduinoComm : public bno08x::CommInterface<ArduinoComm> {
private:
    uint8_t device_addr_;

public:
    ArduinoComm(uint8_t addr) : device_addr_(addr) {}

    bool Open() {
        Wire.begin();
        Wire.setClock(400000);
        delay(50);
        return true;
    }

    void Close() {
        // Nothing to do
    }

    int Write(const uint8_t* data, uint32_t length) {
        Wire.beginTransmission(device_addr_);
        Wire.write(data, length);
        return (Wire.endTransmission() == 0) ? length : -1;
    }

    int Read(uint8_t* data, uint32_t length) {
        Wire.requestFrom(device_addr_, length);
        size_t count = 0;
        while (Wire.available() && count < length) {
            data[count++] = Wire.read();
        }
        return count;
    }

    bool DataAvailable() {
        return true;
    }

    void Delay(uint32_t ms) {
        delay(ms);
    }

    uint32_t GetTimeUs() {
        return micros();
    }
};
```

## Optional Pin Control Methods

If your hardware has these pins wired, implement the optional methods:

```cpp
void SetReset(bool state) {
    // Control RSTN pin (active low)
    gpio_set_level(rst_pin_, state ? 0 : 1);
}

void SetBoot(bool state) {
    // Control BOOTN pin (for DFU mode)
    gpio_set_level(boot_pin_, state ? 0 : 1);
}

void SetWake(bool state) {
    // Control WAKE pin (SPI mode only)
    gpio_set_level(wake_pin_, state ? 0 : 1);
}
```

## Testing Your Implementation

1. Create a simple test that opens the interface
2. Verify I²C/SPI communication works
3. Test read/write operations
4. Verify timing functions return correct values

## Next Steps

- Review the [API Reference](api_reference.md) for driver methods
- Check [Examples](examples.md) for complete usage examples
- See [Troubleshooting](troubleshooting.md) for common issues

---

**Navigation**
⬅️ [Hardware Setup](hardware_setup.md) | [Next: Configuration ➡️](configuration.md) | [Back to Index](index.md)

