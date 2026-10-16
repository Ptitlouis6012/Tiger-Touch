#pragma once

#include <stdbool.h>

/* The first-boot setup and the account screens, laid out as on TigerScale V3:
 * a question with "Set up / Later", one sign-in page (e-mail, password, Sign in,
 * Continue with Google), and the QR pairing page for Google accounts. Like
 * every screen they only post events; the application task decides. Every
 * ui_* call here needs the LVGL lock. */

typedef enum
{
    PROMPT_WIFI,
    PROMPT_ACCOUNT,
} prompt_kind_t;

/* A full-screen question: icon, title, one line under it, the primary button
 * and "Later". Posts EV_PROMPT_GO or EV_PROMPT_LATER. */
void ui_show_prompt(prompt_kind_t kind, const char *title, const char *sub, const char *primary);

/* The same question as a panel sliding in from the right over whatever is on
 * show, for the reminders on the home screens. Touching outside it is "Later".
 * It closes itself when answered; ui_side_prompt_close() takes it away
 * unanswered (before another screen is shown). */
void ui_show_side_prompt(prompt_kind_t kind, const char *title, const char *primary);
void ui_side_prompt_close(void);
bool ui_side_prompt_open(void);

/* The sign-in page. `email` is shown in its field; `has_password` draws dots in
 * the other. Touching a field posts EV_EDIT_EMAIL / EV_EDIT_PASS (the caller
 * opens the keyboard form), "Sign in" EV_SUBMIT, "Continue with Google"
 * EV_GOOGLE, the back arrow EV_CANCEL. */
void ui_show_sign_in(const char *email, bool has_password, const char *error);

/* The pairing page: the QR square waits with a spinner until ui_pairing_set()
 * gives it the URL and the code. ui_pairing_result() replaces the QR by a
 * line of text (green when linked). The back arrow posts EV_CANCEL. */
void ui_show_pairing(void);
void ui_pairing_set(const char *verify_url, const char *code);
void ui_pairing_result(const char *text, bool ok, const char *detail);
