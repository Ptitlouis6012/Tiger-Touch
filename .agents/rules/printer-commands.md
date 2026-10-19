# What a printer may be told

**Rule.** A printer is told only to pause, resume or stop a print, or to switch its light, and only on a touch. Stop opens a confirmation and only its red Stop sends the command. Anything else - heating, moving, starting a print - is a new decision of the owner.

**Why.** Founder rule; the stop confirmation replaced a 1.5 s hold (the owner, 2026-10-17). A stray touch ends a print that may be hours in, and a printer that refuses a command never says so.

**How.**
- The pause, stop and light buttons are always drawn, on every card, row and full view; they are dimmed and inert when they cannot act (`ui_job_set`, `ui_job_set_light` in `firmware/main/ui/ui_job.c`), from the view's `can_control` and `can_light`.
- A Bambu reached through the cloud takes no pause or stop; its light goes through the cloud broker.
- An Anycubic reached through the cloud takes its commands as REST orders (`sendOrder`), never as a publish on its shared MQTT session, which the broker closes on a publish it does not allow (the owner, 2026-10-18).
- Prusa, OctoPrint and the Elegoo Centauri Carbon 1 have no light command.
- A touch sent through Live view is a touch on the screen: the same buttons, the same confirmation.
