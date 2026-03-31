# ESP32-C6 Matter Temperature Sensor – Setup Guide

This document provides the correct configuration for building the M5NanoC6 Matter Temperature Sensor firmware using **ESP-IDF v5.4.1** and **ESP-Matter SDK v1.5**.

## Project Structure

This project is **based on the official [ESP-Matter Sensors Example](https://github.com/espressif/esp-matter/tree/release/v1.5/examples/sensors)** and has been customized for the M5Stack NanoC6 board using the ESP32-C6's built-in temperature sensor.

## Compatibility Matrix

| Component | Version | Status |
|-----------|---------|--------|
| **ESP-IDF** | v5.4.1 | ✅ Recommended & Tested |
| **ESP-Matter SDK** | v1.5 (release branch) | ✅ Compatible & Tested |
| **Device** | M5NanoC6 (ESP32-C6) | ✅ Supported |
| **Connectivity** | Wi-Fi 6 (802.11ax) | ✅ Matter over Wi-Fi |
| **Build System** | Official ESP-Matter example structure | ✅ Verified working |

**⚠️ Critical Version Compatibility:**
- ESP-Matter v1.5 **requires** ESP-IDF v5.3 or v5.4
- ESP-Matter v1.5 is **incompatible** with ESP-IDF v5.5+ (requires ESP-Matter v1.6+)
- Always use the official ESP-Matter example structure as a base - custom project structures often fail due to CHIP SDK component dependencies

---

## Installation Steps

### 1.1 Install ESP-IDF v5.4.1

```bash
# Clone ESP-IDF v5.4.1 (not v5.5.3 as originally documented)
mkdir -p ~/esp
cd ~/esp
git clone --recursive --branch v5.4.1 \
    https://github.com/espressif/esp-idf.git

# Install toolchain for ESP32-C6
cd ~/esp/esp-idf
./install.sh esp32c6

# Source the environment
source ~/esp/esp-idf/export.sh

# Verify version
idf.py --version  # Should show: ESP-IDF v5.4.1
```

### 1.2 Install ESP-Matter SDK v1.5

```bash
# Clone ESP-Matter v1.5 (release branch)
sudo apt-get install -y libglib2.0-dev-bin
sudo apt install git gcc g++ pkg-config libssl-dev libavahi-client-dev libicu-dev
cd ~/esp
git clone --recursive --branch release/v1.5 \
    https://github.com/espressif/esp-matter.git

# Install Matter SDK dependencies
cd ~/esp/esp-matter
./install.sh

# Set ESP_MATTER_PATH environment variable (add to ~/.bashrc)
export ESP_MATTER_PATH=~/esp/esp-matter
```

### 1.3 Verify Installation

```bash
# In a new terminal, verify both are set correctly
echo "IDF_PATH: $IDF_PATH"
echo "ESP_MATTER_PATH: $ESP_MATTER_PATH"

# ESP-IDF should be v5.4.1
idf.py --version

# Matter SDK should be v1.5
cd $ESP_MATTER_PATH
git describe --all  # Should show: release/v1.5
```

---

## Configuration


Open `sdkconfig` with menuconfig and review the key settings:

```bash
idf.py menuconfig
```

Key menus to review:

| Menu path | Setting |
|-----------|---------|
| `Component config → CHIP Device Layer → Device Identification Options` | Vendor ID, Product ID, Discriminator, Passcode |
| `Component config → CHIP Device Layer → Wi-Fi station` | Pre-provisioned Wi-Fi credentials (optional) |
| `Component config → ESP Matter → Enable CHIP Shell` | Enable/disable the debug shell |

> **Important for production:** Change the Discriminator and Passcode from the
> defaults. The defaults (`3840` / `20202021`) are test values and are
> well-known.


### Controller Configuration

Device parameters (VID, PID, discriminator, passcode) are defined in:
- **Header file**: `device_config.h` (C preprocessor defines)
- **Can be overridden** via CMake variables at build time

**Important Parameters:**
```c
DEVICE_TYPE              = 0x0302      /* Temperature Sensor */
DEVICE_VENDOR_ID         = 0xFFF1      /* Test VID (production: use real VID) */
DEVICE_PRODUCT_ID        = 0x8001      /* Test PID */
DEVICE_HARDWARE_VERSION  = 1
DEVICE_SOFTWARE_VERSION  = 1
DEVICE_DISCRIMINATOR     = 3840        /* 0-4095 range */
DEVICE_SPAKE2_PASSCODE   = 20202021    /* Change for production! */
```

### Build Configuration (sdkconfig.defaults)

The `sdkconfig.defaults` file now contains only **ESP-IDF-specific** settings. Matter/CHIP component configuration is provided by the ESP-Matter SDK automatically.

**Key settings:**
```
CONFIG_IDF_TARGET="esp32c6"
CONFIG_ESP_WIFI_ENABLED=y
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_ENABLE_CHIP_SHELL=y
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

---

## Building the Firmware

### Build Procedure

```bash
# 1. Source the ESP-IDF environment
source ~/esp/esp-idf/export.sh

# 2. Navigate to project directory
cd ~/repos/matter-poc1/m5nanoc6-temperature

# 3. Set build target (first time only)
idf.py set-target esp32c6

# 4. Build the firmware
idf.py build

# Optional: Save build log with timestamp
idf.py build 2>&1 | tee build_$(date +%Y%m%d_%H%M%S).log
```

### Expected Build Output

Successful build produces these artifacts in `./build/`:

```
bootloader/bootloader.bin           - ESP32-C6 bootloader
partition_table/partition-table.bin - Flash partition table
sensors.elf                         - Main application ELF (linked executable)
sensors.bin                         - Main application binary (flashable)
ota_data_initial.bin                - OTA data partition
```

**Note:** The project name in `CMakeLists.txt` is `sensors`, so the binary is named `sensors.bin`.

### Build Verification

```bash
# Check build completed successfully
echo $?  # Should output: 0

# Verify key binaries exist
ls -lh build/bootloader/bootloader.bin \
       build/partition_table/partition-table.bin \
       build/sensors.bin \
       build/ota_data_initial.bin

# View complete flash command with offsets
idf.py -p /dev/ttyACM0 flash --dry-run
```

### Build Time

- **First build**: ~5-10 minutes (compiles all ESP-Matter SDK components)
- **Incremental builds**: ~30 seconds (only changed files)

---

## Troubleshooting

### Issue: Compiler not found
**Symptom**: "CMAKE_C_COMPILER: ... is not a full path and was not found in the PATH"

**Solution**: Source the IDF environment before building
```bash
source ~/esp/esp-idf/export.sh
idf.py build
```

### Issue: Unknown Kconfig symbols
**Symptom**: "warning: unknown kconfig symbol 'CHIP_DEVICE_TYPE'"

**Solution**: These symbols belong to the CHIP component, not ESP-IDF. They are defined in `device_config.h` as preprocessor macros instead.

### Issue: "DefaultSessionKeystore does not name a type"
**Symptom**: Compilation errors in `Server.h`

**Cause**: Version mismatch between ESP-IDF and ESP-Matter SDK

**Solution**: Ensure you're using:
- ✅ ESP-IDF v5.4.1 (NOT v5.5.3)
- ✅ ESP-Matter v1.5 (NOT v1.6+)

---

## Flashing the Device

```bash
# Using ESP ROM bootloader + USB-C
idf.py -p /dev/ttyACM0 flash

# Monitor serial output
idf.py -p /dev/ttyACM0 monitor

# Combined build, flash, and monitor
idf.py -p /dev/ttyACM0 build flash monitor
```

---

## Device Information

| Property | Value |
|----------|-------|
| **Board** | M5Stack NanoC6 |
| **MCU** | ESP32-C6 (RISC-V) |
| **Clock** | 160 MHz |
| **Flash** | 4 MB |
| **RAM** | 512 KB (SRAM) |
| **Wireless** | Wi-Fi 6 (802.11ax) + Bluetooth 5.3 |
| **Temp Sensor** | On-chip (±2°C accuracy) |
| **USB** | USB-C (Serial + JTAG) |

---

## Next Steps

After successful build:

1. **Flash the Device**
   ```bash
   idf.py -p /dev/ttyACM0 flash monitor
   ```

2. **Commission to Apple Home**
   - See [README.md](README.md#commissioning-to-apple-home) for detailed commissioning steps
   - QR code and manual pairing code appear in serial output on first boot

3. **Test with chip-tool** (optional)
   ```bash
   # Install chip-tool (if not already installed)
   cd $ESP_MATTER_PATH/connectedhomeip/connectedhomeip
   scripts/examples/gn_build_example.sh examples/chip-tool out/host
   
   # Commission device
   out/host/chip-tool pairing ble-wifi 1 SSID PASSWORD 20202021 3840
   
   # Read temperature
   out/host/chip-tool temperaturemeasurement read measured-value 1 1
   ```

---

## Common Build Issues

### Issue: sensors.bin not generated

**Symptom**: Only `bootloader.bin` and `partition-table.bin` exist after build

**Cause**: Build failed during main application compilation

**Solution**:
```bash
# Clean and rebuild
idf.py fullclean
idf.py build

# Check for error messages during build
# Common causes:
# - Missing ESP_MATTER_PATH environment variable
# - Wrong ESP-IDF version (must be v5.4.1)
# - Component dependencies not installed
```

---

## References

- [ESP-IDF v5.4.1 Documentation](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/)
- [ESP-Matter SDK Repository](https://github.com/espressif/esp-matter)
- [ESP-Matter Sensors Example](https://github.com/espressif/esp-matter/tree/release/v1.5/examples/sensors)
- [Matter Specification](https://csa-iot.org/csa_iot_working_groups/connectivity-standards/matter/)
- [M5NanoC6 Datasheet](https://docs.m5stack.com/en/core/M5NanoC6)

---

**Last Updated:** January 2025  
**Verified:** ESP-IDF v5.4.1 + ESP-Matter v1.5 + M5NanoC6
