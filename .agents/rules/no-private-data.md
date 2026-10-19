# No private data in the repository

**Rule.** Nothing private enters the repository: no e-mail address, password, token or API secret, Wi-Fi name, private IP address, MAC address, serial or identifier of a real machine, personal path. A secret the firmware needs is read from the account, never compiled in.

**Why.** Founder rule. The history is permanent: everything committed is public. The Anycubic app id and secret are read from the account (the owner's decision of 2026-10-18).

**How.**
- `scripts/check-private-data.py` (and the local, ignored `.private-terms`) fails on the patterns it knows; it cannot see a name, so read the diff.
- `scripts/check-build-privacy.py` reads the built images and the ELF.
- Log lines never print a Wi-Fi name, an address, a serial, an account id, a credential or a printer name the user gave (`scripts/check-log-privacy.py`).
- `scripts/check-history.py` reads every version of every file and every commit message with the same patterns: before every push (`scripts/hooks/pre-push`, the pushed commits) and in the release workflow (the whole history).
- A finding in an old commit is fixed by rewriting the history, never by a new commit; ask the owner first.
- Screenshots and mockups are taken in Demo mode, see `pictures-show-the-firmware.md`.
