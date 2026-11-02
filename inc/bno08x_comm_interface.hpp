#pragma once
/**
 * @file bno08x_comm_interface.hpp
 * @brief CRTP-based communication interface for the BNO08x IMU family
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 */
#include <cstdint>
#include <type_traits>
#include <utility>

// ============================================================================
// Interface Type Enum
// ============================================================================

/**
 * @enum BNO085Interface
 * @brief Identifies the host interface type that a CommInterface provides.
 *
 * Returned by CommInterface::GetInterfaceType(). The BNO085 driver uses this
 * value to guard which operations are valid at runtime:
 * - **SH-2 mode** (Begin, Update, EnableSensor, Dfu): requires `I2C`, `SPI`, or `UART`.
 * - **RVC mode** (BeginRvc, ServiceRvc, CloseRvc): requires `UARTRVC`.
 *
 * The enum value is the PS1:PS0 pin pair (PS1 = bit 1, PS0 = bit 0) that
 * selects the interface at reset, per datasheet 1000-3927 figure 1-5.
 */
enum class BNO085Interface : uint8_t {
  I2C = 0,     ///< I2C bus (PS1=0, PS0=0). Supports SH-2 and DFU.
  UARTRVC = 1, ///< UART in RVC mode (PS1=0, PS0=1). RVC frames only.
  UART = 2,    ///< UART in SH-2 mode (PS1=1, PS0=0). Supports SH-2 and DFU.
  SPI = 3      ///< SPI bus (PS1=1, PS0=1). Supports SH-2 and DFU.
};

// ============================================================================
// CommInterface CRTP Base
// ============================================================================

namespace bno08x {

// ============================================================================
// GPIO Enums -- Standardised Control Pin Model
// ============================================================================

/**
 * @enum CtrlPin
 * @brief Identifies the hardware control pins of the BNO08x.
 *
 * Used with `GpioSet()` / `GpioSetActive()` / `GpioSetInactive()` to
 * control the sensor's dedicated GPIO pins through the CommInterface.
 *
 * The mapping from `GpioSignal::ACTIVE` / `INACTIVE` to physical HIGH / LOW
 * is determined by the platform bus implementation, based on the board's
 * active-level design:
 * - **RSTN**:  Active-low (ACTIVE → physical LOW, asserts reset)
 * - **BOOTN**: Active-low (ACTIVE → physical LOW, enters bootloader)
 * - **WAKE**:  Active-low (ACTIVE → physical LOW, wakes sensor from suspend)
 * - **PS0**:   Active-high (ACTIVE → physical HIGH, PS0 = 1)
 * - **PS1**:   Active-high (ACTIVE → physical HIGH, PS1 = 1)
 */
enum class CtrlPin : uint8_t {
  RSTN = 0, ///< Hardware reset (active-low on the physical pin)
  BOOTN,    ///< Bootloader entry (active-low on the physical pin)
  WAKE,     ///< Wake from suspend, SPI mode only (active-low on the physical pin)
  PS0,      ///< Protocol select bit 0 (active-high on the physical pin)
  PS1       ///< Protocol select bit 1 (active-high on the physical pin)
};

/**
 * @enum GpioSignal
 * @brief Abstract signal level for control pins.
 *
 * Decouples the driver's intent from the physical pin polarity. The platform
 * bus implementation translates `ACTIVE` / `INACTIVE` to the correct
 * electrical level for each pin.
 */
enum class GpioSignal : uint8_t {
  INACTIVE = 0, ///< Pin function is deasserted
  ACTIVE   = 1  ///< Pin function is asserted
};

namespace detail {

/** @brief Return type of `Port::GpioSet(CtrlPin, GpioSignal)`. */
template <typename Port>
using GpioSetResult =
    decltype(std::declval<Port&>().GpioSet(std::declval<CtrlPin>(), std::declval<GpioSignal>()));

/** @brief True when the port's GpioSet() reports a status (returns bool). */
template <typename Port>
inline constexpr bool kGpioSetReportsStatus =
    std::is_same_v<std::remove_cv_t<std::remove_reference_t<GpioSetResult<Port>>>, bool>;

/**
 * @brief Call the port's GpioSet() and return its status.
 *
 * A `bool` result is returned as-is; a `void` GpioSet() (the earlier
 * contract) is treated as always successful. Any other return type is
 * rejected at compile time so that an SDK status code (for example an
 * `esp_err_t`, where 0 means success) cannot be read as a bool.
 */
template <typename Port>
inline bool GpioSet(Port& port, CtrlPin pin, GpioSignal signal) noexcept {
  using Result = GpioSetResult<Port>;
  static_assert(std::is_void_v<Result> || kGpioSetReportsStatus<Port>,
                "bno08x: the port's GpioSet(pin, signal) must return bool (true = pin driven, "
                "false = GPIO error), or void for a port that never reports errors");
  if constexpr (std::is_void_v<Result>) {
    port.GpioSet(pin, signal);
    return true;
  } else {
    return static_cast<bool>(port.GpioSet(pin, signal));
  }
}

/// @cond BNO08X_INTERNAL
template <typename Port, typename = void>
struct HasI2cAddressHooks : std::false_type {};

template <typename Port>
struct HasI2cAddressHooks<
    Port, std::void_t<decltype(std::declval<Port&>().GetI2cAddress()),
                      decltype(std::declval<Port&>().SetI2cAddress(std::declval<uint8_t>()))>>
    : std::true_type {};
/// @endcond

/**
 * @brief True when the port provides the optional I2C address hooks.
 *
 * `uint8_t GetI2cAddress()` returns the 7-bit address the port talks to and
 * `bool SetI2cAddress(uint8_t)` changes it (also while the bus is open). The
 * driver uses them to reach the bootloader at 0x28 / 0x29 during a firmware
 * update over I2C (datasheet 1000-3927 section 1.4) and to restore the
 * application address afterwards.
 */
template <typename Port>
inline constexpr bool kHasI2cAddressHooks = HasI2cAddressHooks<Port>::value;

} // namespace detail

/**
 * @brief CRTP base class for BNO08x communication interfaces.
 *
 * Platform-specific implementations inherit from this template with themselves
 * as the template parameter. All method calls are resolved at compile time
 * via `static_cast<Derived*>(this)->Method()` -- no vtable, no indirection.
 *
 * @tparam Derived  The concrete implementation class (CRTP pattern).
 *
 * @note  Implementations must provide **all** listed methods. The `GpioSet()`
 *        method may be a no-op for pins that are not wired on the board.
 *
 * Optional hooks, detected at compile time (the base class has no default for
 * them, so a port without them keeps compiling):
 * - `uint8_t GetI2cAddress()` and `bool SetI2cAddress(uint8_t addr7)`: I2C
 *   ports should provide both. During a firmware update the driver switches
 *   the port to the bootloader address (0x28 with SA0 low, 0x29 with SA0
 *   high; datasheet section 1.4) and restores the application address
 *   (0x4A / 0x4B, figure 1-12) afterwards. Without them the port itself must
 *   talk to the bootloader address during DFU.
 */
template <typename Derived>
class CommInterface {
public:
  // --------------------------------------------------------------------------
  /// @name Interface Identification
  /// @{

  /**
   * @brief Report the transport type this implementation provides.
   *
   * The BNO085 driver calls this once during Begin() or BeginRvc() to
   * determine which operations to allow.
   *
   * @return  BNO085Interface enum identifying the bus protocol.
   */
  BNO085Interface GetInterfaceType() noexcept {
    return static_cast<Derived*>(this)->GetInterfaceType();
  }

  /// @}

  // --------------------------------------------------------------------------
  /// @name Bus I/O -- Required Methods
  /// @{

  /**
   * @brief Open and initialise the communication bus.
   *
   * Called by BNO085::Begin() (SH-2 mode) or BNO085::BeginRvc() (RVC mode).
   * Implementations should configure the peripheral, set up GPIO pins, and
   * prepare for data transfer.
   *
   * @return  `true` on success, `false` on failure.
   */
  bool Open() noexcept {
    return static_cast<Derived*>(this)->Open();
  }

  /**
   * @brief Close the communication bus and release resources.
   *
   * Called during driver shutdown. Implementations should de-initialise the
   * peripheral and release any allocated resources.
   *
   * Returns `void` on purpose: the vendor `sh2_Hal_t::close` hook is `void`,
   * and the driver has nothing to recover on teardown. Log failures in the port.
   */
  void Close() noexcept {
    static_cast<Derived*>(this)->Close();
  }

  /**
   * @brief Write raw bytes to the sensor.
   *
   * For I2C/SPI this sends data on the bus. For UART-RVC this is typically
   * unused (RVC mode is read-only).
   *
   * @param[in]  data    Pointer to the buffer to transmit.
   * @param[in]  length  Number of bytes to write.
   * @return  Number of bytes written, or negative on error.
   */
  int Write(const uint8_t* data, uint32_t length) noexcept {
    return static_cast<Derived*>(this)->Write(data, length);
  }

  /**
   * @brief Read raw bytes from the sensor.
   *
   * For SH-2 mode, the vendor SHTP library calls this to receive packets.
   * For RVC mode, the driver calls `Read(&byte, 1)` repeatedly to feed the
   * internal 19-byte frame parser.
   *
   * @param[out] data    Buffer to fill with received bytes.
   * @param[in]  length  Maximum number of bytes to read.
   * @return  Number of bytes actually read, 0 if no data available,
   *          or negative on error.
   */
  int Read(uint8_t* data, uint32_t length) noexcept {
    return static_cast<Derived*>(this)->Read(data, length);
  }

  /**
   * @brief Check whether the sensor has data ready.
   *
   * Typically checks the interrupt (INT) pin level. For transports without
   * an interrupt pin, return `true` unconditionally to allow polling.
   *
   * @return  `true` if data is available (or if unknown), `false` otherwise.
   */
  bool DataAvailable() noexcept {
    return static_cast<Derived*>(this)->DataAvailable();
  }

  /**
   * @brief Block for a specified duration.
   *
   * Used by the driver for reset timing and post-DFU delays. Returns `void`:
   * a delay has no failure the driver could act on.
   *
   * @param[in] ms  Delay duration in milliseconds.
   */
  void Delay(uint32_t ms) noexcept {
    static_cast<Derived*>(this)->Delay(ms);
  }

  /**
   * @brief Return the current monotonic time in microseconds.
   *
   * The vendor SH-2 library requires a microsecond time source for
   * timestamping sensor events. The value does not need to represent
   * wall-clock time -- only monotonicity matters.
   *
   * @return  Current time in microseconds.
   */
  uint32_t GetTimeUs() noexcept {
    return static_cast<Derived*>(this)->GetTimeUs();
  }

  /// @}

  // --------------------------------------------------------------------------
  /// @name GPIO Pin Control
  ///
  /// Unified interface for controlling BNO08x hardware control pins. The
  /// platform bus implementation maps GpioSignal::ACTIVE/INACTIVE to the
  /// correct physical level for each pin based on its polarity.
  /// @{

  /**
   * @brief Set a control pin to the specified signal state.
   *
   * The bus implementation is responsible for mapping ACTIVE/INACTIVE to
   * the correct physical level based on the pin's active-level design.
   *
   * The port's GpioSet() returns `bool`: `true` when the pin was driven (or
   * is not wired, so there is nothing to drive), `false` on a GPIO error.
   * A port written for the earlier contract, with a `void` GpioSet(), still
   * compiles and is treated as never failing (see detail::GpioSet()).
   *
   * @param[in] pin     Which control pin to drive.
   * @param[in] signal  ACTIVE to assert the pin function, INACTIVE to deassert.
   * @return  `true` on success, `false` if the port reported a GPIO error.
   */
  bool GpioSet(CtrlPin pin, GpioSignal signal) noexcept {
    return detail::GpioSet(*static_cast<Derived*>(this), pin, signal);
  }

  /**
   * @brief Assert a control pin (set to ACTIVE).
   *
   * Convenience wrapper for `GpioSet(pin, GpioSignal::ACTIVE)`.
   *
   * @param[in] pin  Which control pin to assert.
   * @return  `true` on success, `false` if the port reported a GPIO error.
   */
  bool GpioSetActive(CtrlPin pin) noexcept {
    return GpioSet(pin, GpioSignal::ACTIVE);
  }

  /**
   * @brief Deassert a control pin (set to INACTIVE).
   *
   * Convenience wrapper for `GpioSet(pin, GpioSignal::INACTIVE)`.
   *
   * @param[in] pin  Which control pin to deassert.
   * @return  `true` on success, `false` if the port reported a GPIO error.
   */
  bool GpioSetInactive(CtrlPin pin) noexcept {
    return GpioSet(pin, GpioSignal::INACTIVE);
  }

  /// @}

protected:
  /// @brief Protected constructor -- prevents direct instantiation.
  CommInterface() = default;
  /// @brief Protected destructor -- prevent polymorphic deletion through base.
  ~CommInterface() = default;

  CommInterface(const CommInterface&) = delete;                 ///< Non-copyable.
  CommInterface& operator=(const CommInterface&) = delete;      ///< Non-copyable.
  CommInterface(CommInterface&&) noexcept = default;            ///< Movable.
  CommInterface& operator=(CommInterface&&) noexcept = default; ///< Movable.
};

} // namespace bno08x
