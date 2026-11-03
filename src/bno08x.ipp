/**
 * @file bno08x.ipp
 * @brief Template method implementations for the BNO085 driver class
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 */
#ifndef BNO085_HEADER_INCLUDED
#include "bno08x.hpp"
#endif

#include <algorithm>
#include "dfu/MemoryFirmware.hpp"

// ============================================================================
/// @name SH-2 Mode Implementation
/// @brief Full sensor hub protocol over I2C, SPI, or UART.
// ============================================================================

/**
 * @brief Initialise the sensor hub in SH-2 mode.
 *
 * Opens the communication bus, wires the SH-2 C library HAL callbacks to the
 * CRTP CommInterface, opens the SH-2 session, sends a reinitialize command,
 * and registers the internal sensor and async event handlers.
 *
 * @pre  io_.GetInterfaceType() must NOT be BNO085Interface::UARTRVC.
 * @post state_ == BNO085DriverState::Sh2Active on success.
 * @return true on success, false on failure or incompatible transport.
 */
template <typename CommType>
void BNO085<CommType>::prepareHalWrapper() noexcept {
  halWrapper_.comm = &io_;
  halWrapper_.hal.open = halOpen;
  halWrapper_.hal.close = halClose;
  halWrapper_.hal.read = halRead;
  halWrapper_.hal.write = halWrite;
  halWrapper_.hal.getTimeUs = halGetTimeUs;
}

template <typename CommType>
bool BNO085<CommType>::Begin() noexcept {
  if (state_ == BNO085DriverState::Sh2Active)
    return true;
  if (state_ != BNO085DriverState::Closed) {
    last_error_ = SH2_ERR_OP_IN_PROGRESS;
    return false;
  }
  if (io_.GetInterfaceType() == BNO085Interface::UARTRVC) {
    last_error_ = SH2_ERR_BAD_PARAM;
    return false;
  }

  // The SH-2 library has one global session; a second sh2_open() would reset the first.
  if (!acquireSh2Session()) {
    last_error_ = BNO08X_ERR_SH2_SESSION_IN_USE;
    return false;
  }

  last_error_ = 0;
  prepareHalWrapper();

  int status = sh2_open(halWrapper_.asHal(), asyncCallback, this);
  if (status != SH2_OK) {
    last_error_ = status;
    io_.Close();
    releaseSh2Session();
    return false;
  }
  status = sh2_reinitialize();
  if (status != SH2_OK) {
    last_error_ = status;
    sh2_close();
    io_.Close();
    releaseSh2Session();
    return false;
  }
  status = sh2_setSensorCallback(sensorCallback, this);
  if (status != SH2_OK) {
    last_error_ = status;
    sh2_close();
    io_.Close();
    releaseSh2Session();
    return false;
  }

  new_flag_.fill(false);
  last_interval_.fill(0);
  last_sensitivity_.fill(0);
  state_ = BNO085DriverState::Sh2Active;
  return true;
}

/**
 * @brief Enable periodic reporting for a sensor.
 *
 * Converts the millisecond interval to microseconds and forwards to the
 * SH-2 configuration API. The interval and sensitivity are cached so they
 * can be automatically re-applied after a sensor reset.
 */
template <typename CommType>
bool BNO085<CommType>::EnableSensor(BNO085Sensor sensor, uint32_t interval_ms,
                                    float sensitivity) noexcept {
  if (state_ != BNO085DriverState::Sh2Active) {
    last_error_ = SH2_ERR_OP_IN_PROGRESS;
    return false;
  }
  auto id = static_cast<uint8_t>(sensor);
  if (id >= last_interval_.size())
    return false;
  uint32_t interval_us = interval_ms * 1000;
  if (!configure(sensor, interval_us, sensitivity, 0))
    return false;
  last_interval_[id] = interval_us;
  last_sensitivity_[id] = sensitivity;
  return true;
}

/** @brief Disable reporting for a sensor by setting its interval to zero. */
template <typename CommType>
bool BNO085<CommType>::DisableSensor(BNO085Sensor sensor) noexcept {
  if (state_ != BNO085DriverState::Sh2Active) {
    last_error_ = SH2_ERR_OP_IN_PROGRESS;
    return false;
  }
  auto id = static_cast<uint8_t>(sensor);
  if (id >= last_interval_.size())
    return false;
  if (!configure(sensor, 0, 0, 0))
    return false;
  last_interval_[id] = 0;
  last_sensitivity_[id] = 0.0f;
  new_flag_[id] = false;
  return true;
}

/** @brief Register a callback invoked for every received SH-2 sensor event. */
template <typename CommType>
void BNO085<CommType>::SetCallback(SensorCallback cb) noexcept {
  callback_ = cb;
}

/** @brief Register a callback invoked for every decoded RVC frame. */
template <typename CommType>
void BNO085<CommType>::SetRvcCallback(RvcCallback cb) noexcept {
  rvc_cb_ = cb;
}

/** @brief Check the new-data flag for a sensor. */
template <typename CommType>
bool BNO085<CommType>::HasNewData(BNO085Sensor sensor) const {
  if (state_ != BNO085DriverState::Sh2Active)
    return false;
  auto id = static_cast<uint8_t>(sensor);
  if (id >= new_flag_.size())
    return false;
  return new_flag_[id];
}

/**
 * @brief Convert a decoded SH-2 value into a high-level SensorEvent.
 */
template <typename CommType>
SensorEvent BNO085<CommType>::toSensorEvent(BNO085Sensor sensor,
                                            const sh2_SensorValue_t& val) const noexcept {
  SensorEvent out{};
  out.sensor = sensor;
  out.timestamp = val.timestamp;
  out.detected = false;
  const uint8_t accuracy = val.status & 0x03;

  switch (sensor) {
  case BNO085Sensor::RawAccelerometer:
    out.raw.x = val.un.rawAccelerometer.x;
    out.raw.y = val.un.rawAccelerometer.y;
    out.raw.z = val.un.rawAccelerometer.z;
    out.raw.sensorTimeUs = val.un.rawAccelerometer.timestamp;
    break;
  case BNO085Sensor::Accelerometer:
    out.vector.x = val.un.accelerometer.x;
    out.vector.y = val.un.accelerometer.y;
    out.vector.z = val.un.accelerometer.z;
    out.vector.accuracy = accuracy;
    break;
  case BNO085Sensor::LinearAcceleration:
    out.vector.x = val.un.linearAcceleration.x;
    out.vector.y = val.un.linearAcceleration.y;
    out.vector.z = val.un.linearAcceleration.z;
    out.vector.accuracy = accuracy;
    break;
  case BNO085Sensor::Gravity:
    out.vector.x = val.un.gravity.x;
    out.vector.y = val.un.gravity.y;
    out.vector.z = val.un.gravity.z;
    out.vector.accuracy = accuracy;
    break;
  case BNO085Sensor::RawGyroscope:
    out.raw.x = val.un.rawGyroscope.x;
    out.raw.y = val.un.rawGyroscope.y;
    out.raw.z = val.un.rawGyroscope.z;
    out.raw.temperature = val.un.rawGyroscope.temperature;
    out.raw.sensorTimeUs = val.un.rawGyroscope.timestamp;
    break;
  case BNO085Sensor::Gyroscope:
    out.vector.x = val.un.gyroscope.x;
    out.vector.y = val.un.gyroscope.y;
    out.vector.z = val.un.gyroscope.z;
    out.vector.accuracy = accuracy;
    break;
  case BNO085Sensor::GyroUncalibrated:
    out.vector.x = val.un.gyroscopeUncal.x;
    out.vector.y = val.un.gyroscopeUncal.y;
    out.vector.z = val.un.gyroscopeUncal.z;
    out.vector.accuracy = accuracy;
    out.bias.x = val.un.gyroscopeUncal.biasX;
    out.bias.y = val.un.gyroscopeUncal.biasY;
    out.bias.z = val.un.gyroscopeUncal.biasZ;
    break;
  case BNO085Sensor::RawMagnetometer:
    out.raw.x = val.un.rawMagnetometer.x;
    out.raw.y = val.un.rawMagnetometer.y;
    out.raw.z = val.un.rawMagnetometer.z;
    out.raw.sensorTimeUs = val.un.rawMagnetometer.timestamp;
    break;
  case BNO085Sensor::Magnetometer:
    out.vector.x = val.un.magneticField.x;
    out.vector.y = val.un.magneticField.y;
    out.vector.z = val.un.magneticField.z;
    out.vector.accuracy = accuracy;
    break;
  case BNO085Sensor::MagneticFieldUncalibrated:
    out.vector.x = val.un.magneticFieldUncal.x;
    out.vector.y = val.un.magneticFieldUncal.y;
    out.vector.z = val.un.magneticFieldUncal.z;
    out.vector.accuracy = accuracy;
    out.bias.x = val.un.magneticFieldUncal.biasX;
    out.bias.y = val.un.magneticFieldUncal.biasY;
    out.bias.z = val.un.magneticFieldUncal.biasZ;
    break;
  case BNO085Sensor::RotationVector:
    out.rotation.w = val.un.rotationVector.real;
    out.rotation.x = val.un.rotationVector.i;
    out.rotation.y = val.un.rotationVector.j;
    out.rotation.z = val.un.rotationVector.k;
    out.rotation.accuracy = accuracy;
    break;
  case BNO085Sensor::GameRotationVector:
    out.rotation.w = val.un.gameRotationVector.real;
    out.rotation.x = val.un.gameRotationVector.i;
    out.rotation.y = val.un.gameRotationVector.j;
    out.rotation.z = val.un.gameRotationVector.k;
    out.rotation.accuracy = accuracy;
    break;
  case BNO085Sensor::GeomagneticRotationVector:
    out.rotation.w = val.un.geoMagRotationVector.real;
    out.rotation.x = val.un.geoMagRotationVector.i;
    out.rotation.y = val.un.geoMagRotationVector.j;
    out.rotation.z = val.un.geoMagRotationVector.k;
    out.rotation.accuracy = accuracy;
    break;
  case BNO085Sensor::ARVRStabilizedRV:
    out.rotation.w = val.un.arvrStabilizedRV.real;
    out.rotation.x = val.un.arvrStabilizedRV.i;
    out.rotation.y = val.un.arvrStabilizedRV.j;
    out.rotation.z = val.un.arvrStabilizedRV.k;
    out.rotation.accuracy = accuracy;
    break;
  case BNO085Sensor::ARVRStabilizedGameRV:
    out.rotation.w = val.un.arvrStabilizedGRV.real;
    out.rotation.x = val.un.arvrStabilizedGRV.i;
    out.rotation.y = val.un.arvrStabilizedGRV.j;
    out.rotation.z = val.un.arvrStabilizedGRV.k;
    out.rotation.accuracy = accuracy;
    break;
  case BNO085Sensor::GyroIntegratedRV:
    out.rotation.w = val.un.gyroIntegratedRV.real;
    out.rotation.x = val.un.gyroIntegratedRV.i;
    out.rotation.y = val.un.gyroIntegratedRV.j;
    out.rotation.z = val.un.gyroIntegratedRV.k;
    out.rotation.accuracy = accuracy;
    out.angularVelocity.x = val.un.gyroIntegratedRV.angVelX;
    out.angularVelocity.y = val.un.gyroIntegratedRV.angVelY;
    out.angularVelocity.z = val.un.gyroIntegratedRV.angVelZ;
    break;
  case BNO085Sensor::Pressure:
    out.scalar = val.un.pressure.value;
    break;
  case BNO085Sensor::AmbientLight:
    out.scalar = val.un.ambientLight.value;
    break;
  case BNO085Sensor::Humidity:
    out.scalar = val.un.humidity.value;
    break;
  case BNO085Sensor::Proximity:
    out.scalar = val.un.proximity.value;
    break;
  case BNO085Sensor::Temperature:
    out.scalar = val.un.temperature.value;
    break;
  case BNO085Sensor::StepDetector:
    out.latencyUs = val.un.stepDetector.latency;
    out.detected = true;
    out.eventFlags = 1;
    break;
  case BNO085Sensor::StepCounter:
    out.latencyUs = val.un.stepCounter.latency;
    out.stepCount = val.un.stepCounter.steps;
    break;
  case BNO085Sensor::SignificantMotion:
    out.eventFlags = val.un.sigMotion.motion;
    out.detected = out.eventFlags != 0;
    break;
  case BNO085Sensor::StabilityClassifier:
    out.classification = val.un.stabilityClassifier.classification;
    break;
  case BNO085Sensor::TapDetector:
    out.eventFlags = val.un.tapDetector.flags;
    out.tap.doubleTap = (val.un.tapDetector.flags & TAPDET_DOUBLE);
    if (val.un.tapDetector.flags & TAPDET_X)
      out.tap.direction = (val.un.tapDetector.flags & TAPDET_X_POS) ? 0 : 1;
    else if (val.un.tapDetector.flags & TAPDET_Y)
      out.tap.direction = (val.un.tapDetector.flags & TAPDET_Y_POS) ? 2 : 3;
    else if (val.un.tapDetector.flags & TAPDET_Z)
      out.tap.direction = (val.un.tapDetector.flags & TAPDET_Z_POS) ? 4 : 5;
    else
      out.tap.direction = 0;
    out.detected = val.un.tapDetector.flags & (TAPDET_X | TAPDET_Y | TAPDET_Z);
    break;
  case BNO085Sensor::ShakeDetector:
    out.eventFlags = val.un.shakeDetector.shake;
    out.detected = out.eventFlags != 0;
    break;
  case BNO085Sensor::FlipDetector:
    out.eventFlags = val.un.flipDetector.flip;
    out.detected = out.eventFlags != 0;
    break;
  case BNO085Sensor::PickupDetector:
    out.eventFlags = val.un.pickupDetector.pickup;
    out.detected = out.eventFlags != 0;
    break;
  case BNO085Sensor::StabilityDetector:
    out.eventFlags = val.un.stabilityDetector.stability;
    out.detected = out.eventFlags != 0;
    break;
  case BNO085Sensor::PersonalActivityClassifier:
    out.activity.page = val.un.personalActivityClassifier.page;
    out.activity.lastPage = val.un.personalActivityClassifier.lastPage;
    out.activity.mostLikelyState = val.un.personalActivityClassifier.mostLikelyState;
    for (std::size_t i = 0; i < out.activity.confidence.size(); ++i) {
      out.activity.confidence[i] = val.un.personalActivityClassifier.confidence[i];
    }
    out.classification = out.activity.mostLikelyState;
    break;
  case BNO085Sensor::SleepDetector:
    out.sleepState = val.un.sleepDetector.sleepState;
    out.detected = out.sleepState != 0;
    break;
  case BNO085Sensor::TiltDetector:
    out.eventFlags = val.un.tiltDetector.tilt;
    out.detected = out.eventFlags != 0;
    break;
  case BNO085Sensor::PocketDetector:
    out.eventFlags = val.un.pocketDetector.pocket;
    out.detected = out.eventFlags != 0;
    break;
  case BNO085Sensor::CircleDetector:
    out.eventFlags = val.un.circleDetector.circle;
    out.detected = out.eventFlags != 0;
    break;
  case BNO085Sensor::HeartRateMonitor:
    out.scalar = static_cast<float>(val.un.heartRateMonitor.heartRate);
    break;
  case BNO085Sensor::IZroMotionRequest:
    out.motionIntent = static_cast<uint8_t>(val.un.izroRequest.intent);
    out.motionRequest = static_cast<uint8_t>(val.un.izroRequest.request);
    break;
  case BNO085Sensor::RawOpticalFlow:
    out.rawOpticalFlow.timestamp = val.un.rawOptFlow.timestamp;
    out.rawOpticalFlow.dt = val.un.rawOptFlow.dt;
    out.rawOpticalFlow.dx = val.un.rawOptFlow.dx;
    out.rawOpticalFlow.dy = val.un.rawOptFlow.dy;
    out.rawOpticalFlow.iq = val.un.rawOptFlow.iq;
    out.rawOpticalFlow.resX = val.un.rawOptFlow.resX;
    out.rawOpticalFlow.resY = val.un.rawOptFlow.resY;
    out.rawOpticalFlow.shutter = val.un.rawOptFlow.shutter;
    out.rawOpticalFlow.frameMax = val.un.rawOptFlow.frameMax;
    out.rawOpticalFlow.frameAvg = val.un.rawOptFlow.frameAvg;
    out.rawOpticalFlow.frameMin = val.un.rawOptFlow.frameMin;
    out.rawOpticalFlow.laserOn = val.un.rawOptFlow.laserOn;
    break;
  case BNO085Sensor::DeadReckoningPose:
    out.deadReckoningPose.timestamp = val.un.deadReckoningPose.timestamp;
    out.deadReckoningPose.linearPosition.x = val.un.deadReckoningPose.linPosX;
    out.deadReckoningPose.linearPosition.y = val.un.deadReckoningPose.linPosY;
    out.deadReckoningPose.linearPosition.z = val.un.deadReckoningPose.linPosZ;
    out.deadReckoningPose.rotation.w = val.un.deadReckoningPose.real;
    out.deadReckoningPose.rotation.x = val.un.deadReckoningPose.i;
    out.deadReckoningPose.rotation.y = val.un.deadReckoningPose.j;
    out.deadReckoningPose.rotation.z = val.un.deadReckoningPose.k;
    out.deadReckoningPose.linearVelocity.x = val.un.deadReckoningPose.linVelX;
    out.deadReckoningPose.linearVelocity.y = val.un.deadReckoningPose.linVelY;
    out.deadReckoningPose.linearVelocity.z = val.un.deadReckoningPose.linVelZ;
    out.deadReckoningPose.angularVelocity.x = val.un.deadReckoningPose.angVelX;
    out.deadReckoningPose.angularVelocity.y = val.un.deadReckoningPose.angVelY;
    out.deadReckoningPose.angularVelocity.z = val.un.deadReckoningPose.angVelZ;
    break;
  case BNO085Sensor::WheelEncoder:
    out.wheelEncoder.timestamp = val.un.wheelEncoder.timestamp;
    out.wheelEncoder.wheelIndex = val.un.wheelEncoder.wheelIndex;
    out.wheelEncoder.dataType = val.un.wheelEncoder.dataType;
    out.wheelEncoder.data = val.un.wheelEncoder.data;
    break;
  default:
    break;
  }
  return out;
}

/**
 * @brief Retrieve the most recent event for a sensor and clear unread flag.
 */
template <typename CommType>
SensorEvent BNO085<CommType>::GetLatest(BNO085Sensor sensor) noexcept {
  SensorEvent out{};
  out.sensor = sensor;
  if (state_ != BNO085DriverState::Sh2Active)
    return out;
  const auto id = static_cast<uint8_t>(sensor);
  if (id >= latest_.size())
    return out;
  out = toSensorEvent(sensor, latest_[id]);
  new_flag_[id] = false;
  return out;
}

/** @brief Service the SH-2 protocol. Invokes sensor and async callbacks. */
template <typename CommType>
void BNO085<CommType>::Update() noexcept {
  if (state_ == BNO085DriverState::Sh2Active) {
    sh2_service();
  }
}

/** @brief Close the currently active session and release transport resources. */
template <typename CommType>
void BNO085<CommType>::Close() noexcept {
  if (state_ == BNO085DriverState::DfuInProgress) {
    last_error_ = SH2_ERR_OP_IN_PROGRESS;
    return;
  }
  if (state_ == BNO085DriverState::RvcActive) {
    io_.Close();
    state_ = BNO085DriverState::Closed;
    rvc_frame_len_ = 0;
    return;
  }
  if (state_ != BNO085DriverState::Sh2Active)
    return;
  sh2_close();
  io_.Close();
  releaseSh2Session();
  state_ = BNO085DriverState::Closed;
  new_flag_.fill(false);
  last_interval_.fill(0);
  last_sensitivity_.fill(0.0f);
}

/** @brief Claim the process-wide SH-2 session; false if another object holds it. */
template <typename CommType>
bool BNO085<CommType>::acquireSh2Session() noexcept {
  const void* expected = nullptr;
  return bno08x::detail::Sh2SessionOwner().compare_exchange_strong(expected, this) ||
         expected == this;
}

/** @brief Release the SH-2 session if this object holds it. */
template <typename CommType>
void BNO085<CommType>::releaseSh2Session() noexcept {
  const void* expected = this;
  bno08x::detail::Sh2SessionOwner().compare_exchange_strong(expected, nullptr);
}

// ---- SH-2 HAL callback bridges (CRTP CommInterface -> C sh2_Hal_t) ---------

/// @private Bridge: sh2_Hal_t::open -> CommInterface::Open().
template <typename CommType>
int BNO085<CommType>::halOpen(sh2_Hal_t* self) {
  auto* t = reinterpret_cast<CommHal*>(self);
  return t->comm->Open() ? SH2_OK : SH2_ERR;
}

/// @private Bridge: sh2_Hal_t::close -> CommInterface::Close().
template <typename CommType>
void BNO085<CommType>::halClose(sh2_Hal_t* self) {
  reinterpret_cast<CommHal*>(self)->comm->Close();
}

/// @private Bridge: sh2_Hal_t::read -> CommInterface::Read() + GetTimeUs().
template <typename CommType>
int BNO085<CommType>::halRead(sh2_Hal_t* self, uint8_t* buf, unsigned len, uint32_t* t) {
  auto* th = reinterpret_cast<CommHal*>(self);
  int ret = th->comm->Read(buf, len);
  *t = th->comm->GetTimeUs();
  return ret;
}

/// @private Bridge: sh2_Hal_t::write -> CommInterface::Write().
template <typename CommType>
int BNO085<CommType>::halWrite(sh2_Hal_t* self, uint8_t* buf, unsigned len) {
  return reinterpret_cast<CommHal*>(self)->comm->Write(buf, len);
}

/// @private Bridge: sh2_Hal_t::getTimeUs -> CommInterface::GetTimeUs().
template <typename CommType>
uint32_t BNO085<CommType>::halGetTimeUs(sh2_Hal_t* self) {
  return reinterpret_cast<CommHal*>(self)->comm->GetTimeUs();
}

// ---- SH-2 event callback trampolines --------------------------------------

/// @private C trampoline: routes sh2_SensorEvent_t to handleSensorEvent().
template <typename CommType>
void BNO085<CommType>::sensorCallback(void* cookie, sh2_SensorEvent_t* event) {
  static_cast<BNO085<CommType>*>(cookie)->handleSensorEvent(event);
}

/// @private C trampoline: routes sh2_AsyncEvent_t to handleAsyncEvent().
template <typename CommType>
void BNO085<CommType>::asyncCallback(void* cookie, sh2_AsyncEvent_t* event) {
  static_cast<BNO085<CommType>*>(cookie)->handleAsyncEvent(event);
}

/**
 * @brief Decode a raw SH-2 sensor event and update the cache.
 *
 * Stores the decoded value in latest_[], sets the new-data flag, and
 * optionally invokes the user callback.
 */
template <typename CommType>
void BNO085<CommType>::handleSensorEvent(const sh2_SensorEvent_t* event) noexcept {
  sh2_SensorValue_t value;
  int decode_status = sh2_decodeSensorEvent(&value, event);
  if (decode_status != SH2_OK) {
    last_error_ = decode_status;
    return;
  }
  uint8_t id = value.sensorId;
  if (id >= latest_.size())
    return;
  latest_[id] = value;
  new_flag_[id] = true;
  if (callback_) {
    callback_(toSensorEvent(static_cast<BNO085Sensor>(id), value));
  }
}

/**
 * @brief Handle asynchronous SH-2 events.
 *
 * On SH2_RESET, re-enables all previously configured sensors using the
 * cached interval and sensitivity values.
 */
template <typename CommType>
void BNO085<CommType>::handleAsyncEvent(const sh2_AsyncEvent_t* event) noexcept {
  if (event->eventId == SH2_RESET) {
    for (uint8_t id = 0; id < last_interval_.size(); ++id) {
      if (last_interval_[id])
        configure(static_cast<BNO085Sensor>(id), last_interval_[id], last_sensitivity_[id], 0);
    }
  }
}

/**
 * @brief Send a sensor configuration command to the SH-2 library.
 *
 * @param[in] sensor       Sensor to configure.
 * @param[in] interval_us Report interval in microseconds (0 = disable).
 * @param[in] sensitivity  Change sensitivity threshold.
 * @param[in] batch_us    Batch interval in microseconds (0 = no batching).
 * @return true on success, false on failure (error stored in last_error_).
 */
template <typename CommType>
bool BNO085<CommType>::configure(BNO085Sensor sensor, uint32_t interval_us, float sensitivity,
                                 uint32_t batch_us) noexcept {
  sh2_SensorConfig_t cfg{};
  cfg.reportInterval_us = interval_us;
  cfg.batchInterval_us = batch_us;
  cfg.sensorSpecific = 0;
  const float clamped_sensitivity = std::clamp(sensitivity, 0.0f, 65535.0f);
  cfg.changeSensitivity = static_cast<uint16_t>(clamped_sensitivity + 0.5f);
  cfg.changeSensitivityEnabled = clamped_sensitivity > 0.0f;
  int status = sh2_setSensorConfig(static_cast<sh2_SensorId_t>(sensor), &cfg);
  if (status != SH2_OK) {
    last_error_ = status;
    return false;
  }
  return true;
}

// ============================================================================
/// @name Pin Control
/// @brief Drive BNO08x hardware control pins via the CommInterface.
// ============================================================================

/**
 * @brief Drive one control pin through the port and return its status.
 *
 * Goes through bno08x::detail::GpioSet() so a port whose GpioSet() returns
 * `void` (the earlier contract) still compiles and counts as success.
 */
template <typename CommType>
bool BNO085<CommType>::setPin(bno08x::CtrlPin pin, bool active) noexcept {
  return bno08x::detail::GpioSet(io_, pin,
                                 active ? bno08x::GpioSignal::ACTIVE : bno08x::GpioSignal::INACTIVE);
}

/**
 * @brief Assert RSTN for @p lowMs, then release and wait for boot.
 *
 * After NRST rises the hub needs t1 (internal initialization, min 90 ms)
 * plus t2 (internal configuration, typ 4 ms) before it asserts H_INTN and
 * can be addressed (datasheet 1000-3927 section 6.5.3, figures 6-8 and 6-9).
 * The driver waits RESET_STARTUP_MS = t1 + t2.
 *
 * If asserting RSTN fails nothing else is done. If releasing it fails the
 * hub may still be held in reset, so the start-up wait is skipped.
 */
template <typename CommType>
bool BNO085<CommType>::HardwareReset(uint32_t lowMs) noexcept {
  using bno08x::CtrlPin;
  if (!setPin(CtrlPin::RSTN, true)) { // Assert reset
    return false;
  }
  io_.Delay(lowMs);
  if (!setPin(CtrlPin::RSTN, false)) { // Release reset
    return false;
  }
  io_.Delay(RESET_STARTUP_MS); // t1 + t2 (datasheet 6.5.3)
  return true;
}

/** @brief Forward BOOTN control to the CommInterface. */
template <typename CommType>
bool BNO085<CommType>::SetBootPin(bool state) noexcept {
  return setPin(bno08x::CtrlPin::BOOTN, state);
}

/** @brief Enter bootloader mode via BOOTN+reset sequence. */
template <typename CommType>
bool BNO085<CommType>::EnterBootloader(uint32_t resetLowMs, uint32_t settleMs) noexcept {
  if (io_.GetInterfaceType() == BNO085Interface::UARTRVC) {
    last_error_ = SH2_ERR_BAD_PARAM;
    return false;
  }
  if (state_ == BNO085DriverState::DfuInProgress) {
    last_error_ = SH2_ERR_OP_IN_PROGRESS;
    return false;
  }
  if (state_ != BNO085DriverState::Closed) {
    Close();
  }
  if (state_ != BNO085DriverState::Closed) {
    last_error_ = SH2_ERR_OP_IN_PROGRESS;
    return false;
  }

  if (!io_.Open()) {
    last_error_ = SH2_ERR;
    return false;
  }
  // Assert BOOTN, reset, then release BOOTN (BOOTN is sampled at reset, datasheet section 1.4).
  const bool ok = setPin(bno08x::CtrlPin::BOOTN, true) && HardwareReset(resetLowMs);
  const bool released = setPin(bno08x::CtrlPin::BOOTN, false);
  if (!ok || !released) {
    io_.Close();
    last_error_ = SH2_ERR_IO;
    return false;
  }
  if (settleMs) {
    io_.Delay(settleMs);
  }
  last_error_ = SH2_OK;
  return true;
}

/** @brief Exit bootloader mode and reboot application firmware. */
template <typename CommType>
bool BNO085<CommType>::ExitBootloaderAndReboot(uint32_t resetLowMs, uint32_t settleMs) noexcept {
  if (io_.GetInterfaceType() == BNO085Interface::UARTRVC) {
    last_error_ = SH2_ERR_BAD_PARAM;
    return false;
  }
  if (state_ == BNO085DriverState::DfuInProgress) {
    last_error_ = SH2_ERR_OP_IN_PROGRESS;
    return false;
  }
  if (state_ != BNO085DriverState::Closed) {
    Close();
  }
  if (state_ != BNO085DriverState::Closed) {
    last_error_ = SH2_ERR_OP_IN_PROGRESS;
    return false;
  }

  if (!io_.Open()) {
    last_error_ = SH2_ERR;
    return false;
  }
  // BOOTN high at reset boots the application (datasheet section 1.4).
  if (!setPin(bno08x::CtrlPin::BOOTN, false) || !HardwareReset(resetLowMs)) {
    io_.Close();
    last_error_ = SH2_ERR_IO;
    return false;
  }
  if (settleMs) {
    io_.Delay(settleMs);
  }
  last_error_ = SH2_OK;
  return true;
}

/** @brief Forward WAKE control to the CommInterface (SPI only). */
template <typename CommType>
bool BNO085<CommType>::SetWakePin(bool state) noexcept {
  return setPin(bno08x::CtrlPin::WAKE, state);
}

/**
 * @brief Drive PS0/PS1 pins to select the host interface.
 *
 * PS pins are sampled at reset. This is only useful if the pins are
 * connected to controllable GPIOs. Levels follow datasheet 1000-3927
 * figure 1-5 (Protocol selection): PS1/PS0 = 00 I2C, 01 UART-RVC,
 * 10 UART (SHTP), 11 SPI. Both pins are always driven; the result is
 * `false` if either failed.
 */
template <typename CommType>
bool BNO085<CommType>::SelectInterface(BNO085Interface iface) noexcept {
  using bno08x::CtrlPin;
  const auto bits = static_cast<uint8_t>(iface); // the enum value is the PS1:PS0 pair
  if (bits > 3) {
    return false;
  }
  const bool ps1 = setPin(CtrlPin::PS1, (bits & 0x2U) != 0);
  const bool ps0 = setPin(CtrlPin::PS0, (bits & 0x1U) != 0);
  return ps1 && ps0;
}

// ============================================================================
/// @name RVC Mode Implementation
/// @brief Reads raw UART bytes via io_.Read() and parses 19-byte RVC frames.
///
/// ### RVC Frame Format (19 bytes)
/// | Byte(s) | Field              | Units           |
/// |---------|--------------------|-----------------|
/// | 0-1     | Header (0xAA 0xAA) | --              |
/// | 2       | Sequence index     | --              |
/// | 3-4     | Yaw (LE int16)     | 0.01 degrees    |
/// | 5-6     | Pitch              | 0.01 degrees    |
/// | 7-8     | Roll               | 0.01 degrees    |
/// | 9-10    | Accel X            | 0.001 g         |
/// | 11-12   | Accel Y            | 0.001 g         |
/// | 13-14   | Accel Z            | 0.001 g         |
/// | 15      | Motion intent      | enum            |
/// | 16      | Motion request     | enum            |
/// | 17      | Reserved           | --              |
/// | 18      | Checksum           | sum(bytes 2..17)|
// ============================================================================

/**
 * @brief Open the UART transport and start the RVC frame parser.
 * @pre  io_.GetInterfaceType() must be BNO085Interface::UARTRVC.
 * @return true on success, false on failure or incompatible transport.
 */
template <typename CommType>
bool BNO085<CommType>::BeginRvc() noexcept {
  if (state_ == BNO085DriverState::RvcActive)
    return true;
  if (state_ != BNO085DriverState::Closed) {
    last_error_ = SH2_ERR_OP_IN_PROGRESS;
    return false;
  }
  if (io_.GetInterfaceType() != BNO085Interface::UARTRVC) {
    last_error_ = SH2_ERR_BAD_PARAM;
    return false;
  }
  if (!io_.Open()) {
    last_error_ = SH2_ERR;
    return false;
  }
  last_error_ = SH2_OK;
  rvc_frame_len_ = 0;
  state_ = BNO085DriverState::RvcActive;
  return true;
}

/**
 * @brief Read all available UART bytes and parse RVC frames.
 *
 * Each byte is fed into a 19-byte sliding window. When a valid frame is
 * detected (0xAA 0xAA header + matching checksum), the fields are extracted,
 * decoded to floating-point, and dispatched to the RVC callback.
 */
template <typename CommType>
void BNO085<CommType>::ServiceRvc() noexcept {
  if (state_ != BNO085DriverState::RvcActive)
    return;
  uint8_t c;
  while (io_.Read(&c, 1) == 1) {
    rvcProcessByte(c);
  }
}

/** @brief Close the UART transport and deactivate the RVC parser. */
template <typename CommType>
void BNO085<CommType>::CloseRvc() noexcept {
  if (state_ == BNO085DriverState::RvcActive) {
    io_.Close();
    state_ = BNO085DriverState::Closed;
    rvc_frame_len_ = 0;
  }
}

/**
 * @brief Feed one byte into the RVC frame accumulator.
 *
 * Uses a sliding window approach: once the buffer is full (19 bytes), each
 * new byte shifts the window left by one. When the buffer contains a valid
 * header (0xAA 0xAA) and the checksum matches, the frame is decoded and
 * the callback is invoked.
 *
 * @param[in] c  The incoming UART byte.
 */
template <typename CommType>
void BNO085<CommType>::rvcProcessByte(uint8_t c) noexcept {
  if (rvc_frame_len_ == RVC_FRAME_LEN_) {
    std::memmove(rvc_frame_, rvc_frame_ + 1, RVC_FRAME_LEN_ - 1);
    rvc_frame_[RVC_FRAME_LEN_ - 1] = c;
  } else {
    rvc_frame_[rvc_frame_len_++] = c;
  }

  if (rvc_frame_len_ == RVC_FRAME_LEN_ && rvc_frame_[0] == 0xAA && rvc_frame_[1] == 0xAA) {
    uint8_t check = 0;
    for (int i = 2; i < RVC_FRAME_LEN_ - 1; ++i)
      check += rvc_frame_[i];

    if (check == rvc_frame_[RVC_FRAME_LEN_ - 1]) {
      RvcSensorEvent event{};
      event.timestamp_uS = io_.GetTimeUs();
      event.index = rvc_frame_[2];
      event.yaw = static_cast<int16_t>((rvc_frame_[4] << 8) | rvc_frame_[3]);
      event.pitch = static_cast<int16_t>((rvc_frame_[6] << 8) | rvc_frame_[5]);
      event.roll = static_cast<int16_t>((rvc_frame_[8] << 8) | rvc_frame_[7]);
      event.acc_x = static_cast<int16_t>((rvc_frame_[10] << 8) | rvc_frame_[9]);
      event.acc_y = static_cast<int16_t>((rvc_frame_[12] << 8) | rvc_frame_[11]);
      event.acc_z = static_cast<int16_t>((rvc_frame_[14] << 8) | rvc_frame_[13]);
      event.mi = rvc_frame_[15];
      event.mr = rvc_frame_[16];

      if (rvc_cb_) {
        RvcSensorValue val;
        decodeRvc(&val, &event);
        rvc_cb_(val);
      }
      rvc_frame_len_ = 0;
    }
  }
}

/**
 * @brief Convert raw RVC fixed-point values to floating-point SI units.
 *
 * @param[out] out  Decoded values in degrees and g.
 * @param[in]  in   Raw frame values in 0.01-degree and 0.001-g units.
 */
template <typename CommType>
void BNO085<CommType>::decodeRvc(RvcSensorValue* out, const RvcSensorEvent* in) noexcept {
  out->index = in->index;
  out->yaw_deg = 0.01f * in->yaw;
  out->pitch_deg = 0.01f * in->pitch;
  out->roll_deg = 0.01f * in->roll;
  out->acc_x_g = 0.001f * in->acc_x;
  out->acc_y_g = 0.001f * in->acc_y;
  out->acc_z_g = 0.001f * in->acc_z;
  out->mi = in->mi;
  out->mr = in->mr;
  out->timestamp_uS = in->timestamp_uS;
}

// ============================================================================
/// @name DFU Implementation
/// @brief BNO08x bootloader firmware update protocol.
///
/// The DFU sequence:
/// 1. Validate firmware image (format "BNO_V1", valid part number, length).
/// 2. Open transport (same CommInterface used for SH-2).
/// 3. Send application size (4 bytes big-endian + CRC-16).
/// 4. Send packet size (1 byte + CRC-16).
/// 5. Send firmware data in packets (each with CRC-16 + ACK/NAK retry).
/// 6. Wait for flash write completion, then close.
///
/// CRC-16 uses the CCITT polynomial 0x1021 with initial value 0xFFFF.
// ============================================================================

/// @name DFU Protocol Constants
/// @{
static constexpr uint8_t DFU_ACK = 's';                 ///< Expected ACK byte from bootloader.
static constexpr uint32_t DFU_MAX_PACKET_LEN = 64;      ///< Maximum DFU packet payload size.
static constexpr uint32_t DFU_MAX_ATTEMPTS = 5;         ///< Retry count per packet.
static constexpr uint32_t DFU_DELAY_POST_US = 10000;    ///< Post-DFU flash write delay (us).
static constexpr uint32_t DFU_SEND_TIMEOUT_US = 100000; ///< Per-packet I/O timeout (us).
/// @}

template <typename CommType>
bool BNO085<CommType>::dfuIsKnownPartNumber(const char* part) noexcept {
  if (!part)
    return false;
  return std::strcmp(part, "1000-3608") == 0 || std::strcmp(part, "1000-3676") == 0 ||
         std::strcmp(part, "1000-4148") == 0 || std::strcmp(part, "1000-4563") == 0;
}

template <typename CommType>
bool BNO085<CommType>::IsPlaceholderFirmware(const HcBin_t& fw) noexcept {
  // The placeholder's functions are static to firmware-bno.c, so an HcBin_t that uses its data
  // function is the placeholder or a copy of it.
  return &fw == &firmware || (fw.getAppData != nullptr && fw.getAppData == firmware.getAppData);
}

/**
 * @brief Check an opened firmware image before the DFU touches the bus or the SH-2 session.
 * @param[out] appLen The image's application length.
 * @return SH2_OK, or SH2_ERR_BAD_PARAM for an image that must not be sent.
 */
template <typename CommType>
int BNO085<CommType>::dfuCheckImage(const HcBin_t& fw, const DfuOptions& options,
                                    uint32_t& appLen) noexcept {
  const char* meta = nullptr;
  if (options.requireFormatMatch) {
    const char* expected_format = options.requiredFormat ? options.requiredFormat : "BNO_V1";
    meta = fw.getMeta("FW-Format");
    if (!meta || std::strcmp(meta, expected_format) != 0) {
      return SH2_ERR_BAD_PARAM;
    }
  }
  if (options.requirePartNumber) {
    meta = fw.getMeta("SW-Part-Number");
    if (!meta) {
      return SH2_ERR_BAD_PARAM;
    }
    if (options.requiredPartNumber) {
      if (std::strcmp(meta, options.requiredPartNumber) != 0) {
        return SH2_ERR_BAD_PARAM;
      }
    } else if (!dfuIsKnownPartNumber(meta)) {
      return SH2_ERR_BAD_PARAM;
    }
  }
  appLen = fw.getAppLen();
  if (appLen < 1024) { // an empty image included
    return SH2_ERR_BAD_PARAM;
  }
  return SH2_OK;
}

/** @brief Write a 32-bit value in big-endian byte order. */
template <typename CommType>
void BNO085<CommType>::dfuWrite32be(uint8_t* buf, uint32_t value) noexcept {
  *buf++ = (value >> 24) & 0xFF;
  *buf++ = (value >> 16) & 0xFF;
  *buf++ = (value >> 8) & 0xFF;
  *buf++ = (value >> 0) & 0xFF;
}

/** @brief Compute and append CRC-16-CCITT to a packet. */
template <typename CommType>
void BNO085<CommType>::dfuAppendCrc(uint8_t* packet, uint8_t len) noexcept {
  uint16_t crc = 0xFFFF;
  for (int n = 0; n < len; n++) {
    uint16_t x = static_cast<uint16_t>(packet[n]) << 8;
    for (int i = 0; i < 8; i++) {
      if ((crc ^ x) & 0x8000)
        crc = (crc << 1) ^ 0x1021;
      else
        crc = crc << 1;
      x <<= 1;
    }
  }
  packet[len] = (crc >> 8) & 0xFF;
  packet[len + 1] = crc & 0xFF;
}

/**
 * @brief Send a DFU packet with ACK/NAK retry logic.
 *
 * Writes @p p_data, then reads back a 1-byte ACK ('s'). Retries up to
 * DFU_MAX_ATTEMPTS times on timeout or NAK.
 *
 * @return SH2_OK on success, or a negative SH-2 error code.
 */
template <typename CommType>
int BNO085<CommType>::dfuSend(uint8_t* p_data, uint32_t len) noexcept {
  unsigned int retries = 0;
  int status = SH2_OK;
  uint8_t ack = 0;
  bool got_ack = false;
  uint32_t t;
  sh2_Hal_t* hal = halWrapper_.asHal();

  while (!got_ack && (retries < DFU_MAX_ATTEMPTS)) {
    uint32_t now = hal->getTimeUs(hal);
    uint32_t start = now;
    status = 0;
    while ((status == 0) && ((now - start) < DFU_SEND_TIMEOUT_US)) {
      status = hal->write(hal, p_data, len);
      now = hal->getTimeUs(hal);
    }
    if (status == 0)
      status = SH2_ERR_TIMEOUT;
    if (status > 0) {
      status = 0;
      while ((status == 0) && ((now - start) < DFU_SEND_TIMEOUT_US)) {
        status = hal->read(hal, &ack, 1, &t);
        now = hal->getTimeUs(hal);
      }
      if (status == 0)
        status = SH2_ERR_TIMEOUT;
    }
    if (status > 0) {
      if (ack == DFU_ACK) {
        got_ack = true;
        status = SH2_OK;
      } else {
        got_ack = false;
        status = SH2_ERR_HUB;
      }
    }
    if (!got_ack)
      retries++;
  }
  return (status >= 0) ? SH2_OK : status;
}

/** @brief Send the 4-byte application size (big-endian) + CRC. */
template <typename CommType>
int BNO085<CommType>::dfuSendAppSize(uint8_t* dfu_buff, uint32_t app_size) noexcept {
  dfuWrite32be(dfu_buff, app_size);
  dfuAppendCrc(dfu_buff, 4);
  return dfuSend(dfu_buff, 6);
}

/** @brief Send the 1-byte packet size + CRC. */
template <typename CommType>
int BNO085<CommType>::dfuSendPktSize(uint8_t* dfu_buff, uint8_t packet_len) noexcept {
  dfu_buff[0] = packet_len;
  dfuAppendCrc(dfu_buff, 1);
  return dfuSend(dfu_buff, 3);
}

/** @brief Send one firmware data packet + CRC. */
template <typename CommType>
int BNO085<CommType>::dfuSendPkt(uint8_t* dfu_buff, uint8_t* p_data, uint32_t len) noexcept {
  std::memcpy(dfu_buff, p_data, len);
  dfuAppendCrc(dfu_buff, len);
  return dfuSend(dfu_buff, len + 2);
}

/**
 * @brief Switch an I2C port to the bootloader address for a DFU transfer.
 *
 * The bootloader answers at 0x28 | SA0 (datasheet 1000-3927 section 1.4).
 * Ports without the optional address hooks are left alone.
 *
 * @param[out] appAddress  The port's address before the switch.
 * @param[out] switched    True when the address was changed and must be restored.
 * @return SH2_OK, SH2_ERR_BAD_PARAM (not a BNO08x address) or SH2_ERR_IO.
 */
template <typename CommType>
int BNO085<CommType>::dfuSelectBootloaderAddress(uint8_t& appAddress, bool& switched) noexcept {
  switched = false;
  appAddress = 0;
  if constexpr (bno08x::detail::kHasI2cAddressHooks<CommType>) {
    appAddress = io_.GetI2cAddress();
    const uint8_t boot = BootloaderI2cAddress(appAddress);
    if (boot == 0) {
      return SH2_ERR_BAD_PARAM;
    }
    if (boot == appAddress) {
      return SH2_OK; // the port already talks to the bootloader
    }
    if (!io_.SetI2cAddress(boot)) {
      return SH2_ERR_IO;
    }
    switched = true;
  }
  return SH2_OK;
}

/** @brief Restore the application I2C address after a DFU transfer. */
template <typename CommType>
bool BNO085<CommType>::dfuRestoreAppAddress(uint8_t appAddress) noexcept {
  if constexpr (bno08x::detail::kHasI2cAddressHooks<CommType>) {
    return io_.SetI2cAddress(appAddress);
  } else {
    (void)appAddress;
    return true;
  }
}

/** @brief Execute DFU with default validation options. */
template <typename CommType>
int BNO085<CommType>::Dfu(const HcBin_t& fw) noexcept {
  DfuOptions options{};
  return DfuWithOptions(fw, options);
}

/** @brief Execute DFU from an in-memory image descriptor. */
template <typename CommType>
int BNO085<CommType>::DfuFromMemory(const DfuMemoryImage& image,
                                    const DfuOptions& options) noexcept {
  if (!image.data || image.length == 0) {
    last_error_ = SH2_ERR_BAD_PARAM;
    return SH2_ERR_BAD_PARAM;
  }
  MemoryFirmware memfw(image.data, image.length, image.format ? image.format : "BNO_V1",
                       image.partNumber ? image.partNumber : "unknown", image.preferredPacketLen);
  return DfuWithOptions(memfw.hcbin(), options);
}

/** @brief Convenience overload for memory DFU from raw pointers. */
template <typename CommType>
int BNO085<CommType>::DfuFromMemory(const uint8_t* data, uint32_t len, const char* partNumber,
                                    const DfuOptions& options) noexcept {
  DfuMemoryImage image{};
  image.data = data;
  image.length = len;
  image.partNumber = partNumber ? partNumber : "unknown";
  return DfuFromMemory(image, options);
}

/** @brief Full workflow: enter bootloader, transfer memory image, reboot app. */
template <typename CommType>
int BNO085<CommType>::RunDfuFromMemory(const DfuMemoryImage& image, const DfuOptions& options,
                                       uint32_t enterResetLowMs, uint32_t enterSettleMs,
                                       uint32_t exitResetLowMs, uint32_t exitSettleMs) noexcept {
  if (!EnterBootloader(enterResetLowMs, enterSettleMs))
    return last_error_;

  int status = DfuFromMemory(image, options);
  const int dfu_status = status;
  if (!ExitBootloaderAndReboot(exitResetLowMs, exitSettleMs) && status == SH2_OK) {
    // Reboot failure becomes the workflow result only if DFU transfer succeeded.
    status = last_error_;
  }

  if (dfu_status != SH2_OK) {
    return dfu_status;
  }
  return status;
}

/**
 * @brief Execute the full DFU firmware transfer with configurable policy.
 *
 * Validates firmware metadata/length, opens the bootloader transport, sends
 * app size + packet size, streams firmware data with CRC+ACK retry, and
 * optionally reports progress.
 */
template <typename CommType>
int BNO085<CommType>::DfuWithOptions(const HcBin_t& fw, const DfuOptions& options) noexcept {
  if (io_.GetInterfaceType() == BNO085Interface::UARTRVC) {
    last_error_ = SH2_ERR_BAD_PARAM;
    return SH2_ERR_BAD_PARAM;
  }
  if (state_ == BNO085DriverState::RvcActive || state_ == BNO085DriverState::DfuInProgress) {
    last_error_ = SH2_ERR_OP_IN_PROGRESS;
    return SH2_ERR_OP_IN_PROGRESS;
  }
  // Refuse a bad image (the placeholder, an empty or malformed one) before anything changes: the
  // SH-2 session keeps running and nothing goes out on the bus. The image stays open for the
  // transfer (MemoryFirmware can be opened once per hcbin() call).
  if (fw.open == nullptr || fw.close == nullptr || fw.getMeta == nullptr ||
      fw.getAppLen == nullptr || fw.getPacketLen == nullptr || fw.getAppData == nullptr ||
      IsPlaceholderFirmware(fw)) {
    last_error_ = SH2_ERR_BAD_PARAM;
    return SH2_ERR_BAD_PARAM;
  }
  if (fw.open() != 0) {
    last_error_ = SH2_ERR;
    return SH2_ERR;
  }
  {
    uint32_t checked_len = 0;
    const int image_status = dfuCheckImage(fw, options, checked_len);
    if (image_status != SH2_OK) {
      (void)fw.close();
      last_error_ = image_status;
      return image_status;
    }
  }
  if (state_ == BNO085DriverState::Sh2Active) {
    // DFU uses the same transport HAL and must not race an active SH-2 session.
    Close();
  }
  if (state_ != BNO085DriverState::Closed) {
    (void)fw.close();
    last_error_ = SH2_ERR_OP_IN_PROGRESS;
    return SH2_ERR_OP_IN_PROGRESS;
  }
  state_ = BNO085DriverState::DfuInProgress;
  prepareHalWrapper();

  int status = SH2_OK;
  uint32_t app_len = 0;
  uint8_t packet_len = 0;
  uint32_t offset = 0;
  bool fw_opened = false;
  bool hal_opened = false;
  uint8_t app_address = 0;
  bool address_switched = false;
  uint8_t dfu_buff[DFU_MAX_PACKET_LEN + 2];
  sh2_Hal_t* hal = halWrapper_.asHal();

  // Opened and checked above, before the session was closed.
  fw_opened = true;
  app_len = fw.getAppLen();

  packet_len = fw.getPacketLen();
  if (options.packetLenOverride != 0) {
    packet_len = options.packetLenOverride;
  }
  if (packet_len == 0 || packet_len > DFU_MAX_PACKET_LEN)
    packet_len = DFU_MAX_PACKET_LEN;

  if (io_.GetInterfaceType() == BNO085Interface::I2C) {
    // Datasheet section 1.4: over I2C the bootloader answers at 0x28 / 0x29, not 0x4A / 0x4B.
    status = dfuSelectBootloaderAddress(app_address, address_switched);
    if (status != SH2_OK)
      goto dfu_close;
  }

  status = hal->open(hal);
  if (status != SH2_OK)
    goto dfu_close;
  hal_opened = true;

  status = dfuSendAppSize(dfu_buff, app_len);
  if (status != SH2_OK)
    goto dfu_close;
  status = dfuSendPktSize(dfu_buff, packet_len);
  if (status != SH2_OK)
    goto dfu_close;

  if (options.progress) {
    options.progress(DfuProgress{0, app_len});
  }

  offset = 0;
  while (offset < app_len) {
    uint32_t to_send = app_len - offset;
    if (to_send > packet_len)
      to_send = packet_len;
    status = fw.getAppData(dfu_buff, offset, to_send);
    if (status != SH2_OK) {
      status = SH2_ERR;
      goto dfu_close;
    }
    status = dfuSendPkt(dfu_buff, dfu_buff, to_send);
    if (status != SH2_OK)
      goto dfu_close;
    offset += to_send;
    if (options.progress) {
      options.progress(DfuProgress{offset, app_len});
    }
  }

dfu_close:
  if (fw_opened)
    fw.close();
  if (status == SH2_OK && hal_opened) {
    uint32_t now = hal->getTimeUs(hal), start = now;
    while ((now - start) < DFU_DELAY_POST_US)
      now = hal->getTimeUs(hal);
  }
  if (hal_opened)
    hal->close(hal);
  if (address_switched && !dfuRestoreAppAddress(app_address) && status == SH2_OK)
    status = SH2_ERR_IO;

  state_ = BNO085DriverState::Closed;
  last_error_ = status;
  return status;
}
