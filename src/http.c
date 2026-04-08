/*
 * http.c  —  HTTP engine using libcurl
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "bad.h"
#include "bad_platform.h"

typedef struct {
    char  *data;
    size_t len;
} MemBuf;

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    MemBuf *buf = (MemBuf *)userdata;
    size_t total = size * nmemb;
    char *tmp = realloc(buf->data, buf->len + total + 1);
    if (!tmp) return 0;
    buf->data = tmp;
    memcpy(buf->data + buf->len, ptr, total);
    buf->len += total;
    buf->data[buf->len] = '\0';
    return total;
}

Response http_request(const char *method, const char *url,
                      const char *body_json,
                      const char **hdr_keys,
                      const char **hdr_vals,
                      int          hdr_count,
                      int          timeout_ms) {
    Response resp = {0, NULL, 0};

    CURL *curl = curl_easy_init();
    if (!curl) {
        resp.status = -1;
        resp.body   = BAD_STRDUP("{\"error\":\"curl init failed\"}");
        return resp;
    }

    MemBuf buf = {NULL, 0};
    buf.data = malloc(1);
    buf.data[0] = '\0';

    struct curl_slist *hdrs = NULL;

    /* Default JSON content type if body present */
    if (body_json) {
        hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    }
    /* User headers */
    for (int i = 0; i < hdr_count; i++) {
        char line[512];
        snprintf(line, sizeof line, "%s: %s", hdr_keys[i], hdr_vals[i]);
        hdrs = curl_slist_append(hdrs, line);
    }

    curl_easy_setopt(curl, CURLOPT_URL,            url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &buf);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     hdrs);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,     (long)timeout_ms);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    /* Method */
    if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (body_json) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_json);
        } else {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
        }
    } else if (strcmp(method, "PUT") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        if (body_json) curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_json);
    } else if (strcmp(method, "PATCH") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        if (body_json) curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_json);
    } else if (strcmp(method, "DELETE") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else {
        /* GET is default */
    }

    long t0 = bad_monotonic_ms();
    CURLcode rc = curl_easy_perform(curl);
    long t1 = bad_monotonic_ms();

    resp.time_ms = t1 - t0;
    if (resp.time_ms < 0) resp.time_ms = 0;

    if (rc != CURLE_OK) {
        resp.status = -1;
        char errbuf[256];
        snprintf(errbuf, sizeof errbuf,
                 "{\"error\":\"%s\"}", curl_easy_strerror(rc));
        resp.body = BAD_STRDUP(errbuf);
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        resp.status = (int)http_code;
        resp.body   = buf.data;
        buf.data    = NULL; /* transferred */
    }

    free(buf.data);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);
    return resp;
}

void response_free(Response *r) {
    if (r) { free(r->body); r->body = NULL; }
}
