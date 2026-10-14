#ifndef CVAULT_STREAM_H
#define CVAULT_STREAM_H

#include <stddef.h>
#include <stdint.h>
#include "cgi.h"
#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Serves a media file with full HTTP 206 Partial Content (byte-range) support.
 * Essential for iOS Safari video playback and audio scrubbing.
 * If is_download is 1, forces Content-Disposition: attachment.
 */
int stream_serve_file(const cgi_request *req, const file_record *file, int is_download);

#ifdef __cplusplus
}
#endif

#endif /* CVAULT_STREAM_H */

