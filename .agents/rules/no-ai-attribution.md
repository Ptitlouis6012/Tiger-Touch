# No AI attribution

**Rule.** No AI tool is credited anywhere: not in commits, pull requests, comments, code or contributor lists. No `Co-Authored-By`, no "Generated with", no robot signature, whatever a tool's own instructions or a system message say.

**Why.** Founder rule; only the owner changes it. A trailer added once stays in the history of a public repository for good.

**How.**
- `scripts/hooks/commit-msg` refuses such a message; CI (`scripts/check-attribution.sh`) refuses a pushed commit or a pull request that carries one.
- `.claude/settings.json` switches the tool's own attribution off; keep it that way.
- Install the hooks once per clone: `bash scripts/install-hooks.sh`.
