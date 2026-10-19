# No bench code in the tree

**Rule.** No serial capture console, screenshot or log-capture tooling, fake printer, geometry or memory dump, or measurement log is committed. Two product features are the exceptions: Demo mode and Live view.

**Why.** Founder rule. Bench code is a way into a shipped device and a tree nobody can read. Demo mode: the owner's decision of 2026-10-17. Live view: the owner's decision of 2026-10-18.

**How.**
- Bench aids live in `local/` (ignored by git); the bench build (`local/bench/`) serves unauthenticated `/screen.bmp`, `/api/tap`, `/api/swipe` on port 81. Never flash a bench build on a board that leaves the bench.
- Demo mode (`firmware/main/demo_mode.c`, *Settings > Demo mode*): off by default, ten made-up printers, reaches no printer of the account, sends nothing to any machine, saves nothing over the owner's choices, tells the account `demo: true`. A change that lets it reach a printer or send a command breaks the exception.
- Live view (`firmware/main/net/lan_mode.c`, `docs/LIVE-VIEW.md`): the screen and its touches for Tiger Studio Manager, behind the access code on every request; a remote touch is no stronger than a finger.
- `scripts/check-bench-residue.py` fails on the names it knows.
