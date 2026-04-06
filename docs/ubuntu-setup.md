# Ubuntu 24.04 Setup Guide

This guide walks through preparing a **Ubuntu 24.04 LTS** for building and flashing both Matter
sensor devices in this project.

---

## Part 1 – Install WSL2 and Ubuntu 24.04

Use Ubuntu Server on a VM which allows USB passthrough.

### 1.1 Initial Ubuntu configuration

After restart, Ubuntu will open automatically and prompt you to:
1. Create a UNIX username (e.g. `dev`)
2. Set a password

### 1.2 Update to Ubuntu 24.04 LTS

If you already have Ubuntu installed at an older version, update the distribution:

```bash
# Inside WSL Ubuntu shell
sudo apt-get update && sudo apt-get dist-upgrade -y
lsb_release -a   # verify: Ubuntu 24.04
```

---

## Part 2 – Check USB ports

### 2.1 Attach a USB device

Every time you connect a USB device (M5NanoC6 or nRF52840 Dongle):

```bash
# Verify the device is visible
lsusb
ls /dev/ttyACM* /dev/ttyUSB*

# Grant your user permission to access the serial port (if needed)
sudo usermod -aG dialout $USER
# Log out and back in, or run: newgrp dialout
```

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
> **Note:** All dependencies should be installed globally. If you are in a Python virtualenv, deactivate it before running pip installs for tools like west or nrfutil.

---

## Part 4 – ESP-IDF Setup (for M5NanoC6)

Follow the detailed instructions in the M5NanoC6 README:
[`../m5nanoc6-temperature/README.md`](../m5nanoc6-temperature/README.md)


---

## Part 5 – nRF Connect SDK Setup (for nRF52840 Dongle)

Follow the detailed instructions in the nRF52840 Dongle README:
[`../nrf52840-dongle-temperature/README.md`](../nrf52840-dongle-temperature/README.md)

---

## Part 7 – USB Device Reference

| Device | USB VID:PID | Description |
|--------|------------|-------------|
| M5NanoC6 (normal mode) | `303a:1001` | ESP32-C6 USB JTAG/Serial |
| M5NanoC6 (download mode) | `303a:0002` | ESP32-C6 ROM DL |
| nRF52840 Dongle (DFU) | `1915:521f` | Open DFU Bootloader |
| nRF52840 Dongle (app) | `1915:cafe` | Application CDC ACM |


