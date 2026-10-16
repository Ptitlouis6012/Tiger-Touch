#!/usr/bin/env bash
# Point git at the tracked hooks, once per clone.
#
# core.hooksPath rather than copies in .git/hooks: a copy goes stale the moment
# the hook changes, and nobody re-runs an installer they ran once.
#   pre-commit  the guards (verify.sh --quick), no compiling
#   commit-msg  refuses a message that credits an AI tool
set -euo pipefail
cd "$(dirname "$0")/.."
chmod +x scripts/hooks/*
git config core.hooksPath scripts/hooks
echo "hooks installed: guards before every commit, attribution check on every message"
