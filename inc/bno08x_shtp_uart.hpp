#pragma once
/**
 * @file bno08x_shtp_uart.hpp
 * @brief Portable framing for SHTP over UART (UART-SHTP mode, PS1 = 1, PS0 = 0).
 *
 * In UART-SHTP mode every SHTP transfer travels inside a frame (datasheet
 * 1000-3927 section 1.2.3.1, which defers to the SHTP reference manual
 * 1000-3535; byte values as in CEVA's reference UART HAL):
 *
 * | Byte(s)       | Value                                                   |
 * |---------------|---------------------------------------------------------|
 * | 0             | Flag 0x7E                                               |
 * | 1             | Protocol ID: 0x01 SHTP transfer, 0x00 control           |
 * | 2 .. n-2      | Payload; 0x7E / 0x7D sent as 0x7D, byte XOR 0x20         |
 * | n-1           | Flag 0x7E                                               |
 *
 * Before sending a frame the host asks how much the hub can accept with a
 * Buffer Status Query (control frame with no payload: 7E 00 7E); the hub
 * answers with a control frame carrying the free space as a 16-bit little-
 * endian count. The host sends a frame only when it fits, one byte at a time
 * with at least HOST_BYTE_GAP_US between bytes (section 1.2.3.1). Bytes from
 * the hub have no spacing.
 *
 * This header has no platform dependencies: a UART port encodes with
 * EncodeFrame() and feeds every received byte to FrameDecoder::Push(). The
 * ESP32 reference port is examples/esp32/main/esp32_bno08x_uart_bus.hpp.
 *
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 */
#include <cstddef>
#include <cstdint>

namespace bno08x::shtp_uart {

/// UART-SHTP line rate: 3 Mbit/s, 8 data bits, no parity, 1 stop bit (datasheet 1.2.3.1).
inline constexpr uint32_t BAUD_RATE = 3000000;
/// Minimum spacing between bytes sent from the host to the hub, in µs (datasheet 1.2.3.1).
inline constexpr uint32_t HOST_BYTE_GAP_US = 100;

inline constexpr uint8_t FLAG = 0x7E;             ///< Frame delimiter.
inline constexpr uint8_t ESCAPE = 0x7D;           ///< Escape: the next byte is XOR ESCAPE_XOR.
inline constexpr uint8_t ESCAPE_XOR = 0x20;       ///< Value XORed into an escaped byte.
inline constexpr uint8_t PROTOCOL_CONTROL = 0x00; ///< Protocol ID of control frames (BSQ / BSN).
inline constexpr uint8_t PROTOCOL_SHTP = 0x01;    ///< Protocol ID of SHTP transfers.

/// Buffer Status Query, ready to send.
inline constexpr uint8_t BUFFER_STATUS_QUERY[3] = {FLAG, PROTOCOL_CONTROL, FLAG};

/// Largest frame EncodeFrame() can produce for @p payloadLen bytes (every byte escaped).
constexpr std::size_t MaxEncodedSize(std::size_t payloadLen) noexcept {
  return 2 * payloadLen + 3;
}

/**
 * @brief Wrap one SHTP transfer (header included) in a UART-SHTP frame.
 *
 * @param[in]  shtp  The transfer, as the SH-2 library passes it to `Write()`.
 * @param[in]  len   Its length in bytes.
 * @param[out] out   Destination buffer.
 * @param[in]  cap   Size of @p out; MaxEncodedSize(len) always fits.
 * @return Number of bytes written to @p out, or 0 if they do not fit.
 */
inline std::size_t EncodeFrame(const uint8_t* shtp, std::size_t len, uint8_t* out,
                               std::size_t cap) noexcept {
  std::size_t n = 0;
  auto put = [&](uint8_t b) {
    if (n < cap) {
      out[n] = b;
    }
    ++n;
  };
  put(FLAG);
  put(PROTOCOL_SHTP);
  for (std::size_t i = 0; i < len; ++i) {
    const uint8_t b = shtp[i];
    if (b == FLAG || b == ESCAPE) {
      put(ESCAPE);
      put(static_cast<uint8_t>(b ^ ESCAPE_XOR));
    } else {
      put(b);
    }
  }
  put(FLAG);
  return n <= cap ? n : 0;
}

/**
 * @brief Byte-at-a-time decoder for frames received from the hub.
 *
 * Push() every received byte. When it returns Event::ShtpFrame, Data() and
 * Size() hold the SHTP transfer (protocol ID removed) until the next Push().
 * Event::BufferStatus updates BufferSpace(). Bytes outside a frame are
 * ignored, and a frame that overflows @p Capacity or has an unknown protocol
 * ID is reported as Event::Dropped.
 *
 * @tparam Capacity  Largest frame payload kept, protocol ID included.
 */
template <std::size_t Capacity>
class FrameDecoder {
public:
  /// What the last byte completed.
  enum class Event : uint8_t {
    None,         ///< Nothing complete yet.
    ShtpFrame,    ///< An SHTP transfer is in Data() / Size().
    BufferStatus, ///< A Buffer Status Notification updated BufferSpace().
    Dropped       ///< A frame was discarded (overflow or unknown protocol ID).
  };

  /** @brief Feed one received byte. */
  Event Push(uint8_t c) noexcept {
    switch (state_) {
    case State::Outside:
      if (c == FLAG) {
        startFrame();
      }
      return Event::None;
    case State::Escaped:
      store(static_cast<uint8_t>(c ^ ESCAPE_XOR));
      state_ = State::Inside;
      return Event::None;
    case State::Inside:
      break;
    }
    if (c == ESCAPE) {
      state_ = State::Escaped;
      return Event::None;
    }
    if (c != FLAG) {
      store(c);
      return Event::None;
    }
    // A flag closes the frame; it may also open the next one, so stay inside.
    const Event ev = finishFrame();
    startFrame();
    return ev;
  }

  /** @brief SHTP transfer of the last Event::ShtpFrame (header first). */
  const uint8_t* Data() const noexcept {
    return frame_ + 1;
  }

  /** @brief Length of the last SHTP transfer. */
  std::size_t Size() const noexcept {
    return shtpLen_;
  }

  /** @brief Free space the hub reported in its last Buffer Status Notification. */
  uint16_t BufferSpace() const noexcept {
    return bufferSpace_;
  }

  /** @brief Forget the reported buffer space (the next frame needs a new query). */
  void ClearBufferSpace() noexcept {
    bufferSpace_ = 0;
  }

  /** @brief Drop any partial frame and wait for the next flag. */
  void Reset() noexcept {
    state_ = State::Outside;
    len_ = 0;
    overflow_ = false;
    shtpLen_ = 0;
    bufferSpace_ = 0;
  }

private:
  enum class State : uint8_t { Outside, Inside, Escaped };

  void startFrame() noexcept {
    state_ = State::Inside;
    len_ = 0;
    overflow_ = false;
  }

  void store(uint8_t c) noexcept {
    if (len_ < Capacity) {
      frame_[len_++] = c;
    } else {
      overflow_ = true;
    }
  }

  Event finishFrame() noexcept {
    if (len_ == 0) {
      return Event::None; // two flags in a row: an empty frame, or a closing flag reused
    }
    if (overflow_) {
      return Event::Dropped;
    }
    if (frame_[0] == PROTOCOL_SHTP) {
      shtpLen_ = len_ - 1;
      return Event::ShtpFrame;
    }
    if (frame_[0] == PROTOCOL_CONTROL && len_ >= 3) {
      bufferSpace_ = static_cast<uint16_t>(frame_[1] | (frame_[2] << 8));
      return Event::BufferStatus;
    }
    return Event::Dropped;
  }

  static_assert(Capacity >= 3, "a frame needs room for the protocol ID and a buffer status");

  uint8_t frame_[Capacity]{};
  std::size_t len_{0};
  std::size_t shtpLen_{0};
  uint16_t bufferSpace_{0};
  bool overflow_{false};
  State state_{State::Outside};
};

} // namespace bno08x::shtp_uart
