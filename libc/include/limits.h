
#pragma once
#define CHAR_BIT 8
#define SCHAR_MIN (-128)
#define SCHAR_MAX 127
#define UCHAR_MAX 255
#define CHAR_MIN SCHAR_MIN
#define CHAR_MAX SCHAR_MAX
#define MB_LEN_MAX 4
#define SHRT_MIN (-32768)
#define SHRT_MAX 32767
#define USHRT_MAX 65535
#define INT_MIN (-2147483647 - 1)
#define INT_MAX 2147483647
#define UINT_MAX 4294967295U
#define LONG_MIN (-2147483647L - 1L)
#define LONG_MAX 2147483647L
#define ULONG_MAX 4294967295UL
/* POSIX: maximum value of an object of type ssize_t */
#define SSIZE_MAX 2147483647L
/* POSIX: maximum length of a pathname, including the terminating NUL */
#define PATH_MAX 4096
/* POSIX: maximum length of a filename (excluding NUL) */
#define NAME_MAX 255
/* POSIX <regex.h> limits: max repetitions in an interval, and longest
   character-class name. */
#define RE_DUP_MAX 255
#define CHARCLASS_NAME_MAX 14
#define LLONG_MIN (-9223372036854775807LL - 1LL)
#define LLONG_MAX 9223372036854775807LL
#define ULLONG_MAX 18446744073709551615ULL
  