#include "cgi.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

char *cgi_url_decode(const char *src) {
    if (!src) return NULL;
    size_t src_len = strlen(src);
    char *dst = (char *)malloc(src_len + 1);
    if (!dst) return NULL;

    size_t si = 0, di = 0;
    while (si < src_len) {
        if (src[si] == '+') {
            dst[di++] = ' ';
            si++;
        } else if (src[si] == '%' && si + 2 < src_len) {
            int h1 = hex_val(src[si + 1]);
            int h2 = hex_val(src[si + 2]);
            if (h1 >= 0 && h2 >= 0) {
                dst[di++] = (char)((h1 << 4) | h2);
                si += 3;
            } else {
                dst[di++] = src[si++];
            }
        } else {
            dst[di++] = src[si++];
        }
    }
    dst[di] = '\0';
    return dst;
}

char *cgi_html_escape(const char *src) {
    if (!src) return safe_strdup("");

    size_t len = 0;
    for (const char *p = src; *p; p++) {
        switch (*p) {
            case '&':  len += 5; break; /* &amp; */
            case '<':  len += 4; break; /* &lt; */
            case '>':  len += 4; break; /* &gt; */
            case '"':  len += 6; break; /* &quot; */
            case '\'': len += 5; break; /* &#39; */
            default:   len += 1; break;
        }
    }

    char *dst = (char *)malloc(len + 1);
    if (!dst) return NULL;

    char *out = dst;
    for (const char *p = src; *p; p++) {
        switch (*p) {
            case '&':  memcpy(out, "&amp;", 5);  out += 5; break;
            case '<':  memcpy(out, "&lt;", 4);   out += 4; break;
            case '>':  memcpy(out, "&gt;", 4);   out += 4; break;
            case '"':  memcpy(out, "&quot;", 6); out += 6; break;
            case '\'': memcpy(out, "&#39;", 5);  out += 5; break;
            default:   *out++ = *p; break;
        }
    }
    *out = '\0';
    return dst;
}

static void parse_query_params(cgi_request *req, const char *query) {
    if (!query || !*query) return;

    char *qcopy = safe_strdup(query);
    if (!qcopy) return;

    char *token = qcopy;
    while (token && req->param_count < CGI_MAX_PARAMS) {
        char *next = strchr(token, '&');
        if (next) {
            *next = '\0';
            next++;
        }

        char *eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            req->params[req->param_count].key = cgi_url_decode(token);
            req->params[req->param_count].value = cgi_url_decode(eq + 1);
        } else {
            req->params[req->param_count].key = cgi_url_decode(token);
            req->params[req->param_count].value = safe_strdup("");
        }
        req->param_count++;
        token = next;
    }
    free(qcopy);
}

static void parse_cookies(cgi_request *req, const char *cookie_str) {
    if (!cookie_str || !*cookie_str) return;

    char *ccopy = safe_strdup(cookie_str);
    if (!ccopy) return;

    char *token = ccopy;
    while (token && req->cookie_count < CGI_MAX_COOKIES) {
        char *next = strchr(token, ';');
        if (next) {
            *next = '\0';
            next++;
        }

        /* Trim leading whitespace */
        while (*token == ' ' || *token == '\t') token++;

        char *eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            char *val = eq + 1;
            while (*val == ' ' || *val == '\t') val++;
            req->cookies[req->cookie_count].key = safe_strdup(token);
            req->cookies[req->cookie_count].value = safe_strdup(val);
            req->cookie_count++;
        }
        token = next;
    }
    free(ccopy);
}

cgi_request *cgi_request_init(void) {
    cgi_request *req = (cgi_request *)calloc(1, sizeof(cgi_request));
    if (!req) return NULL;

    const char *env_val;

    env_val = getenv("REQUEST_METHOD");
    req->method = safe_strdup(env_val ? env_val : "GET");

    env_val = getenv("PATH_INFO");
    if (env_val && *env_val) {
        req->path_info = safe_strdup(env_val);
    } else {
        /* Fallback: extract path from REQUEST_URI */
        const char *uri = getenv("REQUEST_URI");
        if (uri) {
            const char *q = strchr(uri, '?');
            if (q) {
                size_t path_len = (size_t)(q - uri);
                req->path_info = (char *)malloc(path_len + 1);
                if (req->path_info) {
                    memcpy(req->path_info, uri, path_len);
                    req->path_info[path_len] = '\0';
                }
            } else {
                req->path_info = safe_strdup(uri);
            }
        } else {
            req->path_info = safe_strdup("/");
        }
    }

    env_val = getenv("QUERY_STRING");
    req->query_string = safe_strdup(env_val ? env_val : "");

    env_val = getenv("CONTENT_TYPE");
    req->content_type = safe_strdup(env_val ? env_val : "");

    env_val = getenv("CONTENT_LENGTH");
    req->content_length = env_val ? (size_t)strtoull(env_val, NULL, 10) : 0;

    env_val = getenv("HTTP_COOKIE");
    req->cookie_header = safe_strdup(env_val ? env_val : "");

    env_val = getenv("HTTP_AUTHORIZATION");
    req->auth_header = safe_strdup(env_val ? env_val : "");

    env_val = getenv("HTTP_RANGE");
    req->range_header = safe_strdup(env_val ? env_val : "");

    env_val = getenv("REMOTE_ADDR");
    req->remote_addr = safe_strdup(env_val ? env_val : "127.0.0.1");

    /* Parse query string and cookies into key-value pairs */
    parse_query_params(req, req->query_string);
    parse_cookies(req, req->cookie_header);

    return req;
}

void cgi_request_free(cgi_request *req) {
    if (!req) return;

    free(req->method);
    free(req->path_info);
    free(req->query_string);
    free(req->content_type);
    free(req->cookie_header);
    free(req->auth_header);
    free(req->range_header);
    free(req->remote_addr);

    for (int i = 0; i < req->param_count; i++) {
        free(req->params[i].key);
        free(req->params[i].value);
    }

    for (int i = 0; i < req->cookie_count; i++) {
        free(req->cookies[i].key);
        free(req->cookies[i].value);
    }

    free(req);
}

const char *cgi_param(const cgi_request *req, const char *key) {
    if (!req || !key) return NULL;
    for (int i = 0; i < req->param_count; i++) {
        if (strcmp(req->params[i].key, key) == 0) {
            return req->params[i].value;
        }
    }
    return NULL;
}

const char *cgi_cookie(const cgi_request *req, const char *name) {
    if (!req || !name) return NULL;
    for (int i = 0; i < req->cookie_count; i++) {
        if (strcmp(req->cookies[i].key, name) == 0) {
            return req->cookies[i].value;
        }
    }
    return NULL;
}

/* HTTP Response Output Helpers */

void cgi_send_status(int code, const char *status_text) {
    printf("Status: %d %s\r\n", code, status_text ? status_text : "OK");
}

void cgi_send_header(const char *name, const char *value) {
    if (name && value) {
        printf("%s: %s\r\n", name, value);
    }
}

void cgi_send_content_type(const char *content_type) {
    if (content_type) {
        printf("Content-Type: %s\r\n", content_type);
    }
}

void cgi_send_redirect(const char *location) {
    cgi_send_status(303, "See Other");
    cgi_send_header("Location", location);
    cgi_send_header("Content-Length", "0");
    cgi_send_headers_end();
}

void cgi_send_cookie(const char *name, const char *value, int max_age_seconds,
                     int http_only, int secure, const char *same_site) {
    if (!name || !value) return;

    printf("Set-Cookie: %s=%s; Path=/; Max-Age=%d", name, value, max_age_seconds);
    if (http_only) printf("; HttpOnly");
    if (secure) printf("; Secure");
    if (same_site && *same_site) printf("; SameSite=%s", same_site);
    printf("\r\n");
}

void cgi_send_headers_end(void) {
    printf("\r\n");
    fflush(stdout);
}
