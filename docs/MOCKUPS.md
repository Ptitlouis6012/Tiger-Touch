# Pictures of the screen: captures, mockups, animations

How the pictures of Tiger Touch are made, so that a person or an AI agent can
make another one later - a new view, a view animated, the screen in its frame -
and get the same result. Everything below is in the repository; nothing needs
the device except a new capture.

## What there is

| Folder | What | Made by |
|---|---|---|
| [`assets/screens/`](../assets/screens/) | Captures of the real screen, 800 x 480, in Demo mode | the device (below) |
| [`assets/devices/`](../assets/devices/) | The Panda Touch alone, transparent PNGs (front, three-quarter, back, dock); the 45-degree drawings | BIGTREETECH's product pictures, cut out; the drawings by hand |
| [`assets/mockups/front/`](../assets/mockups/front/) | Each capture in the front picture | the method below, *Still mockups* |
| [`assets/mockups/angle/`](../assets/mockups/angle/) | Each capture, the start-up screen and the protocols' screens in the three-quarter photograph | [`scripts/make-mockups.py`](../scripts/make-mockups.py) `stills` |
| [`assets/mockups/animated/`](../assets/mockups/animated/) | Views that move (cards, list, update, and the tour: start-up, list, cards), the screen alone, in the front picture (`-front`) and in the three-quarter photograph (`-angle`), GIF and WebP | [`scripts/make-mockups.py`](../scripts/make-mockups.py) |
| [`assets/brand/`](../assets/brand/) | TigerSystem's square tiger head, framed and full | TigerTag-RFID-Guide's brand folder |

All of it is in the media kit, `press/` on the site, with one ZIP of the lot.

## Captures: the source of truth

A capture is the screen as the firmware draws it, never a drawing of it. They are
taken in **Demo mode** (*Settings > Demo mode*): ten made-up printers, no
account's machine, so no name, address or serial of a real printer ends up in a
picture. Read each capture before committing it anyway.

Two ways to take one:

- **Live view** (every build, [LIVE-VIEW.md](LIVE-VIEW.md)): `GET /screen.bmp?c=CODE`
  on the device's address, with the code shown in *Settings > Live view*.
- **A bench build** (port 81, unauthenticated, never committed): `local/bench/tt.py shot`.

## Animations: a capture with its values changed

An animation is a capture whose changing values are drawn again, frame by frame,
the way the firmware draws them. Nothing else on the capture is touched, so the
rest stays pixel for pixel what the device showed.

What makes it faithful:

1. **The same font.** LVGL's Montserrat Medium
   (`firmware/managed_components/lvgl__lvgl/scripts/generators/built_in_font/`,
   there after the first firmware build), at the size the screen uses:
   `font_ui_14/16/20/24` are 14, 16, 20, 24 px, the update's percentage is 48.
2. **The same geometry and colours,** read in `firmware/main/ui/`: `ui_theme.h` for
   the colours, `ui_cards.c` for a card's ring (96 px, 9 px wide, from 12 o'clock,
   round end) and a row's bar (150 x 6 px at x 268), `ui_settings.c` for the update
   ring (220 px centred at (400, 170), 14 px wide, track `#26456F`).
3. **Each label found on the capture, not guessed.** `Text(...)` in the script
   draws the label's current text at every position around a starting point and
   keeps the one that matches the capture best; the run prints the worst mismatch
   and stops above 16/255. A label then keeps its own alignment: left, right
   (the time left in a row) or centred (the clock).
4. **The same logic as the firmware.** Demo mode moves a print by 1 % every 20 s
   (`demo_mode.c`); the time left is `(100 - percent) x 20 s`, shown rounded up to
   the minute (`ui_fmt_left`); the estimated end is the clock plus that, so it
   stays still while the clock runs. At 100 % the ring or bar turns green, the
   word says Done, the time left goes (`--`, `--:--`), and pause and stop dim to
   40 % (`ui_job_set`). A paused print stands still.
5. **One palette for every frame of a GIF,** taken from the first, middle and last
   frames, without dithering: no flicker. The WebP keeps every colour and the
   transparency around the frame.

Each frame is one step of 20 s: a time-lapse of a few minutes in a few seconds.

```bash
python3 scripts/make-mockups.py            # cards, list and update
python3 scripts/make-mockups.py cards      # one of them
```

### Adding a view

1. Take the capture in Demo mode, put it in `assets/screens/`.
2. Find what draws the values in `firmware/main/ui/`: the widget, its font, its
   position, its colour, how its value is written.
3. Write a `view_<name>()` in `scripts/make-mockups.py`: one `Text` per label that
   changes (its text on the capture, its size and colour, a point near it), the
   shapes with `arc_ring()` or `bar()`, then a loop over time that draws each frame.
   Add it to `VIEWS`.
4. Run it. Check the reported mismatch, then look at the first frame beside the
   capture: they must be the same picture.

A view the device cannot show is not made this way: a mockup shows the product.

## The start-up screen

It is not a capture: the firmware keeps it as pixels, `firmware/main/ui/boot_splash.c`
(800 x 480, RGB565). `splash()` decodes that file, so the picture is exactly what
the screen shows at power-on.

## Three-quarter mockups: the screen in perspective

`in_angle()` puts an 800 x 480 screen on the glass of
`assets/devices/panda-touch-angle.png`, the product photograph cut out. The four
corners of the glass were measured on the picture (`ANGLE_GLASS`: top left
(247.5, 70), top right (1231.5, 89), bottom right (1119.5, 768), bottom left
(110.8, 655.8)); the screen, enlarged twice for sharpness, is warped onto them by
a perspective transform (a homography solved from the four pairs of corners), its
edge anti-aliased, and a faint diagonal reflection laid over it. Another
photograph needs its own four corners, measured the same way: a grid over the
picture, then each corner zoomed.

## Still mockups: a capture in the frame

`in_frame()` in the script puts an 800 x 480 screen in the glass of
`assets/devices/panda-touch-front.png`: the glass is the rectangle at x 38, y 27,
372 x 225 px on that picture (measured, a hair narrower than 5:3), the screen is
scaled to cover it and its sides trimmed evenly, then the picture is enlarged
2x (the still mockups use 3x). The other cut-out pictures in `assets/devices/`
have the BIGTREETECH interface on their glass and would need it replaced in
perspective.

## Rules that apply

- A picture shows what the firmware does today. A changed screen means a new
  capture and the pictures made from it again.
- No real account, printer name, address or serial in a picture: Demo mode.
- BIGTREETECH's pictures and the printer makers' logos are theirs; the media kit
  says so, [TRADEMARK.md](../TRADEMARK.md).
