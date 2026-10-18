# Live view: the screen for Tiger Studio

*Settings > Live view* lets Tiger Studio Manager, on the same network, see the
screen live, touch it and take screenshots of it, as TigerScale V3 does. This is
how to get a screenshot for an issue report or a post. The device serves no web
page: Studio is the client.

## On the screen

*Settings > Live view* shows:
- **Live view**, on by default. Switched off, nothing is served at all.
- **Address**, the screen's address on the local network.
- **Access code**, six characters, with the ↻ button that draws a new one.

## The access code

- **What it is.** Six characters drawn on the device the first time it starts,
  capitals and digits with no look-alikes (no `I`, `O`, `0` or `1`): 32
  characters, 30 bits. Typed in lower case it is still accepted.
- **Where it is kept.** It stays across restarts and updates (setting
  `lan_code`).
- **A new code.** The ↻ button draws a new code; a client using the old one is
  refused from its next request.
- **In the account.** The code is written to the account, in the device's
  presence document `users/{uid}/tigertouchs/{mac}`, which only the owner can
  read. That is where Tiger Studio finds it ([TELEMETRY.md](TELEMETRY.md)):

| Field | Value |
|---|---|
| `lan_live` | whether live view is on |
| `lan_access_code` | the six characters |
| `lan_url` | `http://<address>` while live view is on and the device has an address, otherwise null |

The device writes these fields with every heartbeat, and at once when the
switch or the code changes.

## What is served

The server runs on port 80, only while **Live view** is on. Every request
carries the code in `c`.

| Request | Answer |
|---|---|
| `GET /screen.ver?c=CODE` | the frame counter: it moves each time the screen draws |
| `GET /screen.ver?c=CODE&wait=VER&ms=MAX` | the same, held until a frame after `VER` has **settled** or `MAX` ms have passed (5000 at most); at once when the counter already differs from `VER` |
| `GET /screen.raw?c=CODE&fmt=rle565[&scale=2][&since=VER]` | the screen's pixels, run-length coded (below): **the fast one** |
| `GET /screen.raw?c=CODE&fmt=rgb565[&scale=2][&since=VER]` | the same pixels uncompressed, 2 bytes each, little-endian |
| `GET /screen.bmp?c=CODE` | the whole screen, 800x480, 24-bit BMP (1,152,054 bytes): slow, kept for simple clients |
| `POST /api/tap?c=CODE&x=&y=[&wait=1]` | a touch at those coordinates (0-799, 0-479); 204 |
| `POST /api/swipe?c=CODE&x1=&y1=&x2=&y2=&ms=[&wait=1]` | a drag over `ms` milliseconds (60 to 2000); 204 |

**Settled** means the counter has moved past `VER` and no new frame has come
for 150 ms. That is the end of an animation, not its middle: a scroll's frames
came up to 100 ms apart on this board.

### `/screen.raw`

- **`scale=2`** halves the picture to 400x240, each pixel the mean of four.
- **`since=VER`** sends only the rectangle that changed after frame `VER` (the
  union of the areas LVGL redrew). The screen keeps the last 64 frames. Without
  `since`, or when `VER` is older than that (or newer than the screen, after a
  restart), the whole screen is sent.
- **Response headers:**
  - `X-Frame-Ver`: the frame the pixels show.
  - `X-Rect: x,y,w,h`: the rectangle, in 800x480 coordinates before scaling. On
    even coordinates with `scale=2`. `0,0,0,0` with an empty body means nothing
    changed.
  - `X-Scale`: `1` or `2`.
  - `X-Format`: `rle565` or `rgb565`.
- **The body** is the rectangle's pixels, row-major, `w/scale` by `h/scale`.

**`rle565`** is TigerScale V3's live-view code, so one decoder reads both. Read
a token byte `t`:

| `t` | Then |
|---|---|
| `0x00`-`0x7F` | `t+1` pixels follow, 2 bytes each |
| `0x80`-`0xFE` | one pixel follows, repeated `t-0x80+2` times |
| `0xFF` | a u16 count, then one pixel repeated that many times |

All values are little-endian. A pixel is RGB565: `r = p>>11`, `g = (p>>5)&63`,
`b = p&31`.

### `wait=1` on a touch

The answer comes once the screen has settled after the touch, 1.5 s at most,
with `X-Frame-Ver` set: the client fetches that frame at once instead of
polling.

### A live view, the fast way

1. `GET /screen.raw?fmt=rle565`: draw it, and keep `X-Frame-Ver` as `ver`.
2. `GET /screen.ver?wait=ver&ms=5000`.
3. `GET /screen.raw?fmt=rle565&since=ver`: draw the rectangle at `X-Rect`, and
   update `ver`.
4. Go back to step 2. After a touch with `wait=1`, go straight to step 3.

A held `/screen.ver` does not block the server: a touch sent meanwhile is
answered at once.

### How the code is checked

- **The comparison** takes the same time whatever the code: a wrong one cannot
  be found character by character.
- **A wrong code** gets 403.
- **After ten wrong codes in a row**, nothing is accepted for a minute and every
  request gets 429.
- **CORS:** every answer carries `Access-Control-Allow-Origin: *`, and
  `Access-Control-Expose-Headers` names the `X-` headers, so that Studio's page
  can call the screen and read them. The code is the gate, not the origin.
- **There is no web page:** `GET /` and `GET /<code>` answer 404.

## What a touch from Studio can do

It can do what a finger on the screen can do, nothing more. It presses the same
buttons, so stopping a print still opens the confirmation and needs its red
**Stop**. Anyone who has the code and is on the same network can do this; to
take the access away, draw a new code or switch **Live view** off.

## Its cost

- **Memory.** The server's task stack is 16 KB in PSRAM. A screenshot draws the
  screen on that task: the measured peak was 5.4 KB, and a 6 KB stack
  overflowed. The server keeps four connections at most. A picture is taken in
  PSRAM and sent line by line.
- **Speed**, measured from a Mac on the same Wi-Fi (2026-10-18, home screen with
  five printers):

| Request | Bytes | Time |
|---|---|---|
| `/screen.bmp` | 1,152,054 | 3.7 s |
| `/screen.raw?fmt=rgb565` | 768,000 | 2.3 s |
| `/screen.raw?fmt=rle565` | 32,107 | 0.22 s |
| `/screen.raw?fmt=rle565&scale=2` | 14,973 | 0.15 s |
| `/screen.raw?fmt=rle565&since=VER`, nothing changed | 0 | 0.16 s |
| `/screen.ver` | a few | 0.1 s |
| `POST /api/tap&wait=1` (a scroll of the list) | - | 0.69 s, the scroll finished |

  The link carries about 330 KB/s: the compression is what makes it fast.
