/**
 * @file dfu_example.cpp
 * @brief Device Firmware Update (DFU) example for BNO08x
 *
 * This example demonstrates:
 * - Entering BNO08x bootloader mode via BOOTN pin
 * - BNO085::Dfu() refusing the compiled-in placeholder image
 * - Using MemoryFirmware for runtime-provided firmware images
 *
 * IMPORTANT HARDWARE NOTES:
 * - DFU requires the BOOTN pin to be wired and active (active-low)
 * - The default esp32_bno08x_bus.hpp does NOT wire BOOTN. To use DFU,
 *   you must modify SetBoot() in esp32_bno08x_bus.hpp to drive a real GPIO.
 * - The sensor must be reset while BOOTN is held low to enter bootloader mode.
 * - Dfu() needs an image from the caller. The compiled-in firmware-bno.c is a
 *   placeholder that the driver refuses; pass a real image from CEVA.
 *
 * @note This example runs independently on an I2C bus that only communicates
 *       with the BNO08x device. The I2C bus setup (GPIO4 SDA, GPIO5 SCL)
 *       mirrors the other BNO08x examples for pin consistency.
 *
 * @author Nebiyu Tadesse
 * @date 2025
 * @copyright HardFOC
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <memory>
#include <stdio.h>

#include "../../../inc/bno08x.hpp"
#include "../../../src/dfu/MemoryFirmware.hpp"
#include "esp32_bno08x_bus.hpp"

static const char* TAG = "BNO08x_DFU";

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "BNO08x Device Firmware Update (DFU) Example");
  ESP_LOGI(TAG, "=============================================");
  ESP_LOGI(TAG, "Driver version: %s", GetBNO08xDriverVersion());
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "This example demonstrates the DFU API. To perform a real");
  ESP_LOGI(TAG, "firmware update you need:");
  ESP_LOGI(TAG, "  1. BOOTN pin wired to a GPIO (modify SetBoot() in esp32_bno08x_bus.hpp)");
  ESP_LOGI(TAG, "  2. A valid BNO08x firmware binary (replace firmware-bno.c stub)");
  ESP_LOGI(TAG, "");

  // Configure I2C transport
  Esp32Bno08xI2cBus::I2CConfig config;
  config.sda_pin = GPIO_NUM_4;
  config.scl_pin = GPIO_NUM_5;
  config.frequency = 400000;
  config.rst_pin = GPIO_NUM_16; // Reset pin (RSTN) on GPIO16
  config.int_pin = GPIO_NUM_17; // Interrupt pin (INT) on GPIO17

  // BNO08x uses 7-bit I2C addresses: 0x4A (SA0=LOW) or 0x4B (SA0=HIGH)
  // Try both addresses automatically
  const uint8_t addresses[] = {0x4B, 0x4A};
  std::unique_ptr<Esp32Bno08xI2cBus> transport;
  BNO085<Esp32Bno08xI2cBus>* imu = nullptr;
  bool initialized = false;

  for (size_t i = 0; i < sizeof(addresses) / sizeof(addresses[0]); i++) {
    config.device_address = addresses[i];
    ESP_LOGI(TAG, "Trying BNO08x at I2C address 0x%02X (SA0=%s)...", config.device_address,
             (i == 0) ? "HIGH" : "LOW");

    transport = CreateEsp32Bno08xI2cBus(config);
    if (!transport) {
      ESP_LOGW(TAG, "Failed to create I2C transport for address 0x%02X", config.device_address);
      continue;
    }

    // Perform hardware reset BEFORE probing
    ESP_LOGI(TAG, "Performing hardware reset before probing...");
    transport->HardwareReset(2, 200);

    ESP_LOGI(TAG, "Probing I2C device at address 0x%02X...", config.device_address);
    if (!transport->Probe()) {
      ESP_LOGW(TAG, "I2C probe failed at address 0x%02X", config.device_address);
      transport.reset();
      continue;
    }
    ESP_LOGI(TAG, "I2C probe successful at address 0x%02X", config.device_address);

    imu = new BNO085<Esp32Bno08xI2cBus>(*transport);

    if (imu->Begin()) {
      ESP_LOGI(TAG, "BNO085 initialized successfully at address 0x%02X", config.device_address);
      initialized = true;
      break;
    } else {
      ESP_LOGW(TAG, "Failed to initialize BNO085 at address 0x%02X", config.device_address);
      delete imu;
      imu = nullptr;
      transport.reset();
    }
  }

  if (!initialized || !imu) {
    ESP_LOGE(TAG, "Failed to initialize BNO085 at any address");
    return;
  }

  // ========================================================================
  // DFU DEMONSTRATION
  // ========================================================================

  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "--- DFU API Demonstration ---");
  ESP_LOGI(TAG, "");

  // ---- Method 1: The compiled-in placeholder is refused ----
  //
  // BNO085::Dfu() has no default image: the caller passes one. The `firmware`
  // object from firmware-bno.c is a 64-byte placeholder ("Dummy Firmware
  // Image"); every DFU call refuses it with SH2_ERR_BAD_PARAM before touching
  // the bus, so the sensor stays in its running session.
  //
  // The DFU flow with a real image is:
  //   1. Hold BOOTN low
  //   2. Reset the sensor (RSTN pulse)
  //   3. Release BOOTN -> sensor enters bootloader
  //   4. Call imu->Dfu(image) to transfer firmware
  //   5. Sensor reboots with new firmware

  ESP_LOGI(TAG, "Method 1: the placeholder image is refused");
  ESP_LOGI(TAG, "  IsPlaceholderFirmware(firmware) = %s",
           BNO085<Esp32Bno08xI2cBus>::IsPlaceholderFirmware(firmware) ? "true" : "false");

  int status = imu->Dfu(firmware); // refused: nothing is sent, the bootloader is not entered
  if (status == SH2_ERR_BAD_PARAM) {
    ESP_LOGI(TAG, "  Dfu(firmware) refused the placeholder (SH2_ERR_BAD_PARAM), as it must");
  } else {
    ESP_LOGE(TAG, "  Dfu(firmware) returned %d; the placeholder must be refused", status);
  }

  // ---- Method 2: Using MemoryFirmware for runtime-provided images ----
  //
  // If you have the firmware binary in memory (e.g., downloaded via WiFi/BLE),
  // you can use MemoryFirmware instead of the compiled-in stub:
  //
  //   const uint8_t* fw_data = ...;  // firmware binary in memory
  //   uint32_t fw_len = ...;          // firmware size in bytes
  //   MemoryFirmware memFw(fw_data, fw_len, "BNO_V1", "1000-3608");
  //   int status = imu->Dfu(memFw.hcbin());

  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Method 2: MemoryFirmware (runtime firmware images)");
  ESP_LOGI(TAG, "  MemoryFirmware allows loading firmware from any memory source:");
  ESP_LOGI(TAG, "  - Downloaded via WiFi/BLE/HTTP");
  ESP_LOGI(TAG, "  - Read from SD card or flash filesystem");
  ESP_LOGI(TAG, "  - Received via serial protocol");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "  Example code (not executed - requires real firmware data):");
  ESP_LOGI(TAG, "    const uint8_t* fw_data = <your_firmware_bytes>;");
  ESP_LOGI(TAG, "    uint32_t fw_len = <firmware_size>;");
  ESP_LOGI(TAG, "    MemoryFirmware memFw(fw_data, fw_len, \"BNO_V1\", \"1000-3608\");");
  ESP_LOGI(TAG, "    int status = imu->Dfu(memFw.hcbin());");

  // ========================================================================
  // After DFU, reset and re-initialize to use the (possibly new) firmware
  // ========================================================================

  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "--- Post-DFU: Re-initializing sensor ---");

  // Normal reset (without BOOTN) to boot into application mode
  imu->HardwareReset(2);
  vTaskDelay(pdMS_TO_TICKS(500)); // Wait for sensor to boot

  // Re-initialize
  if (imu->Begin()) {
    ESP_LOGI(TAG, "Sensor re-initialized successfully after DFU");

    // Enable a sensor to verify the device is working
    if (imu->EnableSensor(BNO085Sensor::RotationVector, 100)) {
      ESP_LOGI(TAG, "Rotation Vector enabled - sensor operational");

      // Read a few samples
      for (int sample = 0; sample < 20; sample++) {
        imu->Update();
        if (imu->HasNewData(BNO085Sensor::RotationVector)) {
          auto rot = imu->GetLatest(BNO085Sensor::RotationVector);
          ESP_LOGI(TAG, "  Quat: w=%.3f x=%.3f y=%.3f z=%.3f", rot.rotation.w, rot.rotation.x,
                   rot.rotation.y, rot.rotation.z);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }
  } else {
    ESP_LOGW(TAG, "Sensor re-initialization failed after DFU (expected with stub firmware)");
  }

  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "DFU example complete.");

  // Clean up
  delete imu;

  // Idle
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
