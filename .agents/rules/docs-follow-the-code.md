# Documentation follows the code

**Rule.** A change of behaviour corrects its documentation in the same commit: CLAUDE.md, AGENTS.md, llms.txt, CODEMAP.md, docs/, comments and the rules here. A rule that turns out wrong is rewritten in place, in the same session.

**Why.** Founder rule; the owner's order of 2026-10-19 after an audit found 29 claims contradicted by the code.

**How.**
- Check a claim against the code, never against another document.
- Numbers (languages, keys, sizes, counts) are written only where something checks them, or not at all.
- A rule that changes is rewritten where it is; no "superseded" note, git keeps the history.
- `scripts/check-agent-rules.py` checks that the index and the rule files agree and that the paths they cite exist; `scripts/check-docs.py` checks the links of every document.
