# Firmware update

Replace the BNO08x's SH-2 firmware in the field through its bootloader, with an image you supply.

!!! warning "Use an image from CEVA for your exact part"
    A wrong or interrupted image leaves the hub in the bootloader. Keep BOOTN on a GPIO so you can always re-enter
    it, keep the supply stable during the transfer, and check that the image's part number matches your part.

## What the bootloader needs

- **BOOTN low at reset.** The bootloader samples BOOTN with PS0 and PS1 when NRST rises. Wire BOOTN to a GPIO
  (with its 10 kΩ pull-up) and NRST to another.
- **I2C, SPI or UART.** The bootloader uses the interface the PS pins select; UART-RVC has no bootloader mode, so
  the DFU calls fail with `SH2_ERR_BAD_PARAM` on a `UARTRVC` port.
- **A different I2C address.** Over I2C the bootloader answers at `0x28` (SA0 low) or `0x29` (SA0 high), not
  `0x4A` / `0x4B`. The driver does not switch addresses: your port must talk to the bootloader address between
  `EnterBootloader()` and `ExitBootloaderAndReboot()`.
- **A vendor image.** CEVA supplies BNO08x firmware as an HcBin image with metadata (`FW-Format`, `SW-Part-Number`).
  The image compiled in from `src/dfu/firmware-bno.c` is a 64-byte stub that the driver refuses.

## Update from memory

```cpp
--8<-- "tests/docs/dfu.cpp:dfu"
```

`RunDfuFromMemory()` does the whole sequence and returns `SH2_OK` (0) or a negative SH-2 code:

<div class="hf-steps" markdown>

1. **Enter the bootloader.** `EnterBootloader(10, 50)`: close any open session, open the port, assert BOOTN, hold
   NRST low 10 ms, wait 94 ms (`RESET_STARTUP_MS`), release BOOTN, wait 50 ms.
2. **Check the image.** `FW-Format` must be `BNO_V1` and `SW-Part-Number` a known part (or the one you require),
   and the image at least 1024 bytes; otherwise `SH2_ERR_BAD_PARAM` before anything is sent.
3. **Send the size.** The application length as 4 bytes big-endian plus CRC-16, then the packet length (1 to 64
   bytes) plus CRC-16. The bootloader acknowledges each with `s`.
4. **Send the image.** Packets of up to 64 bytes, each with a CRC-16 (CCITT, polynomial 0x1021, initial value
   0xFFFF), each acknowledged; up to 5 attempts and 100 ms per packet. `options.progress` runs after each one.
5. **Reboot.** `ExitBootloaderAndReboot(2, 100)`: BOOTN high, NRST low 2 ms, wait 94 ms plus 100 ms. Then call
   `Begin()` as usual.

</div>

## Options

| `DfuOptions` field | Default | Effect |
| --- | --- | --- |
| `requireFormatMatch` | `true` | Check the image's `FW-Format` against `requiredFormat` |
| `requiredFormat` | `"BNO_V1"` | Expected format |
| `requirePartNumber` | `true` | Check `SW-Part-Number` against `requiredPartNumber`, or, when that is null, against the known part numbers `1000-3608`, `1000-3676`, `1000-4148`, `1000-4563` |
| `requiredPartNumber` | `nullptr` | Exact part number to require |
| `packetLenOverride` | 0 | Packet length 1 to 64; 0 uses the image's preference, capped at 64 |
| `progress` | empty | Called with 0 bytes before the first packet, then after each packet |

`DfuMemoryImage` describes a raw image in memory: `data`, `length`, `format`, `partNumber` and an optional
`preferredPacketLen`. For an image you stream from storage, implement the `HcBin_t` function table
([`src/dfu/HcBin.h`](../src/dfu/HcBin.h)) and call `DfuWithOptions(image, options)` between the bootloader calls.

## Step by step

Call the pieces yourself when you need to do something between them, such as switching the I2C address:

| Call | Does |
| --- | --- |
| `EnterBootloader(resetLowMs = 10, settleMs = 50)` | BOOTN + reset sequence above; `false` on a `UARTRVC` port or during a transfer |
| `DfuFromMemory(image, options)` | Steps 2 to 4 from a `DfuMemoryImage` |
| `DfuWithOptions(hcbin, options)` / `Dfu(hcbin)` | Steps 2 to 4 from any `HcBin_t`; `Dfu()` uses the default options |
| `ExitBootloaderAndReboot(resetLowMs = 2, settleMs = 100)` | BOOTN released, reset, wait |

During the transfer `GetState()` is `DfuInProgress` and `Close()` is refused. A transfer that starts while an SH-2
session is open closes the session first.

## Next

The [`dfu_update` and `dfu_workflow_test`](examples.md) ESP32 apps run this on hardware;
[Hardware](hardware.md#control-pins) has the BOOTN and NRST wiring.
