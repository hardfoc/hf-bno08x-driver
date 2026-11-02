# Hardware Setup

![BNO08x SH-2 sensor stack and host integration](assets/bno08x-stack.svg)

This guide covers the physical connections and hardware requirements for the BNO08x sensor.

## Pin Connections

### I²C Interface (Recommended)

```
MCU              BNO08x
─────────────────────────
3.3V      ────── VIN
GND       ────── GND
SCL       ────── SCL   (with 4.7 kΩ pull-up)
SDA       ────── SDA   (with 4.7 kΩ pull-up)
GPIO      ────── INT   (optional, active-low interrupt)
GPIO      ────── NRST  (optional, active-low reset)
```

**Interface Selection**: PS0=GND, PS1=GND selects I²C mode

**I²C Address** (7-bit):
- **ADR/SA0 = VIN (HIGH)** → **0x4B** (typical default)
- ADR/SA0 = GND (LOW) → 0x4A

**Startup**: Perform a hardware reset before the first transaction; many boards need this for reliable communication. On ESP32, the examples try 0x4B first, then 0x4A if probe fails, and use a transport `Probe()` after reset to confirm the device is present.

### SPI Interface

```
MCU              BNO08x
─────────────────────────
3.3V      ────── VIN
GND       ────── GND
SCK       ────── SCL
MOSI      ────── SDA
MISO      ────── MISO
CS        ────── CS
GPIO      ────── INT   (optional)
GPIO      ────── NRST  (optional)
GPIO      ────── WAKE  (optional, SPI mode only)
```

**Interface Selection**: PS0=VIN, PS1=VIN selects SPI mode

### UART Interface (RVC Mode)

```
MCU              BNO08x
─────────────────────────
3.3V      ────── VIN
GND       ────── GND
TX        ────── SDA
RX        ────── SCL
GPIO      ────── INT   (optional)
GPIO      ────── NRST  (optional)
```

**Interface Selection**: PS1=VIN, PS0=GND selects UART RVC mode (simplified streaming; see [RVC Mode](special_feature_rvc.md)). For full SH-2 over UART use PS1=GND, PS0=VIN.

**Baud Rate**: 115200 bps (8N1)

## Pin Descriptions

| Pin | Name | Description | Required |
|-----|------|-------------|----------|
| VIN | Power | 3.3V power supply | Yes |
| GND | Ground | Ground reference | Yes |
| SCL | Clock | I²C clock (SCL) or SPI clock (SCK) or UART TX | Yes |
| SDA | Data | I²C data (SDA) or SPI data (SDA/MOSI) or UART RX | Yes |
| INT | Interrupt | Active-low interrupt output | No |
| NRST | Reset | Active-low reset input | No |
| WAKE | Wake | SPI mode wake control (active low) | No |
| PS0 | Protocol Select | Interface selection bit 0 | Yes |
| PS1 | Protocol Select | Interface selection bit 1 | Yes |
| ADR/SA0 | Address | I²C address selection | No |

## Power Requirements

- **Supply Voltage**: 3.3V (2.4V - 3.6V typical)
- **Current Consumption**: ~20-30 mA (depends on enabled sensors)
- **Power Supply**: Clean 3.3V supply with decoupling capacitors (100nF ceramic + 10µF tantalum recommended)

## Interface Selection

The BNO08x supports multiple interfaces selected via PS0 and PS1 pins:

| PS1 | PS0 | Interface |
|-----|-----|-----------|
| GND | GND | I²C |
| GND | VIN | UART |
| VIN | GND | UART (RVC mode) |
| VIN | VIN | SPI |

**Note**: Interface selection is sampled at reset. Change pins before resetting the device.

## Example Pins (ESP32-S3)

The repository’s ESP32 examples use the following default connections:

| BNO08x Pin | ESP32-S3 GPIO | Function |
|------------|---------------|----------|
| SDA        | GPIO 4        | I²C Data |
| SCL        | GPIO 5        | I²C Clock |
| INT        | GPIO 17       | Interrupt (active-low, data ready) |
| NRST       | GPIO 16       | Reset (active-low) |
| VIN        | 3.3 V         | Supply |
| GND        | GND           | Ground |

You can change pins in code via the transport configuration (e.g. `Esp32Bno08xBus::I2CConfig`). If your transport supports it, call `HardwareReset(2, 200)` (or equivalent) before probing or calling `Begin()`.

## Physical Layout Recommendations

- Keep I²C/SPI traces short (< 10cm recommended)
- Use ground plane for noise reduction
- Place decoupling capacitors (100nF ceramic + 10µF tantalum) close to VIN pin
- Route clock and data lines away from noise sources
- Use appropriate pull-up resistors for I²C (4.7 kΩ typical)
- Keep sensor away from strong magnetic fields

## Example Wiring Diagram (I²C)

![BNO08x I2C wiring](assets/bno08x-wiring.svg)

```
                    BNO08x
                    ┌─────────┐
        3.3V ───────┤ VIN     │
        GND  ───────┤ GND     │
        SCL  ───────┤ SCL     │───[4.7kΩ]─── 3.3V
        SDA  ───────┤ SDA     │───[4.7kΩ]─── 3.3V
        INT  ───────┤ INT     │
        NRST ───────┤ NRST    │
        GND  ───────┤ PS0     │
        GND  ───────┤ PS1     │
        GND  ───────┤ ADR     │
                    └─────────┘
```

## Next Steps

- Verify connections with a multimeter
- Proceed to [Quick Start](quickstart.md) to test the connection
- Review [Platform Integration](platform_integration.md) for software setup

---

**Navigation**
⬅️ [Quick Start](quickstart.md) | [Next: Platform Integration ➡️](platform_integration.md) | [Back to Index](index.md)

