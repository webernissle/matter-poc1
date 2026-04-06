# M5NanoC6 Matter Temperature Sensor

This project demonstrates a **Matter-over-Wi-Fi temperature sensor** using the **M5Stack NanoC6** (ESP32-C6) development board. The device uses the ESP32-C6's built-in temperature sensor and can be commissioned into **Apple Home** via a HomePod mini (Matter border router).

**Based on:** [ESP-Matter Sensors Example](https://github.com/espressif/esp-matter/tree/release/v1.5/examples/sensors)  
**Tested with:** ESP-IDF v5.4.1 + ESP-Matter v1.5

---

## Hardware

| Component | Details |
|-----------|---------|
| **Board** | [M5Stack NanoC6](https://docs.m5stack.com/en/core/M5NanoC6) |
| **MCU** | ESP32-C6 (RISC-V, 160 MHz) |
| **Flash** | 4 MB |
| **RAM** | 512 KB SRAM |
| **Wireless** | Wi-Fi 6 (802.11ax) + Bluetooth 5.3 |
| **Temperature Sensor** | ESP32-C6 on-chip sensor (±2°C accuracy) |
| **USB** | USB-C (Serial + JTAG) |

> **Note:** This project uses the **ESP32-C6 internal temperature sensor** - no external sensors required!

---

## Quick Start

See **[SETUP.md](SETUP.md)** for detailed installation and build instructions.

### Prerequisites
- ESP-IDF v5.4.1
- ESP-Matter SDK v1.5
- M5Stack NanoC6 board
- USB-C cable

### Build & Flash

```bash
# Set up environment
source ~/esp/esp-idf/export.sh

# Navigate to project
cd ~/repos/matter-poc1/m5nanoc6-temperature

# Configure for ESP32-C6
idf.py set-target esp32c6

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyACM0 flash monitor
```

---

## Commissioning to Apple Home

After flashing, the device can be commissioned into Apple Home via a Matter-compatible border router (e.g., HomePod mini).

### 1. Start the Device

```bash
# Flash and monitor serial output
idf.py -p /dev/ttyACM0 flash monitor
```

### 2. Find QR Code

The firmware now prints the Matter onboarding codes at startup. Look for:

```
I (12345) app_main: QR Code: MT:Y.K9042C00KA0648G00
I (12346) app_main: Manual pairing code: 34970112332
```

Check in main/app_main.cpp the define settings!

If these lines are not visible in your monitor session, run this in the device shell:

```
matter onboardingcodes
```

### 3. Commission in Apple Home

1. Open **Home** app on iPhone/iPad
2. Tap **+** → **Add Accessory**
3. Scan the QR code from serial output, or
4. Enter the **manual pairing code**: `34970112332`
5. Follow on-screen instructions

### 4. Verify Connection

Once paired, the temperature reading should appear in the Home app and update periodically.

### Wi-Fi Defaults in This Repo

- Default Wi-Fi SSID is preconfigured as `SCHOFSEGGEL`.
- Default Wi-Fi password is empty (`""`). If your network is secured, set it in `idf.py menuconfig` under `Component config -> CHIP Device Layer -> Wi-Fi Station`.

---

## Using chip-tool (Alternative Controller)

For testing with the Matter `chip-tool` command-line controller:

```bash
# Commission via BLE + Wi-Fi
chip-tool pairing ble-wifi 1 YOUR_SSID YOUR_PASSWORD 20202021 3840

# Read temperature (endpoint 1)
chip-tool temperaturemeasurement read measured-value 1 1

# Subscribe to temperature updates
chip-tool temperaturemeasurement subscribe measured-value 3 10 1 1
```

---

## Device Details

### Matter Endpoints

| Endpoint | Device Type | Description |
|----------|-------------|-------------|
| **0** | Root Node | Main device node |
| **1** | Temperature Sensor | ESP32-C6 internal temperature sensor |

### Cluster Support

**Endpoint 1 (Temperature Sensor):**
- Temperature Measurement (0x0402)
- Identify (0x0003)

### Default Credentials

| Parameter | Default Value | Notes |
|-----------|---------------|-------|
| **Vendor ID** | `0xFFF1` | Test VID - change for production |
| **Product ID** | `0x8001` | Test PID |
| **Discriminator** | `3840` | Used for pairing |
| **Passcode** | `20202021` | **Change for production!** |

> ⚠️ **Security Warning:** The default passcode `20202021` is well-known and should not be used in production devices.

---

## Customization

### Changing Device Parameters

Edit these values in your code or via `idf.py menuconfig`:

- **Vendor ID / Product ID**: Configure in source code
- **Discriminator / Passcode**: Configure in source code  
- **Wi-Fi Credentials**: Can be pre-provisioned via `menuconfig` → "Component config" → "CHIP Device Layer" → "Wi-Fi Station"

### Temperature Sensor Configuration

The ESP32-C6 internal temperature sensor is configured in [main/app_main.cpp](main/app_main.cpp):
- **Update Interval**: 5 seconds (default)
- **Measurement Range**: -20°C to 80°C
- **Accuracy**: ±2°C typically

**Note:** The ESP32-C6 temperature sensor measures **die temperature** (chip temperature), which is typically 10-15°C higher than ambient temperature.

---

## Troubleshooting

### Build Issues

| Problem | Solution |
|---------|----------|
| "DefaultSessionKeystore not found" | Ensure ESP-IDF v5.4.1 (not v5.5+) |
| Build fails with CHIP SDK errors | Use official ESP-Matter example structure |
| Build hangs during compilation | First build takes 5-10 minutes - be patient |

### Runtime Issues

| Problem | Solution |
|---------|----------|
| Device not appearing in Home app | Check Wi-Fi credentials, verify HomePod mini on same network |
| Temperature reading seems high | ESP32-C6 sensor measures die temperature (~10-15°C above ambient) |
| Commissioning fails | Reset device with `idf.py erase-flash`, reflash firmware |

### Serial Monitor Commands

Press `Enter` in the monitor to access the CHIP shell (if enabled):

```
esp> matter config           # View current Matter configuration
esp> wifi connect SSID PASS  # Connect to Wi-Fi network
esp> matter factoryreset     # Factory reset device
```

---

## Project Structure

```
m5nanoc6-temperature/
├── CMakeLists.txt              # Top-level build configuration
├── sdkconfig.defaults          # Default ESP-IDF/Matter config
├── partitions.csv              # Flash partition table
├── SETUP.md                    # Build environment setup guide
├── README.md                   # This file
├── main/
│   ├── CMakeLists.txt          # Main component build config
│   ├── app_main.cpp            # Application entry point
│   └── CHIPProjectConfig.h     # CHIP SDK config (optional)
├── build/                      # Build output (generated)
│   ├── sensors.bin             # Main flashable binary
│   ├── bootloader/             # Bootloader
│   └── partition_table/        # Partition table
└── managed_components/         # ESP-Matter dependencies (generated)
```

---

## References

- 📖 [SETUP.md](SETUP.md) - Detailed environment setup and build instructions
- 🏠 [ESP-Matter Documentation](https://docs.espressif.com/projects/esp-matter/)
- 🛠️ [M5NanoC6 Docs](https://docs.m5stack.com/en/core/M5NanoC6)  
- 🔌 [Matter Specification](https://csa-iot.org/all-solutions/matter/)
- 💾 [ESP-Matter GitHub](https://github.com/espressif/esp-matter)
- 🔧 [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/)

---

## License

See [LICENSE](../LICENSE) in the repository root.

---

**Last Updated:** January 2025  
**Tested Configuration:** ESP-IDF v5.4.1 + ESP-Matter v1.5 + M5Stack NanoC6
- `master`

- If you're using an older ESP-IDF version, you can apply this [commit as a patch](https://github.com/espressif/esp-idf/commit/466328cd7e4c90c749a406d2bcee73f782ac0016) to add support manually.
