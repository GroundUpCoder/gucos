
#pragma once
__require_source("__ctype.c");
int isalnum(int c);
int isalpha(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);
int tolower(int c);
int toupper(int c);
/* isascii/toascii (XSI, docs/archive/0325 Group A — _decimal.c calls isascii).
   Removed from POSIX.1-2008 but still universally present, and unlike the
   is*() family above they are defined for ALL int values, not just
   unsigned char and EOF — which is exactly why callers reach for isascii
   before narrowing. Real functions, not macros: code takes their address. */
/* A consumer may already have supplied its own isascii/toascii macro behind
   an #ifndef (netsurf's utils/config.h does), and if that header was reached
   first the macro would rewrite the declarations below into a parse error
   INSIDE <ctype.h> — a thoroughly confusing place to land. A standard header
   is entitled to win: drop any prior definition, declare the real functions,
   then re-supply the macro form ourselves. */
#undef isascii
#undef toascii
int isascii(int c);
int toascii(int c);
/* glibc and musl expose these as function-like MACROS as well as functions,
   and portable code tests an #ifndef on isascii to decide whether to supply
   its own. Defining the macro here matches the platforms such code is
   written against, so that test correctly finds the platform already has it.
   Taking the address still reaches the FUNCTION: a bare isascii not
   followed by an open paren does not expand, which is what keeps it usable
   in a dispatch table. */
#define isascii(c) (((c) & ~0x7f) == 0)
#define toascii(c) ((c) & 0x7f)
  