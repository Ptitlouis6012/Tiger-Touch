# Contributing

Thanks for being here. Talk to us on **[Discord](https://discord.gg/3Qv5TSqnJH)**.

## What helps most, in order

### 1. Run it on your printer, and tell us

Seven of the ten drivers are written from each maker's protocol and compiled,
but have never run against the real machine
([docs/PRINTER-COMPATIBILITY.md](docs/PRINTER-COMPATIBILITY.md)). A report saying
"Elegoo Centauri Carbon 2, firmware 1.1.4, LAN: state and temperatures right,
the filament units empty" is worth more than any amount of code review. Use the
**Printer support** issue template.

### 2. Translations

Every on-screen sentence is one row of `firmware/main/i18n_table.h`, in eleven
columns. A better word in your language is a one-line pull request. The panel is
800 px wide: a sentence much longer than the English one will be cut off.

### 3. Code

Read [CLAUDE.md](CLAUDE.md) (the rules, with the reason for each) and
[CODEMAP.md](CODEMAP.md) (the map, and the Landmines table) first. Then:

```bash
bash scripts/install-hooks.sh   # once per clone: the guards and the attribution check
bash scripts/verify.sh          # every guard CI runs, then the build
```

**A new printer brand** is one driver in `firmware/main/drivers/` that fills
`printer_view_t`, plus its line in `drivers.c`. If it needs a screen to know
which brand is behind it, say so in the pull request: the interface is probably
wrong, which is worth knowing.

## Rules that are not negotiable

- **No private data in the repository**: no e-mail, password, token, Wi-Fi name,
  IP or MAC address, serial number or personal path - not in code, logs, issues
  or screenshots. The history is permanent.
- **English everywhere that is committed.** On-screen text goes through
  `i18n_table.h`, in all eleven languages at once.
- **A printer is commanded only to pause, resume, stop, or switch its light, on
  a touch.** Anything else is a decision of the maintainer, not a convenience.
- **A constant that looks arbitrary carries a comment saying what breaks
  without it.**
- **No AI tool is credited anywhere**: not in commits, pull requests, comments or
  contributor lists. Use whatever tools you like; the `commit-msg` hook and CI
  refuse a `Co-Authored-By`, `Generated with` or similar line naming one.

## Pull requests

1. Fork, branch from `main`, one concern per pull request.
2. Say **what you ran it on**: the board, and for a driver the printer model, its
   firmware and LAN or cloud. "Compiled, not run on the machine" is an honest,
   useful answer.
3. Add a line under `## [Unreleased]` in [CHANGELOG.md](CHANGELOG.md) for
   anything a user would notice. Releases are cut by the maintainer
   (`scripts/bump-version.sh`, then a tag); do not change `PROJECT_VER` yourself.

Commits: a short imperative subject, and a body that says *why* when the diff
does not.

## Licensing

Contributions are MIT-licensed like the rest of the repository ([LICENSE](LICENSE)).
Do not contribute code you do not have the right to relicense, including
anything decompiled from a maker's software.

## Code of conduct

[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). Be decent to each other.
