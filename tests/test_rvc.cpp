/**
 * @file test_rvc.cpp
 * @brief UART-RVC frame parsing: the datasheet's example frame, resynchronisation and checksum
 * rejection.
 */
#include "test_harness.hpp"

#include "mock_bno08x.hpp"

#include <cmath>
#include <vector>

namespace {

// The example message of the BNO08X datasheet (1000-3927 v1.17, section 1.2.5.2).
const std::vector<uint8_t> kDatasheetFrame = {0xAA, 0xAA, 0xDE, 0x01, 0x00, 0x92, 0xFF,
                                              0x25, 0x08, 0x8D, 0xFE, 0xEC, 0xFF, 0xD1,
                                              0x03, 0x00, 0x00, 0x00, 0xE7};

bool near(float a, float b) {
  return std::fabs(a - b) < 1e-4f;
}

struct Capture {
  std::vector<RvcSensorValue> frames;
};

} // namespace

TEST_CASE("RVC decodes the datasheet example frame") {
  MockBus bus(BNO085Interface::UARTRVC);
  BNO085<MockBus> imu(bus);
  Capture cap;
  imu.SetRvcCallback([&cap](const RvcSensorValue& v) { cap.frames.push_back(v); });
  REQUIRE(imu.BeginRvc());
  bus.queue(kDatasheetFrame);
  imu.ServiceRvc();
  REQUIRE(cap.frames.size() == 1);
  const auto& v = cap.frames[0];
  CHECK_EQ(v.index, 0xDE);
  CHECK(near(v.yaw_deg, 0.01f));
  CHECK(near(v.pitch_deg, -1.10f));
  CHECK(near(v.roll_deg, 20.85f));
  CHECK(near(v.acc_x_g, -0.371f));
  CHECK(near(v.acc_y_g, -0.020f));
  CHECK(near(v.acc_z_g, 0.977f));
}

TEST_CASE("RVC resynchronises after leading garbage and across ServiceRvc calls") {
  MockBus bus(BNO085Interface::UARTRVC);
  BNO085<MockBus> imu(bus);
  int frames = 0;
  imu.SetRvcCallback([&frames](const RvcSensorValue&) { ++frames; });
  REQUIRE(imu.BeginRvc());
  bus.queue({0x13, 0xAA, 0x37});
  bus.queue(std::vector<uint8_t>(kDatasheetFrame.begin(), kDatasheetFrame.begin() + 7));
  imu.ServiceRvc();
  CHECK_EQ(frames, 0);
  bus.queue(std::vector<uint8_t>(kDatasheetFrame.begin() + 7, kDatasheetFrame.end()));
  imu.ServiceRvc();
  CHECK_EQ(frames, 1);
}

TEST_CASE("RVC drops a frame whose checksum does not match") {
  MockBus bus(BNO085Interface::UARTRVC);
  BNO085<MockBus> imu(bus);
  int frames = 0;
  imu.SetRvcCallback([&frames](const RvcSensorValue&) { ++frames; });
  REQUIRE(imu.BeginRvc());
  auto bad = kDatasheetFrame;
  bad[18] ^= 0x01;
  bus.queue(bad);
  imu.ServiceRvc();
  CHECK_EQ(frames, 0);
}

TEST_CASE("RVC session opens and closes the transport") {
  MockBus bus(BNO085Interface::UARTRVC);
  BNO085<MockBus> imu(bus);
  REQUIRE(imu.BeginRvc());
  CHECK(imu.GetState() == BNO085DriverState::RvcActive);
  CHECK_EQ(bus.opens, 1);
  imu.CloseRvc();
  CHECK(imu.GetState() == BNO085DriverState::Closed);
  CHECK_EQ(bus.closes, 1);
}
