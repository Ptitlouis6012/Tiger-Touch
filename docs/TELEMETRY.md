# Telemetry contract

What a Tiger Touch reports about itself, where, how often, and what each value
means. This is the integration reference for Tiger Studio Manager, for a
dashboard, or for anything that reads a Tiger Touch's state. When the firmware
changes a field, this file changes in the same commit.

The shape is the TigerScale's and the TigerSpool's, on purpose: a device list,
an online dot and a battery icon should not need to know which product they are
looking at. Identity, liveness and power fields carry the same names as a
TigerScale's
([TELEMETRY.md](https://github.com/TigerTag-Project/Tiger-Scale-V3/blob/main/docs/TELEMETRY.md)).
Only the state block differs, because a screen that follows printers does
different work from a scale.

## Two channels, different audiences

| Channel | Who reads it | Transport |
|---|---|---|
| Firestore heartbeat | Tiger Studio Manager, any cloud dashboard | `documents:commit` to `users/{uid}/tigertouchs/{mac}` |
| Live view | Tiger Studio on the same network | `http://<ip>/...?c=CODE`, behind the access code ([LIVE-VIEW.md](LIVE-VIEW.md)) |

Firestore carries what changes slowly and must be reachable from anywhere:
identity, signal, power, the printers followed, and how to reach the live view.
The live view carries the screen itself: its picture and touches.

## Where it writes

```
users/{uid}/tigertouchs/{mac}
```

- **`{mac}`** is the Wi-Fi MAC: 12 lowercase hex digits with no separators. The
  format is frozen, because it is the document id and changing it would orphan
  every device already registered.
- **Ownership.** The device writes under its owner's account, as the owner. No
  device allow-list or service account is involved.
- **The Firestore rule** that has to exist:

```
match /users/{uid}/tigertouchs/{deviceId} {
  allow read, write: if request.auth != null && request.auth.uid == uid;
}
```

Without it every heartbeat comes back with `403` and the device stays invisible.
Nothing else on the device changes.

## What it writes

One `documents:commit` per heartbeat, with an `updateMask`. The first heartbeat
creates the document. Fields outside the mask are left alone, and Studio owns
`display_name`.

**Identity: on a full heartbeat only.** A full heartbeat is sent at start and
after a sign-in.

| Field | Type | Value |
|---|---|---|
| `mac` | string | the document id |
| `fw_version` | string | the running version, e.g. `2.1.0` |
| `mdns_hostname` | string | `tigertouch-xxxx` (the last four hex digits of the MAC) |
| `display_name` | string | `TigerTouch-XXXX`, written only when the document has none; a name given in Studio is kept |
| `hardware_revision` | null | always null on this board |

**Liveness, power and signal: on every heartbeat.**

| Field | Type | Value |
|---|---|---|
| `last_heartbeat_at` | timestamp | **set by the server** (`REQUEST_TIME`), never the device's clock |
| `wifi_signal_dbm` | int or null | RSSI in dBm; null when it is not known |
| `ip_address` | string | the address on the local network |
| `power_state` | string | `"active"`, or `"screen_off"` while the screen sleeps (Wi-Fi still on) |
| `battery_present` | bool | what the owner declared in *Settings > Battery* (yes by default: the board ships with one). This is not a measurement. |
| `battery_percent`, `is_charging`, `power_source` | null | always null: the board gives the firmware no way to read them |

**State: on every heartbeat.**

| Field | Type | Value |
|---|---|---|
| `printers_active` | int | how many of the account's printers are switched on on this screen |
| `printer_ids` | array of strings | those printers' document ids in the account |
| `printers_online` | array of strings | the subset of `printer_ids` the screen reaches right now (its green dot) |
| `demo` | bool | `true` while *Settings > Demo mode* is on. The screen then shows ten made-up printers and `printer_ids` is empty. |

**Live view: on every heartbeat.**

| Field | Type | Value |
|---|---|---|
| `lan_live` | bool | live view is on (the default) |
| `lan_access_code` | string | six characters, capitals and digits without `I`, `O`, `0` or `1`, e.g. `K7QX2M` |
| `lan_url` | string or null | `http://<ip>`, the base of the requests below, while live view is on and the screen has an address; null otherwise |

### Rules that matter to a reader

- **Absent is not null.** Identity fields come only in a full heartbeat. A
  field missing from a later write means "unchanged". Only an explicit
  `nullValue` means null.
- **Nulls are written explicitly.** `battery_percent` is always written as
  null, never left out, so a stale value can never survive.
- **The full flag is cleared only on success.** A failed full heartbeat is
  retried as a full one.
- **`lan_access_code` is a secret.** Only the owner can read the document.
  Never copy it anywhere another user can read, such as a shared or public
  inventory, a friend's view or a log.

## When it beats

| | |
|---|---|
| Screen on | every 30 s |
| Screen asleep | every 5 min |
| The printers switched on change, one starts or stops answering, the screen sleeps or wakes, the battery setting changes, live view is switched, a new code is drawn | at once (within 5 s) |
| Three failures in a row | the next try waits 5 min |

**Online or offline.**
- **Decided from the server's time:** `now - last_heartbeat_at`. Allow about
  90 s while `power_state` is `"active"`, and about 11 min while it is
  `"screen_off"`.
- **A sleeping screen is not offline.** In `"screen_off"` it is awake, on Wi-Fi,
  and still following its printers; only its backlight is off.

A heartbeat is skipped while the internal RAM has no 24 KB block for a TLS
session. The device then catches up on the next one.

## Live view: the screen itself

With `lan_live` true and `lan_url` set, the screen answers on the local network.
There is no web page (`GET /` and `GET /<code>` answer 404); every request
carries the code in `c`. The full reference, with the `rle565` format and the
fast live-view loop, is [LIVE-VIEW.md](LIVE-VIEW.md#what-is-served).

| Request | Answer |
|---|---|
| `GET {lan_url}/screen.ver?c=CODE[&wait=VER&ms=MAX]` | the frame counter; with `wait`, held until a frame after `VER` has settled (5 s at most) |
| `GET {lan_url}/screen.raw?c=CODE&fmt=rle565[&scale=2][&since=VER]` | the pixels, run-length coded; only what changed since `VER` |
| `GET {lan_url}/screen.bmp?c=CODE` | the whole screen as a BMP (slow) |
| `POST {lan_url}/api/tap?c=CODE&x=&y=[&wait=1]` | a touch; 204, `X-Frame-Ver` |
| `POST {lan_url}/api/swipe?c=CODE&x1=&y1=&x2=&y2=&ms=[&wait=1]` | a drag; 204, `X-Frame-Ver` |

- **Wrong code:** a wrong code gets `403`.
- **Lockout:** after ten wrong codes in a row, everything gets `429` for a
  minute.
- **Case:** the code is compared without regard to case.
- **CORS:** sent. Every answer carries `Access-Control-Allow-Origin: *`, and
  `Access-Control-Expose-Headers` lists `X-Frame-Ver`, `X-Rect`, `X-Scale` and
  `X-Format`.

## Brief for the Studio Manager agent

Self-contained; paste it as is.

> You are adding **Tiger Touch** to Tiger Studio Manager, next to TigerScale
> and TigerSpool. A Tiger Touch is a BIGTREETECH Panda Touch / K-Touch screen
> (800x480) running the Tiger Touch firmware: it follows the account's 3D
> printers. Two things to build: the device in the devices list, and a
> **screenshot / live view** of its screen.
>
> **1. The document.** `users/{uid}/tigertouchs/{mac}` (collection spelled
> `tigertouchs`), one per screen, written by the device as its owner. List the
> collection for the owner's screens. Fields:
>
> - **Identity**, written only at start and after a sign-in: `mac` (= the
>   document id), `fw_version`, `mdns_hostname` (`tigertouch-xxxx`),
>   `display_name` (default `TigerTouch-XXXX`; Studio may rename it, and the
>   device never overwrites an existing name), `hardware_revision` (null).
> - **Every heartbeat:**
>   - `last_heartbeat_at`, a server timestamp;
>   - `wifi_signal_dbm` (int or null) and `ip_address`;
>   - `power_state`: `"active"` or `"screen_off"`;
>   - `battery_present` (bool, the owner's declaration, not a measurement);
>     `battery_percent`, `is_charging` and `power_source` are always null on
>     this board;
>   - `printers_active` (int), `printer_ids` (array of the account's printer
>     doc ids) and `printers_online` (the subset the screen reaches now);
>   - `demo` (bool: Demo mode, made-up printers, `printer_ids` empty);
>   - `lan_live` (bool), `lan_access_code` (6 chars, capitals and digits) and
>     `lan_url` (`http://<ip>`, or null when live view is off or there is no
>     address).
> - **Absent is not null:** keep the last value of a field missing from an
>   update; only an explicit null means null.
>
> **2. Online.** The device beats every 30 s when `power_state` is `"active"`,
> every 5 min when `"screen_off"`, and at once when something changes. Mark it
> offline past about 90 s (active) or about 11 min (screen_off), measured
> against the server time. `"screen_off"` is asleep, not offline: draw it the
> way TigerScale's standby is drawn (blue). Signal bars:
> `Math.floor((Math.min(-40, Math.max(-100, rssi)) + 100) * 5 / 60)` (0-5), as
> for TigerScale. Show no battery level: the board cannot measure it.
>
> **3. Printers.** Next to each printer whose id is in `printer_ids`, show the
> Tiger Touch with a green dot if the id is in `printers_online`, and grey if
> not. Do not derive that dot from Studio's own connection to the printer. When
> `demo` is true, say "Demo mode" and list no printers.
>
> **4. The screen (Live view).** Available when `lan_live` is true and `lan_url`
> is a string. The screen must be on the same network as this computer.
>
> - **Requests.** Every request is
>   `{lan_url}<path>?c=<lan_access_code>&...`. CORS is sent
>   (`Access-Control-Allow-Origin: *`, and the `X-` headers are exposed), so
>   the renderer may `fetch` directly. There is no web page: `/` answers 404.
> - **Screenshot**, the button users want for issue reports and posts:
>   `GET /screen.raw?fmt=rle565`. The answer is the whole screen, about
>   30 KB, in about 0.2 s. Decode it (below) into an 800x480 RGBA image and
>   offer Save as PNG (`tiger-touch-YYYYMMDD-HHMMSS.png`) and Copy to
>   clipboard.
> - **Live view**, in an 800x480 canvas scaled to fit:
>   1. `GET /screen.raw?fmt=rle565`, then draw it; `ver = X-Frame-Ver`.
>   2. `GET /screen.ver?wait=<ver>&ms=5000`. It is held until the screen has
>      changed and settled, so there is no polling interval to tune.
>   3. `GET /screen.raw?fmt=rle565&since=<ver>`. Draw the body into the
>      rectangle `X-Rect: x,y,w,h` (800x480 coordinates); `ver = X-Frame-Ver`.
>      `X-Rect: 0,0,0,0` with an empty body means nothing changed.
>   4. Back to step 2. Run one loop per open view and stop it when the view
>      closes.
>   - For a small thumbnail, add `&scale=2`: 400x240, and `X-Rect` stays in
>     800x480 coordinates, so divide it by 2.
> - **Touch.** A click on the canvas sends
>   `POST /api/tap?x=<0-799>&y=<0-479>&wait=1`, in screen pixels scaled back
>   from the canvas. A drag sends
>   `POST /api/swipe?x1=&y1=&x2=&y2=&ms=<drag duration, 60-2000>&wait=1`. The
>   answer (204) comes once the screen has settled, with `X-Frame-Ver`: fetch
>   `/screen.raw?fmt=rle565&since=<previous ver>` at once. A touch is a finger
>   on the screen, nothing more, so stopping a print still asks for a
>   confirmation on the screen.
> - **Answers.**
>   - `403`: wrong code. Re-read the document (the code may have been changed
>     on the screen) and retry once.
>   - `429`: too many wrong codes. Wait 60 s.
>   - `503`: no memory for a picture now. Retry after 1 s.
>   - A network error: say "The screen is not reachable from this computer
>     (same Wi-Fi?)". Back off; do not retry in a tight loop.
> - **Connections.** Use at most 2 parallel connections: the screen keeps 4.
> - **`rle565` decoding.** All values are little-endian, and a pixel is RGB565.
>   Read a token byte `t`:
>   - `t <= 0x7F`: `t+1` literal pixels follow (2 bytes each);
>   - `0x80 <= t <= 0xFE`: one pixel follows, repeated `t-0x80+2` times;
>   - `t == 0xFF`: a u16 count, then one pixel repeated count times.
>
>   Fill the rectangle row-major. To convert a pixel `p` to RGB888:
>   `r=(p>>11)<<3|(p>>13)`, `g=((p>>5)&63)<<2|((p>>9)&3)`,
>   `b=(p&31)<<3|((p>>2)&7)`. It is TigerScale V3's live-view code: reuse that
>   decoder if Studio has one.
> - **The code.** Show `lan_access_code` in the device's details, monospace,
>   with a copy button. It is changed only on the screen (*Settings > LAN
>   mode*); offer no "regenerate" button. When `lan_live` is false, show "Live
>   view is off on the screen (Settings > Live view)".
>
> **5. Secrets.** `lan_access_code` controls the screen for anyone on that
> network. Never put it, or a URL carrying it, in anything shared: friends'
> views, public inventory, exports, logs, MCP output, analytics.

## Where this lives in the firmware

| What | Where |
|---|---|
| The heartbeat's payload | `cloud_heartbeat()`, `firmware/main/net/cloud.c` |
| What goes into it, and when | `presence_task()` and `collect_presence()`, `firmware/main/main.c` |
| The access code, live view, the server | `firmware/main/net/lan_mode.c` |
