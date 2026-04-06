# nRF52840 Dongle – Matter Temperature Sensor

This directory contains the firmware for the **nRF52840 Dongle** (PCA10059) acting
as a Matter-over-Thread temperature sensor. The device commissions into **Apple Home**
through a **HomePod mini** that acts as both the Matter controller and the
**Thread Border Router**.

This guide assumes a native **Ubuntu Server 24.04 LTS** environment, not WSL2.

---

## Hardware

| Item | Details |
|------|---------|
| Board | [Nordic nRF52840 Dongle (PCA10059)](https://www.nordicsemi.com/Products/Development-hardware/nRF52840-Dongle) |
| MCU | nRF52840 (ARM Cortex-M4F, 64 MHz) |
| Temperature sensor | nRF52840 on-chip TEMP peripheral |
| Connectivity | Bluetooth Low Energy, Thread, Zigbee, 802.15.4, ANT, and 2.4 GHz proprietary protocols |
| USB | USB-A plug, USB-powered, built-in USB bootloader, USB communication |
| Flash | 1 MB |
| RAM | 256 KB |
| GPIO | 15 GPIOs exposed on castellated edge pads |
| User I/O | User-programmable LEDs and button |
| Debug support | No onboard debug support; programming is done through USB bootloader |

> **Important:** The nRF52840 Dongle uses **Matter over Thread**. The HomePod mini
> or another Apple Thread Border Router includes a built-in
> **Thread Border Router** that connects the Thread mesh network to your IP network
> and Apple Home. No separate Border Router hardware is required.

Official Nordic references:
- Product page: https://www.nordicsemi.com/Products/Development-hardware/nRF52840-Dongle
- Hardware user guide: https://docs.nordicsemi.com/bundle/ug_nrf52840_dongle/page/UG/nrf52840_Dongle/intro.html

---

## Software Requirements

All steps below are performed on **Ubuntu Server 24.04 LTS**.

| Component | Version |
|-----------|---------|
| Ubuntu Server | 24.04 LTS |
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

> **Prerequisite:** Install the standard Ubuntu build tools and ensure your user
> can access serial devices such as `/dev/ttyACM0`.

### 1.1 Install system dependencies

```bash
sudo apt-get update && sudo apt-get install -y \
  git build-essential cmake ninja-build gperf ccache dfu-util wget \
    python3 python3-pip python3-dev python3-venv \
  xz-utils file \
    libsdl2-dev libmagic1 usbutils

# Allow current user to access USB serial devices after next login
sudo usermod -aG dialout "$USER"
```

On Ubuntu Server 24.04 `arm64`, do not install `gcc-multilib` or `g++-multilib`.
Those packages are for x86 multilib toolchains and are not needed for this Zephyr/nRF build flow.

### 1.2 Create a Python virtualenv and install west

Ubuntu 24.04 enforces **PEP 668** (externally managed environment), which prevents
global or `--user` `pip` installs without `--break-system-packages`. A virtualenv is
the clean solution and is also [recommended by the Zephyr project](https://docs.zephyrproject.org/latest/develop/beyond-GSG.html#python-pip)
when package versions may conflict with system packages.

```bash
# Create a dedicated virtualenv for all Zephyr/nRF tooling
python3 -m venv ~/.venv/zephyr

# Activate it (required for every new shell session)
source ~/.venv/zephyr/bin/activate

# Install west inside the venv (no --user flag needed inside a venv)
pip install -U west

west --version
```

Official west installation reference:
- https://docs.zephyrproject.org/latest/develop/west/install.html

### 1.3 Initialise nRF Connect SDK v2.6

```bash
mkdir -p ~/ncs
cd ~/ncs
west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.6.0
west update          # downloads ~4–6 GB; takes 15–30 minutes
west zephyr-export
```

### 1.4 Install Python dependencies

Ensure the venv is active (`source ~/.venv/zephyr/bin/activate`), then:

```bash
cd ~/ncs/zephyr
pip install -r scripts/requirements.txt

cd ~/ncs/nrf
pip install -r scripts/requirements-base.txt

cd ~/ncs/bootloader/mcuboot
pip install -r scripts/requirements.txt
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
# Install nrfutil inside the venv (venv must be active)
pip install nrfutil

# Verify
nrfutil version
```
---

## Step 2 – Recommended Shell Configuration

Add the following block to your `~/.bashrc` so that all tools are available in every new terminal session:

```bash
# Activate the Zephyr Python virtualenv (provides west, nrfutil, and all pip deps)
if [ -f ~/.venv/zephyr/bin/activate ]; then
    source ~/.venv/zephyr/bin/activate
fi

# nRF Connect SDK / Zephyr
if [ -d ~/ncs ]; then
    export ZEPHYR_BASE=~/ncs/zephyr
fi

# Zephyr SDK
if [ -d ~/ncs/zephyr-sdk-0.16.5 ]; then
    export ZEPHYR_SDK_INSTALL_DIR=~/ncs/zephyr-sdk-0.16.5
fi
```

---

## Step 3 – Build the Firmware

```bash
cd ~/repos/matter-poc1/nrf52840-dongle-temperature

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

The nRF52840 Dongle is flashed using its built-in USB bootloader and a DFU zip package.

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

## Step 4 – Flash the nRF52840 Dongle

The nRF52840 Dongle must be put into **DFU (bootloader) mode** before flashing.

### 4.1 Enter DFU mode

1. Insert the Dongle into a USB port.
2. Press the **RESET button** (the small button on the side of the PCB near the
   USB connector) **while the LED is off**, or press it once quickly to enter
   DFU mode.
3. The LED will flash **red** (LD2 on the silk screen) indicating DFU mode.

### 4.2 Identify the USB DFU serial port

```bash
ls /dev/ttyACM*
# Typically: /dev/ttyACM0
```

### 4.3 Flash using nrfutil

```bash
nrfutil dfu usb-serial \
    -pkg build/zephyr/dfu_application.zip \
    -p /dev/ttyACM0 \
    -b 115200
```

Wait for the progress bar to complete. The Dongle reboots automatically after
flashing and begins running the Matter firmware.

### 4.4 Alternative: nrfjprog (requires J-Link or nRF9161 DK as programmer)

If you have a J-Link programmer connected via SWD (not available on the Dongle
PCB itself without modification), you can use:

```bash
nrfjprog --program build/zephyr/zephyr.hex --chiperase --verify --reset
```

---

## Step 5 – Monitor the Device

The Dongle exposes a USB CDC ACM serial port for logging.

### 5.1 Connect to the console

```bash
# Install minicom or use screen/picocom
sudo apt-get install -y minicom

# Connect (115200 baud, 8N1)
minicom -D /dev/ttyACM0 -b 115200
```

Press **Ctrl+A X** to exit minicom.

### 5.2 Expected boot output

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

## Step 6 – Commission into Apple Home

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

## Step 7 – Verify Operation

### 7.1 Serial console

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

### 7.2 Apple Home

- Open **Home** app → Room → **nRF52840 Temp Sensor**.
- Temperature updates every 30 seconds (or on change if you adjust the code).

### 7.3 Matter shell commands

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
- Verify Linux can see the device: `lsusb | grep -i nordic`.
- If `/dev/ttyACM0` is missing after the application boots, check `dmesg | tail`.
- If serial access is denied, log out and back in after adding your user to `dialout`.

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
west build --pristine -b nrf52840dongle/nrf52840 -- -DCONF_FILE=prj.conf
# Flash the new build with nrfutil (see Step 4)
```

## Accuracy Notes

This README has been aligned with Nordic's official product page and hardware guide:

- The dongle is described as a low-cost USB development dongle with built-in USB bootloader support.
- It has no onboard debug support.
- It exposes 15 GPIOs and user LEDs/button.
- The environment assumptions now target native Ubuntu Server 24.04 rather than WSL2.
