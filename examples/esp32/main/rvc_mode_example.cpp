/**
 * @file rvc_mode_example.cpp
 * @brief RVC (Rotation Vector Computation) mode example
 *
 * This example demonstrates:
 * - RVC mode initialization using a UART CommInterface
 * - Frame callback handling
 * - Yaw, pitch, roll reading
 *
 * RVC mode uses a UART transport (Esp32UartRvcBus) instead of I2C.
 * The driver reads raw UART bytes and parses 19-byte RVC frames internally.
 * No separate RVC HAL is needed -- just a CommInterface with GetInterfaceType()
 * returning BNO085Interface::UARTRVC.
 *
 * @note The BNO08x must have PS0/PS1 configured for RVC mode (PS1=LOW, PS0=HIGH)
 *       at reset time. RVC mode operates at 115200 baud.
 *
 * @author Nebiyu Tadesse
 * @date 2025
 * @copyright HardFOC
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#include "../../../inc/bno08x.hpp"
#include "esp32_uart_rvc_bus.hpp"

static const char* TAG = "BNO08x_RvcMode";

// RVC frame callback
static void on_rvc_frame(const RvcSensorValue& value) {
  ESP_LOGI(TAG, "RVC Frame: Yaw=%.2f, Pitch=%.2f, Roll=%.2f (ax=%.3fg ay=%.3fg az=%.3fg)",
           value.yaw_deg, value.pitch_deg, value.roll_deg, value.acc_x_g, value.acc_y_g,
           value.acc_z_g);
}

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "BNO08x RVC Mode Example");
  ESP_LOGI(TAG, "========================");
  ESP_LOGI(TAG, "Driver version: %s", GetBNO08xDriverVersion());
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "RVC mode uses UART at 115200 baud.");
  ESP_LOGI(TAG, "Ensure BNO08x PS0/PS1 are set for RVC mode (PS1=LOW, PS0=HIGH).");
  ESP_LOGI(TAG, "");

  // Configure UART transport for RVC mode
  Esp32UartRvcBus::UartConfig uart_config;
  uart_config.port = UART_NUM_1;
  uart_config.tx_pin = GPIO_NUM_21;
  uart_config.rx_pin = GPIO_NUM_20;
  uart_config.rst_pin = GPIO_NUM_16; // Reset pin (RSTN)
  uart_config.baud_rate = 115200;

  Esp32UartRvcBus uart_bus(uart_config);
  BNO085<Esp32UartRvcBus> imu(uart_bus);

  // Set RVC callback
  imu.SetRvcCallback(on_rvc_frame);

  // Initialize RVC mode -- opens UART, resets sensor, starts frame parser
  ESP_LOGI(TAG, "Initializing BNO085 in RVC mode...");
  if (!imu.BeginRvc()) {
    ESP_LOGE(TAG, "Failed to initialize RVC mode");
    ESP_LOGE(TAG, "Check:");
    ESP_LOGE(TAG, "  1. UART wiring (TX/RX)");
    ESP_LOGE(TAG, "  2. Power supply (3.3V)");
    ESP_LOGE(TAG, "  3. PS0/PS1 pins set for RVC mode");
    ESP_LOGE(TAG, "  4. Reset pin wiring");
    return;
  }

  ESP_LOGI(TAG, "RVC mode initialized successfully");
  ESP_LOGI(TAG, "Starting RVC service loop...");

  // Main RVC service loop
  while (true) {
    imu.ServiceRvc();              // Read UART bytes, parse frames, dispatch callbacks
    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay
  }
}
