
#include <stdlib.h>
#include <locale.h>
#include <langinfo.h>
#include <string.h>

static struct lconv __c_lconv = {
  ".",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  127,
  127,
  127,
  127,
  127,
  127,
  127,
  127,
};

/* C uses single-byte characters; C.UTF-8 uses Unicode. Keep
   category selection independent, and validate every requested change before
   committing LC_ALL. An empty name follows POSIX environment precedence. */
int __ctype_utf8;
static int __locale_categories[6]; /* 0 = C, 1 = C.UTF-8 */
static const char *__locale_keys[6] = {
  "LC_ALL", "LC_COLLATE", "LC_CTYPE", "LC_MONETARY", "LC_NUMERIC", "LC_TIME"
};
static const char *__locale_names[2] = { "C", "C.UTF-8" };
static char __locale_result[160];

static int __locale_id(const char *name) {
  if (!strcmp(name, "C") || !strcmp(name, "POSIX")) return 0;
  /* UTF-8 charset aliases use C numeric/time/collation conventions. */
  if (!strcmp(name, "C.UTF-8") || !strcmp(name, "C.utf8") ||
      !strcmp(name, "en_US.UTF-8") || !strcmp(name, "en_US.utf8") ||
      !strcmp(name, "el_GR.UTF-8") || !strcmp(name, "el_GR.utf8")) return 1;
  return -1;
}

char *setlocale(int category, const char *locale) {
  if (category < LC_ALL || category > LC_TIME) return 0;
  int next[6];
  for (int i = 1; i <= LC_TIME; i++) next[i] = __locale_categories[i];
  if (locale) {
    if (category == LC_ALL && !strncmp(locale, "LC_COLLATE=", 11)) {
      /* Our LC_ALL query representation is accepted back unchanged. */
      const char *p = locale;
      for (int i = 1; i <= LC_TIME; i++) {
        size_t n = strlen(__locale_keys[i]);
        if (strncmp(p, __locale_keys[i], n) || p[n] != '=') return 0;
        p += n + 1;
        const char *end = strchr(p, ';');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        char name[12];
        if (len >= sizeof name) return 0;
        memcpy(name, p, len); name[len] = 0;
        next[i] = __locale_id(name);
        if (next[i] < 0 || ((i < LC_TIME) != (end != 0))) return 0;
        if (end) p = end + 1;
      }
    } else {
      int first = category == LC_ALL ? 1 : category;
      int last = category == LC_ALL ? LC_TIME : category;
      for (int i = first; i <= last; i++) {
        const char *name = locale;
        if (!*name) {
          name = getenv("LC_ALL");
          if (!name || !*name) name = getenv(__locale_keys[i]);
          if (!name || !*name) name = getenv("LANG");
          if (!name || !*name) name = "C.UTF-8";
        }
        next[i] = __locale_id(name);
        if (next[i] < 0) return 0;
      }
    }
    for (int i = 1; i <= LC_TIME; i++) __locale_categories[i] = next[i];
    __ctype_utf8 = next[LC_CTYPE];
  }
  if (category != LC_ALL) return (char *)__locale_names[next[category]];
  int same = 1;
  for (int i = 2; i <= LC_TIME; i++) if (next[i] != next[1]) same = 0;
  if (same) return (char *)__locale_names[next[1]];
  __locale_result[0] = 0;
  for (int i = 1; i <= LC_TIME; i++) {
    if (i > 1) strcat(__locale_result, ";");
    strcat(__locale_result, __locale_keys[i]);
    strcat(__locale_result, "=");
    strcat(__locale_result, __locale_names[next[i]]);
  }
  return __locale_result;
}

struct lconv *localeconv(void) {
  return &__c_lconv;
}

/* nl_langinfo: C-locale answers; CODESET follows LC_CTYPE.
   Indices MUST match the <langinfo.h> item constants. */
static const char *__nl_items[] = {
  "UTF-8",                                          /* CODESET */
  "%a %b %e %H:%M:%S %Y", "%m/%d/%y", "%H:%M:%S",   /* D_T_FMT D_FMT T_FMT */
  "%I:%M:%S %p", "AM", "PM",                        /* T_FMT_AMPM AM_STR PM_STR */
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
  "January", "February", "March", "April", "May", "June", "July",
  "August", "September", "October", "November", "December",
  "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
  "Aug", "Sep", "Oct", "Nov", "Dec",
  ".", "",                                          /* RADIXCHAR THOUSEP */
  "^[yY]", "^[nN]",                                 /* YESEXPR NOEXPR */
  "", "", "", "", "", "",                           /* CRNCYSTR ERA.. ALT_DIGITS */
};

char *nl_langinfo(nl_item item) {
  if (item == CODESET && !__ctype_utf8) return "ANSI_X3.4-1968";
  if (item < 0 || item >= (int)(sizeof __nl_items / sizeof __nl_items[0]))
    return "";
  return (char *)__nl_items[item];
}
  