# One driver per way of reaching a printer

**Rule.** A driver fills `printer_view_t` and nothing else, so no screen learns which brand is behind it; a new brand is one driver, its line in `drivers.c` and `printer_driver.h`, and its measured price in `links.c`.

**Why.** Founder rule: screens never decide anything about a brand.

**How.**
- Every LAN address is built through `firmware/main/net/addr.h`.
- `docs/PRINTER-COMPATIBILITY.md` records how each driver reaches its printer and what it reads.
