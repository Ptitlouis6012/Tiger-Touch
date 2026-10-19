# Changelog written as the work is done

**Rule.** Anything a user would notice gets an entry under `## [Unreleased]` in `CHANGELOG.md`, written with the change, and a line in `WORKLOG.md`.

**Why.** Founder rule. Notes written at release time are written from memory.

**How.**
- The changelog is the technical record; describe the end state, fold a fix into the entry it fixes.
- `WORKLOG.md` holds one line per change since the last release; `scripts/bump-version.sh` resets it.
- `scripts/check-release-notes.py` refuses a release whose changelog section or owners' note is missing.
