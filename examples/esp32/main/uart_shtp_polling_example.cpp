/**
 * @file uart_shtp_polling_example.cpp
 * @brief SH-2 over UART (UART-SHTP mode): rotation vector and linear acceleration, polled.
 *
 * Wiring (ESP32-S3, defaults of Esp32Bno08xUartBus::UartConfig):
 * - ESP32 TX GPIO21 -> BNO08x RX, ESP32 RX GPIO20 <- BNO08x TX, 3 Mbit/s 8N1
 * - H_INTN GPIO17 (optional), NRST GPIO16
 * - PS1 strapped high and PS0 low (UART-SHTP, datasheet 1000-3927 figure 1-5); the
 *   BNO08x needs its external 32.768 kHz crystal or clock for UART (section 1.2.3).
 *
 * @copyright HardFOC
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../../../inc/bno08x.hpp"
#include "esp32_bno08x_uart_bus.hpp"

static const char* TAG = "BNO08x_UART_Polling";

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "BNO08x SH-2 over UART, driver %s", GetBNO08xDriverVersion());

  static Esp32Bno08xUartBus bus; // default pins, see the file header
  static BNO085<Esp32Bno08xUartBus> imu(bus);

  if (!bus.Open()) {
    ESP_LOGE(TAG, "UART setup failed");
    return;
  }
  if (!imu.HardwareReset()) { // NRST low 2 ms, then RESET_STARTUP_MS for the hub to start
    ESP_LOGE(TAG, "NRST could not be driven");
    return;
  }
  if (!imu.Begin()) {
    ESP_LOGE(TAG, "Begin() failed: %d (check TX/RX, PS1 high / PS0 low, the crystal)",
             imu.GetLastError());
    return;
  }

  imu.EnableSensor(BNO085Sensor::RotationVector, 20); // 50 Hz
  imu.EnableSensor(BNO085Sensor::LinearAcceleration, 20);

  while (true) {
    imu.Update();
    if (imu.HasNewData(BNO085Sensor::RotationVector)) {
      const auto e = imu.GetLatest(BNO085Sensor::RotationVector);
      ESP_LOGI(TAG, "quat w=%.3f x=%.3f y=%.3f z=%.3f (accuracy %u)", e.rotation.w, e.rotation.x,
               e.rotation.y, e.rotation.z, static_cast<unsigned>(e.rotation.accuracy));
    }
    if (imu.HasNewData(BNO085Sensor::LinearAcceleration)) {
      const auto a = imu.GetLatest(BNO085Sensor::LinearAcceleration);
      ESP_LOGI(TAG, "linear accel m/s^2 x=%.2f y=%.2f z=%.2f", a.vector.x, a.vector.y, a.vector.z);
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
