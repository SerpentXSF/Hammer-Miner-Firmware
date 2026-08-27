#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "cJSON.h"

#include "api_auth.h"
#include "nvs_config.h"

static const char *TAG = "api_auth";

/* Concurrent sessions. Small on purpose: a handful of browser tabs and a
 * script or two is the realistic ceiling for a single miner. */
#define MAX_SESSIONS 4

/* Sessions expire after this long without use. */
#define SESSION_IDLE_TIMEOUT_US (12ULL * 60 * 60 * 1000000)

/* After this many consecutive failures, reject for LOCKOUT_US. The device
 * is on a LAN with no TLS, so this is a speed bump against a script, not a
 * defence against a determined attacker who can already sniff the token. */
#define MAX_FAILURES 8
#define LOCKOUT_US (60ULL * 1000000)

typedef struct {
    char token[API_AUTH_TOKEN_CHARS + 1];
    int64_t last_seen_us;
    bool in_use;
} session_t;

static session_t sessions[MAX_SESSIONS];
static int failure_count;
static int64_t lockout_until_us;
static bool auth_disabled;

/*
 * Compare two NUL-terminated strings without leaking their common prefix
 * length through timing. Length still leaks, which is acceptable here.
 */
static bool constant_time_equals(const char *a, const char *b)
{
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    unsigned char diff = (unsigned char)(len_a ^ len_b);

    for (size_t i = 0; i < len_a && i < len_b; i++) {
        diff |= (unsigned char)(a[i] ^ b[i]);
    }

    return diff == 0 && len_a == len_b;
}

static void configured_password(char *out, size_t out_len)
{
    char *stored = nvs_config_get_string(NVS_CONFIG_API_PASSWORD, "");

    out[0] = '\0';
    if (stored != NULL) {
        strlcpy(out, stored, out_len);
        free(stored);
    }
}

void api_auth_init(void)
{
    char password[API_AUTH_MAX_PASSWORD + 1];

    memset(sessions, 0, sizeof(sessions));
    failure_count = 0;
    lockout_until_us = 0;

    configured_password(password, sizeof(password));
    auth_disabled = (password[0] == '\0');

    if (auth_disabled) {
        ESP_LOGW(TAG, "=====================================================");
        ESP_LOGW(TAG, "No API password is set. Every HTTP endpoint on this");
        ESP_LOGW(TAG, "device is reachable, without credentials, by any host");
        ESP_LOGW(TAG, "on this network -- including pool settings and OTA.");
        ESP_LOGW(TAG, "Set one in Settings, or in config.cvs as apipassword.");
        ESP_LOGW(TAG, "=====================================================");
    } else {
        ESP_LOGI(TAG, "API authentication enabled");
    }

    /* Do not keep the secret sitting in this stack frame. */
    memset(password, 0, sizeof(password));
}

bool api_auth_is_disabled(void)
{
    return auth_disabled;
}

void api_auth_revoke_all(void)
{
    memset(sessions, 0, sizeof(sessions));
    api_auth_init();
    ESP_LOGI(TAG, "All sessions revoked");
}

static void expire_stale_sessions(int64_t now_us)
{
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].in_use &&
            now_us - sessions[i].last_seen_us > (int64_t)SESSION_IDLE_TIMEOUT_US) {
            memset(&sessions[i], 0, sizeof(sessions[i]));
        }
    }
}

static bool touch_session(const char *token, int64_t now_us)
{
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].in_use && constant_time_equals(sessions[i].token, token)) {
            sessions[i].last_seen_us = now_us;
            return true;
        }
    }
    return false;
}

/* Issue a token into `out`, which must hold API_AUTH_TOKEN_CHARS + 1 bytes.
 * Evicts the least recently used slot when all are taken. */
static void create_session(char *out, int64_t now_us)
{
    static const char hex[] = "0123456789abcdef";
    uint8_t raw[API_AUTH_TOKEN_CHARS / 2];

    esp_fill_random(raw, sizeof(raw));
    for (size_t i = 0; i < sizeof(raw); i++) {
        out[i * 2]     = hex[raw[i] >> 4];
        out[i * 2 + 1] = hex[raw[i] & 0x0F];
    }
    out[API_AUTH_TOKEN_CHARS] = '\0';

    int slot = 0;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!sessions[i].in_use) {
            slot = i;
            break;
        }
        if (sessions[i].last_seen_us < sessions[slot].last_seen_us) {
            slot = i;
        }
    }

    memcpy(sessions[slot].token, out, API_AUTH_TOKEN_CHARS + 1);
    sessions[slot].last_seen_us = now_us;
    sessions[slot].in_use = true;
}

/* Copy the token out of an "Authorization: Bearer <token>" header. */
static bool bearer_token(httpd_req_t *req, char *out, size_t out_len)
{
    size_t len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (len == 0 || len >= 256) {
        return false;
    }

    char header[256];
    if (httpd_req_get_hdr_value_str(req, "Authorization", header, sizeof(header)) != ESP_OK) {
        return false;
    }

    const char *prefix = "Bearer ";
    size_t prefix_len = strlen(prefix);
    if (strncasecmp(header, prefix, prefix_len) != 0) {
        return false;
    }

    strlcpy(out, header + prefix_len, out_len);
    return out[0] != '\0';
}

esp_err_t api_auth_require(httpd_req_t *req)
{
    if (auth_disabled) {
        return ESP_OK;
    }

    int64_t now_us = esp_timer_get_time();
    expire_stale_sessions(now_us);

    char token[API_AUTH_TOKEN_CHARS + 1];
    if (bearer_token(req, token, sizeof(token)) && touch_session(token, now_us)) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Rejected unauthenticated %s %s",
             req->method == HTTP_GET ? "GET" : "request", req->uri);
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Bearer");
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Authentication required");
    return ESP_FAIL;
}

esp_err_t POST_api_login(httpd_req_t *req)
{
    int64_t now_us = esp_timer_get_time();

    if (now_us < lockout_until_us) {
        ESP_LOGW(TAG, "Login attempt during lockout");
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Too many attempts");
        return ESP_OK;
    }

    if (req->content_len <= 0 || req->content_len > 512) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_OK;
    }

    char body[513];
    int received = httpd_req_recv(req, body, req->content_len);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol error");
        return ESP_OK;
    }
    body[received] = '\0';

    cJSON *root = cJSON_Parse(body);
    memset(body, 0, sizeof(body));
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    cJSON *item = cJSON_GetObjectItem(root, "password");
    char supplied[API_AUTH_MAX_PASSWORD + 1] = {0};
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        strlcpy(supplied, item->valuestring, sizeof(supplied));
    }
    cJSON_Delete(root);

    char expected[API_AUTH_MAX_PASSWORD + 1];
    configured_password(expected, sizeof(expected));

    bool ok = expected[0] != '\0' && constant_time_equals(supplied, expected);

    memset(supplied, 0, sizeof(supplied));
    memset(expected, 0, sizeof(expected));

    if (!ok) {
        if (++failure_count >= MAX_FAILURES) {
            lockout_until_us = now_us + (int64_t)LOCKOUT_US;
            failure_count = 0;
            ESP_LOGW(TAG, "Too many failed logins; locking out for 60s");
        }
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid password");
        return ESP_OK;
    }

    failure_count = 0;

    char token[API_AUTH_TOKEN_CHARS + 1];
    create_session(token, now_us);

    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "token", token);
    char *payload = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, payload);

    free(payload);
    memset(token, 0, sizeof(token));

    ESP_LOGI(TAG, "Login succeeded");
    return ESP_OK;
}
