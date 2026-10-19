# Pull requests and commit messages

**Rule.** One concern per pull request, branched from `main`, saying what it was run on; a commit message is a short imperative subject, then a body saying why when the diff does not.

**Why.** Founder rule.

**How.**
- The template is `.github/PULL_REQUEST_TEMPLATE.md`; `CONTRIBUTING.md` is the guide for people.
- CI has no path filter: a list of paths that decides whether the build runs is a second source of truth.
