/**
 * @file my_i2c.hpp
 * @brief The I2C port shown on the documentation's Quickstart and Porting pages, compiled by the
 * host build.
 *
 * Every platform call goes through the `platform_*` functions declared below; a real port replaces
 * them with its SDK (ESP-IDF, STM32 HAL, Zephyr, Linux i2c-dev). The class is what the driver
 * needs, nothing more.
 */
#pragma once

#include "bno08x.hpp"

#include <cstdint>

// Platform hooks: your SDK's calls. Declared only, so the docs build checks the port compiles.
bool platform_i2c_open(uint32_t clock_hz) noexcept;
void platform_i2c_close() noexcept;
bool platform_i2c_write(uint8_t addr7, const uint8_t* data, uint32_t len) noexcept; // START .. STOP
bool platform_i2c_read(uint8_t addr7, uint8_t* data, uint32_t len) noexcept;        // START .. STOP
bool platform_gpio_write(int gpio, bool level) noexcept; // false on a GPIO error
bool platform_gpio_read(int gpio) noexcept;
void platform_delay_ms(uint32_t ms) noexcept;
uint32_t platform_micros() noexcept;

// --8<-- [start:port]
class MyI2c : public bno08x::CommInterface<MyI2c> {
public:
  static constexpr uint8_t kAddress = 0x4A; // SA0 low; 0x4B with SA0 high
  static constexpr int kIntnGpio = 17;      // H_INTN, active low
  static constexpr int kRstnGpio = 16;      // NRST, active low
  static constexpr int kBootnGpio = 18;     // BOOTN, active low (optional, for firmware updates)

  BNO085Interface GetInterfaceType() noexcept {
    return BNO085Interface::I2C;
  }

  // Called by Begin() and the bootloader helpers, possibly more than once: initialise once.
  bool Open() noexcept {
    if (!open_) {
      open_ = platform_i2c_open(400000); // standard 100 kHz or fast 400 kHz
    }
    return open_;
  }

  void Close() noexcept {
    platform_i2c_close();
    open_ = false;
  }

  // One SHTP transfer from the SH-2 library. Return the bytes accepted, or a negative value.
  int Write(const uint8_t* data, uint32_t length) noexcept {
    return platform_i2c_write(kAddress, data, length) ? static_cast<int>(length) : -1;
  }

  // Return one complete SHTP transfer (header included), 0 when the hub has nothing to send.
  int Read(uint8_t* data, uint32_t length) noexcept {
    if (!DataAvailable()) {
      return 0;
    }
    uint8_t header[4];
    if (!platform_i2c_read(kAddress, header, sizeof(header))) {
      return -1;
    }
    uint32_t transfer = (header[0] | (header[1] << 8)) & 0x7FFFU; // bit 15: continuation flag
    if (transfer == 0) {
      return 0; // empty transfer
    }
    if (transfer > length) {
      transfer = length;
    }
    // A new read starts again at the header, so read the whole transfer.
    return platform_i2c_read(kAddress, data, transfer) ? static_cast<int>(transfer) : -1;
  }

  // H_INTN low: the hub has a transfer waiting.
  bool DataAvailable() noexcept {
    return !platform_gpio_read(kIntnGpio);
  }

  void Delay(uint32_t ms) noexcept {
    platform_delay_ms(ms);
  }
  uint32_t GetTimeUs() noexcept {
    return platform_micros();
  }

  // ACTIVE means "function asserted": low on NRST and BOOTN. PS0/PS1 are strapped to GND for I2C.
  // Return false on a GPIO error; a pin the board does not wire is not an error.
  bool GpioSet(bno08x::CtrlPin pin, bno08x::GpioSignal signal) noexcept {
    const bool active = signal == bno08x::GpioSignal::ACTIVE;
    switch (pin) {
    case bno08x::CtrlPin::RSTN:
      return platform_gpio_write(kRstnGpio, !active);
    case bno08x::CtrlPin::BOOTN:
      return platform_gpio_write(kBootnGpio, !active);
    case bno08x::CtrlPin::WAKE:
    case bno08x::CtrlPin::PS0:
    case bno08x::CtrlPin::PS1:
      break; // not wired on this board
    }
    return true;
  }

private:
  bool open_ = false;
};
// --8<-- [end:port]
