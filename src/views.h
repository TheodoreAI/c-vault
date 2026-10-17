#ifndef CVAULT_VIEWS_H
#define CVAULT_VIEWS_H

#include "cgi.h"
#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Render main gallery dashboard with uploader and media grid */
void views_render_gallery(const cgi_request *req, const file_record *files,
                          int file_count, const vault_stats *stats,
                          const char *active_filter, const char *search_query);

/* Render single media view page with video player or high-res photo viewer */
void views_render_view(const cgi_request *req, const file_record *file);

/* Render login PIN entry page */
void views_render_login(const char *error_message);

/* Format bytes to human-readable string (e.g. "14.2 MB", "1.4 GB") */
void views_format_bytes(int64_t bytes, char *out, size_t out_max);

#ifdef __cplusplus
}
#endif

#endif /* CVAULT_VIEWS_H */

