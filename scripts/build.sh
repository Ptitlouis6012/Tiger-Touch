#!/usr/bin/env bash
# Build the firmware with ESP-IDF 5.5.
#
# ESP-IDF has to be active in this shell first:
#   . $IDF_PATH/export.sh
#
# The result is firmware/build/tiger_touch.bin, plus the bootloader and the
# partition table beside it.

set -euo pipefail
cd "$(dirname "$0")/../firmware"

if ! command -v idf.py >/dev/null 2>&1; then
  echo "idf.py not found: activate ESP-IDF 5.5 first (. \$IDF_PATH/export.sh)" >&2
  exit 2
fi

# sdkconfig.defaults only applies when sdkconfig does not exist yet.
[ -f sdkconfig ] || idf.py set-target esp32s3
idf.py build
