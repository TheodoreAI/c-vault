#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#else
#define MKDIR(d) mkdir(d, 0755)
#endif

static sqlite3 *g_db = NULL;

static void ensure_parent_dir(const char *path) {
    char buf[512];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *slash = strrchr(buf, '/');
    if (!slash) {
        slash = strrchr(buf, '\\');
    }
    if (slash) {
        *slash = '\0';
        MKDIR(buf);
    }
}

int db_init(const char *db_path) {
    if (g_db) return 0; /* Already initialized */

    if (!db_path || !*db_path) {
        db_path = CVAULT_DEFAULT_DB_PATH;
    }

    ensure_parent_dir(db_path);

    int rc = sqlite3_open_v2(
        db_path, &g_db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        NULL
    );

    if (rc != SQLITE_OK) {
        fprintf(stderr, "[db_init] sqlite3_open error: %s\n", sqlite3_errmsg(g_db));
        if (g_db) {
            sqlite3_close(g_db);
            g_db = NULL;
        }
        return -1;
    }

    /* Set recommended high-performance WAL pragmas */
    char *err_msg = NULL;
    const char *pragmas =
        "PRAGMA journal_mode = WAL;"
        "PRAGMA busy_timeout = 5000;"
        "PRAGMA foreign_keys = ON;"
        "PRAGMA synchronous = NORMAL;";

    rc = sqlite3_exec(g_db, pragmas, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[db_init] pragma error: %s\n", err_msg ? err_msg : "unknown");
        sqlite3_free(err_msg);
    }

    /* Schema initialization */
    const char *ddl =
        "CREATE TABLE IF NOT EXISTS files ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    uuid TEXT NOT NULL UNIQUE,"
        "    original_filename TEXT NOT NULL,"
        "    stored_filename TEXT NOT NULL,"
        "    file_path TEXT NOT NULL,"
        "    mime_type TEXT NOT NULL,"
        "    file_size INTEGER NOT NULL,"
        "    sha256 TEXT NOT NULL,"
        "    media_type TEXT NOT NULL,"
        "    width INTEGER DEFAULT 0,"
        "    height INTEGER DEFAULT 0,"
        "    duration INTEGER DEFAULT 0,"
        "    is_favorite INTEGER DEFAULT 0,"
        "    is_deleted INTEGER DEFAULT 0,"
        "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_files_sha256 ON files(sha256);"
        "CREATE INDEX IF NOT EXISTS idx_files_media_type ON files(media_type);"
        "CREATE INDEX IF NOT EXISTS idx_files_created_at ON files(created_at);"
        "CREATE INDEX IF NOT EXISTS idx_files_is_deleted ON files(is_deleted);"
        "CREATE TABLE IF NOT EXISTS tags ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name TEXT NOT NULL UNIQUE"
        ");"
        "CREATE TABLE IF NOT EXISTS file_tags ("
        "    file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,"
        "    tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,"
        "    PRIMARY KEY(file_id, tag_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS sessions ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    token TEXT NOT NULL UNIQUE,"
        "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "    expires_at TIMESTAMP NOT NULL"
        ");"
        "CREATE VIRTUAL TABLE IF NOT EXISTS files_fts USING fts5("
        "    original_filename,"
        "    media_type,"
        "    content='files',"
        "    content_rowid='id'"
        ");";

    rc = sqlite3_exec(g_db, ddl, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[db_init] DDL schema error: %s\n", err_msg ? err_msg : "unknown");
        sqlite3_free(err_msg);
        return -1;
    }

    return 0;
}

void db_close(void) {
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}

sqlite3 *db_get_handle(void) {
    return g_db;
}

static void fill_file_record(sqlite3_stmt *stmt, file_record *out) {
    out->id = sqlite3_column_int64(stmt, 0);
    snprintf(out->uuid, sizeof(out->uuid), "%s", (const char *)sqlite3_column_text(stmt, 1));
    snprintf(out->original_filename, sizeof(out->original_filename), "%s", (const char *)sqlite3_column_text(stmt, 2));
    snprintf(out->stored_filename, sizeof(out->stored_filename), "%s", (const char *)sqlite3_column_text(stmt, 3));
    snprintf(out->file_path, sizeof(out->file_path), "%s", (const char *)sqlite3_column_text(stmt, 4));
    snprintf(out->mime_type, sizeof(out->mime_type), "%s", (const char *)sqlite3_column_text(stmt, 5));
    out->file_size = sqlite3_column_int64(stmt, 6);
    snprintf(out->sha256, sizeof(out->sha256), "%s", (const char *)sqlite3_column_text(stmt, 7));
    snprintf(out->media_type, sizeof(out->media_type), "%s", (const char *)sqlite3_column_text(stmt, 8));
    out->width = sqlite3_column_int(stmt, 9);
    out->height = sqlite3_column_int(stmt, 10);
    out->duration = sqlite3_column_int(stmt, 11);
    out->is_favorite = sqlite3_column_int(stmt, 12);
    out->is_deleted = sqlite3_column_int(stmt, 13);
    snprintf(out->created_at, sizeof(out->created_at), "%s", (const char *)sqlite3_column_text(stmt, 14));
    snprintf(out->uploaded_at, sizeof(out->uploaded_at), "%s", (const char *)sqlite3_column_text(stmt, 15));
}

int db_insert_file(const file_record *file) {
    if (!g_db || !file) return -1;

    const char *sql =
        "INSERT INTO files ("
        "    uuid, original_filename, stored_filename, file_path, mime_type, "
        "    file_size, sha256, media_type, width, height, duration, is_favorite, is_deleted"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[db_insert_file] prepare error: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, file->uuid, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, file->original_filename, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, file->stored_filename, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, file->file_path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, file->mime_type, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, file->file_size);
    sqlite3_bind_text(stmt, 7, file->sha256, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, file->media_type, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 9, file->width);
    sqlite3_bind_int(stmt, 10, file->height);
    sqlite3_bind_int(stmt, 11, file->duration);
    sqlite3_bind_int(stmt, 12, file->is_favorite);
    sqlite3_bind_int(stmt, 13, file->is_deleted);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[db_insert_file] step error: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }

    int64_t last_id = sqlite3_last_insert_rowid(g_db);

    /* Sync to FTS5 index */
    const char *fts_sql = "INSERT INTO files_fts(rowid, original_filename, media_type) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(g_db, fts_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, last_id);
        sqlite3_bind_text(stmt, 2, file->original_filename, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, file->media_type, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    return 0;
}

int db_find_file_by_uuid(const char *uuid, file_record *out_file) {
    if (!g_db || !uuid || !out_file) return -1;

    const char *sql =
        "SELECT id, uuid, original_filename, stored_filename, file_path, mime_type, "
        "       file_size, sha256, media_type, width, height, duration, is_favorite, "
        "       is_deleted, created_at, uploaded_at "
        "FROM files WHERE uuid = ? LIMIT 1;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, uuid, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    int found = 0;
    if (rc == SQLITE_ROW) {
        fill_file_record(stmt, out_file);
        found = 1;
    }

    sqlite3_finalize(stmt);
    return found ? 0 : -1;
}

int db_find_file_by_sha256(const char *sha256, file_record *out_file) {
    if (!g_db || !sha256 || !out_file) return -1;

    const char *sql =
        "SELECT id, uuid, original_filename, stored_filename, file_path, mime_type, "
        "       file_size, sha256, media_type, width, height, duration, is_favorite, "
        "       is_deleted, created_at, uploaded_at "
        "FROM files WHERE sha256 = ? AND is_deleted = 0 LIMIT 1;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, sha256, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    int found = 0;
    if (rc == SQLITE_ROW) {
        fill_file_record(stmt, out_file);
        found = 1;
    }

    sqlite3_finalize(stmt);
    return found ? 0 : -1;
}

int db_list_files(const char *media_type, int is_deleted, int only_favorites,
                  int limit, int offset, file_record **out_files, int *out_count) {
    if (!g_db || !out_files || !out_count) return -1;

    *out_files = NULL;
    *out_count = 0;

    if (limit <= 0) limit = 50;
    if (offset < 0) offset = 0;

    char query[512];
    snprintf(query, sizeof(query),
        "SELECT id, uuid, original_filename, stored_filename, file_path, mime_type, "
        "       file_size, sha256, media_type, width, height, duration, is_favorite, "
        "       is_deleted, created_at, uploaded_at "
        "FROM files WHERE is_deleted = %d "
        "%s%s%s "
        "%s "
        "ORDER BY uploaded_at DESC LIMIT ? OFFSET ?;",
        is_deleted ? 1 : 0,
        (media_type && *media_type) ? "AND media_type = '" : "",
        (media_type && *media_type) ? media_type : "",
        (media_type && *media_type) ? "'" : "",
        only_favorites ? "AND is_favorite = 1" : ""
    );

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[db_list_files] prepare error: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, limit);
    sqlite3_bind_int(stmt, 2, offset);

    int capacity = 32;
    file_record *list = (file_record *)malloc(capacity * sizeof(file_record));
    if (!list) {
        sqlite3_finalize(stmt);
        return -1;
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            file_record *new_list = (file_record *)realloc(list, capacity * sizeof(file_record));
            if (!new_list) {
                free(list);
                sqlite3_finalize(stmt);
                return -1;
            }
            list = new_list;
        }
        fill_file_record(stmt, &list[count]);
        count++;
    }

    sqlite3_finalize(stmt);

    *out_files = list;
    *out_count = count;
    return 0;
}

int db_search_files(const char *search_query, file_record **out_files, int *out_count) {
    if (!g_db || !search_query || !out_files || !out_count) return -1;

    *out_files = NULL;
    *out_count = 0;

    const char *sql =
        "SELECT f.id, f.uuid, f.original_filename, f.stored_filename, f.file_path, f.mime_type, "
        "       f.file_size, f.sha256, f.media_type, f.width, f.height, f.duration, f.is_favorite, "
        "       f.is_deleted, f.created_at, f.uploaded_at "
        "FROM files f "
        "WHERE f.is_deleted = 0 AND f.original_filename LIKE ? "
        "ORDER BY f.uploaded_at DESC LIMIT 50;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    char like_pattern[256];
    snprintf(like_pattern, sizeof(like_pattern), "%%%s%%", search_query);
    sqlite3_bind_text(stmt, 1, like_pattern, -1, SQLITE_STATIC);

    int capacity = 32;
    file_record *list = (file_record *)malloc(capacity * sizeof(file_record));
    if (!list) {
        sqlite3_finalize(stmt);
        return -1;
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            file_record *new_list = (file_record *)realloc(list, capacity * sizeof(file_record));
            if (!new_list) {
                free(list);
                sqlite3_finalize(stmt);
                return -1;
            }
            list = new_list;
        }
        fill_file_record(stmt, &list[count]);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_files = list;
    *out_count = count;
    return 0;
}

void db_free_file_list(file_record *files) {
    if (files) {
        free(files);
    }
}

int db_soft_delete_file(const char *uuid) {
    if (!g_db || !uuid) return -1;

    const char *sql = "UPDATE files SET is_deleted = 1 WHERE uuid = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, uuid, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_restore_file(const char *uuid) {
    if (!g_db || !uuid) return -1;

    const char *sql = "UPDATE files SET is_deleted = 0 WHERE uuid = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, uuid, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_purge_file(const char *uuid, char *out_file_path, size_t path_max) {
    if (!g_db || !uuid) return -1;

    file_record rec;
    if (db_find_file_by_uuid(uuid, &rec) != 0) {
        return -1;
    }

    if (out_file_path && path_max > 0) {
        strncpy(out_file_path, rec.file_path, path_max - 1);
        out_file_path[path_max - 1] = '\0';
    }

    const char *sql = "DELETE FROM files WHERE uuid = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, uuid, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_toggle_favorite(const char *uuid) {
    if (!g_db || !uuid) return -1;

    const char *sql = "UPDATE files SET is_favorite = (1 - is_favorite) WHERE uuid = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, uuid, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_stats(vault_stats *stats) {
    if (!g_db || !stats) return -1;
    memset(stats, 0, sizeof(vault_stats));

    const char *sql =
        "SELECT COUNT(*), COALESCE(SUM(file_size), 0), "
        "       COUNT(CASE WHEN media_type = 'photo' THEN 1 END), "
        "       COUNT(CASE WHEN media_type = 'video' THEN 1 END), "
        "       COUNT(CASE WHEN media_type = 'document' THEN 1 END) "
        "FROM files WHERE is_deleted = 0;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats->total_files  = sqlite3_column_int64(stmt, 0);
        stats->total_bytes  = sqlite3_column_int64(stmt, 1);
        stats->photos_count = sqlite3_column_int64(stmt, 2);
        stats->videos_count = sqlite3_column_int64(stmt, 3);
        stats->docs_count   = sqlite3_column_int64(stmt, 4);
    }

    sqlite3_finalize(stmt);
    return 0;
}

/* Media & MIME Helpers */

static int strcasecmp_custom(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        if (diff != 0) return diff;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

const char *db_detect_mime_type(const char *filename) {
    if (!filename) return "application/octet-stream";

    const char *dot = strrchr(filename, '.');
    if (!dot) return "application/octet-stream";

    /* iPhone & standard image formats */
    if (strcasecmp_custom(dot, ".heic") == 0) return "image/heic";
    if (strcasecmp_custom(dot, ".heif") == 0) return "image/heif";
    if (strcasecmp_custom(dot, ".jpg") == 0 || strcasecmp_custom(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcasecmp_custom(dot, ".png") == 0)  return "image/png";
    if (strcasecmp_custom(dot, ".gif") == 0)  return "image/gif";
    if (strcasecmp_custom(dot, ".webp") == 0) return "image/webp";
    if (strcasecmp_custom(dot, ".dng") == 0)  return "image/x-adobe-dng";
    if (strcasecmp_custom(dot, ".svg") == 0)  return "image/svg+xml";

    /* Video formats */
    if (strcasecmp_custom(dot, ".mov") == 0)  return "video/quicktime";
    if (strcasecmp_custom(dot, ".mp4") == 0)  return "video/mp4";
    if (strcasecmp_custom(dot, ".m4v") == 0)  return "video/x-m4v";
    if (strcasecmp_custom(dot, ".webm") == 0) return "video/webm";
    if (strcasecmp_custom(dot, ".mkv") == 0)  return "video/x-matroska";

    /* Audio formats */
    if (strcasecmp_custom(dot, ".m4a") == 0)  return "audio/mp4";
    if (strcasecmp_custom(dot, ".mp3") == 0)  return "audio/mpeg";
    if (strcasecmp_custom(dot, ".wav") == 0)  return "audio/wav";

    /* Documents */
    if (strcasecmp_custom(dot, ".pdf") == 0)  return "application/pdf";
    if (strcasecmp_custom(dot, ".txt") == 0)  return "text/plain";
    if (strcasecmp_custom(dot, ".md") == 0)   return "text/markdown";
    if (strcasecmp_custom(dot, ".json") == 0) return "application/json";
    if (strcasecmp_custom(dot, ".zip") == 0)  return "application/zip";

    return "application/octet-stream";
}

const char *db_detect_media_type(const char *mime_type, const char *filename) {
    if (mime_type) {
        if (strncmp(mime_type, "image/", 6) == 0) return "photo";
        if (strncmp(mime_type, "video/", 6) == 0) return "video";
        if (strncmp(mime_type, "audio/", 6) == 0) return "audio";
        if (strcmp(mime_type, "application/pdf") == 0 ||
            strncmp(mime_type, "text/", 5) == 0) {
            return "document";
        }
    }

    if (filename) {
        const char *dot = strrchr(filename, '.');
        if (dot) {
            if (strcasecmp_custom(dot, ".heic") == 0 || strcasecmp_custom(dot, ".heif") == 0 ||
                strcasecmp_custom(dot, ".jpg") == 0 || strcasecmp_custom(dot, ".png") == 0 ||
                strcasecmp_custom(dot, ".dng") == 0) {
                return "photo";
            }
            if (strcasecmp_custom(dot, ".mov") == 0 || strcasecmp_custom(dot, ".mp4") == 0) {
                return "video";
            }
            if (strcasecmp_custom(dot, ".pdf") == 0 || strcasecmp_custom(dot, ".txt") == 0) {
                return "document";
            }
        }
    }

    return "other";
}

void db_generate_uuid(char uuid_out[37]) {
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL) ^ (unsigned int)clock());
        seeded = 1;
    }

    uint8_t bytes[16];
    for (int i = 0; i < 16; i++) {
        bytes[i] = (uint8_t)(rand() & 0xFF);
    }

    /* Set version 4 (0100) and variant (10xx) */
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    snprintf(uuid_out, 37,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5],
        bytes[6], bytes[7],
        bytes[8], bytes[9],
        bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]
    );
}
