/**
 * @file esp32_bno08x_spi_bus.hpp
 * @brief ESP32 SPI communication interface for the BNO08x driver (SH-2 over SPI).
 *
 * SPI mode is selected with PS1 = 1 and PS0 = 1 from before reset until the first
 * H_INTN assertion; afterwards PS0 is the active-low WAKE input (datasheet 1000-3927
 * figure 1-5 and section 1.2.4, note 5), so PS0/WAKE must be on a GPIO. The bus is
 * mode 3 (CPOL = 1, CPHA = 1), MSB first (section 1.2.4.2), at most 3 MHz (section
 * 6.5.2). H_INTN is required: the hub asserts it when it has a transfer and after a
 * wake (section 1.2.4.3).
 *
 * Read(): when H_INTN is low, assert CS, clock the 4-byte SHTP header, then the rest of
 * the transfer in the same chip-select (datasheet section 1.3.1, figure 1-26), and
 * release CS. Write(): drive WAKE low, wait for H_INTN, then one full-duplex transfer
 * of the outgoing bytes; whatever the hub clocks out at the same time is kept and
 * returned by the next Read() (the SHTP length field tells how much of it is valid).
 *
 * Chip select is driven as a GPIO so that header and payload share one CS assertion.
 * Firmware updates over SPI use the bootloader's own protocol; this port implements
 * SH-2 only.
 *
 * @copyright HardFOC
 */

#pragma once

#include <cstdint>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sh2/sh2_err.h"
#include "sh2/sh2_hal.h"
#ifdef __cplusplus
}
#endif

#include "../../../inc/bno08x_comm_interface.hpp"

static constexpr const char* TAG_SPI = "BNO08x_SPI";

/**
 * @class Esp32Bno08xSpiBus
 * @brief ESP32 implementation of bno08x::CommInterface for SPI mode.
 */
class Esp32Bno08xSpiBus : public bno08x::CommInterface<Esp32Bno08xSpiBus> {
public:
  /**
   * @brief SPI bus and control-pin configuration.
   */
  struct SpiConfig {
    spi_host_device_t host = SPI2_HOST; ///< SPI host
    gpio_num_t mosi_pin = GPIO_NUM_11;  ///< MOSI -> BNO08x H_MOSI
    gpio_num_t miso_pin = GPIO_NUM_13;  ///< MISO <- BNO08x H_MISO
    gpio_num_t sclk_pin = GPIO_NUM_12;  ///< SCLK -> BNO08x H_SCL/SCK
    gpio_num_t cs_pin = GPIO_NUM_10;    ///< CS -> BNO08x H_CSN (driven as a GPIO)
    gpio_num_t int_pin = GPIO_NUM_17;   ///< H_INTN, active low (required)
    gpio_num_t rst_pin = GPIO_NUM_16;   ///< NRST, active low (GPIO_NUM_NC if not used)
    gpio_num_t wake_pin = GPIO_NUM_18;  ///< PS0/WAKE (required in SPI mode)
    gpio_num_t boot_pin = GPIO_NUM_NC;  ///< BOOTN, active low (GPIO_NUM_NC if not used)
    gpio_num_t ps1_pin = GPIO_NUM_NC;   ///< PS1 if on a GPIO (else strapped high)
    int clock_hz = 3000000;             ///< SCLK, at most 3 MHz (datasheet 6.5.2)
    uint32_t wake_timeout_us = 200000;  ///< Wait for H_INTN after WAKE (typ 150 µs, fig. 6-12)
  };

  Esp32Bno08xSpiBus() : Esp32Bno08xSpiBus(SpiConfig{}) {}
  explicit Esp32Bno08xSpiBus(const SpiConfig& config) : config_(config) {}

  ~Esp32Bno08xSpiBus() {
    Close();
  }

  // ── CommInterface required methods ─────────────────────────────────────

  BNO085Interface GetInterfaceType() noexcept {
    return BNO085Interface::SPI;
  }

  /**
   * @brief Configure the control pins, the SPI bus and the device (once).
   */
  bool Open() noexcept {
    if (initialized_) {
      return true;
    }
    if (config_.int_pin == GPIO_NUM_NC || config_.wake_pin == GPIO_NUM_NC) {
      ESP_LOGE(TAG_SPI, "SPI mode needs H_INTN and PS0/WAKE on GPIOs");
      return false;
    }
    if (!initGpio()) {
      ESP_LOGE(TAG_SPI, "GPIO configuration failed");
      return false;
    }

    spi_bus_config_t bus{};
    bus.mosi_io_num = config_.mosi_pin;
    bus.miso_io_num = config_.miso_pin;
    bus.sclk_io_num = config_.sclk_pin;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = static_cast<int>(MAX_IN);
    esp_err_t err = spi_bus_initialize(config_.host, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
      ESP_LOGE(TAG_SPI, "spi_bus_initialize failed: %s", esp_err_to_name(err));
      return false;
    }

    spi_device_interface_config_t dev{};
    dev.mode = 3; // CPOL = 1, CPHA = 1 (datasheet 1.2.4.2)
    dev.clock_speed_hz = config_.clock_hz;
    dev.spics_io_num = -1; // CS is a GPIO: header and payload in one assertion
    dev.queue_size = 1;
    err = spi_bus_add_device(config_.host, &dev, &device_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG_SPI, "spi_bus_add_device failed: %s", esp_err_to_name(err));
      spi_bus_free(config_.host);
      return false;
    }

    pendingLen_ = 0;
    initialized_ = true;
    ESP_LOGI(TAG_SPI, "SPI bus open (host %d, %d Hz, mode 3)", static_cast<int>(config_.host),
             config_.clock_hz);
    return true;
  }

  void Close() noexcept {
    if (!initialized_) {
      return;
    }
    spi_bus_remove_device(device_);
    device_ = nullptr;
    spi_bus_free(config_.host);
    pendingLen_ = 0;
    initialized_ = false;
  }

  /**
   * @brief Send one SHTP transfer: WAKE, wait for H_INTN, one full-duplex CS transfer.
   * @return @p length when sent, 0 while an earlier received transfer is unread,
   *         negative on error or timeout.
   */
  int Write(const uint8_t* data, uint32_t length) noexcept {
    if (!initialized_) {
      return SH2_ERR_IO;
    }
    if (length == 0 || length > SH2_HAL_MAX_TRANSFER_OUT) {
      return SH2_ERR_BAD_PARAM;
    }
    if (pendingLen_ > 0) {
      return 0; // the SH-2 library reads it first, then retries
    }

    gpio_set_level(config_.wake_pin, 0); // WAKE asserted (active low)
    const bool ready = waitForInt(config_.wake_timeout_us);
    if (!ready) {
      gpio_set_level(config_.wake_pin, 1);
      ESP_LOGW(TAG_SPI, "H_INTN did not assert after WAKE");
      return SH2_ERR_TIMEOUT;
    }

    gpio_set_level(config_.cs_pin, 0);
    const bool ok = transfer(data, pending_, length);
    gpio_set_level(config_.cs_pin, 1);
    gpio_set_level(config_.wake_pin, 1); // WAKE released
    if (!ok) {
      return SH2_ERR_IO;
    }

    // The hub may have sent a transfer at the same time; keep what was clocked of it.
    const uint32_t rxLen = headerLength(pending_);
    if (rxLen > 0) {
      pendingLen_ = rxLen < length ? rxLen : length;
    }
    return static_cast<int>(length);
  }

  /**
   * @brief Return one SHTP transfer (header included), or 0 when H_INTN is high.
   */
  int Read(uint8_t* data, uint32_t length) noexcept {
    if (!initialized_) {
      return SH2_ERR_IO;
    }
    if (pendingLen_ > 0) {
      const uint32_t n = pendingLen_ < length ? pendingLen_ : length;
      std::memcpy(data, pending_, n);
      pendingLen_ = 0;
      return static_cast<int>(n);
    }
    if (!DataAvailable() || length < HEADER_LEN) {
      return 0;
    }

    gpio_set_level(config_.cs_pin, 0);
    bool ok = transfer(nullptr, data, HEADER_LEN);
    uint32_t total = ok ? headerLength(data) : 0;
    if (total > length) {
      total = length; // the hub sends the rest later as a continuation (datasheet 1.3.1)
    }
    if (ok && total > HEADER_LEN) {
      ok = transfer(nullptr, data + HEADER_LEN, total - HEADER_LEN);
    }
    gpio_set_level(config_.cs_pin, 1);

    if (!ok) {
      return SH2_ERR_IO;
    }
    return total > HEADER_LEN ? static_cast<int>(total) : 0;
  }

  /**
   * @brief H_INTN low: the hub has a transfer (or answers a wake).
   */
  bool DataAvailable() noexcept {
    return gpio_get_level(config_.int_pin) == 0;
  }

  void Delay(uint32_t ms) noexcept {
    vTaskDelay(pdMS_TO_TICKS(ms));
  }

  uint32_t GetTimeUs() noexcept {
    return static_cast<uint32_t>(esp_timer_get_time());
  }

  /**
   * @brief Drive NRST / BOOTN / WAKE (active low) and PS0 / PS1 (active high).
   *
   * PS0 and WAKE are the same pin: PS0 before reset, WAKE afterwards.
   * @return false on an ESP-IDF GPIO error (an unwired pin is not an error)
   */
  bool GpioSet(bno08x::CtrlPin pin, bno08x::GpioSignal signal) noexcept {
    const bool active = signal == bno08x::GpioSignal::ACTIVE;
    gpio_num_t gpio = GPIO_NUM_NC;
    int level = 0;
    switch (pin) {
    case bno08x::CtrlPin::RSTN:
      gpio = config_.rst_pin;
      level = active ? 0 : 1;
      break;
    case bno08x::CtrlPin::BOOTN:
      gpio = config_.boot_pin;
      level = active ? 0 : 1;
      break;
    case bno08x::CtrlPin::WAKE:
      gpio = config_.wake_pin;
      level = active ? 0 : 1;
      break;
    case bno08x::CtrlPin::PS0:
      gpio = config_.wake_pin;
      level = active ? 1 : 0;
      break;
    case bno08x::CtrlPin::PS1:
      gpio = config_.ps1_pin;
      level = active ? 1 : 0;
      break;
    }
    if (gpio == GPIO_NUM_NC) {
      return true;
    }
    return gpio_set_level(gpio, level) == ESP_OK;
  }

private:
  static constexpr uint32_t HEADER_LEN = 4;
  static constexpr std::size_t MAX_IN = SH2_HAL_MAX_TRANSFER_IN;

  /// Bits 14:0 of the SHTP length field (bit 15 marks a continuation; datasheet figure 1-26).
  static uint32_t headerLength(const uint8_t* hdr) noexcept {
    const uint32_t len =
        (static_cast<uint32_t>(hdr[0]) | (static_cast<uint32_t>(hdr[1]) << 8)) & 0x7FFFU;
    return len == 0x7FFFU ? 0 : len; // 0xFFFF is reserved: a bus with nothing driving MISO
  }

  /// One full-duplex SPI transaction inside the current CS assertion; tx == nullptr sends zeros.
  bool transfer(const uint8_t* tx, uint8_t* rx, uint32_t n) noexcept {
    spi_transaction_t t{};
    t.length = n * 8;
    t.rxlength = n * 8;
    t.tx_buffer = tx != nullptr ? tx : zeros_;
    t.rx_buffer = rx;
    const esp_err_t err = spi_device_polling_transmit(device_, &t);
    if (err != ESP_OK) {
      ESP_LOGE(TAG_SPI, "SPI transfer of %lu bytes failed: %s", static_cast<unsigned long>(n),
               esp_err_to_name(err));
      return false;
    }
    return true;
  }

  bool waitForInt(uint32_t timeout_us) noexcept {
    const uint32_t start = GetTimeUs();
    while (!DataAvailable()) {
      if (GetTimeUs() - start > timeout_us) {
        return false;
      }
      esp_rom_delay_us(10);
    }
    return true;
  }

  bool configureOutput(gpio_num_t gpio, int level) noexcept {
    if (gpio == GPIO_NUM_NC) {
      return true;
    }
    gpio_config_t io{};
    io.pin_bit_mask = 1ULL << gpio;
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    return gpio_config(&io) == ESP_OK && gpio_set_level(gpio, level) == ESP_OK;
  }

  bool initGpio() noexcept {
    gpio_config_t io{};
    io.pin_bit_mask = 1ULL << config_.int_pin;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&io) != ESP_OK) {
      return false;
    }
    // CS idle high, NRST and BOOTN released; PS0/WAKE and PS1 high select SPI at reset.
    return configureOutput(config_.cs_pin, 1) && configureOutput(config_.rst_pin, 1) &&
           configureOutput(config_.boot_pin, 1) && configureOutput(config_.wake_pin, 1) &&
           configureOutput(config_.ps1_pin, 1);
  }

  SpiConfig config_;
  bool initialized_{false};
  spi_device_handle_t device_{nullptr};

  uint8_t pending_[MAX_IN]{}; ///< bytes the hub clocked out during a Write()
  uint32_t pendingLen_{0};
  uint8_t zeros_[MAX_IN]{}; ///< MOSI data while reading
};
