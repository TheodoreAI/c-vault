#include "auth.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char *auth_get_configured_pin(void) {
    const char *env_pin = getenv("CVAULT_PIN");
    if (env_pin && *env_pin) {
        return env_pin;
    }
    return CVAULT_DEFAULT_PIN;
}

static void generate_token(char hex[65]) {
    uint8_t raw[32];
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL) ^ (unsigned int)clock());
        seeded = 1;
    }

    for (int i = 0; i < 32; i++) {
        raw[i] = (uint8_t)(rand() & 0xFF);
    }
    sha256_hash_to_hex(raw, hex);
}

int auth_check_request(const cgi_request *req) {
    if (!req) return 0;

    /* If auth disabled explicitly via environment, allow access */
    const char *disabled = getenv("CVAULT_DISABLE_AUTH");
    if (disabled && strcmp(disabled, "1") == 0) {
        return 1;
    }

    const char *token = NULL;

    /* Check Authorization header (for iOS Shortcuts and API clients) */
    if (req->auth_header && *req->auth_header) {
        if (strncmp(req->auth_header, "Bearer ", 7) == 0) {
            token = req->auth_header + 7;
        } else {
            token = req->auth_header;
        }
    }

    /* Check cookie */
    if (!token || !*token) {
        token = cgi_cookie(req, CVAULT_SESSION_COOKIE);
    }

    if (!token || !*token) {
        return 0;
    }

    sqlite3 *db = db_get_handle();
    if (!db) return 0;

    const char *sql =
        "SELECT id FROM sessions WHERE token = ? AND datetime('now') < expires_at LIMIT 1;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    int is_valid = (rc == SQLITE_ROW);
    sqlite3_finalize(stmt);

    return is_valid;
}

int auth_verify_pin(const char *submitted_pin, char token_out[65]) {
    if (!submitted_pin || !token_out) return 0;

    const char *configured_pin = auth_get_configured_pin();
    if (strcmp(submitted_pin, configured_pin) != 0) {
        return 0;
    }

    generate_token(token_out);

    sqlite3 *db = db_get_handle();
    if (!db) return 0;

    /* 30-day session lifetime */
    const char *sql =
        "INSERT INTO sessions (token, expires_at) "
        "VALUES (?, datetime('now', '+30 days'));";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, token_out, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 1 : 0;
}

int auth_destroy_session(const char *token) {
    if (!token || !*token) return 0;

    sqlite3 *db = db_get_handle();
    if (!db) return 0;

    const char *sql = "DELETE FROM sessions WHERE token = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

