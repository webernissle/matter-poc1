# M5NanoC6 – Matter Temperature Sensor

This directory contains the firmware for the **M5NanoC6** (ESP32-C6) acting as a
Matter-over-Wi-Fi temperature sensor. The device commissions into **Apple Home**
through a **HomePod mini** acting as the Matter hub / border router.

---

## Hardware

| Item | Details |
|------|---------|
| Board | [M5Stack NanoC6](https://docs.m5stack.com/en/core/M5NanoC6) |
| MCU | ESP32-C6 (RISC-V, 160 MHz, Wi-Fi 6 + Bluetooth 5) |
| Temperature sensor | ESP32-C6 on-chip temperature sensor (±2 °C accuracy) |
| Connectivity | Wi-Fi 802.11ax (Wi-Fi 6) |
| USB | USB-C (USB Serial/JTAG for flashing and console) |
| Flash | 4 MB |

> **Note:** The M5NanoC6 uses **Matter over Wi-Fi** (not Thread). The HomePod mini
> bridges the device into Apple Home via standard IP networking.

---

## Software Requirements

All steps below are performed inside **Windows Subsystem for Linux (WSL2)** running
**Ubuntu 24.04 LTS**. See [`../docs/wsl-setup.md`](../docs/wsl-setup.md) for the
full WSL setup instructions.

| Component | Version |
|-----------|---------|
| Ubuntu (WSL2) | 24.04 LTS |
| ESP-IDF | v5.2.x |
| ESP Matter SDK | v1.3 |
| Python | 3.10+ (bundled with ESP-IDF) |
| CMake | 3.16+ |

---

## Directory Structure

```
m5nanoc6-temperature/
├── CMakeLists.txt          # Top-level CMake project file
├── partitions.csv          # Custom flash partition table (4 MB)
├── sdkconfig.defaults      # Default Kconfig options for this project
├── main/
│   ├── CMakeLists.txt      # Component CMakeLists
│   └── app_main.cpp        # Application entry point
└── README.md               # This file
```

---

## Step 1 – Set Up the Build Environment

> **Prerequisite:** Complete the WSL setup described in
> [`../docs/wsl-setup.md`](../docs/wsl-setup.md) before continuing.

### 1.1 Install ESP-IDF v5.2

```bash
# Install system dependencies
sudo apt-get update && sudo apt-get install -y \
    git wget flex bison gperf python3 python3-pip python3-venv \
    cmake ninja-build ccache libffi-dev libssl-dev dfu-util \
    libusb-1.0-0 usbutils

# Clone ESP-IDF (v5.2 LTS branch)
mkdir -p ~/esp
cd ~/esp
git clone --recursive --branch v5.2 \
    https://github.com/espressif/esp-idf.git

# Install the ESP32-C6 toolchain
cd ~/esp/esp-idf
./install.sh esp32c6

# Set up the environment (add to ~/.bashrc for persistence)
echo 'source ~/esp/esp-idf/export.sh > /dev/null 2>&1' >> ~/.bashrc
source ~/esp/esp-idf/export.sh
```

### 1.2 Install ESP Matter SDK v1.3

```bash
# Clone the ESP Matter SDK
cd ~/esp
git clone --recursive --branch v1.3 \
    https://github.com/espressif/esp-matter.git

# Install Matter SDK dependencies
cd ~/esp/esp-matter
./install.sh

# Set ESP_MATTER_PATH (add to ~/.bashrc for persistence)
echo 'export ESP_MATTER_PATH=~/esp/esp-matter' >> ~/.bashrc
export ESP_MATTER_PATH=~/esp/esp-matter

# Source the Matter environment (do this in every new shell session)
echo 'source $ESP_MATTER_PATH/export.sh > /dev/null 2>&1' >> ~/.bashrc
source $ESP_MATTER_PATH/export.sh
```

> **Tip:** The ESP Matter SDK install downloads a large number of submodules
> (connectedhomeip, etc.). This can take 10–30 minutes depending on your
> internet connection.

---

## Step 2 – Configure the Project

### 2.1 Navigate to the project directory

```bash
# Assuming you cloned this repository to ~/matter-poc1
cd ~/matter-poc1/m5nanoc6-temperature
```

### 2.2 Set the target chip

```bash
idf.py set-target esp32c6
```

This creates a `build/` directory and applies `sdkconfig.defaults`.

### 2.3 (Optional) Customise Wi-Fi credentials and commissioning parameters

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

---

## Step 3 – Build the Firmware

```bash
cd ~/matter-poc1/m5nanoc6-temperature

# Source environments if not already done
source ~/esp/esp-idf/export.sh
source ~/esp/esp-matter/export.sh

# Build
idf.py build
```

A successful build ends with output similar to:

```
[100%] Built target m5nanoc6_temperature_sensor.elf
Generating binary image from built executable
esptool.py v4.x.x
...
Generated /home/<user>/matter-poc1/m5nanoc6-temperature/build/m5nanoc6_temperature_sensor.bin
Project build complete. To flash, run this command:
 idf.py -p (PORT) flash
```

---

## Step 4 – Flash the M5NanoC6

### 4.1 Identify the USB serial port

Connect the M5NanoC6 to your PC via USB-C. In WSL2 you must first attach the
USB device using **usbipd-win** (see [`../docs/wsl-setup.md`](../docs/wsl-setup.md)).

```bash
# After USB is attached to WSL, list serial ports:
ls /dev/ttyACM* /dev/ttyUSB*
# Typically: /dev/ttyACM0 or /dev/ttyUSB0
```

### 4.2 Flash and monitor

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

This command:
1. Erases the relevant flash sectors
2. Writes the bootloader, partition table, and application
3. Resets the device
4. Opens a serial console so you can see the device logs

Press **Ctrl+]** to exit the monitor.

### 4.3 Expected boot output

```
I (xxx) chip[DL]: CHIP task running
I (xxx) m5nanoc6_temp_sensor: Internal temperature sensor enabled
I (xxx) chip[SVR]: Server Listening...
I (xxx) chip[DIS]: Updating services using commissioning mode 1
I (xxx) chip[DIS]: CHIP minimal mDNS started advertising.
I (xxx) CHIP:DL: CHIP task running
I (xxx) chip[SVR]: SetupQRCode: [MT:XXXXXXXXXXXX]
I (xxx) chip[SVR]: Manual pairing code: [XXXXX-XXXXX]
I (xxx) m5nanoc6_temp_sensor: Matter stack started – waiting for commissioning
```

The **QR Code** URL and **manual pairing code** will appear in the log.

---

## Step 5 – Commission into Apple Home

See the full commissioning guide at
[`../docs/apple-home-commissioning.md`](../docs/apple-home-commissioning.md).

**Quick summary:**
1. Open the **Home** app on iPhone/iPad connected to the same Wi-Fi as the
   HomePod mini.
2. Tap **+** → **Add Accessory**.
3. Scan the QR code shown in the serial monitor, or enter the manual pairing
   code.
4. Follow the on-screen steps – the HomePod mini acts as the Matter hub.
5. After commissioning, a **Temperature Sensor** tile appears in Apple Home and
   updates every ~30 seconds.

---

## Step 6 – Verify Operation

### 6.1 Serial monitor

Re-open the serial monitor at any time:

```bash
idf.py -p /dev/ttyACM0 monitor
```

You should see log lines like:

```
I (xxxxx) m5nanoc6_temp_sensor: Temperature: 28.45 °C  (Matter raw: 2845)
```

### 6.2 Apple Home

- Open the **Home** app.
- Navigate to your home → room containing the sensor.
- The **M5NanoC6 Temp Sensor** tile should show the current temperature.

### 6.3 Matter chip-tool (optional, for protocol-level testing)

Install `chip-tool` on a Linux machine in the same network:

```bash
# Build chip-tool from the connectedhomeip source (included in ESP Matter SDK)
cd ~/esp/esp-matter/connectedhomeip/connectedhomeip
./scripts/examples/gn_build_example.sh examples/chip-tool out/chip-tool

# Read MeasuredValue attribute (endpoint 1, cluster 0x0402, attribute 0x0000)
./out/chip-tool/chip-tool temperaturemeasurement read measured-value 1 1
```

---

## Troubleshooting

### Device not detected in WSL

- Ensure `usbipd-win` is installed on Windows and the device is attached.
- Run `usbipd list` in PowerShell and `usbipd attach --wsl --busid <ID>`.

### Commissioning fails

- Ensure the M5NanoC6 and the phone used to commission are on the same Wi-Fi
  network as the HomePod mini.
- Check that BLE is enabled on the phone (used for initial commissioning).
- Try erasing NVS and re-commissioning:
  ```bash
  idf.py -p /dev/ttyACM0 erase-flash
  idf.py -p /dev/ttyACM0 flash monitor
  ```

### Temperature reads 0 or invalid

- The internal sensor needs ~1 second to stabilise after boot.
- The first valid reading appears after the 30-second task delay.
- Check the serial monitor for `Temperature read failed` warnings.

### Wi-Fi provisioning

The ESP Matter SDK supports **Wi-Fi provisioning over BLE** (Bluetooth Low Energy)
during the commissioning flow. When you scan the QR code in Apple Home, the
HomePod mini sends the Wi-Fi credentials to the device over BLE automatically.
You do **not** need to hardcode Wi-Fi credentials in the firmware.

---

## Factory Reset

To remove all stored credentials and return the device to an unprovisioned state:

1. Hold the button on the M5NanoC6 for **10 seconds** (if button is wired to
   GPIO0), **or**
2. Use the serial shell:
   ```
   matter device factoryreset
   ```
   **or**
3. Erase the NVS partition:
   ```bash
   idf.py -p /dev/ttyACM0 erase-flash
   idf.py -p /dev/ttyACM0 flash
   ```
