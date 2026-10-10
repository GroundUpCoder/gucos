/* /usr/bin/guc-fetch URL FILE: download one URL to a file over the runtime's
 * HTTP transport (the __http_* primitives; the kernel runs the fetch on the
 * page's behalf, so same-origin URLs always work and others need CORS).
 * gucman uses it to pull prebuilt package artifacts from the page's origin
 * (docs/PACKAGES.md). Exit 0 on success, 2 when the server answered with a
 * status other than 200 (a missing artifact), 1 on any other failure.
 * -m MAX_BYTES bounds response bytes and removes an over-limit partial file.
 * -q silences the status message, so a missing artifact is quiet while a
 * transport failure still says what went wrong. -t TYPE requires the
 * response's Content-Type to start with TYPE and exits 3 otherwise: a server
 * that answers every path with its front page must not pass for a tarball. */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/select.h>

__import int __http_open(const char *method, const char *url, const char *headers, const void *body, int blen, int headers_ms, int idle_ms);
__import int __http_status(int fd, int *status_out, char *hdr, int hdrcap);
__import int __http_error(int fd, char *buf, int cap);

static int quiet;
static int secure;
static unsigned long byte_limit=ULONG_MAX;
static const char *expected_type;

/* Header lines arrive as "Name: Value" separated by newlines. */
static int content_type_matches(const char *headers) {
    const char *p = headers;
    while (*p) {
        const char *end = strchr(p, '\n');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len > 13 && strncasecmp(p, "content-type:", 13) == 0) {
            const char *v = p + 13;
            while (*v == ' ' || *v == '\t') v++;
            return strncasecmp(v, expected_type, strlen(expected_type)) == 0;
        }
        if (!end) break;
        p = end + 1;
    }
    return 0;
}

static int fail(int fd, const char *what) {
    char text[512];
    if (fd >= 0 && __http_error(fd, text, sizeof text) > 0) fprintf(stderr, "guc-fetch: %s: %s\n", what, text);
    else fprintf(stderr, "guc-fetch: %s: %s\n", what, strerror(errno));
    if (fd >= 0) close(fd);
    return 1;
}

static void await(int fd) {
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(fd, &readable);
    select(fd + 1, &readable, NULL, NULL, NULL);
}

int main(int argc, char **argv) {
    int arg = 1;
    while (arg < argc && argv[arg][0] == '-') {
        if (strcmp(argv[arg], "-s") == 0) { secure = 1; arg++; }
        else if (strcmp(argv[arg], "-q") == 0) { quiet = 1; arg++; }
        else if (strcmp(argv[arg], "-t") == 0 && arg + 1 < argc) { expected_type = argv[arg + 1]; arg += 2; }
        else if (strcmp(argv[arg], "-m") == 0 && arg + 1 < argc) {
            const char *s=argv[arg+1]; if(!*s){fprintf(stderr,"guc-fetch: invalid byte limit\n");return 1;}
            for(const char *p=s;*p;p++)if(*p<'0'||*p>'9'){fprintf(stderr,"guc-fetch: invalid byte limit\n");return 1;}
            errno=0;char *end;byte_limit=strtoul(s,&end,10);if(errno||*end){fprintf(stderr,"guc-fetch: invalid byte limit\n");return 1;}arg+=2;
        }
        else break;
    }
    if (argc - arg != 2) { fprintf(stderr, "usage: guc-fetch [-q] [-t CONTENT-TYPE] [-m MAX_BYTES] URL FILE\n"); return 1; }
    const char *url = argv[arg], *path = argv[arg + 1];
    int fd = __http_open("GET", url, "", NULL, 0, 30000, 30000);
    if (fd < 0) return fail(-1, url);
    int status = 0;
    static char headers[16384];
    for (;;) {
        await(fd);
        if (__http_status(fd, &status, headers, sizeof headers - 1) >= 0) break;
        if (errno != EAGAIN) return fail(fd, url);
    }
    headers[sizeof headers - 1] = 0;
    if (status != 200) {
        if (!quiet) fprintf(stderr, "guc-fetch: %s: HTTP %d\n", url, status);
        close(fd);
        return 2;
    }
    if (expected_type && !content_type_matches(headers)) {
        if (!quiet) fprintf(stderr, "guc-fetch: %s: not %s\n", url, expected_type);
        close(fd);
        return 3;
    }
    if (secure) {
        const char *final = strstr(headers, "x-guc-final-url: ");
        if (!final) { close(fd); fputs("guc-fetch: missing final URL\n", stderr); return 1; }
        final += 17;
        size_t length = strcspn(final, "\r\n");
        if ((strncmp(url, "https://", 8) == 0 && strncmp(final, "https://", 8) != 0) ||
            memchr(final, '@', length) || memchr(final, '#', length)) {
            close(fd); fputs("guc-fetch: unsafe redirect\n", stderr); return 1;
        }
    }
    FILE *out = fopen(path, "wb");
    if (!out) { fprintf(stderr, "guc-fetch: %s: %s\n", path, strerror(errno)); close(fd); return 1; }
    static char buffer[65536];
    unsigned long received=0;
    for (;;) {
        ssize_t n = read(fd, buffer, sizeof buffer);
        if (n > 0) {
            if ((unsigned long)n > byte_limit-received) {fprintf(stderr,"guc-fetch: response exceeds byte limit\n");fclose(out);close(fd);unlink(path);return 1;}
            received+=(unsigned long)n;
            if (fwrite(buffer, 1, (size_t)n, out) != (size_t)n) { fprintf(stderr, "guc-fetch: %s: %s\n", path, strerror(errno)); fclose(out); close(fd); return 1; }
            continue;
        }
        if (n == 0) break;
        if (errno == EAGAIN) { await(fd); continue; }
        fclose(out);
        unlink(path);
        return fail(fd, url);
    }
    close(fd);
    if (fclose(out) != 0) { fprintf(stderr, "guc-fetch: %s: %s\n", path, strerror(errno)); return 1; }
    return 0;
}
