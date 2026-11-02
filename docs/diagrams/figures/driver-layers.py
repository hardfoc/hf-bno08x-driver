"""The layers of the driver: hardware-agnostic core, the CEVA SH-2 library, communication interface, platform port.

Reads: From top to bottom. Your application talks to one BNO085<Comm> object, whose public calls are listed from
inc/bno08x.hpp. In SH-2 mode the core hands every transfer to the CEVA SH-2 and SHTP library (the src/sh2
submodule); in UART-RVC mode it parses the 19-byte frames itself; firmware updates use the bootloader protocol in
the core. Both paths end in the communication interface (bno08x::CommInterface in inc/bno08x_comm_interface.hpp),
a CRTP base whose methods your platform port implements: move bytes, report data-ready, wait, keep time and drive
the control pins. Below it is the sensor hub.
"""

import re
from pathlib import Path

from hardfoc_mkdocs_theme.figkit import Fig

ROOT = Path(__file__).resolve().parents[3]
DRIVER = "inc/bno08x.hpp"
COMM = "inc/bno08x_comm_interface.hpp"


def data():
    drv = (ROOT / DRIVER).read_text()
    cls = drv[drv.index("class BNO085 {"):]
    public = cls[: cls.index("private:")]
    calls = []
    for name in re.findall(r"^  (?:static constexpr )?(?:bool|void|int|SensorEvent|BNO085DriverState|uint8_t)\s+([A-Z]\w+)\(", public, re.M):
        if name not in calls and not name.startswith("GetDriverVersion"):
            calls.append(name)
    comm = (ROOT / COMM).read_text()
    hooks = []
    for name in re.findall(r"^\s+(?:return )?(?:static_cast<Derived\*>\(this\)->|detail::)(\w+)\(", comm, re.M):
        if name not in hooks:
            hooks.append(name)
    return {
        "calls": calls,
        "port_hooks": hooks,
        "sources": [f"{DRIVER}:BNO085 public members", f"{COMM}:CommInterface"],
    }


def build(d):
    f = Fig(
        "driver-layers",
        "Three layers, one of them yours",
        alt="Layer diagram: the application calls the hardware-agnostic BNO085 driver, which uses the CEVA SH-2 "
        "library or its own UART-RVC parser, then the CRTP communication interface, which calls the platform port "
        "you write, which drives the I2C, SPI or UART bus and the control pins of the BNO08x.",
        width=1200,
        kicker="Architecture",
        subtitle="The driver and the interface are the same on every MCU. The port is a small class with nine methods.",
        profile="overview",
        legend=[("brand", "this library"), ("amber", "CEVA SH-2 library"), ("teal", "you write"), ("slate", "hardware")],
    )
    W = f.body_w
    app = f.card(0, 0, W, "Your application", ["fusion consumer, control loop, logger ..."], tone="slate")
    top = app.b + 40
    chips_h = f.chips(16, f.region_inner_top(top), W - 32, d["calls"], tone="brand")
    reg = f.region(0, top, W, f.region_inner_top(top) - top + chips_h + 18, "Hardware-agnostic core", tone="brand",
                   sub="BNO085<Comm>, inc/bno08x.hpp")
    half = (W - 40) / 2
    sh2 = f.card(0, reg.b + 40, half, "CEVA SH-2 and SHTP",
                 ["src/sh2 submodule: sensor configuration,", "report decoding, transport framing"],
                 tone="amber", accent=True)
    rvc = f.card(half + 40, reg.b + 40, half, "UART-RVC parser and DFU",
                 ["in the core: 19-byte frames at 100 Hz;", "bootloader transfer with CRC-16"],
                 tone="brand", accent=True)
    comm = f.card(0, max(sh2.b, rvc.b) + 40, W, "Communication interface",
                  ["bno08x::CommInterface<Derived>: CRTP base, resolved at compile time, no vtable;",
                   "control pins by name (RSTN, BOOTN, WAKE, PS0, PS1) as ACTIVE / INACTIVE"],
                  tone="brand", accent=True)
    port = f.card(0, comm.b + 40, W, "Platform port (your class)", ["  ".join(h + "()" for h in d["port_hooks"])],
                  tone="teal", accent=True, mono=True)
    hw = f.card(0, port.b + 40, W, "BNO080 / BNO085 / BNO086",
                ["I2C up to 400 kHz, SPI mode 3 up to 3 MHz, UART 3 Mbit/s or UART-RVC 115200  /  H_INTN, NRST, BOOTN"],
                tone="slate")
    f.connect(app, reg, "bottom", "top", tone="rail")
    f.connect(reg, sh2, "bottom", "top", tone="rail")
    f.connect(reg, rvc, "bottom", "top", tone="rail")
    f.connect(sh2, comm, "bottom", "top", tone="rail")
    f.connect(rvc, comm, "bottom", "top", tone="rail")
    for a, b in ((comm, port), (port, hw)):
        f.connect(a, b, "bottom", "top", tone="rail")
    return f
