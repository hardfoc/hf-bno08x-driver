/**
 * @file test_dfu_image.cpp
 * @brief Dfu() takes the image from the caller: there is no default, and the compiled-in
 * placeholder (firmware-bno.c, "Dummy Firmware Image"), an empty image and a malformed HcBin_t are
 * refused before the driver touches the bus or the running SH-2 session.
 */
#include "test_harness.hpp"

#include "mock_bno08x.hpp"

#include <type_traits>
#include <utility>

namespace {

using Imu = BNO085<MockBus>;

template <typename T, typename = void>
struct CanDfuWithoutImage : std::false_type {};
template <typename T>
struct CanDfuWithoutImage<T, std::void_t<decltype(std::declval<T&>().Dfu())>> : std::true_type {};

// The caller must name the image: Dfu() has no default argument.
static_assert(!CanDfuWithoutImage<Imu>::value, "Dfu() must not default to the placeholder image");

// An image with no app data: open/close succeed, the metadata is valid, the length is 0.
int okOpen() {
  return 0;
}
int okClose() {
  return 0;
}
const char* goodMeta(const char* key) {
  if (std::string(key) == "FW-Format") {
    return "BNO_V1";
  }
  if (std::string(key) == "SW-Part-Number") {
    return "1000-3608";
  }
  return nullptr;
}
uint32_t zeroLen() {
  return 0;
}
uint32_t noPacketPref() {
  return 0;
}
int noData(uint8_t*, uint32_t, uint32_t) {
  return 0;
}

const HcBin_t kEmptyImage = {okOpen, okClose, goodMeta, zeroLen, noPacketPref, noData};
const HcBin_t kNullImage = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

MockBus activeBus() {
  MockBus bus(BNO085Interface::I2C);
  bus.answerInitialize = true;
  return bus;
}

/// Run @p dfu against a driver with a running SH-2 session; the image must be refused with
/// SH2_ERR_BAD_PARAM and the session, the bus and the address left as they were.
template <typename Fn>
void checkRefusedWithoutSideEffects(Fn&& dfu) {
  MockBus bus = activeBus();
  bus.i2cAddress = 0x4A;
  Imu imu(bus);
  REQUIRE(imu.Begin());
  const auto written_before = bus.written.size();
  const auto closes_before = bus.closes;
  CHECK_EQ(dfu(imu), SH2_ERR_BAD_PARAM);
  CHECK_EQ(imu.GetLastError(), SH2_ERR_BAD_PARAM);
  CHECK(imu.GetState() == BNO085DriverState::Sh2Active); // the session was not closed
  CHECK_EQ(bus.written.size(), written_before);          // nothing sent
  CHECK_EQ(bus.closes, closes_before);
  CHECK_EQ(bus.i2cAddress, 0x4A); // never switched to the bootloader address
  imu.Close();
}

} // namespace

TEST_CASE("Dfu() refuses the compiled-in placeholder image without closing the session") {
  checkRefusedWithoutSideEffects([](Imu& imu) { return imu.Dfu(firmware); });
}

TEST_CASE("Dfu() refuses a copy of the placeholder image") {
  const HcBin_t copy = firmware;
  checkRefusedWithoutSideEffects([&](Imu& imu) { return imu.Dfu(copy); });
  DfuOptions lax{};
  lax.requireFormatMatch = false;
  lax.requirePartNumber = false;
  checkRefusedWithoutSideEffects([&](Imu& imu) { return imu.DfuWithOptions(copy, lax); });
}

TEST_CASE("Dfu() refuses an empty image") {
  checkRefusedWithoutSideEffects([](Imu& imu) { return imu.Dfu(kEmptyImage); });
  checkRefusedWithoutSideEffects(
      [](Imu& imu) { return imu.DfuFromMemory(nullptr, 0, "1000-3608"); });
}

TEST_CASE("Dfu() refuses an HcBin_t with missing functions instead of calling through null") {
  checkRefusedWithoutSideEffects([](Imu& imu) { return imu.Dfu(kNullImage); });
}

TEST_CASE("Dfu() refuses an image with the wrong format or an unknown part without side effects") {
  static const std::vector<uint8_t> data(1024, 0x5A);
  checkRefusedWithoutSideEffects([](Imu& imu) {
    MemoryFirmware fw(data.data(), static_cast<uint32_t>(data.size()), "NOT_BNO", "1000-3608");
    return imu.Dfu(fw.hcbin());
  });
  checkRefusedWithoutSideEffects([](Imu& imu) {
    MemoryFirmware fw(data.data(), static_cast<uint32_t>(data.size()), "BNO_V1", "9999-0000");
    return imu.Dfu(fw.hcbin());
  });
  checkRefusedWithoutSideEffects([](Imu& imu) {
    MemoryFirmware fw(data.data(), 512, "BNO_V1", "1000-3608"); // shorter than any image
    return imu.Dfu(fw.hcbin());
  });
}
