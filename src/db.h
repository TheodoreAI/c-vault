#ifndef CVAULT_DB_H
#define CVAULT_DB_H

#include <stddef.h>
#include <stdint.h>
#include "sqlite3.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CVAULT_DEFAULT_DB_PATH "data/vault.db"

typedef struct {
    int64_t id;
    char uuid[37];
    char original_filename[256];
    char stored_filename[256];
    char file_path[512];
    char mime_type[64];
    int64_t file_size;
    char sha256[65];
    char media_type[32]; /* "photo", "video", "document", "other" */
    int width;
    int height;
    int duration;
    int is_favorite;
    int is_deleted;
    char created_at[32];
    char uploaded_at[32];
} file_record;

typedef struct {
    int64_t total_files;
    int64_t total_bytes;
    int64_t photos_count;
    int64_t videos_count;
    int64_t docs_count;
} vault_stats;

/* Database Connection & Initialization */
int db_init(const char *db_path);
void db_close(void);
sqlite3 *db_get_handle(void);

/* File Operations */
int db_insert_file(const file_record *file);
int db_find_file_by_uuid(const char *uuid, file_record *out_file);
int db_find_file_by_sha256(const char *sha256, file_record *out_file);
int db_list_files(const char *media_type, int is_deleted, int only_favorites,
                  int limit, int offset, file_record **out_files, int *out_count);
int db_search_files(const char *search_query, file_record **out_files, int *out_count);
void db_free_file_list(file_record *files);

/* State Modifications */
int db_soft_delete_file(const char *uuid);
int db_restore_file(const char *uuid);
int db_purge_file(const char *uuid, char *out_file_path, size_t path_max);
int db_toggle_favorite(const char *uuid);

/* Metrics & Stats */
int db_get_stats(vault_stats *stats);

/* Media & MIME Helpers */
const char *db_detect_mime_type(const char *filename);
const char *db_detect_media_type(const char *mime_type, const char *filename);
void db_generate_uuid(char uuid_out[37]);

#ifdef __cplusplus
}
#endif

#endif /* CVAULT_DB_H */
