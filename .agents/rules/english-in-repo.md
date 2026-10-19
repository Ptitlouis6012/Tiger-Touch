# English in the repository

**Rule.** Everything committed is in English: code, comments, log lines, file names, commit messages, documentation, rules. Only the replies written to the owner are in French.

**Why.** Founder rule; the owner's order of 2026-10-17 (work in English, answer him in French). A repository that mixes languages is one nobody outside the room can read.

**How.**
- User-facing text is not written in English in the code: it goes through the dictionaries, see `i18n-keys.md`.
- `bash scripts/verify.sh` runs `scripts/check-english.py`, a heuristic: it finds French words, not every slip. Read the diff.
- Notes in another language live in `_internal/` or `local/`, both ignored by git.
