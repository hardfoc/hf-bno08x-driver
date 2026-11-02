# Porting

Run the driver on a new MCU by writing one class: the communication interface for your bus.

The driver never calls your SDK directly. It calls nine methods on a class you derive from
`bno08x::CommInterface<Self>` (in [`inc/bno08x_comm_interface.hpp`](../inc/bno08x_comm_interface.hpp)). The base
class uses CRTP: the calls are resolved at compile time, so there are no virtual functions and no vtable.

<!-- figure: driver-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="diagrams/driver-layers.dark.svg">
  <img src="diagrams/driver-layers.svg"
    alt="Layer diagram: the application calls the hardware-agnostic BNO085 driver, which uses the CEVA SH-2
    library or its own UART-RVC parser, then the CRTP communication interface, which calls the platform
    port you write, which drives the I2C, SPI or UART bus and the control pins of the BNO08x.">
</picture>
<!-- /figure -->

## What you implement

| Method | Called by | Contract |
| --- | --- | --- |
| `GetInterfaceType()` | `Begin()`, `BeginRvc()`, the DFU calls | Return `I2C`, `SPI`, `UART` or `UARTRVC`; it decides which calls are allowed |
| `Open()` | `Begin()`, `BeginRvc()`, the bootloader helpers, DFU | Initialise the bus and pins. May be called again while open: return `true` without re-initialising |
| `Close()` | `Close()`, `CloseRvc()`, the end of a DFU transfer | Release the bus |
| `Write(data, length)` | SH-2 library, DFU | Send one SHTP transfer (or DFU packet). Return the bytes accepted, 0 if the bus is busy, negative on error |
| `Read(data, length)` | SH-2 library, RVC parser, DFU | SH-2: return one **complete** SHTP transfer, header included, or 0 when there is none. RVC: return whatever bytes have arrived, without blocking |
| `DataAvailable()` | your port, usually inside `Read()` | `true` when H_INTN is low; `true` always when there is no interrupt line |
| `Delay(ms)` | reset and bootloader sequences | Block for at least `ms` milliseconds |
| `GetTimeUs()` | SH-2 timestamps and timeouts, RVC timestamps | A monotonic 32-bit microsecond counter; wrapping is fine |
| `GpioSet(pin, signal)` | `HardwareReset()`, `SetBootPin()`, `SetWakePin()`, `SelectInterface()`, the bootloader helpers | Drive `RSTN`, `BOOTN`, `WAKE`, `PS0`, `PS1`; ignore pins the board does not wire. Return `true`, or `false` on a GPIO error |

`GpioSetActive(pin)` and `GpioSetInactive(pin)` come with the base class.

## GPIO errors

Return `false` from `GpioSet()` when your SDK reports a GPIO failure; a pin the board does not wire is not a
failure, so return `true` for it. The driver passes the status on: `HardwareReset()`, `SetBootPin()`,
`SetWakePin()` and `SelectInterface()` return `false`, and `EnterBootloader()` / `ExitBootloaderAndReboot()` fail
with `SH2_ERR_IO` and close the port again. `HardwareReset()` stops at the failing pin, so it does not wait out a
reset that never happened.

Return a `bool`, not your SDK's status code: the driver rejects any other return type at compile time, so an
`esp_err_t` (where 0 is success) cannot be read as `true`. A port written for the earlier contract, with a `void`
`GpioSet()`, still compiles; the driver treats it as never failing. `Close()` and `Delay()` stay `void`: the
driver has nothing to recover when either goes wrong, so log it in the port.

## I2C port

```cpp title="my_i2c.hpp"
--8<-- "tests/docs/my_i2c.hpp:port"
```

Rules the transfer must follow:

- **Plain transactions.** One START ... STOP per call, no register address, no repeated start.
- **Read the length, then the transfer.** Bits 0 to 14 of the 4-byte SHTP header are the transfer length, header
  included; bit 15 marks a continuation. On I2C every read starts at the header again, so read 4 bytes, then the
  whole length. A zero length is an empty transfer (BNO086 sends them when it has nothing): return 0.
- **Gate on H_INTN.** Reading when the hub has nothing makes a BNO085 stretch the clock until it does.
- **Return a negative value only for a bus failure.** The SH-2 library handles protocol errors.

## UART-RVC port

```cpp title="my_rvc.hpp"
--8<-- "tests/docs/my_rvc.hpp:port"
```

`ServiceRvc()` calls `Read(&byte, 1)` until it returns 0, so `Read()` must not block. Give the UART a receive
buffer of a few frames (19 bytes each, one every 10 ms) so nothing is lost between calls.

## SPI and UART-SHTP ports

The driver supports both through the same interface: report `SPI` or `UART` from `GetInterfaceType()` and keep the
`Read()` contract (one complete SHTP transfer). The repository has no reference port for either yet. Points to
cover:

=== "SPI"

    - Mode 3, MSB first, up to 3 MHz. Wait for H_INTN low before you assert H_CSN.
    - SPI is full duplex: the hub may send while you write. Read the 4-byte header first to learn the length,
      and follow the
      [SHTP specification](https://www.ceva-ip.com/wp-content/uploads/2019/10/Sensor-Hub-Transport-Protocol.pdf)
      for the SPI framing.
    - Map `CtrlPin::WAKE` to the PS0/WAKE GPIO: drive it low to wake the hub before a write.

=== "UART"

    - 3 Mbit/s, 8N1, with an external 32.768 kHz clock or crystal on the BNO08x.
    - SHTP over UART adds framing bytes around each transfer; see the
      [SHTP specification](https://www.ceva-ip.com/wp-content/uploads/2019/10/Sensor-Hub-Transport-Protocol.pdf).
    - Leave at least 100 µs between bytes sent to the hub.

## Control-pin levels

`GpioSignal::ACTIVE` means "function asserted". On the BNO08x that is a low level for NRST, BOOTN and WAKE, and a
high level (logic 1) for PS0 and PS1. Your `GpioSet()` applies the board's levels; inverters or level shifters
change them.

| Pin | ACTIVE level on the chip | Board without the pin |
| --- | --- | --- |
| `RSTN` | low | Tie NRST to the board reset; `HardwareReset()` then only waits |
| `BOOTN` | low | Pull up through 10 kΩ; no firmware updates |
| `WAKE` | low | Not needed on I2C or UART |
| `PS0`, `PS1` | high | Strap them ([Hardware](hardware.md#protocol-selection)) |

## Reference implementation

The ESP32 examples use [`esp32_bno08x_bus.hpp`](../examples/esp32/main/esp32_bno08x_bus.hpp), an ESP-IDF
`i2c_master` port with H_INTN and NRST, and [`esp32_uart_rvc_bus.hpp`](../examples/esp32/main/esp32_uart_rvc_bus.hpp)
for UART-RVC. Start from them for an ESP-IDF target. The I2C reference reads the whole 1024-byte buffer on every
`Read()`; the header-first read on this page moves far fewer bytes.

## Check your port

1. Build it: both ports on this page are compiled by the host CI from [`tests/docs/`](../tests/docs/my_i2c.hpp),
   with the driver instantiated over them.
2. Watch H_INTN after `HardwareReset()`: it goes low about 94 ms after NRST rises when the strapping is right.
3. `Begin()` returns `true`: the SH-2 library received the hub's reset message and a reply to a command.
4. Enable `RotationVector` at 10 ms and count reports for a second: about 100 means reads keep up.

## Threads

The driver is not thread-safe, and the CEVA SH-2 library keeps one global session. Give the sensor one owner task
that calls `Update()`, or guard every call on the driver object (and its port) with one mutex.
