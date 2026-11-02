# API guide

How to do the common jobs with the driver: start the hub, enable reports, read them, stop, and handle errors.

Every snippet on this page is compiled by the host CI from
[`tests/docs/api_tasks.cpp`](../tests/docs/api_tasks.cpp), with `Imu` standing for `BNO085<MyI2c>`. The
[API reference](api-reference.md) lists every member.

## The shape of the API

One `BNO085<Comm>` object per sensor. Its calls fall into four groups; the port's `GetInterfaceType()` decides
which groups work:

| Group | Calls | Interface |
| --- | --- | --- |
| SH-2 session | `Begin()`, `Close()`, `Update()` | I2C, SPI, UART |
| Reports | `EnableSensor()`, `DisableSensor()`, `SetCallback()`, `HasNewData()`, `GetLatest()` | I2C, SPI, UART |
| UART-RVC | `BeginRvc()`, `ServiceRvc()`, `SetRvcCallback()`, `CloseRvc()` | UARTRVC ([UART-RVC mode](rvc.md)) |
| Firmware update | `EnterBootloader()`, `Dfu()`, `DfuFromMemory()`, `RunDfuFromMemory()`, `ExitBootloaderAndReboot()` | I2C, SPI, UART ([Firmware update](dfu.md)) |
| Pins and state | `HardwareReset()`, `SetBootPin()`, `SetWakePin()`, `SelectInterface()`, `GetState()`, `GetLastError()` | any |

Calls that can fail return `bool` (or an SH-2 status `int` for firmware updates) and never throw. After a
`false`, `GetLastError()` holds the reason as an `SH2_ERR_*` code.

## Start the hub

```cpp
--8<-- "tests/docs/api_tasks.cpp:begin"
```

`Begin()` opens your port, starts the CEVA SH-2 library, waits up to 200 ms for the hub's reset message and sends
a reinitialise command. Calling it again while the session is open returns `true` and does nothing.

## Enable reports

```cpp
--8<-- "tests/docs/api_tasks.cpp:enable"
```

`EnableSensor(sensor, interval_ms, sensitivity)` takes the report period in milliseconds; 0 suits on-change
reports such as the step counter. The hub picks a rate between 0.9 and 2.1 times the one you ask for, and not every
report can run at its maximum at once. The driver remembers each interval and re-sends it when the hub reports a
reset. [Configuration](configuration.md) lists the report types and their maximum rates.

## Read reports with a callback

```cpp
--8<-- "tests/docs/api_tasks.cpp:callback"
```

The callback runs inside `Update()`, on the thread that calls it, once per report. Keep it short: copy the data
out. `SensorEvent` is one struct for every report type; the fields that matter depend on `e.sensor`:

| Report | Field | Unit |
| --- | --- | --- |
| Rotation vectors (`RotationVector`, `GameRotationVector`, `GeomagneticRotationVector`, `ARVRStabilized*`) | `rotation` (w, x, y, z, accuracy) | unit quaternion |
| `GyroIntegratedRV` | `rotation`, `angularVelocity` | quaternion, rad/s |
| `Accelerometer`, `LinearAcceleration`, `Gravity` | `vector` | m/s² |
| `Gyroscope` / `GyroUncalibrated` | `vector` (+ `bias`) | rad/s |
| `Magnetometer` / `MagneticFieldUncalibrated` | `vector` (+ `bias`) | µT |
| `RawAccelerometer`, `RawGyroscope`, `RawMagnetometer` | `raw` | ADC counts |
| `Pressure`, `AmbientLight`, `Humidity`, `Proximity`, `Temperature` | `scalar` | hPa, lux, %, cm, °C |
| `StepCounter` | `stepCount`, `latencyUs` | steps, µs |
| `TapDetector` | `tap` (doubleTap, direction) | |
| `StabilityClassifier`, `PersonalActivityClassifier` | `classification`, `activity` | |
| Gesture detectors (shake, flip, pickup, tilt, ...) | `detected`, `eventFlags` | |

## Read reports by polling

```cpp
--8<-- "tests/docs/api_tasks.cpp:poll"
```

`HasNewData()` is set when a report arrives and cleared by `GetLatest()`. The callback does not clear it, so you can
mix both styles. Outside an SH-2 session `HasNewData()` returns `false` and `GetLatest()` a default event.

## Stop reports and close

```cpp
--8<-- "tests/docs/api_tasks.cpp:disable"
```

The destructor closes an open SH-2 or RVC session too. Close the session before you power the sensor down.

## Handle errors

```cpp
--8<-- "tests/docs/api_tasks.cpp:errors"
```

| Signal | Meaning | Where to look |
| --- | --- | --- |
| `Begin()` is `false`, `SH2_ERR` | The port's `Open()` failed, or the SH-2 library could not start | Bus pins, address, power |
| `SH2_ERR_TIMEOUT` | The hub did not answer a command in time | H_INTN wiring, `Read()` contract, PS0/PS1 straps |
| `SH2_ERR_BAD_PARAM` | Wrong interface for the call (SH-2 call on UART-RVC, or the reverse), or a bad DFU image | `GetInterfaceType()` |
| `SH2_ERR_OP_IN_PROGRESS` | Wrong state: reports before `Begin()`, a call during a firmware update | `GetState()` |
| `accuracy` 0 or 1 | Calibration not settled yet | Move the device; keep it away from magnetic fields |
| Reports stop, then resume | The hub reset; the driver re-enabled your reports | Supply, NRST, ESD |

[Troubleshooting](troubleshooting.md) maps symptoms to causes.

## Control pins

`HardwareReset(lowMs)` holds NRST active for `lowMs` (default 2 ms) and then waits `RESET_STARTUP_MS` (94 ms, the
start-up time of datasheet figure 6-8). `SetBootPin(true)`
asserts BOOTN (low) for the bootloader; `SetWakePin(true)` asserts WAKE in SPI mode. Each forwards to your port's
`GpioSet()`, so a pin the board does not wire is a no-op. [Hardware](hardware.md#control-pins) has the levels.
