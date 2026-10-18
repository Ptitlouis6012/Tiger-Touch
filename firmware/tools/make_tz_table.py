#!/usr/bin/env python3
"""Generate firmware/main/net/tz_table.h: country (or Canadian province) -> POSIX
time zone rule, from the IANA time zone database.

    python firmware/tools/make_tz_table.py --zoneinfo /usr/share/zoneinfo

The device learns a country code from the TigerSystem account or from the access
point, and needs the rule that turns UTC into local time. The rules are not
written by hand: each one is the footer of the compiled zone file of the zone
chosen for the country, so a change of law reaches the device by regenerating
this file from a newer database.

A country with several zones is represented by the zone of its capital or its
largest city (PRIMARY below). The script refuses to run when a multi-zone
country is not listed there, so new data cannot silently pick one at random.

Every emitted rule is then checked against the zone file itself: the offset it
gives is compared with the database's every half hour from the day the script
runs for three years, for every country. A table that does not reproduce the
database is not written. (The check starts today because a rule is the database's
description of what follows its last explicit change: before that it says nothing.)

Two things cannot be carried over, and both are listed in APPROXIMATE with what
is lost; nothing else is excused:

  - a daylight period that follows the lunar calendar: Morocco and Western Sahara
    move back an hour for Ramadan, which no POSIX rule can say;
  - a negative transition hour (`M3.5.0/-1`, Greenland): the C library of the
    device does not read it (measured on the board: the whole year came out wrong),
    so it is written as `/0`, which moves that one transition one hour later.

The first of these was found by running every rule on the board and comparing; see
the note in docs/ARCHITECTURE.md.

Names of zones (`<-03>`, `CET`) are replaced by fixed ones: the device shows
a time, never a zone name, and plain names are the form every C library parses.
"""
import argparse
import calendar
import collections
import datetime
import functools
import os
import re
import struct
import sys
import zoneinfo

# One zone for a country that has several: where most of its people live, or its
# capital. This is a choice, and the only one in the file.
PRIMARY = {
    "AQ": "Antarctica/McMurdo", "AR": "America/Argentina/Buenos_Aires", "AU": "Australia/Sydney",
    "BR": "America/Sao_Paulo", "CA": "America/Toronto", "CD": "Africa/Kinshasa", "CL": "America/Santiago",
    "CN": "Asia/Shanghai", "CY": "Asia/Nicosia", "DE": "Europe/Berlin", "EC": "America/Guayaquil",
    "ES": "Europe/Madrid", "FM": "Pacific/Pohnpei", "GL": "America/Nuuk", "ID": "Asia/Jakarta",
    "KI": "Pacific/Tarawa", "KZ": "Asia/Almaty", "MH": "Pacific/Majuro", "MN": "Asia/Ulaanbaatar",
    "MX": "America/Mexico_City", "MY": "Asia/Kuala_Lumpur", "NZ": "Pacific/Auckland",
    "PF": "Pacific/Tahiti", "PG": "Pacific/Port_Moresby", "PS": "Asia/Hebron", "PT": "Europe/Lisbon",
    "RU": "Europe/Moscow", "UA": "Europe/Kyiv", "UM": "Pacific/Midway", "US": "America/New_York",
    "UZ": "Asia/Tashkent",
}

# Zones whose database offset a POSIX rule cannot reproduce at all times (see above).
APPROXIMATE = {
    "MA": "UTC+1 all year, UTC+0 during Ramadan",
    "EH": "UTC+1 all year, UTC+0 during Ramadan",
    "GL": "daylight time begins one hour late (a negative transition hour is not readable)",
}

# The Studio account stores a Canadian province as CA-XX (VAT is per province),
# which names a finer zone than the country does.
PROVINCES = {
    "CA-AB": "America/Edmonton", "CA-BC": "America/Vancouver", "CA-MB": "America/Winnipeg",
    "CA-NB": "America/Moncton", "CA-NL": "America/St_Johns", "CA-NS": "America/Halifax",
    "CA-NT": "America/Yellowknife", "CA-NU": "America/Iqaluit", "CA-ON": "America/Toronto",
    "CA-PE": "America/Halifax", "CA-QC": "America/Toronto", "CA-SK": "America/Regina",
    "CA-YT": "America/Whitehorse",
}


def footer(path):
    """The POSIX rule at the end of a TZif (v2+) file."""
    data = open(path, "rb").read()
    if data[:4] != b"TZif":
        raise SystemExit("not a TZif file: " + path)
    end = data.rstrip(b"\n").rfind(b"\n")
    rule = data[end + 1:].strip().decode("ascii")
    if data[-1:] != b"\n" or not rule:
        raise SystemExit("no POSIX footer in " + path + " (a 'slim' file is needed, v2 or later)")
    return rule


NAME = r"(?:<[^>]+>|[A-Za-z]{3,})"
OFF = r"[-+]?\d{1,2}(?::\d{2}(?::\d{2})?)?"
RULE = re.compile(r"^(%s)(%s)(?:(%s)(%s)?(?:,([^,]+),([^,]+))?)?$" % (NAME, OFF, NAME, OFF))


def canonical(rule):
    m = RULE.match(rule)
    if not m:
        raise SystemExit("cannot parse rule: " + rule)
    std, soff, dst, doff, start, end = m.groups()
    out = "STD" + soff
    if dst:
        out += "DST" + (doff or "")
        if start and end:
            # The device's C library cannot read a negative transition hour.
            start = re.sub(r"/-\d+(?::\d+)*$", "/0", start)
            end = re.sub(r"/-\d+(?::\d+)*$", "/0", end)
            out += "," + start + "," + end
    return out


def hms(s):
    sign = -1 if s.startswith("-") else 1
    parts = [int(x) for x in s.lstrip("+-").split(":")]
    parts += [0] * (3 - len(parts))
    return sign * (parts[0] * 3600 + parts[1] * 60 + parts[2])


def day_of(year, month, week, wday):
    """POSIX Mm.w.d: day of the month of the w-th (5 = last) d (0 = Sunday) of the month."""
    first = datetime.date(year, month, 1)
    shift = (wday - (first.isoweekday() % 7)) % 7
    day = 1 + shift + (week - 1) * 7
    last = calendar.monthrange(year, month)[1]
    while day > last:
        day -= 7
    return day


def at(rule_part, year):
    """Seconds since the epoch, as if the local clock were UTC, of a rule's date and time."""
    when, _, t = rule_part.partition("/")
    secs = hms(t) if t else 7200
    m = re.match(r"^M(\d+)\.(\d)\.(\d)$", when)
    if not m:
        raise SystemExit("unsupported rule date: " + rule_part)
    mo, w, d = (int(x) for x in m.groups())
    day = datetime.datetime(year, mo, day_of(year, mo, w, d))
    return int((day - datetime.datetime(1970, 1, 1)).total_seconds()) + secs


@functools.lru_cache(maxsize=None)
def _year(canon, year):
    m = RULE.match(canon)
    std, soff, dst, doff, start, end = m.groups()
    std_east = -hms(soff)
    if not dst or not start:
        return std_east, None, None, None
    dst_east = -hms(doff) if doff else std_east + 3600
    s_utc = at(start, year) - std_east    # the start is read on the standard clock
    e_utc = at(end, year) - dst_east      # the end is read on the daylight clock
    return std_east, dst_east, s_utc, e_utc


def offset_at(canon, utc_ts):
    """Seconds east of UTC that the POSIX rule gives at an instant."""
    year = datetime.datetime.fromtimestamp(utc_ts, datetime.timezone.utc).year
    std_east, dst_east, s_utc, e_utc = _year(canon, year)
    if dst_east is None:
        return std_east
    if s_utc < e_utc:
        return dst_east if s_utc <= utc_ts < e_utc else std_east
    return std_east if e_utc <= utc_ts < s_utc else dst_east


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--zoneinfo", required=True, help="a directory of compiled zone files with zone.tab and iso3166.tab")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "main", "net", "tz_table.h"))
    args = ap.parse_args()
    zi = args.zoneinfo

    version = "unknown"
    for line in open(os.path.join(zi, "tzdata.zi"), encoding="utf-8"):
        m = re.match(r"# version (\S+)", line)
        if m:
            version = m.group(1)
            break

    zones = collections.defaultdict(list)
    for line in open(os.path.join(zi, "zone.tab"), encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        cc, _, name = line.rstrip("\n").split("\t")[:3]
        zones[cc].append(name)

    chosen = {}
    for cc in sorted(zones):
        if len(zones[cc]) == 1:
            chosen[cc] = zones[cc][0]
        elif cc in PRIMARY and PRIMARY[cc] in zones[cc]:
            chosen[cc] = PRIMARY[cc]
        else:
            raise SystemExit("%s has %d zones and no usable entry in PRIMARY" % (cc, len(zones[cc])))
    for code, name in PROVINCES.items():
        chosen[code] = name

    rules, index, entries = [], {}, []
    for code in sorted(chosen):
        raw = footer(os.path.join(zi, *chosen[code].split("/")))
        c = canonical(raw)
        if c not in index:
            index[c] = len(rules)
            rules.append(c)
        entries.append((code, index[c], chosen[code]))
    if len(rules) > 255:
        raise SystemExit("more than 255 distinct rules")

    # The check: the rule reproduces the database for every entry.
    zoneinfo.reset_tzpath([zi])
    now = datetime.datetime.now(datetime.timezone.utc)
    start = int(now.timestamp()) // 1800 * 1800
    stop = int(now.replace(year=now.year + 3).timestamp())
    bad = 0
    approx = []
    for code, ri, name in entries:
        tz = zoneinfo.ZoneInfo(name)
        for ts in range(start, stop, 1800):
            want = int(datetime.datetime.fromtimestamp(ts, tz).utcoffset().total_seconds())
            got = offset_at(rules[ri], ts)
            if want != got:
                if code in APPROXIMATE:
                    approx.append(code)
                    break
                print("MISMATCH %s (%s) rule %s at %d: database %d, rule %d" % (code, name, rules[ri], ts, want, got))
                bad += 1
                break
    for code in APPROXIMATE:
        if code in chosen and code not in approx:
            print("note: %s is listed as approximate but matched; remove it from APPROXIMATE" % code)
    if bad:
        raise SystemExit("%d entries do not reproduce the database; nothing written" % bad)

    out = []
    out.append("/* GENERATED by firmware/tools/make_tz_table.py from the IANA time zone database")
    out.append(" * %s. Do not edit: run the script. One POSIX rule per country (the zone of its" % version)
    out.append(" * capital or largest city when it has several) and one per Canadian province. */")
    out.append("#pragma once")
    out.append("")
    out.append("#include <stdint.h>")
    out.append("")
    out.append("static const char *const TZ_RULES[] = {")
    for r in rules:
        out.append('    "%s",' % r)
    out.append("};")
    out.append("")
    out.append("typedef struct")
    out.append("{")
    out.append("    char code[6];")
    out.append("    uint8_t rule;")
    out.append("} tz_entry_t;")
    out.append("")
    out.append("/* Sorted by code, for a binary search. */")
    out.append("static const tz_entry_t TZ_BY_CODE[] = {")
    for code, ri, _ in entries:
        out.append('    {"%s", %d},' % (code, ri))
    out.append("};")
    out.append("")
    path = os.path.normpath(args.out)
    open(path, "w", encoding="utf-8", newline="\n").write("\n".join(out))
    print("database %s: %d entries (%d provinces), %d distinct rules, verified for three years from %s -> %s"
          % (version, len(entries), len(PROVINCES), len(rules), now.date(), os.path.basename(path)))
    for code in approx:
        print("approximate: %s (%s)" % (code, APPROXIMATE[code]))


if __name__ == "__main__":
    main()
