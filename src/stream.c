#include "stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define SET_BINARY_MODE(handle) _setmode(_fileno(handle), _O_BINARY)
#define FSEEK(stream, offset, whence) _fseeki64(stream, offset, whence)
#define FTELL(stream) _ftelli64(stream)
#else
#define SET_BINARY_MODE(handle) ((void)0)
#define FSEEK(stream, offset, whence) fseeko(stream, offset, whence)
#define FTELL(stream) ftello(stream)
#endif

#define STREAM_CHUNK_SIZE 65536

static int parse_range(const char *range_str, int64_t total_size,
                       int64_t *out_start, int64_t *out_end) {
    if (!range_str || strncmp(range_str, "bytes=", 6) != 0) {
        return 0;
    }

    const char *spec = range_str + 6;
    const char *dash = strchr(spec, '-');
    if (!dash) return 0;

    int64_t start = 0;
    int64_t end = total_size - 1;

    if (dash == spec) {
        /* Suffix byte range: e.g. bytes=-500 (last 500 bytes) */
        int64_t suffix = (int64_t)strtoll(dash + 1, NULL, 10);
        if (suffix <= 0) return 0;
        start = total_size - suffix;
        if (start < 0) start = 0;
    } else {
        start = (int64_t)strtoll(spec, NULL, 10);
        if (*(dash + 1) != '\0') {
            end = (int64_t)strtoll(dash + 1, NULL, 10);
        }
    }

    if (start < 0 || start >= total_size || end < start || end >= total_size) {
        return -1; /* Unsatisfiable range */
    }

    *out_start = start;
    *out_end = end;
    return 1;
}

int stream_serve_file(const cgi_request *req, const file_record *file, int is_download) {
    if (!file) return -1;

    FILE *f = fopen(file->file_path, "rb");
    if (!f) {
        cgi_send_status(404, "Not Found");
        cgi_send_content_type("text/plain; charset=utf-8");
        cgi_send_headers_end();
        printf("Error: Physical file not found on disk: %s\n", file->file_path);
        return -1;
    }

    FSEEK(f, 0, SEEK_END);
    int64_t total_size = FTELL(f);
    FSEEK(f, 0, SEEK_SET);

    int64_t start = 0;
    int64_t end = total_size > 0 ? total_size - 1 : 0;
    int is_range = 0;

    if (req->range_header && *req->range_header) {
        int r = parse_range(req->range_header, total_size, &start, &end);
        if (r == -1) {
            /* 416 Range Not Satisfiable */
            fclose(f);
            cgi_send_status(416, "Range Not Satisfiable");
            char cr[64];
            snprintf(cr, sizeof(cr), "bytes */%lld", (long long)total_size);
            cgi_send_header("Content-Range", cr);
            cgi_send_headers_end();
            return 0;
        } else if (r == 1) {
            is_range = 1;
        }
    }

    int64_t bytes_to_send = (total_size > 0) ? (end - start + 1) : 0;

    /* Emit response headers */
    if (is_range) {
        cgi_send_status(206, "Partial Content");
        char cr_buf[128];
        snprintf(cr_buf, sizeof(cr_buf), "bytes %lld-%lld/%lld",
                 (long long)start, (long long)end, (long long)total_size);
        cgi_send_header("Content-Range", cr_buf);
    } else {
        cgi_send_status(200, "OK");
    }

    cgi_send_content_type(file->mime_type[0] ? file->mime_type : "application/octet-stream");
    cgi_send_header("Accept-Ranges", "bytes");

    char cl_buf[64];
    snprintf(cl_buf, sizeof(cl_buf), "%lld", (long long)bytes_to_send);
    cgi_send_header("Content-Length", cl_buf);

    if (is_download) {
        char cd_buf[384];
        snprintf(cd_buf, sizeof(cd_buf), "attachment; filename=\"%s\"", file->original_filename);
        cgi_send_header("Content-Disposition", cd_buf);
    } else {
        char cd_buf[384];
        snprintf(cd_buf, sizeof(cd_buf), "inline; filename=\"%s\"", file->original_filename);
        cgi_send_header("Content-Disposition", cd_buf);
    }

    cgi_send_header("Cache-Control", "public, max-age=86400");
    cgi_send_headers_end();

    /* Switch stdout to binary mode for raw payload */
    SET_BINARY_MODE(stdout);

    if (start > 0) {
        FSEEK(f, start, SEEK_SET);
    }

    uint8_t buffer[STREAM_CHUNK_SIZE];
    int64_t remaining = bytes_to_send;

    while (remaining > 0) {
        size_t chunk_want = (size_t)(remaining > STREAM_CHUNK_SIZE ? STREAM_CHUNK_SIZE : remaining);
        size_t bytes_read = fread(buffer, 1, chunk_want, f);
        if (bytes_read == 0) break;

        size_t written = fwrite(buffer, 1, bytes_read, stdout);
        if (written == 0) break;

        remaining -= written;
    }

    fflush(stdout);
    fclose(f);
    return 0;
}
