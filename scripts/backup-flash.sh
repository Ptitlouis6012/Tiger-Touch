#!/usr/bin/env bash
# Read a board's whole flash (16 MB) into one file, in verified pieces.
#
#   bash scripts/backup-flash.sh PORT [OUT]      OUT defaults to bigtreetech-backup.bin
#
# Meant for the copy of the BigTreeTech firmware taken before the first install
# (docs/INSTALL.md). The screen's CH340K drops a byte now and then even at
# 115200 baud, so a single 16 MB read rarely gets to the end. This reads 128 KB
# at a time - esptool checks each piece against the chip's own MD5 - retries a
# piece that fails, and resumes where it stopped if run again.
#
# The output holds the board's settings, Wi-Fi included: never commit or share it.
set -uo pipefail

PORT="${1:-}"
OUT="${2:-bigtreetech-backup.bin}"
if [ -z "$PORT" ]; then sed -n '4,4p' "$0"; exit 2; fi
command -v esptool >/dev/null 2>&1 || { echo "esptool not found: pip install esptool" >&2; exit 2; }

PIECE=$((128 * 1024))
COUNT=128
DIR="$OUT.pieces"
mkdir -p "$DIR"
for i in $(seq 0 $((COUNT - 1))); do
  f="$DIR/$(printf %03d "$i").bin"
  if [ -f "$f" ] && [ "$(wc -c < "$f")" -eq "$PIECE" ]; then continue; fi
  for try in $(seq 1 20); do
    rm -f "$f"
    if esptool --chip esp32s3 -p "$PORT" -b 115200 read-flash $((i * PIECE)) "$PIECE" "$f" >/dev/null 2>&1 \
       && [ -f "$f" ] && [ "$(wc -c < "$f")" -eq "$PIECE" ]; then
      break
    fi
    sleep 1
  done
  [ -f "$f" ] || { echo "piece $i failed 20 times: check the cable and the port, then run again" >&2; exit 1; }
  printf '\r%d / %d' $((i + 1)) "$COUNT"
done
echo
cat "$DIR"/*.bin > "$OUT"
rm -rf "$DIR"
echo "$OUT written ($(wc -c < "$OUT") bytes). Keep it private: it holds the board's settings."
