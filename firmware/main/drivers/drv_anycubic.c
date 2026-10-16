/* Anycubic (Kobra 3 / Kobra X family, ACE Pro), in both modes:
 *
 * - LAN: the printer's own MQTT broker over TLS on port 9883, reached with the
 *   broker credentials the slicer paired (reference: Tiger Studio's
 *   anycubic/PROTOCOL.md, sections 5 and 5b).
 * - Cloud: Anycubic's broker, one session for every cloud printer of the
 *   account (below, "Cloud mode"), as TigerSpool does.
 *
 * The messages are the same in both: the topics and the ACE layout are the
 * ones the TigerSpool reads, and everything after the connection is shared.
 *
 * Nothing about this printer can be read off the printer itself: the device id,
 * the user name, the password and the numeric model id exist only in the
 * slicer's own configuration, which Tiger Studio imports into the account. A
 * printer without them says so.
 *
 * The broker's certificate is self-signed and there is no authority to vouch
 * for it: it is accepted, the way a Bambu printer in LAN mode is, and trust
 * comes from the credentials. The handshake is TLS 1.2 (the broker asks for an
 * optional client certificate, which a 1.3 stack aborts on).
 *
 * Reports arrive on `.../public/{model}/{device}/{family}/report`; one
 * subscription to the whole subtree brings all of them. At rest the printer
 * does not push its temperatures, so they are asked for. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_rom_md5.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "cloud.h"
#include "i18n.h"
#include "mbedtls/base64.h"
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mqtt_client.h"
#include "mqtt_rx.h"
#include "printer_driver.h"

static const char *TAG = "anycubic";

#define MAX_MSG (48 * 1024)
#define TEMP_EVERY_US (10LL * 1000 * 1000)
#define INFO_EVERY_US (30LL * 1000 * 1000)
#define ONLINE_WINDOW_US (60LL * 1000 * 1000)
#define MAX_BOXES 6

typedef struct
{
    int id; /* -1 = the external box, 0.. = the ACE units */
    bool used;
    struct
    {
        bool present;
        uint32_t color;
        char material[12];
    } slot[4];
} box_t;

typedef struct
{
    esp_mqtt_client_handle_t mqtt;
    SemaphoreHandle_t lock;
    char devid[40], model[16];
    char t_sub[160], t_cmd_prefix[140];
    int reason; /* 0 = fine, 1 = not paired, 2 = cloud identity missing from the account */
    bool shared;  /* rides on the account's one cloud session (below), which it does not own */
    int sub_msg;  /* the id of its pending subscription on that session */
    char prefix[80]; /* "/public/{model}/{device}/": what routes a cloud message to it */

    mqtt_rx_t rx;

    bool connected, subscribed, reported_offline;
    int64_t last_msg_us, last_temp_us, last_info_us;
    uint32_t seq;

    char pstate[12]; /* idle preparing printing paused finished failed */
    int progress, remain_min, layer, total_layer;
    char file[48];
    float bed, bed_target, nozzle, nozzle_target;

    box_t box[MAX_BOXES];
    bool ext_present; /* the single external spool of a printer without ACE */
    uint32_t ext_color;
    char ext_material[12];
    bool light_on; /* the chamber light, from the `light` report */
} acu_t;

/* ---- Requests ------------------------------------------------------------- */

static void send_request(acu_t *c, const char *endpoint, const char *type, const char *action)
{
    char topic[200], body[200];
    snprintf(topic, sizeof topic, "%s/%s", c->t_cmd_prefix, endpoint);
    const int n = snprintf(body, sizeof body,
                           "{\"type\":\"%s\",\"action\":\"%s\",\"timestamp\":%lld,\"msgid\":\"tt-%u\"}", type, action,
                           (long long)time(NULL) * 1000LL, (unsigned)++c->seq);
    esp_mqtt_client_publish(c->mqtt, topic, body, n, 0, 0);
}

/* Over the cloud only the two getInfo TigerSpool sends are asked: the broker
 * closes the whole session - every cloud printer's - on a publish it does not
 * allow, and the light and temperature queries are refused (seen on the bench,
 * 2026-10-14). */
static void ask_layout(acu_t *c)
{
    send_request(c, "multiColorBox", "multiColorBox", "getInfo");
    send_request(c, "extfilbox", "extfilbox", "getInfo");
    if (!c->shared)
    {
        send_request(c, "light", "light", "query");
    }
    c->last_info_us = esp_timer_get_time();
}

static void ask_temps(acu_t *c)
{
    if (!c->shared)
    {
        send_request(c, "tempature", "tempature", "query"); /* (sic) the firmware's spelling */
    }
    c->last_temp_us = esp_timer_get_time();
}

/* ---- State merge ---------------------------------------------------------- */

static void num(const cJSON *o, const char *key, float *out)
{
    const cJSON *v = cJSON_GetObjectItem(o, key);
    if (cJSON_IsNumber(v))
    {
        *out = (float)v->valuedouble;
    }
}

static void inum(const cJSON *o, const char *key, int *out)
{
    const cJSON *v = cJSON_GetObjectItem(o, key);
    if (cJSON_IsNumber(v))
    {
        *out = (int)v->valuedouble;
    }
}

static uint32_t rgb_of(const cJSON *arr)
{
    if (!cJSON_IsArray(arr) || cJSON_GetArraySize(arr) < 3)
    {
        return 0x5a5a5a;
    }
    const int r = cJSON_GetArrayItem(arr, 0)->valueint;
    const int g = cJSON_GetArrayItem(arr, 1)->valueint;
    const int b = cJSON_GetArrayItem(arr, 2)->valueint;
    return ((uint32_t)(r & 0xff) << 16) | ((uint32_t)(g & 0xff) << 8) | (uint32_t)(b & 0xff);
}

static void apply_print_fields(acu_t *c, const cJSON *d)
{
    const cJSON *fn = cJSON_GetObjectItem(d, "filename");
    if (cJSON_IsString(fn) && fn->valuestring[0])
    {
        const char *name = fn->valuestring;
        const char *slash = strrchr(name, '/');
        strlcpy(c->file, slash ? slash + 1 : name, sizeof c->file);
        char *dot = strrchr(c->file, '.');
        if (dot && strcasecmp(dot, ".gcode") == 0)
        {
            *dot = '\0';
        }
    }
    inum(d, "progress", &c->progress);
    inum(d, "remain_time", &c->remain_min); /* minutes */
    inum(d, "curr_layer", &c->layer);
    inum(d, "total_layers", &c->total_layer);
    num(d, "curr_hotbed_temp", &c->bed);
    num(d, "curr_nozzle_temp", &c->nozzle);
    const cJSON *st = cJSON_GetObjectItem(d, "settings");
    if (cJSON_IsObject(st))
    {
        num(st, "target_hotbed_temp", &c->bed_target);
        num(st, "target_nozzle_temp", &c->nozzle_target);
    }
}

/* A transition: the action says what happened, the state where it stands. */
static const char *print_state(const char *action, const char *state)
{
    if (!strcmp(action, "pause") && (!strcmp(state, "pausing") || !strcmp(state, "paused")))
    {
        return "paused";
    }
    if (!strcmp(action, "resume") && (!strcmp(state, "resuming") || !strcmp(state, "resumed")))
    {
        return "printing";
    }
    if (!strcmp(action, "start") || !strcmp(action, "stop"))
    {
        if (!strcmp(state, "printing"))
            return "printing";
        if (!strcmp(state, "downloading") || !strcmp(state, "checking") || !strcmp(state, "preheating"))
            return "preparing";
        if (!strcmp(state, "finished"))
            return "finished";
        if (!strcmp(state, "stoped") || !strcmp(state, "stopping")) /* (sic) */
            return "idle";
        if (!strcmp(state, "failed"))
            return "failed";
    }
    return NULL; /* a data tick, not a transition */
}

static void apply_layout(acu_t *c, const cJSON *boxes)
{
    const cJSON *b;
    cJSON_ArrayForEach(b, boxes)
    {
        const cJSON *id = cJSON_GetObjectItem(b, "id");
        const cJSON *slots = cJSON_GetObjectItem(b, "slots");
        /* A partial report (drying, feeding...) has no slots: it is not a
         * layout, and treating it as one would empty the cards. */
        if (!cJSON_IsNumber(id) || !cJSON_IsArray(slots))
        {
            continue;
        }
        box_t *bx = NULL;
        for (int i = 0; i < MAX_BOXES; i++)
        {
            if (c->box[i].used && c->box[i].id == id->valueint)
            {
                bx = &c->box[i];
                break;
            }
        }
        for (int i = 0; !bx && i < MAX_BOXES; i++)
        {
            if (!c->box[i].used)
            {
                bx = &c->box[i];
                memset(bx, 0, sizeof *bx);
                bx->used = true;
                bx->id = id->valueint;
            }
        }
        if (!bx)
        {
            continue;
        }
        const cJSON *s;
        cJSON_ArrayForEach(s, slots)
        {
            const cJSON *ix = cJSON_GetObjectItem(s, "index");
            const int i = cJSON_IsNumber(ix) ? ix->valueint : -1;
            if (i < 0 || i > 3)
            {
                continue;
            }
            const cJSON *type = cJSON_GetObjectItem(s, "type");
            const bool has = cJSON_IsString(type) && type->valuestring[0];
            bx->slot[i].present = has;
            strlcpy(bx->slot[i].material, has ? type->valuestring : "", sizeof bx->slot[i].material);
            bx->slot[i].color = rgb_of(cJSON_GetObjectItem(s, "color"));
        }
    }
}

static void handle_message(acu_t *c, const char *buf, size_t len)
{
    cJSON *root = cJSON_ParseWithLength(buf, len);
    if (!root)
    {
        return;
    }
    const cJSON *tj = cJSON_GetObjectItem(root, "type");
    const cJSON *aj = cJSON_GetObjectItem(root, "action");
    const cJSON *sj = cJSON_GetObjectItem(root, "state");
    const char *type = cJSON_IsString(tj) ? tj->valuestring : "";
    const char *action = cJSON_IsString(aj) ? aj->valuestring : "";
    const char *state = cJSON_IsString(sj) ? sj->valuestring : "";
    const cJSON *data = cJSON_GetObjectItem(root, "data");

    xSemaphoreTake(c->lock, portMAX_DELAY);
    if (!strcmp(type, "print"))
    {
        const char *st = print_state(action, state);
        if (st)
        {
            strlcpy(c->pstate, st, sizeof c->pstate);
        }
        if (cJSON_IsObject(data))
        {
            apply_print_fields(c, data);
        }
        if (!strcmp(c->pstate, "finished") || !strcmp(c->pstate, "idle") || !strcmp(c->pstate, "failed"))
        {
            c->remain_min = 0;
        }
    }
    else if (!strcmp(type, "tempature"))
    {
        if (cJSON_IsObject(data))
        {
            num(data, "curr_hotbed_temp", &c->bed);
            num(data, "curr_nozzle_temp", &c->nozzle);
            num(data, "target_hotbed_temp", &c->bed_target);
            num(data, "target_nozzle_temp", &c->nozzle_target);
        }
    }
    else if (!strcmp(type, "light"))
    {
        /* Shape not fully known (Tiger Studio parses it defensively too): a
         * `status` of 0 or 1 beside the brightness. */
        const cJSON *st = cJSON_IsObject(data) ? cJSON_GetObjectItem(data, "status") : NULL;
        if (cJSON_IsNumber(st))
        {
            c->light_on = st->valuedouble != 0.0;
        }
    }
    else if (!strcmp(type, "status"))
    {
        /* The coarse busy / free heartbeat: it lifts "idle" to "preparing"
         * while the printer works before its first print tick, and drops a
         * lingering active state when the printer reports free (the end of a
         * job that was missed). */
        if (!strcmp(action, "workReport"))
        {
            if (!strcmp(state, "busy") && !strcmp(c->pstate, "idle"))
            {
                strlcpy(c->pstate, "preparing", sizeof c->pstate);
            }
            else if (!strcmp(state, "free") &&
                     (!strcmp(c->pstate, "printing") || !strcmp(c->pstate, "preparing") ||
                      !strcmp(c->pstate, "paused")))
            {
                strlcpy(c->pstate, "idle", sizeof c->pstate);
            }
        }
    }
    else if (!strcmp(type, "multiColorBox"))
    {
        const cJSON *boxes = cJSON_IsObject(data) ? cJSON_GetObjectItem(data, "multi_color_box") : NULL;
        if (cJSON_IsArray(boxes))
        {
            apply_layout(c, boxes);
        }
    }
    else if (!strcmp(type, "extfilbox"))
    {
        if (!strcmp(action, "reportInfo") && cJSON_IsObject(data))
        {
            const cJSON *t = cJSON_GetObjectItem(data, "type");
            c->ext_present = cJSON_IsString(t) && t->valuestring[0];
            strlcpy(c->ext_material, cJSON_IsString(t) ? t->valuestring : "", sizeof c->ext_material);
            c->ext_color = rgb_of(cJSON_GetObjectItem(data, "color"));
        }
    }
    else if (!strcmp(type, "lastWill"))
    {
        if (!strcmp(action, "onlineReport"))
        {
            c->reported_offline = !strcmp(state, "offline");
        }
    }
    xSemaphoreGive(c->lock);
    cJSON_Delete(root);
}

/* ---- MQTT ----------------------------------------------------------------- */

static void on_mqtt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)base;
    acu_t *c = (acu_t *)arg;
    esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)data;

    switch ((esp_mqtt_event_id_t)id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected to the printer's broker");
        xSemaphoreTake(c->lock, portMAX_DELAY);
        c->connected = true;
        c->subscribed = false;
        xSemaphoreGive(c->lock);
        /* The questions go out once the subscription is acknowledged: asked
         * earlier, the answers could be missed. */
        esp_mqtt_client_subscribe(c->mqtt, c->t_sub, 0);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        xSemaphoreTake(c->lock, portMAX_DELAY);
        c->subscribed = true;
        xSemaphoreGive(c->lock);
        ask_layout(c);
        ask_temps(c);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "disconnected");
        xSemaphoreTake(c->lock, portMAX_DELAY);
        c->connected = false;
        c->subscribed = false;
        xSemaphoreGive(c->lock);
        break;

    case MQTT_EVENT_ERROR:
        if (ev->error_handle && ev->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            ESP_LOGW(TAG, "broker refused the session (code %d)", (int)ev->error_handle->connect_return_code);
        }
        break;

    case MQTT_EVENT_DATA:
    {
        const char *text;
        size_t len;
        if (mqtt_rx_feed(&c->rx, ev, MAX_MSG, &text, &len))
        {
            c->last_msg_us = esp_timer_get_time();
            handle_message(c, text, len);
        }
        break;
    }

    default:
        break;
    }
}

/* ---- Cloud mode: one session for every cloud Anycubic of the account ------
 *
 * After TigerSpool's anycubic_cloud.cpp, measured there on the bench:
 *
 * - ONE session. The broker fixes the client id per account - md5(email +
 *   type) - and refuses any other: two sessions for one account would kick
 *   each other off in turn. Every cloud printer is a subscription on it, and
 *   messages are routed by topic, as for Bambu Lab.
 * - Signed in as the slicer does: the password is the workbench token,
 *   RSA-encrypted (PKCS#1 v1.5) to the key of Anycubic's CA and base64'd; the
 *   username is user|type|email|md5(cid + password + cid); and the client
 *   certificate is presented for mutual TLS. Tiger Studio writes the token, the
 *   CA and the client certificate on every cloud printer (net/cloud.c).
 * - As "android", beside the others: the broker keeps one session per account
 *   AND per type, and the newest takes the place of an older one. Tiger Studio
 *   and the slicer sign in as "pcf", TigerSpool as "web"; on the bench a Tiger
 *   Touch signed in as "web" beside a TigerSpool following the same printer, and
 *   each kicked the other off every few seconds (2026-10-14). TigerSpool found
 *   "web", "android" and "ios" accepted. A type that is refused, or whose
 *   sessions keep being taken (three in a row lasting under KICK_US), is left
 *   for the next one.
 * - The broker's certificate comes from Anycubic's own "AC Root CA" and is
 *   signed with SHA-1, which mbedTLS's default profile refuses. The connection
 *   is verified against that CA only, under a profile that also accepts SHA-1
 *   (acu_tls_hook); every other connection keeps the default profile. */
#define ACU_BROKER "mqtts://mqtt-universe.anycubic.com:8883"
#define ACU_MAX_SUBS 8
#define ACU_PASS_CAP 720 /* base64 of an RSA-4096 block, and its NUL */

static const char *const ACU_TYPES[] = {"android", "ios", "web", "pcf"};
#define ACU_NTYPES ((int)(sizeof ACU_TYPES / sizeof ACU_TYPES[0]))
#define KICK_US (10LL * 1000 * 1000)

static struct
{
    esp_mqtt_client_handle_t mqtt;
    esp_mqtt_client_config_t cfg; /* kept: a refused type is retried with the next one */
    bool connected, refused;
    int type;
    bool type_found;
    int64_t up_us; /* when the session last came up */
    int kicks;     /* sessions in a row that lasted under KICK_US */
    acu_t *subs[ACU_MAX_SUBS];
    int nsubs;
    acu_t *rx_target; /* the printer the message being received is for */
    cloud_acu_identity_t id; /* the client keeps pointers to the PEMs: they live as long as it */
    mbedtls_x509_crt ca;
    char cid[33], user[200];
    char *pass;
} s_as;

static SemaphoreHandle_t as_lock(void)
{
    static StaticSemaphore_t buf;
    static SemaphoreHandle_t h;
    if (!h)
    {
        h = xSemaphoreCreateMutexStatic(&buf);
    }
    return h;
}

static const mbedtls_x509_crt_profile ACU_PROFILE = {
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA1) | MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256) |
        MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384) | MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA512),
    0xFFFFFFF, /* any public-key algorithm */
    0xFFFFFFF, /* any curve */
    2048,
};

/* The signature algorithms offered in the hello. The broker answers
 * "insufficient security" (alert 71) to a client that does not list RSA with
 * SHA-1, the one its own certificate is signed with - checked with openssl
 * from a computer. mbedTLS 3 leaves it out by default; the mbedTLS 2 of
 * TigerSpool's Arduino core still offered it. */
static const uint16_t ACU_SIG_ALGS[] = {
    MBEDTLS_TLS1_3_SIG_RSA_PKCS1_SHA256,       MBEDTLS_TLS1_3_SIG_RSA_PKCS1_SHA384,
    MBEDTLS_TLS1_3_SIG_RSA_PKCS1_SHA512,       MBEDTLS_TLS1_3_SIG_ECDSA_SECP256R1_SHA256,
    MBEDTLS_TLS1_3_SIG_ECDSA_SECP384R1_SHA384, MBEDTLS_TLS1_3_SIG_RSA_PKCS1_SHA1,
    MBEDTLS_TLS1_3_SIG_NONE,
};

/* esp-tls hands the TLS configuration here before the handshake: the server is
 * verified against Anycubic's CA alone, SHA-1 signatures allowed. */
static esp_err_t acu_tls_hook(void *conf)
{
    mbedtls_ssl_config *c = (mbedtls_ssl_config *)conf;
    mbedtls_ssl_conf_authmode(c, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(c, &s_as.ca, NULL);
    mbedtls_ssl_conf_cert_profile(c, &ACU_PROFILE);
    mbedtls_ssl_conf_sig_algs(c, ACU_SIG_ALGS);
    return ESP_OK;
}

static int acu_rng(void *ctx, unsigned char *buf, size_t len)
{
    (void)ctx;
    esp_fill_random(buf, len);
    return 0;
}

static void md5_hex(const char *const *parts, int n, char out[33])
{
    md5_context_t m;
    uint8_t d[ESP_ROM_MD5_DIGEST_LEN];
    esp_rom_md5_init(&m);
    for (int i = 0; i < n; i++)
    {
        esp_rom_md5_update(&m, parts[i], strlen(parts[i]));
    }
    esp_rom_md5_final(d, &m);
    for (int i = 0; i < ESP_ROM_MD5_DIGEST_LEN; i++)
    {
        sprintf(out + 2 * i, "%02x", d[i]);
    }
}

/* The three login fields for the current type, a fresh encryption each time. */
static bool build_login(void)
{
    const char *type = ACU_TYPES[s_as.type];
    const char *cid_parts[] = {s_as.id.email, type};
    md5_hex(cid_parts, 2, s_as.cid);
    uint8_t cipher[512];
    size_t clen = 0, olen = 0;
    const char *tok = s_as.id.token;
    if (mbedtls_pk_encrypt(&s_as.ca.pk, (const unsigned char *)tok, strlen(tok), cipher, &clen, sizeof cipher, acu_rng,
                           NULL) != 0 ||
        mbedtls_base64_encode((unsigned char *)s_as.pass, ACU_PASS_CAP, &olen, cipher, clen) != 0)
    {
        return false;
    }
    s_as.pass[olen] = '\0';
    char sig[33];
    const char *sig_parts[] = {s_as.cid, s_as.pass, s_as.cid};
    md5_hex(sig_parts, 3, sig);
    snprintf(s_as.user, sizeof s_as.user, "user|%s|%s|%s", type, s_as.id.email, sig);
    return true;
}

/* Everything the session holds, once its client is gone. */
static void as_release(void)
{
    mbedtls_x509_crt_free(&s_as.ca);
    cloud_anycubic_identity_free(&s_as.id);
    if (s_as.pass)
    {
        memset(s_as.pass, 0, ACU_PASS_CAP);
        free(s_as.pass);
        s_as.pass = NULL;
    }
    memset(s_as.user, 0, sizeof s_as.user);
}

/* The identity from the account, the CA parsed, the login built. */
static bool as_prepare(void)
{
    if (!cloud_anycubic_identity(&s_as.id))
    {
        return false;
    }
    mbedtls_x509_crt_init(&s_as.ca);
    s_as.pass = heap_caps_malloc(ACU_PASS_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    size_t der_len = 0;
    const char *b64 = s_as.id.ca_der_b64;
    mbedtls_base64_decode(NULL, 0, &der_len, (const unsigned char *)b64, strlen(b64));
    uint8_t *der = der_len ? heap_caps_malloc(der_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : NULL;
    const bool ok = s_as.pass && der &&
                    mbedtls_base64_decode(der, der_len, &der_len, (const unsigned char *)b64, strlen(b64)) == 0 &&
                    mbedtls_x509_crt_parse_der(&s_as.ca, der, der_len) == 0 && build_login();
    free(der);
    if (!ok)
    {
        ESP_LOGW(TAG, "the Anycubic cloud identity in the account could not be used");
        as_release();
    }
    return ok;
}

/* Whether a topic (not NUL-terminated) contains `part`. */
static bool topic_has(const char *topic, int len, const char *part)
{
    const int n = (int)strlen(part);
    for (int i = 0; i + n <= len; i++)
    {
        if (memcmp(topic + i, part, n) == 0)
        {
            return true;
        }
    }
    return false;
}

/* The report families a cloud printer is followed on, one subscription each:
 * the broker closes the session on a subscription it does not allow, and the
 * whole subtree ("/#") is one of those. TigerSpool's two first. */
static const char *const ACU_FAMILIES[] = {"multiColorBox", "extfilbox", "print",   "tempature",
                                           "status",        "light",     "lastWill"};
#define ACU_NFAMILIES ((int)(sizeof ACU_FAMILIES / sizeof ACU_FAMILIES[0]))

static void as_topic(const acu_t *c, int family, char *out, size_t size)
{
    snprintf(out, size, "%s%s/#", c->t_sub, ACU_FAMILIES[family]);
}

static void as_subscribe(acu_t *c)
{
    c->subscribed = false;
    for (int f = 0; f < ACU_NFAMILIES; f++)
    {
        char t[200];
        as_topic(c, f, t, sizeof t);
        c->sub_msg = esp_mqtt_client_subscribe(s_as.mqtt, t, 0);
        ESP_LOGI(TAG, "subscribing to %s (msg %d)", ACU_FAMILIES[f], c->sub_msg);
    }
}

static void on_cloud_mqtt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)data;
    xSemaphoreTake(as_lock(), portMAX_DELAY);
    switch ((esp_mqtt_event_id_t)id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected to the Anycubic cloud as '%s' (%d printer(s) on the session)",
                 ACU_TYPES[s_as.type], s_as.nsubs);
        s_as.connected = true;
        s_as.refused = false;
        s_as.type_found = true;
        s_as.up_us = esp_timer_get_time();
        for (int i = 0; i < s_as.nsubs; i++)
        {
            acu_t *c = s_as.subs[i];
            xSemaphoreTake(c->lock, portMAX_DELAY);
            c->connected = true;
            xSemaphoreGive(c->lock);
            as_subscribe(c);
        }
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "subscription %d acknowledged (%d byte(s): %02x)", ev->msg_id, ev->data_len,
                 ev->data_len > 0 ? (unsigned)(uint8_t)ev->data[0] : 0xffu);
        for (int i = 0; i < s_as.nsubs; i++)
        {
            acu_t *c = s_as.subs[i];
            if (c->sub_msg == ev->msg_id)
            {
                xSemaphoreTake(c->lock, portMAX_DELAY);
                c->subscribed = true;
                xSemaphoreGive(c->lock);
                ask_layout(c);
                ask_temps(c);
            }
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Anycubic cloud session disconnected");
        if (s_as.connected)
        {
            /* Taken over within seconds, again and again: another client signs
             * in with this type. Leave it the type and take the next one. */
            s_as.kicks = esp_timer_get_time() - s_as.up_us < KICK_US ? s_as.kicks + 1 : 0;
            if (s_as.kicks >= 3 && s_as.pass)
            {
                s_as.kicks = 0;
                s_as.type = (s_as.type + 1) % ACU_NTYPES;
                ESP_LOGW(TAG, "another client keeps taking the session: signing in as '%s'", ACU_TYPES[s_as.type]);
                if (build_login())
                {
                    esp_mqtt_set_config(s_as.mqtt, &s_as.cfg);
                }
            }
        }
        s_as.connected = false;
        s_as.rx_target = NULL;
        for (int i = 0; i < s_as.nsubs; i++)
        {
            acu_t *c = s_as.subs[i];
            xSemaphoreTake(c->lock, portMAX_DELAY);
            c->connected = c->subscribed = false;
            xSemaphoreGive(c->lock);
        }
        break;
    case MQTT_EVENT_ERROR:
        if (ev->error_handle && ev->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            const int rc = (int)ev->error_handle->connect_return_code;
            const bool creds = rc == MQTT_CONNECTION_REFUSE_BAD_USERNAME || rc == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED;
            ESP_LOGW(TAG, "the Anycubic cloud refused '%s' (return code %d)", ACU_TYPES[s_as.type], rc);
            if (creds && !s_as.type_found && s_as.type + 1 < ACU_NTYPES)
            {
                s_as.type++; /* not yet a verdict on the token: try the next type */
            }
            else if (creds)
            {
                /* The token was revoked by a newer slicer sign-in; only Tiger
                 * Studio can fetch a new one. Retried slowly, not every 5 s. */
                s_as.refused = true;
                s_as.type = 0;
                s_as.type_found = false;
                s_as.cfg.network.reconnect_timeout_ms = 60000;
            }
            if (s_as.pass && build_login())
            {
                esp_mqtt_set_config(s_as.mqtt, &s_as.cfg); /* the strings are copied; reconnects with them */
            }
        }
        break;
    case MQTT_EVENT_DATA:
    {
        /* The topic comes with the first chunk only: it decides who the
         * following chunks are for. */
        if (ev->current_data_offset == 0)
        {
            s_as.rx_target = NULL;
            for (int i = 0; ev->topic && i < s_as.nsubs; i++)
            {
                const acu_t *c = s_as.subs[i];
                if (topic_has(ev->topic, ev->topic_len, c->prefix))
                {
                    s_as.rx_target = s_as.subs[i];
                    break;
                }
            }
        }
        acu_t *c = s_as.rx_target;
        const char *text;
        size_t len;
        if (c && mqtt_rx_feed(&c->rx, ev, MAX_MSG, &text, &len))
        {
            if (!c->last_msg_us)
            {
                ESP_LOGI(TAG, "first report from a cloud printer (%u bytes)", (unsigned)len);
            }
            c->last_msg_us = esp_timer_get_time();
            handle_message(c, text, len);
        }
        break;
    }
    default:
        break;
    }
    xSemaphoreGive(as_lock());
}

/* Joins the account's cloud session, opening it when this is the first cloud
 * printer. Returns false when the account holds no usable identity. */
static bool cloud_join(acu_t *c)
{
    xSemaphoreTake(as_lock(), portMAX_DELAY);
    if (s_as.nsubs >= ACU_MAX_SUBS)
    {
        xSemaphoreGive(as_lock());
        return false;
    }
    if (!s_as.mqtt)
    {
        s_as.type = 0;
        s_as.type_found = false;
        s_as.refused = false;
        s_as.kicks = 0;
        if (!as_prepare())
        {
            xSemaphoreGive(as_lock());
            return false;
        }
        s_as.cfg = (esp_mqtt_client_config_t){
            .broker.address.uri = ACU_BROKER,
            .broker.verification.crt_bundle_attach = acu_tls_hook,
            .credentials.client_id = s_as.cid,
            .credentials.username = s_as.user,
            .credentials.authentication.password = s_as.pass,
            .credentials.authentication.certificate = s_as.id.cert_pem,
            .credentials.authentication.key = s_as.id.key_pem,
            .session.keepalive = 30,
            .buffer.size = 4096,
            .buffer.out_size = 1024,
            .task.stack_size = 8192,
            .network.reconnect_timeout_ms = 5000,
            .network.timeout_ms = 8000,
        };
        s_as.mqtt = esp_mqtt_client_init(&s_as.cfg);
        if (!s_as.mqtt)
        {
            as_release();
            xSemaphoreGive(as_lock());
            return false;
        }
        s_as.connected = false;
        esp_mqtt_client_register_event(s_as.mqtt, ESP_EVENT_ANY_ID, on_cloud_mqtt, NULL);
        esp_mqtt_client_start(s_as.mqtt);
        ESP_LOGI(TAG, "opening the Anycubic cloud session");
    }
    c->mqtt = s_as.mqtt;
    c->shared = true;
    s_as.subs[s_as.nsubs++] = c;
    if (s_as.connected)
    {
        xSemaphoreTake(c->lock, portMAX_DELAY);
        c->connected = true;
        xSemaphoreGive(c->lock);
        as_subscribe(c);
    }
    xSemaphoreGive(as_lock());
    return true;
}

static void cloud_leave(acu_t *c)
{
    esp_mqtt_client_handle_t closing = NULL;
    xSemaphoreTake(as_lock(), portMAX_DELAY);
    for (int i = 0; i < s_as.nsubs; i++)
    {
        if (s_as.subs[i] == c)
        {
            s_as.subs[i] = s_as.subs[--s_as.nsubs];
            break;
        }
    }
    if (s_as.rx_target == c)
    {
        s_as.rx_target = NULL;
    }
    if (s_as.connected && s_as.nsubs > 0)
    {
        for (int f = 0; f < ACU_NFAMILIES; f++)
        {
            char t[200];
            as_topic(c, f, t, sizeof t);
            esp_mqtt_client_unsubscribe(s_as.mqtt, t);
        }
    }
    if (s_as.nsubs == 0)
    {
        closing = s_as.mqtt;
        s_as.mqtt = NULL;
        s_as.connected = false;
    }
    xSemaphoreGive(as_lock());
    c->mqtt = NULL;
    /* Outside the lock: stopping waits for the client's task, whose event
     * handler takes that lock. */
    if (closing)
    {
        esp_mqtt_client_stop(closing);
        esp_mqtt_client_destroy(closing);
        xSemaphoreTake(as_lock(), portMAX_DELAY);
        if (!s_as.mqtt)
        {
            as_release();
        }
        xSemaphoreGive(as_lock());
        ESP_LOGI(TAG, "Anycubic cloud session closed");
    }
}

/* ---- Driver --------------------------------------------------------------- */

static drv_kind_t acu_support(const account_printer_t *p)
{
    if (p->cloud)
    {
        return DRV_CLOUD;
    }
    return p->ip[0] ? DRV_LAN : DRV_NONE;
}

static void *acu_start(const account_printer_t *p)
{
    acu_t *c = calloc(1, sizeof *c);
    if (!c)
    {
        return NULL;
    }
    c->lock = xSemaphoreCreateMutex();
    strlcpy(c->pstate, "idle", sizeof c->pstate);
    strlcpy(c->devid, p->dev_id, sizeof c->devid);
    strlcpy(c->model, p->acu_model, sizeof c->model);

    if (p->cloud)
    {
        const bool has_key = p->dev_id[0], has_type = p->acu_model[0];
        if (!has_key || !has_type)
        {
            c->reason = 1;
            ESP_LOGW(TAG, "an Anycubic cloud printer lacks%s%s in the account", has_key ? "" : " key",
                     has_type ? "" : " machineType");
            return c;
        }
        /* The first segment is not always "printer": a wildcard, as on TigerSpool.
         * The base of the topics; as_topic() adds each family. */
        snprintf(c->t_sub, sizeof c->t_sub, "anycubic/anycubicCloud/v1/+/public/%s/%s/", c->model, c->devid);
        snprintf(c->t_cmd_prefix, sizeof c->t_cmd_prefix, "anycubic/anycubicCloud/v1/web/printer/%s/%s", c->model,
                 c->devid);
        snprintf(c->prefix, sizeof c->prefix, "/public/%s/%s/", c->model, c->devid);
        if (!cloud_join(c))
        {
            c->reason = 2; /* alive, silent, and the notice says why */
        }
        return c;
    }
    if (!p->dev_id[0] || !p->user[0] || !p->acu_model[0] || !p->cc[0])
    {
        c->reason = 1;
        /* Which of the four is missing, said by field name, never by value. */
        const char *no_device = p->dev_id[0] ? "" : " deviceId";
        const char *no_user = p->user[0] ? "" : " username";
        const char *no_model = p->acu_model[0] ? "" : " acuModelId";
        const char *no_secret = p->cc[0] ? "" : " password";
        ESP_LOGW(TAG, "an Anycubic printer lacks%s%s%s%s in the account", no_device, no_user, no_model,
                 no_secret);
        return c;
    }

    snprintf(c->t_sub, sizeof c->t_sub, "anycubic/anycubicCloud/v1/printer/public/%s/%s/#", c->model, c->devid);
    snprintf(c->t_cmd_prefix, sizeof c->t_cmd_prefix, "anycubic/anycubicCloud/v1/web/printer/%s/%s", c->model,
             c->devid);

    char uri[64], cid[48];
    snprintf(uri, sizeof uri, "mqtts://%s:9883", p->ip);
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    /* One id per printer: two on one account would otherwise kick each other
     * off the broker in turn. */
    snprintf(cid, sizeof cid, "tigertouch-%02x%02x%02x-%.8s", mac[3], mac[4], mac[5], c->devid);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = uri,
        .broker.verification.skip_cert_common_name_check = true, /* self-signed, on the LAN */
        .credentials.username = p->user,
        .credentials.client_id = cid,
        .credentials.authentication.password = p->cc,
        .session.keepalive = 30,
        .buffer.size = 4096,
        .buffer.out_size = 1024,
        .task.stack_size = 8192,
        .network.reconnect_timeout_ms = 5000,
        .network.timeout_ms = 8000,
    };
    c->mqtt = esp_mqtt_client_init(&cfg);
    if (!c->mqtt)
    {
        vSemaphoreDelete(c->lock);
        free(c);
        return NULL;
    }
    esp_mqtt_client_register_event(c->mqtt, ESP_EVENT_ANY_ID, on_mqtt, c);
    esp_mqtt_client_start(c->mqtt);
    ESP_LOGI(TAG, "connecting to the printer's broker");
    return c;
}

static void acu_stop(void *ctx)
{
    acu_t *c = (acu_t *)ctx;
    if (!c)
    {
        return;
    }
    if (c->shared)
    {
        cloud_leave(c);
    }
    else if (c->mqtt)
    {
        esp_mqtt_client_stop(c->mqtt);
        esp_mqtt_client_destroy(c->mqtt);
    }
    vSemaphoreDelete(c->lock);
    mqtt_rx_free(&c->rx);
    free(c);
}

static int box_order(const void *a, const void *b)
{
    return ((const box_t *)a)->id - ((const box_t *)b)->id;
}

/* Pause, resume, stop: a print request with action pause / resume / stop and taskid -1, on the family topic. Written from Tiger Studio's protocol notes; this driver
 * has not been run against a real printer, so the command is not verified. */
static bool acu_control(void *ctx, pv_control_t cmd)
{
    static const char *const ACTION[] = {[PV_CTL_PAUSE] = "pause", [PV_CTL_RESUME] = "resume", [PV_CTL_STOP] = "stop"};
    acu_t *c = (acu_t *)ctx;
    if (c && c->shared)
    {
        return false; /* no command over the cloud until each one is seen allowed (ask_layout) */
    }
    if (c && c->mqtt && c->connected && c->subscribed && (cmd == PV_CTL_LIGHT_ON || cmd == PV_CTL_LIGHT_OFF))
    {
        /* `light` family, type 3 is the chamber light (type 1 is the camera's). */
        const bool on = cmd == PV_CTL_LIGHT_ON;
        char ltopic[200], lbody[240];
        snprintf(ltopic, sizeof ltopic, "%s/light", c->t_cmd_prefix);
        const int ln = snprintf(lbody, sizeof lbody,
                                "{\"type\":\"light\",\"action\":\"control\",\"timestamp\":%lld,\"msgid\":\"tt-%u\","
                                "\"data\":{\"type\":3,\"status\":%d,\"brightness\":%d}}",
                                (long long)time(NULL) * 1000LL, (unsigned)++c->seq, on ? 1 : 0, on ? 100 : 0);
        return ln > 0 && esp_mqtt_client_publish(c->mqtt, ltopic, lbody, ln, 0, 0) >= 0;
    }
    if (!c || !c->mqtt || !c->connected || !c->subscribed || (int)cmd < 0 || (int)cmd > PV_CTL_STOP)
    {
        return false;
    }
    char topic[200], body[240];
    snprintf(topic, sizeof topic, "%s/print", c->t_cmd_prefix);
    const int n = snprintf(body, sizeof body,
                           "{\"type\":\"print\",\"action\":\"%s\",\"timestamp\":%lld,\"msgid\":\"tt-%u\","
                           "\"data\":{\"taskid\":\"-1\"}}",
                           ACTION[cmd], (long long)time(NULL) * 1000LL, (unsigned)++c->seq);
    return n > 0 && esp_mqtt_client_publish(c->mqtt, topic, body, n, 0, 0) >= 0;
}

static void acu_snapshot(void *ctx, printer_view_t *v)
{
    acu_t *c = (acu_t *)ctx;
    if (c->reason)
    {
        v->online = false;
        v->can_control = v->online;
        v->can_light = false;
        snprintf(v->notice, sizeof v->notice, "%s", tr(c->reason == 2 ? S_ACU_CLOUD_NO_ID : S_ACU_NOT_PAIRED));
        v->notice_error = true;
        return;
    }
    if (c->shared && s_as.refused)
    {
        snprintf(v->notice, sizeof v->notice, "%s", tr(S_ACU_CLOUD_REFUSED));
        v->notice_error = true;
    }

    const int64_t now = esp_timer_get_time();
    if (c->connected && c->subscribed)
    {
        if (now - c->last_temp_us > TEMP_EVERY_US)
        {
            ask_temps(c);
        }
        if (now - c->last_info_us > INFO_EVERY_US)
        {
            ask_layout(c);
        }
    }

    xSemaphoreTake(c->lock, portMAX_DELAY);
    v->online = c->connected && c->subscribed && !c->reported_offline && c->last_msg_us != 0 &&
                (now - c->last_msg_us) < ONLINE_WINDOW_US;
    v->can_control = v->online && !c->shared;
    v->can_light = v->online && !c->shared;
    v->light_on = c->light_on;

    const char *s = c->pstate;
    if (!strcmp(s, "printing") || !strcmp(s, "preparing"))
        v->state = PV_PRINTING;
    else if (!strcmp(s, "paused"))
        v->state = PV_PAUSED;
    else if (!strcmp(s, "finished"))
        v->state = PV_DONE;
    else if (!strcmp(s, "failed"))
        v->state = PV_ERROR;
    else
        v->state = PV_IDLE;

    v->progress = v->state == PV_IDLE ? 0 : (v->state == PV_DONE ? 100 : (c->progress > 100 ? 100 : c->progress));
    v->layer = c->layer;
    v->total_layer = c->total_layer;
    v->left_s = (v->state == PV_PRINTING || v->state == PV_PAUSED) ? c->remain_min * 60 : 0;
    v->nozzle = c->nozzle;
    v->nozzle_target = c->nozzle_target;
    v->bed = c->bed;
    v->bed_target = c->bed_target;
    v->chamber = 0;
    strlcpy(v->file, c->file, sizeof v->file);

    memset(v->unit, 0, sizeof v->unit);
    int n = 0;
    if (c->ext_present)
    {
        /* A printer with no ACE: one spool, on its own compact card. */
        pv_unit_t *u = &v->unit[n++];
        u->present = true;
        u->slots = 1;
        snprintf(u->label, sizeof u->label, PV_LABEL_EXT);
        u->slot[0].present = true;
        u->slot[0].color = c->ext_color;
        u->slot[0].percent = -1;
        strlcpy(u->slot[0].material, c->ext_material, sizeof u->slot[0].material);
    }
    box_t sorted[MAX_BOXES];
    int nb = 0;
    for (int i = 0; i < MAX_BOXES; i++)
    {
        if (c->box[i].used)
        {
            sorted[nb++] = c->box[i];
        }
    }
    qsort(sorted, (size_t)nb, sizeof sorted[0], box_order);
    int aces = 0;
    for (int i = 0; i < nb; i++)
    {
        aces += sorted[i].id >= 0;
    }
    int letter = 0;
    for (int i = 0; i < nb && n < PV_MAX_UNITS; i++)
    {
        pv_unit_t *u = &v->unit[n++];
        u->present = true;
        u->slots = 4;
        if (sorted[i].id < 0)
        {
            /* The external box of an ACE printer has four slots of its own,
             * so it is not the compact single-spool card. */
            snprintf(u->label, sizeof u->label, "EXT");
        }
        else if (aces > 1)
        {
            snprintf(u->label, sizeof u->label, "ACE %c", 'A' + (letter++ % 26));
        }
        else
        {
            snprintf(u->label, sizeof u->label, "ACE");
        }
        for (int k = 0; k < 4; k++)
        {
            u->slot[k].present = sorted[i].slot[k].present;
            u->slot[k].color = sorted[i].slot[k].color;
            u->slot[k].percent = -1;
            strlcpy(u->slot[k].material, sorted[i].slot[k].material, sizeof u->slot[k].material);
        }
    }
    xSemaphoreGive(c->lock);
}

const printer_driver_t DRIVER_ANYCUBIC = {
    .brand = "anycubic",
    .support = acu_support,
    .start = acu_start,
    .stop = acu_stop,
    .snapshot = acu_snapshot,
    .control = acu_control,
};
