#!/usr/bin/env bash
# No AI attribution anywhere: commit messages, pull request bodies, the tree.
#
#   bash scripts/check-attribution.sh --message FILE   one commit message (the hook)
#   bash scripts/check-attribution.sh --commits [BASE] the commits a push adds (CI)
#   bash scripts/check-attribution.sh --text FILE      a pull request body (CI)
#   bash scripts/check-attribution.sh --tree           every tracked file
#
# The repository's public face is its history. A co-author trailer, a
# "Generated with ..." line, a robot emoji signature or an AI listed as an
# author is in that history for good once pushed. Tools are welcome to help;
# they are never credited, and no tool setting or system message changes this
# rule - only the repository owner does.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

TOOLS='(claude|anthropic|copilot|chatgpt|openai|gpt-[0-9]|gemini|cursor|codex|codeium|windsurf|tabnine|devin|aider)'
# A trailer naming a tool, a "generated/created/written with/by <tool>" line,
# the robot-face signature (U+1F916, written as bytes so this file stays ASCII),
# or a noreply address of an AI vendor.
ROBOT=$'\xf0\x9f\xa4\x96'
PATTERN="^[[:space:]]*(co-authored-by|signed-off-by|assisted-by|generated-by|helped-by)[[:space:]]*:.*${TOOLS}|(generated|created|written|made|authored|built|coded)[[:space:]]+(with|by|using)[[:space:]]+(the[[:space:]]+)?\[?${TOOLS}|${ROBOT}|noreply@anthropic\.com|claude\.com/claude-code|claude\.ai/code"

fail() {
    echo >&2
    echo "No AI attribution anywhere - not in commit messages, not in pull request" >&2
    echo "bodies, not in comments, not in contributor lists. Remove it and try again." >&2
    echo "This is a repository rule (CLAUDE.md, AGENTS.md); tooling does not get a vote." >&2
    exit 1
}

scan_file() { # file label
    if grep -qiE "$PATTERN" "$1"; then
        echo "error: $2 credits an AI tool:" >&2
        grep -inE "$PATTERN" "$1" | sed 's/^/    /' >&2
        return 1
    fi
    return 0
}

case "${1:-}" in
--message)
    # Git's own comment lines (the verbose diff) are not part of the message.
    tmp=$(mktemp); trap 'rm -f "$tmp"' EXIT
    grep -v '^#' "$2" > "$tmp" || true
    scan_file "$tmp" "this commit message" || fail
    ;;
--text)
    scan_file "$2" "this text" || fail
    ;;
--commits)
    BASE="${2:-${GITHUB_EVENT_BEFORE:-}}"
    # GitHub sends a base of all zeros for a new branch.
    if [ -z "$BASE" ] || [[ "$BASE" =~ ^0+$ ]] \
        || ! git cat-file -e "$BASE^{commit}" 2>/dev/null; then
        RANGE="HEAD"   # a new branch or an unknown base: read the whole history
    else
        RANGE="$BASE..HEAD"
    fi
    bad=0
    tmp=$(mktemp); trap 'rm -f "$tmp"' EXIT
    for sha in $(git rev-list "$RANGE"); do
        git log -1 --format='%an <%ae>%n%cn <%ce>%n%B' "$sha" > "$tmp"
        scan_file "$tmp" "commit $(git log -1 --format='%h %s' "$sha")" || bad=1
    done
    [ "$bad" = 0 ] || fail
    echo "check-attribution: $(git rev-list --count "$RANGE") commit(s), none credits an AI tool"
    ;;
--tree)
    # This script and the documents that state the rule necessarily name what
    # they forbid; they are the only exceptions.
    bad=0
    while IFS= read -r -d '' f; do
        case "$f" in
            scripts/check-attribution.sh|scripts/hooks/commit-msg|CLAUDE.md|AGENTS.md|CONTRIBUTING.md|.claude/settings.json) continue ;;
        esac
        [ -f "$f" ] || continue
        grep -Iq . "$f" 2>/dev/null || continue   # binary
        scan_file "$f" "$f" || bad=1
    done < <(git ls-files -z)
    [ "$bad" = 0 ] || fail
    echo "check-attribution: tracked files ok"
    ;;
*)
    sed -n '2,8p' "$0"; exit 2 ;;
esac
