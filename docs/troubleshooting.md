# Troubleshooting

Symptoms you may see while bringing up a BNO08x, their causes and the fix for each, cheapest check first.

Start with `GetLastError()` after the call that failed and with a scope on H_INTN: together they tell a wiring or
strapping problem (H_INTN never goes low) from a transport problem (H_INTN goes low but the SH-2 library times
out) and from a usage problem (the call is refused for the interface or state).

## The hub does not start

| Symptom | Cause | Fix |
| --- | --- | --- |
| `Begin()` is `false`, `GetLastError()` is `SH2_ERR` (-1) | The port's `Open()` returned `false` | Check the bus initialisation, pins and clock |
| `Begin()` is `false` with `SH2_ERR_TIMEOUT` (-6) | The hub never answered the reinitialise command | Measure H_INTN after reset; check PS0/PS1 (both low for I2C) and the I2C address (0x4A with SA0 low, 0x4B high) |
| H_INTN never goes low after reset | Not powered, held in reset, or in the bootloader | VDD before or with VDDIO; NRST high; BOOTN pulled up; CAP has its 100 nF |
| `Begin()` is `false` with `SH2_ERR_BAD_PARAM` (-2) | The port reports `UARTRVC` | Use `BeginRvc()`, or report `I2C` / `SPI` / `UART` |
| `Begin()` is `false` with `SH2_ERR_OP_IN_PROGRESS` (-3) | An RVC session or firmware update is active | `Close()` first; check `GetState()` |
| `Begin()` is `false` with `BNO08X_ERR_SH2_SESSION_IN_USE` (-100) | Another `BNO085` object already runs SH-2; the CEVA library has one session per program | Use one SH-2 object, or `Close()` the other one first |
| Works after power-up, fails after an MCU reset | The hub kept running and its state no longer matches | Call `HardwareReset()` before `Begin()` |

## Reports are missing or wrong

| Symptom | Cause | Fix |
| --- | --- | --- |
| `EnableSensor()` is `false` with `SH2_ERR_OP_IN_PROGRESS` | Called before `Begin()` succeeded | Check `Begin()`'s result first |
| No reports, no error | `Update()` not called, or not often enough | Call it at least once per fastest report period |
| Reports arrive at a different rate than asked | The hub rounds to its own sample rates (0.9x to 2.1x) | Expected; read the timestamps |
| Some reports drop when many are enabled | Bus or hub bandwidth exceeded | Lower rates, raise the I2C clock to 400 kHz, or use SPI |
| Reports stop for a moment, then come back | The hub reset (supply dip, ESD, NRST glitch); the driver re-enabled them | Check the supply and NRST |
| Heading drifts or jumps; `accuracy` stays at 0 or 1 | Magnetometer not calibrated, or magnetic disturbance | Move the device through several orientations; keep motors and magnets away; use `GameRotationVector` if heading is not needed |
| Every read is slow (tens of ms) over I2C | `Read()` reads the whole 1024-byte buffer each time | Read the 4-byte header first, then the transfer ([Porting](porting.md#i2c-port)) |
| I2C bus hangs | BNO085 stretches SCL when read with no data | Gate `Read()` on H_INTN; set a clock-stretch timeout |

## UART-RVC

| Symptom | Cause | Fix |
| --- | --- | --- |
| `BeginRvc()` is `false` with `SH2_ERR_BAD_PARAM` | The port does not report `UARTRVC` | Return `BNO085Interface::UARTRVC` from `GetInterfaceType()` |
| No frames at all | PS1/PS0 not 0/1 at reset, or RX not on the chip's TX (pin 20) | Strap PS1 low and PS0 high; check the wiring |
| Garbage or occasional frames | Baud rate off: no external 32.768 kHz clock, or not 115200 8N1 | Fit the crystal; check the UART settings |
| Frames lost under load | `Read()` blocks, or the receive buffer is too small | Make `Read()` non-blocking; buffer a few 19-byte frames |

## Firmware update

| Symptom | Cause | Fix |
| --- | --- | --- |
| `SH2_ERR_BAD_PARAM` at once, nothing sent | The image failed the checks: format, part number, or shorter than 1024 bytes (the built-in stub) | Use the vendor image; set `requiredPartNumber` or `requirePartNumber = false` deliberately |
| `SH2_ERR_TIMEOUT` on the first packet | The bootloader did not acknowledge | Over I2C, talk to `0x28` / `0x29`; check BOOTN was low when NRST rose |
| The hub stays in the bootloader | Transfer interrupted or wrong image | Enter the bootloader again and repeat with the right image |

## Where else to look

- [Hardware](hardware.md): pins, straps, timing.
- [Porting](porting.md): the `Read()` contract and the control-pin levels.
- [API guide](api-guide.md#handle-errors): what each error code means.
