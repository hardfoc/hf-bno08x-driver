/**
 * @file test_shtp_uart.cpp
 * @brief UART-SHTP framing (bno08x_shtp_uart.hpp): flags, protocol IDs, byte stuffing and the
 * buffer status exchange, as in datasheet 1000-3927 section 1.2.3.1 and the SHTP reference
 * manual 1000-3535 it refers to.
 */
#include "test_harness.hpp"

#include "bno08x_shtp_uart.hpp"

#include <vector>

namespace su = bno08x::shtp_uart;

namespace {

using Decoder = su::FrameDecoder<64>;

std::vector<uint8_t> encode(const std::vector<uint8_t>& shtp) {
  std::vector<uint8_t> out(su::MaxEncodedSize(shtp.size()));
  out.resize(su::EncodeFrame(shtp.data(), shtp.size(), out.data(), out.size()));
  return out;
}

// Feeds bytes until a frame completes; returns that event (None if none completed).
template <typename D>
typename D::Event feed(D& d, const std::vector<uint8_t>& bytes) {
  for (uint8_t b : bytes) {
    const auto ev = d.Push(b);
    if (ev != D::Event::None) {
      return ev;
    }
  }
  return D::Event::None;
}

} // namespace

TEST_CASE("UART-SHTP: a transfer is framed 7E 01 ... 7E") {
  const std::vector<uint8_t> shtp = {0x05, 0x00, 0x02, 0x00, 0xF9};
  const std::vector<uint8_t> expected = {0x7E, 0x01, 0x05, 0x00, 0x02, 0x00, 0xF9, 0x7E};
  CHECK(encode(shtp) == expected);
}

TEST_CASE("UART-SHTP: 0x7E and 0x7D in the payload are escaped as 7D, byte ^ 0x20") {
  const std::vector<uint8_t> shtp = {0x7E, 0x7D, 0x20};
  const std::vector<uint8_t> expected = {0x7E, 0x01, 0x7D, 0x5E, 0x7D, 0x5D, 0x20, 0x7E};
  CHECK(encode(shtp) == expected);
}

TEST_CASE("UART-SHTP: EncodeFrame() returns 0 when the frame does not fit") {
  const uint8_t shtp[] = {0x7E, 0x7E};
  uint8_t out[5];
  CHECK_EQ(su::EncodeFrame(shtp, sizeof(shtp), out, sizeof(out)), 0U);
  CHECK_EQ(su::MaxEncodedSize(sizeof(shtp)), 7U);
}

TEST_CASE("UART-SHTP: the buffer status query is 7E 00 7E") {
  CHECK_EQ(su::BUFFER_STATUS_QUERY[0], 0x7E);
  CHECK_EQ(su::BUFFER_STATUS_QUERY[1], 0x00);
  CHECK_EQ(su::BUFFER_STATUS_QUERY[2], 0x7E);
  CHECK_EQ(su::BAUD_RATE, 3000000U);
  CHECK_EQ(su::HOST_BYTE_GAP_US, 100U);
}

TEST_CASE("UART-SHTP: the decoder returns an encoded transfer unchanged") {
  const std::vector<uint8_t> shtp = {0x08, 0x00, 0x01, 0x03, 0x7E, 0x7D, 0x00, 0xFF};
  Decoder d;
  REQUIRE(feed(d, encode(shtp)) == Decoder::Event::ShtpFrame);
  CHECK(std::vector<uint8_t>(d.Data(), d.Data() + d.Size()) == shtp);
}

TEST_CASE("UART-SHTP: a buffer status notification reports the free space (little endian)") {
  Decoder d;
  // 0x017E = 382 bytes free; the 0x7E byte arrives escaped.
  REQUIRE(feed(d, {0x7E, 0x00, 0x7D, 0x5E, 0x01, 0x7E}) == Decoder::Event::BufferStatus);
  CHECK_EQ(d.BufferSpace(), 382U);
  d.ClearBufferSpace();
  CHECK_EQ(d.BufferSpace(), 0U);
}

TEST_CASE("UART-SHTP: bytes before the first flag are ignored and frames may share a flag") {
  Decoder d;
  // Garbage, then two frames where the closing flag of the first also opens the second.
  const std::vector<uint8_t> stream = {0x11, 0x22, 0x7E, 0x01, 0xAA, 0x7E, 0x01, 0xBB, 0xCC, 0x7E};
  std::vector<std::vector<uint8_t>> frames;
  for (uint8_t b : stream) {
    if (d.Push(b) == Decoder::Event::ShtpFrame) {
      frames.emplace_back(d.Data(), d.Data() + d.Size());
    }
  }
  REQUIRE(frames.size() == 2);
  CHECK(frames[0] == (std::vector<uint8_t>{0xAA}));
  CHECK(frames[1] == (std::vector<uint8_t>{0xBB, 0xCC}));
}

TEST_CASE("UART-SHTP: oversize frames and unknown protocol IDs are dropped") {
  su::FrameDecoder<4> small;
  CHECK(feed(small, {0x7E, 0x01, 1, 2, 3, 4, 5, 0x7E}) == su::FrameDecoder<4>::Event::Dropped);
  // The decoder recovers on the next frame.
  REQUIRE(feed(small, {0x7E, 0x01, 9, 0x7E}) == su::FrameDecoder<4>::Event::ShtpFrame);
  CHECK_EQ(small.Size(), 1U);
  Decoder d;
  CHECK(feed(d, {0x7E, 0x05, 1, 0x7E}) == Decoder::Event::Dropped);
}
