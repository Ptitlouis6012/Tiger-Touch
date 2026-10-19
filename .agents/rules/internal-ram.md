# Internal RAM is the limit

**Rule.** Large static data goes to PSRAM with `EXT_RAM_BSS_ATTR`, task stacks are created in PSRAM (`xTaskCreatePinnedToCoreWithCaps`), and the return value of every `xTaskCreate*` is checked.

**Why.** Founder rule, from the measurements in `docs/CONNECTION-BUDGET.md`: about 200 KB of internal RAM are free with no printer open, about 88 KB with ten (2026-10-15).

**How.**
- Exceptions that keep an internal stack: the update task (it writes the flash), and the clients whose stacks belong to their ESP-IDF components (MQTT, WebSocket).
- Every connection costs internal RAM; a new brand is measured before its price goes into `firmware/main/drivers/links.c` (`docs/CONNECTION-BUDGET.md`).
