# Core 1 is the screen's

**Rule.** LVGL runs alone on core 1; every task that waits on the network is pinned to core 0, and a new client task gets `task_core_id = 0`.

**Why.** Founder rule: a network wait on core 1 stalls the drawing.

**How.**
- The exception is the update task (`firmware/main/net/ota.c`), which runs on core 1 while the screen shows only the update ring.
