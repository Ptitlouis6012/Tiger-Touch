# PandaTouch Reforged - Reddit launch draft

An English creator post for the Bambu Lab community. Draft only, not published.

## Title

PandaTouch Reforged: we gave that screen in your drawer a second life. Free, open-source firmware.

## Post

Anyone else have a Panda Touch in the "I'll use this again someday" box?

There's still a perfectly good touchscreen in there. So we wrote it a new future.

**Meet TigerTouch. Same screen. New fire.**

We're the TigerSquad, the maker community behind TigerSystem. We wrote a new firmware from scratch for the BIGTREETECH Panda Touch: a standalone printer dashboard built around your workshop.

For a Bambu setup, that means:

- Print progress, time left, temperatures and AMS filament information.
- Bambu cloud or LAN connections, depending on your setup.
- Cards when you want the details. A list when you want the whole workshop at a glance.
- Updates over Wi-Fi, eleven languages, and support for other printer families if your workshop grows.

**Same Panda Touch. No new screen to buy. Free firmware. MIT licensed. No subscription.**

Setup uses a free TigerSystem account and the Tiger Studio Manager desktop app to add your printers. Then flash the screen from Chrome or Edge. After setup, no computer or Raspberry Pi needs to stay running as a bridge.

Compatibility and available controls vary by printer and connection. The guide spells out what has actually been tested.

**[See TigerTouch for Bambu Lab and install it](https://tigertag-project.github.io/Tiger-Touch/bambu-lab/)**

**[Source code](https://github.com/TigerTag-Project/Tiger-Touch)**

If yours is boxed up: what would make you plug it back in?

## Optional first comment

A little more detail for anyone thinking of trying it:

- This replaces the screen's firmware, not the printer's firmware. No soldering or hardware modification is needed.
- Bambu cloud monitoring has been tested during a print on a P2S. Bambu LAN monitoring has been checked on an X1 Carbon and an A1 at idle; that is not a claim that every model or every control has been proven on a real printer.
- Pause, resume and stop are unavailable through the Bambu cloud connection. Their LAN implementation still needs real-machine validation. The light command has been tested through the Bambu cloud on a P2S.
- The launch illustration uses a forge as a metaphor. The actual interface shown comes from Demo mode, so the printer data in the images is made up.
- The original BIGTREETECH firmware can be reinstalled; follow the [installation and restoration guide](https://github.com/TigerTag-Project/Tiger-Touch/blob/main/docs/INSTALL.md).

We're sharing our own community project. It is independent of, and not endorsed by, Bambu Lab or BIGTREETECH.

## Images to attach

1. [PandaTouch Reforged cover](../../assets/social/pandatouch-reforged.png) - the visual hook: from a dusty box to a screen running TigerTouch. Concept illustration, not an installation procedure.
2. [Actual Bambu X1 Carbon interface in the device frame](../../assets/mockups/front/panda-touch-front-printer-bambu-x1c.png) - lets people inspect the interface without the illustrated workshop.
3. [Cards view animation](../../assets/mockups/animated/cards-front.gif) - optional follow-up showing the existing Demo mode interface. A time-lapse with made-up printer data, not a recording of live printers.

Suggested cover alternative text: "PandaTouch Reforged: an unused black Panda Touch in a cardboard box, a symbolic circuit board on an anvil, and the same device running TigerTouch with a Bambu X1 Carbon dashboard. Free, open-source firmware by TigerSquad at TigerSystem. Screen shown in Demo mode."

## Editorial choices

- Lead with bringing useful hardware back into service, a concrete maker story.
- Show the existing device and its real interface; the product is replacement firmware, not a new screen.
- Keep the benefit specific instead of asserting that the original product is useless for everybody or that every Bambu printer is supported and tested.
- Invite product feedback without requesting votes or pretending to be an unaffiliated customer.

## Facts and artwork sources

- [Project description and setup](../../README.md)
- [Verified printer status and controls](../PRINTER-COMPATIBILITY.md)
- [Capture and mockup method](../MOCKUPS.md)
- [Artwork brief](pandatouch-reforged-artwork.md)
- [Trademarks](../../TRADEMARK.md)

Printer names, product pictures and trademarks belong to their respective owners.
