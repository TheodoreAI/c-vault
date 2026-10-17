#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cgi.h"
#include "db.h"
#include "upload.h"
#include "stream.h"
#include "auth.h"
#include "views.h"

static void handle_login_post(const cgi_request *req) {
    /* Read body to extract submitted PIN */
    char pin[64] = {0};
    if (req->content_length > 0) {
        char *body = (char *)malloc(req->content_length + 1);
        if (body) {
            size_t r = fread(body, 1, req->content_length, stdin);
            body[r] = '\0';

            char *p = strstr(body, "pin=");
            if (p) {
                p += 4;
                char *end = strchr(p, '&');
                if (end) *end = '\0';
                char *decoded = cgi_url_decode(p);
                if (decoded) {
                    snprintf(pin, sizeof(pin), "%s", decoded);
                    free(decoded);
                }
            }
            free(body);
        }
    }

    char token[65];
    if (auth_verify_pin(pin, token)) {
        cgi_send_status(303, "See Other");
        cgi_send_header("Location", "/");
        cgi_send_cookie(CVAULT_SESSION_COOKIE, token, 30 * 86400, 1, 0, "Lax");
        cgi_send_headers_end();
    } else {
        cgi_send_status(200, "OK");
        cgi_send_content_type("text/html; charset=utf-8");
        cgi_send_headers_end();
        views_render_login("Incorrect PIN. Please try again.");
    }
}

static void handle_logout(const cgi_request *req) {
    const char *token = cgi_cookie(req, CVAULT_SESSION_COOKIE);
    if (token) {
        auth_destroy_session(token);
    }
    cgi_send_status(303, "See Other");
    cgi_send_header("Location", "/login");
    cgi_send_cookie(CVAULT_SESSION_COOKIE, "", 0, 1, 0, "Lax");
    cgi_send_headers_end();
}

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

    /* Public route: /login */
    if (strcmp(req->path_info, "/login") == 0) {
        if (strcmp(req->method, "POST") == 0) {
            handle_login_post(req);
        } else {
            if (auth_check_request(req)) {
                cgi_send_redirect("/");
            } else {
                cgi_send_status(200, "OK");
                cgi_send_content_type("text/html; charset=utf-8");
                cgi_send_headers_end();
                views_render_login(NULL);
            }
        }
        db_close();
        cgi_request_free(req);
        return 0;
    }

    /* Public route: /logout */
    if (strcmp(req->path_info, "/logout") == 0) {
        handle_logout(req);
        db_close();
        cgi_request_free(req);
        return 0;
    }

    /* Authenticate all remaining routes */
    if (!auth_check_request(req)) {
        if (strncmp(req->path_info, "/api/", 5) == 0) {
            cgi_send_status(401, "Unauthorized");
            cgi_send_content_type("application/json; charset=utf-8");
            cgi_send_headers_end();
            printf("{\"error\": \"Unauthorized. Provide Bearer token or login.\"}\n");
        } else {
            cgi_send_redirect("/login");
        }
        db_close();
        cgi_request_free(req);
        return 0;
    }

    /* API Metrics: /stats or /api/stats */
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

    /* Upload routes: /upload (Web) and /api/upload (iOS Shortcut) */
    if ((strcmp(req->path_info, "/upload") == 0 || strcmp(req->path_info, "/api/upload") == 0) &&
        strcmp(req->method, "POST") == 0) {
        upload_batch batch;
        upload_process_multipart(req, &batch);

        int is_api = (strcmp(req->path_info, "/api/upload") == 0) ||
                     (req->content_type && strstr(req->content_type, "multipart/form-data") &&
                      req->auth_header && *req->auth_header);

        if (is_api) {
            cgi_send_status(200, "OK");
            cgi_send_content_type("application/json; charset=utf-8");
            cgi_send_headers_end();

            printf("{\n");
            printf("  \"status\": \"success\",\n");
            printf("  \"uploaded_count\": %d,\n", batch.total_success);
            printf("  \"deduplicated_count\": %d,\n", batch.total_deduplicated);
            printf("  \"bytes_saved\": %lld,\n", (long long)batch.total_bytes_saved);
            printf("  \"files\": [\n");
            for (int i = 0; i < batch.file_count; i++) {
                upload_result *f = &batch.files[i];
                printf("    {\"uuid\": \"%s\", \"filename\": \"%s\", \"size\": %lld, \"mime\": \"%s\", \"deduped\": %s}%s\n",
                       f->uuid, f->original_filename, (long long)f->file_size,
                       f->mime_type, f->is_deduplicated ? "true" : "false",
                       (i + 1 < batch.file_count) ? "," : "");
            }
            printf("  ]\n");
            printf("}\n");
        } else {
            /* Web browser upload response: redirect to gallery */
            cgi_send_redirect("/");
        }

        db_close();
        cgi_request_free(req);
        return 0;
    }

    /* Raw streaming route: /raw/:uuid */
    if (strncmp(req->path_info, "/raw/", 5) == 0) {
        const char *uuid = req->path_info + 5;
        file_record file;
        if (db_find_file_by_uuid(uuid, &file) == 0) {
            stream_serve_file(req, &file, 0);
        } else {
            cgi_send_status(404, "Not Found");
            cgi_send_content_type("text/plain; charset=utf-8");
            cgi_send_headers_end();
            printf("File not found: %s\n", uuid);
        }
        db_close();
        cgi_request_free(req);
        return 0;
    }

    /* Direct download route: /download/:uuid */
    if (strncmp(req->path_info, "/download/", 10) == 0) {
        const char *uuid = req->path_info + 10;
        file_record file;
        if (db_find_file_by_uuid(uuid, &file) == 0) {
            stream_serve_file(req, &file, 1);
        } else {
            cgi_send_status(404, "Not Found");
            cgi_send_content_type("text/plain; charset=utf-8");
            cgi_send_headers_end();
            printf("File not found: %s\n", uuid);
        }
        db_close();
        cgi_request_free(req);
        return 0;
    }

    /* Single file detail view: /view/:uuid */
    if (strncmp(req->path_info, "/view/", 6) == 0) {
        const char *uuid = req->path_info + 6;
        file_record file;
        if (db_find_file_by_uuid(uuid, &file) == 0) {
            cgi_send_status(200, "OK");
            cgi_send_content_type("text/html; charset=utf-8");
            cgi_send_headers_end();
            views_render_view(req, &file);
        } else {
            cgi_send_status(404, "Not Found");
            cgi_send_content_type("text/plain; charset=utf-8");
            cgi_send_headers_end();
            printf("Media not found: %s\n", uuid);
        }
        db_close();
        cgi_request_free(req);
        return 0;
    }

    /* Action: /delete/:uuid */
    if (strncmp(req->path_info, "/delete/", 8) == 0 && strcmp(req->method, "POST") == 0) {
        const char *uuid = req->path_info + 8;
        db_soft_delete_file(uuid);
        cgi_send_redirect("/");
        db_close();
        cgi_request_free(req);
        return 0;
    }

    /* Action: /restore/:uuid */
    if (strncmp(req->path_info, "/restore/", 9) == 0 && strcmp(req->method, "POST") == 0) {
        const char *uuid = req->path_info + 9;
        db_restore_file(uuid);
        cgi_send_redirect("/?filter=trash");
        db_close();
        cgi_request_free(req);
        return 0;
    }

    /* Action: /favorite/:uuid */
    if (strncmp(req->path_info, "/favorite/", 10) == 0 && strcmp(req->method, "POST") == 0) {
        const char *uuid = req->path_info + 10;
        db_toggle_favorite(uuid);
        cgi_send_redirect("/");
        db_close();
        cgi_request_free(req);
        return 0;
    }

    /* Default: Main Gallery Dashboard */
    const char *filter = cgi_param(req, "filter");
    const char *q = cgi_param(req, "q");

    file_record *files = NULL;
    int file_count = 0;

    if (q && *q) {
        db_search_files(q, &files, &file_count);
    } else {
        int is_trash = (filter && strcmp(filter, "trash") == 0);
        int only_fav = (filter && strcmp(filter, "favorites") == 0);
        const char *media_type = NULL;
        if (filter && (strcmp(filter, "photo") == 0 ||
                       strcmp(filter, "video") == 0 ||
                       strcmp(filter, "document") == 0)) {
            media_type = filter;
        }
        db_list_files(media_type, is_trash, only_fav, 100, 0, &files, &file_count);
    }

    vault_stats stats;
    db_get_stats(&stats);

    cgi_send_status(200, "OK");
    cgi_send_content_type("text/html; charset=utf-8");
    cgi_send_headers_end();

    views_render_gallery(req, files, file_count, &stats, filter ? filter : "all", q);

    if (files) {
        db_free_file_list(files);
    }

    db_close();
    cgi_request_free(req);
    return 0;
}
