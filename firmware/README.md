# firmware/

The Tiger Touch firmware: one ESP-IDF project for the BIGTREETECH Panda Touch /
K-Touch screen (one board, sold under two names).

## Layout

```
firmware/
├── CMakeLists.txt        PROJECT_VER - the one place the version is written
├── sdkconfig.defaults    what the project sets; the generated sdkconfig is ignored
├── partitions.csv        nvs, otadata and two 6 MB application slots (frozen)
├── dependencies.lock     pins the managed components - committed on purpose
├── main/
│   ├── main.c            the application task (flow) and the feed task
│   ├── settings.c        the NVS wrapper
│   ├── i18n.c            tr(); i18n_table.h holds every sentence, 11 languages
│   ├── drivers/          how a printer is reached
│   │   ├── links.c         one open connection per enabled printer, and the budget
│   │   ├── drivers.c       the registry; drv_*.c one per way of reaching a printer
│   │   ├── k2_client.c     the Creality WebSocket client
│   │   ├── driver_util.h   JSON and colour helpers every driver shares
│   │   └── ws_rx.c, mqtt_rx.c   chunked-message reassembly, once
│   ├── net/              the network and the account
│   │   ├── cloud.c         the TigerTag account: sign-in, printers, language
│   │   ├── wifi_mgr.c      Wi-Fi: scan, connect, signal
│   │   ├── http_lite.c     a small blocking HTTP client for the pollers
│   │   └── ota.c           over-the-air update: manifest, verified download, probation
│   └── ui/               screens and what they draw with
│       ├── ui_setup.c, ui_settings.c, ui_dashboard.c, ui_wifi.c
│       ├── ui_onboard.c    the first-start questions, sign-in, Google pairing
│       ├── screen.c        brightness, sleep, orientation
│       ├── fonts/          generated faces              (never hand-edited)
│       ├── brand_logos.c   generated brand logos        (never hand-edited)
│       ├── pictos.c        generated pictograms         (never hand-edited)
│       └── boot_splash.c   generated boot picture       (never hand-edited)
├── components/
│   └── PandaTouch_IDF/   BigTreeTech's board component, vendored and patched
└── tools/                generators: fonts, brand logos, boot picture
```

What each file owns and the landmines to read before editing it are in
[`../CODEMAP.md`](../CODEMAP.md).

## Rules for this tree

**No credentials, no private data.** Everything a device needs comes from the
account or from NVS at run time; the published binary is the same for everyone.

**English only.** Comments, identifiers, log lines. User-facing text goes through
`i18n_table.h`, in all eleven languages at once.

**No bench code.** No capture console, no demo data, no memory dump. See
`scripts/check-bench-residue.py`.

**An arbitrary-looking constant says why.** A delay, a buffer size, a retry count:
it looks arbitrary because it was expensive to find. Say what breaks if it changes.

## Build

```bash
. $IDF_PATH/export.sh            # ESP-IDF 5.5
bash ../scripts/build.sh
bash ../scripts/flash.sh /dev/ttyUSB0
```

## Regenerating the generated files

| File | Script | Needs |
|---|---|---|
| `main/ui/fonts/*` | `python tools/make_fonts.py --noto PATH/NotoSansSC-Medium.otf` | one build (LVGL's fonts), Node |
| `main/ui/brand_logos.c` | `python tools/make_brand_logos.py` | `tools/brand_logos.json` |
| `main/ui/pictos.c` | `python tools/make_pictos.py` | `tools/pictos.json` (drawn from `tools/pictos/*.svg`) |
| `main/ui/boot_splash.c` | `python tools/make_splash.py PICTURE.png` | the boot picture |
