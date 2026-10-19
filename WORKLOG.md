# Worklog

Everything done since the last release, noted the moment it is done. Describe
the end state, not the journey; fold fixes into the entry they fix; let reverts
disappear. `CHANGELOG.md`'s `[Unreleased]` section is written from it, and it is
reset to this header at each release.

## Added
- Agent rules: one file each in .agents/rules/, indexed in CLAUDE.md, checked by scripts/check-agent-rules.py; scripts/i18n-key.py adds or removes a translation key in every language; scripts/check-history.py reads the whole history for private data, before every push and at release.

## Changed
- Documentation checked against the code and corrected; 11 unused translation rows removed; bump-version.sh resets this file.

## Fixed
- The feed task's creation is checked; make_fonts.py finds LVGL 9.6's fonts.
