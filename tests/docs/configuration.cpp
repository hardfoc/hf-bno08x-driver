/**
 * @file configuration.cpp
 * @brief The Configuration page's snippets, compiled by the host build (never run).
 */
#include "my_i2c.hpp"

// --8<-- [start:rates]
bool configure_reports(BNO085<MyI2c>& imu) {
  // interval_ms: period between reports. The hub picks a rate between 0.9x and 2.1x the request.
  // sensitivity: for on-change reports, the change that triggers a report; 0 disables the filter.
  return imu.EnableSensor(BNO085Sensor::RotationVector, 10)         // 100 Hz, 9-axis
         && imu.EnableSensor(BNO085Sensor::GyroIntegratedRV, 2)     // 500 Hz, up to 1 kHz
         && imu.EnableSensor(BNO085Sensor::Accelerometer, 4)        // 250 Hz, up to 500 Hz
         && imu.EnableSensor(BNO085Sensor::Magnetometer, 20)        // 50 Hz, up to 100 Hz
         && imu.EnableSensor(BNO085Sensor::StabilityClassifier, 0); // on change
}
// --8<-- [end:rates]

// --8<-- [start:pins]
void select_interface_and_reset(BNO085<MyI2c>& imu) {
  imu.SetBootPin(false); // BOOTN high: boot the application, not the bootloader
  imu.HardwareReset(10); // NRST low 10 ms; PS0, PS1, BOOTN and SA0 are sampled now
}
// --8<-- [end:pins]
