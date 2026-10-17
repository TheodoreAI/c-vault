#ifndef CVAULT_UPLOAD_H
#define CVAULT_UPLOAD_H

#include <stddef.h>
#include <stdint.h>
#include "cgi.h"
#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char uuid[37];
    char original_filename[256];
    char file_path[512];
    char mime_type[64];
    int64_t file_size;
    char sha256[65];
    int is_deduplicated;
    int success;
    char error_message[128];
} upload_result;

#define MAX_UPLOAD_BATCH 64

typedef struct {
    upload_result files[MAX_UPLOAD_BATCH];
    int file_count;
    int total_success;
    int total_deduplicated;
    int64_t total_bytes_saved;
} upload_batch;

/*
 * Parses a streaming multipart/form-data upload directly from stdin.
 * Streams file content to disk in 64 KB chunks without loading files into RAM.
 * Calculates SHA-256 in-flight, performs deduplication, and records in SQLite.
 */
int upload_process_multipart(const cgi_request *req, upload_batch *batch);

/* Ensures upload directories (uploads/YYYY/MM and uploads/temp) exist */
int upload_ensure_dirs(char *out_current_dir, size_t dir_max);

#ifdef __cplusplus
}
#endif

#endif /* CVAULT_UPLOAD_H */

