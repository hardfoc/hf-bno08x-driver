/**
 * @file esp32_uart_rvc_bus.hpp
 * @brief ESP32 UART CommInterface implementation for BNO08x RVC mode.
 *
 * This file provides an ESP32-specific CommInterface using the UART driver
 * for RVC (Rotation Vector Computation) mode at 115200 baud. The driver's
 * RVC frame parser reads raw bytes via Read() and handles sync/checksum.
 *
 * @note The BNO08x must have PS0/PS1 set to RVC mode (PS1=LOW, PS0=HIGH,
 *       datasheet figure 1-5) at reset time. The sensor outputs 19-byte RVC
 *       frames continuously.
 *
 * @author Nebiyu Tadesse
 * @date 2025
 * @copyright HardFOC
 */

#pragma once

// System headers
#include <cstdint>

// Third-party headers (ESP-IDF)
#ifdef __cplusplus
extern "C" {
#endif
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#ifdef __cplusplus
}
#endif

// Project headers
#include "../../../inc/bno08x_comm_interface.hpp"

static constexpr const char* TAG_UART_RVC = "BNO08x_UART_RVC";

/**
 * @class Esp32UartRvcBus
 * @brief ESP32 UART CommInterface for BNO08x RVC mode.
 *
 * Reads raw UART bytes that the BNO085 driver's internal RVC frame parser
 * processes. The driver calls Read() one byte at a time to accumulate and
 * validate 19-byte RVC frames.
 */
class Esp32UartRvcBus : public bno08x::CommInterface<Esp32UartRvcBus> {
public:
  /**
   * @brief UART configuration for RVC mode.
   */
  struct UartConfig {
    uart_port_t port = UART_NUM_1;     ///< UART port number
    gpio_num_t tx_pin = GPIO_NUM_21;   ///< UART TX pin
    gpio_num_t rx_pin = GPIO_NUM_20;   ///< UART RX pin
    uint32_t baud_rate = 115200;       ///< Baud rate (BNO08x RVC is always 115200)
    gpio_num_t rst_pin = GPIO_NUM_16;  ///< Reset pin (RSTN, active-low, GPIO_NUM_NC if not used)
    gpio_num_t boot_pin = GPIO_NUM_NC; ///< Boot pin (BOOTN, GPIO_NUM_NC if not used)
  };

  Esp32UartRvcBus() : Esp32UartRvcBus(UartConfig{}) {}
  explicit Esp32UartRvcBus(const UartConfig& config) : config_(config) {}

  ~Esp32UartRvcBus() {
    if (initialized_)
      Close();
  }

  // ── CommInterface required methods ─────────────────────────────────────

  BNO085Interface GetInterfaceType() noexcept {
    return BNO085Interface::UARTRVC;
  }

  bool Open() noexcept {
    if (initialized_)
      return true;

    uart_config_t cfg{};
    cfg.baud_rate = static_cast<int>(config_.baud_rate);
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;

    esp_err_t err = uart_param_config(config_.port, &cfg);
    if (err != ESP_OK) {
      ESP_LOGE(TAG_UART_RVC, "uart_param_config failed: %s", esp_err_to_name(err));
      return false;
    }

    err = uart_set_pin(config_.port, config_.tx_pin, config_.rx_pin, UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
      ESP_LOGE(TAG_UART_RVC, "uart_set_pin failed: %s", esp_err_to_name(err));
      return false;
    }

    err = uart_driver_install(config_.port, RX_BUF_SIZE, 0, 0, nullptr, 0);
    if (err != ESP_OK) {
      ESP_LOGE(TAG_UART_RVC, "uart_driver_install failed: %s", esp_err_to_name(err));
      return false;
    }

    // Configure and assert reset pin if wired
    if (config_.rst_pin != GPIO_NUM_NC) {
      gpio_config_t io_conf{};
      io_conf.pin_bit_mask = (1ULL << config_.rst_pin);
      io_conf.mode = GPIO_MODE_OUTPUT;
      io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
      io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
      io_conf.intr_type = GPIO_INTR_DISABLE;
      gpio_config(&io_conf);

      // Reset the sensor into RVC mode
      gpio_set_level(config_.rst_pin, 0); // Assert reset
      vTaskDelay(pdMS_TO_TICKS(2));
      gpio_set_level(config_.rst_pin, 1); // Release reset
      vTaskDelay(pdMS_TO_TICKS(200));     // Wait for sensor boot
    }

    initialized_ = true;
    ESP_LOGI(TAG_UART_RVC, "UART RVC bus opened on port %d (TX:%d, RX:%d, %lu baud)", config_.port,
             config_.tx_pin, config_.rx_pin, static_cast<unsigned long>(config_.baud_rate));
    return true;
  }

  void Close() noexcept {
    if (initialized_) {
      uart_driver_delete(config_.port);
      initialized_ = false;
    }
  }

  /**
   * @brief Read raw bytes from UART.
   *
   * The BNO085 driver calls this one byte at a time to feed the RVC frame
   * parser. Non-blocking: returns 0 immediately if no data available.
   */
  int Read(uint8_t* data, uint32_t length) noexcept {
    if (!initialized_)
      return -1;
    int ret = uart_read_bytes(config_.port, data, length, 0);
    return (ret >= 0) ? ret : -1;
  }

  int Write(const uint8_t* /*data*/, uint32_t /*length*/) noexcept {
    return -1; // RVC mode is read-only (sensor outputs data, host doesn't write)
  }

  bool DataAvailable() noexcept {
    return true; // UART is polled via Read()
  }

  void Delay(uint32_t ms) noexcept {
    vTaskDelay(pdMS_TO_TICKS(ms));
  }

  uint32_t GetTimeUs() noexcept {
    return static_cast<uint32_t>(esp_timer_get_time());
  }

  // ── GPIO pin control ──────────────────────────────────────────────────

  /**
   * @brief Set a control pin to the specified signal state (required by CommInterface)
   *
   * Maps GpioSignal to physical level based on pin polarity:
   * - RSTN:  active-low (ACTIVE → GPIO 0, INACTIVE → GPIO 1)
   * - BOOTN: active-low (ACTIVE → GPIO 0, INACTIVE → GPIO 1)
   * - WAKE/PS0/PS1: not wired in this implementation (no-op)
   *
   * @param pin   Which control pin to drive
   * @param signal ACTIVE to assert, INACTIVE to deassert
   */
  void GpioSet(bno08x::CtrlPin pin, bno08x::GpioSignal signal) noexcept {
    switch (pin) {
      case bno08x::CtrlPin::RSTN:
        if (config_.rst_pin != GPIO_NUM_NC)
          gpio_set_level(config_.rst_pin,
                         signal == bno08x::GpioSignal::ACTIVE ? 0 : 1);
        break;
      case bno08x::CtrlPin::BOOTN:
        if (config_.boot_pin != GPIO_NUM_NC)
          gpio_set_level(config_.boot_pin,
                         signal == bno08x::GpioSignal::ACTIVE ? 0 : 1);
        break;
      case bno08x::CtrlPin::WAKE:
      case bno08x::CtrlPin::PS0:
      case bno08x::CtrlPin::PS1:
        break; // Not wired in this implementation
    }
  }

private:
  static constexpr int RX_BUF_SIZE = 256;
  UartConfig config_;
  bool initialized_{false};
};
