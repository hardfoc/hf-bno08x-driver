/**
 * @file esp32_bno08x_uart_bus.hpp
 * @brief ESP32 UART-SHTP communication interface for the BNO08x driver (SH-2 over UART).
 *
 * UART-SHTP mode is selected with PS1 = 1, PS0 = 0 at reset (datasheet 1000-3927
 * figure 1-5). The line runs at 3 Mbit/s 8N1 and needs the BNO08x external clock or
 * crystal (section 1.2.3). Every SHTP transfer travels in a frame (flag 0x7E, protocol
 * ID, byte-stuffed payload, flag 0x7E); before each frame the host sends a Buffer
 * Status Query and waits for the hub to report enough free space, then sends the frame
 * with at least 100 µs between bytes (section 1.2.3.1). The framing itself is the
 * portable bno08x::shtp_uart code in inc/bno08x_shtp_uart.hpp.
 *
 * Read() returns one complete SHTP transfer per call, or 0. Write() follows the
 * SH-2 HAL contract: it returns 0 ("busy, call again") while it waits for the hub's
 * buffer status, so the SH-2 library keeps servicing Read() meanwhile.
 *
 * Firmware updates over UART use the bootloader's own raw-byte protocol at 115200
 * bit/s; this port implements SH-2 only.
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
#include "driver/uart.h"
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
#include "../../../inc/bno08x_shtp_uart.hpp"

static constexpr const char* TAG_UART_SHTP = "BNO08x_UART";

/**
 * @class Esp32Bno08xUartBus
 * @brief ESP32 implementation of bno08x::CommInterface for UART-SHTP mode.
 */
class Esp32Bno08xUartBus : public bno08x::CommInterface<Esp32Bno08xUartBus> {
public:
  /**
   * @brief UART and control-pin configuration.
   */
  struct UartConfig {
    uart_port_t port = UART_NUM_1;                     ///< UART port
    gpio_num_t tx_pin = GPIO_NUM_21;                   ///< ESP32 TX -> BNO08x RX (H_SDA/RX)
    gpio_num_t rx_pin = GPIO_NUM_20;                   ///< ESP32 RX <- BNO08x TX (H_SCL/TX)
    uint32_t baud_rate = bno08x::shtp_uart::BAUD_RATE; ///< 3 Mbit/s (datasheet 1.2.3.1)
    gpio_num_t int_pin = GPIO_NUM_17;                  ///< H_INTN (optional, GPIO_NUM_NC)
    gpio_num_t rst_pin = GPIO_NUM_16;                  ///< NRST, active low (GPIO_NUM_NC)
    gpio_num_t boot_pin = GPIO_NUM_NC;                 ///< BOOTN, active low (GPIO_NUM_NC)
    gpio_num_t ps0_pin = GPIO_NUM_NC;                  ///< PS0 if on a GPIO (else strapped)
    gpio_num_t ps1_pin = GPIO_NUM_NC;                  ///< PS1 if on a GPIO (else strapped)
    uint32_t buffer_status_timeout_us = 200000;        ///< Wait for the hub's buffer status
  };

  Esp32Bno08xUartBus() : Esp32Bno08xUartBus(UartConfig{}) {}
  explicit Esp32Bno08xUartBus(const UartConfig& config) : config_(config) {}

  ~Esp32Bno08xUartBus() {
    Close();
  }

  // ── CommInterface required methods ─────────────────────────────────────

  BNO085Interface GetInterfaceType() noexcept {
    return BNO085Interface::UART;
  }

  /**
   * @brief Configure the control pins and install the UART driver (once).
   */
  bool Open() noexcept {
    if (initialized_) {
      return true;
    }
    if (!initGpio()) {
      return false;
    }

    uart_config_t cfg{};
    cfg.baud_rate = static_cast<int>(config_.baud_rate);
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;

    esp_err_t err = uart_driver_install(config_.port, RX_RING_SIZE, 0, 0, nullptr, 0);
    if (err != ESP_OK) {
      ESP_LOGE(TAG_UART_SHTP, "uart_driver_install failed: %s", esp_err_to_name(err));
      return false;
    }
    err = uart_param_config(config_.port, &cfg);
    if (err == ESP_OK) {
      err = uart_set_pin(config_.port, config_.tx_pin, config_.rx_pin, UART_PIN_NO_CHANGE,
                         UART_PIN_NO_CHANGE);
    }
    if (err != ESP_OK) {
      ESP_LOGE(TAG_UART_SHTP, "UART configuration failed: %s", esp_err_to_name(err));
      uart_driver_delete(config_.port);
      return false;
    }

    resetFraming();
    initialized_ = true;
    ESP_LOGI(TAG_UART_SHTP, "UART-SHTP bus open on port %d (TX:%d, RX:%d, %lu bit/s)",
             static_cast<int>(config_.port), static_cast<int>(config_.tx_pin),
             static_cast<int>(config_.rx_pin), static_cast<unsigned long>(config_.baud_rate));
    return true;
  }

  void Close() noexcept {
    if (!initialized_) {
      return;
    }
    uart_driver_delete(config_.port);
    resetFraming();
    initialized_ = false;
  }

  /**
   * @brief Send one SHTP transfer (SH-2 HAL contract).
   *
   * First call: frame the transfer, send the Buffer Status Query and return 0.
   * Later calls (the SH-2 library retries while servicing Read()): once the hub
   * reports at least the frame's size free, send the frame byte by byte with
   * HOST_BYTE_GAP_US spacing and return @p length.
   *
   * @return @p length when sent, 0 while waiting, negative on error or timeout.
   */
  int Write(const uint8_t* data, uint32_t length) noexcept {
    if (!initialized_) {
      return SH2_ERR_IO;
    }
    if (!txPending_) {
      txLen_ = bno08x::shtp_uart::EncodeFrame(data, length, txFrame_, sizeof(txFrame_));
      if (txLen_ == 0) {
        return SH2_ERR_BAD_PARAM;
      }
      decoder_.ClearBufferSpace();
      if (!sendSpaced(bno08x::shtp_uart::BUFFER_STATUS_QUERY,
                      sizeof(bno08x::shtp_uart::BUFFER_STATUS_QUERY))) {
        return SH2_ERR_IO;
      }
      txPending_ = true;
      txStartUs_ = GetTimeUs();
      return 0;
    }

    // Look for the buffer status. Stop at an SHTP transfer the library has not read yet.
    while (decoder_.BufferSpace() < txLen_ && pendingLen_ == 0) {
      const int n = pump(pending_, sizeof(pending_));
      if (n > 0) {
        pendingLen_ = static_cast<uint32_t>(n);
      } else if (n == 0) {
        break; // no more bytes for now
      } else {
        txPending_ = false;
        return n;
      }
    }
    if (decoder_.BufferSpace() >= txLen_) {
      txPending_ = false;
      decoder_.ClearBufferSpace();
      return sendSpaced(txFrame_, txLen_) ? static_cast<int>(length) : SH2_ERR_IO;
    }
    if (GetTimeUs() - txStartUs_ > config_.buffer_status_timeout_us) {
      ESP_LOGW(TAG_UART_SHTP, "no buffer status from the hub; frame dropped");
      txPending_ = false;
      return SH2_ERR_TIMEOUT;
    }
    return 0;
  }

  /**
   * @brief Return one complete SHTP transfer received from the hub, or 0 (non-blocking).
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
    return pump(data, length);
  }

  /**
   * @brief True when bytes are buffered or H_INTN is low.
   */
  bool DataAvailable() noexcept {
    size_t buffered = 0;
    if (initialized_ && uart_get_buffered_data_len(config_.port, &buffered) == ESP_OK &&
        buffered > 0) {
      return true;
    }
    if (config_.int_pin != GPIO_NUM_NC) {
      return gpio_get_level(config_.int_pin) == 0;
    }
    return rxPos_ < rxLen_ || pendingLen_ > 0;
  }

  void Delay(uint32_t ms) noexcept {
    vTaskDelay(pdMS_TO_TICKS(ms));
  }

  uint32_t GetTimeUs() noexcept {
    return static_cast<uint32_t>(esp_timer_get_time());
  }

  /**
   * @brief Drive NRST, BOOTN (active low) and PS0 / PS1 (active high) if wired.
   *
   * WAKE is an SPI-mode function and is not used on UART.
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
    case bno08x::CtrlPin::PS0:
      gpio = config_.ps0_pin;
      level = active ? 1 : 0;
      break;
    case bno08x::CtrlPin::PS1:
      gpio = config_.ps1_pin;
      level = active ? 1 : 0;
      break;
    case bno08x::CtrlPin::WAKE:
      break;
    }
    if (gpio == GPIO_NUM_NC) {
      return true;
    }
    return gpio_set_level(gpio, level) == ESP_OK;
  }

private:
  static constexpr int RX_RING_SIZE = 2048;
  static constexpr std::size_t RX_CHUNK = 64;
  static constexpr std::size_t MAX_IN = SH2_HAL_MAX_TRANSFER_IN;

  /**
   * @brief Decode buffered bytes until an SHTP transfer completes.
   * @return Its length (copied to @p out), 0 when the input is exhausted, negative on error.
   */
  int pump(uint8_t* out, uint32_t cap) noexcept {
    using Event = Decoder::Event;
    while (true) {
      if (rxPos_ >= rxLen_) {
        const int got = uart_read_bytes(config_.port, rxChunk_, RX_CHUNK, 0);
        if (got < 0) {
          return SH2_ERR_IO;
        }
        if (got == 0) {
          return 0;
        }
        rxLen_ = static_cast<std::size_t>(got);
        rxPos_ = 0;
      }
      while (rxPos_ < rxLen_) {
        if (decoder_.Push(rxChunk_[rxPos_++]) == Event::ShtpFrame) {
          const std::size_t n = decoder_.Size() < cap ? decoder_.Size() : cap;
          std::memcpy(out, decoder_.Data(), n);
          return static_cast<int>(n);
        }
      }
    }
  }

  /**
   * @brief Send bytes one at a time, at least HOST_BYTE_GAP_US apart (datasheet 1.2.3.1).
   */
  bool sendSpaced(const uint8_t* bytes, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i) {
      if (uart_write_bytes(config_.port, &bytes[i], 1) != 1) {
        return false;
      }
      if (uart_wait_tx_done(config_.port, pdMS_TO_TICKS(10)) != ESP_OK) {
        return false;
      }
      esp_rom_delay_us(bno08x::shtp_uart::HOST_BYTE_GAP_US);
    }
    return true;
  }

  void resetFraming() noexcept {
    decoder_.Reset();
    rxPos_ = rxLen_ = 0;
    pendingLen_ = 0;
    txPending_ = false;
    txLen_ = 0;
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
    if (config_.int_pin != GPIO_NUM_NC) {
      gpio_config_t io{};
      io.pin_bit_mask = 1ULL << config_.int_pin;
      io.mode = GPIO_MODE_INPUT;
      io.pull_up_en = GPIO_PULLUP_ENABLE;
      io.pull_down_en = GPIO_PULLDOWN_DISABLE;
      io.intr_type = GPIO_INTR_DISABLE;
      if (gpio_config(&io) != ESP_OK) {
        return false;
      }
    }
    // NRST and BOOTN released (high); PS1 = 1, PS0 = 0 selects UART-SHTP (figure 1-5).
    return configureOutput(config_.rst_pin, 1) && configureOutput(config_.boot_pin, 1) &&
           configureOutput(config_.ps1_pin, 1) && configureOutput(config_.ps0_pin, 0);
  }

  using Decoder = bno08x::shtp_uart::FrameDecoder<MAX_IN + 1>;

  UartConfig config_;
  bool initialized_{false};

  Decoder decoder_{};
  uint8_t rxChunk_[RX_CHUNK]{};
  std::size_t rxPos_{0};
  std::size_t rxLen_{0};

  uint8_t pending_[MAX_IN]{}; ///< SHTP transfer received while Write() waited
  uint32_t pendingLen_{0};

  uint8_t txFrame_[bno08x::shtp_uart::MaxEncodedSize(SH2_HAL_MAX_TRANSFER_OUT)]{};
  std::size_t txLen_{0};
  bool txPending_{false};
  uint32_t txStartUs_{0};
};
