
#pragma once
#include <stdint.h>
#include <wchar.h>    /* size_t and mbstate_t; the codecs live in __wchar.c */
typedef uint_least16_t char16_t;
typedef uint_least32_t char32_t;
#define __STDC_UTF_16__ 1
#define __STDC_UTF_32__ 1
/* C11 7.28.1: restartable UTF-16/UTF-32 conversions against the selected
   execution encoding. wchar_t is already UTF-32 here, so the char32_t pair
   shares mbrtowc/wcrtomb's codec; the char16_t pair keeps a pending
   surrogate in the conversion state. */
size_t mbrtoc16(char16_t *pc16, const char *s, size_t n, mbstate_t *ps);
size_t c16rtomb(char *s, char16_t c16, mbstate_t *ps);
size_t mbrtoc32(char32_t *pc32, const char *s, size_t n, mbstate_t *ps);
size_t c32rtomb(char *s, char32_t c32, mbstate_t *ps);
  