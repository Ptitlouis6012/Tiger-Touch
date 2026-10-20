# Worklog

Everything done since the last release, noted the moment it is done. Describe
the end state, not the journey; fold fixes into the entry they fix; let reverts
disappear. `CHANGELOG.md`'s `[Unreleased]` section is written from it, and it is
reset to this header at each release.

## Added
- Agent rule this-repository-only: files changed only here, issues in any repository.

## Removed
- The state words (S_ST_*, S_OFFLINE) and ui_state_text: no screen shows them any more.

## Fixed
- List rows: the nozzle and bed pictograms are drawn (24 px, the only size they have), not asked for at 16 px.

## Changed
- Printer cards: the progress is a line under the three buttons, which share the card's width; the ring is gone.
- Full view: the ring is a line under the three buttons, which share the card's width; no state pill, the logo takes the state's colour, the name stays white (ui_state_tone, ui_beat).
- Cards and list: no state dot and word; the logo and the progress line take the state's colour, the name stays white (tone_of); an error pulses (ui_beat).
