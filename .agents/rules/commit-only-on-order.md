# Commit, tag and push only on the owner's order

**Rule.** Never commit, tag or push unless the owner asks for it in this conversation. Push with the `BenGlut` GitHub account; never switch to another account to push. A pushed tag is never moved.

**Why.** Founder rule. A pushed `v*` tag is acted on by the release workflow and publishes an update to every device; no later edit takes it back. The `BenGlut` account: the owner's order of 2026-10-18.

**How.**
- Commit only the files of the change you were asked for (`git add <paths>`, not `git add -A`): files written by another tool or session may sit in the tree.
- Push as the active account: `gh auth status` shows `BenGlut`; no `gh auth switch`. If a push or a workflow run is refused for rights, say so and ask.
- A wrong tag is followed by the next version, never re-pointed.
- The hooks run on every commit (`scripts/hooks/pre-commit`, `scripts/hooks/commit-msg`); `--no-verify` is for a branch nobody pulls.
