# Size text buffers for the longest language

**Rule.** A buffer that holds translated text is sized for the longest language in bytes, not in letters.

**Why.** Founder rule: a Cyrillic letter is two bytes in UTF-8 ("Правое" is 12 bytes).

**How.**
- Check the longest value of the key in `firmware/main/i18n_table.h` across the eleven columns.
