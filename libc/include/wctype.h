
#pragma once
__require_source("__wchar.c");
typedef unsigned int wint_t;
typedef int wctrans_t;
typedef int wctype_t;
#define WEOF ((wint_t)-1)
int iswalnum(wint_t c);
int iswalpha(wint_t c);
int iswblank(wint_t c);
int iswcntrl(wint_t c);
int iswdigit(wint_t c);
int iswgraph(wint_t c);
int iswlower(wint_t c);
int iswprint(wint_t c);
int iswpunct(wint_t c);
int iswspace(wint_t c);
int iswupper(wint_t c);
int iswxdigit(wint_t c);
wint_t towlower(wint_t c);
wint_t towupper(wint_t c);
/* Character-class lookup/test (C95). wctype(name) maps a POSIX class name
   ("alpha", "digit", "space", ...) to an opaque handle; iswctype(c, handle)
   tests c against it. Used by POSIX regex bracket classes ([[:alpha:]]). */
wctype_t wctype(const char *name);
int iswctype(wint_t c, wctype_t type);

wctrans_t wctrans(const char *name);
wint_t towctrans(wint_t c, wctrans_t trans);
