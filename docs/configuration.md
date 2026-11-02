# Configuration

This guide covers all configuration options available for the BNO08x driver.

## Sensor Configuration

### Enabling Sensors

```cpp
// Enable Rotation Vector at 100 Hz (10ms interval)
imu.EnableSensor(BNO085Sensor::RotationVector, 10);

// Enable Step Counter (on-change, interval = 0)
imu.EnableSensor(BNO085Sensor::StepCounter, 0);

// Enable Accelerometer at 50 Hz with sensitivity threshold
imu.EnableSensor(BNO085Sensor::Accelerometer, 20, 0.1f);
```

**Interval**: Report interval in milliseconds (0 = on-change only)

**Sensitivity**: Change sensitivity threshold for on-change sensors (0.0 = disabled)

### Available Sensors

| Sensor | Description | Typical Rate |
|--------|-------------|--------------|
| `RotationVector` | Fused orientation quaternion | 1-400 Hz |
| `GameRotationVector` | Orientation without heading | 1-400 Hz |
| `Accelerometer` | Calibrated acceleration | 1-400 Hz |
| `Gyroscope` | Calibrated angular velocity | 1-400 Hz |
| `Magnetometer` | Calibrated magnetic field | 1-50 Hz |
| `LinearAcceleration` | Acceleration minus gravity | 1-400 Hz |
| `Gravity` | Gravity vector | 1-50 Hz |
| `StepCounter` | Step count (on-change) | 0 (on-change) |
| `StepDetector` | Step detection event | 0 (on-change) |
| `TapDetector` | Tap gesture detection | 0 (on-change) |
| `ShakeDetector` | Shake gesture detection | 0 (on-change) |

### Disabling Sensors

```cpp
imu.DisableSensor(BNO085Sensor::RotationVector);
```

## Callback Configuration

### Sensor Event Callback

```cpp
imu.SetCallback([](const SensorEvent& e) {
    if (e.sensor == BNO085Sensor::RotationVector) {
        printf("Quat: w=%.3f x=%.3f y=%.3f z=%.3f\n",
               e.rotation.w, e.rotation.x, e.rotation.y, e.rotation.z);
    }
});
```

### RVC Mode Callback

```cpp
imu.SetRvcCallback([](const RvcSensorValue& val) {
    printf("Yaw: %.2f°, Pitch: %.2f°, Roll: %.2f°\n",
           val.yaw_deg, val.pitch_deg, val.roll_deg);
    printf("Accel: X=%.3f Y=%.3f Z=%.3f g\n",
           val.acc_x_g, val.acc_y_g, val.acc_z_g);
});
```

## Interface Selection

### Selecting Communication Interface

```cpp
// Select I²C interface (PS1=0, PS0=0)
imu.SelectInterface(BNO085Interface::I2C);

// Select SPI interface (PS1=1, PS0=1)
imu.SelectInterface(BNO085Interface::SPI);

// Select UART interface (PS1=0, PS0=1)
imu.SelectInterface(BNO085Interface::UART);

// Select UART RVC mode (PS1=1, PS0=0)
imu.SelectInterface(BNO085Interface::UARTRVC);
```

**Note**: Interface selection is typically done via hardware pins (PS0/PS1) at boot time. `SelectInterface()` is only useful when those pins are connected to controllable GPIOs.

## Hardware Pin Control

### Reset Control

```cpp
// Hardware reset (if RSTN pin is wired)
imu.HardwareReset(2);  // Hold reset low for 2ms
```

### Boot Pin Control (DFU Mode)

```cpp
// Set BOOTN pin low to enter bootloader mode
imu.SetBootPin(true);   // Active low, so true = low
imu.HardwareReset(10);  // Reset while BOOTN is low
```

### Wake Pin Control (SPI Mode)

```cpp
// Pull WAKE pin low to wake sensor from suspend
imu.SetWakePin(true);   // Active low
```

## Data Access Modes

### Callback Mode (Recommended)

```cpp
imu.SetCallback([](const SensorEvent& e) {
    // Process event immediately
});
imu.Update();  // Call frequently
```

### Polling Mode

```cpp
imu.Update();  // Must be called frequently

if (imu.HasNewData(BNO085Sensor::RotationVector)) {
    auto event = imu.GetLatest(BNO085Sensor::RotationVector);
    // Process event
}
```

## Sensor Accuracy

For orientation reports, calibration status is in `event.rotation.accuracy` (0-3). Other report types use their own accuracy or status fields.

- **0**: Unreliable - sensor not calibrated
- **1**: Low accuracy - calibration in progress
- **2**: Medium accuracy - partially calibrated
- **3**: High accuracy - fully calibrated

**Recommendation**: Wait for accuracy = 3 before trusting orientation data, especially for heading/yaw.

## Power Management

### Disable Unused Sensors

To save power, disable sensors you don't need:

```cpp
// Only enable what you need
imu.EnableSensor(BNO085Sensor::RotationVector, 10);
// Don't enable other sensors if not needed
```

### Update Frequency

Call `Update()` as frequently as possible for best performance. The driver is designed to be called from:
- Main loop
- RTOS task
- Interrupt service routine (ISR)
- Timer callback

## Default Configuration

After `Begin()`, the sensor is initialized with:
- No sensors enabled (must enable explicitly)
- No callbacks registered
- Default sensor fusion settings
- Auto re-sync enabled (detects resets and re-enables configured sensors)

## Next Steps

- Review [Examples](examples.md) for configuration examples
- Check [API Reference](api_reference.md) for all configuration methods
- See [Troubleshooting](troubleshooting.md) for configuration issues

---

**Navigation**
⬅️ [Platform Integration](platform_integration.md) | [Next: Examples ➡️](examples.md) | [Back to Index](index.md)

