#include "i18n.h"

#include <strings.h>

#include "settings.h"

#define KEY "lang"
#define KEY_ACCOUNT "lang_a" /* 1.x: the account's language, read only */
#define KEY_PUSH "lang_push" /* a language picked here, not yet written to the account */

static const char *const TABLE[S_COUNT][LANG_COUNT] = {
#define X(id, en, fr, de, es, it, zh, pt, ptpt, pl, ru, nl) [id] = {en, fr, de, es, it, zh, pt, ptpt, pl, ru, nl},
#include "i18n_table.h"
#undef X
};

static const char *const CODES[LANG_COUNT] = {"en", "fr", "de", "es", "it", "zh", "pt", "pt-pt", "pl", "ru", "nl"};

/* Each in its own script: somebody who cannot read the current language must
 * still find their own. */
static const char *const NAMES[LANG_COUNT] = {"English",       "Français", "Deutsch", "Español",
                                              "Italiano",      "中文",     "Português (BR)", "Português (PT)",
                                              "Polski",        "Русский",  "Nederlands"};

static volatile lang_t s_lang = LANG_EN;
static bool s_chosen;

bool i18n_from_code(const char *code, lang_t *out)
{
    if (!code || !code[0])
    {
        return false;
    }
    for (int i = 0; i < LANG_COUNT; i++)
    {
        if (strcasecmp(code, CODES[i]) == 0)
        {
            *out = (lang_t)i;
            return true;
        }
    }
    /* "pt-BR", "zh-CN", "fr_FR": the language part alone. */
    char head[3] = {code[0], code[1], '\0'};
    if (code[0] && code[1] && (code[2] == '-' || code[2] == '_'))
    {
        for (int i = 0; i < LANG_COUNT; i++)
        {
            if (strcasecmp(head, CODES[i]) == 0)
            {
                *out = (lang_t)i;
                return true;
            }
        }
    }
    return false;
}

const char *i18n_native_name(lang_t l)
{
    return l < LANG_COUNT ? NAMES[l] : NAMES[LANG_EN];
}

void i18n_init(void)
{
    char code[12];
    lang_t l;
    if (settings_get(KEY, code, sizeof code) && i18n_from_code(code, &l))
    {
        s_lang = l;
        s_chosen = true;
    }
    else if (settings_get(KEY_ACCOUNT, code, sizeof code) && i18n_from_code(code, &l))
    {
        /* A device set up under 1.x spoke its account's language without a
         * choice of its own. It is not a fresh device: keep that language as
         * the device's, so the first-boot setup does not start over. */
        s_lang = l;
        s_chosen = true;
        settings_set(KEY, CODES[l]);
        settings_erase(KEY_ACCOUNT);
    }
}

void i18n_apply_account_lang(lang_t l)
{
    if (l >= LANG_COUNT)
    {
        return;
    }
    s_lang = l;
    s_chosen = true;
    settings_set(KEY, CODES[l]);
}

const char *i18n_code(lang_t l)
{
    return l < LANG_COUNT ? CODES[l] : CODES[LANG_EN];
}

bool i18n_push_pending(void)
{
    char code[12];
    return settings_get(KEY_PUSH, code, sizeof code);
}

void i18n_push_done(void)
{
    settings_erase(KEY_PUSH);
}

lang_t i18n_lang(void)
{
    return s_lang;
}

bool i18n_is_chosen(void)
{
    return s_chosen;
}

void i18n_set_lang(lang_t l, bool save)
{
    if (l >= LANG_COUNT)
    {
        return;
    }
    s_lang = l;
    if (save)
    {
        s_chosen = true;
        settings_set(KEY, CODES[l]);
        /* Picked by a person on this device: it goes up to the account at the
         * next chance, and until it has, the account's does not replace it. */
        settings_set(KEY_PUSH, CODES[l]);
    }
}

const char *tr(str_id_t id)
{
    if (id >= S_COUNT)
    {
        return "";
    }
    const char *s = TABLE[id][s_lang];
    if (!s || !s[0])
    {
        s = TABLE[id][LANG_EN];
    }
    return s ? s : "";
}
