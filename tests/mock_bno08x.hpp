/**
 * @file mock_bno08x.hpp
 * @brief A scriptable BNO08x transport for the host unit tests.
 *
 * The mock records every control-pin change and delay in order, serves queued bytes to Read() one
 * call at a time, and reports a fixed interface type. GpioSet() can be made to fail per pin. Time
 * is virtual: Delay() advances the clock that GetTimeUs() returns, so tests can check the reset and
 * bootloader sequences exactly.
 */
#pragma once

#include "bno08x.hpp"

#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <vector>

class MockBus : public bno08x::CommInterface<MockBus> {
public:
  explicit MockBus(BNO085Interface type) noexcept : type_(type) {}

  // ---- CommInterface ------------------------------------------------------
  BNO085Interface GetInterfaceType() noexcept {
    return type_;
  }

  bool Open() noexcept {
    ++opens;
    open = openSucceeds;
    return openSucceeds;
  }

  void Close() noexcept {
    ++closes;
    open = false;
  }

  int Write(const uint8_t* data, uint32_t length) noexcept {
    written.insert(written.end(), data, data + length);
    writeAddresses.push_back(i2cAddress);
    return static_cast<int>(length);
  }

  int Read(uint8_t* data, uint32_t length) noexcept {
    uint32_t n = 0;
    while (n < length && !rx.empty()) {
      data[n++] = rx.front();
      rx.pop_front();
    }
    return static_cast<int>(n);
  }

  bool DataAvailable() noexcept {
    return !rx.empty();
  }

  void Delay(uint32_t ms) noexcept {
    nowUs += ms * 1000U;
    log.push_back("delay " + std::to_string(ms));
  }

  uint32_t GetTimeUs() noexcept {
    return nowUs += 10; // every call moves the clock, so polling loops terminate
  }

  bool GpioSet(bno08x::CtrlPin pin, bno08x::GpioSignal signal) noexcept {
    static const char* const names[] = {"RSTN", "BOOTN", "WAKE", "PS0", "PS1"};
    const bool fails = failPins.count(pin) != 0;
    log.push_back(std::string(names[static_cast<int>(pin)]) +
                  (signal == bno08x::GpioSignal::ACTIVE ? " active" : " inactive") +
                  (fails ? " FAILED" : ""));
    return !fails;
  }

  // ---- Optional I2C address hooks (used by DFU over I2C) ------------------
  uint8_t GetI2cAddress() noexcept {
    return i2cAddress;
  }

  bool SetI2cAddress(uint8_t address) noexcept {
    addressLog.push_back(address);
    if (!setAddressSucceeds) {
      return false;
    }
    i2cAddress = address;
    return true;
  }

  // ---- Test controls ------------------------------------------------------
  void queue(const std::vector<uint8_t>& bytes) {
    rx.insert(rx.end(), bytes.begin(), bytes.end());
  }

  bool openSucceeds = true;
  bool open = false;
  int opens = 0;
  int closes = 0;
  uint32_t nowUs = 0;
  std::deque<uint8_t> rx;
  std::vector<uint8_t> written;
  std::vector<std::string> log;
  std::set<bno08x::CtrlPin> failPins; ///< GpioSet() reports an error for these pins
  uint8_t i2cAddress = 0x4A;          ///< address the port talks to (SA0 low)
  bool setAddressSucceeds = true;
  std::vector<uint8_t> addressLog;     ///< every SetI2cAddress() argument, in order
  std::vector<uint8_t> writeAddresses; ///< i2cAddress at each Write()

private:
  BNO085Interface type_;
};
