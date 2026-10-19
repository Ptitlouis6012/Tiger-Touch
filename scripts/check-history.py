#!/usr/bin/env python3
"""The whole history, read for what must never be published.

`check-private-data.py` reads the files as they are now. A secret committed once
and removed later is still in the history, and the history is what a public
repository publishes: every version of every file, deleted ones included, and
every commit message. This reads all of it with the same patterns and the same
local `.private-terms`.

  python3 scripts/check-history.py          every commit reachable from any ref
  python3 scripts/check-history.py A..B     only the commits of a range (a push)

It runs before every push (scripts/hooks/pre-push) and in the release workflow.
A finding in a commit already pushed is fixed by rewriting the history, never by
a new commit: that is the owner's decision (.agents/rules/no-private-data.md).
"""
import importlib.util
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
spec = importlib.util.spec_from_file_location("cpd", os.path.join(ROOT, "scripts", "check-private-data.py"))
cpd = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cpd)


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, check=True).stdout


def main():
    rng = sys.argv[1:] or ["--all"]
    commits = git("rev-list", *rng).split()
    terms = cpd.local_terms()
    problems = []
    seen = set()
    for c in commits:
        # The message, author and committer of each commit.
        meta = git("log", "-1", "--format=%an <%ae>%n%cn <%ce>%n%B", c)
        for _rel, n, what in cpd.scan("commit " + c[:10], meta, terms):
            problems.append("commit %s, message line %d: %s" % (c[:10], n, what))
        # Every file of that commit, each blob read once across the whole history.
        for line in git("ls-tree", "-r", c).splitlines():
            meta_part, rel = line.split("\t", 1)
            kind, blob = meta_part.split()[1], meta_part.split()[2]
            if kind != "blob" or blob in seen or not cpd.is_text(rel):
                continue
            seen.add(blob)
            try:
                text = subprocess.run(["git", "cat-file", "-p", blob], cwd=ROOT, capture_output=True,
                                      check=True).stdout.decode("utf-8")
            except UnicodeDecodeError:
                continue
            for _rel, n, what in cpd.scan(rel, text, terms):
                problems.append("commit %s, %s:%d: %s" % (c[:10], rel, n, what))
    if problems:
        print("private data in the history (%d):" % len(problems))
        for p in problems[:80]:
            print("  " + p)
        if len(problems) > 80:
            print("  ... and %d more" % (len(problems) - 80))
        return 1
    print("check-history: ok (%d commits, %d file versions, %d local terms)" % (len(commits), len(seen), len(terms)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
