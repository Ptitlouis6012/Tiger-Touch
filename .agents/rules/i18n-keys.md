# Translations: every key in every language

**Rule.** English is the reference language. Every user-facing text is a key, added in all eleven languages (en, fr, de, es, it, zh, pt, pt-pt, pl, ru, nl) in the same commit; no user-facing text is written into the code.

**Why.** Founder rule. A missing key falls back to English on the device or the site and nobody notices; a value written in the wrong column shows another language.

**How.**
Where the translations live:
- Device: `firmware/main/i18n_table.h`, one `X(id, en, fr, de, es, it, zh, pt, pt-pt, pl, ru, nl)` row per text. The table is positional: a value in the wrong place shifts a language.
- Site: `installer/i18n.js`, one dictionary per language, keyed by name.
- Brand pages: `installer/brand-pages.json` (English), `installer/brand-i18n/<lang>.json` (the ten others).

Adding or removing a key:
- Device: add or remove the whole row in `i18n_table.h`, with the eleven values in the order above; `S_<ID>` is used through `tr()`. Run `python firmware/tools/make_fonts.py --noto <NotoSansSC-Medium.otf>` when a value brings a character the font does not hold.
- Site: add the key to each of the eleven blocks of `i18n.js`, then use it with `data-i18n`, `data-i18n-html` (only `<b>` inside), `data-i18n-aria` or `data-i18n-alt`.
- Brand words: the same field in `brand-pages.json` and in every `brand-i18n/<lang>.json`.
- Use a script to add a key to every language at once; never edit eleven places by hand one by one.

Checks: `scripts/check-i18n.py` (the device table: count, empty values, conversions, script of the zh and ru columns, orphan ids as a warning) and `scripts/check-installer-i18n.py` (the site: same keys everywhere, markup, used keys, brand words). Both run in `scripts/verify.sh`.

Hard-coded text: an aria-label, a title, an alt or a placeholder is user-facing text too. Product and brand names are not translated.
