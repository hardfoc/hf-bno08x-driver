/**
 * @file dfu.cpp
 * @brief The firmware update page's example, compiled by the host build (never run).
 */
#include "my_i2c.hpp"

#include <cstdio>

// --8<-- [start:dfu]
int update_firmware(MyI2c& bus, const uint8_t* image, uint32_t size) {
  BNO085<MyI2c> imu(bus);

  DfuMemoryImage fw{};
  fw.data = image;
  fw.length = size;
  fw.format = "BNO_V1";        // FW-Format of the vendor image
  fw.partNumber = "1000-3608"; // SW-Part-Number of the vendor image

  DfuOptions options{};
  options.progress = [](const DfuProgress& p) {
    std::printf("%u / %u bytes\n", static_cast<unsigned>(p.bytesSent),
                static_cast<unsigned>(p.totalBytes));
  };

  // BOOTN low across a reset, transfer, then BOOTN high and reset into the application.
  const int status = imu.RunDfuFromMemory(fw, options);
  if (status != SH2_OK) {
    return status;
  }
  return imu.Begin() ? SH2_OK : imu.GetLastError();
}
// --8<-- [end:dfu]
