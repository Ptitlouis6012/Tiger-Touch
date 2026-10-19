# Keep the patched board component

**Rule.** `firmware/components/PandaTouch_IDF/` is vendored and patched; never replace it with another copy.

**Why.** Founder rule: the patch turns the picture and the touch coordinates by 180 degrees at run time (*Settings > Screen*, orientation); another copy silently loses it.

**How.**
- Its upstream text is left as it came; `scripts/check-docs.py` skips it.
