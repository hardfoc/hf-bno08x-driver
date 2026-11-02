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
    if (answerInitialize) {
      answerInitializeCommand(data, length);
    }
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

  // ---- Minimal SH-2 responder --------------------------------------------
  // SHTP transfer: 4-byte header (length LE with bit 15 = continuation, channel, sequence), then
  // the cargo. Channel 2 carries SH-2 control reports; 0xF2 is a Command Request whose command
  // 0x04 is Initialize, answered by a 0xF1 Command Response with status 0 (SH-2 Reference
  // Manual 1000-3625, Command Request / Command Response reports; the layout matches
  // CommandReq_t / CommandResp_t in src/sh2/sh2.c). That is all Begin() needs to succeed.
  void answerInitializeCommand(const uint8_t* data, uint32_t length) {
    if (length < 4 + 3 || data[2] != 2 || data[4] != 0xF2 || data[6] != 0x04) {
      return;
    }
    const uint8_t cmdSeq = data[5];
    std::vector<uint8_t> resp = {0, 0, 2, controlSeq++, 0xF1, 0, 0x04, cmdSeq, 0};
    resp.resize(4 + 16, 0); // reportId, seq, command, commandSeq, respSeq, r[11]
    resp[0] = static_cast<uint8_t>(resp.size());
    queue(resp);
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
  bool answerInitialize = false;      ///< answer the SH-2 Initialize command so Begin() succeeds
  uint8_t controlSeq = 0;             ///< SHTP sequence number of the hub's control channel
  uint8_t i2cAddress = 0x4A;          ///< address the port talks to (SA0 low)
  bool setAddressSucceeds = true;
  std::vector<uint8_t> addressLog;     ///< every SetI2cAddress() argument, in order
  std::vector<uint8_t> writeAddresses; ///< i2cAddress at each Write()

private:
  BNO085Interface type_;
};
