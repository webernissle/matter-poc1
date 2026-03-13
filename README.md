# matter-poc1 – Matter Sensor End Devices PoC

A proof-of-concept implementation of two **Matter temperature sensor end devices**
that report data through an **Apple HomePod mini** and are visible in **Apple Home**.

---

## Devices

| Device | MCU | Transport | Directory |
|--------|-----|-----------|-----------|
| M5NanoC6 | ESP32-C6 | Matter over Wi-Fi | [`m5nanoc6-temperature/`](m5nanoc6-temperature/) |
| nRF52840 Dongle (PCA10059) | nRF52840 | Matter over Thread | [`nrf52840-dongle-temperature/`](nrf52840-dongle-temperature/) |

Both devices implement the **Matter Temperature Sensor** device type (`0x0302`)
using the **Temperature Measurement** cluster (`0x0402`).

---

## Architecture

```
Apple Home (iPhone/iPad/Mac)
        │
        │ HomeKit / iCloud
        ▼
HomePod mini  ←──── Thread Border Router (for nRF52840)
        │                    │
        │ Wi-Fi (IP)         │ Thread (802.15.4)
        ▼                    ▼
  M5NanoC6            nRF52840 Dongle
  (ESP32-C6)          (nRF52840)
  Temp Sensor         Temp Sensor
```

---

## Documentation

| Document | Description |
|----------|-------------|
| [`docs/wsl-setup.md`](docs/wsl-setup.md) | WSL2 Ubuntu 24.04 environment setup |
| [`docs/apple-home-commissioning.md`](docs/apple-home-commissioning.md) | Commissioning both devices into Apple Home |
| [`m5nanoc6-temperature/README.md`](m5nanoc6-temperature/README.md) | M5NanoC6 build, flash & test guide |
| [`nrf52840-dongle-temperature/README.md`](nrf52840-dongle-temperature/README.md) | nRF52840 Dongle build, flash & test guide |

---

## Quick Start

### 1. Set up WSL2 Ubuntu 24.04

See **[docs/wsl-setup.md](docs/wsl-setup.md)** for the complete guide.

### 2. Build and flash the M5NanoC6

```bash
# Source ESP-IDF + ESP Matter SDK
source ~/esp/esp-idf/export.sh
source ~/esp/esp-matter/export.sh

cd m5nanoc6-temperature
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

See **[m5nanoc6-temperature/README.md](m5nanoc6-temperature/README.md)** for full details.

### 3. Build and flash the nRF52840 Dongle

```bash
cd nrf52840-dongle-temperature
west build -b nrf52840dongle/nrf52840 -- -DCONF_FILE=prj.conf

# Enter DFU mode on the Dongle (press RESET button), then:
nrfutil dfu usb-serial -pkg build/zephyr/dfu_application.zip \
    -p /dev/ttyACM0 -b 115200
```

See **[nrf52840-dongle-temperature/README.md](nrf52840-dongle-temperature/README.md)** for full details.

### 4. Commission into Apple Home

See **[docs/apple-home-commissioning.md](docs/apple-home-commissioning.md)** for step-by-step commissioning instructions.

---

## Build Environment

| Tool | Used for |
|------|----------|
| WSL2 Ubuntu 24.04 | All build and flash operations |
| ESP-IDF v5.2 | M5NanoC6 (ESP32-C6) toolchain |
| ESP Matter SDK v1.3 | M5NanoC6 Matter stack |
| nRF Connect SDK v2.6 | nRF52840 Dongle toolchain + Matter stack |
| west | nRF Connect SDK build tool |
| nrfutil | nRF52840 Dongle DFU flashing |
| usbipd-win | USB device forwarding from Windows to WSL2 |

> **CLI only:** No IDE is required. All build, flash, and test operations are
> performed from the command line inside WSL2.

---

## License

See [LICENSE](LICENSE).
