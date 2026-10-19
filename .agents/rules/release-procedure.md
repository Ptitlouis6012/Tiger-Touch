# Release procedure

**Rule.** A release is: `bash scripts/bump-version.sh X.Y.Z`, the owners' note `release-notes/vX.Y.Z.md`, `bash scripts/verify.sh`, a commit `Release X.Y.Z: <what it holds>`, the tag `vX.Y.Z`, a push - each step on the owner's order.

**Why.** Founder rule. The tag is what publishes: the release workflow builds, publishes the images and the OTA manifest, and every device offers the update within a day.

**How.**
- Version: a fix or a hardening is a patch, a feature a minor, a break a major.
- `PROJECT_VER` in `firmware/CMakeLists.txt` is the only place the version is written; `bump-version.sh` writes it, moves `[Unreleased]` to `[X.Y.Z]` and resets `WORKLOG.md`.
- The owners' note says the release in plain words for people who own the screen ("Fixed an issue where ..."), no internal names. The release page shows it, then the changelog section folded under it (`.github/workflows/release.yml`).
- `scripts/check-version.py` refuses a tag that disagrees with `PROJECT_VER`.
