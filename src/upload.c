#include "upload.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#define SET_BINARY_MODE(handle) _setmode(_fileno(handle), _O_BINARY)
#else
#include <unistd.h>
#include <sys/stat.h>
#define MKDIR(d) mkdir(d, 0755)
#define SET_BINARY_MODE(handle) ((void)0)
#endif

#define CHUNK_SIZE 1048576

static void make_path_dirs(const char *path) {
    char temp[512];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    for (char *p = temp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char slash = *p;
            *p = '\0';
            MKDIR(temp);
            *p = slash;
        }
    }
    MKDIR(temp);
}

int upload_ensure_dirs(char *out_current_dir, size_t dir_max) {
    make_path_dirs("uploads/temp");

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char dir[128];
    if (t) {
        snprintf(dir, sizeof(dir), "uploads/%04d/%02d", t->tm_year + 1900, t->tm_mon + 1);
    } else {
        snprintf(dir, sizeof(dir), "uploads/archive");
    }
    make_path_dirs(dir);

    if (out_current_dir && dir_max > 0) {
        strncpy(out_current_dir, dir, dir_max - 1);
        out_current_dir[dir_max - 1] = '\0';
    }
    return 0;
}

static char *extract_boundary(const char *content_type) {
    if (!content_type) return NULL;
    const char *b = strstr(content_type, "boundary=");
    if (!b) return NULL;
    b += 9;

    if (*b == '"') {
        b++;
        const char *end = strchr(b, '"');
        if (!end) return NULL;
        size_t len = (size_t)(end - b);
        char *boundary = (char *)malloc(len + 1);
        if (boundary) {
            memcpy(boundary, b, len);
            boundary[len] = '\0';
        }
        return boundary;
    } else {
        const char *end = b;
        while (*end && *end != ';' && *end != ' ' && *end != '\r' && *end != '\n') {
            end++;
        }
        size_t len = (size_t)(end - b);
        char *boundary = (char *)malloc(len + 1);
        if (boundary) {
            memcpy(boundary, b, len);
            boundary[len] = '\0';
        }
        return boundary;
    }
}

static void extract_header_filename(const char *header_line, char *filename, size_t max_len) {
    filename[0] = '\0';
    const char *fn = strstr(header_line, "filename=");
    if (!fn) return;
    fn += 9;

    if (*fn == '"') {
        fn++;
        const char *end = strchr(fn, '"');
        if (end) {
            size_t len = (size_t)(end - fn);
            if (len >= max_len) len = max_len - 1;
            memcpy(filename, fn, len);
            filename[len] = '\0';
        }
    } else {
        const char *end = fn;
        while (*end && *end != ';' && *end != '\r' && *end != '\n' && *end != ' ') end++;
        size_t len = (size_t)(end - fn);
        if (len >= max_len) len = max_len - 1;
        memcpy(filename, fn, len);
        filename[len] = '\0';
    }

    /* Strip any path components from filename to prevent path traversal */
    char *slash1 = strrchr(filename, '/');
    char *slash2 = strrchr(filename, '\\');
    char *safe_name = slash1 > slash2 ? slash1 : slash2;
    if (safe_name) {
        memmove(filename, safe_name + 1, strlen(safe_name + 1) + 1);
    }
}

static int memsearch(const uint8_t *haystack, size_t haystack_len,
                     const uint8_t *needle, size_t needle_len) {
    if (needle_len == 0 || haystack_len < needle_len) return -1;
    const uint8_t first = needle[0];
    const uint8_t *cur = haystack;
    const uint8_t *end = haystack + (haystack_len - needle_len);
    while (cur <= end) {
        const uint8_t *match = (const uint8_t *)memchr(cur, first, (size_t)(end - cur + 1));
        if (!match) return -1;
        if (memcmp(match, needle, needle_len) == 0) {
            return (int)(match - haystack);
        }
        cur = match + 1;
    }
    return -1;
}

int upload_process_multipart(const cgi_request *req, upload_batch *batch) {
    if (!req || !batch) return -1;
    memset(batch, 0, sizeof(upload_batch));

    if (req->content_length == 0) return 0;

    char *boundary = extract_boundary(req->content_type);
    if (!boundary) {
        return -1;
    }

    SET_BINARY_MODE(stdin);
    setvbuf(stdin, NULL, _IOFBF, 262144);

    char delimiter[256];
    snprintf(delimiter, sizeof(delimiter), "\r\n--%s", boundary);
    size_t delim_len = strlen(delimiter);

    char start_delimiter[256];
    snprintf(start_delimiter, sizeof(start_delimiter), "--%s", boundary);
    size_t start_delim_len = strlen(start_delimiter);

    uint8_t *buf = (uint8_t *)malloc(CHUNK_SIZE * 2);
    if (!buf) {
        free(boundary);
        return -1;
    }

    size_t bytes_remaining = req->content_length;
    size_t buf_len = 0;

    /* Read initial chunk */
    size_t to_read = bytes_remaining > CHUNK_SIZE ? CHUNK_SIZE : bytes_remaining;
    size_t n = fread(buf, 1, to_read, stdin);
    buf_len = n;
    bytes_remaining -= n;

    /* Find opening boundary */
    int start_pos = memsearch(buf, buf_len, (const uint8_t *)start_delimiter, start_delim_len);
    if (start_pos < 0) {
        free(buf);
        free(boundary);
        return -1;
    }

    size_t cursor = start_pos + start_delim_len;

    while (1) {
        /* Check if we reached the closing boundary: "--boundary--" */
        while (cursor + 4 > buf_len && bytes_remaining > 0) {
            size_t want = bytes_remaining > CHUNK_SIZE ? CHUNK_SIZE : bytes_remaining;
            size_t r = fread(buf + buf_len, 1, want, stdin);
            if (r == 0) break;
            buf_len += r;
            bytes_remaining -= r;
        }

        if (cursor + 2 <= buf_len && buf[cursor] == '-' && buf[cursor + 1] == '-') {
            /* End of multipart body */
            break;
        }

        /* Skip CRLF after boundary */
        if (cursor + 2 <= buf_len && buf[cursor] == '\r' && buf[cursor + 1] == '\n') {
            cursor += 2;
        }

        /* Parse Part Headers until "\r\n\r\n" */
        int header_end = -1;
        while (1) {
            header_end = memsearch(buf + cursor, buf_len - cursor, (const uint8_t *)"\r\n\r\n", 4);
            if (header_end >= 0) break;
            if (bytes_remaining == 0) break;

            /* Slide unread window to front if buffer is filling */
            if (cursor > 0) {
                memmove(buf, buf + cursor, buf_len - cursor);
                buf_len -= cursor;
                cursor = 0;
            }

            size_t want = bytes_remaining > CHUNK_SIZE ? CHUNK_SIZE : bytes_remaining;
            size_t r = fread(buf + buf_len, 1, want, stdin);
            if (r == 0) break;
            buf_len += r;
            bytes_remaining -= r;
        }

        if (header_end < 0) break;

        size_t h_len = header_end;
        char *header_block = (char *)malloc(h_len + 1);
        if (!header_block) break;
        memcpy(header_block, buf + cursor, h_len);
        header_block[h_len] = '\0';
        cursor += h_len + 4; /* skip past \r\n\r\n */

        char original_filename[256];
        extract_header_filename(header_block, original_filename, sizeof(original_filename));
        free(header_block);

        /* Only process if it has a non-empty filename */
        if (original_filename[0] != '\0' && batch->file_count < MAX_UPLOAD_BATCH) {
            upload_result *res = &batch->files[batch->file_count];
            memset(res, 0, sizeof(upload_result));
            snprintf(res->original_filename, sizeof(res->original_filename), "%s", original_filename);
            db_generate_uuid(res->uuid);

            char temp_path[512];
            snprintf(temp_path, sizeof(temp_path), "uploads/temp/%s.tmp", res->uuid);

            FILE *out_f = fopen(temp_path, "wb");
            if (!out_f) {
                snprintf(res->error_message, sizeof(res->error_message), "Could not open temp file for writing");
                batch->file_count++;
                continue;
            }
            setvbuf(out_f, NULL, _IOFBF, 262144);

            sha256_ctx sha_ctx;
            sha256_init(&sha_ctx);
            int64_t file_bytes = 0;

            /* Stream payload to disk until next boundary delimiter ("\r\n--boundary") */
            int found_delim = 0;
            while (!found_delim) {
                int delim_pos = memsearch(buf + cursor, buf_len - cursor, (const uint8_t *)delimiter, delim_len);
                if (delim_pos >= 0) {
                    /* Write payload up to delimiter */
                    if (delim_pos > 0) {
                        fwrite(buf + cursor, 1, delim_pos, out_f);
                        sha256_update(&sha_ctx, buf + cursor, delim_pos);
                        file_bytes += delim_pos;
                    }
                    cursor += delim_pos + 2; /* Skip \r\n, leave cursor at "--boundary" */
                    found_delim = 1;
                } else {
                    /* Write safe bytes leaving delim_len in buffer to avoid splitting delimiter */
                    size_t available = buf_len - cursor;
                    if (available > delim_len) {
                        size_t safe_write = available - delim_len;
                        fwrite(buf + cursor, 1, safe_write, out_f);
                        sha256_update(&sha_ctx, buf + cursor, safe_write);
                        file_bytes += safe_write;
                        cursor += safe_write;
                    }

                    if (bytes_remaining == 0) {
                        /* End of input */
                        break;
                    }

                    /* Shift remaining bytes to start of buffer and read more */
                    if (cursor > 0) {
                        memmove(buf, buf + cursor, buf_len - cursor);
                        buf_len -= cursor;
                        cursor = 0;
                    }

                    size_t want = bytes_remaining > CHUNK_SIZE ? CHUNK_SIZE : bytes_remaining;
                    size_t r = fread(buf + buf_len, 1, want, stdin);
                    buf_len += r;
                    bytes_remaining -= r;
                    if (r == 0 && buf_len == 0) break;
                }
            }

            fclose(out_f);

            uint8_t hash_raw[32];
            sha256_final(&sha_ctx, hash_raw);
            sha256_hash_to_hex(hash_raw, res->sha256);
            res->file_size = file_bytes;

            /* Detect MIME and Media Type */
            const char *mime = db_detect_mime_type(res->original_filename);
            const char *media = db_detect_media_type(mime, res->original_filename);
            snprintf(res->mime_type, sizeof(res->mime_type), "%s", mime);

            /* Deduplication check in SQLite */
            file_record existing;
            if (db_find_file_by_sha256(res->sha256, &existing) == 0) {
                /* Duplicate detected: remove temp file and reference existing stored path */
                remove(temp_path);
                snprintf(res->file_path, sizeof(res->file_path), "%s", existing.file_path);
                res->is_deduplicated = 1;
                batch->total_deduplicated++;
                batch->total_bytes_saved += res->file_size;
            } else {
                /* New unique file: move to uploads/YYYY/MM/<sha256>.<ext> */
                char current_dir[128];
                upload_ensure_dirs(current_dir, sizeof(current_dir));

                const char *dot = strrchr(res->original_filename, '.');
                char ext[16] = "";
                if (dot) {
                    snprintf(ext, sizeof(ext), "%s", dot);
                }

                char final_path[512];
                snprintf(final_path, sizeof(final_path), "%s/%s%s", current_dir, res->sha256, ext);

                remove(final_path); /* Remove if orphaned exists */
                if (rename(temp_path, final_path) != 0) {
                    /* Fallback copy if cross-device rename */
                    FILE *src = fopen(temp_path, "rb");
                    FILE *dst = fopen(final_path, "wb");
                    if (src && dst) {
                        uint8_t cbuf[65536];
                        size_t cr;
                        while ((cr = fread(cbuf, 1, sizeof(cbuf), src)) > 0) {
                            fwrite(cbuf, 1, cr, dst);
                        }
                    }
                    if (src) fclose(src);
                    if (dst) fclose(dst);
                    remove(temp_path);
                }

                snprintf(res->file_path, sizeof(res->file_path), "%s", final_path);
                res->is_deduplicated = 0;
            }

            /* Record file in SQLite database */
            file_record record;
            memset(&record, 0, sizeof(record));
            snprintf(record.uuid, sizeof(record.uuid), "%s", res->uuid);
            snprintf(record.original_filename, sizeof(record.original_filename), "%s", res->original_filename);
            snprintf(record.stored_filename, sizeof(record.stored_filename), "%s", strrchr(res->file_path, '/') ? strrchr(res->file_path, '/') + 1 : res->file_path);
            snprintf(record.file_path, sizeof(record.file_path), "%s", res->file_path);
            snprintf(record.mime_type, sizeof(record.mime_type), "%s", res->mime_type);
            snprintf(record.media_type, sizeof(record.media_type), "%s", media);
            record.file_size = res->file_size;
            snprintf(record.sha256, sizeof(record.sha256), "%s", res->sha256);

            if (db_insert_file(&record) == 0) {
                res->success = 1;
                batch->total_success++;
            } else {
                res->success = 0;
                snprintf(res->error_message, sizeof(res->error_message), "Database insert failed");
            }

            batch->file_count++;
        } else {
            /* Non-file form field or empty file: skip until next delimiter */
            while (1) {
                int delim_pos = memsearch(buf + cursor, buf_len - cursor, (const uint8_t *)delimiter, delim_len);
                if (delim_pos >= 0) {
                    cursor += delim_pos + 2;
                    break;
                }
                if (bytes_remaining == 0) break;
                if (cursor > 0) {
                    memmove(buf, buf + cursor, buf_len - cursor);
                    buf_len -= cursor;
                    cursor = 0;
                }
                size_t want = bytes_remaining > CHUNK_SIZE ? CHUNK_SIZE : bytes_remaining;
                size_t r = fread(buf + buf_len, 1, want, stdin);
                buf_len += r;
                bytes_remaining -= r;
                if (r == 0 && buf_len == 0) break;
            }
        }
    }

    free(buf);
    free(boundary);
    return 0;
}
