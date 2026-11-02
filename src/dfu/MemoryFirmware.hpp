#pragma once
#include "HcBin.h"
#include <cstdint>
#include <cstring>

/**
 * @brief HcBin_t implementation for firmware stored in memory.
 *
 * Construct with a pointer to the binary image and its length. Optional
 * strings describe the format and part number. Pass the returned handle
 * to dfu() to stream the image to the device.
 *
 * This adapter is single-session per thread: `open()` binds the most recently
 * selected MemoryFirmware instance for that thread and rejects nested opens.
 */
class MemoryFirmware {
public:
  MemoryFirmware(const uint8_t* data, uint32_t len, const char* format = "BNO_V1",
                 const char* part = "unknown", uint32_t packet_len = 0)
      : data_(data), len_(len), format_(format), part_(part), packet_len_(packet_len) {}

  /** Obtain an HcBin handle for the DFU helpers. */
  const HcBin_t& hcbin() const {
    pending() = this;
    return impl_;
  }

private:
  static int open();
  static int close();
  static const char* getMeta(const char* key);
  static uint32_t getAppLen();
  static uint32_t getPacketLen();
  static int getAppData(uint8_t* packet, uint32_t offset, uint32_t len);

  static const HcBin_t impl_;

  // Per-thread adapter state, kept in function-local statics: Apple clang emits a
  // non-weak thread-local wrapper for an inline thread_local static data member, so
  // two translation units using MemoryFirmware failed to link with duplicate symbols.
  static const MemoryFirmware*& pending() noexcept {
    static thread_local const MemoryFirmware* p = nullptr;
    return p;
  }
  static const MemoryFirmware*& active() noexcept {
    static thread_local const MemoryFirmware* a = nullptr;
    return a;
  }

  const uint8_t* data_;
  uint32_t len_;
  const char* format_;
  const char* part_;
  uint32_t packet_len_;
};

// --- Implementation ---------------------------------------------------------

inline int MemoryFirmware::open() {
  if (active() != nullptr) {
    return -1;
  }
  if (pending() == nullptr) {
    return -1;
  }
  active() = pending();
  pending() = nullptr;
  return 0;
}
inline int MemoryFirmware::close() {
  active() = nullptr;
  pending() = nullptr;
  return 0;
}
inline const char* MemoryFirmware::getMeta(const char* key) {
  if (!active())
    return nullptr;
  if (std::strcmp(key, "FW-Format") == 0)
    return active()->format_;
  if (std::strcmp(key, "SW-Part-Number") == 0)
    return active()->part_;
  return nullptr;
}
inline uint32_t MemoryFirmware::getAppLen() {
  return active() ? active()->len_ : 0;
}
inline uint32_t MemoryFirmware::getPacketLen() {
  return active() ? active()->packet_len_ : 0;
}
inline int MemoryFirmware::getAppData(uint8_t* packet, uint32_t offset, uint32_t len) {
  if (!active() || offset > active()->len_ || len > (active()->len_ - offset))
    return -1;
  std::memcpy(packet, active()->data_ + offset, len);
  return 0;
}

inline const HcBin_t MemoryFirmware::impl_ = {
    MemoryFirmware::open,      MemoryFirmware::close,        MemoryFirmware::getMeta,
    MemoryFirmware::getAppLen, MemoryFirmware::getPacketLen, MemoryFirmware::getAppData};
