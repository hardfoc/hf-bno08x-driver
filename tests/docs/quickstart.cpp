/**
 * @file quickstart.cpp
 * @brief The Quickstart page's example, compiled by the host build (never run).
 */
#include "my_i2c.hpp"

#include <cstdio>

// --8<-- [start:quickstart]
#include "bno08x.hpp"

int print_orientation(MyI2c& bus) {
  BNO085<MyI2c> imu(bus);

  imu.HardwareReset();         // 1. pulse NRST
  if (!imu.Begin()) {          // 2. open the bus, start SH-2
    return imu.GetLastError(); //    SH2_ERR_* from sh2_err.h
  }
  if (!imu.EnableSensor(BNO085Sensor::RotationVector, 10)) { // 3. orientation every 10 ms
    return imu.GetLastError();
  }
  for (;;) {
    imu.Update(); // 4. service the hub
    if (imu.HasNewData(BNO085Sensor::RotationVector)) {
      const Quaternion q = imu.GetLatest(BNO085Sensor::RotationVector).rotation;
      std::printf("w=%.3f x=%.3f y=%.3f z=%.3f accuracy=%u\n", q.w, q.x, q.y, q.z,
                  static_cast<unsigned>(q.accuracy));
    }
    bus.Delay(2);
  }
}
// --8<-- [end:quickstart]
