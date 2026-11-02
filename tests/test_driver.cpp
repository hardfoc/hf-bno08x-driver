/**
 * @file test_driver.cpp
 * @brief Mode guards, error codes and the control-pin sequences of BNO085<Comm>, over the mock
 * transport.
 */
#include "test_harness.hpp"

#include "mock_bno08x.hpp"

#include <string>
#include <vector>

TEST_CASE("SH-2 calls are refused on a UART-RVC transport") {
  MockBus bus(BNO085Interface::UARTRVC);
  BNO085<MockBus> imu(bus);
  CHECK(!imu.Begin());
  CHECK_EQ(imu.GetLastError(), SH2_ERR_BAD_PARAM);
  CHECK_EQ(bus.opens, 0);
  CHECK_EQ(imu.Dfu(), SH2_ERR_BAD_PARAM);
  CHECK(!imu.EnterBootloader());
}

TEST_CASE("RVC calls are refused on an SH-2 transport") {
  MockBus bus(BNO085Interface::I2C);
  BNO085<MockBus> imu(bus);
  CHECK(!imu.BeginRvc());
  CHECK_EQ(imu.GetLastError(), SH2_ERR_BAD_PARAM);
  CHECK(imu.GetState() == BNO085DriverState::Closed);
}

TEST_CASE("sensor calls before Begin() fail with SH2_ERR_OP_IN_PROGRESS") {
  MockBus bus(BNO085Interface::I2C);
  BNO085<MockBus> imu(bus);
  CHECK(!imu.EnableSensor(BNO085Sensor::RotationVector, 10));
  CHECK_EQ(imu.GetLastError(), SH2_ERR_OP_IN_PROGRESS);
  CHECK(!imu.DisableSensor(BNO085Sensor::RotationVector));
  CHECK(!imu.HasNewData(BNO085Sensor::RotationVector));
}

TEST_CASE("Begin() fails and leaves the driver closed when the transport does not open") {
  MockBus bus(BNO085Interface::I2C);
  bus.openSucceeds = false;
  BNO085<MockBus> imu(bus);
  CHECK(!imu.Begin());
  CHECK(imu.GetLastError() != SH2_OK);
  CHECK(imu.GetState() == BNO085DriverState::Closed);
}

TEST_CASE("HardwareReset() asserts RSTN, waits, releases it and waits 50 ms") {
  MockBus bus(BNO085Interface::I2C);
  BNO085<MockBus> imu(bus);
  imu.HardwareReset(5);
  const std::vector<std::string> expected = {"RSTN active", "delay 5", "RSTN inactive", "delay 50"};
  CHECK(bus.log == expected);
}

TEST_CASE("EnterBootloader() holds BOOTN across a reset, then releases it") {
  MockBus bus(BNO085Interface::I2C);
  BNO085<MockBus> imu(bus);
  REQUIRE(imu.EnterBootloader(10, 50));
  const std::vector<std::string> expected = {"BOOTN active",  "RSTN active", "delay 10",
                                             "RSTN inactive", "delay 50",    "BOOTN inactive",
                                             "delay 50"};
  CHECK(bus.log == expected);
  CHECK_EQ(bus.opens, 1);
}

TEST_CASE("Dfu() refuses the compiled-in stub image before sending anything") {
  MockBus bus(BNO085Interface::I2C);
  BNO085<MockBus> imu(bus);
  CHECK_EQ(imu.Dfu(), SH2_ERR_BAD_PARAM); // the stub is 64 bytes; a real image is at least 1024
  CHECK(bus.written.empty());
  CHECK(imu.GetState() == BNO085DriverState::Closed);
}

TEST_CASE("DfuFromMemory() rejects an empty image") {
  MockBus bus(BNO085Interface::SPI);
  BNO085<MockBus> imu(bus);
  CHECK_EQ(imu.DfuFromMemory(nullptr, 0), SH2_ERR_BAD_PARAM);
  CHECK_EQ(imu.GetLastError(), SH2_ERR_BAD_PARAM);
}

TEST_CASE("the version accessors match the build settings") {
  CHECK(std::string(BNO085<MockBus>::GetDriverVersion()) == HF_BNO08X_VERSION_STRING);
  CHECK(std::string(GetBNO08xDriverVersion()) == HF_BNO08X_VERSION_STRING);
}
