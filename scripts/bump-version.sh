#!/usr/bin/env bash
# Set the firmware version, and only ever from here.
#
#   bash scripts/bump-version.sh 2.1.0
#
# The version is written in exactly one place, PROJECT_VER in
# firmware/CMakeLists.txt. This script changes it and moves everything written
# under `## [Unreleased]` in CHANGELOG.md into a heading for the release, dated
# today. If nothing had been written there, it leaves a scaffold line that
# check-release-notes.py refuses, so a release cannot go out with notes nobody
# wrote.
#
# A fix or a hardening is a patch, a feature a minor, a break a major.
# The script does not commit, tag or push: those are decisions, and a script
# that makes them for you eventually makes them by accident.

set -euo pipefail
cd "$(dirname "$0")/.."

NEW="${1:-}"
if ! [[ "$NEW" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "usage: bash scripts/bump-version.sh MAJOR.MINOR.PATCH" >&2
  exit 2
fi

PY=""
# Run it, do not just look it up: on Windows `python3` can be a Store shortcut
# that exists and does nothing.
for candidate in python3 python py; do
  if "$candidate" -c "import sys" >/dev/null 2>&1; then PY="$candidate"; break; fi
done
if [ -z "$PY" ]; then echo "error: no python interpreter found" >&2; exit 2; fi

# Python rather than `sed -i`: GNU and BSD sed disagree on -i, and this runs on
# Linux, macOS and Windows alike.
"$PY" - "$NEW" "$(date +%Y-%m-%d)" <<'PY'
import pathlib, re, sys

new, today = sys.argv[1], sys.argv[2]

cmake = pathlib.Path("firmware/CMakeLists.txt")
s = cmake.read_text(encoding="utf-8")
m = re.search(r'^set\(PROJECT_VER "([^"]*)"\)', s, re.M)
if not m:
    sys.exit("PROJECT_VER not found in firmware/CMakeLists.txt")
old = m.group(1)
if old == new:
    print(f"PROJECT_VER is already {new}")
else:
    key = lambda v: tuple(int(x) for x in v.split("."))
    if key(new) < key(old):
        sys.exit(f"refusing to go backwards: {old} -> {new}")
    cmake.write_text(s[:m.start(1)] + new + s[m.end(1):], encoding="utf-8", newline="")
    print(f"firmware/CMakeLists.txt: {old} -> {new}")

log = pathlib.Path("CHANGELOG.md")
s = log.read_text(encoding="utf-8")
if re.search(rf"^## \[{re.escape(new)}\]", s, re.M):
    print(f"CHANGELOG.md: [{new}] already there")
    sys.exit(0)
marker = "## [Unreleased]"
if marker not in s:
    sys.exit("CHANGELOG.md has no [Unreleased] section")
head, rest = s.split(marker, 1)
body = rest.split("\n## ", 1)[0]
said = [l for l in body.splitlines() if l.strip() and not l.strip().startswith("#")]
if not said:
    rest = ("\n_Describe this release. See WORKLOG.md - that is what it is for._\n" + rest)
    print(f"CHANGELOG.md: nothing under Unreleased - scaffolded [{new}]")
else:
    print(f"CHANGELOG.md: moved Unreleased into [{new}]")
log.write_text(f"{head}{marker}\n\n## [{new}] - {today}\n{rest}", encoding="utf-8", newline="")
PY

cat <<MSG

Next, if that is what you want (each step is a decision of its own):

  bash scripts/verify.sh
  git commit -am "Release $NEW: <what it holds>"
  git tag v$NEW && git push origin main v$NEW

The release workflow refuses to publish when the tag and PROJECT_VER disagree,
or when [$NEW] has no notes. It builds, publishes the images and the OTA
manifest; every Tiger Touch in the field then sees the update.
MSG
