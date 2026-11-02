# Quick Start

This guide will get you up and running with the BNO08x driver in just a few steps.

## Prerequisites

- [Driver installed](installation.md)
- [Hardware wired](hardware_setup.md)
- [Communication interface implemented](platform_integration.md)

## Minimal Example

Here's a complete working example:

```cpp
#include "bno08x.hpp"

// 1. Implement the communication interface (must include GetInterfaceType — see platform_integration.md)
class MyComm : public bno08x::CommInterface<MyComm> {
public:
    BNO085Interface GetInterfaceType() { return BNO085Interface::I2C; }  // or SPI, UART, UARTRVC
    bool Open() {
        // Your I²C/SPI/UART initialization
        return true;
    }
    void Close() { /* cleanup */ }
    int Write(const uint8_t* data, uint32_t length) { return length; }
    int Read(uint8_t* data, uint32_t length) { return length; }
    bool DataAvailable() { return true; }
    void Delay(uint32_t ms) { /* delay */ }
    uint32_t GetTimeUs() { return 0; }
};

// 2. Create instances
MyComm comm;
BNO085<MyComm> imu(comm);

// 3. Initialize
if (!imu.Begin()) {
    printf("Initialization failed\n");
    return;
}

// 4. Enable sensors
imu.EnableSensor(BNO085Sensor::RotationVector, 10);  // 100 Hz
imu.EnableSensor(BNO085Sensor::Accelerometer, 50); // 20 Hz

// 5. Set callback (use event.rotation.accuracy for calibration status 0–3)
imu.SetCallback([](const SensorEvent& e) {
    if (e.sensor == BNO085Sensor::RotationVector) {
        printf("Quat: w=%.3f x=%.3f y=%.3f z=%.3f (accuracy=%d)\n",
               e.rotation.w, e.rotation.x, e.rotation.y, e.rotation.z, e.rotation.accuracy);
    }
});

// 6. Update loop
while (true) {
    imu.Update();  // Call as often as possible
    delay(5);
}
```

## Step-by-Step Explanation

### Step 1: Include the Header

```cpp
#include "bno08x.hpp"
```

This includes the main driver class and all necessary types.

### Step 2: Implement the Communication Interface

You need to implement the `CommInterface` for your platform. See [Platform Integration](platform_integration.md) for detailed examples.

```cpp
class MyComm : public bno08x::CommInterface<MyComm> {
    // Implement all required methods
};
```

### Step 3: Create Driver Instance

```cpp
MyComm comm;
BNO085<MyComm> imu(comm);
```

The constructor takes a reference to your communication interface implementation.

### Step 4: Initialize

```cpp
if (!imu.Begin()) {
    // Handle error
    return;
}
```

### Step 5: Enable Sensors

```cpp
imu.EnableSensor(BNO085Sensor::RotationVector, 10);  // 100 Hz
```

The interval is in milliseconds. Use `0` for on-change sensors.

### Step 6: Set Callback

```cpp
imu.SetCallback([](const SensorEvent& e) {
    // Handle sensor events
});
```

### Step 7: Update Loop

```cpp
while (true) {
    imu.Update();  // Must be called frequently
    delay(5);
}
```

## Complete Example with Error Handling

```cpp
#include "bno08x.hpp"

class MyComm : public bno08x::CommInterface<MyComm> {
    // ... communication implementation
};

void app_main() {
    MyComm comm;
    BNO085<MyComm> imu(comm);

    // Initialize
    if (!imu.Begin()) {
        printf("Initialization failed\n");
        return;
    }

    // Enable sensors
    if (!imu.EnableSensor(BNO085Sensor::RotationVector, 10)) {
        printf("Failed to enable rotation vector\n");
        return;
    }

    // Set callback
    imu.SetCallback([](const SensorEvent& e) {
        if (e.sensor == BNO085Sensor::RotationVector) {
            printf("Quat: w=%.3f x=%.3f y=%.3f z=%.3f\n",
                   e.rotation.w, e.rotation.x, e.rotation.y, e.rotation.z);
        }
    });

    // Main loop
    while (true) {
        imu.Update();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

## Expected Output

When running this example, you should see quaternion and optionally accuracy (0–3, 3 = fully calibrated):

```
Quat: w=0.924 x=0.012 y=-0.018 z=0.382 (accuracy=3)
Quat: w=0.924 x=0.012 y=-0.018 z=0.383 (accuracy=3)
```

## Troubleshooting

If you encounter issues:

- **Compilation errors**: Check that you've implemented all required methods in your communication interface
- **Initialization fails**: Verify I²C/SPI connections and hardware setup
- **No data**: Ensure `Update()` is called frequently
- **See**: [Troubleshooting](troubleshooting.md) for common issues

## Next Steps

- Explore [Examples](examples.md) for more advanced usage
- Review the [API Reference](api_reference.md) for all available methods
- Check [Configuration](configuration.md) for customization options

---

**Navigation**
⬅️ [Installation](installation.md) | [Next: Hardware Setup ➡️](hardware_setup.md) | [Back to Index](index.md)

