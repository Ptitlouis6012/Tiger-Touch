# Worklog

Everything done since the last release, noted the moment it is done. Describe
the end state, not the journey; fold fixes into the entry they fix; let reverts
disappear. `CHANGELOG.md`'s `[Unreleased]` section is written from it, and it is
reset to this header at each release.

## Added
- Settings > LAN mode, as on TigerScale V3: Tiger Studio takes screenshots, shows the screen live and sends touches, behind a six-character access code also written to the account (lan_live, lan_access_code, lan_url). No web page.
- docs/TELEMETRY.md: the presence document field by field, and the brief for Tiger Studio.
- The home screen shows the time (hours and minutes) in the middle of its top line.
- Settings > Demo mode: ten made-up printers printing, for showrooms and screenshots; nothing real reached or sent.
- Settings > Battery (yes by default), sent to the account as battery_present.
- The installer page invites people not installing today to join the TigerSquad on tigersystem.io.

## Changed
- The presence document is now users/{uid}/tigertouchs/{mac} (was tigertouches).
- Settings shows the account's display name, as TigerScale V3 does, instead of its e-mail.
- One header on every screen: 64 px, no rule under it, everything centred on the same line.
- Every picture of the screen (README, docs, installer) taken anew, in English, in Demo mode; the gallery gains the X1 Carbon and the K2 Plus.
- Progress colours: blue printing, orange paused, green finished, red on a fault (ring, card, list bar).
- Stop opens a confirmation (Cancel / Stop) instead of a 1.5 s hold.
- The clock also sits in the middle of the Settings header and of a printer's full view.
- README: an animated picture of the screen (start-up, cards, list, full view) and printing screenshots; installer flip shows the cards printing.
- The settings row 'Change account' is now just 'Account'.
- The installer page opens on the name, Tiger Touch, in large type, on every screen size.
- The installer page is laid out for desktop, tablet and phone; the brand logos follow the text colour.
- The printer-brands strip sits above the picture of the screen, in the README and on the installer page.
- The README presents the TigerPOD Mini (picture, what it does, its free MakerWorld file) in place of the original TigerPOD.
- The TigerPOD Mini's picture is the new blue one, at the old picture's size (420 x 622).

## Fixed
- The title of an AMS HT no longer runs into the edge of its card: the small units centre it over the dot.
- Demo mode: the estimated end no longer moves (the time left runs at the progress's pace); the Ender-3 S1 prints too.

## Removed
