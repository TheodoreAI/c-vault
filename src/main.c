#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"
#include "sha256.h"

int main(void) {
    /* Standard CGI HTTP Header */
    printf("Status: 200 OK\r\n");
    printf("Content-Type: text/plain; charset=utf-8\r\n");
    printf("X-Powered-By: C-Vault (BCHS C CGI)\r\n");
    printf("\r\n");

    /* Diagnostic output */
    printf("C-Vault Core Diagnostic\n");
    printf("=======================\n");
    printf("SQLite Version : %s (source ID: %s)\n", sqlite3_libversion(), sqlite3_sourceid());

    /* Test SHA-256 */
    char test_hash[65];
    sha256_buffer((const uint8_t *)"c-vault", 7, test_hash);
    printf("SHA-256 Test   : sha256('c-vault') = %s\n", test_hash);

    /* Environment check */
    const char *method = getenv("REQUEST_METHOD");
    const char *uri = getenv("REQUEST_URI");
    printf("REQUEST_METHOD : %s\n", method ? method : "(null/CLI mode)");
    printf("REQUEST_URI    : %s\n", uri ? uri : "(null/CLI mode)");
    printf("\nC-Vault binary compiled and initialized successfully.\n");

    return 0;
}

