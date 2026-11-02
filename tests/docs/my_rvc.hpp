/**
 * @file my_rvc.hpp
 * @brief The UART-RVC port shown on the documentation's RVC and Porting pages, compiled by the host
 * build.
 */
#pragma once

#include "bno08x.hpp"

#include <cstdint>

// Platform hooks: your SDK's calls. Declared only, so the docs build checks the port compiles.
bool platform_uart_open(uint32_t baud) noexcept; // 8 data bits, no parity, 1 stop bit
void platform_uart_close() noexcept;
// Non-blocking: the number of bytes copied, 0 if none.
int platform_uart_read(uint8_t* data, uint32_t len) noexcept;
bool platform_gpio_write(int gpio, bool level) noexcept; // false on a GPIO error
void platform_delay_ms(uint32_t ms) noexcept;
uint32_t platform_micros() noexcept;

// --8<-- [start:port]
class MyRvcUart : public bno08x::CommInterface<MyRvcUart> {
public:
  static constexpr int kRstnGpio = 16; // NRST, active low (optional)

  BNO085Interface GetInterfaceType() noexcept {
    return BNO085Interface::UARTRVC;
  }

  bool Open() noexcept {
    return platform_uart_open(115200);
  }
  void Close() noexcept {
    platform_uart_close();
  }

  // RVC is transmit-only on the sensor side: the driver never writes.
  int Write(const uint8_t* /*data*/, uint32_t /*length*/) noexcept {
    return 0;
  }

  // Non-blocking: whatever the UART has received, up to length bytes. ServiceRvc() calls it until
  // it returns 0.
  int Read(uint8_t* data, uint32_t length) noexcept {
    return platform_uart_read(data, length);
  }

  // No interrupt line in RVC mode.
  bool DataAvailable() noexcept {
    return true;
  }

  void Delay(uint32_t ms) noexcept {
    platform_delay_ms(ms);
  }
  uint32_t GetTimeUs() noexcept {
    return platform_micros();
  }

  bool GpioSet(bno08x::CtrlPin pin, bno08x::GpioSignal signal) noexcept {
    if (pin == bno08x::CtrlPin::RSTN) {
      return platform_gpio_write(kRstnGpio, signal != bno08x::GpioSignal::ACTIVE);
    }
    return true; // only NRST is wired
  }
};
// --8<-- [end:port]
