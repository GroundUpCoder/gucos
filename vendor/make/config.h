/* GNU Make 4.4.1 configuration for the wasm-posix C runtime.
   Fork/exec and pipe jobserver; no dynamic modules, Guile or NLS. */
#pragma once
#include "src/mkconfig.h"
#define VERSION PACKAGE_VERSION
#define MAKE_HOST "wasm32-unknown-wasmposix"
#define MK_CONFIGURE 1
#define MAKE_JOBSERVER 1
#define MAKE_SYMLINKS 1
#define POSIX 1
#define STDC_HEADERS 1
#define HAVE_ALLOCA 1
#define HAVE_ALLOCA_H 1
#define HAVE_ATEXIT 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_DIRENT_H 1
#define HAVE_DUP 1
#define HAVE_DUP2 1
#define HAVE_FCNTL_H 1
#define HAVE_FDOPEN 1
#define HAVE_FORK 1
#define HAVE_WORKING_FORK 1
#define HAVE_GETCWD 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_INTMAX_T 1
#define HAVE_INTTYPES_H 1
#define HAVE_ISATTY 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_LONG_LONG_INT 1
#define HAVE_LSTAT 1
#define HAVE_MKSTEMP 1
#define HAVE_PIPE 1
#define HAVE_READLINK 1
#define HAVE_REALPATH 1
#define HAVE_SA_RESTART 1
#define HAVE_SETVBUF 1
#define HAVE_SIGACTION 1
#define HAVE_SIG_ATOMIC_T 1
#define HAVE_STDBOOL_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRCASECMP 1
#define HAVE_STRCOLL 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_STRNCASECMP 1
#define HAVE_STRSIGNAL 1
#define HAVE_STRTOLL 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_UINTMAX_T 1
#define HAVE_UMASK 1
#define HAVE_UNISTD_H 1
#define HAVE_UNSIGNED_LONG_LONG_INT 1
#define HAVE_WAITPID 1
#define HAVE_WCHAR_H 1
#define HAVE_FNMATCH 1
#define PATH_SEPARATOR_CHAR ':'
#define SCCS_GET "get"
#define LOCALEDIR "/usr/share/locale"
#define LIBDIR "/usr/lib"
#define INCLUDEDIR "/usr/include"
#define _GL_INLINE static inline
#define _GL_EXTERN_INLINE static inline
#include "src/mkcustom.h"
#define FILE_TIMESTAMP_HI_RES 0
#define vfork fork
#define HAVE_MEMRCHR 1
