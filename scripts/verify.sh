#!/usr/bin/env bash
# One command that runs everything CI runs.
#
#   bash scripts/verify.sh          the guards, then the firmware build
#   bash scripts/verify.sh --quick  the guards only, no compiling
#
# CI and the bench must not be able to disagree about what "passing" means, so
# the workflow calls this file rather than carrying its own copy of the checks.
#
# Every guard runs even when an earlier one fails: stopping at the first turns
# one session into five. Failures are counted and reported once, at the end.

set -uo pipefail
cd "$(dirname "$0")/.."

MODE="build"
case "${1:-}" in
  --quick) MODE="quick" ;;
  "" | --all) ;;
  *) echo "unknown option: $1"; sed -n '3,6p' "$0"; exit 2 ;;
esac

PY=""
# Run it, do not just look it up: on Windows `python3` can be a Store shortcut
# that exists and does nothing.
for candidate in python3 python py; do
  if "$candidate" -c "import sys" >/dev/null 2>&1; then PY="$candidate"; break; fi
done
if [ -z "$PY" ]; then echo "error: no python interpreter found"; exit 2; fi

FAILED=0
FAILED_NAMES=""

run() {
  local name="$1"; shift
  echo "---- $name"
  if "$@"; then :; else FAILED=$((FAILED + 1)); FAILED_NAMES="$FAILED_NAMES $name"; fi
}

run private-data   "$PY" scripts/check-private-data.py
run bench-residue  "$PY" scripts/check-bench-residue.py
run log-privacy    "$PY" scripts/check-log-privacy.py
run file-format    "$PY" scripts/check-file-format.py
run version        "$PY" scripts/check-version.py
run release-notes  "$PY" scripts/check-release-notes.py
run attribution    bash scripts/check-attribution.sh --tree
run installer-i18n "$PY" scripts/check-installer-i18n.py
run docs           "$PY" scripts/check-docs.py
run agent-rules    "$PY" scripts/check-agent-rules.py
run english        "$PY" scripts/check-english.py
run i18n           "$PY" scripts/check-i18n.py

if [ "$MODE" = "build" ]; then
  run firmware-build bash scripts/build.sh
  # After the build, because it reads what the build made.
  run build-privacy  "$PY" scripts/check-build-privacy.py
fi

echo
if [ "$FAILED" -ne 0 ]; then
  echo "verify: $FAILED failed:$FAILED_NAMES"
  exit 1
fi
echo "verify: all passed"
