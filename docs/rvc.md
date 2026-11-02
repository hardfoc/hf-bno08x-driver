# RVC Mode (Robot Vacuum Cleaner Mode)

RVC mode is a simplified UART streaming protocol that provides basic orientation and motion data without requiring the full SH-2 command interface. This mode is ideal for resource-constrained systems that only need yaw/pitch/roll and linear acceleration.

## Why Use RVC Mode?

- **No Command Parsing**: Sensor outputs data continuously once powered
- **Lower Overhead**: Simplified protocol reduces processing requirements
- **Resource Efficient**: Perfect for systems with limited CPU/memory
- **Simple Integration**: Fixed frame format, no configuration needed

## Frame Format

Each RVC frame is 19 bytes with the following structure:

| Offset | Bytes | Description |
|--------|-------|-------------|
| 0 | 2 | Sync bytes `0xAA 0xAA` |
| 2 | 1 | `index` sequence counter |
| 3 | 2 | `yaw` (little endian, 0.01°/LSB) |
| 5 | 2 | `pitch` (little endian, 0.01°/LSB) |
| 7 | 2 | `roll` (little endian, 0.01°/LSB) |
| 9 | 2 | `acc_x` (little endian, 0.001 g/LSB) |
| 11 | 2 | `acc_y` (little endian, 0.001 g/LSB) |
| 13 | 2 | `acc_z` (little endian, 0.001 g/LSB) |
| 15 | 1 | `mi` Motion Intent |
| 16 | 1 | `mr` Motion Request |
| 17 | 1 | Checksum of bytes 2-17 |

**Checksum**: Simple unsigned sum of bytes 2 through 17 modulo 256.

## Motion Intent and Motion Request

The `mi` (Motion Intent) and `mr` (Motion Request) fields indicate sensor state:

### Motion Intent Values

```cpp
MI_UNKNOWN                      0
MI_STATIONARY_WITHOUT_VIBRATION 1
MI_STATIONARY_WITH_VIBRATION    2
MI_IN_MOTION                    3
```

### Motion Request Values

```cpp
MR_NO_CONSTRAINT                0
MR_STAY_STATIONARY_REQUIRED     1
MR_NON_URGENT_STAY_STATIONARY   3
MR_URGENT_STATIONARY            4
MR_TIMER_STATIONARY             5
```

## Entering RVC Mode

RVC mode is selected at boot time via hardware pins:

- **PS1 = VIN (1), PS0 = GND (0)**: UART RVC mode
- Sensor must be reset after setting pins
- Once in RVC mode, the sensor continuously streams 19-byte frames at 115200 bps (8N1)

## Implementation

RVC uses the same **CommInterface** as I²C/SPI/UART SH-2 mode. You implement a transport that returns `GetInterfaceType() == BNO085Interface::UARTRVC` (e.g. a UART at 115200 baud). The driver then uses that transport for `BeginRvc()`, `ServiceRvc()`, and `CloseRvc()`; no separate RVC HAL or class is required.

### Step 1: Implement a UART CommInterface that reports UARTRVC

Your transport must implement all required `CommInterface` methods (including `GetInterfaceType()` returning `BNO085Interface::UARTRVC`), and use UART at 115200 8N1. The ESP32 examples provide `Esp32UartRvcBus` in `esp32_uart_rvc_bus.hpp` as a reference.

### Step 2: Use BNO085 RVC methods

```cpp
#include "bno08x.hpp"
#include "esp32_uart_rvc_bus.hpp"   // or your UART CommInterface

// Create UART RVC transport (115200, PS1=1 PS0=0 on hardware)
Esp32UartRvcBus::UartConfig uart_config;
uart_config.port = UART_NUM_1;
uart_config.tx_pin = GPIO_NUM_17;
uart_config.rx_pin = GPIO_NUM_18;
uart_config.baud_rate = 115200;
Esp32UartRvcBus transport(uart_config);
BNO085<Esp32UartRvcBus> imu(transport);

// Set callback for decoded frames (RvcSensorValue)
imu.SetRvcCallback([](const RvcSensorValue& val) {
    printf("Yaw: %.2f°, Pitch: %.2f°, Roll: %.2f°\n",
           val.yaw_deg, val.pitch_deg, val.roll_deg);
    printf("Accel: X=%.3f Y=%.3f Z=%.3f g\n",
           val.acc_x_g, val.acc_y_g, val.acc_z_g);
});

if (imu.BeginRvc()) {
    while (true) {
        imu.ServiceRvc();   // Reads UART, parses frames, invokes callback
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    imu.CloseRvc();
}
```

### Step 3: RvcSensorValue fields

The callback receives a `RvcSensorValue` with floating-point fields derived from the 19-byte frame:

| Field | Unit | Description |
|-------|------|-------------|
| `yaw_deg` | degrees | Yaw angle |
| `pitch_deg` | degrees | Pitch angle |
| `roll_deg` | degrees | Roll angle |
| `acc_x_g`, `acc_y_g`, `acc_z_g` | g | Linear acceleration |
| `motion_intent` | — | Motion intent (see Motion Intent values below) |
| `motion_request` | — | Motion request (see Motion Request values below) |

## Complete ESP32 example

See `examples/esp32/main/rvc_mode_example.cpp` for a full example using `Esp32UartRvcBus`. Summary:

```cpp
Esp32UartRvcBus transport(uart_config);
BNO085<Esp32UartRvcBus> imu(transport);
imu.SetRvcCallback(on_rvc_frame);
if (imu.BeginRvc()) {
    while (true) { imu.ServiceRvc(); vTaskDelay(pdMS_TO_TICKS(10)); }
    imu.CloseRvc();
}
```

## UART Configuration

- **Baud Rate**: 115200 bps (typical, check datasheet for your device)
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Flow Control**: None

## Data Units

- **Yaw/Pitch/Roll**: Degrees (0.01° per LSB)
- **Acceleration**: g (0.001 g per LSB)
- **Time**: Microseconds

## Limitations

- **Fixed Data**: Only yaw/pitch/roll and linear acceleration
- **No Configuration**: Cannot change report rates or enable other sensors
- **UART Only**: Requires UART interface (not I²C or SPI)
- **Boot-Time Selection**: Must be selected at boot via pins

## When to Use RVC Mode

**Use RVC Mode When:**
- You only need basic orientation data
- System resources are limited
- Simple integration is preferred
- UART interface is available

**Use Full SH-2 Mode When:**
- You need multiple sensor types
- You need configurable report rates
- You need gesture detection or step counting
- You need I²C or SPI interface

## Next Steps

- See [DFU](special_feature_dfu.md) for firmware update guide
- Review [Examples](examples.md) for more usage examples
- Check [Troubleshooting](troubleshooting.md) for RVC mode issues

---

**Navigation**
⬅️ [Examples](examples.md) | [Next: DFU ➡️](special_feature_dfu.md) | [Back to Index](index.md)

