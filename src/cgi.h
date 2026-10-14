#ifndef CVAULT_CGI_H
#define CVAULT_CGI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CGI_MAX_PARAMS 64
#define CGI_MAX_COOKIES 32

typedef struct {
    char *key;
    char *value;
} cgi_kv_pair;

typedef struct {
    char *method;          /* REQUEST_METHOD: "GET", "POST", etc. */
    char *path_info;       /* PATH_INFO: e.g. "/view/abc-123" */
    char *query_string;    /* QUERY_STRING */
    char *content_type;    /* CONTENT_TYPE */
    size_t content_length; /* CONTENT_LENGTH */
    char *cookie_header;   /* HTTP_COOKIE */
    char *auth_header;     /* HTTP_AUTHORIZATION */
    char *range_header;    /* HTTP_RANGE: e.g. "bytes=0-1048575" */
    char *remote_addr;     /* REMOTE_ADDR */

    /* Parsed query parameters */
    cgi_kv_pair params[CGI_MAX_PARAMS];
    int param_count;

    /* Parsed cookies */
    cgi_kv_pair cookies[CGI_MAX_COOKIES];
    int cookie_count;
} cgi_request;

/* Request Lifecycle */
cgi_request *cgi_request_init(void);
void cgi_request_free(cgi_request *req);

/* Query & Cookie Lookup */
const char *cgi_param(const cgi_request *req, const char *key);
const char *cgi_cookie(const cgi_request *req, const char *name);

/* String Utilities */
char *cgi_url_decode(const char *src);
char *cgi_html_escape(const char *src);

/* HTTP Response Output Helpers (writes directly to stdout) */
void cgi_send_status(int code, const char *status_text);
void cgi_send_header(const char *name, const char *value);
void cgi_send_content_type(const char *content_type);
void cgi_send_redirect(const char *location);
void cgi_send_cookie(const char *name, const char *value, int max_age_seconds,
                     int http_only, int secure, const char *same_site);
void cgi_send_headers_end(void);

#ifdef __cplusplus
}
#endif

#endif /* CVAULT_CGI_H */
