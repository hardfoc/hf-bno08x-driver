/**
 * @file rvc.cpp
 * @brief The UART-RVC page's example, compiled by the host build (never run).
 */
#include "my_rvc.hpp"

#include <cstdio>

// --8<-- [start:rvc]
int stream_heading(MyRvcUart& uart) {
  BNO085<MyRvcUart> imu(uart);
  imu.SetRvcCallback([](const RvcSensorValue& v) {
    std::printf("#%u yaw %.2f pitch %.2f roll %.2f deg  a %.3f %.3f %.3f g\n",
                static_cast<unsigned>(v.index), v.yaw_deg, v.pitch_deg, v.roll_deg, v.acc_x_g,
                v.acc_y_g, v.acc_z_g);
  });
  if (!imu.BeginRvc()) { // opens the UART; fails unless the port reports UARTRVC
    return imu.GetLastError();
  }
  for (;;) {
    imu.ServiceRvc(); // parse what has arrived; one frame every 10 ms
    uart.Delay(5);
  }
}
// --8<-- [end:rvc]
