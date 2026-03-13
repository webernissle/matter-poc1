# nRF52840 Dongle – Matter Temperature Sensor

This directory contains the firmware for the **nRF52840 Dongle** (PCA10059) acting
as a Matter-over-Thread temperature sensor. The device commissions into **Apple Home**
through a **HomePod mini** that acts as both the Matter controller and the
**Thread Border Router**.

---

## Hardware

| Item | Details |
|------|---------|
| Board | [Nordic nRF52840 Dongle (PCA10059)](https://www.nordicsemi.com/Products/Development-hardware/nRF52840-Dongle) |
| MCU | nRF52840 (ARM Cortex-M4F, 64 MHz) |
| Temperature sensor | nRF52840 on-chip TEMP peripheral (±0.25 °C typical) |
| Connectivity | IEEE 802.15.4 (Thread) + Bluetooth 5 |
| USB | USB-A plug (DFU bootloader for flashing) |
| Flash | 1 MB |

> **Important:** The nRF52840 Dongle uses **Matter over Thread**. The HomePod mini
> (2nd generation or HomePod mini running HomeOS 16.3+) includes a built-in
> **Thread Border Router** that connects the Thread mesh network to your IP network
> and Apple Home. No separate Border Router hardware is required.

---

## Software Requirements

All steps below are performed inside **Windows Subsystem for Linux (WSL2)** running
**Ubuntu 24.04 LTS**. See [`../docs/wsl-setup.md`](../docs/wsl-setup.md) for the
full WSL setup instructions.

| Component | Version |
|-----------|---------|
| Ubuntu (WSL2) | 24.04 LTS |
| nRF Connect SDK | v2.6.x |
| west | ≥ 1.2 |
| CMake | 3.20+ |
| Ninja | ≥ 1.10 |
| nRF Command Line Tools | ≥ 10.23 (for `nrfutil`) |
| nrfutil | v7+ (for DFU flashing) |

---

## Directory Structure

```
nrf52840-dongle-temperature/
├── CMakeLists.txt                          # Zephyr/west CMake project
├── prj.conf                                # Kconfig options (Matter, Thread, BLE…)
├── boards/
│   └── nrf52840dongle_nrf52840.overlay     # DTS overlay (TEMP sensor, USB CDC ACM)
├── src/
│   ├── CHIPProjectConfig.h                 # Project-level Matter config overrides
│   └── main.cpp                            # Application entry point
└── README.md                               # This file
```

---

## Step 1 – Set Up the Build Environment

> **Prerequisite:** Complete the WSL setup described in
> [`../docs/wsl-setup.md`](../docs/wsl-setup.md) before continuing.

### 1.1 Install system dependencies

```bash
sudo apt-get update && sudo apt-get install -y \
    git cmake ninja-build gperf ccache dfu-util wget \
    python3 python3-pip python3-dev python3-venv \
    xz-utils file make gcc-multilib g++-multilib \
    libsdl2-dev libmagic1 usbutils
```

### 1.2 Install west (Zephyr's meta-tool)

```bash
pip3 install --user west
# Add ~/.local/bin to PATH if not already present
echo 'export PATH=$HOME/.local/bin:$PATH' >> ~/.bashrc
source ~/.bashrc

west --version   # should print: West version: x.x.x
```

### 1.3 Initialise nRF Connect SDK v2.6

```bash
mkdir -p ~/ncs
cd ~/ncs
west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.6.0
west update          # downloads ~4–6 GB; takes 15–30 minutes
west zephyr-export
```

### 1.4 Install Python dependencies

```bash
cd ~/ncs/zephyr
pip3 install --user -r scripts/requirements.txt

cd ~/ncs/nrf
pip3 install --user -r scripts/requirements-base.txt

cd ~/ncs/bootloader/mcuboot
pip3 install --user -r scripts/requirements.txt
```

### 1.5 Install the Zephyr SDK (ARM toolchain)

```bash
# Download Zephyr SDK 0.16.5 (compatible with nRF Connect SDK v2.6)
cd ~/ncs
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.5/zephyr-sdk-0.16.5_linux-x86_64.tar.xz
tar xf zephyr-sdk-0.16.5_linux-x86_64.tar.xz
cd zephyr-sdk-0.16.5
./setup.sh

# Register the SDK cmake package
cmake -P share/zephyr-package/cmake/ZephyrConfig.cmake
```

### 1.6 Install nrfutil for DFU flashing

The nRF52840 Dongle uses DFU (Device Firmware Update) for programming – it does
not have a standard SWD debug header.

```bash
# Install nrfutil (Nordic's unified tool)
pip3 install --user nrfutil

# Verify
nrfutil version
```

---

## Step 2 – Build the Firmware

```bash
cd ~/matter-poc1/nrf52840-dongle-temperature

# Build for the nRF52840 Dongle
west build -b nrf52840dongle/nrf52840 \
    -- -DCONF_FILE=prj.conf

# The build artefacts are in build/
ls build/zephyr/
# app.hex  zephyr.hex  zephyr.bin  dfu_application.zip ...
```

A successful build ends with:

```
[xxx/xxx] Linking C executable zephyr/zephyr.elf
Memory region         Used Size  Region Size  %age Used
           FLASH:      xxx KB       1024 KB    xx.x%
            SRAM:       xxx KB        256 KB    xx.x%
...
Build complete!
```

### Generating the DFU package

The nRF52840 Dongle is flashed using a DFU zip package:

```bash
# Package is generated automatically by west build.
# If you need to re-generate it manually:
nrfutil pkg generate \
    --hw-version 52 \
    --sd-req=0x00 \
    --application build/zephyr/app_signed.hex \
    --application-version 1 \
    dfu_application.zip
```

---

## Step 3 – Flash the nRF52840 Dongle

The nRF52840 Dongle must be put into **DFU (bootloader) mode** before flashing.

### 3.1 Enter DFU mode

1. Insert the Dongle into a USB port.
2. Press the **RESET button** (the small button on the side of the PCB near the
   USB connector) **while the LED is off**, or press it once quickly to enter
   DFU mode.
3. The LED will flash **red** (LD2 on the silk screen) indicating DFU mode.

> In WSL2, first attach the USB device using usbipd-win
> (see [`../docs/wsl-setup.md`](../docs/wsl-setup.md)).

### 3.2 Identify the USB DFU serial port

```bash
ls /dev/ttyACM*
# Typically: /dev/ttyACM0
```

### 3.3 Flash using nrfutil

```bash
nrfutil dfu usb-serial \
    -pkg build/zephyr/dfu_application.zip \
    -p /dev/ttyACM0 \
    -b 115200
```

Wait for the progress bar to complete. The Dongle reboots automatically after
flashing and begins running the Matter firmware.

### 3.4 Alternative: nrfjprog (requires J-Link or nRF9161 DK as programmer)

If you have a J-Link programmer connected via SWD (not available on the Dongle
PCB itself without modification), you can use:

```bash
nrfjprog --program build/zephyr/zephyr.hex --chiperase --verify --reset
```

---

## Step 4 – Monitor the Device

The Dongle exposes a USB CDC ACM serial port for logging.

### 4.1 Connect to the console

```bash
# Install minicom or use screen/picocom
sudo apt-get install -y minicom

# Connect (115200 baud, 8N1)
minicom -D /dev/ttyACM0 -b 115200
```

Press **Ctrl+A X** to exit minicom.

### 4.2 Expected boot output

```
*** Booting nRF Connect SDK v2.6.0 ***
[00:00:00.500,000] <inf> nrf52840_temp_sensor: nRF52840 Dongle Matter Temperature Sensor starting...
[00:00:00.510,000] <inf> chip: [DL] CHIP task running
[00:00:00.520,000] <inf> chip: [SVR] Server Listening...
[00:00:00.530,000] <inf> chip: [DIS] CHIP minimal mDNS started advertising.
[00:00:00.540,000] <inf> chip: [SVR] SetupQRCode: [MT:XXXXXXXXXXXX]
[00:00:00.550,000] <inf> chip: [SVR] Manual pairing code: [XXXXX-XXXXX]
[00:00:00.560,000] <inf> nrf52840_temp_sensor: Matter stack started – waiting for commissioning via BLE
```

Note the **QR code** and **manual pairing code** from the log.

---

## Step 5 – Commission into Apple Home

See the full commissioning guide at
[`../docs/apple-home-commissioning.md`](../docs/apple-home-commissioning.md).

**Quick summary:**
1. Ensure your HomePod mini (running HomeOS 16.3 or later) is on the same Wi-Fi
   network.
2. Ensure Bluetooth is enabled on your iPhone/iPad.
3. Open the **Home** app → **+** → **Add Accessory**.
4. Scan the QR code from the serial console, or enter the manual pairing code.
5. The HomePod mini will:
   a. Communicate with the Dongle over BLE (commissioning channel).
   b. Provide Thread network credentials to the Dongle.
   c. The Dongle joins the Thread mesh network.
6. After commissioning, an **nRF52840 Temp Sensor** tile appears in Apple Home.

---

## Step 6 – Verify Operation

### 6.1 Serial console

After commissioning and Thread attachment you should see:

```
[00:00:xx.xxx] <inf> nrf52840_temp_sensor: Thread network attached
[00:00:xx.xxx] <inf> nrf52840_temp_sensor: Commissioning complete – device is now in the fabric
[00:00:xx.xxx] <inf> nrf52840_temp_sensor: Temperature: 32.50 °C  (Matter raw: 3250)
```

> The nRF52840 die temperature is higher than ambient because the chip
> generates heat. This is expected for the on-chip TEMP sensor.
> For an accurate ambient reading, connect an external I2C sensor (e.g. SHT40)
> and update the DTS overlay and application accordingly.

### 6.2 Apple Home

- Open **Home** app → Room → **nRF52840 Temp Sensor**.
- Temperature updates every 30 seconds (or on change if you adjust the code).

### 6.3 Matter shell commands

With `CONFIG_CHIP_LIB_SHELL=y` you can send commands via the serial console:

```
uart:~$ matter help
uart:~$ matter device factoryreset
uart:~$ matter onboardingcodes ble
```

---

## Troubleshooting

### DFU flashing fails

- Ensure the Dongle is in **DFU mode** (LED flashing red).
- Try a different USB port or USB hub.
- In WSL2, verify the USB device is attached: `lsusb | grep Nordic`.
- If using WSL2, ensure usbipd-win has attached the DFU device:
  ```powershell
  # In Windows PowerShell (admin)
  usbipd list
  usbipd attach --wsl --busid <BUSID>
  ```

### Thread network not joining

- Verify the HomePod mini is running HomeOS 16.3 or later.
- Check that the HomePod mini is the only Thread Border Router on the network
  (or that all Border Routers use compatible Thread credentials).
- Retry commissioning after a factory reset.

### Commissioning fails / BLE not visible

- Move the phone and the Dongle closer together (BLE range is ~10 m).
- Ensure no other Matter controller is trying to commission the device
  simultaneously.
- If the pairing code is rejected, factory reset the device:
  ```
  uart:~$ matter device factoryreset
  ```
  or rebuild and reflash to clear NVS.

### Factory Reset

```bash
# Via shell
uart:~$ matter device factoryreset

# Or rebuild and reflash (clears all NVS)
west build --pristine
# Flash the new build with nrfutil (see Step 3)
```
