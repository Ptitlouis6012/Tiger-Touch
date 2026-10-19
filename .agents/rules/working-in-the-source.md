# Working in the source

**Rule.** Before touching a module, read its row and its landmines in `CODEMAP.md`; grep for the symbol, read that range and make the smallest edit that does the job.

**Why.** Founder rule.

**How.**
- C: 4-space indent, braces on their own line.
- A constant that looks arbitrary carries a comment saying what breaks without it.
- ESP-IDF 5.5, native `idf.py`, not PlatformIO (settled: the wrapper is where the toolchain conflicts came from).
