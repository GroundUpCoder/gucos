
#pragma once
__require_source("__stdio.c");
__require_source("__posix.c");   /* fopen/freopen call open(); renameat lives there too */
int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
#include <stddef.h>
#include <stdarg.h>
#define NULL ((void *)0)
#define EOF (-1)

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
#define BUFSIZ 1024
#define FOPEN_MAX 64
#define FILENAME_MAX 4096
#define L_tmpnam 20
#define TMP_MAX 10000

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define __F_READ  1
#define __F_WRITE 2
#define __F_APPEND 4
#define __F_EOF   8
#define __F_ERR   16
/* The buffer currently holds data from fread/buffered input.
 * Used to keep fflush from treating a partially-consumed read buffer
 * as pending write data in an r+ / w+ stream. */
#define __F_RBUF  32
/* The buffer was allocated by the library (fclose/setvbuf may free it).
 * Not set for the static stdio buffers or user buffers from setvbuf. */
#define __F_OWNBUF 64
/* The FILE object itself is static (stdin/stdout/stderr): fclose must
 * mark it closed but must not free() it. */
#define __F_STATIC 128

typedef struct FILE {
  int fd;
  int flags;
  int buf_mode;
  char *buf;
  int buf_size;
  int buf_pos;
  int buf_len;
  int ungetc_char;
  /* Cookie-stream seam (ticket #114, funopen/fopencookie-shaped): a stream
   * whose backing store is not an fd sets these; NULL means fd-backed (the
   * default — the static stdio FILEs zero-fill them, fopen/fdopen clear
   * them explicitly). Every buffered-I/O path in __stdio.c dispatches
   * through the seam, never straight to read/write/lseek/close, so cookie
   * streams get buffering, ungetc, scanf, and the printf family for free.
   * io_flush is an extension over the classic four: it runs after buffered
   * data reaches the cookie, letting open_memstream publish its buffer at
   * every fflush as POSIX requires. First consumers: open_memstream and
   * fmemopen. */
  void *cookie;
  long (*io_read)(struct FILE *f, char *buf, long n);
  long (*io_write)(struct FILE *f, const char *buf, long n);
  long long (*io_seek)(struct FILE *f, long long off, int whence);
  int (*io_flush)(struct FILE *f);
  int (*io_close)(struct FILE *f);
} FILE;

/* 64-bit, opaque file position (LFS): fgetpos/fsetpos can address files past
   2 GiB even though the standard fseek/ftell stay long-bounded. */
typedef long long fpos_t;

extern FILE __stdin_file;
extern FILE __stdout_file;
extern FILE __stderr_file;

#define stdin  (&__stdin_file)
#define stdout (&__stdout_file)
#define stderr (&__stderr_file)

int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t size, const char *fmt, ...);
__require_source_if("vsnprintf", "__printf.c");
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int fprintf(FILE *stream, const char *fmt, ...);
int dprintf(int fd, const char *fmt, ...);
int vdprintf(int fd, const char *fmt, va_list ap);
int vfprintf(FILE *stream, const char *fmt, va_list ap);
int vsprintf(char *buf, const char *fmt, va_list ap);
int putchar(int c);
int puts(const char *s);
FILE *fopen(const char *path, const char *mode);
FILE *fdopen(int fd, const char *mode);
int fileno(FILE *stream);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fflush(FILE *stream);
int fputs(const char *s, FILE *stream);
int fputc(int c, FILE *stream);
int fgetc(FILE *stream);
int getc_unlocked(FILE *stream);
void flockfile(FILE *stream);
void funlockfile(FILE *stream);
char *fgets(char *s, int n, FILE *stream);
int ungetc(int c, FILE *stream);

int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);
int fgetpos(FILE *stream, fpos_t *pos);
int fsetpos(FILE *stream, const fpos_t *pos);
/* POSIX off_t-wide seek/tell (>2 GiB safe, like fgetpos/fsetpos). */
typedef long long off_t;
int fseeko(FILE *stream, off_t offset, int whence);
off_t ftello(FILE *stream);

int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);
void setbuf(FILE *stream, char *buf);
void perror(const char *s);
char *gets(char *s);

__require_source_if("__vsscanf_impl", "__scan.c");
int __vsscanf_impl(const char *str, int str_len, const char *fmt,
                    int *consumed, va_list ap);
int vsscanf(const char *s, const char *fmt, va_list ap);
int sscanf(const char *s, const char *fmt, ...);
int vfscanf(FILE *stream, const char *fmt, va_list ap);
int fscanf(FILE *stream, const char *fmt, ...);
int vscanf(const char *fmt, va_list ap);
int scanf(const char *fmt, ...);

__import int remove(const char *path);
__import int rename(const char *oldpath, const char *newpath);

FILE *freopen(const char *path, const char *mode, FILE *stream);
/* Memory-backed streams (POSIX, ticket #114), built on the cookie seam. */
FILE *open_memstream(char **bufp, size_t *sizep);
FILE *fmemopen(void *buf, size_t size, const char *mode);
FILE *tmpfile(void);
char *tmpnam(char *s);
FILE *popen(const char *command, const char *type);
int pclose(FILE *stream);

#define getc(stream)     fgetc(stream)
#define getchar()        fgetc(stdin)
#define putc(c, stream)  fputc(c, stream)
  