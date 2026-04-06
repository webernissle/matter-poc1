#!/usr/bin/env bash
set -euo pipefail

source ~/esp/esp-idf/export.sh
export ESP_MATTER_PATH="${ESP_MATTER_PATH:-$HOME/esp/esp-matter}"
source ~/esp/esp-matter/export.sh

cd "$(dirname "$0")/m5nanoc6-temperature"

: "${WIFI_PASSWORD:?Please set WIFI_PASSWORD environment variable before running this script.}"
WIFI_SSID="${WIFI_SSID:-SCHOFSEGGEL}"

WIFI_PASSWORD_ESC="${WIFI_PASSWORD//\\/\\\\}"
WIFI_PASSWORD_ESC="${WIFI_PASSWORD_ESC//\"/\\\"}"
WIFI_SSID_ESC="${WIFI_SSID//\\/\\\\}"
WIFI_SSID_ESC="${WIFI_SSID_ESC//\"/\\\"}"

TMP_SDKCONFIG_DEFAULTS="$(mktemp)"
trap 'rm -f "$TMP_SDKCONFIG_DEFAULTS"' EXIT

printf 'CONFIG_DEFAULT_WIFI_SSID="%s"\n' "$WIFI_SSID_ESC" > "$TMP_SDKCONFIG_DEFAULTS"
printf 'CONFIG_DEFAULT_WIFI_PASSWORD="%s"\n' "$WIFI_PASSWORD_ESC" >> "$TMP_SDKCONFIG_DEFAULTS"

SDKCONFIG_DEFAULTS_COMBINED="sdkconfig.defaults;$TMP_SDKCONFIG_DEFAULTS"

idf.py -D SDKCONFIG_DEFAULTS="$SDKCONFIG_DEFAULTS_COMBINED" set-target esp32c6
idf.py -D SDKCONFIG_DEFAULTS="$SDKCONFIG_DEFAULTS_COMBINED" build
idf.py -D SDKCONFIG_DEFAULTS="$SDKCONFIG_DEFAULTS_COMBINED" -p /dev/ttyACM0 flash
#timeout --kill-after=3 20 script -q -c "idf.py -p /dev/ttyACM0 monitor" monitor.log || true
# Strip ANSI escape codes for readability
#sed -i 's/\x1b\[[0-9;]*[mGKHF]//g; s/\r//g' monitor.log
