# Roadmap

What is deliberately not done yet, why, and what has to be decided before it can be.
An item leaves this file by being built or by being ruled out - never by rotting.

## Decided for the next updates

1. **Anycubic cloud commands and temperatures.** The cloud session is built
   and seen working idle (2.0.0); pause, resume, stop, the light and the
   temperature query are left out because the broker closes the session on a
   publish it does not allow. Each needs trying alone on the bench
   (`docs/PRINTER-COMPATIBILITY.md`, *Anycubic through the cloud*).

Over-the-air update, the other one, is built (2.0.0, `docs/OTA.md`).

## Before and around the first public release

- **Installing from the BigTreeTech firmware's own update page.** The stock
  firmware has a web page (*Update FW*) that installs a `.bin` from any browser,
  with no driver, no cable and no computer of a particular kind. A small
  transition image in that format, which rewrites the partition table to Tiger
  Touch's on its first start, would make the first install as simple as an
  update. The most promising route to a beginner-proof install; to try on a
  board, with its backup at hand.

- **The board component's license.** BigTreeTech's `PandaTouch_IDF` says "MIT
  (assumed)" and neither it nor its upstream repository ships a `LICENSE`.
  Asking BigTreeTech to state it is the clean fix; until then
  `THIRD_PARTY_LICENSES.md` says what is known. This project itself is MIT.
- **Read the whole history again for private data,** with `.private-terms` filled
  in, just before the repository is opened: every file of every commit, deleted
  ones included, and every commit identity.
- **Signed updates.** The update is verified (TLS, SHA-256, name, version,
  rollback) but not signed: whoever can publish a release can ship firmware to
  every unit. Worth doing only with a key that never touches CI (`docs/OTA.md`).

## Drivers

- **Run each "compiled" driver on a real machine** and record its load price
  (`docs/CONNECTION-BUDGET.md`). Order of likelihood: Snapmaker, Elegoo, FlashForge.
- **A Centauri Carbon 1 session**, end to end: discovery, the status mapping, the
  meaning of `PrintInfo.Status` values on current firmware.
- **Multi-nozzle printers** (`docs/PRINTER-COMPATIBILITY.md`): the left/right order of
  a dual-head Bambu is read from the protocol notes, not from a machine.
- **The H2C's nozzle rack.** Needs a real report to know what to show.

## Platform


## Code

- **The poll tasks of FlashForge and Prusa** repeat the same start, stop and
  retry skeleton. One shared poller would also make a third HTTP brand cheap.
- **`cloud.c` and `http_lite.c` each accumulate an HTTP response**, for different
  reasons (a Firestore list can be large; a printer's answer is small). Worth
  looking at again if a third client appears.
- **Measure the "estimated" load prices.**
