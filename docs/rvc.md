# UART-RVC mode

Heading, pitch, roll and acceleration at 100 Hz over one UART line, with no protocol stack and no commands.

UART-RVC is a simplified output mode CEVA designed for ground robots such as robot vacuum cleaners. The hub sends a
fixed 19-byte frame every 10 ms and listens to nothing. You cannot choose reports, change the rate or update
firmware in this mode; for any of that, use SH-2 over I2C, SPI or UART.

## Select it

Strap PS1 low and PS0 high, then reset (datasheet figure 1-5). The UART runs at 115200 bit/s, 8 data bits, no
parity, 1 stop bit, and needs the external 32.768 kHz crystal or clock: the internal oscillator is not accurate
enough. Only the chip's TX pin (pin 20) is used; connect it to your MCU's RX.

## Use it

The port reports `BNO085Interface::UARTRVC` and returns received bytes from `Read()` without blocking:

```cpp title="my_rvc.hpp"
--8<-- "tests/docs/my_rvc.hpp:port"
```

```cpp
--8<-- "tests/docs/rvc.cpp:rvc"
```

`BeginRvc()` opens the port and starts the parser; it fails with `SH2_ERR_BAD_PARAM` on any other interface type.
`ServiceRvc()` reads every byte that has arrived and calls your callback once per valid frame. `CloseRvc()` (or
`Close()`) closes the port. SH-2 calls on an RVC object fail the same way, so one object cannot mix the modes.

## Frame format

<div class="hf-registers" markdown>

| Offset | Bytes | Field | Unit | `RvcSensorValue` |
| --- | --- | --- | --- | --- |
| `0` | 2 | Header `0xAA 0xAA` | | |
| `2` | 1 | Index, counts 0 to 255 | | `index` |
| `3` | 2 | Yaw, little-endian `int16`, ±180° | 0.01° | `yaw_deg` |
| `5` | 2 | Pitch, ±90° | 0.01° | `pitch_deg` |
| `7` | 2 | Roll, ±180° | 0.01° | `roll_deg` |
| `9` | 2 | X acceleration | mg | `acc_x_g` |
| `11` | 2 | Y acceleration | mg | `acc_y_g` |
| `13` | 2 | Z acceleration | mg | `acc_z_g` |
| `15` | 1 | Motion intent (BNO086; reserved otherwise) | | `mi` |
| `16` | 1 | Motion request (BNO086; reserved otherwise) | | `mr` |
| `17` | 1 | Reserved, zero | | |
| `18` | 1 | Checksum: 8-bit sum of bytes 2 to 17 | | |

</div>

Yaw is measured from the orientation at reset. Apply the rotations in the order yaw, pitch, roll. The driver
converts to degrees and g (`RvcSensorValue`) and stamps each frame with `GetTimeUs()`.

The datasheet's example frame, `AA AA DE 01 00 92 FF 25 08 8D FE EC FF D1 03 00 00 00 E7`, decodes to index 222,
yaw 0.01°, pitch −1.10°, roll 20.85° and acceleration (−0.371, −0.020, 0.977) g. The host tests feed exactly this
frame to the parser ([Testing and CI](testing.md)).

## How the parser finds frames

The parser keeps a 19-byte sliding window. Each byte shifts the window; when it starts with `0xAA 0xAA` and the
checksum matches, the frame is decoded, the callback runs and the window empties. A frame with a wrong checksum is
skipped and the window keeps sliding, so the parser recovers by itself after noise or a partial frame.

## Next

[Porting](porting.md#uart-rvc-port) has the port's contract; the [`rvc_mode`](examples.md) ESP32 example runs it
on hardware.
