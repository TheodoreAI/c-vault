#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cgi.h"
#include "db.h"
#include "sha256.h"

int main(void) {
    cgi_request *req = cgi_request_init();
    if (!req) {
        printf("Status: 500 Internal Server Error\r\n\r\nFailed to initialize CGI request.\n");
        return 1;
    }

    if (db_init("data/vault.db") != 0) {
        cgi_send_status(500, "Internal Server Error");
        cgi_send_content_type("text/plain; charset=utf-8");
        cgi_send_headers_end();
        printf("Failed to initialize SQLite database at data/vault.db\n");
        cgi_request_free(req);
        return 1;
    }

    /* Route: /stats or /api/stats (returns JSON metrics) */
    if (strcmp(req->path_info, "/stats") == 0 || strcmp(req->path_info, "/api/stats") == 0) {
        vault_stats stats;
        db_get_stats(&stats);

        cgi_send_status(200, "OK");
        cgi_send_content_type("application/json; charset=utf-8");
        cgi_send_headers_end();

        printf("{\n");
        printf("  \"total_files\": %lld,\n", (long long)stats.total_files);
        printf("  \"total_bytes\": %lld,\n", (long long)stats.total_bytes);
        printf("  \"photos\": %lld,\n", (long long)stats.photos_count);
        printf("  \"videos\": %lld,\n", (long long)stats.videos_count);
        printf("  \"documents\": %lld\n", (long long)stats.docs_count);
        printf("}\n");

        db_close();
        cgi_request_free(req);
        return 0;
    }

    /* Route: /test-seed (seeds a sample file record to verify write pipeline) */
    if (strcmp(req->path_info, "/test-seed") == 0) {
        file_record rec;
        memset(&rec, 0, sizeof(rec));

        db_generate_uuid(rec.uuid);
        snprintf(rec.original_filename, sizeof(rec.original_filename), "IMG_4281.HEIC");
        snprintf(rec.stored_filename, sizeof(rec.stored_filename), "%s.heic", rec.uuid);
        snprintf(rec.file_path, sizeof(rec.file_path), "uploads/2026/10/%s.heic", rec.uuid);
        snprintf(rec.mime_type, sizeof(rec.mime_type), "%s", db_detect_mime_type(rec.original_filename));
        snprintf(rec.media_type, sizeof(rec.media_type), "%s", db_detect_media_type(rec.mime_type, rec.original_filename));
        rec.file_size = 3456789; /* ~3.4 MB */

        /* SHA-256 of dummy content */
        sha256_buffer((const uint8_t *)rec.uuid, strlen(rec.uuid), rec.sha256);
        rec.width = 4032;
        rec.height = 3024;
        rec.is_favorite = 1;

        int rc = db_insert_file(&rec);

        cgi_send_status(200, "OK");
        cgi_send_content_type("application/json; charset=utf-8");
        cgi_send_headers_end();

        if (rc == 0) {
            printf("{\"status\": \"seeded\", \"uuid\": \"%s\", \"filename\": \"%s\", \"mime\": \"%s\"}\n",
                   rec.uuid, rec.original_filename, rec.mime_type);
        } else {
            printf("{\"status\": \"error\", \"message\": \"failed to insert record\"}\n");
        }

        db_close();
        cgi_request_free(req);
        return 0;
    }

    /* Default Dashboard / Diagnostic */
    vault_stats stats;
    db_get_stats(&stats);

    cgi_send_status(200, "OK");
    cgi_send_content_type("text/plain; charset=utf-8");
    cgi_send_header("X-Powered-By", "C-Vault (BCHS C CGI)");
    cgi_send_headers_end();

    printf("=====================================================\n");
    printf("  C-Vault: BCHS Core Diagnostic & Database Status   \n");
    printf("=====================================================\n");
    printf("REQUEST_METHOD : %s\n", req->method);
    printf("PATH_INFO      : %s\n", req->path_info);
    printf("QUERY_STRING   : %s\n", req->query_string);
    printf("CLIENT_IP      : %s\n", req->remote_addr);
    printf("\nParsed Parameters (%d):\n", req->param_count);
    for (int i = 0; i < req->param_count; i++) {
        printf("  - %s = %s\n", req->params[i].key, req->params[i].value);
    }

    printf("\nDatabase Metrics (data/vault.db):\n");
    printf("  SQLite Version : %s\n", sqlite3_libversion());
    printf("  Total Files    : %lld\n", (long long)stats.total_files);
    printf("  Total Storage  : %.2f MB (%lld bytes)\n",
           (double)stats.total_bytes / (1024.0 * 1024.0), (long long)stats.total_bytes);
    printf("  Photos         : %lld\n", (long long)stats.photos_count);
    printf("  Videos         : %lld\n", (long long)stats.videos_count);
    printf("  Documents      : %lld\n", (long long)stats.docs_count);
    printf("=====================================================\n");

    db_close();
    cgi_request_free(req);
    return 0;
}
