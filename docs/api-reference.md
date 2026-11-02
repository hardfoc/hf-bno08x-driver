# API reference

Every class, member and type of the driver, generated from the headers by Doxygen.

[Open the API reference](api/index.html){ .md-button .md-button--primary }

## Start here

| Page | What it documents |
| --- | --- |
| [`BNO085`](api/class_b_n_o085.html) | The driver: SH-2, UART-RVC, firmware update and pin calls |
| [`bno08x::CommInterface`](api/classbno08x_1_1_comm_interface.html) | The interface your port derives from |
| [Sensor types](api/group___sensor_types.html) | `BNO085Sensor`, `SensorEvent`, RVC and DFU types, callbacks |
| [`SensorEvent`](api/struct_sensor_event.html) | The decoded SH-2 report |
| [`DfuOptions`](api/struct_dfu_options.html) | Firmware-update validation and progress |
| [File list](api/files.html) | Every header |

The [API guide](api-guide.md) shows the same API by task, with code that compiles. The SH-2 library's own API
(`sh2_*`) is documented in its headers in `src/sh2/` and in the
[SH-2 reference manual](https://www.ceva-ip.com/wp-content/uploads/2019/10/SH-2-Reference-Manual.pdf).
