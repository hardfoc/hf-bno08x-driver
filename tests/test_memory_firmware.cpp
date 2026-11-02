/**
 * @file test_memory_firmware.cpp
 * @brief In-memory DFU image checks, from a second translation unit: MemoryFirmware's per-thread
 * state must not be defined once per file that uses DfuFromMemory().
 */
#include "test_harness.hpp"

#include "mock_bno08x.hpp"

#include <vector>

TEST_CASE("DfuFromMemory() rejects an unknown part number before sending anything") {
  MockBus bus(BNO085Interface::SPI);
  BNO085<MockBus> imu(bus);
  const std::vector<uint8_t> fw(1024, 0);
  CHECK_EQ(imu.DfuFromMemory(fw.data(), static_cast<uint32_t>(fw.size()), "9999-0000"),
           SH2_ERR_BAD_PARAM);
  CHECK(bus.written.empty());
}

TEST_CASE("DfuFromMemory() rejects an image that does not match the required part number") {
  MockBus bus(BNO085Interface::SPI);
  BNO085<MockBus> imu(bus);
  const std::vector<uint8_t> fw(1024, 0);
  DfuOptions options{};
  options.requiredPartNumber = "1000-4563";
  CHECK_EQ(imu.DfuFromMemory(fw.data(), static_cast<uint32_t>(fw.size()), "1000-3608", options),
           SH2_ERR_BAD_PARAM);
  CHECK(bus.written.empty());
  // A second update in the same thread starts from a clean adapter state.
  CHECK_EQ(imu.DfuFromMemory(fw.data(), static_cast<uint32_t>(fw.size()), "1000-3608", options),
           SH2_ERR_BAD_PARAM);
}
