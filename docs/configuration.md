# Configuration

The settings a BNO08x needs: which reports at which rates, the pins sampled at reset, and the limits of the hub.

Every snippet here is compiled from [`tests/docs/configuration.cpp`](../tests/docs/configuration.cpp). The driver
has no configuration struct: the hub is configured per report with `EnableSensor()`, and the board with straps.

## Report rates

```cpp
--8<-- "tests/docs/configuration.cpp:rates"
```

| Argument | Meaning |
| --- | --- |
| `interval_ms` | Report period. The driver sends `interval_ms × 1000` µs; the hub runs between 0.9 and 2.1 times the requested rate |
| `sensitivity` | For on-change reports, the change that triggers a report, rounded to an integer and clamped to 0 ... 65535; 0 turns the filter off |

The driver does not expose batching or the sensor-specific configuration word: it sends a batch interval of 0
and `sensorSpecific = 0`. Use the SH-2 library's `sh2_setSensorConfig()` directly if you need them.

## Maximum rates

From the datasheet (figure 6-16). Each report can run at its maximum on its own; all of them at once cannot, so
balance high and low rates against the bus bandwidth and the hub's processing.

| Report | Maximum (Hz) |
| --- | ---: |
| `GyroIntegratedRV` | 1000 |
| `Accelerometer` | 500 |
| `RotationVector`, `GameRotationVector` (and the AR/VR stabilised versions) | 400 |
| `Gravity`, `LinearAcceleration`, `Gyroscope` | 400 |
| `Magnetometer` | 100 |
| `GeomagneticRotationVector` | 90 |

The SH-2 firmware sets the accelerometer to ±8 g and the gyroscope to ±2000 °/s.

## Report types

`BNO085Sensor` holds the 40 SH-2 report IDs the driver decodes; the values are the SH-2 sensor IDs.

| Group | Reports |
| --- | --- |
| Orientation | `RotationVector` (9-axis), `GameRotationVector` (no magnetometer, no heading drift correction), `GeomagneticRotationVector` (accelerometer + magnetometer, low power), `ARVRStabilizedRV`, `ARVRStabilizedGameRV`, `GyroIntegratedRV` |
| Motion | `Accelerometer`, `LinearAcceleration`, `Gravity`, `Gyroscope`, `Magnetometer` |
| Uncalibrated and raw | `GyroUncalibrated`, `MagneticFieldUncalibrated`, `RawAccelerometer`, `RawGyroscope`, `RawMagnetometer` |
| Environmental (external sensors on ENV_SCL / ENV_SDA) | `Pressure`, `AmbientLight`, `Humidity`, `Proximity`, `Temperature` |
| Activity and gestures | `StepCounter`, `StepDetector`, `TapDetector`, `ShakeDetector`, `FlipDetector`, `PickupDetector`, `TiltDetector`, `PocketDetector`, `CircleDetector`, `SignificantMotion`, `SleepDetector` |
| Classifiers | `StabilityClassifier`, `StabilityDetector`, `PersonalActivityClassifier` |
| Other | `HeartRateMonitor`, `IZroMotionRequest`, `RawOpticalFlow`, `DeadReckoningPose`, `WheelEncoder` |

Which reports a part supports depends on its firmware. The
[SH-2 reference manual](https://www.ceva-ip.com/wp-content/uploads/2019/10/SH-2-Reference-Manual.pdf) defines each
report.

## Pins sampled at reset

```cpp
--8<-- "tests/docs/configuration.cpp:pins"
```

| Pin | Selects | Typical strap |
| --- | --- | --- |
| PS0, PS1 | Host interface ([Hardware](hardware.md#protocol-selection)) | Both to GND for I2C |
| SA0 | I2C address bit 0: `0x4A` or `0x4B` | GND or VDDIO |
| BOOTN | Application or bootloader | 10 kΩ to VDDIO; a GPIO for firmware updates |
| CLKSEL0, CLKSEL1 | Internal oscillator, external crystal or external clock | Board-specific; UART modes need the crystal or clock |

## Accuracy

Rotation vectors and calibrated vectors carry an `accuracy` from 0 to 3: unreliable, low, medium, high. The hub
calibrates continuously; heading (`RotationVector`) needs the magnetometer calibrated, which takes a few
movements through different orientations. Use `GameRotationVector` when heading does not matter and magnetic
disturbances do.

## Where the values come from

The report IDs and units are in [`inc/bno08x.hpp`](../inc/bno08x.hpp) and the [API reference](api-reference.md);
the rates and ranges in the [datasheet](datasheet/BNO080_085-Datasheet.pdf), sections 6.6 and 6.9.
