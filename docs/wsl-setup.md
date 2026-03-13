# WSL2 Ubuntu 24.04 Setup Guide

This guide walks through preparing a **Windows Subsystem for Linux 2 (WSL2)**
environment running **Ubuntu 24.04 LTS** for building and flashing both Matter
sensor devices in this project.

---

## Prerequisites (Windows side)

| Requirement | Version |
|-------------|---------|
| Windows 10 | 22H2 or later (build 19045+) |
| Windows 11 | Any release |
| Windows PowerShell | 5.1+ or PowerShell 7+ |
| WSL2 | Kernel 5.15+ |

> **Admin rights** are required for WSL installation and usbipd-win.

---

## Part 1 – Install WSL2 and Ubuntu 24.04

### 1.1 Enable WSL2

Open **PowerShell as Administrator** and run:

```powershell
# Enable WSL and the Virtual Machine Platform feature
wsl --install

# This also installs Ubuntu by default. To install Ubuntu 24.04 specifically:
wsl --install -d Ubuntu-24.04
```

Restart Windows when prompted.

### 1.2 Initial Ubuntu configuration

After restart, Ubuntu will open automatically and prompt you to:
1. Create a UNIX username (e.g. `dev`)
2. Set a password

### 1.3 Update to Ubuntu 24.04 LTS

If you already have Ubuntu installed at an older version, update the distribution:

```bash
# Inside WSL Ubuntu shell
sudo apt-get update && sudo apt-get dist-upgrade -y
lsb_release -a   # verify: Ubuntu 24.04
```

### 1.4 Verify WSL2 kernel version

```powershell
# In Windows PowerShell
wsl --version
```

Output should show `WSL version: 2.x.x` and `Kernel version: 5.15.x` or later.

---

## Part 2 – Install usbipd-win (USB Device Sharing for WSL2)

WSL2 cannot access USB devices directly. The **usbipd-win** tool forwards USB
devices from Windows to the WSL2 Linux kernel.

### 2.1 Install usbipd-win

Download and install the latest release from:
<https://github.com/dorssel/usbipd-win/releases>

Or install via winget in an elevated PowerShell:

```powershell
winget install --id dorssel.usbipd-win -e
```

Restart the PowerShell session after installation.

### 2.2 Install the USB/IP Linux kernel modules in WSL2

```bash
# Inside Ubuntu WSL2 shell
sudo apt-get install -y linux-tools-generic hwdata
sudo update-alternatives --install /usr/local/bin/usbip usbip \
    /usr/lib/linux-tools/$(uname -r)/usbip 20
```

### 2.3 Attach a USB device to WSL2

Every time you connect a USB device (M5NanoC6 or nRF52840 Dongle):

```powershell
# 1. List USB devices (in Windows PowerShell, run as Administrator)
usbipd list

# Example output:
# BUSID  VID:PID    DEVICE                              STATE
# 1-1    10c4:ea60  Silicon Labs CP210x USB to UART…   Not shared
# 2-3    1915:521f  Open DFU Bootloader                Not shared

# 2. Bind the device (only needed once per device / Windows session)
usbipd bind --busid 1-1

# 3. Attach to WSL2 (needed each time you plug in the device)
usbipd attach --wsl --busid 1-1
```

Then in Ubuntu WSL2:

```bash
# Verify the device is visible
lsusb
ls /dev/ttyACM* /dev/ttyUSB*

# Grant your user permission to access the serial port (if needed)
sudo usermod -aG dialout $USER
# Log out and back in, or run: newgrp dialout
```

> **Tip:** Create a PowerShell script to automate the bind/attach steps for each
> device so you don't need to type the BUSID each time.

---

## Part 3 – General Build Dependencies

These packages are needed by both the M5NanoC6 (ESP-IDF) and nRF52840
(nRF Connect SDK) build systems.

```bash
sudo apt-get update && sudo apt-get install -y \
    git wget curl unzip \
    cmake ninja-build \
    python3 python3-pip python3-venv python3-dev \
    build-essential gcc g++ \
    flex bison gperf \
    ccache \
    libffi-dev libssl-dev \
    libusb-1.0-0 usbutils \
    dfu-util \
    minicom picocom \
    pkg-config \
    xz-utils file
```

---

## Part 4 – ESP-IDF Setup (for M5NanoC6)

Follow the detailed instructions in the M5NanoC6 README:
[`../m5nanoc6-temperature/README.md`](../m5nanoc6-temperature/README.md)

Summary:

```bash
# Clone ESP-IDF
mkdir -p ~/esp && cd ~/esp
git clone --recursive --branch v5.2 https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32c6

# Clone ESP Matter SDK
cd ~/esp
git clone --recursive --branch v1.3 https://github.com/espressif/esp-matter.git
cd esp-matter && ./install.sh

# Add to ~/.bashrc
cat >> ~/.bashrc << 'EOF'
source ~/esp/esp-idf/export.sh > /dev/null 2>&1
source ~/esp/esp-matter/export.sh > /dev/null 2>&1
export ESP_MATTER_PATH=~/esp/esp-matter
EOF
source ~/.bashrc
```

---

## Part 5 – nRF Connect SDK Setup (for nRF52840 Dongle)

Follow the detailed instructions in the nRF52840 Dongle README:
[`../nrf52840-dongle-temperature/README.md`](../nrf52840-dongle-temperature/README.md)

Summary:

```bash
# Install west
pip3 install --user west
echo 'export PATH=$HOME/.local/bin:$PATH' >> ~/.bashrc
source ~/.bashrc

# Initialise nRF Connect SDK
mkdir -p ~/ncs && cd ~/ncs
west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.6.0
west update
west zephyr-export

# Install Python requirements
pip3 install --user -r ~/ncs/zephyr/scripts/requirements.txt
pip3 install --user -r ~/ncs/nrf/scripts/requirements-base.txt
pip3 install --user -r ~/ncs/bootloader/mcuboot/scripts/requirements.txt

# Install Zephyr SDK (ARM toolchain)
cd ~/ncs
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.5/zephyr-sdk-0.16.5_linux-x86_64.tar.xz
tar xf zephyr-sdk-0.16.5_linux-x86_64.tar.xz
cd zephyr-sdk-0.16.5 && ./setup.sh

# Install nrfutil
pip3 install --user nrfutil
```

---

## Part 6 – Recommended Shell Configuration

Add the following block to your `~/.bashrc` so that all tools are available in
every new terminal session:

```bash
# -----------------------------------------------------------------------
# Matter PoC – Shell Environment
# -----------------------------------------------------------------------

# ESP-IDF (for M5NanoC6)
if [ -f ~/esp/esp-idf/export.sh ]; then
    source ~/esp/esp-idf/export.sh > /dev/null 2>&1
fi

# ESP Matter SDK (for M5NanoC6)
if [ -d ~/esp/esp-matter ]; then
    export ESP_MATTER_PATH=~/esp/esp-matter
    if [ -f ~/esp/esp-matter/export.sh ]; then
        source ~/esp/esp-matter/export.sh > /dev/null 2>&1
    fi
fi

# nRF Connect SDK / Zephyr (for nRF52840 Dongle)
if [ -d ~/ncs ]; then
    export ZEPHYR_BASE=~/ncs/zephyr
fi

# nrfutil and west (installed via pip --user)
export PATH="$HOME/.local/bin:$PATH"

# Zephyr SDK
if [ -d ~/ncs/zephyr-sdk-0.16.5 ]; then
    export ZEPHYR_SDK_INSTALL_DIR=~/ncs/zephyr-sdk-0.16.5
fi
```

Apply the changes:

```bash
source ~/.bashrc
```

---

## Part 7 – USB Device Reference

| Device | USB VID:PID | Description |
|--------|------------|-------------|
| M5NanoC6 (normal mode) | `303a:1001` | ESP32-C6 USB JTAG/Serial |
| M5NanoC6 (download mode) | `303a:0002` | ESP32-C6 ROM DL |
| nRF52840 Dongle (DFU) | `1915:521f` | Open DFU Bootloader |
| nRF52840 Dongle (app) | `1915:cafe` | Application CDC ACM |

---

## Part 8 – Troubleshooting WSL2

### Permission denied on /dev/ttyACMx

```bash
sudo usermod -aG dialout $USER
newgrp dialout
```

### Device not visible after usbipd attach

1. Check the device is bound: `usbipd list` should show `Shared` state.
2. Re-attach: `usbipd attach --wsl --busid <BUSID>`.
3. Check WSL kernel supports USB: `uname -r` should be ≥ 5.15.

### WSL2 runs out of memory during SDK builds

Create or edit `C:\Users\<YourUser>\.wslconfig`:

```ini
[wsl2]
memory=8GB
processors=4
swap=4GB
```

Restart WSL2:

```powershell
wsl --shutdown
wsl
```

### Slow git clone / west update

WSL2 filesystem performance is best when files are stored in the Linux
filesystem (`/home/<user>/`) rather than on the Windows filesystem
(`/mnt/c/`). Always clone repositories into `~/` directories inside WSL2.

---

## Quick Reference Card

```bash
# --- M5NanoC6 build and flash ---
source ~/esp/esp-idf/export.sh
source ~/esp/esp-matter/export.sh
cd ~/matter-poc1/m5nanoc6-temperature
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyACM0 flash monitor

# --- nRF52840 Dongle build ---
source ~/.bashrc
cd ~/matter-poc1/nrf52840-dongle-temperature
west build -b nrf52840dongle/nrf52840 -- -DCONF_FILE=prj.conf

# --- nRF52840 Dongle flash (DFU mode first!) ---
nrfutil dfu usb-serial -pkg build/zephyr/dfu_application.zip \
    -p /dev/ttyACM0 -b 115200

# --- USB device attach (Windows PowerShell, Admin) ---
usbipd list
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```
