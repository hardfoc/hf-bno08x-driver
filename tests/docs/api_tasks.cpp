/**
 * @file api_tasks.cpp
 * @brief The API guide's task snippets, compiled by the host build (never run).
 */
#include "my_i2c.hpp"

#include <cstdio>

using Imu = BNO085<MyI2c>;

// --8<-- [start:begin]
bool start_imu(Imu& imu) {
  imu.HardwareReset(); // optional: NRST low 2 ms, then 94 ms for the hub to boot
  if (!imu.Begin()) {
    std::printf("Begin failed: %d\n", imu.GetLastError()); // SH2_ERR_* (sh2_err.h)
    return false;
  }
  return imu.GetState() == BNO085DriverState::Sh2Active;
}
// --8<-- [end:begin]

// --8<-- [start:enable]
bool enable_reports(Imu& imu) {
  bool ok = true;
  ok &= imu.EnableSensor(BNO085Sensor::GameRotationVector, 5);  // 200 Hz, no magnetometer
  ok &= imu.EnableSensor(BNO085Sensor::LinearAcceleration, 10); // 100 Hz
  ok &= imu.EnableSensor(BNO085Sensor::Gyroscope, 10);          // 100 Hz
  ok &= imu.EnableSensor(BNO085Sensor::StepCounter, 0);         // on change
  ok &= imu.EnableSensor(BNO085Sensor::TapDetector, 0);         // on change
  return ok;
}
// --8<-- [end:enable]

// --8<-- [start:callback]
void use_callbacks(Imu& imu) {
  imu.SetCallback([](const SensorEvent& e) {
    switch (e.sensor) {
    case BNO085Sensor::GameRotationVector:
      std::printf("q %.3f %.3f %.3f %.3f\n", e.rotation.w, e.rotation.x, e.rotation.y,
                  e.rotation.z);
      break;
    case BNO085Sensor::StepCounter:
      std::printf("steps %u\n", static_cast<unsigned>(e.stepCount));
      break;
    case BNO085Sensor::TapDetector:
      std::printf("%s tap\n", e.tap.doubleTap ? "double" : "single");
      break;
    default:
      break;
    }
  });
  for (;;) {
    imu.Update(); // the callback runs from here, on this thread
  }
}
// --8<-- [end:callback]

// --8<-- [start:poll]
void poll(Imu& imu) {
  imu.Update();
  if (imu.HasNewData(BNO085Sensor::LinearAcceleration)) {
    const SensorEvent e = imu.GetLatest(BNO085Sensor::LinearAcceleration); // clears the flag
    std::printf("a %.2f %.2f %.2f m/s^2 (accuracy %u)\n", e.vector.x, e.vector.y, e.vector.z,
                static_cast<unsigned>(e.vector.accuracy));
  }
}
// --8<-- [end:poll]

// --8<-- [start:disable]
void stop(Imu& imu) {
  imu.DisableSensor(BNO085Sensor::Gyroscope); // report interval 0: the hub stops it
  imu.Close();                                // sh2_close(), then the port's Close()
}
// --8<-- [end:disable]

// --8<-- [start:errors]
const char* sh2_error_name(int code) {
  switch (code) {
  case SH2_OK:
    return "ok";
  case SH2_ERR:
    return "general error (transport did not open, hub did not answer)";
  case SH2_ERR_BAD_PARAM:
    return "bad parameter, or a call for the wrong interface type";
  case SH2_ERR_OP_IN_PROGRESS:
    return "wrong driver state (not started, or another mode is active)";
  case SH2_ERR_IO:
    return "bus error";
  case SH2_ERR_HUB:
    return "the hub reported an error";
  case SH2_ERR_TIMEOUT:
    return "the hub did not answer in time";
  default:
    return "unknown";
  }
}

bool enable_or_report(Imu& imu) {
  if (!imu.EnableSensor(BNO085Sensor::RotationVector, 10)) {
    std::printf("EnableSensor: %s\n", sh2_error_name(imu.GetLastError()));
    return false;
  }
  return true;
}
// --8<-- [end:errors]
