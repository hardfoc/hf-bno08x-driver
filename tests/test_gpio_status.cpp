/**
 * @file test_gpio_status.cpp
 * @brief GPIO errors reported by the port's GpioSet() reach the caller; a port with the earlier
 * `void GpioSet()` contract still compiles and is treated as never failing.
 */
#include "test_harness.hpp"

#include "mock_bno08x.hpp"

#include <string>
#include <type_traits>
#include <vector>

namespace {

/// A port written for the earlier contract: GpioSet() returns void.
class LegacyVoidBus : public bno08x::CommInterface<LegacyVoidBus> {
public:
  BNO085Interface GetInterfaceType() noexcept {
    return BNO085Interface::I2C;
  }
  bool Open() noexcept {
    return true;
  }
  void Close() noexcept {}
  int Write(const uint8_t*, uint32_t length) noexcept {
    return static_cast<int>(length);
  }
  int Read(uint8_t*, uint32_t) noexcept {
    return 0;
  }
  bool DataAvailable() noexcept {
    return false;
  }
  void Delay(uint32_t ms) noexcept {
    delayed += ms;
  }
  uint32_t GetTimeUs() noexcept {
    return now += 10;
  }
  void GpioSet(bno08x::CtrlPin, bno08x::GpioSignal) noexcept {
    ++pinWrites;
  }

  int pinWrites = 0;
  uint32_t delayed = 0;
  uint32_t now = 0;
};

static_assert(!bno08x::detail::kGpioSetReportsStatus<LegacyVoidBus>);
static_assert(bno08x::detail::kGpioSetReportsStatus<MockBus>);
static_assert(std::is_same_v<decltype(std::declval<BNO085<MockBus>&>().HardwareReset()), bool>);

} // namespace

TEST_CASE("HardwareReset() returns false and skips the waits when RSTN cannot be asserted") {
  MockBus bus(BNO085Interface::I2C);
  bus.failPins = {bno08x::CtrlPin::RSTN};
  BNO085<MockBus> imu(bus);
  CHECK(!imu.HardwareReset(5));
  const std::vector<std::string> expected = {"RSTN active FAILED"};
  CHECK(bus.log == expected);
}

TEST_CASE("HardwareReset() returns true when the port drives RSTN") {
  MockBus bus(BNO085Interface::I2C);
  BNO085<MockBus> imu(bus);
  CHECK(imu.HardwareReset());
}

TEST_CASE("SetBootPin(), SetWakePin() and SelectInterface() report GPIO errors") {
  MockBus bus(BNO085Interface::SPI);
  BNO085<MockBus> imu(bus);
  CHECK(imu.SetBootPin(true));
  CHECK(imu.SetWakePin(true));
  CHECK(imu.SelectInterface(BNO085Interface::SPI));
  bus.failPins = {bno08x::CtrlPin::BOOTN, bno08x::CtrlPin::WAKE, bno08x::CtrlPin::PS1};
  CHECK(!imu.SetBootPin(false));
  CHECK(!imu.SetWakePin(false));
  bus.log.clear();
  CHECK(!imu.SelectInterface(BNO085Interface::SPI));
  // PS0 is still driven when PS1 fails, so the pins do not stay half-switched silently.
  const std::vector<std::string> expected = {"PS1 active FAILED", "PS0 active"};
  CHECK(bus.log == expected);
}

TEST_CASE("EnterBootloader() fails with SH2_ERR_IO, releases BOOTN and closes the port") {
  MockBus bus(BNO085Interface::I2C);
  bus.failPins = {bno08x::CtrlPin::RSTN};
  BNO085<MockBus> imu(bus);
  CHECK(!imu.EnterBootloader(10, 50));
  CHECK_EQ(imu.GetLastError(), SH2_ERR_IO);
  CHECK(!bus.open);
  CHECK(bus.log.back() == "BOOTN inactive");
  CHECK(imu.GetState() == BNO085DriverState::Closed);
}

TEST_CASE("ExitBootloaderAndReboot() fails with SH2_ERR_IO when BOOTN cannot be released") {
  MockBus bus(BNO085Interface::I2C);
  bus.failPins = {bno08x::CtrlPin::BOOTN};
  BNO085<MockBus> imu(bus);
  CHECK(!imu.ExitBootloaderAndReboot());
  CHECK_EQ(imu.GetLastError(), SH2_ERR_IO);
  CHECK(!bus.open);
}

TEST_CASE("a port with a void GpioSet() compiles and counts as never failing") {
  LegacyVoidBus bus;
  BNO085<LegacyVoidBus> imu(bus);
  CHECK(imu.HardwareReset(3));
  CHECK_EQ(bus.pinWrites, 2);
  CHECK_EQ(bus.delayed, 3U + BNO085<LegacyVoidBus>::RESET_STARTUP_MS);
  CHECK(imu.SelectInterface(BNO085Interface::I2C));
  CHECK(imu.EnterBootloader());
  CHECK_EQ(imu.GetLastError(), SH2_OK);
}
