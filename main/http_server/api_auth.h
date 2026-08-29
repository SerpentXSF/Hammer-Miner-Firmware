#ifndef MAIN_HTTP_SERVER_API_AUTH_H
#define MAIN_HTTP_SERVER_API_AUTH_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"

/*
 * Shared-secret authentication for the HTTP API.
 *
 * The vendor firmware shipped a login page in the web UI that POSTed to
 * /api/system/login, but no such route existed in the firmware and no
 * endpoint ever checked a credential. Access was gated only by
 * is_network_allowed(), which tests whether the caller sits in an RFC1918
 * range -- a DNS-rebinding mitigation, not an access control. Any host on
 * the same network could repoint the pool, read the configuration, or
 * start a firmware update. See docs/SECURITY.md finding 1.
 *
 * This implements the endpoint the UI was already written against.
 */

/* Longest accepted password, excluding the terminator. */
#define API_AUTH_MAX_PASSWORD 63

/* Hex characters in an issued session token. */
#define API_AUTH_TOKEN_CHARS 64

void api_auth_init(void);

/* True when no password is configured, in which case the API is open and
 * only the network-range check applies. Reported to the UI so it can warn. */
bool api_auth_is_disabled(void);

/*
 * Gate for a request. Returns ESP_OK when the caller may proceed.
 *
 * On failure this has already sent a 401 response, so the handler must
 * return ESP_OK to the server without writing anything further.
 */
esp_err_t api_auth_require(httpd_req_t *req);

/*
 * Gate for the websocket. A browser WebSocket cannot set an Authorization
 * header, so this also accepts ?token=... from the query string. Sends no 401:
 * the socket is already upgraded by the time a handler runs, so the caller
 * closes the connection instead.
 */
esp_err_t api_auth_require_ws(httpd_req_t *req);

/* POST /api/system/login -- exchange the password for a session token. */
esp_err_t POST_api_login(httpd_req_t *req);

/* Invalidate every issued token. Call after the password changes. */
void api_auth_revoke_all(void);

#endif /* MAIN_HTTP_SERVER_API_AUTH_H */
