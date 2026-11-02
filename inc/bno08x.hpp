#pragma once
/**
 * @file bno08x.hpp
 * @brief High-level C++ driver for the BNO08x 9-axis IMU family
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 */
#include "bno08x_comm_interface.hpp"
#include "bno08x_version.h"
#include "dfu/HcBin.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>

extern "C" {
#include "sh2/sh2.h"
#include "sh2/sh2_SensorValue.h"
#include "sh2/sh2_err.h"
}

/**
 * @brief Default compiled-in firmware image for DFU.
 *
 * Defined in `firmware-bno.c`. Contains a stub by default; replace with a
 * real BNO08x firmware binary for production DFU. Alternatively, use
 * MemoryFirmware for runtime-provided images.
 */
extern const HcBin_t firmware;

// ============================================================================
/// @defgroup SensorTypes Sensor Types and Data Structures
/// @brief Enumerations, value containers, and callback types.
/// @{
// ============================================================================

/**
 * @enum BNO085Sensor
 * @brief Identifiers for all SH-2 sensor reports supported by the BNO08x.
 *
 * Pass these to EnableSensor() / DisableSensor() / HasNewData() / GetLatest().
 * Values correspond to the SH-2 sensor ID bytes defined in the CEVA datasheet.
 */
enum class BNO085Sensor : uint8_t {
  Accelerometer = 0x01,              ///< Calibrated acceleration vector (m/s^2)
  Gyroscope = 0x02,                  ///< Calibrated angular velocity (rad/s)
  Magnetometer = 0x03,               ///< Calibrated magnetic field (uT)
  LinearAcceleration = 0x04,         ///< Acceleration minus gravity (m/s^2)
  RotationVector = 0x05,             ///< Fused absolute orientation quaternion
  Gravity = 0x06,                    ///< Gravity vector (m/s^2)
  GyroUncalibrated = 0x07,           ///< Raw angular velocity (rad/s)
  GameRotationVector = 0x08,         ///< Orientation without magnetometer heading
  GeomagneticRotationVector = 0x09,  ///< Orientation using magnetometer for heading
  Pressure = 0x0A,                   ///< Barometric pressure (hPa)
  AmbientLight = 0x0B,               ///< Ambient light level (lux)
  Humidity = 0x0C,                   ///< Relative humidity (%)
  Proximity = 0x0D,                  ///< Proximity detector (cm)
  Temperature = 0x0E,                ///< Temperature (deg C)
  MagneticFieldUncalibrated = 0x0F,  ///< Raw magnetic field (uT)
  TapDetector = 0x10,                ///< Single or double tap event
  StepCounter = 0x11,                ///< Cumulative step count
  SignificantMotion = 0x12,          ///< Significant motion detected
  StabilityClassifier = 0x13,        ///< Device stability state (on table, in hand, etc.)
  RawAccelerometer = 0x14,           ///< Raw accelerometer ADC counts
  RawGyroscope = 0x15,               ///< Raw gyroscope ADC counts
  RawMagnetometer = 0x16,            ///< Raw magnetometer ADC counts
  StepDetector = 0x18,               ///< Individual step detected
  ShakeDetector = 0x19,              ///< Shake gesture detected
  FlipDetector = 0x1A,               ///< Flip gesture detected
  PickupDetector = 0x1B,             ///< Pickup gesture detected
  StabilityDetector = 0x1C,          ///< Stability change detected
  PersonalActivityClassifier = 0x1E, ///< Activity classification (walking, running, etc.)
  SleepDetector = 0x1F,              ///< Sleep state detected
  TiltDetector = 0x20,               ///< Tilt event detected
  PocketDetector = 0x21,             ///< Device-in-pocket detected
  CircleDetector = 0x22,             ///< Circular motion detected
  HeartRateMonitor = 0x23,           ///< Heart rate data
  ARVRStabilizedRV = 0x28,           ///< AR/VR stabilised rotation vector
  ARVRStabilizedGameRV = 0x29,       ///< AR/VR stabilised game rotation vector
  GyroIntegratedRV = 0x2A,           ///< High-rate gyro-integrated rotation vector
  IZroMotionRequest = 0x2B,          ///< Interactive ZRO motion intent/request
  RawOpticalFlow = 0x2C,             ///< Raw optical flow report
  DeadReckoningPose = 0x2D,          ///< Dead reckoning pose estimate
  WheelEncoder = 0x2E                ///< Wheel encoder report
};

/**
 * @struct Vector3
 * @brief Three-axis vector with sensor accuracy indicator.
 *
 * Used for accelerometer, gyroscope, magnetometer, gravity, and linear
 * acceleration reports.
 */
struct Vector3 {
  float x{0};          ///< X-axis component
  float y{0};          ///< Y-axis component
  float z{0};          ///< Z-axis component
  uint8_t accuracy{0}; ///< Calibration accuracy (0 = unreliable, 3 = fully calibrated)
};

/**
 * @struct Quaternion
 * @brief Unit quaternion with sensor accuracy indicator.
 *
 * Used for rotation vector, game rotation vector, geomagnetic rotation vector,
 * and gyro-integrated rotation vector reports.
 */
struct Quaternion {
  float w{1};          ///< Real (scalar) component
  float x{0};          ///< Imaginary i component
  float y{0};          ///< Imaginary j component
  float z{0};          ///< Imaginary k component
  uint8_t accuracy{0}; ///< Calibration accuracy (0 = unreliable, 3 = fully calibrated)
};

/**
 * @struct TapEvent
 * @brief Tap detector event details.
 */
struct TapEvent {
  bool doubleTap{false}; ///< True if double-tap, false if single-tap
  uint8_t direction{0};  ///< Tap direction: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
};

/**
 * @struct RawVector3
 * @brief Raw integer 3-axis sample (ADC counts) with optional metadata.
 */
struct RawVector3 {
  int16_t x{0};
  int16_t y{0};
  int16_t z{0};
  int16_t temperature{0};   ///< Used by RawGyroscope only.
  uint32_t sensorTimeUs{0}; ///< Sensor-provided timestamp where available.
};

/**
 * @struct ActivityClassifierEvent
 * @brief Personal activity classifier payload.
 */
struct ActivityClassifierEvent {
  uint8_t page{0};
  bool lastPage{false};
  uint8_t mostLikelyState{0};
  std::array<uint8_t, 10> confidence{};
};

/**
 * @struct RawOpticalFlowEvent
 * @brief Raw optical flow payload from SH2_RAW_OPTICAL_FLOW.
 */
struct RawOpticalFlowEvent {
  uint32_t timestamp{0};
  int16_t dt{0};
  int16_t dx{0};
  int16_t dy{0};
  int16_t iq{0};
  uint8_t resX{0};
  uint8_t resY{0};
  uint8_t shutter{0};
  uint8_t frameMax{0};
  uint8_t frameAvg{0};
  uint8_t frameMin{0};
  uint8_t laserOn{0};
};

/**
 * @struct DeadReckoningPoseEvent
 * @brief Dead reckoning position/orientation/velocity payload.
 */
struct DeadReckoningPoseEvent {
  uint32_t timestamp{0};
  Vector3 linearPosition{};
  Quaternion rotation{};
  Vector3 linearVelocity{};
  Vector3 angularVelocity{};
};

/**
 * @struct WheelEncoderEvent
 * @brief Wheel encoder payload.
 */
struct WheelEncoderEvent {
  uint32_t timestamp{0};
  uint8_t wheelIndex{0};
  uint8_t dataType{0};
  uint16_t data{0};
};

/**
 * @struct SensorEvent
 * @brief Container for a single decoded SH-2 sensor report.
 *
 * Which fields are populated depends on the sensor type:
 * - vector sensors fill `vector` (and `bias` for uncalibrated reports)
 * - quaternion sensors fill `rotation`
 * - scalar sensors fill `scalar`
 * - detector/classifier sensors use `detected`, `eventFlags`, and related fields
 * - advanced reports fill `rawOpticalFlow`, `deadReckoningPose`, etc.
 */
struct SensorEvent {
  BNO085Sensor sensor{BNO085Sensor::Accelerometer}; ///< Which sensor produced this event
  uint64_t timestamp{0};                            ///< Event timestamp in microseconds
  Vector3 vector{};                                 ///< 3-axis data (accel/gyro/mag/gravity/etc.)
  Vector3 bias{};                                   ///< Bias for uncalibrated gyro/mag reports.
  Vector3 angularVelocity{};                        ///< Angular velocity for GyroIntegratedRV.
  Quaternion rotation{};                            ///< Quaternion data (rotation vector reports)
  float scalar{0};                    ///< Scalar payload (pressure/light/humidity/temp/etc.)
  RawVector3 raw{};                   ///< Raw integer payload for raw IMU reports.
  uint32_t latencyUs{0};              ///< Latency for step detector/counter reports.
  uint32_t stepCount{0};              ///< Cumulative step count (StepCounter)
  uint16_t eventFlags{0};             ///< Bitfield payload for gesture detector reports.
  uint8_t classification{0};          ///< Classifier result (stability/activity, etc.).
  uint8_t sleepState{0};              ///< Sleep detector state.
  TapEvent tap{};                     ///< Tap event details (TapDetector)
  ActivityClassifierEvent activity{}; ///< Personal activity classifier payload.
  uint8_t motionIntent{0};            ///< Interactive ZRO motion intent.
  uint8_t motionRequest{0};           ///< Interactive ZRO motion request.
  RawOpticalFlowEvent rawOpticalFlow{};
  DeadReckoningPoseEvent deadReckoningPose{};
  WheelEncoderEvent wheelEncoder{};
  bool detected{false}; ///< Generic detection flag for event-like reports.
};

// ---- RVC Mode Data Types ---------------------------------------------------

/**
 * @struct RvcSensorEvent
 * @brief Raw RVC frame data in integer fixed-point format.
 *
 * Fields are extracted directly from the 19-byte RVC UART frame.
 * Yaw/pitch/roll are in 0.01-degree units; acceleration in 0.001 g units.
 * Use BNO085::decodeRvc() or the RvcCallback to get floating-point values.
 */
struct RvcSensorEvent {
  uint8_t index;         ///< Frame sequence index (0-255)
  int16_t yaw;           ///< Yaw angle (units: 0.01 degrees)
  int16_t pitch;         ///< Pitch angle (units: 0.01 degrees)
  int16_t roll;          ///< Roll angle (units: 0.01 degrees)
  int16_t acc_x;         ///< X linear acceleration (units: 0.001 g)
  int16_t acc_y;         ///< Y linear acceleration (units: 0.001 g)
  int16_t acc_z;         ///< Z linear acceleration (units: 0.001 g)
  uint8_t mi;            ///< Motion intent (0=unknown, 1=stationary, 3=in motion)
  uint8_t mr;            ///< Motion request (0=no constraint, 1=stay stationary)
  uint64_t timestamp_uS; ///< Timestamp in microseconds (set by the driver)
};

/**
 * @struct RvcSensorValue
 * @brief Decoded RVC frame data in floating-point natural units.
 *
 * Delivered to the RvcCallback registered via SetRvcCallback().
 */
struct RvcSensorValue {
  uint8_t index;         ///< Frame sequence index
  float yaw_deg;         ///< Yaw angle in degrees
  float pitch_deg;       ///< Pitch angle in degrees
  float roll_deg;        ///< Roll angle in degrees
  float acc_x_g;         ///< X linear acceleration in g
  float acc_y_g;         ///< Y linear acceleration in g
  float acc_z_g;         ///< Z linear acceleration in g
  uint8_t mi;            ///< Motion intent
  uint8_t mr;            ///< Motion request
  uint64_t timestamp_uS; ///< Timestamp in microseconds
};

// ---- Callback Types --------------------------------------------------------

/** @brief Callback invoked when a new SH-2 sensor event is received. */
using SensorCallback = std::function<void(const SensorEvent&)>;

/** @brief Callback invoked when a decoded RVC frame is available. */
using RvcCallback = std::function<void(const RvcSensorValue&)>;

/**
 * @enum BNO085DriverState
 * @brief Current top-level operating state of the driver.
 *
 * The class is a monolith but enforces mode-safe operations via this state:
 * - `Closed`         : no active session.
 * - `Sh2Active`      : SH-2 protocol session is open.
 * - `RvcActive`      : RVC UART parser is active.
 * - `DfuInProgress`  : DFU transfer is running.
 */
enum class BNO085DriverState : uint8_t { Closed = 0, Sh2Active, RvcActive, DfuInProgress };

/**
 * @struct DfuProgress
 * @brief Progress sample emitted during DFU transfers.
 */
struct DfuProgress {
  uint32_t bytesSent{0};  ///< Number of firmware bytes sent so far.
  uint32_t totalBytes{0}; ///< Total firmware bytes to send.
};

/** @brief Callback for DFU progress updates. */
using DfuProgressCallback = std::function<void(const DfuProgress&)>;

/**
 * @struct DfuOptions
 * @brief Controls DFU validation and transfer behavior.
 */
struct DfuOptions {
  bool requireFormatMatch{true};           ///< Validate FW-Format metadata.
  bool requirePartNumber{true};            ///< Validate SW-Part-Number metadata.
  const char* requiredFormat{"BNO_V1"};    ///< Required format when format check is enabled.
  const char* requiredPartNumber{nullptr}; ///< Optional exact part number match.
  uint8_t packetLenOverride{0};            ///< Optional DFU packet size override (1..64).
  DfuProgressCallback progress{};          ///< Optional per-transfer progress callback.
};

/**
 * @struct DfuMemoryImage
 * @brief Firmware image descriptor for class-aware memory DFU.
 */
struct DfuMemoryImage {
  const uint8_t* data{nullptr};        ///< Pointer to firmware bytes.
  uint32_t length{0};                  ///< Firmware length in bytes.
  const char* format{"BNO_V1"};        ///< FW-Format metadata value.
  const char* partNumber{"1000-3608"}; ///< SW-Part-Number metadata value.
  uint32_t preferredPacketLen{0};      ///< Optional preferred DFU packet size.
};

/// @} // end of SensorTypes group

// ============================================================================
/// @defgroup BNO085Driver BNO085 Driver Class
/// @brief The main driver class integrating SH-2, RVC, and DFU functionality.
/// @{
// ============================================================================

/**
 * @class BNO085
 * @brief Unified driver for the BNO08x IMU -- SH-2 mode, RVC mode, and DFU.
 *
 * @details
 * A single template class that adapts to the transport type at compile time.
 * The CommInterface::GetInterfaceType() method determines which mode of
 * operation is valid:
 *
 * | Interface Type | Valid Operations                                    |
 * |----------------|-----------------------------------------------------|
 * | I2C / SPI / UART | Begin(), Update(), EnableSensor(), Dfu(), etc.   |
 * | UARTRVC        | BeginRvc(), ServiceRvc(), CloseRvc()               |
 *
 * Calling a mode-incompatible method returns `false` or `SH2_ERR` gracefully.
 * The active mode is tracked by an internal state machine exposed via
 * GetState(). Illegal transitions are rejected with `SH2_ERR_OP_IN_PROGRESS`
 * or `SH2_ERR_BAD_PARAM`.
 *
 * Error policy:
 * - Interface/mode mismatch (e.g. SH-2 call on UARTRVC transport): `SH2_ERR_BAD_PARAM`
 * - Runtime state mismatch (e.g. enabling SH-2 sensors while not in SH-2 state):
 *   `SH2_ERR_OP_IN_PROGRESS`
 * - Query methods (`GetState`, `HasNewData`, `GetLatest`) are side-effect free
 *   with respect to `last_error_`.
 *
 * The driver automatically re-enables configured sensors after a sensor reset
 * (handled internally by the async event callback).
 *
 * @tparam CommType  Platform-specific transport. Must inherit from
 *                   `bno08x::CommInterface<CommType>`.
 *
 * @note  The CommType instance passed to the constructor must outlive this
 *        object. The driver stores a reference, not a copy.
 * @note  This class is not thread-safe. Callers must serialize access
 *        externally if used from multiple tasks/threads.
 */
template <typename CommType>
class BNO085 {
public:
  /**
   * @brief Construct the driver with a communication interface.
   * @param[in] comm  Reference to the transport (must outlive this object).
   */
  explicit BNO085(CommType& comm) noexcept : io_(comm) {
    prepareHalWrapper();
  }

  /**
   * @brief Destructor. Closes active SH-2/RVC sessions.
   */
  ~BNO085() noexcept {
    Close();
    CloseRvc();
  }

  // --------------------------------------------------------------------------
  /// @name SH-2 Mode API
  ///
  /// These methods are valid when GetInterfaceType() returns I2C, SPI, or UART.
  /// They provide full access to the BNO08x sensor hub via the SH-2 protocol.
  /// @{

  /**
   * @brief Initialise the sensor in SH-2 mode.
   *
   * Opens the communication bus, connects to the SH-2 layer, sends the
   * reinitialize command, and registers the internal sensor/event callbacks.
   *
   * @pre  Interface type must NOT be UARTRVC.
   * @return  `true` on success, `false` on failure or wrong interface type.
   */
  bool Begin() noexcept;

  /**
   * @brief Close the currently active session and release transport resources.
   *
   * If SH-2 is active, calls `sh2_close()` and closes the transport.
   * If RVC is active, closes the UART transport and stops the parser.
   * If DFU is in progress, the call is rejected and `last_error_` is set to
   * `SH2_ERR_OP_IN_PROGRESS`.
   * Safe to call multiple times.
   */
  void Close() noexcept;

  /**
   * @brief Enable periodic reporting for a sensor.
   *
   * @param[in] sensor       Which sensor to enable.
   * @param[in] interval_ms  Report interval in milliseconds (e.g. 10 = 100 Hz).
   *                         Use 0 for on-change sensors (step counter, tap, etc.).
   * @param[in] sensitivity  Change sensitivity threshold for on-change sensors.
   *                         Set to 0.0 to disable sensitivity filtering.
   * @return  `true` if the sensor was configured successfully.
   */
  bool EnableSensor(BNO085Sensor sensor, uint32_t interval_ms, float sensitivity = 0.0f) noexcept;

  /**
   * @brief Disable reporting for a sensor.
   * @param[in] sensor  Which sensor to disable.
   * @return  `true` if successful.
   */
  bool DisableSensor(BNO085Sensor sensor) noexcept;

  /**
   * @brief Register a callback for SH-2 sensor events.
   *
   * The callback is invoked from within Update() whenever the SH-2 library
   * delivers a new sensor report. Only one callback can be registered at a
   * time; calling this again replaces the previous callback.
   *
   * @param[in] cb  Callback function or lambda. Pass `{}` to clear.
   */
  void SetCallback(SensorCallback cb) noexcept;

  /**
   * @brief Check if new data is available for a sensor.
   *
   * The flag is set when a report arrives and cleared when GetLatest() is
   * called. Callback dispatch does not clear this flag, which allows mixed
   * callback + polling usage.
   *
   * Returns `false` unless the driver is currently in `Sh2Active` state.
   *
   * @param[in] sensor  Which sensor to check.
   * @return  `true` if unread data is available.
   */
  bool HasNewData(BNO085Sensor sensor) const;

  /**
   * @brief Retrieve the most recent event for a sensor.
   *
   * Returns a decoded SensorEvent populated with the latest values. Which
   * fields are meaningful depends on the sensor type.
   *
   * @param[in] sensor  Which sensor to query.
   * Calling this method clears the unread-data flag for @p sensor.
   * If the driver is not in `Sh2Active` state, returns a default event
   * and leaves cached SH-2 flags unchanged.
   *
   * @return  Copy of the latest SensorEvent.
   */
  SensorEvent GetLatest(BNO085Sensor sensor) noexcept;

  /**
   * @brief Pump the SH-2 service loop.
   *
   * Must be called frequently (every 5-10 ms or faster) to keep data flowing
   * between the host and the sensor hub. Sensor callbacks and async event
   * handlers are invoked from within this method.
   *
   * @note  No-op if Begin() has not been called or if in RVC mode.
   */
  void Update() noexcept;

  /// @}

  // --------------------------------------------------------------------------
  /// @name RVC Mode API
  ///
  /// These methods are valid when GetInterfaceType() returns UARTRVC.
  /// RVC mode provides simplified yaw/pitch/roll + acceleration data
  /// via UART at 115200 baud without the overhead of the SH-2 protocol.
  /// @{

  /**
   * @brief Register a callback for decoded RVC frames.
   *
   * The callback is invoked from within ServiceRvc() each time a valid
   * 19-byte RVC frame is received and decoded.
   *
   * @param[in] cb  Callback function or lambda. Pass `{}` to clear.
   */
  void SetRvcCallback(RvcCallback cb) noexcept;

  /**
   * @brief Initialise RVC mode.
   *
   * Opens the UART transport and starts the internal frame parser. The
   * sensor must have PS0/PS1 set for RVC mode at boot time.
   *
   * @pre  Interface type must be UARTRVC.
   * @return  `true` on success, `false` on failure or wrong interface type.
   */
  bool BeginRvc() noexcept;

  /**
   * @brief Poll for RVC frames and dispatch callbacks.
   *
   * Reads all available UART bytes, feeds them into the frame parser, and
   * invokes the RVC callback for each complete valid frame. Call this in
   * your main loop or an RTOS task.
   *
   * @note  No-op if BeginRvc() has not been called.
   */
  void ServiceRvc() noexcept;

  /**
   * @brief Stop RVC processing and close the transport.
   *
   * Convenience API for explicit RVC teardown. `Close()` can also be used
   * to close whichever mode is currently active.
   */
  void CloseRvc() noexcept;

  /// @}

  // --------------------------------------------------------------------------
  /// @name Common API
  ///
  /// These methods work regardless of the interface type.
  /// @{

  /**
   * @brief Get the last error code from the SH-2 driver.
   * @return  SH-2 error code (0 = no error). See `sh2_err.h` for values.
   */
  int GetLastError() const {
    return last_error_;
  }

  /**
   * @brief Get the current high-level driver state.
   */
  BNO085DriverState GetState() const noexcept {
    return state_;
  }

  /**
   * @brief Perform a hardware reset via the RSTN pin.
   *
   * Drives RSTN LOW for @p lowMs milliseconds, then releases it HIGH and
   * waits RESET_STARTUP_MS (94 ms) for the sensor to boot: t1 (internal
   * initialization, min 90 ms) plus t2 (internal configuration, typ 4 ms)
   * of datasheet 1000-3927 section 6.5.3, figure 6-8.
   *
   * @param[in] lowMs  Duration to hold RSTN LOW (default 2 ms; the datasheet
   *                   minimum t_nrst is 10 ns).
   * @return  `false` if the port's GpioSet() reported an error on RSTN.
   */
  bool HardwareReset(uint32_t lowMs = 2) noexcept;

  /**
   * @brief Wait after releasing NRST before the hub can be addressed, in ms.
   *
   * t1 (internal initialization, min 90 ms) + t2 (internal configuration,
   * typ 4 ms), datasheet 1000-3927 section 6.5.3, figures 6-8 and 6-9.
   */
  static constexpr uint32_t RESET_STARTUP_MS = 90 + 4;

  /**
   * @brief Drive the BOOTN pin to enter/exit DFU bootloader mode.
   * @param[in] state  `true` = drive LOW (enter bootloader), `false` = HIGH.
   * @return  `false` if the port's GpioSet() reported an error.
   */
  bool SetBootPin(bool state) noexcept;

  /**
   * @brief Drive the WAKE pin (SPI mode only).
   * @param[in] state  `true` = drive LOW (wake sensor), `false` = release.
   * @return  `false` if the port's GpioSet() reported an error.
   */
  bool SetWakePin(bool state) noexcept;

  /**
   * @brief Select the host interface by driving PS0/PS1 pins.
   *
   * This is only useful if the PS pins are connected to controllable GPIOs
   * (rare -- most boards hard-wire them).
   *
   * @param[in] iface  Desired interface.
   * @return  `false` if the port's GpioSet() reported an error on PS0 or PS1.
   */
  bool SelectInterface(BNO085Interface iface) noexcept;

  /**
   * @brief Perform a Device Firmware Update (DFU).
   *
   * The sensor must already be in bootloader mode (hold BOOTN LOW during
   * reset). The method validates the firmware image, then transfers it
   * in packets with CRC-16 and ACK/NAK retry logic.
   *
   * @pre  Interface type must NOT be UARTRVC.
   * @param[in] fw  Firmware image. Defaults to the compiled-in stub from
   *                `firmware-bno.c`. Use MemoryFirmware for runtime images.
   * @return  `SH2_OK` (0) on success, or a negative SH-2 error code.
   *
   * @see MemoryFirmware  For loading firmware from memory at runtime.
   */
  int Dfu(const HcBin_t& fw = firmware) noexcept;

  /**
   * @brief Perform DFU with explicit validation and transfer options.
   *
   * Uses the same HcBin image interface as Dfu(), but allows metadata policy,
   * packet-size override, and progress reporting customization.
   */
  int DfuWithOptions(const HcBin_t& fw, const DfuOptions& options) noexcept;

  /**
   * @brief Perform DFU from an in-memory firmware image.
   *
   * Wraps @p image in a MemoryFirmware adapter internally and executes the
   * same DFU protocol as DfuWithOptions().
   */
  int DfuFromMemory(const DfuMemoryImage& image, const DfuOptions& options = {}) noexcept;

  /**
   * @brief Convenience overload for in-memory DFU.
   */
  int DfuFromMemory(const uint8_t* data, uint32_t len, const char* partNumber = "1000-3608",
                    const DfuOptions& options = {}) noexcept;

  /**
   * @brief Enter bootloader mode using BOOTN + reset pin sequence.
   *
   * Holds BOOTN low, pulses reset, then releases BOOTN. Requires a transport
   * with BOOTN/RSTN wiring support.
   *
   * @return true on success, false if interface/state is incompatible
   *         (`SH2_ERR_BAD_PARAM` / `SH2_ERR_OP_IN_PROGRESS`), the port did not
   *         open (`SH2_ERR`) or a control pin could not be driven
   *         (`SH2_ERR_IO`; the port is closed again).
   */
  bool EnterBootloader(uint32_t resetLowMs = 10, uint32_t settleMs = 50) noexcept;

  /**
   * @brief Exit bootloader mode and reboot into application firmware.
   *
   * Ensures BOOTN is released high, pulses reset, then waits for boot.
   * Fails like EnterBootloader(), including `SH2_ERR_IO` on a GPIO error.
   */
  bool ExitBootloaderAndReboot(uint32_t resetLowMs = 2, uint32_t settleMs = 100) noexcept;

  /**
   * @brief Full class-aware DFU workflow:
   *        enter bootloader -> transfer firmware -> reboot to app.
   */
  int RunDfuFromMemory(const DfuMemoryImage& image, const DfuOptions& options = {},
                       uint32_t enterResetLowMs = 10, uint32_t enterSettleMs = 50,
                       uint32_t exitResetLowMs = 2, uint32_t exitSettleMs = 100) noexcept;

  /// @}

  // ===========================================================================
  // Driver Version
  // ===========================================================================

  /** @brief Get the compiled driver version string. */
  static constexpr const char* GetDriverVersion() noexcept {
    return HF_BNO08X_VERSION_STRING;
  }

  /** @brief Get the compiled driver major version number. */
  static constexpr uint8_t GetDriverVersionMajor() noexcept {
    return HF_BNO08X_VERSION_MAJOR;
  }

  /** @brief Get the compiled driver minor version number. */
  static constexpr uint8_t GetDriverVersionMinor() noexcept {
    return HF_BNO08X_VERSION_MINOR;
  }

  /** @brief Get the compiled driver patch version number. */
  static constexpr uint8_t GetDriverVersionPatch() noexcept {
    return HF_BNO08X_VERSION_PATCH;
  }

private:
  /** @brief Initialize HAL wrapper function pointers and comm binding. */
  void prepareHalWrapper() noexcept;

  /** @brief Drive a control pin; false if the port reported a GPIO error. */
  bool setPin(bno08x::CtrlPin pin, bool active) noexcept;

  /** @brief Convert a cached SH-2 value to the high-level SensorEvent type. */
  SensorEvent toSensorEvent(BNO085Sensor sensor, const sh2_SensorValue_t& val) const noexcept;

  // --------------------------------------------------------------------------
  /// @name SH-2 HAL Bridge
  /// @brief Adapts the CRTP CommInterface to the vendor C `sh2_Hal_t` struct.
  /// @{

  /// @cond BNO08X_INTERNAL
  // Wrapper pairing the C HAL struct with the typed CommInterface pointer.
  struct CommHal {
    sh2_Hal_t* asHal() {
      return &hal;
    }
    sh2_Hal_t hal;           ///< C HAL function-pointer struct
    CommType* comm{nullptr}; ///< Typed pointer to the user's CommInterface
  } halWrapper_{};
  /// @endcond

  static int halOpen(sh2_Hal_t* self);                                          ///< @private
  static void halClose(sh2_Hal_t* self);                                        ///< @private
  static int halRead(sh2_Hal_t* self, uint8_t* buf, unsigned len, uint32_t* t); ///< @private
  static int halWrite(sh2_Hal_t* self, uint8_t* buf, unsigned len);             ///< @private
  static uint32_t halGetTimeUs(sh2_Hal_t* self);                                ///< @private

  /// @}

  // --------------------------------------------------------------------------
  /// @name SH-2 Callback Trampolines
  /// @{

  static void sensorCallback(void* cookie, sh2_SensorEvent_t* event); ///< @private
  static void asyncCallback(void* cookie, sh2_AsyncEvent_t* event);   ///< @private

  /// @}

  // --------------------------------------------------------------------------
  /// @name Internal SH-2 Handlers
  /// @{

  /** @brief Decode a raw SH-2 event and update the latest values cache. */
  void handleSensorEvent(const sh2_SensorEvent_t* event) noexcept;
  /** @brief Handle asynchronous events (e.g. sensor reset -> re-enable sensors). */
  void handleAsyncEvent(const sh2_AsyncEvent_t* event) noexcept;
  /** @brief Send a sensor configuration command to the SH-2 library. */
  bool configure(BNO085Sensor sensor, uint32_t interval_us, float sensitivity,
                 uint32_t batch_us = 0) noexcept;

  /// @}

  // --------------------------------------------------------------------------
  /// @name RVC Frame Parser
  ///
  /// Reads raw UART bytes one at a time via io_.Read() and accumulates them
  /// into a 19-byte sliding window. When a valid frame is detected (0xAA 0xAA
  /// header + checksum match), it is decoded and dispatched to the callback.
  /// @{

  static constexpr uint8_t RVC_FRAME_LEN_ = 19; ///< RVC frame size in bytes.

  /** @brief Feed one byte into the RVC frame accumulator. */
  void rvcProcessByte(uint8_t c) noexcept;

  /** @brief Convert fixed-point RvcSensorEvent to float RvcSensorValue. */
  static void decodeRvc(RvcSensorValue* out, const RvcSensorEvent* in) noexcept;

  uint8_t rvc_frame_[RVC_FRAME_LEN_]{}; ///< Sliding frame accumulation buffer.
  uint8_t rvc_frame_len_{0};            ///< Bytes currently in the buffer.

  /// @}

  // --------------------------------------------------------------------------
  /// @name DFU Protocol Helpers
  ///
  /// Implement the BNO08x bootloader protocol: CRC-16-CCITT, big-endian
  /// encoding, send-with-ACK retry, and the full firmware transfer sequence.
  /// @{

  static void dfuWrite32be(uint8_t* buf, uint32_t value) noexcept;           ///< @private
  static bool dfuIsKnownPartNumber(const char* part) noexcept;               ///< @private
  static void dfuAppendCrc(uint8_t* packet, uint8_t len) noexcept;           ///< @private
  int dfuSend(uint8_t* dfu_buff, uint8_t* p_data, uint32_t len) noexcept;    ///< @private
  int dfuSendAppSize(uint8_t* dfu_buff, uint32_t app_size) noexcept;         ///< @private
  int dfuSendPktSize(uint8_t* dfu_buff, uint8_t packet_len) noexcept;        ///< @private
  int dfuSendPkt(uint8_t* dfu_buff, uint8_t* p_data, uint32_t len) noexcept; ///< @private

  /// @}

  // --------------------------------------------------------------------------
  /// @name Instance State
  /// @{

  CommType& io_;              ///< Reference to the user-provided transport.
  SensorCallback callback_{}; ///< Registered SH-2 sensor event callback.
  RvcCallback rvc_cb_{};      ///< Registered RVC frame callback.
  int last_error_{0};         ///< Most recent SH-2 error code.
  BNO085DriverState state_{BNO085DriverState::Closed}; ///< Current operating state.

  static constexpr std::size_t SENSOR_CACHE_SIZE_ = 0x2F;      ///< Supports IDs 0x00..0x2E.
  std::array<sh2_SensorValue_t, SENSOR_CACHE_SIZE_> latest_{}; ///< Cached latest sensor values.
  std::array<bool, SENSOR_CACHE_SIZE_> new_flag_{};            ///< Per-sensor new-data flags.
  std::array<uint32_t, SENSOR_CACHE_SIZE_>
      last_interval_{}; ///< Last configured interval per sensor (for re-enable on reset).
  std::array<float, SENSOR_CACHE_SIZE_> last_sensitivity_{}; ///< Last configured sensitivity.

  /// @}
};

/// @} // end of BNO085Driver group

// =============================================================================
// Free-function version accessor (no namespace — class is at global scope)
// =============================================================================

/** @brief Get the BNO08x driver version string (free function). */
inline const char* GetBNO08xDriverVersion() noexcept {
  return HF_BNO08X_VERSION_STRING;
}

// ============================================================================
// Template Implementation
// ============================================================================
// The template method bodies live in bno08x.cpp and are included here so
// that the compiler can instantiate them for any CommType.
#define BNO085_HEADER_INCLUDED
// NOLINTNEXTLINE(bugprone-suspicious-include) - Intentional: template implementation file
#include "../src/bno08x.ipp"
#undef BNO085_HEADER_INCLUDED
