#pragma once

#include <stdbool.h>

/* The time zone of the device.
 *
 * The device is used in every country, so the zone is not written in the code.
 * It is taken from a country, in this order: the one kept in the TigerTag account,
 * then the one the access point announces, and UTC when neither is known. Nothing
 * is sent anywhere to find out: no address is geolocated.
 *
 * The zone only matters for the time of day shown on the printer card (the
 * estimated end of the print). The clock that TLS needs is UTC and does not
 * depend on it. */

typedef enum
{
    TZ_NONE,    /* no country known: UTC */
    TZ_ROUTER,  /* the access point's country */
    TZ_ACCOUNT, /* the account's country (or, for Canada, province) */
} tz_source_t;

/* From the country the account gave last time (kept in NVS), UTC when there is
 * none. Call once at start, before anything shows a time. */
void tz_init(void);

/* The access point's country, two letters; "" when it announced none. */
tz_source_t tz_from_router(const char *country);

/* What the account holds: `squad` is the country picked for the ranking, `vat`
 * the country of the prices, which can be a Canadian province (CA-QC). Pass
 * empty strings when the account was read and holds neither: the one remembered
 * is then forgotten. Do not call it when the read failed. */
tz_source_t tz_from_account(const char *squad, const char *vat);

tz_source_t tz_source(void);
