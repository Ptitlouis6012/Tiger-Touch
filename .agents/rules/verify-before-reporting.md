# Verify before reporting

**Rule.** Run `bash scripts/verify.sh` (every guard, then the firmware build) before saying a piece of work is done, and report what was done and what it gave; never claim a result you do not have.

**Why.** Founder rule.

**How.**
- `verify.sh` needs ESP-IDF 5.5 for the build: `. $IDF_PATH/export.sh`, or run it in the `espressif/idf:release-v5.5` container.
- `bash scripts/verify.sh --quick` runs the guards without the build (the pre-commit hook does this).
- Ask the owner for a test on a machine only when it needs the machine in front of him, once, at the moment it is needed.
