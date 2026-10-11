
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <spawn.h>      // popen() spawns /bin/sh
#include <sys/wait.h>   // pclose() reaps the child
#include <__thread.h>
/* One recursive lock serializes the buffered stdio entry points across threads. */
static __lock_t __stdio_lock = __LOCK_INIT;
void __stdio_fork_lock(int acquire) { if (acquire) __lock_acquire(&__stdio_lock); else __lock_release(&__stdio_lock); }
int __freading(FILE *f) { return (f->flags & __F_RBUF) != 0 || ((f->flags & __F_READ) && !(f->flags & __F_WRITE)); }
int __fwriting(FILE *f) { return (f->flags & __F_WRITE) && !(f->flags & __F_RBUF); }
size_t __freadahead(FILE *f) { return (f->flags & __F_RBUF ? f->buf_len - f->buf_pos : 0) + (f->ungetc_char != EOF); }
size_t __fpending(FILE *f) { return __fwriting(f) ? f->buf_pos : 0; }
void __fpurge(FILE *f) { f->buf_pos = f->buf_len = 0; f->ungetc_char = EOF; }
static int __fflush_unlocked(FILE *stream);
static size_t __fwrite_unlocked(const void *ptr, size_t size, size_t nmemb, FILE *stream);
static size_t __fread_unlocked(void *ptr, size_t size, size_t nmemb, FILE *stream);
static int __fgetc_unlocked(FILE *stream);
static int __ungetc_unlocked(int c, FILE *stream);
static char *__fgets_unlocked(char *s, int n, FILE *stream);
static int __fputc_unlocked(int c, FILE *stream);
static int __fputs_unlocked(const char *s, FILE *stream);
static int __fclose_unlocked(FILE *stream);
static int __fseek_unlocked(FILE *stream, long offset, int whence);
static long __ftell_unlocked(FILE *stream);
static int __vfscanf_unlocked(FILE *stream, const char *fmt, va_list ap);
static int __setvbuf_unlocked(FILE *stream, char *buf, int mode, size_t size);
static int __fseeko_unlocked(FILE *stream, off_t offset, int whence);
static off_t __ftello_unlocked(FILE *stream);
/* These use the same recursive lock as the other stdio entry points.
   Until stdio gains per-stream locks, holding one also serializes other streams. */
void flockfile(FILE *stream) { (void)stream; __lock_acquire(&__stdio_lock); }
void funlockfile(FILE *stream) { (void)stream; __lock_release(&__stdio_lock); }
int getc_unlocked(FILE *stream) { return __fgetc_unlocked(stream); }
int fflush(FILE *stream) { __lock_acquire(&__stdio_lock); int r = __fflush_unlocked(stream); __lock_release(&__stdio_lock); return r; }
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) { __lock_acquire(&__stdio_lock); size_t r = __fwrite_unlocked(ptr, size, nmemb, stream); __lock_release(&__stdio_lock); return r; }
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) { __lock_acquire(&__stdio_lock); size_t r = __fread_unlocked(ptr, size, nmemb, stream); __lock_release(&__stdio_lock); return r; }
int fgetc(FILE *stream) { __lock_acquire(&__stdio_lock); int r = __fgetc_unlocked(stream); __lock_release(&__stdio_lock); return r; }
int ungetc(int c, FILE *stream) { __lock_acquire(&__stdio_lock); int r = __ungetc_unlocked(c, stream); __lock_release(&__stdio_lock); return r; }
char *fgets(char *s, int n, FILE *stream) { __lock_acquire(&__stdio_lock); char * r = __fgets_unlocked(s, n, stream); __lock_release(&__stdio_lock); return r; }
int fputc(int c, FILE *stream) { __lock_acquire(&__stdio_lock); int r = __fputc_unlocked(c, stream); __lock_release(&__stdio_lock); return r; }
int fputs(const char *s, FILE *stream) { __lock_acquire(&__stdio_lock); int r = __fputs_unlocked(s, stream); __lock_release(&__stdio_lock); return r; }
int fclose(FILE *stream) { __lock_acquire(&__stdio_lock); int r = __fclose_unlocked(stream); __lock_release(&__stdio_lock); return r; }
int fseek(FILE *stream, long offset, int whence) { __lock_acquire(&__stdio_lock); int r = __fseek_unlocked(stream, offset, whence); __lock_release(&__stdio_lock); return r; }
long ftell(FILE *stream) { __lock_acquire(&__stdio_lock); long r = __ftell_unlocked(stream); __lock_release(&__stdio_lock); return r; }
int vfscanf(FILE *stream, const char *fmt, va_list ap) { __lock_acquire(&__stdio_lock); int r = __vfscanf_unlocked(stream, fmt, ap); __lock_release(&__stdio_lock); return r; }
int setvbuf(FILE *stream, char *buf, int mode, size_t size) { __lock_acquire(&__stdio_lock); int r = __setvbuf_unlocked(stream, buf, mode, size); __lock_release(&__stdio_lock); return r; }
int fseeko(FILE *stream, off_t offset, int whence) { __lock_acquire(&__stdio_lock); int r = __fseeko_unlocked(stream, offset, whence); __lock_release(&__stdio_lock); return r; }
off_t ftello(FILE *stream) { __lock_acquire(&__stdio_lock); off_t r = __ftello_unlocked(stream); __lock_release(&__stdio_lock); return r; }

static char __stdin_buf[BUFSIZ];
static char __stdout_buf[BUFSIZ];

FILE __stdin_file  = {0, __F_READ  | __F_STATIC, _IOLBF, __stdin_buf,  BUFSIZ, 0, 0, EOF};
FILE __stdout_file = {1, __F_WRITE | __F_STATIC, -1, __stdout_buf, BUFSIZ, 0, 0, EOF};
FILE __stderr_file = {2, __F_WRITE | __F_STATIC, _IONBF, 0, 0, 0, 0, EOF};

static FILE *__open_files[64];
static int __num_open_files;

/* Cookie-stream seam (ticket #114): every path that used to hit the fd
   syscalls directly funnels through these, so a FILE backed by io_* hooks
   (open_memstream, fmemopen) behaves identically to an fd stream. */
static long __raw_read(FILE *f, char *buf, long n) {
  return f->io_read ? f->io_read(f, buf, n) : read(f->fd, buf, n);
}
static long __raw_write(FILE *f, const char *buf, long n) {
  return f->io_write ? f->io_write(f, buf, n) : write(f->fd, buf, n);
}
static long long __raw_seek(FILE *f, long long off, int whence) {
  return f->io_seek ? f->io_seek(f, off, whence) : lseek(f->fd, off, whence);
}

/* malloc'd FILEs must clear the seam fields explicitly (the static stdio
   FILEs get them zero-filled by their partial initializers). */
static void __clear_cookie(FILE *f) {
  f->cookie = 0;
  f->io_read = 0;
  f->io_write = 0;
  f->io_seek = 0;
  f->io_flush = 0;
  f->io_close = 0;
}

static int __flush_buf(FILE *stream) {
  int pos = 0;
  while (pos < stream->buf_pos) {
    long w = __raw_write(stream, stream->buf + pos, stream->buf_pos - pos);
    if (w <= 0) {
      /* Preserve unwritten data at front of buffer */
      int remaining = stream->buf_pos - pos;
      memmove(stream->buf, stream->buf + pos, remaining);
      stream->buf_pos = remaining;
      stream->flags |= __F_ERR;
      return -1;
    }
    pos += w;
  }
  stream->buf_pos = 0;
  return 0;
}

static int __fflush_unlocked(FILE *stream);
int __fflush_unlocked(FILE *stream) {
  if (!stream) {
    int result = 0;
    if (fflush(stdout) < 0) result = -1;
    if (fflush(stderr) < 0) result = -1;
    for (int i = 0; i < __num_open_files; i++) {
      if (__open_files[i] && __fwriting(__open_files[i]) && fflush(__open_files[i]) < 0) result = -1;
    }
    return result;
  }
  if ((stream->flags & __F_WRITE) && !(stream->flags & __F_RBUF) && stream->buf_pos > 0) {
    if (__flush_buf(stream) < 0) return -1;
  }
  if ((stream->flags & __F_RBUF) || stream->ungetc_char != EOF) {
    off_t unread = (stream->flags & __F_RBUF ? stream->buf_len - stream->buf_pos : 0)
                 + (stream->ungetc_char != EOF);
    int saved_errno = errno;
    if (__raw_seek(stream, -unread, SEEK_CUR) >= 0) {
      stream->buf_pos = stream->buf_len = 0;
      stream->ungetc_char = EOF;
    } else if (errno != ESPIPE) return -1;
    errno = saved_errno;
  }
  /* Cookie streams may need a post-flush step (open_memstream publishes
     its buffer/size here — POSIX makes them current at every fflush). */
  if (stream->io_flush) return stream->io_flush(stream);
  return 0;
}

static size_t __fwrite_unlocked(const void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t __fwrite_unlocked(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
  if (!(stream->flags & __F_WRITE)) {
    /* C11 7.21: wrong-direction access fails with the error indicator
       set — it must not kill the process. */
    stream->flags |= __F_ERR;
    errno = EBADF;
    return 0;
  }
  if (size == 0 || nmemb == 0) return 0;
  /* Decide after shell redirections, on first use; setvbuf overrides this. */
  if (stream->buf_mode == -1) stream->buf_mode = isatty(stream->fd) ? _IOLBF : _IOFBF;
  if (stream->flags & __F_RBUF) {
    if (!(stream->flags & __F_EOF) && fflush(stream) < 0) return 0;
    stream->buf_pos = stream->buf_len = 0; stream->ungetc_char = EOF;
    stream->flags &= ~(__F_RBUF | __F_EOF);
  }
  size_t total = size * nmemb;
  const char *src = (const char *)ptr;

  if (stream->buf_mode == _IONBF || !stream->buf) {
    long w = __raw_write(stream, src, total);
    if (w < 0) { stream->flags |= __F_ERR; return 0; }
    return w / size;
  }

  if (stream->buf_mode == _IOLBF) {
    for (size_t i = 0; i < total; i++) {
      stream->buf[stream->buf_pos++] = src[i];
      if (src[i] == '\n' || stream->buf_pos >= stream->buf_size) {
        if (__flush_buf(stream) < 0) return i / size;
      }
    }
    return nmemb;
  }

  /* _IOFBF */
  for (size_t i = 0; i < total; i++) {
    stream->buf[stream->buf_pos++] = src[i];
    if (stream->buf_pos >= stream->buf_size) {
      if (__flush_buf(stream) < 0) return i / size;
    }
  }
  return nmemb;
}

static size_t __fread_unlocked(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t __fread_unlocked(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  if (!(stream->flags & __F_READ)) {
    stream->flags |= __F_ERR;
    errno = EBADF;
    return 0;
  }
  if (size == 0 || nmemb == 0) return 0;
  size_t total = size * nmemb;
  char *dst = (char *)ptr;
  size_t got = 0;
  if (stream->ungetc_char != EOF && got < total) {
    dst[got++] = (unsigned char)stream->ungetc_char;
    stream->ungetc_char = EOF;
  }
  while (got < total) {
    if (stream->buf_pos < stream->buf_len) {
      size_t avail = stream->buf_len - stream->buf_pos;
      size_t want = total - got;
      size_t n = avail < want ? avail : want;
      memcpy(dst + got, stream->buf + stream->buf_pos, n);
      stream->buf_pos += n;
      got += n;
    } else {
      /* C11 7.21.3p3: flush all line-buffered output streams when input
         is requested on an unbuffered or line-buffered stream */
      if (stream->buf_mode != _IOFBF) fflush(0);
      if (stream->buf_mode == _IONBF || !stream->buf || stream->buf_size == 0) {
        long r = __raw_read(stream, dst + got, total - got);
        if (r <= 0) {
          if (r == 0) stream->flags |= __F_EOF;
          else stream->flags |= __F_ERR;
          break;
        }
        got += r;
      } else if (total - got >= (size_t)stream->buf_size) {
        /* A request of at least one buffer reads whole blocks straight
           into the caller's memory; only the tail goes through the buffer.
           The buffer no longer precedes the descriptor position. */
        size_t direct = total - got;
        direct -= direct % (size_t)stream->buf_size;
        if (direct > 0x7fffffff) direct = 0x7fffffff - 0x7fffffff % (size_t)stream->buf_size;
        stream->buf_len = 0;
        stream->buf_pos = 0;
        stream->flags |= __F_RBUF;
        long r = __raw_read(stream, dst + got, (long)direct);
        if (r <= 0) {
          if (r == 0) stream->flags |= __F_EOF;
          else stream->flags |= __F_ERR;
          break;
        }
        got += r;
      } else {
        long r = __raw_read(stream, stream->buf, stream->buf_size);
        if (r <= 0) {
          if (r == 0) stream->flags |= __F_EOF;
          else stream->flags |= __F_ERR;
          break;
        }
        stream->buf_len = r;
        stream->buf_pos = 0;
        stream->flags |= __F_RBUF;
      }
    }
  }
  return got / size;
}

static int __fgetc_unlocked(FILE *stream);
int __fgetc_unlocked(FILE *stream) {
  unsigned char c;
  size_t n = fread(&c, 1, 1, stream);
  if (n == 0) return EOF;
  return c;
}

static int __ungetc_unlocked(int c, FILE *stream);
int __ungetc_unlocked(int c, FILE *stream) {
  if (c == EOF) return EOF;
  stream->ungetc_char = (unsigned char)c;
  stream->flags &= ~__F_EOF;
  return (unsigned char)c;
}

static char *__fgets_unlocked(char *s, int n, FILE *stream);
char *__fgets_unlocked(char *s, int n, FILE *stream) {
  if (n <= 0) return 0;
  int i = 0;
  while (i < n - 1) {
    int c = fgetc(stream);
    if (c == EOF) break;
    s[i++] = c;
    if (c == '\n') break;
  }
  if (i == 0) return 0;
  s[i] = '\0';
  return s;
}

static int __fputc_unlocked(int c, FILE *stream);
int __fputc_unlocked(int c, FILE *stream) {
  unsigned char ch = c;
  size_t n = fwrite(&ch, 1, 1, stream);
  if (n == 0) return EOF;
  return ch;
}

static int __fputs_unlocked(const char *s, FILE *stream);
int __fputs_unlocked(const char *s, FILE *stream) {
  size_t len = strlen(s);
  size_t n = fwrite(s, 1, len, stream);
  if (n < len) return EOF;
  return 0;
}

int vfprintf(FILE *stream, const char *fmt, va_list ap) {
  /* Format once into a stack buffer; only longer output needs a second pass. */
  va_list ap2;
  va_copy(ap2, ap);
  char stackbuf[256];
  char *buf = stackbuf;
  int len = vsnprintf(stackbuf, sizeof stackbuf, fmt, ap);
  if (len < 0) { va_end(ap2); return len; }
  if (len + 1 > (int)sizeof stackbuf) {
    buf = (char *)malloc(len + 1);
    if (!buf) { va_end(ap2); return -1; }
    vsnprintf(buf, len + 1, fmt, ap2);
  }
  va_end(ap2);
  fwrite(buf, 1, len, stream);
  if (buf != stackbuf) free(buf);
  return len;
}

int fprintf(FILE *stream, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vfprintf(stream, fmt, ap);
  va_end(ap);
  return r;
}

/* POSIX dprintf/vdprintf: formatted output written unbuffered straight to a
   file descriptor (no FILE, no stdio buffering). Mirrors vfprintf but uses
   write() as the sink. */
int vdprintf(int fd, const char *fmt, va_list ap) {
  va_list ap2;
  va_copy(ap2, ap);
  char stackbuf[256];
  char *buf = stackbuf;
  int len = vsnprintf(stackbuf, sizeof stackbuf, fmt, ap);
  if (len < 0) { va_end(ap2); return len; }
  if (len + 1 > (int)sizeof stackbuf) {
    buf = (char *)malloc(len + 1);
    if (!buf) { va_end(ap2); return -1; }
    vsnprintf(buf, len + 1, fmt, ap2);
  }
  va_end(ap2);
  write(fd, buf, len);
  if (buf != stackbuf) free(buf);
  return len;
}

int dprintf(int fd, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vdprintf(fd, fmt, ap);
  va_end(ap);
  return r;
}

int vprintf(const char *fmt, va_list ap) {
  return vfprintf(stdout, fmt, ap);
}

int printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vfprintf(stdout, fmt, ap);
  va_end(ap);
  return r;
}

int putchar(int c) {
  return fputc(c, stdout);
}

int puts(const char *s) {
  fputs(s, stdout);
  fputc('\n', stdout);
  return 0;
}

FILE *fopen(const char *path, const char *mode) {
  int flags = 0;
  int fflags = 0;
  if (mode[0] == 'r') {
    if (mode[1] == '+' || (mode[1] == 'b' && mode[2] == '+')) {
      flags = O_RDWR;
      fflags = __F_READ | __F_WRITE;
    } else {
      flags = O_RDONLY;
      fflags = __F_READ;
    }
  } else if (mode[0] == 'w') {
    if (mode[1] == '+' || (mode[1] == 'b' && mode[2] == '+')) {
      flags = O_RDWR | O_CREAT | O_TRUNC;
      fflags = __F_READ | __F_WRITE;
    } else {
      flags = O_WRONLY | O_CREAT | O_TRUNC;
      fflags = __F_WRITE;
    }
  } else if (mode[0] == 'a') {
    if (mode[1] == '+' || (mode[1] == 'b' && mode[2] == '+')) {
      flags = O_RDWR | O_CREAT | O_APPEND;
      fflags = __F_READ | __F_WRITE | __F_APPEND;
    } else {
      flags = O_WRONLY | O_CREAT | O_APPEND;
      fflags = __F_WRITE | __F_APPEND;
    }
  } else {
    return 0;
  }
  int fd = open(path, flags, 0666);
  if (fd < 0) return 0;

  FILE *f = (FILE *)malloc(sizeof(FILE));
  char *buf = (char *)malloc(BUFSIZ);
  f->fd = fd;
  f->flags = fflags | __F_OWNBUF;
  f->buf_mode = _IOFBF;
  f->buf = buf;
  f->buf_size = BUFSIZ;
  f->buf_pos = 0;
  f->buf_len = 0;
  f->ungetc_char = EOF;
  __clear_cookie(f);

  if (__num_open_files < 64) {
    __open_files[__num_open_files++] = f;
  }
  return f;
}

FILE *fdopen(int fd, const char *mode) {
  int fflags = 0;
  if (mode[0] == 'r') {
    fflags = (mode[1] == '+' || (mode[1] == 'b' && mode[2] == '+')) ? (__F_READ | __F_WRITE) : __F_READ;
  } else if (mode[0] == 'w') {
    fflags = (mode[1] == '+' || (mode[1] == 'b' && mode[2] == '+')) ? (__F_READ | __F_WRITE) : __F_WRITE;
  } else if (mode[0] == 'a') {
    fflags = (mode[1] == '+' || (mode[1] == 'b' && mode[2] == '+')) ? (__F_READ | __F_WRITE | __F_APPEND) : (__F_WRITE | __F_APPEND);
  } else {
    return 0;
  }
  FILE *f = (FILE *)malloc(sizeof(FILE));
  char *buf = (char *)malloc(BUFSIZ);
  f->fd = fd;
  f->flags = fflags | __F_OWNBUF;
  f->buf_mode = _IOFBF;
  f->buf = buf;
  f->buf_size = BUFSIZ;
  f->buf_pos = 0;
  f->buf_len = 0;
  f->ungetc_char = EOF;
  __clear_cookie(f);
  if (__num_open_files < 64) {
    __open_files[__num_open_files++] = f;
  }
  return f;
}

int fileno(FILE *stream) { return stream ? stream->fd : -1; }

static int __fclose_unlocked(FILE *stream);
int __fclose_unlocked(FILE *stream) {
  fflush(stream);
  int r = stream->io_close ? stream->io_close(stream) : close(stream->fd);
  if (stream->buf && (stream->flags & __F_OWNBUF)) free(stream->buf);
  for (int i = 0; i < __num_open_files; i++) {
    if (__open_files[i] == stream) {
      __open_files[i] = __open_files[--__num_open_files];
      break;
    }
  }
  if (stream->flags & __F_STATIC) {
    /* stdin/stdout/stderr are static objects: mark the stream closed
       (drop the read/write bits so the exit-path fflush and any later
       I/O leave the dead fd alone) instead of freeing it. */
    stream->flags = __F_STATIC;
    stream->buf = 0;
    stream->buf_size = 0;
    stream->buf_pos = 0;
    stream->buf_len = 0;
    stream->ungetc_char = EOF;
    return r;
  }
  free(stream);
  return r;
}

static int __tmpfile_counter;
__import long long __time_now(void);

FILE *tmpfile(void) {
  /* Generate a unique path, open it, and unlink immediately: the open
     fd keeps working (POSIX) while the name disappears. The time()
     component avoids collisions between concurrent processes. */
  for (int tries = 0; tries < 100; tries++) {
    char name[80];
    snprintf(name, sizeof name, "/tmp/__wasm_tmpfile_%ld_%d",
             (long)__time_now(), __tmpfile_counter++);
    FILE *f = fopen(name, "w+b");
    if (f) {
      remove(name);
      return f;
    }
  }
  return 0;
}

static int __fseek_unlocked(FILE *stream, long offset, int whence);
int __fseek_unlocked(FILE *stream, long offset, int whence) {
  if (!(stream->flags & __F_RBUF) && fflush(stream) < 0) return -1;
  if (whence == SEEK_CUR) {
    /* The fd position sits at the end of any read-ahead; the stream's
       logical position is buf_len - buf_pos bytes (plus a pending
       ungetc) behind it. Relative seeks are relative to the logical
       position. */
    if (stream->flags & __F_RBUF) {
      offset -= (long)(stream->buf_len - stream->buf_pos);
    }
    if (stream->ungetc_char != EOF) offset--;
  }
  /* Seek FIRST, reset the buffer only on success: a failed seek (EINVAL on
     a negative target, say) must leave the logical position — which lives
     partly in the read-ahead buffer — untouched (POSIX; the libc-test
     memstream test asserts ftell survives a refused fseek). */
  long long r = __raw_seek(stream, offset, whence);
  if (r < 0) return -1;
  stream->buf_pos = 0;
  stream->buf_len = 0;
  stream->ungetc_char = EOF;
  stream->flags &= ~(__F_EOF | __F_RBUF);
  return 0;
}

static long __ftell_unlocked(FILE *stream);
long __ftell_unlocked(FILE *stream) {
  long pos = __raw_seek(stream, 0, SEEK_CUR);
  if (pos < 0) return -1;
  if (stream->flags & __F_RBUF) {
    /* Buffer holds read-ahead: logical position is behind the fd. */
    pos -= (stream->buf_len - stream->buf_pos);
  } else if (stream->flags & __F_WRITE) {
    /* Buffer holds unflushed output: logical position is ahead of
       the fd. On update streams (r+/w+/a+) both flags are set, so
       __F_RBUF must decide which way buf_pos counts. */
    pos += stream->buf_pos;
  }
  if ((stream->flags & __F_READ) && stream->ungetc_char != EOF) pos--;
  return pos;
}

void rewind(FILE *stream) {
  fseek(stream, 0, SEEK_SET);
  stream->flags &= ~__F_ERR;
}

/* fgetpos/fsetpos carry a full 64-bit off_t (unlike ftell/fseek's long), so
   they work on files larger than 2 GiB. The position is computed at off_t
   width by mirroring ftell's buffer-adjustment logic, and restored with a
   64-bit lseek (mirroring fseek's SEEK_SET path). */
int fgetpos(FILE *stream, fpos_t *pos) {
  off_t p = __raw_seek(stream, 0, SEEK_CUR);
  if (p < 0) return -1;
  if (stream->flags & __F_RBUF) {
    p -= (stream->buf_len - stream->buf_pos);
  } else if (stream->flags & __F_WRITE) {
    p += stream->buf_pos;
  }
  if ((stream->flags & __F_READ) && stream->ungetc_char != EOF) p--;
  *pos = p;
  return 0;
}

/* fseeko/ftello: the POSIX off_t-wide fseek/ftell (od's dump_skip seeks
   with fseeko — docs/archive/0034). Same buffer discipline as fseek/ftell, at
   64-bit width like fgetpos/fsetpos. */
static int __fseeko_unlocked(FILE *stream, off_t offset, int whence);
int __fseeko_unlocked(FILE *stream, off_t offset, int whence) {
  if (!(stream->flags & __F_RBUF) && fflush(stream) < 0) return -1;
  if (whence == SEEK_CUR) {
    if (stream->flags & __F_RBUF) {
      offset -= (off_t)(stream->buf_len - stream->buf_pos);
    }
    if (stream->ungetc_char != EOF) offset--;
  }
  /* Same order as fseek: buffer state survives a refused seek. */
  off_t r = __raw_seek(stream, offset, whence);
  if (r < 0) return -1;
  stream->buf_pos = 0;
  stream->buf_len = 0;
  stream->ungetc_char = EOF;
  stream->flags &= ~(__F_EOF | __F_RBUF);
  return 0;
}

static off_t __ftello_unlocked(FILE *stream);
off_t __ftello_unlocked(FILE *stream) {
  fpos_t p;
  if (fgetpos(stream, &p) != 0) return -1;
  return (off_t)p;
}

int fsetpos(FILE *stream, const fpos_t *pos) {
  if (!(stream->flags & __F_RBUF) && fflush(stream) < 0) return -1;
  off_t r = __raw_seek(stream, *pos, SEEK_SET);
  if (r < 0) return -1;
  stream->buf_pos = 0;
  stream->buf_len = 0;
  stream->ungetc_char = EOF;
  stream->flags &= ~(__F_EOF | __F_RBUF);
  return 0;
}

int feof(FILE *stream) {
  return (stream->flags & __F_EOF) != 0;
}

int ferror(FILE *stream) {
  return (stream->flags & __F_ERR) != 0;
}

void clearerr(FILE *stream) {
  stream->flags &= ~(__F_EOF | __F_ERR);
}

static int __setvbuf_unlocked(FILE *stream, char *buf, int mode, size_t size);
int __setvbuf_unlocked(FILE *stream, char *buf, int mode, size_t size) {
  fflush(stream);
  stream->buf_mode = mode;
  if (buf) {
    /* Caller-supplied buffer: the library no longer owns the storage,
       so fclose must not free it (and the old owned buffer, if any,
       would leak without an explicit free here). */
    if ((stream->flags & __F_OWNBUF) && stream->buf) free(stream->buf);
    stream->flags &= ~__F_OWNBUF;
    stream->buf = buf;
    stream->buf_size = size;
  }
  stream->buf_pos = 0;
  stream->buf_len = 0;
  return 0;
}

int vsscanf(const char *s, const char *fmt, va_list ap) {
  int consumed;
  int len = strlen(s);
  return __vsscanf_impl(s, len, fmt, &consumed, ap);
}

int sscanf(const char *s, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vsscanf(s, fmt, ap);
  va_end(ap);
  return r;
}

static int __vfscanf_unlocked(FILE *stream, const char *fmt, va_list ap);
int __vfscanf_unlocked(FILE *stream, const char *fmt, va_list ap) {
  if (!(stream->flags & __F_READ)) {
    stream->flags |= __F_ERR;
    errno = EBADF;
    return EOF;
  }

  /* Handle ungetc char: push it back into the buffer */
  if (stream->ungetc_char != EOF) {
    if (stream->buf_pos > 0) {
      stream->buf_pos--;
      stream->buf[stream->buf_pos] = (char)stream->ungetc_char;
    } else if (stream->buf_len < stream->buf_size) {
      memmove(stream->buf + 1, stream->buf, stream->buf_len);
      stream->buf[0] = (char)stream->ungetc_char;
      stream->buf_len++;
    } else {
      write(2, "vfscanf: buffer full with ungetc pending\n", 41);
      __wasm(void, (), op 0);
    }
    stream->ungetc_char = EOF;
  }

  /* If buffer empty, try to fill it */
  if (stream->buf_pos >= stream->buf_len) {
    long r = __raw_read(stream, stream->buf, stream->buf_size);
    if (r <= 0) {
      if (r == 0) stream->flags |= __F_EOF;
      else stream->flags |= __F_ERR;
      return -1;
    }
    stream->buf_pos = 0;
    stream->buf_len = r;
    stream->flags |= __F_RBUF; /* buffer now holds read-ahead (ftell) */
  }

  /* Shift data to buffer start for accumulation */
  if (stream->buf_pos > 0) {
    memmove(stream->buf, stream->buf + stream->buf_pos, stream->buf_len - stream->buf_pos);
    stream->buf_len -= stream->buf_pos;
    stream->buf_pos = 0;
  }

  /* Loop: try parsing, refill if consumed everything */
  for (;;) {
    va_list ap2;
    va_copy(ap2, ap);
    int consumed;
    int result = __vsscanf_impl(stream->buf, stream->buf_len, fmt, &consumed, ap2);
    va_end(ap2);

    if (consumed < stream->buf_len || (stream->flags & __F_EOF)) {
      /* Done: didn't consume everything, or no more data */
      stream->buf_pos = consumed;
      return result;
    }

    /* Consumed everything — need more data */
    if (stream->buf_len >= stream->buf_size) {
      /* Buffer full, field exceeds buffer size */
      write(2, "vfscanf: field exceeds buffer size\n", 35);
      __wasm(void, (), op 0);
    }

    long got = __raw_read(stream, stream->buf + stream->buf_len,
                          stream->buf_size - stream->buf_len);
    if (got <= 0) {
      if (got == 0) stream->flags |= __F_EOF;
      else stream->flags |= __F_ERR;
      stream->buf_pos = consumed;
      return result;
    }
    stream->buf_len += got;
    stream->flags |= __F_RBUF;
    /* Loop back and retry with more data */
  }
}

int fscanf(FILE *stream, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vfscanf(stream, fmt, ap);
  va_end(ap);
  return r;
}

int vscanf(const char *fmt, va_list ap) {
  return vfscanf(stdin, fmt, ap);
}

int scanf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vscanf(fmt, ap);
  va_end(ap);
  return r;
}

void setbuf(FILE *stream, char *buf) {
  setvbuf(stream, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

void perror(const char *s) {
  if (s && *s)
    fprintf(stderr, "%s: %s\n", s, strerror(errno));
  else
    fprintf(stderr, "%s\n", strerror(errno));
}

/* As with sprintf, the caller must provide enough destination space. */
int vsprintf(char *buf, const char *fmt, va_list ap) {
  return vsnprintf(buf, 0x7fffffff, fmt, ap);
}

int sprintf(char *buf, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vsnprintf(buf, 0x7fffffff, fmt, ap);
  va_end(ap);
  return r;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vsnprintf(buf, size, fmt, ap);
  va_end(ap);
  return r;
}

// open() moved to __posix.c (docs/archive/0382): <fcntl.h> declares it, so <fcntl.h>
// alone must LINK it — while it lived here, the POSIX-correct include set
// failed at link time and only worked by accident of <stdio.h> also being
// included. It also has to sit next to the umask state it now consults.

// Intentionally aborts — gets has no bounds checking and is unsafe.
// Do NOT replace with a working implementation. Use fgets instead.
char *gets(char *s) {
  fprintf(stderr, "gets() is unsafe and not supported; use fgets() instead\n");
  abort();
  return 0;
}

FILE *freopen(const char *path, const char *mode, FILE *stream) {
  if (!stream) return 0;
  fflush(stream);
  if (stream->io_close) stream->io_close(stream);
  else close(stream->fd);
  /* The stream is fd-backed from here on, whatever it was before. */
  __clear_cookie(stream);
  if (!path) return 0;
  int flags = 0;
  int fflags = 0;
  if (mode[0] == 'r') {
    if (mode[1] == '+' || (mode[1] == 'b' && mode[2] == '+')) {
      flags = O_RDWR; fflags = __F_READ | __F_WRITE;
    } else {
      flags = O_RDONLY; fflags = __F_READ;
    }
  } else if (mode[0] == 'w') {
    if (mode[1] == '+' || (mode[1] == 'b' && mode[2] == '+')) {
      flags = O_RDWR | O_CREAT | O_TRUNC; fflags = __F_READ | __F_WRITE;
    } else {
      flags = O_WRONLY | O_CREAT | O_TRUNC; fflags = __F_WRITE;
    }
  } else if (mode[0] == 'a') {
    if (mode[1] == '+' || (mode[1] == 'b' && mode[2] == '+')) {
      flags = O_RDWR | O_CREAT | O_APPEND; fflags = __F_READ | __F_WRITE | __F_APPEND;
    } else {
      flags = O_WRONLY | O_CREAT | O_APPEND; fflags = __F_WRITE | __F_APPEND;
    }
  } else {
    return 0;
  }
  int fd = open(path, flags, 0666);
  if (fd < 0) return 0;
  stream->fd = fd;
  /* Keep the ownership bits: freopen reuses the FILE and its buffer. */
  stream->flags = fflags | (stream->flags & (__F_OWNBUF | __F_STATIC));
  stream->buf_pos = 0;
  stream->buf_len = 0;
  stream->ungetc_char = EOF;
  return stream;
}


char *tmpnam(char *s) { (void)s; return 0; }

/* popen/pclose: spawn /bin/sh -c command with one stdio end wired to a pipe via
   posix_spawn file_actions; pclose reaps the child (FILE->pid map below). Needs
   /bin/sh on the image — seeded by the runtime at mount. */
static struct { FILE *fp; pid_t pid; } __popen_tab[16];

FILE *popen(const char *command, const char *type) {
  if (!command || !type) return 0;
  int reading = (type[0] == 'r');
  int fds[2];
  if (pipe(fds) < 0) return 0;
  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init(&fa);
  if (reading) {                                    /* parent reads fds[0] */
    posix_spawn_file_actions_adddup2(&fa, fds[1], 1);    /* child stdout -> pipe */
    posix_spawn_file_actions_addclose(&fa, fds[0]);
  } else {                                          /* parent writes fds[1] */
    posix_spawn_file_actions_adddup2(&fa, fds[0], 0);    /* child stdin <- pipe */
    posix_spawn_file_actions_addclose(&fa, fds[1]);
  }
  char *argv[4];
  argv[0] = "sh"; argv[1] = "-c"; argv[2] = (char *)command; argv[3] = 0;
  pid_t pid;
  int e = posix_spawn(&pid, "/bin/sh", &fa, 0, argv, environ);
  posix_spawn_file_actions_destroy(&fa);
  if (e != 0) { close(fds[0]); close(fds[1]); errno = e; return 0; }
  FILE *fp;
  if (reading) { close(fds[1]); fp = fdopen(fds[0], "r"); }
  else         { close(fds[0]); fp = fdopen(fds[1], "w"); }
  if (!fp) return 0;
  for (int i = 0; i < 16; i++)
    if (!__popen_tab[i].fp) { __popen_tab[i].fp = fp; __popen_tab[i].pid = pid; break; }
  return fp;
}
int pclose(FILE *stream) {
  if (!stream) return -1;
  pid_t pid = -1;
  for (int i = 0; i < 16; i++)
    if (__popen_tab[i].fp == stream) { pid = __popen_tab[i].pid; __popen_tab[i].fp = 0; break; }
  fclose(stream);
  if (pid < 0) return -1;
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) return -1;
  return status;
}

/* ---- memory-backed streams (POSIX open_memstream/fmemopen, ticket #114) ----
 *
 * Both are cookie streams over the io_* seam; the stdio core neither knows
 * nor cares that no fd is involved (fd is -1, so fileno() reports EBADF-ish
 * -1 as POSIX wants). Logic follows musl 1.2.5 src/stdio/{open_memstream,
 * fmemopen}.c, restated over this FILE's buffer discipline. */

/* open_memstream: a growing write-only buffer the CALLER frees after
   fclose. The caller's (bufp, sizep) are republished at every write, seek,
   and fflush; the buffer always carries a NUL just past the reported size.
   Reported size is min(data length, current position) per POSIX. */
struct __memstream {
  char **bufp;
  size_t *sizep;
  char *buf;
  size_t pos, len, space;
};

static void __ms_publish(struct __memstream *c) {
  c->buf[c->len] = 0;
  *c->bufp = c->buf;
  *c->sizep = c->pos < c->len ? c->pos : c->len;
}

static long __ms_write(FILE *f, const char *src, long n) {
  struct __memstream *c = (struct __memstream *)f->cookie;
  size_t need = c->pos + (size_t)n + 1;
  if (need > c->space) {
    size_t ns = c->space * 2;
    if (ns < need) ns = need;
    char *nb = (char *)realloc(c->buf, ns);
    if (!nb) { f->flags |= __F_ERR; return -1; }   /* realloc set ENOMEM */
    c->buf = nb;
    c->space = ns;
  }
  if (c->pos > c->len)   /* a seek moved past the end: the gap reads as 0s */
    memset(c->buf + c->len, 0, c->pos - c->len);
  memcpy(c->buf + c->pos, src, n);
  c->pos += n;
  if (c->pos > c->len) c->len = c->pos;
  __ms_publish(c);
  return n;
}

static long long __ms_seek(FILE *f, long long off, int whence) {
  struct __memstream *c = (struct __memstream *)f->cookie;
  long long base = whence == SEEK_CUR ? (long long)c->pos
                 : whence == SEEK_END ? (long long)c->len : 0;
  if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
    errno = EINVAL;
    return -1;
  }
  if (off < -base) { errno = EINVAL; return -1; }
  c->pos = (size_t)(base + off);
  __ms_publish(c);
  return base + off;
}

static int __ms_flush(FILE *f) {
  __ms_publish((struct __memstream *)f->cookie);
  return 0;
}

static int __ms_close(FILE *f) {
  /* Publish one last time; the BUFFER survives for the caller to free. */
  __ms_publish((struct __memstream *)f->cookie);
  free(f->cookie);
  return 0;
}

FILE *open_memstream(char **bufp, size_t *sizep) {
  struct __memstream *c = (struct __memstream *)malloc(sizeof *c);
  if (!c) return 0;
  c->buf = (char *)malloc(32);
  if (!c->buf) { free(c); return 0; }
  c->bufp = bufp;
  c->sizep = sizep;
  c->pos = 0;
  c->len = 0;
  c->space = 32;
  FILE *f = (FILE *)malloc(sizeof(FILE));
  if (!f) { free(c->buf); free(c); return 0; }
  f->fd = -1;
  f->flags = __F_WRITE;
  f->buf_mode = _IONBF;   /* writes land in the cookie directly */
  f->buf = 0;
  f->buf_size = 0;
  f->buf_pos = 0;
  f->buf_len = 0;
  f->ungetc_char = EOF;
  __clear_cookie(f);
  f->cookie = c;
  f->io_write = __ms_write;
  f->io_seek = __ms_seek;
  f->io_flush = __ms_flush;
  f->io_close = __ms_close;
  __ms_publish(c);
  if (__num_open_files < 64) __open_files[__num_open_files++] = f;
  return f;
}

/* fmemopen: a FILE over a caller-supplied (or, with buf == NULL, owned)
   fixed-size buffer. musl's rules: 'r' modes read the whole size; 'w'
   truncates (buf[0] = 0); 'a' starts at the first NUL; every write that
   extends the data writes a terminating NUL when it still fits; writes
   are clipped at size (error flag on the clipped remainder); seeks allow
   0..size. Reads/writes go through the normal stdio buffer, so scanf,
   ungetc and read-ahead all behave exactly as on an fd stream. */
struct __memfile {
  char *buf;
  size_t size, pos, len;
  int append;
  int own;
};

static long __mf_read(FILE *f, char *dst, long n) {
  struct __memfile *c = (struct __memfile *)f->cookie;
  if (c->pos >= c->len) return 0;
  size_t avail = c->len - c->pos;
  if ((size_t)n > avail) n = (long)avail;
  memcpy(dst, c->buf + c->pos, n);
  c->pos += n;
  return n;
}

static long __mf_write(FILE *f, const char *src, long n) {
  struct __memfile *c = (struct __memfile *)f->cookie;
  if (c->append) c->pos = c->len;
  size_t room = c->size - c->pos;
  if ((size_t)n > room) {
    /* No room for all of it: write what fits, fail loudly on the rest. */
    f->flags |= __F_ERR;
    errno = ENOSPC;
    if (room == 0) return -1;
    n = (long)room;
  }
  memcpy(c->buf + c->pos, src, n);
  c->pos += n;
  if (c->pos > c->len) {
    c->len = c->pos;
    if (c->len < c->size) c->buf[c->len] = 0;
  }
  return n;
}

static long long __mf_seek(FILE *f, long long off, int whence) {
  struct __memfile *c = (struct __memfile *)f->cookie;
  long long base = whence == SEEK_CUR ? (long long)c->pos
                 : whence == SEEK_END ? (long long)c->len : 0;
  if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
    errno = EINVAL;
    return -1;
  }
  if (off < -base || off > (long long)c->size - base) {
    errno = EINVAL;
    return -1;
  }
  c->pos = (size_t)(base + off);
  return base + off;
}

static int __mf_close(FILE *f) {
  struct __memfile *c = (struct __memfile *)f->cookie;
  if (c->own) free(c->buf);
  free(c);
  return 0;
}

FILE *fmemopen(void *buf, size_t size, const char *mode) {
  int fflags = 0;
  if (mode[0] == 'r') {
    fflags = (mode[1] == '+' || (mode[1] == 'b' && mode[2] == '+')) ? (__F_READ | __F_WRITE) : __F_READ;
  } else if (mode[0] == 'w') {
    fflags = (mode[1] == '+' || (mode[1] == 'b' && mode[2] == '+')) ? (__F_READ | __F_WRITE) : __F_WRITE;
  } else if (mode[0] == 'a') {
    fflags = (mode[1] == '+' || (mode[1] == 'b' && mode[2] == '+')) ? (__F_READ | __F_WRITE) : __F_WRITE;
  } else {
    errno = EINVAL;
    return 0;
  }
  struct __memfile *c = (struct __memfile *)malloc(sizeof *c);
  if (!c) return 0;
  c->own = 0;
  if (!buf) {
    buf = calloc(1, size ? size : 1);
    if (!buf) { free(c); return 0; }
    c->own = 1;
  }
  c->buf = (char *)buf;
  c->size = size;
  c->append = (mode[0] == 'a');
  if (mode[0] == 'r') c->len = size;
  else if (mode[0] == 'a') c->len = strnlen(c->buf, size);
  else { c->len = 0; if (size) c->buf[0] = 0; }
  c->pos = c->append ? c->len : 0;
  FILE *f = (FILE *)malloc(sizeof(FILE));
  if (!f) { if (c->own) free(buf); free(c); return 0; }
  char *iobuf = (char *)malloc(BUFSIZ);
  f->fd = -1;
  f->flags = fflags | __F_OWNBUF;
  f->buf_mode = _IOFBF;
  f->buf = iobuf;
  f->buf_size = BUFSIZ;
  f->buf_pos = 0;
  f->buf_len = 0;
  f->ungetc_char = EOF;
  __clear_cookie(f);
  f->cookie = c;
  f->io_read = __mf_read;
  f->io_write = __mf_write;
  f->io_seek = __mf_seek;
  f->io_close = __mf_close;
  if (__num_open_files < 64) __open_files[__num_open_files++] = f;
  return f;
}
  