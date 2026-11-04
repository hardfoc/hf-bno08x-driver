/**
 * @file main.cpp
 * @brief Compiles every public header from an installed copy of the driver and links a driver
 * instance against the installed library.
 */
#include "bno08x.hpp"
#include "bno08x_comm_interface.hpp"
#include "bno08x_shtp_uart.hpp"
#include "bno08x_version.h"

class InstalledPort : public bno08x::CommInterface<InstalledPort> {
public:
  BNO085Interface GetInterfaceType() noexcept {
    return BNO085Interface::I2C;
  }
  bool Open() noexcept {
    return true;
  }
  void Close() noexcept {}
  int Write(const uint8_t* data, uint32_t length) noexcept {
    (void)data;
    return static_cast<int>(length);
  }
  int Read(uint8_t* data, uint32_t length) noexcept {
    (void)data;
    (void)length;
    return 0;
  }
  bool DataAvailable() noexcept {
    return false;
  }
  void Delay(uint32_t ms) noexcept {
    (void)ms;
  }
  uint32_t GetTimeUs() noexcept {
    return 0;
  }
  bool GpioSet(bno08x::CtrlPin pin, bno08x::GpioSignal signal) noexcept {
    (void)pin;
    (void)signal;
    return true;
  }
};

int main() {
  InstalledPort port;
  BNO085<InstalledPort> imu(port);
  // Begin() and Dfu(firmware) pull in the SH-2 library and the DFU placeholder image from the installed
  // archive.
  const bool started = imu.Begin();
  bno08x::shtp_uart::FrameDecoder<16> decoder;
  return (started ? 1 : 0) + imu.Dfu(firmware) + (GetBNO08xDriverVersion()[0] == '\0' ? 1 : 0) +
         static_cast<int>(decoder.Push(bno08x::shtp_uart::FLAG));
}
