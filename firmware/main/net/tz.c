#include "tz.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "settings.h"
#include "tz_table.h"

static const char *TAG = "tz";

/* What the account gave last time, so that the zone is right at the next power-on
 * before the account has been read again. */
#define KEY_ACCOUNT "tz_a"

#define CODE_MAX 6 /* "FR" or "CA-QC", and the NUL */

static char s_account[CODE_MAX];
static char s_router[CODE_MAX];
static tz_source_t s_source = TZ_NONE;
static bool s_logged;

static const char *rule_for(const char *code)
{
    size_t lo = 0;
    size_t hi = sizeof TZ_BY_CODE / sizeof TZ_BY_CODE[0];
    while (lo < hi)
    {
        const size_t mid = (lo + hi) / 2;
        const int c = strcmp(code, TZ_BY_CODE[mid].code);
        if (c == 0)
        {
            return TZ_RULES[TZ_BY_CODE[mid].rule];
        }
        if (c < 0)
        {
            hi = mid;
        }
        else
        {
            lo = mid + 1;
        }
    }
    return NULL;
}

/* "fr" -> "FR", "ca-qc" -> "CA-QC"; false for anything that is not one of the
 * two shapes (the account can also hold "CUSTOM"). */
static bool normalise(const char *in, char out[CODE_MAX])
{
    out[0] = '\0';
    if (!in)
    {
        return false;
    }
    const size_t n = strlen(in);
    if (n != 2 && n != 5)
    {
        return false;
    }
    for (size_t i = 0; i < n; i++)
    {
        const bool dash = (n == 5 && i == 2);
        if (dash ? in[i] != '-' : !isalpha((unsigned char)in[i]))
        {
            return false;
        }
        out[i] = dash ? '-' : (char)toupper((unsigned char)in[i]);
    }
    out[n] = '\0';
    return true;
}

/* The account holds two countries. The one picked for the ranking is the person's
 * own; the one of the prices can be a Canadian province, which names a finer zone
 * than "CA" does, and is used when it agrees with the country. */
static bool account_code(const char *squad, const char *vat, char out[CODE_MAX])
{
    char s[CODE_MAX];
    char v[CODE_MAX];
    const bool has_s = normalise(squad, s);
    const bool has_v = normalise(vat, v);
    const bool province = has_v && strlen(v) == 5;

    if (province && (!has_s || strcmp(s, "CA") == 0) && rule_for(v))
    {
        strlcpy(out, v, CODE_MAX);
        return true;
    }
    if (has_s && strlen(s) == 2 && rule_for(s))
    {
        strlcpy(out, s, CODE_MAX);
        return true;
    }
    if (has_v)
    {
        char country[3] = {v[0], v[1], '\0'};
        if (rule_for(country))
        {
            strlcpy(out, country, CODE_MAX);
            return true;
        }
    }
    return false;
}

static void apply(const char *rule)
{
    setenv("TZ", rule, 1);
    tzset();
}

/* The account's country, else the access point's, else UTC. */
static void settle(void)
{
    const char *rule;
    tz_source_t src;
    if (s_account[0] && (rule = rule_for(s_account)) != NULL)
    {
        src = TZ_ACCOUNT;
    }
    else if (s_router[0] && (rule = rule_for(s_router)) != NULL)
    {
        src = TZ_ROUTER;
    }
    else
    {
        rule = "UTC0";
        src = TZ_NONE;
    }
    apply(rule);
    if (!s_logged || src != s_source)
    {
        /* Which source, never which country: a country is where somebody lives. */
        ESP_LOGI(TAG, "time zone from %s", src == TZ_ACCOUNT  ? "the account"
                                           : src == TZ_ROUTER ? "the access point"
                                                              : "nothing known (UTC)");
        s_logged = true;
    }
    s_source = src;
}

void tz_init(void)
{
    char saved[CODE_MAX];
    s_account[0] = s_router[0] = '\0';
    if (settings_get(KEY_ACCOUNT, saved, sizeof saved) && normalise(saved, s_account) && !rule_for(s_account))
    {
        s_account[0] = '\0';
    }
    settle();
}

tz_source_t tz_from_router(const char *country)
{
    char c[CODE_MAX];
    s_router[0] = '\0';
    if (normalise(country, c) && strlen(c) == 2)
    {
        strlcpy(s_router, c, sizeof s_router);
    }
    settle();
    return s_source;
}

tz_source_t tz_from_account(const char *squad, const char *vat)
{
    char code[CODE_MAX];
    if (account_code(squad, vat, code))
    {
        if (strcmp(code, s_account) != 0)
        {
            settings_set(KEY_ACCOUNT, code);
        }
        strlcpy(s_account, code, sizeof s_account);
    }
    else
    {
        if (s_account[0])
        {
            settings_erase(KEY_ACCOUNT);
        }
        s_account[0] = '\0';
    }
    settle();
    return s_source;
}

tz_source_t tz_source(void)
{
    return s_source;
}
