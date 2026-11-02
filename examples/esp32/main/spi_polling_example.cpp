/**
 * @file spi_polling_example.cpp
 * @brief SH-2 over SPI: rotation vector and calibrated gyroscope, polled.
 *
 * Wiring (ESP32-S3, defaults of Esp32Bno08xSpiBus::SpiConfig):
 * - SCLK GPIO12, MOSI GPIO11, MISO GPIO13, CS GPIO10 (mode 3, 3 MHz)
 * - H_INTN GPIO17 (required), NRST GPIO16, PS0/WAKE GPIO18 (required)
 * - PS1 strapped high. PS0/WAKE is driven high by the port before the reset, so the
 *   hub boots into SPI mode (datasheet 1000-3927 figure 1-5, section 1.2.4 note 5).
 *
 * @copyright HardFOC
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../../../inc/bno08x.hpp"
#include "esp32_bno08x_spi_bus.hpp"

static const char* TAG = "BNO08x_SPI_Polling";

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "BNO08x SH-2 over SPI, driver %s", GetBNO08xDriverVersion());

  static Esp32Bno08xSpiBus bus; // default pins, see the file header
  static BNO085<Esp32Bno08xSpiBus> imu(bus);

  // Configure the pins (PS0/WAKE high, CS high) before the reset samples PS0/PS1.
  if (!bus.Open()) {
    ESP_LOGE(TAG, "SPI bus setup failed");
    return;
  }
  if (!imu.HardwareReset()) { // NRST low 2 ms, then RESET_STARTUP_MS for the hub to start
    ESP_LOGE(TAG, "NRST could not be driven");
    return;
  }
  if (!imu.Begin()) {
    ESP_LOGE(TAG, "Begin() failed: %d (check H_INTN, PS0/WAKE and PS1)", imu.GetLastError());
    return;
  }

  imu.EnableSensor(BNO085Sensor::RotationVector, 10); // 100 Hz
  imu.EnableSensor(BNO085Sensor::Gyroscope, 10);

  while (true) {
    imu.Update();
    if (imu.HasNewData(BNO085Sensor::RotationVector)) {
      const auto e = imu.GetLatest(BNO085Sensor::RotationVector);
      ESP_LOGI(TAG, "quat w=%.3f x=%.3f y=%.3f z=%.3f (accuracy %u)", e.rotation.w, e.rotation.x,
               e.rotation.y, e.rotation.z, static_cast<unsigned>(e.rotation.accuracy));
    }
    if (imu.HasNewData(BNO085Sensor::Gyroscope)) {
      const auto g = imu.GetLatest(BNO085Sensor::Gyroscope);
      ESP_LOGI(TAG, "gyro rad/s x=%.3f y=%.3f z=%.3f", g.vector.x, g.vector.y, g.vector.z);
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
