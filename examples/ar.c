/* /usr/bin/ar: static archives for the object linker (docs/OBJECTS.md).
 * The format is the common "!<arch>" layout with GNU long names, so archives
 * built here are readable by other tools. No symbol index is written: the
 * linker scans every member. Operations: r (add or replace), q (append),
 * t (list), x (extract), d (delete); modifiers c, s, u and v are accepted. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct member { char *name; unsigned char *data; long size; };
static struct member *members;
static int count;
static size_t capacity;

static void fail(const char *what, const char *path) {
    fprintf(stderr, "ar: %s: %s\n", what, path);
    exit(1);
}

static void reserve_member(const char *archive) {
    if ((size_t)count < capacity) return;
    size_t next = capacity ? capacity * 2 : 64;
    if (next < capacity || next > SIZE_MAX / sizeof *members)
        fail("archive too large", archive);
    struct member *grown = realloc(members, next * sizeof *members);
    if (!grown) fail("out of memory", archive);
    members = grown;
    capacity = next;
}

static unsigned char *read_file(const char *path, long *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = malloc(*size ? (size_t)*size : 1);
    if (fread(data, 1, (size_t)*size, f) != (size_t)*size) fail("short read", path);
    fclose(f);
    return data;
}

static const char *basename_of(const char *path) {
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}

static void load(const char *archive) {
    long size;
    unsigned char *data = read_file(archive, &size);
    if (!data) return;
    if (size < 8 || memcmp(data, "!<arch>\n", 8)) fail("not an archive", archive);
    char *longnames = NULL;
    long p = 8;
    while (p + 60 <= size) {
        char header[61];
        memcpy(header, data + p, 60);
        header[60] = 0;
        long msize = atol(header + 48);
        char name[17];
        memcpy(name, header, 16);
        name[16] = 0;
        for (int i = 15; i >= 0 && name[i] == ' '; i--) name[i] = 0;
        unsigned char *body = data + p + 60;
        p += 60 + msize + (msize & 1);
        if (!strcmp(name, "//")) {
            longnames = malloc((size_t)msize + 1);
            memcpy(longnames, body, (size_t)msize);
            longnames[msize] = 0;
            continue;
        }
        if (!strcmp(name, "/") || !strcmp(name, "/SYM64/")) continue;
        char *mname;
        if (name[0] == '/' && longnames) {
            char *start = longnames + atol(name + 1);
            char *end = strstr(start, "/\n");
            size_t n = end ? (size_t)(end - start) : strlen(start);
            mname = malloc(n + 1);
            memcpy(mname, start, n);
            mname[n] = 0;
        } else {
            size_t n = strlen(name);
            if (n && name[n - 1] == '/') name[n - 1] = 0;
            mname = strdup(name);
        }
        reserve_member(archive);
        members[count].name = mname;
        members[count].data = body;
        members[count].size = msize;
        count++;
    }
}

static void save(const char *archive) {
    FILE *f = fopen(archive, "wb");
    if (!f) fail("cannot write", archive);
    fputs("!<arch>\n", f);
    long longlen = 0;
    for (int i = 0; i < count; i++) if (strlen(members[i].name) > 15) longlen += strlen(members[i].name) + 2;
    if (longlen) {
        fprintf(f, "%-16s%-12s%-6s%-6s%-8s%-10ld`\n", "//", "0", "0", "0", "0", longlen);
        for (int i = 0; i < count; i++) if (strlen(members[i].name) > 15) fprintf(f, "%s/\n", members[i].name);
        if (longlen & 1) fputc('\n', f);
    }
    long offset = 0;
    for (int i = 0; i < count; i++) {
        char field[32];
        if (strlen(members[i].name) > 15) {
            snprintf(field, sizeof field, "/%ld", offset);
            offset += strlen(members[i].name) + 2;
        } else {
            snprintf(field, sizeof field, "%s/", members[i].name);
        }
        fprintf(f, "%-16s%-12s%-6s%-6s%-8s%-10ld`\n", field, "0", "0", "0", "644", members[i].size);
        fwrite(members[i].data, 1, (size_t)members[i].size, f);
        if (members[i].size & 1) fputc('\n', f);
    }
    fclose(f);
}

static int find(const char *name) {
    for (int i = 0; i < count; i++) if (!strcmp(members[i].name, name)) return i;
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: ar [-]{rqtxd}[csuv] archive [member...]\n");
        return 1;
    }
    const char *ops = argv[1];
    if (ops[0] == '-') ops++;
    char op = 0;
    int verbose = 0;
    for (const char *c = ops; *c; c++) {
        if (strchr("rqtxd", *c)) op = *c;
        else if (*c == 'v') verbose = 1;
        else if (!strchr("csuU", *c)) { fprintf(stderr, "ar: unknown option %c\n", *c); return 1; }
    }
    if (!op) { fprintf(stderr, "ar: no operation specified\n"); return 1; }
    const char *archive = argv[2];
    load(archive);
    if (op == 't') {
        for (int i = 0; i < count; i++) {
            int wanted = argc == 3;
            for (int a = 3; a < argc && !wanted; a++) wanted = !strcmp(basename_of(argv[a]), members[i].name);
            if (wanted) printf("%s\n", members[i].name);
        }
        return 0;
    }
    if (op == 'x') {
        for (int i = 0; i < count; i++) {
            int wanted = argc == 3;
            for (int a = 3; a < argc && !wanted; a++) wanted = !strcmp(basename_of(argv[a]), members[i].name);
            if (!wanted) continue;
            FILE *f = fopen(members[i].name, "wb");
            if (!f) fail("cannot write", members[i].name);
            fwrite(members[i].data, 1, (size_t)members[i].size, f);
            fclose(f);
            if (verbose) printf("x - %s\n", members[i].name);
        }
        return 0;
    }
    if (op == 'd') {
        for (int a = 3; a < argc; a++) {
            int i = find(basename_of(argv[a]));
            if (i < 0) continue;
            memmove(members + i, members + i + 1, (size_t)(count - i - 1) * sizeof members[0]);
            count--;
            if (verbose) printf("d - %s\n", argv[a]);
        }
        save(archive);
        return 0;
    }
    for (int a = 3; a < argc; a++) {
        long size;
        unsigned char *data = read_file(argv[a], &size);
        if (!data) fail("No such file or directory", argv[a]);
        const char *name = basename_of(argv[a]);
        int i = op == 'r' ? find(name) : -1;
        if (i < 0) {
            reserve_member(archive);
            i = count++;
            members[i].name = strdup(name);
            if (verbose) printf("a - %s\n", name);
        } else if (verbose) {
            printf("r - %s\n", name);
        }
        members[i].data = data;
        members[i].size = size;
    }
    save(archive);
    return 0;
}
