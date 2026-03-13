# Apple Home Commissioning Guide

This guide explains how to commission both Matter sensor devices into **Apple Home**
using a **HomePod mini** as the Matter hub and Thread Border Router.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│                  Apple Home                     │
│         (iPhone / iPad / Mac app)               │
└────────────────────┬────────────────────────────┘
                     │ iCloud / HomeKit protocol
┌────────────────────▼────────────────────────────┐
│              HomePod mini                       │
│  • Matter controller (hub)                      │
│  • Thread Border Router (IEEE 802.15.4 ↔ IP)   │
│  • Wi-Fi 802.11 a/b/g/n/ac                      │
└──────┬──────────────────────┬───────────────────┘
       │ Wi-Fi (Matter/IP)    │ Thread (OpenThread)
┌──────▼──────┐        ┌──────▼──────────────┐
│  M5NanoC6   │        │ nRF52840 Dongle      │
│  ESP32-C6   │        │ nRF52840             │
│  Temp Sensor│        │ Temp Sensor          │
│ (Wi-Fi 6)   │        │ (Thread MTD)         │
└─────────────┘        └──────────────────────┘
```

**M5NanoC6** connects via **Wi-Fi** (Matter over Wi-Fi / IP).
**nRF52840 Dongle** connects via **Thread** (Matter over Thread). The HomePod
mini bridges Thread to IP and controls both devices.

---

## Prerequisites

| Item | Requirement |
|------|------------|
| HomePod mini | Any HomePod mini running **HomeOS 16.3 or later** |
| iPhone or iPad | Running **iOS 16.1 or later** |
| Apple ID | Signed in to **iCloud** with Two-Factor Authentication enabled |
| Wi-Fi network | 2.4 GHz or 5 GHz, WPA2/WPA3 (both HomePod and M5NanoC6 must be on same network) |
| Bluetooth | Enabled on commissioning iPhone/iPad |

> **Thread Border Router note:** HomePod mini 2nd generation and HomePod mini
> running HomeOS 16.3+ include an integrated Thread Border Router. This means
> the nRF52840 Dongle can join a Thread network automatically during
> commissioning – no separate Thread Border Router hardware is needed.

---

## Step 1 – Set Up the HomePod mini

If the HomePod mini is not yet set up:

1. Plug the HomePod mini into power.
2. Hold your iPhone near the HomePod mini.
3. Follow the on-screen setup in the **Home** app.
4. Sign in with your Apple ID.
5. Ensure the HomePod mini is connected to Wi-Fi and updated to the latest
   HomeOS version (**Home** app → HomePod → Settings → Software Update).

---

## Step 2 – Prepare the Sensor Devices

Before commissioning each device, ensure:

- Firmware has been flashed successfully.
- The device has **not** previously been commissioned (or has been factory reset).
- The serial console shows the **QR code URL** and **manual pairing code**.

### Retrieve the QR code and pairing code

**M5NanoC6** (Wi-Fi):

```bash
# In WSL2 terminal
idf.py -p /dev/ttyACM0 monitor
```

Look for lines like:

```
I (xxx) chip[SVR]: SetupQRCode: [MT:Y3D00KA0648G00000]
I (xxx) chip[SVR]: Manual pairing code: [34970112332]
```

**nRF52840 Dongle** (Thread):

```bash
# In WSL2 terminal
minicom -D /dev/ttyACM0 -b 115200
```

Look for:

```
[inf] chip: [SVR] SetupQRCode: [MT:Y3D00KA0648G00001]
[inf] chip: [SVR] Manual pairing code: [34970112333]
```

> **Tip:** Take a photo or screenshot of the pairing code in case BLE discovery
> is slow. You can also generate a QR code image from the `MT:…` string using
> any QR code generator, or the `chip-tool` utility.

---

## Step 3 – Commission the M5NanoC6 into Apple Home

1. On your iPhone, open the **Home** app.
2. Tap **+** (top right) → **Add Accessory**.
3. Tap **More options** if the device isn't automatically discovered.
4. Tap **I Don't Have a Code or Cannot Scan**.
   - **Or:** Use the phone camera to scan a QR code if you generated one.
5. If using the manual pairing code: tap **Enter Code Manually** and type the
   11-digit code (e.g. `34970112332`).
6. Apple Home shows a list of nearby Matter devices. Select the
   **M5NanoC6 Temp Sensor**.
7. Follow the prompts:
   - **Room assignment:** Choose or create a room (e.g. *Living Room*).
   - **Accessory name:** Rename if desired (e.g. *NanoC6 Temperature*).
   - **Wi-Fi provisioning:** Apple Home sends Wi-Fi credentials to the M5NanoC6
     over BLE automatically. You only need to confirm the network.
8. Wait for the green **✓ Added to Home** confirmation.

The M5NanoC6 now appears in Apple Home as a **Temperature Sensor** tile.

---

## Step 4 – Commission the nRF52840 Dongle into Apple Home

1. On your iPhone, open the **Home** app.
2. Tap **+** → **Add Accessory**.
3. Scan the QR code (or enter the manual pairing code) for the nRF52840 Dongle.
4. Apple Home shows **nRF52840 Temp Sensor**.
5. Follow the prompts:
   - **Room assignment:** Choose a room (e.g. *Office*).
   - **Accessory name:** e.g. *Dongle Temperature*.
   - **Thread provisioning:** The HomePod mini automatically sends Thread
     network credentials to the Dongle during commissioning. This happens
     transparently.
6. The Dongle joins the Thread network (you may see `Thread network attached` in
   the serial console).
7. Wait for the green **✓ Added to Home** confirmation.

---

## Step 5 – Verify in Apple Home

### View temperature readings

1. Open the **Home** app.
2. Navigate to the room containing each sensor.
3. The temperature is shown on the tile (e.g. **28°C**).
4. Tap the tile to see historical data (Apple Home stores history for ~10 days).

### Automations (optional)

You can create automations based on temperature thresholds:

1. **Home** app → **Automation** tab → **+** → **A Sensor Detects Something**.
2. Select the temperature sensor.
3. Set a condition (e.g. *Temperature rises above 30°C*).
4. Add an action (e.g. turn on a fan accessory).

---

## Commissioning via chip-tool (CLI – for Testing)

For protocol-level testing without Apple Home, use `chip-tool` from the
connectedhomeip repository.

```bash
# Build chip-tool (part of the ESP Matter SDK)
cd ~/esp/esp-matter/connectedhomeip/connectedhomeip
./scripts/examples/gn_build_example.sh examples/chip-tool out/chip-tool

# Commission the M5NanoC6 over the network (BLE-WiFi flow)
./out/chip-tool/chip-tool pairing ble-wifi 1 "<SSID>" "<PASSWORD>" 20202021 3840

# Commission the nRF52840 Dongle via BLE-Thread
./out/chip-tool/chip-tool pairing ble-thread 2 hex:<THREAD_DATASET_HEX> 20202021 3841

# Read temperature from M5NanoC6 (node-id 1, endpoint 1)
./out/chip-tool/chip-tool temperaturemeasurement read measured-value 1 1

# Read temperature from nRF52840 Dongle (node-id 2, endpoint 1)
./out/chip-tool/chip-tool temperaturemeasurement read measured-value 2 1
```

> **Note:** `chip-tool` uses its own fabric separate from Apple Home. For
> production use, commission via Apple Home and use chip-tool only for
> supplemental testing.

### Getting the Thread dataset (nRF52840 only)

To use chip-tool with the nRF52840 Dongle you need the Thread network dataset.
With the HomePod mini as Border Router, retrieve the dataset via the iOS
shortcut or from another Thread Border Router admin interface.

Alternatively, during development, you can hard-code a known dataset in `prj.conf`
and the overlay file for a controlled test environment.

---

## Troubleshooting Commissioning

### Device not found during Add Accessory

- Ensure Bluetooth is enabled on the commissioning iPhone.
- Move the iPhone within 1–2 metres of the device.
- Verify the device is in commissioning mode (check serial console for
  `Matter stack started – waiting for commissioning`).
- Try turning Bluetooth off and on again on the iPhone.

### "Accessory Not Supported" error

- The test Vendor ID (`0xFFF1`) and Product ID are recognised as test devices.
  This should work with Apple Home in test/development mode. For production
  devices, register with the CSA and use a production VID/PID with a valid
  Device Attestation Certificate (DAC).

### Commissioning times out

- Apple Home has a ~4-minute commissioning timeout.
- For the M5NanoC6 (Wi-Fi), ensure the iPhone is on the same Wi-Fi network as
  the HomePod mini.
- For the nRF52840 (Thread), ensure the HomePod mini is the Thread Border Router
  (check in **Home** app → home settings → Thread network).
- Restart the device and retry.

### Device disappears from Apple Home after commissioning

- This can happen if the device loses power or NVS is erased.
- Remove the accessory from Apple Home and recommission.
- For the nRF52840 Dongle, ensure the Thread network is stable (HomePod mini
  should be powered on and connected to Wi-Fi).

### "No response" in Apple Home

- Check the serial console for errors.
- For the M5NanoC6, verify the device is connected to Wi-Fi:
  ```
  matter wifi status
  ```
- For the nRF52840, verify Thread attachment:
  ```
  uart:~$ ot state
  ```
  Expected output: `child` or `router`.

---

## Fabric Sharing (Multiple Apple IDs / Homes)

Matter supports **multi-fabric** – a device can be simultaneously commissioned
into multiple fabrics (e.g. Apple Home + Google Home).

To share the M5NanoC6 or nRF52840 sensor with another Apple Home:
1. The current Apple Home admin shares the accessory.
2. **Home** app → Accessory → Settings → **Allow Guest Access**.

> This project uses the default single-fabric commissioning window. Multi-fabric
> / Enhanced Commissioning Window features can be added in a future iteration.

---

## QR Code Generation (Convenience)

If the serial console QR code URL is too small to scan, generate an image:

```bash
# Install qrencode in WSL2
sudo apt-get install -y qrencode

# Generate QR code PNG from the SetupQRCode string
# Replace MT:... with the actual code from your serial console
qrencode -t PNG -o m5nanoc6-qr.png "MT:Y3D00KA0648G00000"
qrencode -t PNG -o nrf52840-qr.png "MT:Y3D00KA0648G00001"

# Copy to Windows for viewing
cp m5nanoc6-qr.png /mnt/c/Users/$WINDOWS_USER/Desktop/
```

Or open the URL `https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT:...`
in a browser to view the QR code.
