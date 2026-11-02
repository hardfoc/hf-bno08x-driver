# API Reference

Complete reference documentation for all public methods and types in the BNO08x driver.

## Source Code

- **Main Header**: [`inc/bno08x.hpp`](../inc/bno08x.hpp)
- **Communication Interface**: [`inc/bno08x_comm_interface.hpp`](../inc/bno08x_comm_interface.hpp)
- **Implementation**: [`src/bno08x.ipp`](../src/bno08x.ipp)

## Core Class

### `BNO085<CommType>`

Main driver class for interfacing with the BNO08x IMU sensors.

**Template Parameter**: `CommType` - Your communication interface implementation (must inherit from `bno08x::CommInterface<CommType>`)

**Location**: [`inc/bno08x.hpp#L144`](../inc/bno08x.hpp#L144)

**Constructor:**
```cpp
explicit BNO085(CommType& comm) noexcept;
```

**Location**: [`inc/bno08x.hpp#L150`](../inc/bno08x.hpp#L150)

## Methods

### Initialization

| Method | Signature | Location |
|--------|-----------|----------|
| `Begin()` | `bool Begin() noexcept` | [`inc/bno08x.hpp#L153`](../inc/bno08x.hpp#L153) |
| `Close()` | `void Close() noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `EnterBootloader()` | `bool EnterBootloader(uint32_t resetLowMs = 10, uint32_t settleMs = 50) noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `ExitBootloaderAndReboot()` | `bool ExitBootloaderAndReboot(uint32_t resetLowMs = 2, uint32_t settleMs = 100) noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |

`Close()` closes whichever mode is currently active (SH-2 or RVC). If DFU is in progress, it is rejected with `SH2_ERR_OP_IN_PROGRESS`.

### Sensor Control

| Method | Signature | Location |
|--------|-----------|----------|
| `EnableSensor()` | `bool EnableSensor(BNO085Sensor sensor, uint32_t interval_ms, float sensitivity = 0.0f)` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `DisableSensor()` | `bool DisableSensor(BNO085Sensor sensor)` | [`inc/bno08x.hpp#L163`](../inc/bno08x.hpp#L163) |

### Callbacks

| Method | Signature | Location |
|--------|-----------|----------|
| `SetCallback()` | `void SetCallback(SensorCallback cb)` | [`inc/bno08x.hpp#L166`](../inc/bno08x.hpp#L166) |
| `SetRvcCallback()` | `void SetRvcCallback(RvcCallback cb)` | [`inc/bno08x.hpp#L169`](../inc/bno08x.hpp#L169) |

### RVC Mode

Requires a transport whose `GetInterfaceType()` returns `BNO085Interface::UARTRVC` (e.g. UART at 115200 baud with PS1=1, PS0=0).

| Method | Signature | Location |
|--------|-----------|----------|
| `BeginRvc()` | `bool BeginRvc() noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `ServiceRvc()` | `void ServiceRvc() noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `CloseRvc()` | `void CloseRvc() noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |

`CloseRvc()` is an explicit RVC teardown convenience; `Close()` can also be used for generic mode shutdown.

### Data Access

| Method | Signature | Location |
|--------|-----------|----------|
| `HasNewData()` | `bool HasNewData(BNO085Sensor sensor) const` | [`inc/bno08x.hpp#L179`](../inc/bno08x.hpp#L179) |
| `GetLatest()` | `SensorEvent GetLatest(BNO085Sensor sensor) noexcept` | [`inc/bno08x.hpp#L181`](../inc/bno08x.hpp#L181) |

`HasNewData()` is cleared by `GetLatest()`. Callback dispatch does not clear it, which supports mixed callback + polling workflows.
Both calls return default/false unless the driver is in `Sh2Active` state.

### Main Loop

| Method | Signature | Location |
|--------|-----------|----------|
| `Update()` | `void Update()` | [`inc/bno08x.hpp#L184`](../inc/bno08x.hpp#L184) |

### Error Handling

| Method | Signature | Location |
|--------|-----------|----------|
| `GetLastError()` | `int GetLastError() const` | [`inc/bno08x.hpp#L187`](../inc/bno08x.hpp#L187) |
| `GetState()` | `BNO085DriverState GetState() const noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |

Error policy:
- Interface/mode mismatch: `SH2_ERR_BAD_PARAM`
- Runtime state mismatch: `SH2_ERR_OP_IN_PROGRESS`
- Query calls (`GetState`, `HasNewData`, `GetLatest`) do not update `GetLastError()`

Thread safety: the driver object is not internally synchronized. Use one task/thread or external locking.

### Hardware Control

| Method | Signature | Location |
|--------|-----------|----------|
| `HardwareReset()` | `void HardwareReset(uint32_t lowMs = 2) noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `SetBootPin()` | `void SetBootPin(bool state) noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `SetWakePin()` | `void SetWakePin(bool state) noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `SelectInterface()` | `void SelectInterface(BNO085Interface iface) noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |

**Note:** `SelectInterface()` only applies when PS0/PS1 pins are under software control. Some transports (e.g. ESP32) extend `HardwareReset()` with an optional second parameter for boot delay (e.g. `HardwareReset(2, 200)`).

### Firmware Update

Not available when `GetInterfaceType()` returns `UARTRVC`. Use `MemoryFirmware` for runtime-loaded firmware images.

| Method | Signature | Location |
|--------|-----------|----------|
| `Dfu()` | `int Dfu(const HcBin_t& fw = firmware) noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `DfuWithOptions()` | `int DfuWithOptions(const HcBin_t& fw, const DfuOptions& options) noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `DfuFromMemory()` | `int DfuFromMemory(const DfuMemoryImage& image, const DfuOptions& options = {}) noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `DfuFromMemory()` | `int DfuFromMemory(const uint8_t* data, uint32_t len, const char* partNumber = "1000-3608", const DfuOptions& options = {}) noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `RunDfuFromMemory()` | `int RunDfuFromMemory(const DfuMemoryImage& image, const DfuOptions& options = {}, uint32_t enterResetLowMs = 10, uint32_t enterSettleMs = 50, uint32_t exitResetLowMs = 2, uint32_t exitSettleMs = 100) noexcept` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |

## Types

### Enumerations

| Type | Values | Location |
|------|--------|----------|
| `BNO085Sensor` | `Accelerometer`, `Gyroscope`, `Magnetometer`, `LinearAcceleration`, `RotationVector`, `Gravity`, `GyroUncalibrated`, `GameRotationVector`, `GeomagneticRotationVector`, `Pressure`, `AmbientLight`, `Humidity`, `Proximity`, `Temperature`, `MagneticFieldUncalibrated`, `TapDetector`, `StepCounter`, `SignificantMotion`, `StabilityClassifier`, `RawAccelerometer`, `RawGyroscope`, `RawMagnetometer`, `StepDetector`, `ShakeDetector`, `FlipDetector`, `PickupDetector`, `StabilityDetector`, `PersonalActivityClassifier`, `SleepDetector`, `TiltDetector`, `PocketDetector`, `CircleDetector`, `HeartRateMonitor`, `ARVRStabilizedRV`, `ARVRStabilizedGameRV`, `GyroIntegratedRV`, `IZroMotionRequest`, `RawOpticalFlow`, `DeadReckoningPose`, `WheelEncoder` | [`inc/bno08x.hpp#L29`](../inc/bno08x.hpp#L29) |
| `BNO085DriverState` | `Closed`, `Sh2Active`, `RvcActive`, `DfuInProgress` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `BNO085Interface` | `I2C`, `UARTRVC`, `UART`, `SPI` | [`inc/bno08x_comm_interface.hpp`](../inc/bno08x_comm_interface.hpp) — returned by `CommInterface::GetInterfaceType()` |

### DFU Types

| Type | Description | Location |
|------|-------------|----------|
| `DfuProgress` | Byte counters emitted during transfer | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `DfuOptions` | Metadata policy, packet override, progress callback | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `DfuMemoryImage` | In-memory firmware descriptor (`data`, `length`, metadata) | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `DfuProgressCallback` | `std::function<void(const DfuProgress&)>` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |

### Structures

| Type | Description | Location |
|------|-------------|----------|
| `Vector3` | 3-axis vector with accuracy flag | [`inc/bno08x.hpp#L82`](../inc/bno08x.hpp#L82) |
| `Quaternion` | Quaternion orientation with accuracy flag | [`inc/bno08x.hpp#L93`](../inc/bno08x.hpp#L93) |
| `TapEvent` | Tap detector event information | [`inc/bno08x.hpp#L105`](../inc/bno08x.hpp#L105) |
| `SensorEvent` | Container for a single sensor report | [`inc/bno08x.hpp#L114`](../inc/bno08x.hpp#L114) |

### Type Aliases

| Type | Definition | Location |
|------|------------|----------|
| `SensorCallback` | `std::function<void(const SensorEvent&)>` | [`inc/bno08x.hpp#L125`](../inc/bno08x.hpp#L125) |
| `RvcCallback` | `std::function<void(const RvcSensorValue&)>` | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |
| `RvcSensorValue` | Struct with `yaw_deg`, `pitch_deg`, `roll_deg`, `acc_x_g`, `acc_y_g`, `acc_z_g`, etc. | [`inc/bno08x.hpp`](../inc/bno08x.hpp) |

## Communication Interface

### `CommInterface<Derived>`

CRTP-based communication interface that must be implemented for your platform.

**Location**: [`inc/bno08x_comm_interface.hpp#L44`](../inc/bno08x_comm_interface.hpp#L44)

### Required Methods

The driver calls `GetInterfaceType()` to determine whether to allow `Begin()` (SH-2), `BeginRvc()` (RVC), or `Dfu()` (not allowed for UARTRVC).

| Method | Signature | Location |
|--------|-----------|----------|
| `GetInterfaceType()` | `BNO085Interface GetInterfaceType() noexcept` | [`inc/bno08x_comm_interface.hpp`](../inc/bno08x_comm_interface.hpp) |
| `Open()` | `bool Open() noexcept` | [`inc/bno08x_comm_interface.hpp`](../inc/bno08x_comm_interface.hpp) |
| `Close()` | `void Close() noexcept` | [`inc/bno08x_comm_interface.hpp`](../inc/bno08x_comm_interface.hpp) |
| `Write()` | `int Write(const uint8_t* data, uint32_t length) noexcept` | [`inc/bno08x_comm_interface.hpp`](../inc/bno08x_comm_interface.hpp) |
| `Read()` | `int Read(uint8_t* data, uint32_t length) noexcept` | [`inc/bno08x_comm_interface.hpp`](../inc/bno08x_comm_interface.hpp) |
| `DataAvailable()` | `bool DataAvailable() noexcept` | [`inc/bno08x_comm_interface.hpp`](../inc/bno08x_comm_interface.hpp) |
| `Delay()` | `void Delay(uint32_t ms) noexcept` | [`inc/bno08x_comm_interface.hpp`](../inc/bno08x_comm_interface.hpp) |
| `GetTimeUs()` | `uint32_t GetTimeUs() noexcept` | [`inc/bno08x_comm_interface.hpp`](../inc/bno08x_comm_interface.hpp) |

### Optional Methods

| Method | Signature | Location |
|--------|-----------|----------|
| `SetReset()` | `void SetReset(bool state) noexcept` | [`inc/bno08x_comm_interface.hpp#L114`](../inc/bno08x_comm_interface.hpp#L114) |
| `SetBoot()` | `void SetBoot(bool state) noexcept` | [`inc/bno08x_comm_interface.hpp#L124`](../inc/bno08x_comm_interface.hpp#L124) |
| `SetWake()` | `void SetWake(bool state) noexcept` | [`inc/bno08x_comm_interface.hpp#L134`](../inc/bno08x_comm_interface.hpp#L134) |
| `SetPS0()` | `void SetPS0(bool state) noexcept` | [`inc/bno08x_comm_interface.hpp#L144`](../inc/bno08x_comm_interface.hpp#L144) |
| `SetPS1()` | `void SetPS1(bool state) noexcept` | [`inc/bno08x_comm_interface.hpp#L148`](../inc/bno08x_comm_interface.hpp#L148) |

---

**Navigation**
⬅️ [Configuration](configuration.md) | [Next: Examples ➡️](examples.md) | [Back to Index](index.md)

