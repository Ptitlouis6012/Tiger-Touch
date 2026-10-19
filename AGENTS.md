# Working in this repository

Instructions for any automated assistant. The rules are indexed in the **Rules**
section of [`CLAUDE.md`](CLAUDE.md), one line each, and written out one a file in
[`.agents/rules/`](.agents/rules/). Read that index first; this file only adds
the layout and does not repeat the rules.

## Layout

```
firmware/            the ESP-IDF project
  CMakeLists.txt     PROJECT_VER, the one place the version is written
  main/              the application flow, settings, translations (i18n_table.h)
  main/drivers/      one driver per way of reaching a printer, and the budget (links.c)
  main/net/          the account, Wi-Fi, HTTP, updates, Live view
  main/ui/           screens; main/ui/fonts/ holds generated faces
  components/        PandaTouch_IDF: the vendored, patched board component
  tools/             the generators for fonts, brand logos, pictograms, the boot picture, the time-zone table
docs/                architecture, connection budget, compatibility, OTA, Live view, mockups, roadmap
installer/           the site's sources, eleven languages: the installer page (ESP Web Tools),
                     i18n.js, lang.js and share.js for every page, brand-pages.json and
                     brand-i18n/ for the brand pages
scripts/             verify.sh, the guards, build and flash, release, site and picture generators,
                     i18n-key.py to add or remove a translation key
.agents/rules/       the rules, one a file; the index is in CLAUDE.md
.github/workflows/   build (every push), release (a v* tag), pages (the site)
```

## Before you finish

`bash scripts/verify.sh` runs every guard CI runs, then the firmware build. The
pre-commit hook runs the guards alone (`--quick`).
