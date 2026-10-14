#ifndef CVAULT_AUTH_H
#define CVAULT_AUTH_H

#include "cgi.h"
#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CVAULT_SESSION_COOKIE "cvault_session"
#define CVAULT_DEFAULT_PIN "123456"

/*
 * Checks if the request is authenticated via session cookie or Authorization header.
 * Returns 1 if authenticated, 0 if unauthenticated.
 */
int auth_check_request(const cgi_request *req);

/*
 * Verifies a PIN or passphrase submission.
 * If valid, creates a session token in SQLite and returns 1 with token_out filled.
 * Returns 0 if invalid.
 */
int auth_verify_pin(const char *submitted_pin, char token_out[65]);

/* Destroys an existing session (logout) */
int auth_destroy_session(const char *token);

/* Retrieves configured PIN/passphrase from environment or default */
const char *auth_get_configured_pin(void);

#ifdef __cplusplus
}
#endif

#endif /* CVAULT_AUTH_H */
