# Worklog

Everything done since the last release, noted the moment it is done. Describe
the end state, not the journey; fold fixes into the entry they fix; let reverts
disappear. `CHANGELOG.md`'s `[Unreleased]` section is written from it, and it is
reset to this header at each release.

## Added
- Agent rules: one file each in .agents/rules/, indexed in CLAUDE.md, checked by scripts/check-agent-rules.py; scripts/i18n-key.py adds or removes a translation key in every language; scripts/check-history.py reads the whole history for private data, before every push and at release.

## Changed
- Documentation checked against the code and corrected; 11 unused translation rows removed; bump-version.sh resets this file.
- Settings > Screen: the screen sleeps after 1 min, 10 min, 30 min, 1 h, or never (it was 30 s, 1 min, 5 min, never); a saved delay no longer offered becomes the next one up.
- Home, list view: the printers are in order of the estimated end of their print (the first to finish on top, those with no print last); the rows hold still for 3 s after a touch.

## Fixed
- Bambu Lab: the light button switches both chamber lights of an H2S, H2D, H2D Pro, H2C and X2D (chamber_light and chamber_light2); on when either is. By the account record's model id, or a report naming chamber_light2.
- The feed task's creation is checked; make_fonts.py finds LVGL 9.6's fonts.
