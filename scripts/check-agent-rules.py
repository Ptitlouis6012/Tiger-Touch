#!/usr/bin/env python3
"""The agent rules: the index and the rule files agree, every rule has its form,
and the paths the entry points cite exist.

- `.agents/rules/<name>.md`, one rule a file, starts with `# Title` and holds
  `**Rule.**`, `**Why.**` and `**How.**`, in that order.
- The "Rules" section of CLAUDE.md is the index: it cites every rule file, and
  every file it cites exists.
- A path cited in CLAUDE.md, AGENTS.md or a rule (a link, or a backticked name
  holding a slash or a file name) exists in the tree. Templates (`<x>`, `{x}`,
  `*`, `X.Y.Z`), URLs and image names (anything with a colon), routes (`/screen.bmp`), paths in other repositories (`../...`) and
  paths git ignores (`local/`, build output) are left alone.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RULES = os.path.join(ROOT, ".agents", "rules")
INDEX = os.path.join(ROOT, "CLAUDE.md")
ENTRY = ["CLAUDE.md", "AGENTS.md"]
FILE_EXT = (".md", ".py", ".sh", ".c", ".h", ".json", ".js", ".csv", ".txt", ".yml", ".html", ".css", ".svg",
            ".png", ".webp", ".gif", ".jpg", ".bin")
# Commands and names that look like files and are not in the tree.
NOT_PATHS = {"idf.py", "flasher_args.json", "values.json", "pt.bin", "TigerTag-Project/Tiger-Touch",
             # what a release publishes, not files of the tree
             "SHA256SUMS.txt", "manifest.json", "version.json"}


def tracked_names():
    out = subprocess.run(["git", "ls-files", "-co", "--exclude-standard"], cwd=ROOT, capture_output=True,
                         text=True).stdout.split()
    return set(out), {os.path.basename(p) for p in out}


def ignored(path):
    r = subprocess.run(["git", "check-ignore", "-q", path], cwd=ROOT)
    return r.returncode == 0


def cited_paths(text):
    found = set()
    for m in re.finditer(r"\]\(([^)\s]+)\)", text):
        found.add(m.group(1).split("#")[0])
    for m in re.finditer(r"`([^`\s]+)`", text):
        tok = m.group(1)
        if "/" in tok or tok.endswith(FILE_EXT):
            found.add(tok)
    return {f for f in found if f}


def check_path(tok, base, tracked, basenames):
    if re.search(r"[<>{}*:]|X\.Y\.Z|^\.\./|^/|^~|^\$|=", tok) or tok in NOT_PATHS:
        return None
    tok = tok.rstrip(".,;:")
    if "/" not in tok:
        return None if tok in basenames else "%s is not a file of the tree" % tok
    for cand in (os.path.normpath(os.path.join(base, tok)), os.path.normpath(os.path.join(ROOT, tok))):
        rel = os.path.relpath(cand, ROOT)
        if os.path.exists(cand) or rel in tracked or rel.rstrip("/") in tracked or ignored(rel):
            return None
    return "%s does not exist" % tok


def main():
    problems = []
    rule_files = sorted(f for f in os.listdir(RULES) if f.endswith(".md"))
    for f in rule_files:
        text = open(os.path.join(RULES, f), encoding="utf-8").read()
        if not text.startswith("# "):
            problems.append(".agents/rules/%s: does not start with '# Title'" % f)
        pos = [text.find(k) for k in ("**Rule.**", "**Why.**", "**How.**")]
        if min(pos) < 0 or pos != sorted(pos):
            problems.append(".agents/rules/%s: needs **Rule.**, **Why.** and **How.**, in that order" % f)
        if not re.fullmatch(r"[a-z0-9]+(-[a-z0-9]+)*\.md", f):
            problems.append(".agents/rules/%s: the name is not kebab-case" % f)

    index = open(INDEX, encoding="utf-8").read()
    m = re.search(r"^## Rules\n(.*?)(?=^## )", index, re.M | re.S)
    if not m:
        problems.append("CLAUDE.md has no '## Rules' section")
        section = ""
    else:
        section = m.group(1)
    cited = set(re.findall(r"\.agents/rules/([a-z0-9-]+\.md)", section))
    for f in sorted(set(rule_files) - cited):
        problems.append("CLAUDE.md's Rules index does not cite .agents/rules/%s" % f)
    for f in sorted(cited - set(rule_files)):
        problems.append("CLAUDE.md's Rules index cites .agents/rules/%s, which does not exist" % f)

    tracked, basenames = tracked_names()
    for doc in ENTRY + [os.path.join(".agents", "rules", f) for f in rule_files]:
        text = open(os.path.join(ROOT, doc), encoding="utf-8").read()
        text = re.sub(r"```.*?```", "", text, flags=re.S)  # commands in a block are examples
        base = os.path.dirname(os.path.join(ROOT, doc))
        for tok in sorted(cited_paths(text)):
            err = check_path(tok, base, tracked, basenames)
            if err:
                problems.append("%s: %s" % (doc, err))

    if problems:
        print("agent rules (%d):" % len(problems))
        for p in problems:
            print("  " + p)
        return 1
    print("check-agent-rules: ok (%d rules, all in the index)" % len(rule_files))
    return 0


if __name__ == "__main__":
    sys.exit(main())
