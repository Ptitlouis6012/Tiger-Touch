# Live view keeps its access code

**Rule.** Every Live view request carries the access code shown in *Settings > Live view*; a remote touch is no stronger than a finger.

**Why.** The owner's decision of 2026-10-18.

**How.**
- The server is `firmware/main/net/lan_mode.c`; its interface is `docs/LIVE-VIEW.md`. No web page is served.
