#pragma once

#include <stdbool.h>

/* The 11 languages of Tiger Studio Manager, in its own order and with its own
 * codes, so that a preference means the same thing everywhere. */
typedef enum
{
    LANG_EN = 0,
    LANG_FR,
    LANG_DE,
    LANG_ES,
    LANG_IT,
    LANG_ZH,
    LANG_PT,    /* Brazilian */
    LANG_PT_PT, /* European */
    LANG_PL,
    LANG_RU,
    LANG_NL,
    LANG_COUNT
} lang_t;

typedef enum
{
#define X(id, ...) id,
#include "i18n_table.h"
#undef X
    S_COUNT
} str_id_t;

/* Reads the saved language (English when none was ever chosen). */
void i18n_init(void);

lang_t i18n_lang(void);
/* `save` keeps it across restarts, and marks it to be written to the account
 * (users/{uid}/prefs/app.lang), as TigerScale does. */
void i18n_set_lang(lang_t l, bool save);

/* The sentence in the current language; English when a column is empty. */
const char *tr(str_id_t id);

/* "fr", "pt-pt"...: what Studio stores in the account's preferences. */
bool i18n_from_code(const char *code, lang_t *out);

/* The language's name in its own script, for the picker. */
const char *i18n_native_name(lang_t l);

/* True once a language is set on this device. False on a fresh device: the
 * first-boot setup starts with the language picker because of it. */
bool i18n_is_chosen(void);

/* The account's language, applied and kept, without marking it to be written
 * back. Called when the account is read and no local choice is waiting to go
 * up: the account then wins, as on TigerScale. */
void i18n_apply_account_lang(lang_t l);

/* "fr", "pt-pt"... for a language. */
const char *i18n_code(lang_t l);

/* A language picked on the device is waiting to be written to the account. */
bool i18n_push_pending(void);
void i18n_push_done(void);
