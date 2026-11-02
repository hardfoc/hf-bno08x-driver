---
hide:
  - navigation
  - toc
---

<div class="hf-hero" markdown>
<span class="hf-kicker">HardFOC driver</span>

# BNO08x Driver

<p>A C++20 driver for the CEVA BNO080, BNO085 and BNO086 9-axis IMU with on-chip sensor fusion. It starts the
SH-2 sensor hub, enables the reports you ask for and hands you quaternions, vectors and events, on any MCU with
I2C, SPI or UART.</p>

[Quickstart](quickstart.md){ .md-button .md-button--primary }
[API guide](api-guide.md){ .md-button }
[Source on GitHub](https://github.com/hardfoc/hf-bno08x-driver){ .md-button }
</div>

The driver has no platform code. You give it a small class that moves bytes over your bus, reports when the hub
has data and drives a reset pin; it does the rest: the SH-2 session through CEVA's own SH-2 library, report
decoding, re-enabling reports after a hub reset, the UART-RVC frame parser and firmware updates.

<div class="hf-chip" markdown>
<div class="hf-chip__head">
<span class="hf-chip__kicker">9-axis IMU with sensor-fusion hub</span>
<span class="hf-chip__name">BNO08x</span>
<span class="hf-chip__vendor">CEVA (formerly Hillcrest Labs)</span>
</div>

Sensors
:   Accelerometer (±8 g), gyroscope (±2000 °/s) and magnetometer, fused on chip by the SH-2 firmware

Outputs
:   Rotation vectors up to 400 Hz (gyro-integrated up to 1 kHz), calibrated and raw IMU data, steps, taps, activity

Host interface
:   I2C up to 400 kHz (address 0x4A / 0x4B), SPI mode 3 up to 3 MHz, UART 3 Mbit/s, or UART-RVC 115200

Supply
:   VDD 2.4 V to 3.6 V (sensors), VDDIO 1.7 V to 3.6 V (core and I/O)

Package
:   28-pin LGA, 3.8 mm × 5.2 mm × 1.1 mm, −40 °C to 85 °C

Driver modes
:   SH-2 over I2C, SPI or UART; UART-RVC streaming; firmware update through the bootloader

[Datasheet](datasheet/BNO080_085-Datasheet.pdf) ·
[SH-2 reference manual](https://www.ceva-ip.com/wp-content/uploads/2019/10/SH-2-Reference-Manual.pdf) ·
[SHTP specification](https://www.ceva-ip.com/wp-content/uploads/2019/10/Sensor-Hub-Transport-Protocol.pdf)
</div>

## What you get

- **One object per sensor.** `BNO085<YourPort>` covers SH-2 reports, UART-RVC and firmware updates; the port's
  interface type decides which calls are allowed. See [API guide](api-guide.md).
- **Every SH-2 report.** 40 report types, from rotation vectors and calibrated IMU data to step counter, tap,
  stability and activity classifiers, decoded into one `SensorEvent`. See [Configuration](configuration.md).
- **Callbacks or polling.** Register a callback, or ask `HasNewData()` and read `GetLatest()`; both work at once.
  No threads inside: you call `Update()`.
- **Survives hub resets.** When the hub reports a reset, the driver re-enables every report you had configured.
- **UART-RVC and firmware update.** Heading, pitch, roll and acceleration at 100 Hz with no protocol stack
  ([UART-RVC mode](rvc.md)), and in-field firmware updates through the bootloader ([Firmware update](dfu.md)).
- **Zero-overhead portability.** The port is a CRTP class: no virtual calls, no RTOS dependency.
  See [Porting](porting.md).
- **Tested without hardware.** Host unit tests run the driver against a mock transport, including the
  datasheet's own RVC frame. See [Testing and CI](testing.md).

## Where to go next

<div class="grid cards" markdown>

-   :material-rocket-launch-outline: **[Quickstart](quickstart.md)**

    ---

    Wire the sensor, write the port, print orientation in about 20 lines.

-   :material-chip: **[Hardware](hardware.md)**

    ---

    Pins, protocol selection, bus settings, start-up timing and power.

-   :material-layers-outline: **[Architecture](architecture.md)**

    ---

    The layers, the SH-2 library underneath and what each one owns.

-   :material-code-braces: **[API guide](api-guide.md)**

    ---

    Start, enable reports, read, handle errors, task by task.

-   :material-swap-horizontal: **[Porting](porting.md)**

    ---

    Implement the communication interface for your MCU.

-   :material-book-open-variant: **[API reference](api-reference.md)**

    ---

    Every class and member, generated from the headers.

</div>
