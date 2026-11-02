/**
 * @file test_dfu_i2c.cpp
 * @brief Firmware update over I2C talks to the bootloader address (0x28 | SA0, datasheet
 * 1000-3927 section 1.4) and restores the application address afterwards.
 */
#include "test_harness.hpp"

#include "mock_bno08x.hpp"

#include <algorithm>
#include <vector>

namespace {

using Imu = BNO085<MockBus>;

// Smallest image the driver accepts: 1024 bytes, 64-byte packets -> 16 data packets.
const std::vector<uint8_t> kImage(1024, 0x5A);
constexpr int kAcksForImage = 2 + 16; // application size, packet size, data packets

DfuMemoryImage image() {
  DfuMemoryImage img{};
  img.data = kImage.data();
  img.length = static_cast<uint32_t>(kImage.size());
  img.partNumber = "1000-3608";
  return img;
}

void queueAcks(MockBus& bus, int n) {
  bus.queue(std::vector<uint8_t>(static_cast<std::size_t>(n), 's'));
}

bool allWritesAt(const MockBus& bus, uint8_t address) {
  return !bus.writeAddresses.empty() &&
         std::all_of(bus.writeAddresses.begin(), bus.writeAddresses.end(),
                     [address](uint8_t a) { return a == address; });
}

} // namespace

static_assert(Imu::BootloaderI2cAddress(0x4A) == 0x28);
static_assert(Imu::BootloaderI2cAddress(0x4B) == 0x29);
static_assert(Imu::BootloaderI2cAddress(0x28) == 0x28);
static_assert(Imu::BootloaderI2cAddress(0x29) == 0x29);
static_assert(Imu::BootloaderI2cAddress(0x50) == 0);
static_assert(bno08x::detail::kHasI2cAddressHooks<MockBus>);

TEST_CASE("DFU over I2C sends to 0x28 with SA0 low and restores 0x4A") {
  MockBus bus(BNO085Interface::I2C);
  bus.i2cAddress = 0x4A;
  queueAcks(bus, kAcksForImage);
  Imu imu(bus);
  CHECK_EQ(imu.DfuFromMemory(image()), SH2_OK);
  CHECK(allWritesAt(bus, 0x28));
  CHECK_EQ(bus.i2cAddress, 0x4A);
  CHECK(bus.addressLog == (std::vector<uint8_t>{0x28, 0x4A}));
}

TEST_CASE("DFU over I2C sends to 0x29 with SA0 high and restores 0x4B") {
  MockBus bus(BNO085Interface::I2C);
  bus.i2cAddress = 0x4B;
  queueAcks(bus, kAcksForImage);
  Imu imu(bus);
  CHECK_EQ(imu.DfuFromMemory(image()), SH2_OK);
  CHECK(allWritesAt(bus, 0x29));
  CHECK_EQ(bus.i2cAddress, 0x4B);
}

TEST_CASE("a failed DFU transfer over I2C still restores the application address") {
  MockBus bus(BNO085Interface::I2C);
  bus.i2cAddress = 0x4B;
  Imu imu(bus); // no ACKs queued: the first packet times out
  CHECK_EQ(imu.DfuFromMemory(image()), SH2_ERR_TIMEOUT);
  CHECK(allWritesAt(bus, 0x29));
  CHECK_EQ(bus.i2cAddress, 0x4B);
  CHECK(imu.GetState() == BNO085DriverState::Closed);
}

TEST_CASE("RunDfuFromMemory() over I2C ends at the application address") {
  MockBus bus(BNO085Interface::I2C);
  queueAcks(bus, kAcksForImage);
  Imu imu(bus);
  CHECK_EQ(imu.RunDfuFromMemory(image()), SH2_OK);
  CHECK(allWritesAt(bus, 0x28));
  CHECK_EQ(bus.i2cAddress, 0x4A);
}

TEST_CASE("DFU over I2C fails with SH2_ERR_IO and sends nothing if the address cannot be set") {
  MockBus bus(BNO085Interface::I2C);
  bus.setAddressSucceeds = false;
  queueAcks(bus, kAcksForImage);
  Imu imu(bus);
  CHECK_EQ(imu.DfuFromMemory(image()), SH2_ERR_IO);
  CHECK(bus.written.empty());
  CHECK_EQ(bus.i2cAddress, 0x4A);
}

TEST_CASE("DFU over I2C refuses a port address that is not a BNO08x address") {
  MockBus bus(BNO085Interface::I2C);
  bus.i2cAddress = 0x50;
  Imu imu(bus);
  CHECK_EQ(imu.DfuFromMemory(image()), SH2_ERR_BAD_PARAM);
  CHECK(bus.written.empty());
  CHECK(bus.addressLog.empty());
}

TEST_CASE("DFU over SPI does not touch the I2C address") {
  MockBus bus(BNO085Interface::SPI);
  queueAcks(bus, kAcksForImage);
  Imu imu(bus);
  CHECK_EQ(imu.DfuFromMemory(image()), SH2_OK);
  CHECK(bus.addressLog.empty());
}
