# Flash a board safely

**Rule.** Before writing to a board, identify it: read its partition table and its application's name and confirm it is a Tiger Touch. Write the parts at their own offsets, never a merged image at 0x0, at 115200 baud.

**Why.** Incident of 2026-10-13: the only ESP32-S3 on USB was a TigerScale, and Tiger Touch was flashed onto it (restored from a full backup). A merged image spans the NVS (0x9000) and wipes the saved Wi-Fi and account. Above 115200 baud the CH340K loses bytes (2026-10-13).

**How.**
- A Panda Touch / K-Touch shows up as `/dev/cu.wchusbserial*` or `usbserial*` (a CH340K), not `usbmodem`.
- Identify: `esptool --chip esp32s3 --port <port> --baud 115200 read-flash 0x8000 0xc00 pt.bin`, then the app descriptor at `0x20020` (project `tiger_touch`). Two 6 MB OTA slots and NVS at 0x9000 is Tiger Touch's table. If unsure, ask which device is on which port; take a full `read-flash` backup before the first write to an unknown board.
- Write: `bash scripts/flash.sh <port>` (bootloader, partition table, OTA data and application; NVS untouched), or the same parts with esptool from `firmware/build/flasher_args.json`. `BAUD=` only on a host known to hold more.
- A lost byte makes esptool stop with "Corrupt data": run it again.
- Opening the serial port with DTR or RTS set resets the board; a logger opens it with both low.
