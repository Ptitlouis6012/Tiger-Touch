# Worklog

Everything done since the last release, noted the moment it is done. Describe
the end state, not the journey; fold fixes into the entry they fix; let reverts
disappear. `CHANGELOG.md`'s `[Unreleased]` section is written from it, and it is
reset to this header at each release.

## Added
- A window on the home screens announces a new version (Later / Yes -> the update page): at once for a new version, again a day later for the same one.

## Changed
- "LAN mode" renamed "Live view" on the screen and in the docs (docs/LIVE-VIEW.md); fields and API unchanged.
- Settings > Battery: green when declared (row icon, value, drawing); the row reads On / Off.
- The update manifest is read 20 s after start (each minute until read), then every 6 h.

## Fixed
- Anycubic cloud printers stayed offline after a restart (link opened before the account brought the cloud identity); the driver now retries every 10 s.
- The update check ran every few minutes: pdMS_TO_TICKS overflowed past 71 min; long waits now go in one-minute steps.
