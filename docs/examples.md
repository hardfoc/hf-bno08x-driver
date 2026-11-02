# Examples

This guide provides complete, working examples demonstrating various use cases for the BNO08x driver.

## Example 1: Basic Polling Mode

This example shows the minimal setup required to read orientation and linear acceleration using polling mode.

```cpp
#include "bno08x.hpp"
#include "esp32_bno08x_bus.hpp"

void app_main() {
    // 1. Configure I2C transport (INT=GPIO17, RST=GPIO16 per hardware_setup.md)
    Esp32Bno08xBus::I2CConfig config;
    config.sda_pin = GPIO_NUM_4;
    config.scl_pin = GPIO_NUM_5;
    config.frequency = 400000;
    config.device_address = 0x4B;   // Try 0x4B first (SA0=HIGH), then 0x4A if probe fails
    config.int_pin = GPIO_NUM_17;
    config.rst_pin = GPIO_NUM_16;

    // 2. Create transport, reset, then probe (examples try 0x4B then 0x4A)
    auto transport = CreateEsp32Bno08xBus(config);
    if (!transport) return;
    transport->HardwareReset(2, 200);   // 2 ms reset, 200 ms boot delay
    if (!transport->Probe()) {
        config.device_address = 0x4A;   // Fallback to 0x4A (SA0=LOW)
        transport = CreateEsp32Bno08xBus(config);
        if (!transport || !transport->Probe()) { printf("No BNO08x at 0x4B or 0x4A\n"); return; }
    }

    // 3. Create IMU instance and initialize
    BNO085<Esp32Bno08xBus> imu(*transport);
    if (!imu.Begin()) {
        printf("Failed to initialize BNO085\n");
        return;
    }

    // 4. Enable sensors
    imu.EnableSensor(BNO085Sensor::RotationVector, 20);       // 50 Hz
    imu.EnableSensor(BNO085Sensor::LinearAcceleration, 20);  // 50 Hz

    // 5. Polling loop
    while (true) {
        imu.Update();

        if (imu.HasNewData(BNO085Sensor::RotationVector)) {
            auto rot = imu.GetLatest(BNO085Sensor::RotationVector);
            printf("Quat: w=%.3f x=%.3f y=%.3f z=%.3f\n",
                   rot.rotation.w, rot.rotation.x, rot.rotation.y, rot.rotation.z);
        }

        if (imu.HasNewData(BNO085Sensor::LinearAcceleration)) {
            auto accel = imu.GetLatest(BNO085Sensor::LinearAcceleration);
            printf("Linear Accel: %.2f %.2f %.2f m/s²\n",
                   accel.vector.x, accel.vector.y, accel.vector.z);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

### Explanation

1. **I2C Configuration**: Configure the I2C bus (pins, address, optional INT/RST pins).
2. **Transport, reset, probe**: Create transport, call `HardwareReset(2, 200)`, then `Probe()`. Examples try 0x4B first, then 0x4A if probe fails.
3. **IMU instance and init**: Create `BNO085<Esp32Bno08xBus>` and call `Begin()`.
4. **Sensor enablement**: Enable desired sensors with report interval in milliseconds.
5. **Polling loop**: Call `Update()` and check `HasNewData()` / `GetLatest()`.

### Expected Output

```
Quat: w=0.924 x=0.012 y=-0.018 z=0.382
Linear Accel: 0.12 0.05 -9.81 m/s²
Quat: w=0.924 x=0.012 y=-0.018 z=0.383
Linear Accel: 0.11 0.06 -9.80 m/s²
```

---

## Example 2: Event-Driven Callback Mode

This example demonstrates using callbacks for event-driven sensor data processing.

```cpp
#include "bno08x.hpp"
#include "esp32_bno08x_bus.hpp"

// Callback function for sensor events
void sensor_callback(const SensorEvent& event) {
    switch (event.sensor) {
        case BNO085Sensor::RotationVector:
            printf("Orientation - w=%.3f x=%.3f y=%.3f z=%.3f (accuracy=%d)\n",
                   event.rotation.w, event.rotation.x,
                   event.rotation.y, event.rotation.z,
                   event.rotation.accuracy);
            break;

        case BNO085Sensor::StepCounter:
            printf("Step Count: %lu\n", event.stepCount);
            break;

        case BNO085Sensor::TapDetector:
            if (event.detected) {
                printf("%s\n", event.tap.doubleTap ? "Double Tap!" : "Tap!");
            }
            break;

        case BNO085Sensor::ShakeDetector:
            if (event.detected) {
                printf("Shake Detected!\n");
            }
            break;

        default:
            break;
    }
}

void app_main() {
    // I2C setup: config, CreateEsp32Bno08xBus, HardwareReset(2,200), Probe(); try 0x4B then 0x4A (see Example 1)
    auto transport = CreateEsp32Bno08xBus(config);
    if (transport) transport->HardwareReset(2, 200);
    if (!transport || !transport->Probe()) return;

    BNO085<Esp32Bno08xBus> imu(*transport);
    if (!imu.Begin()) { printf("Initialization failed\n"); return; }

    // Register callback
    imu.SetCallback(sensor_callback);

    // Enable event-driven sensors (interval = 0 for on-change)
    imu.EnableSensor(BNO085Sensor::RotationVector, 10);  // 100 Hz
    imu.EnableSensor(BNO085Sensor::StepCounter, 0);      // on-change
    imu.EnableSensor(BNO085Sensor::TapDetector, 0);      // events
    imu.EnableSensor(BNO085Sensor::ShakeDetector, 0);    // events

    // Main loop - just call Update() to process events
    while (true) {
        imu.Update();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

### Explanation

1. **Callback Function**: Define a callback function that handles different sensor events
2. **Callback Registration**: Register the callback using `SetCallback()`
3. **Event-Driven Sensors**: Enable sensors with interval `0` for on-change/event-driven behavior
4. **Update Loop**: Simply call `Update()` - callbacks are invoked automatically when data arrives

### Expected Output

```
Orientation - w=0.924 x=0.012 y=-0.018 z=0.382 (accuracy=3)
Orientation - w=0.924 x=0.012 y=-0.018 z=0.383 (accuracy=3)
Tap!
Step Count: 1
Double Tap!
Shake Detected!
```

---

## Example 3: Full Features - Multiple Sensors

This example demonstrates enabling multiple sensors simultaneously using both callback and polling methods.

```cpp
#include "bno08x.hpp"
#include "esp32_bno08x_bus.hpp"

void event_callback(const SensorEvent& event) {
    switch (event.sensor) {
        case BNO085Sensor::RotationVector:
            printf("Rotation w=%.3f x=%.3f y=%.3f z=%.3f\n",
                   event.rotation.w, event.rotation.x,
                   event.rotation.y, event.rotation.z);
            break;

        case BNO085Sensor::LinearAcceleration:
            printf("Linear accel %.2f %.2f %.2f m/s²\n",
                   event.vector.x, event.vector.y, event.vector.z);
            break;

        case BNO085Sensor::Gyroscope:
            printf("Gyro %.2f %.2f %.2f rad/s\n",
                   event.vector.x, event.vector.y, event.vector.z);
            break;

        case BNO085Sensor::StepCounter:
            printf("Steps: %lu\n", event.stepCount);
            break;

        case BNO085Sensor::TapDetector:
            if (event.detected) {
                printf("%s\n", event.tap.doubleTap ? "Double tap!" : "Tap!");
            }
            break;

        default:
            break;
    }
}

void app_main() {
    // I2C setup: config, CreateEsp32Bno08xBus, HardwareReset(2,200), Probe(); try 0x4B then 0x4A (see Example 1)
    auto transport = CreateEsp32Bno08xBus(config);
    if (transport) transport->HardwareReset(2, 200);
    if (!transport || !transport->Probe()) return;

    BNO085<Esp32Bno08xBus> imu(*transport);
    if (!imu.Begin()) return;

    imu.SetCallback(event_callback);

    // Enable multiple sensors with different rates
    imu.EnableSensor(BNO085Sensor::RotationVector, 10);     // 100 Hz
    imu.EnableSensor(BNO085Sensor::LinearAcceleration, 20); // 50 Hz
    imu.EnableSensor(BNO085Sensor::Gyroscope, 20);          // 50 Hz
    imu.EnableSensor(BNO085Sensor::Gravity, 50);             // 20 Hz
    imu.EnableSensor(BNO085Sensor::StepCounter, 0);         // on-change
    imu.EnableSensor(BNO085Sensor::TapDetector, 0);         // events

    while (true) {
        imu.Update();

        // Poll for gravity (not using callback)
        if (imu.HasNewData(BNO085Sensor::Gravity)) {
            auto g = imu.GetLatest(BNO085Sensor::Gravity);
            printf("Gravity %.2f %.2f %.2f m/s²\n",
                   g.vector.x, g.vector.y, g.vector.z);
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

### Explanation

This example shows:
- **Mixed Usage**: Using both callbacks and polling in the same application
- **Multiple Sensors**: Enabling several sensors with different update rates
- **Flexible Data Access**: Some sensors use callbacks, others use polling

---

## Example 4: Error Handling

This example shows proper error handling and recovery.

```cpp
#include "bno08x.hpp"
#include "esp32_bno08x_bus.hpp"

void app_main() {
    // I2C setup: config, CreateEsp32Bno08xBus, HardwareReset(2,200), Probe(); try 0x4B then 0x4A (see Example 1)
    auto transport = CreateEsp32Bno08xBus(config);
    if (!transport) { printf("ERROR: Failed to create I2C transport\n"); return; }
    transport->HardwareReset(2, 200);
    if (!transport->Probe()) { printf("ERROR: Probe failed at 0x%02X\n", config.device_address); return; }

    BNO085<Esp32Bno08xBus> imu(*transport);

    // Initialize with error checking
    if (!imu.Begin()) {
        printf("ERROR: Failed to initialize BNO085\n");
        printf("Check I2C connections and address (0x4A or 0x4B)\n");
        return;
    }

    printf("BNO085 initialized successfully\n");

    // Enable sensor with error checking
    if (!imu.EnableSensor(BNO085Sensor::RotationVector, 10)) {
        printf("ERROR: Failed to enable Rotation Vector sensor\n");
        return;
    }

    printf("Rotation Vector enabled at 100 Hz\n");

    // Main loop with error checking
    // Note: The driver automatically re-enables configured sensors after a
    // sensor reset (handled internally by handleAsyncEvent). You only need
    // to monitor for data and check GetLastError() for issues.
    while (true) {
        imu.Update();

        // Check for driver errors
        if (imu.GetLastError() != 0) {
            printf("WARNING: Driver error %d detected\n", imu.GetLastError());
        }

        if (imu.HasNewData(BNO085Sensor::RotationVector)) {
            auto rot = imu.GetLatest(BNO085Sensor::RotationVector);
            printf("Quat: w=%.3f x=%.3f y=%.3f z=%.3f\n",
                   rot.rotation.w, rot.rotation.x, rot.rotation.y, rot.rotation.z);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### Explanation

1. **Initialization Checks**: Verify `Begin()` returns success
2. **Sensor Enable Checks**: Verify sensor enablement succeeds
3. **Auto-Recovery**: The driver automatically re-enables configured sensors after a sensor reset
4. **Error Monitoring**: Use `GetLastError()` to check for driver errors

---

## Example 5: RVC Mode

For UART RVC (simplified yaw/pitch/roll streaming), use a transport whose `GetInterfaceType()` returns `BNO085Interface::UARTRVC` (e.g. `Esp32UartRvcBus`). See [RVC Mode](special_feature_rvc.md) for full details and `examples/esp32/main/rvc_mode_example.cpp`.

```cpp
// Create UART RVC transport (115200, PS1=1 PS0=0), then:
imu.SetRvcCallback([](const RvcSensorValue& v) {
    printf("Yaw: %.2f° Pitch: %.2f° Roll: %.2f°\n", v.yaw_deg, v.pitch_deg, v.roll_deg);
});
if (imu.BeginRvc()) {
    while (true) { imu.ServiceRvc(); vTaskDelay(pdMS_TO_TICKS(10)); }
    imu.CloseRvc();
}
```

## Example 6: DFU (Firmware Update)

Enter bootloader (hold BOOTN low, then reset), then call `Dfu()`. See [DFU](special_feature_dfu.md) and `examples/esp32/main/dfu_example.cpp`.

```cpp
imu.SetBootPin(true);   // BOOTN low
imu.HardwareReset(10);
// ... then imu.Dfu(firmware) or use MemoryFirmware for runtime image
```

## Running the Examples

### ESP32

From the repository root, use the build script (recommended):

```bash
cd examples/esp32
./scripts/build_app.sh <app_type> Release
./scripts/flash_app.sh <app_type> Release
```

**Available app types:**

| App | Description |
|-----|-------------|
| `driver_integration_test` | Full driver test suite (no hardware required) |
| `basic_polling` | Polling mode: rotation vector + linear acceleration |
| `event_driven_callback` | Callback mode: rotation, step counter, tap/shake |
| `full_features` | Multiple sensors, callback + polling |
| `rvc_mode` | RVC UART streaming (yaw/pitch/roll) |
| `dfu_update` | Device Firmware Update example |

Build examples: `./scripts/build_app.sh basic_polling Release`, then flash and monitor with `idf.py flash monitor` from the build directory, or use `./scripts/flash_app.sh basic_polling Release`.

### Other Platforms

For other platforms, implement the `CommInterface` for your platform (see [Platform Integration](platform_integration.md)) and compile with C++11 or newer:

```bash
g++ -std=c++11 -I inc/ your_code.cpp src/bno08x.cpp -o test
```

## Next Steps

- Review the [API Reference](api_reference.md) for method details
- Check [Troubleshooting](troubleshooting.md) if you encounter issues
- Explore the [examples directory](../examples/) for more examples
- Learn about [RVC Mode](special_feature_rvc.md) for simplified UART streaming
- Check [DFU](special_feature_dfu.md) for firmware update capabilities

---

**Navigation**
⬅️ [API Reference](api_reference.md) | [Next: RVC Mode ➡️](special_feature_rvc.md) | [Back to Index](index.md)
