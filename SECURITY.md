# Security

## Reporting

Open a [private security advisory](https://github.com/TigerTag-Project/Tiger-Touch/security/advisories/new),
or contact the maintainer directly. Please do not open a public issue for a vulnerability.

## What the device stores

All of it is in the NVS partition of the board's flash, which is **not
encrypted**; anyone with physical access to the board can read it.

| Stored | Why |
|---|---|
| Wi-Fi network name and password | to reconnect at power-on |
| The TigerTag account's refresh token and its e-mail | to sign in again without asking; the **password is never stored** |
| The language, the screen settings, which printers are on | preferences |
| The country the account holds (two letters, or a Canadian province) | to choose the time zone before the account is read again |
| The live view's access code, and whether it is on | to give the screen to Tiger Studio ([docs/LIVE-VIEW.md](docs/LIVE-VIEW.md)) |

Printer access details (access codes, addresses) are not stored on the device:
they are read from the account at each start.

## What the device sends, and where

- To Google's identity and Firestore endpoints, over verified TLS: the sign-in and
  the account's printer list, language and country. Written back: the
  language, when it is picked on the device (`users/{uid}/prefs/app.lang`), and
  the device's presence (`users/{uid}/tigertouchs/{mac}`), which includes the
  live view's access code; only the owner reads it
  ([docs/ACCOUNT-PAIRING.md](docs/ACCOUNT-PAIRING.md#the-devices-presence)).
- To TigerTag's Cloud Functions (`pairStart`, `pairPoll`), over verified TLS,
  when the owner chooses **Continue with Google**: the device's name
  (`tigertouch-xxxx`, from its Wi-Fi address), "Tiger Touch", its firmware
  version. The answer is a one-time code; the session it ends in is the same as
  a password sign-in's ([docs/ACCOUNT-PAIRING.md](docs/ACCOUNT-PAIRING.md)).
- To each printer, by the protocol its maker uses: status requests, and a pause,
  resume or stop only on a touch (stop asks for a confirmation first). A Bambu printer in LAN mode
  presents a self-signed certificate; the connection is made to a private address
  with the printer's access code, and the chain is not verified for that
  connection only. The same applies to an Anycubic printer's broker.

- To GitHub, over verified TLS: a read of the published update manifest a minute
  after start-up and once a day, and the firmware image when someone touches
  **Install**. Nothing identifying the device is sent; it is a plain download.

## What the device serves

While *Settings > Live view* is on (the default), a server on port
80 gives the screen's picture to Tiger Studio on the same network and takes
touches from it. Every request carries the six-character access code; after
ten wrong codes nothing is accepted for a minute. The connection is plain HTTP:
someone who can watch the network's traffic can read the code. Anyone with the
code can do what a finger on the screen can, so a stop still asks for its
confirmation. To take the access away, draw a new code or switch live view off
([docs/LIVE-VIEW.md](docs/LIVE-VIEW.md)).

## Updates

An image is installed only when its SHA-256 equals the manifest's, its
application name is this firmware's and its version the one announced; a new
version that does not survive its first minute is rolled back by the
bootloader. **Images are not signed**: whoever can publish a release of this
repository can ship firmware to every unit. Details and the open work in
[docs/OTA.md](docs/OTA.md).

## The serial log

The firmware's own log lines say what happened and to which kind of printer, never
whose it was (`scripts/check-log-privacy.py` enforces it). The ESP-IDF libraries
underneath are not ours: their error lines can print the address they were trying
to reach, and the Wi-Fi driver the name of the network. **Read a serial dump before
pasting it into a public issue**, and remove those.

## What the repository must not contain

Credentials, tokens, Wi-Fi names, addresses, serial numbers or personal paths.
`scripts/check-private-data.py` checks the patterns it can see on every commit.
