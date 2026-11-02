# HF-BNO08x Documentation

Welcome! This directory contains step-by-step guides for installing, building, and using the **HF-BNO08x** library.

The driver supports **I²C**, **SPI**, **UART (SH-2)**, and **UART RVC** via a single `CommInterface`. Your transport reports its type with `GetInterfaceType()` so the driver enables the right mode (`Begin()` for SH-2, `BeginRvc()` for RVC, `Dfu()` for firmware update where supported). See [Platform Integration](platform_integration.md).

## Documentation Structure

### **Getting Started**

1. **[🛠️ Installation](installation.md)** – Prerequisites and how to obtain the source
2. **[⚡ Quick Start](quickstart.md)** – Minimal working example to get you running
3. **[🔌 Hardware Setup](hardware_setup.md)** – Wiring diagrams and pin connections

### **Integration**

4. **[🔧 Platform Integration](platform_integration.md)** – Implement the CRTP communication interface for your platform
5. **[⚙️ Configuration](configuration.md)** – Configuration options and settings

### **Reference**

6. **[📖 API Reference](api_reference.md)** – Complete API documentation
7. **[💡 Examples](examples.md)** – Detailed example walkthroughs

### **Advanced Features**

8. **[🧹 RVC Mode](special_feature_rvc.md)** – Robot Vacuum Cleaner mode for simplified UART streaming
9. **[📦 DFU (Firmware Update)](special_feature_dfu.md)** – Device Firmware Update guide

### **Troubleshooting**

10. **[🐛 Troubleshooting](troubleshooting.md)** – Common issues and solutions

---

## Quick Start Path

**New to BNO08x?** Follow this recommended path:

1. Start with **[Installation](installation.md)** to prepare your environment
2. Follow **[Hardware Setup](hardware_setup.md)** to wire your hardware
3. Read **[Quick Start](quickstart.md)** for a minimal working example
4. Check **[Platform Integration](platform_integration.md)** to implement the communication interface
5. Explore **[Examples](examples.md)** for more advanced usage

---

## Need Help?

- **🐛 Found a bug?** Check the [Troubleshooting](troubleshooting.md) guide
- **❓ Have questions?** Review the [API Reference](api_reference.md)
- **📝 Want to contribute?** See the contributing guidelines in the main README

---

**Navigation**
➡️ [Installation](installation.md)
