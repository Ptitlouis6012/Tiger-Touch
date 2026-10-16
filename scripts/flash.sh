#!/usr/bin/env bash
# Flash a board over its USB-C port.
#
#   bash scripts/flash.sh /dev/ttyUSB0
#
# Three things matter, and each was learned the hard way:
#
#   * 115200 baud. The board's USB-to-serial bridge (CH340K) loses bytes above
#     that (230400 and 460800 failed on a Mac with WCH's driver, with any cable;
#     921600 gives a board that boots into noise). BAUD=230400 to try faster.
#   * Parts are written at their own offsets - bootloader, partition table,
#     application. `idf.py flash` does exactly that and never touches the NVS at
#     0x9000, so the saved Wi-Fi and account survive an update.
#   * Never write a merged image at 0x0: it spans the NVS and wipes it.

set -euo pipefail
cd "$(dirname "$0")/../firmware"

PORT="${1:-}"
if [ -z "$PORT" ]; then
  echo "usage: bash scripts/flash.sh <serial port>" >&2
  exit 2
fi
if ! command -v idf.py >/dev/null 2>&1; then
  echo "idf.py not found: activate ESP-IDF 5.5 first (. \$IDF_PATH/export.sh)" >&2
  exit 2
fi

idf.py -p "$PORT" -b "${BAUD:-115200}" flash
