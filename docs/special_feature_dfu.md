# DFU (Device Firmware Update)

The BNO08x driver includes support for updating sensor firmware via the Device Firmware Update (DFU) protocol. This allows you to update the sensor's firmware without removing it from your system.

## Overview

DFU is built into the BNO085 driver: use the same **CommInterface** (I²C, SPI, or UART SH-2) and call `imu.Dfu(...)` / `imu.DfuFromMemory(...)`. The sensor must be in bootloader mode (BOOTN held low during reset). **DFU is not available when the transport reports `UARTRVC`** — use I²C, SPI, or UART SH-2.

DFU allows you to:
- Update sensor firmware in the field
- Fix firmware bugs without hardware replacement
- Upgrade to newer firmware versions

## Entering Bootloader Mode

The sensor must be in bootloader mode to perform a firmware update:

1. **Hold BOOTN pin low** (active low)
2. **Reset the sensor** (toggle RSTN pin or power cycle)
3. **Release BOOTN** after reset completes
4. Sensor is now in bootloader mode

**Hardware Method (manual):**
```cpp
// If you have BOOTN and RSTN pins wired
imu.SetBootPin(true);   // BOOTN low
imu.HardwareReset(10);   // Reset for 10ms
imu.SetBootPin(false);   // Release BOOTN
```

**Class-aware helper method:**
```cpp
if (!imu.EnterBootloader(10, 50)) {
    printf("Failed to enter bootloader: %d\n", imu.GetLastError());
}
```

**Manual Method:**
- Physically hold BOOTN pin low
- Power cycle or reset the sensor
- Release BOOTN

## Firmware Image Format

Firmware must be provided as an `HcBin_t` object. The driver includes a compile-time stub:

- **`src/dfu/firmware-bno.c`** / **`src/dfu/firmware.h`**: Stub firmware for BNO08x (symbol `firmware`). For production, obtain firmware from the sensor vendor.

For firmware loaded at runtime (e.g. from flash or network), use **`MemoryFirmware`** (see [Firmware Sources](#firmware-sources) below).

## Implementation

Use your existing **CommInterface** (I²C, SPI, or UART — not UARTRVC). Enter bootloader mode, then call `imu.Dfu(fw)`.

### Step 1: Enter bootloader mode

Hold BOOTN low, reset the sensor, then release BOOTN. If your transport implements `SetBoot()` and `SetReset()`, the driver’s `SetBootPin()` and `HardwareReset()` will use them.

### Step 2: Perform firmware update

```cpp
#include "bno08x.hpp"
#include "src/dfu/firmware.h"   // Provides default 'firmware' HcBin_t

// Use your normal transport (e.g. Esp32Bno08xBus)
BNO085<Esp32Bno08xBus> imu(*transport);

// Enter bootloader first: SetBootPin(true), HardwareReset(10), SetBootPin(false)
// Then close/reopen transport if required by your platform.

int result = imu.Dfu(firmware);
if (result == SH2_OK) {
    printf("Firmware update successful!\n");
} else {
    printf("Firmware update failed: %d\n", result);
}
```

### Step 3: Runtime firmware (class-aware memory DFU)

If the image is already in memory or flash, use `DfuFromMemory`:

```cpp
#include "bno08x.hpp"

const uint8_t* fw_data = /* pointer to firmware bytes */;
uint32_t fw_size = /* size in bytes */;

DfuMemoryImage image{};
image.data = fw_data;
image.length = fw_size;
image.format = "BNO_V1";
image.partNumber = "1000-3608";

DfuOptions opts{};
opts.progress = [](const DfuProgress& p) {
    printf("DFU %lu/%lu\n", (unsigned long)p.bytesSent, (unsigned long)p.totalBytes);
};

int result = imu.DfuFromMemory(image, opts);
```

## Complete example (ESP32)

See `examples/esp32/main/dfu_example.cpp` for a full flow. Summary:

```cpp
auto transport = CreateEsp32Bno08xBus(config);
BNO085<Esp32Bno08xBus> imu(*transport);

// 1) Enter bootloader (helper)
imu.EnterBootloader(10, 50);

// 2) Run DFU
int result = imu.Dfu(firmware);   // or imu.DfuFromMemory(image, opts)

// 3) Reboot into application firmware
imu.ExitBootloaderAndReboot(2, 100);

// 4) Start normal SH-2 mode
imu.Begin();
```

## Timeout Handling

The DFU implementation includes built-in timeouts for slow communication links:
- **Default timeout**: Several minutes (varies by firmware size)
- **Automatic retries**: Built into DFU protocol
- **Progress indication**: Check return codes for status

## Error Codes

DFU returns SH-2 status codes:
- **`SH2_OK` (0)**: Update successful
- **`SH2_ERR` (non-zero)**: Update failed

Common error causes:
- Communication timeout
- Invalid firmware image
- Bootloader not responding
- Checksum mismatch

## Firmware Sources

### Compile-Time Firmware

```cpp
#include "src/dfu/firmware.h"

// Use default firmware object
imu.Dfu(firmware);
```

### Runtime Firmware (class-aware)

```cpp
const uint8_t* fw_data = load_from_flash();
size_t fw_size = get_firmware_size();

DfuMemoryImage image{fw_data, static_cast<uint32_t>(fw_size), "BNO_V1", "1000-3608", 0};
imu.DfuFromMemory(image);
```

### Network/File Firmware

```cpp
std::vector<uint8_t> fw_data = load_firmware_file("firmware.bin");
DfuOptions opts{};
opts.requirePartNumber = false; // if metadata handling is done externally
imu.DfuFromMemory(fw_data.data(), static_cast<uint32_t>(fw_data.size()), "1000-3608", opts);
```

## Best Practices

1. **Backup Current Firmware**: If possible, read current firmware before updating
2. **Verify Firmware Source**: Ensure firmware is from trusted source
3. **Test First**: Test firmware update process in development before field deployment
4. **Handle Errors**: Always check return codes and handle failures gracefully
5. **Power Stability**: Ensure stable power during update (avoid brownouts)
6. **Don't Interrupt**: Never interrupt firmware update process

## Troubleshooting

### Update Fails Immediately

- **Check Bootloader Mode**: Verify sensor is actually in bootloader mode
- **Verify Communication**: Test communication interface works
- **Check Firmware**: Ensure firmware image is valid

### Update Times Out

- **Increase Timeout**: Modify timeout values in DFU source if needed
- **Check Communication Speed**: Try slower communication speed
- **Verify Connection**: Check all connections are secure

### Sensor Doesn't Respond After Update

- **Reset Sensor**: Perform hardware reset after update
- **Reinitialize**: Call `Begin()` again after reset
- **Check Firmware**: Verify firmware is compatible with your sensor model

## Next Steps

- Review [Examples](examples.md) for more usage examples
- Check [Troubleshooting](troubleshooting.md) for common issues
- See [RVC Mode](special_feature_rvc.md) for alternative operation mode

---

**Navigation**
⬅️ [RVC Mode](special_feature_rvc.md) | [Back to Index](index.md)

