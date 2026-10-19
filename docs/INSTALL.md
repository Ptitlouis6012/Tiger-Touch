# Installing Tiger Touch on a BIGTREETECH Panda Touch or K-Touch

Tiger Touch **replaces** the firmware BIGTREETECH ships on the screen - the same
hardware sold as the **Panda Touch** and as the **K-Touch**, each with its own
BIGTREETECH firmware. Nothing
is soldered or opened: the board is written over its USB-C port. This page is the
whole procedure, including how to keep a copy of the BigTreeTech firmware and how
to go back to it.

## What you need

- A BIGTREETECH **Panda Touch** or **K-Touch** and a USB-C **data** cable (a charge-only cable
  powers the screen and shows no port).
- A computer with **Chrome or Edge** (Windows, macOS, Linux). Not Safari, not
  Firefox, not a phone: the installer uses WebSerial.
- A TigerSystem account with your printers declared, in
  [Tiger Studio Manager](https://github.com/TigerTag-Project/TigerTag-Studio-Manager).

## 1. The USB driver

The screen's USB-C port is a **WCH CH340K** USB-to-serial chip (USB ID
`1a86:7522`).

| System | What to do |
|---|---|
| Windows 10 / 11 | Usually nothing: Windows Update installs the CH340 driver. If no COM port appears, install WCH's `CH341SER` driver. |
| macOS | **Install WCH's driver - always.** The serial driver built into macOS knows the older CH340 (`1a86:7523`) but not the CH340K: the board is on the USB bus and no port appears. See [On a Mac](#on-a-mac) below. |
| Linux | Nothing: the `ch341` module handles it. Add yourself to the `dialout` (or `uucp`) group if the port is refused. |

Check that it worked: plug the board in and look for the new port (Device Manager
on Windows, `ls /dev/cu.*` on macOS, `ls /dev/ttyUSB*` on Linux).

## 2. Keep a copy of the BigTreeTech firmware (recommended)

Once Tiger Touch is installed, the BigTreeTech firmware is gone from the board.
To be able to go back, read the whole flash (16 MB) first. With
[esptool](https://docs.espressif.com/projects/esptool/) (`pip install esptool`)
and a clone of this repository:

```bash
bash scripts/backup-flash.sh PORT bigtreetech-stock.bin
```

`PORT` is the port from step 1 (`COM5`, `/dev/cu.wchusbserial…`, `/dev/ttyUSB0`).
It takes about 25 minutes. Keep the file: it is the only copy of your board as it
left the factory, settings included. Do not share it: it holds your Wi-Fi and
printer details.

**Why a script and not one `esptool read-flash`:** the screen's CH340K loses
a byte now and then. Above 115200 baud it loses them every few hundred kilobytes
(230400 and 460800 failed on a Mac, with several cables, directly and through a
hub); at 115200 rarely, but over 16 MB one read in one go seldom reaches the
end. The script reads 128 KB at a time, each piece checked against the chip's
own MD5, retries a piece that fails, and picks up where it stopped.

## 3. Install

1. Open **<https://tigertag-project.github.io/Tiger-Touch/>** in Chrome or Edge.
2. Plug the screen in, press **Install**, pick its port.
3. In the window that opens choose **Install Tiger Touch**.
   - **Coming from the BigTreeTech firmware: tick "Erase device".** Its flash is
     laid out differently, and part of it (its update data, at `0xE000`) lies
     inside the area where Tiger Touch keeps its settings. Tiger Touch was seen
     to start without the erase too, but an erased board is a clean start, and
     there is nothing of Tiger Touch's to lose yet.
   - **Updating an existing Tiger Touch over USB: leave it unticked**, and your
     Wi-Fi and account are kept.
4. Wait for the end; the board restarts by itself into the Tiger Touch setup:
   language, Wi-Fi, TigerSystem account ([ONBOARDING.md](ONBOARDING.md)).

Without a browser, from a clone of this repository: see
[Building and flashing it yourself](#building-and-flashing-it-yourself).

## 4. From then on: updates over the air

Nothing more to plug in. *Settings > Update* shows when a new version is
out and installs it (see [OTA.md](OTA.md)). USB is needed again only to go back to
the BigTreeTech firmware, or if a board has to be recovered.

## Going back to the BigTreeTech firmware

Write the copy from step 2 back, whole, at address 0:

```bash
esptool --chip esp32s3 -p PORT -b 115200 write-flash 0 bigtreetech-stock.bin
```

This is the one case where writing a full image at `0x0` is right: it is meant
to restore everything, the BigTreeTech settings included. Writing it over a
Tiger Touch you want to keep would erase its Wi-Fi and account.

Without a copy, BIGTREETECH publishes its own firmware and update procedure:
for the Panda Touch at <https://github.com/bigtreetech/PandaTouch>, and for the
K-Touch on its own product pages. Put back the firmware of the name you bought.

## On a Mac

The CH340K always needs WCH's driver on macOS, and macOS then asks for it to be
allowed - two separate steps; the second is easy to miss.

1. Download **CH34XSER_MAC** from WCH
   ([wch-ic.com](https://www.wch-ic.com/downloads/CH34XSER_MAC_ZIP.html), also on
   [GitHub](https://github.com/WCHSoftGroup/ch34xser_macos)), open the `.zip`, run
   **CH34xVCPDriver** and press **Install**.
2. **Allow it:** *System Settings > General > Login Items & Extensions*, at the
   bottom **Driver Extensions** (the ⓘ button): switch **CH34xVCPDriver** on and
   confirm with your password. Until then the driver is installed but inactive.
3. Unplug and replug the screen. The port appears as
   `/dev/cu.wchusbserial…` (use the `cu.` name, not `tty.`).

To check the driver's state: `systemextensionsctl list | grep wch` must say
`[activated enabled]`; `[activated waiting for user]` means step 2 is not done.

## Building and flashing it yourself

The project is **ESP-IDF 5.5** (`idf.py`), not PlatformIO.

### With Docker (nothing to install but Docker)

```bash
docker run --rm -v "$PWD":/project -w /project/firmware espressif/idf:release-v5.5 idf.py build
```

Docker cannot reach a USB port on macOS or Windows: flash from the computer
itself, with esptool, at the offsets the build lists in `firmware/build/flash_args`:

```bash
cd firmware/build
esptool --chip esp32s3 -p PORT -b 115200 write-flash @flash_args
```

### With ESP-IDF installed (macOS shown; Linux is the same)

Once:

```bash
xcode-select --install
brew install cmake ninja dfu-util python
mkdir -p ~/esp && cd ~/esp
git clone -b release/v5.5 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32s3
```

On a Mac with Apple silicon, if the install complains about the architecture:
`softwareupdate --install-rosetta --agree-to-license`, then run `./install.sh
esp32s3` again.

In every new terminal, then from the root of this repository:

```bash
. ~/esp/esp-idf/export.sh
bash scripts/build.sh
bash scripts/flash.sh PORT
```

`flash.sh` writes the bootloader, the partition table, the OTA data and the
application at their own offsets, at 115200 baud: Wi-Fi and account are kept.
To watch the board: `idf.py -p PORT monitor` (quit with `Ctrl` + `]`).

### With an AI coding assistant

Tell it to activate ESP-IDF **in the same command** as the build (a tool that
runs each command in a fresh shell loses the environment otherwise), never to use
`pio run`, and not to commit unless asked:

> Read CLAUDE.md. ESP-IDF 5.5 is in ~/esp/esp-idf: build with
> `. ~/esp/esp-idf/export.sh && bash scripts/build.sh`. Never run `pio run`.
> Find the port by listing /dev/cu.* before and after plugging the board in.

A sandboxed assistant may be refused the serial port (`Operation not permitted`):
run `flash.sh` yourself in a terminal then.

## When it does not work

| Symptom | Cause, and what to do |
|---|---|
| No port appears | On macOS: the WCH driver, installed **and** allowed ([On a Mac](#on-a-mac)). Otherwise a charge-only cable: the screen lights up, the computer sees nothing. A freshly plugged board can take a few seconds to show its port. |
| `idf.py not found` | ESP-IDF is not active in this terminal: `. ~/esp/esp-idf/export.sh`, then again in the same terminal. |
| The build fails while downloading components | No internet, or a proxy: the first build fetches LVGL and the other managed components. |
| `Operation not permitted` / `Permission denied` on the port | A sandboxed tool, or (Linux) not in the `dialout` group. Flash from a plain terminal. |
| The installer or esptool cannot connect (`Failed to connect`, `No serial data received`) | Another program holds the port (a serial monitor, Cura, a slicer): close it. Unplug and replug. |
| "Corrupt data", garbled output, or a write that fails half-way | The speed: use 115200, and run the command again - a rare lost byte at 115200 is caught and reported, never written silently. |
