# An update is checked whole before it is installed

**Rule.** An update is installed only after its SHA-256 against the manifest, its application name, its version and its image structure have been checked; no switch, setting or build skips a check, and installing is always the owner's touch.

**Why.** Founder rule. Every unit installs what the manifest names; a check that can be turned off is off on someone's device.

**How.**
- The checks are in `firmware/main/net/ota.c`; the whole design is `docs/OTA.md`.
- A rolled-back version is not offered again.
