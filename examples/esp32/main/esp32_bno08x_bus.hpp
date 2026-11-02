/**
 * @file esp32_bno08x_bus.hpp
 * @brief ESP32 I2C communication interface implementation for BNO08x driver
 *
 * This file provides the ESP32-specific implementation of the bno08x::CommInterface
 * interface using ESP-IDF's I2C master driver.
 *
 * @note This implementation mirrors the proven esp32_pca9685_bus.hpp /
 *       esp32_pcal95555_bus.hpp pattern for consistent, working I2C communication
 *       across HardFOC drivers. The BNO08x uses the SH-2 packet protocol which
 *       requires raw I2C transfers (no register addressing), so the CommInterface
 *       methods differ from the register-addressed I2cInterface used by the
 *       PCAL95555 and PCA9685 drivers, but the underlying ESP-IDF bus setup,
 *       device handle caching, and resource management are identical.
 *
 * @author Nebiyu Tadesse
 * @date 2025
 * @copyright HardFOC
 */

#pragma once

// System headers
#include <cstdint>
#include <memory>

// Third-party headers (ESP-IDF)
#ifdef __cplusplus
extern "C" {
#endif
#include "driver/gpio.h"
#include "driver/i2c_master.h"
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

static constexpr const char* TAG_I2C = "BNO08x_I2C";

/**
 * @class Esp32Bno08xI2cBus
 * @brief ESP32 implementation of bno08x::CommInterface using ESP-IDF I2C master driver.
 *
 * This class mirrors the proven Esp32Pca9685Bus / Esp32Pcal9555Bus implementations
 * for consistent I2C communication behavior across HardFOC drivers. The BNO08x
 * uses the SH-2 packet protocol over I2C (raw byte transfers, no register
 * addressing), so the interface methods differ from the register-addressed
 * I2cInterface, but the underlying bus management is identical.
 *
 * Key improvements over the previous implementation:
 * - Device handle caching: avoids add_device/rm_device per transaction
 * - Header-only: consistent with other HardFOC ESP32 bus implementations
 * - Proper resource cleanup via Init()/Deinit() pattern
 * - Factory function for convenient instantiation
 */
class Esp32Bno08xI2cBus : public bno08x::CommInterface<Esp32Bno08xI2cBus> {
public:
  /**
   * @brief I2C bus and device configuration structure
   */
  struct I2CConfig {
    i2c_port_t port = I2C_NUM_0;     ///< I2C port number
    gpio_num_t sda_pin = GPIO_NUM_4; ///< SDA pin (default GPIO4, same as pcal95555/pca9685)
    gpio_num_t scl_pin = GPIO_NUM_5; ///< SCL pin (default GPIO5, same as pcal95555/pca9685)
    uint32_t frequency = 400000;     ///< I2C frequency in Hz (default 400kHz)
    uint8_t device_address =
        0x4B; ///< 7-bit I2C device address (default 0x4B, SA0=HIGH; 0x4A if SA0=LOW)
    bool pullup_enable = true;        ///< Enable internal pullups
    gpio_num_t int_pin = GPIO_NUM_17; ///< Interrupt pin (default GPIO17, GPIO_NUM_NC if not used)
    gpio_num_t rst_pin = GPIO_NUM_16; ///< Reset pin (default GPIO16, GPIO_NUM_NC if not used)
  };

  /**
   * @brief Constructor with default configuration
   */
  Esp32Bno08xI2cBus() : Esp32Bno08xI2cBus(I2CConfig{}) {}

  /**
   * @brief Constructor with custom I2C configuration
   * @param config I2C bus and device configuration
   */
  explicit Esp32Bno08xI2cBus(const I2CConfig& config)
      : config_(config), bus_handle_(nullptr), dev_handle_(nullptr), initialized_(false) {}

  /**
   * @brief Destructor - cleans up I2C resources
   */
  ~Esp32Bno08xI2cBus() {
    Deinit();
  }

  // ── CommInterface required methods ─────────────────────────────────────

  /**
   * @brief Identify this transport as I2C (required by CommInterface).
   */
  BNO085Interface GetInterfaceType() noexcept {
    return BNO085Interface::I2C;
  }

  /**
   * @brief Open the I2C bus and initialize communication (required by CommInterface)
   *
   * Initializes the I2C master bus, GPIO pins (interrupt/reset), and creates
   * a cached device handle for the configured BNO08x address. Subsequent
   * calls return true immediately if already initialized.
   *
   * @return true if successful, false otherwise
   */
  bool Open() noexcept {
    return Init();
  }

  /**
   * @brief Close the I2C bus and release resources (required by CommInterface)
   */
  void Close() noexcept {
    Deinit();
  }

  /**
   * @brief Write raw data to the BNO08x sensor (required by CommInterface)
   *
   * Transmits raw bytes over I2C. The BNO08x SH-2 protocol does not use
   * register addressing — all data is sent as raw byte streams.
   *
   * @param data Pointer to data buffer
   * @param length Number of bytes to write
   * @return Number of bytes written, or negative on error
   */
  int Write(const uint8_t* data, uint32_t length) noexcept {
    if (!initialized_ || dev_handle_ == nullptr) {
      ESP_LOGE(TAG_I2C, "I2C bus not initialized");
      return -1;
    }

    esp_err_t ret = i2c_master_transmit(dev_handle_, data, length, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
      ESP_LOGE(TAG_I2C, "I2C write failed: %s (addr=0x%02X, len=%lu)", esp_err_to_name(ret),
               config_.device_address, static_cast<unsigned long>(length));
      return -1;
    }

    return static_cast<int>(length);
  }

  /**
   * @brief Read raw data from the BNO08x sensor (required by CommInterface)
   *
   * Receives raw bytes over I2C. The BNO08x SH-2 protocol does not use
   * register addressing — all data is received as raw byte streams.
   *
   * When an interrupt pin is configured, this function checks it first to avoid
   * unnecessary I2C transactions. If the interrupt pin indicates no data is
   * available (pin is high), it returns 0 immediately.
   *
   * Note: ESP-IDF's i2c_master_receive() automatically handles clock stretching,
   * which is required by the BNO08x datasheet (the sensor can hold SCL low
   * when it needs more time to prepare data).
   *
   * @param data Pointer to receive buffer
   * @param length Number of bytes to read
   * @return Number of bytes read, 0 if no data, or negative on error
   */
  int Read(uint8_t* data, uint32_t length) noexcept {
    if (!initialized_ || dev_handle_ == nullptr) {
      ESP_LOGE(TAG_I2C, "I2C bus not initialized");
      return -1;
    }

    // If interrupt pin is configured, check it first to avoid unnecessary I2C transactions
    if (config_.int_pin != GPIO_NUM_NC && !DataAvailable()) {
      // Interrupt pin indicates no data available (pin is high, active low)
      return 0; // Return 0 to indicate no data (SH-2 library expects this)
    }

    esp_err_t ret = i2c_master_receive(dev_handle_, data, length, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
      // ESP_ERR_TIMEOUT can occur if device is clock stretching (no data yet)
      // This is acceptable for BNO08x - return 0 to indicate no data available
      if (ret == ESP_ERR_TIMEOUT) {
        return 0;
      }
      ESP_LOGE(TAG_I2C, "I2C read failed: %s (addr=0x%02X, len=%lu)", esp_err_to_name(ret),
               config_.device_address, static_cast<unsigned long>(length));
      return -1;
    }

    return static_cast<int>(length);
  }

  /**
   * @brief Probe the I2C device to verify it's responding
   *
   * Performs a simple I2C read to check if the device acknowledges.
   * For BNO08x using SHTP protocol:
   * - ACK on address = device present
   * - Clock stretching (timeout) = device present but no data (OK)
   * - NAK = device not present or not responding
   *
   * @return true if device ACKs (responds), false if NAK
   */
  bool Probe() noexcept {
    if (!initialized_ || dev_handle_ == nullptr) {
      return false;
    }

    // Try to read 1 byte with longer timeout to allow for clock stretching
    // BNO08x will stretch clock if no data (timeout is OK - means device is present)
    // NAK means device not present
    uint8_t dummy;
    esp_err_t ret = i2c_master_receive(dev_handle_, &dummy, 1, pdMS_TO_TICKS(100));

    // Log the actual error for debugging
    if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT) {
      ESP_LOGW(TAG_I2C, "Probe at 0x%02X returned: %s (0x%x)", config_.device_address,
               esp_err_to_name(ret), ret);
    }

    // ESP_OK = data received (ACK + data) - device present and responding
    // ESP_ERR_TIMEOUT = clock stretching exceeded timeout (device present, no data yet - OK for
    // BNO08x) ESP_ERR_INVALID_RESPONSE = NAK during data phase (but ACK on address means device is
    // present!) For BNO08x, if we get ACK on address (which we see on logic analyzer), the device
    // IS present, even if it NAKs during data phase (no data ready) So we accept timeout and also
    // check if it's a response error (which might indicate ACK then NAK)
    if (ret == ESP_OK || ret == ESP_ERR_TIMEOUT) {
      return true;
    }

    // ESP_ERR_INVALID_RESPONSE typically means NAK during data transfer
    // But if address was ACKed (seen on logic analyzer), device is present
    // BNO08x might NAK if it has no data ready, so we should still consider it present
    if (ret == ESP_ERR_INVALID_RESPONSE) {
      ESP_LOGI(TAG_I2C, "Probe got NAK during data phase, but address was ACKed - device present");
      return true; // Device ACKed address, so it's present
    }

    return false; // Other errors indicate real problems
  }

  /**
   * @brief Check if new data is available (required by CommInterface)
   *
   * When an interrupt pin is configured, checks its level (active low).
   * Without an interrupt pin, always returns true (polling mode).
   *
   * @return true if data is available
   */
  bool DataAvailable() noexcept {
    if (config_.int_pin == GPIO_NUM_NC) {
      return true; // No interrupt pin configured, always return true
    }
    // BNO08x INT pin is active low
    return (gpio_get_level(config_.int_pin) == 0);
  }

  /**
   * @brief Delay execution for specified time (required by CommInterface)
   * @param ms Delay duration in milliseconds
   */
  void Delay(uint32_t ms) noexcept {
    vTaskDelay(pdMS_TO_TICKS(ms));
  }

  /**
   * @brief Get current time in microseconds (required by CommInterface)
   * @return Current monotonic time in microseconds
   */
  uint32_t GetTimeUs() noexcept {
    return static_cast<uint32_t>(esp_timer_get_time());
  }

  /**
   * @brief Set a control pin to the specified signal state (required by CommInterface)
   *
   * Maps GpioSignal to physical level based on pin polarity:
   * - RSTN: active-low (ACTIVE → GPIO 0, INACTIVE → GPIO 1)
   * - BOOTN/WAKE/PS0/PS1: not wired in this I2C implementation (no-op)
   *
   * @param pin   Which control pin to drive
   * @param signal ACTIVE to assert, INACTIVE to deassert
   * @return false if ESP-IDF reported a GPIO error (an unwired pin is not an error)
   */
  bool GpioSet(bno08x::CtrlPin pin, bno08x::GpioSignal signal) noexcept {
    switch (pin) {
      case bno08x::CtrlPin::RSTN:
        if (config_.rst_pin != GPIO_NUM_NC) {
          // RSTN is active-low: ACTIVE → drive LOW (assert reset)
          const esp_err_t ret =
              gpio_set_level(config_.rst_pin, signal == bno08x::GpioSignal::ACTIVE ? 0 : 1);
          if (ret != ESP_OK) {
            ESP_LOGE(TAG_I2C, "RSTN write failed: %s", esp_err_to_name(ret));
            return false;
          }
        }
        break;
      case bno08x::CtrlPin::BOOTN:
      case bno08x::CtrlPin::WAKE:
      case bno08x::CtrlPin::PS0:
      case bno08x::CtrlPin::PS1:
        break; // Not wired in this I2C implementation
    }
    return true;
  }

  // ── Lifecycle management ───────────────────────────────────────────────

  /**
   * @brief Initialize the I2C bus, GPIO, and device handle
   *
   * Performs full initialization: GPIO pins, I2C master bus, and cached
   * device handle creation. Returns true immediately on subsequent calls
   * if already initialized.
   *
   * @return true if successful, false otherwise
   */
  bool Init() noexcept {
    if (initialized_) {
      ESP_LOGW(TAG_I2C, "I2C bus already initialized");
      return true;
    }

    ESP_LOGI(TAG_I2C,
             "Initializing I2C bus on port %d (SDA:GPIO%d, SCL:GPIO%d, "
             "Freq:%lu Hz, Addr:0x%02X)",
             config_.port, config_.sda_pin, config_.scl_pin,
             static_cast<unsigned long>(config_.frequency), config_.device_address);

    // Step 1: Configure GPIO pins (interrupt, reset)
    if (!initGPIO()) {
      ESP_LOGE(TAG_I2C, "Failed to initialize GPIO pins");
      return false;
    }

    // Step 2: Create I2C master bus
    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = config_.port;
    bus_config.sda_io_num = config_.sda_pin;
    bus_config.scl_io_num = config_.scl_pin;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = config_.pullup_enable;
    bus_config.intr_priority = 0;
    bus_config.trans_queue_depth = 0;

    esp_err_t ret = i2c_new_master_bus(&bus_config, &bus_handle_);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG_I2C, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
      return false;
    }

    // Step 3: Create and cache device handle for the BNO08x address
    // Note: scl_wait_us sets maximum clock stretching timeout per byte
    // BNO08x can stretch clock when it has no data, so we allow up to 50ms
    // This must be long enough for the device to prepare data or timeout gracefully
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config_.device_address,
        .scl_speed_hz = config_.frequency,
        .scl_wait_us = 50000, // 50ms max clock stretching per byte (BNO08x datasheet requirement)
        .flags = {},
    };

    ret = i2c_master_bus_add_device(bus_handle_, &dev_config, &dev_handle_);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG_I2C, "Failed to add device 0x%02X: %s", config_.device_address,
               esp_err_to_name(ret));
      i2c_del_master_bus(bus_handle_);
      bus_handle_ = nullptr;
      return false;
    }

    initialized_ = true;
    ESP_LOGI(TAG_I2C, "I2C bus initialized successfully");
    return true;
  }

  /**
   * @brief Deinitialize the I2C bus and release all resources
   */
  void Deinit() noexcept {
    if (!initialized_) {
      return;
    }

    // Remove cached device handle before deleting bus
    if (dev_handle_ != nullptr) {
      i2c_master_bus_rm_device(dev_handle_);
      dev_handle_ = nullptr;
    }

    if (bus_handle_ != nullptr) {
      i2c_del_master_bus(bus_handle_);
      bus_handle_ = nullptr;
    }

    // Reset GPIO pins
    if (config_.int_pin != GPIO_NUM_NC) {
      gpio_reset_pin(config_.int_pin);
    }
    if (config_.rst_pin != GPIO_NUM_NC) {
      gpio_reset_pin(config_.rst_pin);
    }

    initialized_ = false;
    ESP_LOGI(TAG_I2C, "I2C bus deinitialized");
  }

  // ── Accessors ──────────────────────────────────────────────────────────

  /**
   * @brief Get the I2C configuration
   * @return Reference to the I2C configuration
   */
  [[nodiscard]] const I2CConfig& getConfig() const noexcept {
    return config_;
  }

  /**
   * @brief Check if the bus is initialized
   * @return true if initialized, false otherwise
   */
  [[nodiscard]] bool isInitialized() const noexcept {
    return initialized_;
  }

  /**
   * @brief Perform hardware reset pulse on the BNO08x
   *
   * Asserts reset (drives RSTN low) for the specified duration, then releases it.
   * This should be called before attempting to communicate with the device.
   *
   * @param lowMs Duration to hold reset low in milliseconds (default 2ms)
   * @param bootDelayMs Delay after releasing reset to allow sensor boot (default 200ms)
   */
  void HardwareReset(uint32_t lowMs = 2, uint32_t bootDelayMs = 200) noexcept {
    if (config_.rst_pin == GPIO_NUM_NC) {
      ESP_LOGW(TAG_I2C, "Reset pin not configured, skipping hardware reset");
      return;
    }

    ESP_LOGI(TAG_I2C, "Performing hardware reset: asserting RSTN for %lu ms",
             static_cast<unsigned long>(lowMs));

    // Assert reset (drive LOW)
    GpioSet(bno08x::CtrlPin::RSTN, bno08x::GpioSignal::ACTIVE);
    Delay(lowMs);

    // Release reset (drive HIGH)
    GpioSet(bno08x::CtrlPin::RSTN, bno08x::GpioSignal::INACTIVE);
    ESP_LOGI(TAG_I2C, "Reset released, waiting %lu ms for sensor boot",
             static_cast<unsigned long>(bootDelayMs));
    Delay(bootDelayMs);
  }

private:
  I2CConfig config_;
  i2c_master_bus_handle_t bus_handle_;
  i2c_master_dev_handle_t dev_handle_; ///< Cached device handle (avoids add/rm per transaction)
  bool initialized_;

  /**
   * @brief Initialize GPIO pins for interrupt and reset
   * @return true if successful, false otherwise
   */
  bool initGPIO() noexcept {
    // Configure interrupt pin (input, pull-up enabled, active low)
    if (config_.int_pin != GPIO_NUM_NC) {
      gpio_config_t io_conf = {};
      io_conf.pin_bit_mask = (1ULL << config_.int_pin);
      io_conf.mode = GPIO_MODE_INPUT;
      io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
      io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
      io_conf.intr_type = GPIO_INTR_DISABLE;

      esp_err_t ret = gpio_config(&io_conf);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG_I2C, "Failed to configure interrupt pin: %s", esp_err_to_name(ret));
        return false;
      }
      ESP_LOGI(TAG_I2C, "Interrupt pin configured: GPIO%d", config_.int_pin);
    }

    // Configure reset pin (output, high by default = not asserted)
    if (config_.rst_pin != GPIO_NUM_NC) {
      gpio_config_t io_conf = {};
      io_conf.pin_bit_mask = (1ULL << config_.rst_pin);
      io_conf.mode = GPIO_MODE_OUTPUT;
      io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
      io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
      io_conf.intr_type = GPIO_INTR_DISABLE;

      esp_err_t ret = gpio_config(&io_conf);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG_I2C, "Failed to configure reset pin: %s", esp_err_to_name(ret));
        return false;
      }

      // Release reset (set high)
      gpio_set_level(config_.rst_pin, 1);
      ESP_LOGI(TAG_I2C, "Reset pin configured: GPIO%d", config_.rst_pin);
    }

    return true;
  }
};

/**
 * @brief Factory function to create an ESP32 BNO08x I2C bus instance
 *
 * Creates and initializes the bus. Returns nullptr on failure.
 *
 * @param config I2C configuration (optional, uses defaults if not provided)
 * @return Unique pointer to Esp32Bno08xI2cBus instance, or nullptr on failure
 */
inline std::unique_ptr<Esp32Bno08xI2cBus> CreateEsp32Bno08xI2cBus(
    const Esp32Bno08xI2cBus::I2CConfig& config = Esp32Bno08xI2cBus::I2CConfig{}) {
  auto bus = std::make_unique<Esp32Bno08xI2cBus>(config);
  if (!bus->Init()) {
    ESP_LOGE(TAG_I2C, "Failed to initialize I2C bus");
    return nullptr;
  }
  return bus;
}
