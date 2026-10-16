#pragma once

/* A printer's address as the account gives it (account_printer_t.ip): "host",
 * or "host:port" when the printer is not on its brand's usual port - several
 * printers behind one computer, each on a port of its own. Every LAN driver
 * reads it through these, so a bare address keeps the brand's port and an
 * address with a port is used as it is. An IPv6 literal is not expected here
 * (the account holds IPv4 addresses and host names). */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Room for "host:port" from a 40-byte address plus ":65535". */
#define ADDR_HOSTPORT_MAX 48

static inline bool addr_has_port(const char *a)
{
    return a && strchr(a, ':') != NULL;
}

/* "host:port": the address as it is when it carries a port, else with `port`. */
static inline void addr_with_port(char *out, size_t n, const char *a, int port)
{
    if (addr_has_port(a))
    {
        snprintf(out, n, "%s", a);
    }
    else
    {
        snprintf(out, n, "%s:%d", a ? a : "", port);
    }
}

/* The host alone, the port (if any) left out. */
static inline void addr_host_only(char *out, size_t n, const char *a)
{
    snprintf(out, n, "%s", a ? a : "");
    char *colon = strchr(out, ':');
    if (colon)
    {
        *colon = '\0';
    }
}
