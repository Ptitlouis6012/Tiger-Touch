# Worklog

Everything done since the last release, noted the moment it is done. Describe
the end state, not the journey; fold fixes into the entry they fix; let reverts
disappear. `CHANGELOG.md`'s `[Unreleased]` section is written from it, and it is
reset to this header at each release.

## Added
- Media: PandaTouch Reforged campaign illustration and an English Reddit launch draft, using the existing device and Demo mode interface references.
- Site: one page per printer brand (SEO), brand favicons, sitemap and robots.txt, logos linked; five new Demo captures.
- Site: the rest of TigerSystem (Studio, TigerPOD, TigerScale, TigerSpool) at the foot of every page.
- Site: a media kit page (/press/) with every picture and a ZIP; README section Media kit - Press.
- Site icon: the framed tiger head; animated views (cards, list, update) by scripts/make-mockups.py, method in docs/MOCKUPS.md.
- Installer: the three-quarter tour as its picture; animations split into screen/, front/, angle/.
- Anycubic cloud: pause, resume, stop and light as REST orders (sendOrder), signed with fields Tiger Studio writes; seen on a Kobra X (light, pause, resume, stop).
- Site: share buttons on every page (installer/share.js, assets/icons/share/).

## Changed
- Site: one stylesheet and theme script shared by every page.
- TigerSystem account, not TigerTag account: installer, README, docs, screen texts.
- Installer: the README's turning picture of the screen; the brand strip across the whole page.
- Site: every page in eleven languages (brand pages included): one lang.js, brand words in installer/brand-i18n/; one address per language (/fr/...) with hreflang and the sitemap.
- Brand pages: the TigerSquad card right under the top of the page, as on the installer.
- Elegoo (CC1, CC2), Prusa and Klipper seen on real printers: docs, drivers' headers, README, brand pages (11 languages); the K-Touch no longer called untested.
- Home, list view: the printers are in order of the estimated end of their print (the first to finish on top, those with no print last); the rows hold still for 3 s after a touch.

## Fixed
- Share pictures: the button reads "Install for free".
- Site accessibility: language menu keyboard and focus, contrasts (light orange, buttons, TigerSquad card), phone comparison as cards, balanced printer rows, 44 px targets, notice after the title on phones, reduced motion.
