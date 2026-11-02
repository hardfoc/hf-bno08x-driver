# Troubleshooting

This guide helps you diagnose and resolve common issues when using the BNO08x driver.

## Common Error Messages

### Error: Initialization Failed

**Symptoms:**
- `Begin()` returns `false`
- Sensor not responding
- `GetLastError()` returns non-zero

**Causes:**
- Communication interface not properly initialized
- Hardware connections incorrect
- Power supply issues
- Wrong I²C address

**Solutions:**
1. **Verify communication interface**: Ensure the interface is initialized and implements `GetInterfaceType()`; perform a hardware reset before first use
2. **Check connections**: Verify all I²C/SPI connections (SCL, SDA, CS, etc.)
3. **Verify power**: Check power supply voltage (2.4V–3.6V, typically 3.3V)
4. **Check I²C address**: Try 0x4B first (SA0=HIGH), then 0x4A (SA0=LOW); use transport `Probe()` after reset if available
5. **Check pull-ups**: Ensure I²C has proper pull-up resistors (4.7 kΩ)
6. **Verify interface selection**: Check PS0/PS1 pins match the intended interface

---

### Error: No Sensor Data

**Symptoms:**
- `Begin()` succeeds but no data received
- `HasNewData()` always returns false
- Callbacks never called

**Causes:**
- Sensors not enabled
- `Update()` not called frequently enough
- Sensor not calibrated
- Communication errors

**Solutions:**
1. **Enable Sensors**: Call `EnableSensor()` for each sensor you need
2. **Call Update Frequently**: `Update()` must be called as often as possible (every 5-10ms)
3. **Check Sensor Status**: Wait for calibration (accuracy = 3)
4. **Verify Communication**: Check for communication errors with `GetLastError()`
5. **Check Interval**: Ensure report interval is reasonable (not too fast)

---

### Error: Communication Error

**Symptoms:**
- `GetLastError()` returns communication error codes
- Intermittent data loss
- Timeout errors

**Causes:**
- I²C/SPI configuration incorrect
- Signal integrity issues
- Bus speed too high
- Interference

**Solutions:**
1. **Check I²C Speed**: Try lower speed (e.g., 100 kHz instead of 400 kHz)
2. **Verify Pull-ups**: Ensure proper pull-up resistors on I²C bus
3. **Check Wiring**: Verify all connections are secure and wires are short
4. **Reduce Interference**: Keep sensor away from noise sources
5. **Check Bus Loading**: Ensure bus capacitance is within limits

---

### Error: Sensor Not Calibrating

**Symptoms:**
- `accuracy` field stays at 0 or 1
- Orientation data unreliable
- Heading drifts

**Causes:**
- Sensor needs calibration motion
- Magnetic interference
- Sensor not moving
- Calibration timeout

**Solutions:**
1. **Perform Calibration Motion**: Move sensor in figure-8 pattern
2. **Check Magnetic Environment**: Avoid strong magnetic fields
3. **Wait for Calibration**: Calibration can take 10-30 seconds
4. **Check Sensor Placement**: Ensure sensor is not near magnetic materials
5. **Verify Magnetometer**: Check if magnetometer is enabled and working

---

### Error: Wrong Orientation Data

**Symptoms:**
- Euler angles seem incorrect
- Quaternion values unexpected
- Coordinate system mismatch

**Causes:**
- Sensor mounting orientation
- Coordinate system convention
- Calibration issues

**Solutions:**
1. **Check Mounting**: Verify sensor orientation matches expected coordinate system
2. **Apply Transformations**: Rotate quaternion/Euler angles to match your coordinate system
3. **Verify Calibration**: Ensure sensor is fully calibrated (accuracy = 3)
4. **Check Datasheet**: Review sensor coordinate system in datasheet

---

### Error: RVC Mode Not Working

**Symptoms:**
- `BeginRvc()` fails
- No RVC frames received
- UART communication errors

**Causes:**
- Sensor not in RVC mode (wrong PS0/PS1)
- UART configuration incorrect
- Baud rate mismatch
- Transport does not return `UARTRVC` from `GetInterfaceType()`

**Solutions:**
1. **Verify RVC mode**: Set PS1=VIN (1), PS0=GND (0) and reset the sensor
2. **Check baud rate**: Use 115200 bps (8N1)
3. **Use UARTRVC transport**: Implement a `CommInterface` that returns `BNO085Interface::UARTRVC` (e.g. `Esp32UartRvcBus`); there is no separate RVC HAL
4. **Check wiring**: Verify UART TX/RX connections
5. **Call ServiceRvc**: Call `ServiceRvc()` frequently in your loop

---

### Error: DFU Update Failed

**Symptoms:**
- `Dfu()` returns error code
- Firmware update doesn't complete
- Bootloader not responding

**Causes:**
- Sensor not in bootloader mode
- Firmware image invalid
- Communication timeout
- Using UARTRVC transport (DFU is not supported for RVC mode)

**Solutions:**
1. **Enter bootloader**: Hold BOOTN low, then reset (e.g. `SetBootPin(true)`, `HardwareReset(10)`, then release BOOTN)
2. **Verify firmware**: Use a valid `HcBin_t` (e.g. from `firmware.h` or `MemoryFirmware`)
3. **Use I²C/SPI/UART transport**: DFU is only available when `GetInterfaceType()` is not `UARTRVC`
4. **Increase timeout**: DFU can take several minutes on slow links
5. **Reopen transport**: After entering bootloader, close and reopen the transport if required by your platform (see [DFU](special_feature_dfu.md))

---

## Debugging Tips

### Enable Logging

Check your communication interface implementation for logging capabilities:

```cpp
// Example: Add logging to your CommInterface implementation
int Read(uint8_t* data, uint32_t length) {
    int result = /* your read implementation */;
    if (result < 0) {
        printf("Read error: %d\n", result);
    }
    return result;
}
```

### Check Last Error

```cpp
int error = imu.GetLastError();
if (error != 0) {
    printf("Last error: %d\n", error);
    // Error codes are from SH-2 library
}
```

### Verify Sensor Status

```cpp
// Check if sensor is responding (perform hardware reset before Begin if needed)
if (!imu.Begin()) {
    printf("Initialization failed\n");
    return;
}

// Check if sensors are enabled
imu.EnableSensor(BNO085Sensor::RotationVector, 20);
// Wait a bit, then check for data; use event.rotation.accuracy for calibration (0–3)
```

### Monitor Update Frequency

```cpp
uint32_t last_update = 0;
while (true) {
    uint32_t now = xTaskGetTickCount();
    if (now - last_update > 100) {  // More than 100ms
        printf("Warning: Update() not called frequently enough!\n");
    }
    last_update = now;
    imu.Update();
    vTaskDelay(pdMS_TO_TICKS(5));
}
```

## Common Configuration Mistakes

1. **Forgetting to Enable Sensors**: Must call `EnableSensor()` for each sensor
2. **Not Calling Update()**: `Update()` must be called frequently (every 5-10ms)
3. **Wrong I²C Address**: Check ADR/SA0 pin for correct address (0x4A or 0x4B)
4. **Interface Mismatch**: PS0/PS1 pins must match selected interface
5. **Too Fast Update Rate**: Some sensors have maximum rates (check datasheet)
6. **Not Waiting for Calibration**: Orientation data unreliable until accuracy = 3

## Next Steps

- Review [Hardware Setup](hardware_setup.md) for wiring verification
- Check [Platform Integration](platform_integration.md) for communication interface issues
- See [Examples](examples.md) for working code samples
- Review [API Reference](api_reference.md) for error codes

---

**Navigation**
⬅️ [Examples](examples.md) | [Back to Index](index.md)

