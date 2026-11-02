# Quickstart

Read orientation from a BNO08x over I2C: wire seven signals, write a small port class, call four driver functions.

## 1. Wire it

| MCU | BNO08x | Notes |
| --- | --- | --- |
| I2C SCL | H_SCL (pin 19) | 100 or 400 kHz; 2 kΩ to 4 kΩ pull-up to VDDIO |
| I2C SDA | H_SDA (pin 20) | Pull-up as for SCL |
| GPIO in | H_INTN (pin 14) | Active low: the hub has data. Use a pin that can wake the MCU |
| GPIO out | NRST (pin 11) | Active low reset; the driver pulses it in `HardwareReset()` |
| GND | PS0 (pin 6), PS1 (pin 5) | Both low selects I2C; sampled at reset |
| GND or VDDIO | SA0 (pin 17) | Low: address 0x4A, high: 0x4B |
| 3.3 V, GND | VDD, VDDIO, GND | VDD must be up no later than VDDIO |

Pull BOOTN (pin 4) high through 10 kΩ, or wire it to a GPIO if you want [firmware updates](dfu.md). Breakout
boards usually strap PS0, PS1, SA0 and BOOTN already; check yours. The [Hardware](hardware.md) page has the full
pin list, SPI and UART wiring and the timing.

## 2. Add the driver

Add the repository as a submodule with its SH-2 library and link the `hf_bno08x` CMake target.
[Installation](installation.md) covers CMake and ESP-IDF.

```bash
git submodule add https://github.com/hardfoc/hf-bno08x-driver.git external/hf-bno08x-driver
git submodule update --init external/hf-bno08x-driver/src/sh2
```

## 3. Write the port

The port is the only platform code. It moves bytes over I2C, reads H_INTN, drives NRST and BOOTN, waits and keeps
time. Replace the `platform_*` calls with your SDK's.

```cpp title="my_i2c.hpp"
--8<-- "tests/docs/my_i2c.hpp:port"
```

`Read()` returns one complete SHTP transfer: it reads the 4-byte header for the length, then reads the whole
transfer again from the start. [Porting](porting.md) explains each method and shows the UART-RVC port.

## 4. Read orientation

```cpp title="main.cpp"
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

What each step does:

1. `HardwareReset()` holds NRST low for 2 ms and waits 50 ms. PS0, PS1, SA0 and BOOTN are sampled now.
2. `Begin()` opens the port, starts the CEVA SH-2 library, waits for the hub's reset message and sends a
   reinitialise command. Nothing else works until it returns `true`.
3. `EnableSensor()` asks the hub for a rotation vector every 10 ms (100 Hz).
4. `Update()` services the SH-2 library: it reads pending transfers, decodes reports and runs your callbacks.
   Call it at least as often as your fastest report.

`accuracy` goes from 0 (unreliable) to 3 (high). Move the board through a few orientations to calibrate the
magnetometer. This example is built by the host CI, so it compiles against the current headers.

## If it does not work

| Symptom | First thing to check |
| --- | --- |
| `Begin()` returns `false`, `GetLastError()` is `SH2_ERR` (-1) | `Open()` failed: bus pins, pull-ups, the address (0x4A or 0x4B) |
| `Begin()` returns `false` with `SH2_ERR_TIMEOUT` (-6) | H_INTN not wired or read inverted; PS0/PS1 not both low at reset |
| No reports after `EnableSensor()` | `Update()` not called often enough; `HasNewData()` checked for another sensor |
| `accuracy` stays at 0 | Magnetic interference near the board; move it in a figure eight |

[Troubleshooting](troubleshooting.md) has more.

## Next

- [API guide](api-guide.md): callbacks, polling, several reports, errors.
- [Configuration](configuration.md): report rates and the 40 report types.
- [UART-RVC mode](rvc.md): orientation over a single UART line, no protocol.
