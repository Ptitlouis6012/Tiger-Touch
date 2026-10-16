# Authors and credits

## Maintainer

**Benoit Michaut** - [TigerTag Project](https://github.com/TigerTag-Project)

## Where it comes from

Tiger Touch was written by **[Ptitlouis6012](https://github.com/Ptitlouis6012)**,
in the repository this one continues: the drivers for every brand, the load
budget, the cards and the full view, the eleven languages, the time zone from a
country, and the guards that keep the tree clean. That history (1.0.0 to
1.3.2) stays in his repository; this one starts from a single first commit,
which credits him as its co-author.

It is a new firmware for the screen BIGTREETECH sells as the **Panda Touch** and
the **K-Touch** (one board, each name with a BIGTREETECH firmware of its own),
written to replace that firmware. BigTreeTech's board component
(PandaTouch_IDF, MIT) drives the panel
and the touch; it is vendored in `firmware/components/PandaTouch_IDF/`, with a
patch for the 180-degree rotation and for drawing straight into the panel's
frame buffer. Tiger Touch is not made or endorsed by
BigTreeTech.

## Ecosystem

- **[Tiger Studio Manager](https://github.com/TigerTag-Project/TigerTag-Studio-Manager)**
  - where the account and the printers live, and the protocol notes every driver
  here was written from.
- **[TigerSpool RFID](https://github.com/TigerTag-Project/TigerSpool-RFID)** and
  **[TigerScale V3](https://github.com/TigerTag-Project/Tiger-Scale-V3)** - the
  load-budget model, the over-the-air update and the release automation follow
  theirs.

## Third-party

Libraries and their licenses: [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
Protocol notes by the wider community are credited next to each driver.

---

Contributed something? Open a pull request adding yourself.
