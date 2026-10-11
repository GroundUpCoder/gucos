
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

__import long long __time_now(void);
__import long __clock(void);
__import long __timezone_offset(long long t);

time_t time(time_t *t) {
  time_t now = __time_now();
  if (t) *t = now;
  return now;
}

clock_t clock(void) {
  /* __clock() is milliseconds; CLOCKS_PER_SEC is 1000000 (POSIX/XSI). */
  return __clock() * 1000;
}

double difftime(time_t t1, time_t t0) {
  return (double)(t1 - t0);
}

static int __is_leap(int y) {
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int __days_in_month(int m, int leap) {
  static int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (m == 1 && leap) return 29;
  return mdays[m];
}

static int __days_in_year(int y) {
  return __is_leap(y) ? 366 : 365;
}

static struct tm __gmtime_buf;

static int __secs_to_tm(time_t t, struct tm *res) {
  time_t days = t / 86400;
  long rem = (long)(t % 86400);
  if (rem < 0) { rem += 86400; days--; }

  res->tm_hour = (int)(rem / 3600);
  rem %= 3600;
  res->tm_min = (int)(rem / 60);
  res->tm_sec = (int)(rem % 60);

  /* Jan 1, 1970 was a Thursday (wday=4) */
  int wday = (int)((days + 4) % 7);
  if (wday < 0) wday += 7;
  res->tm_wday = wday;

  /* Invert the Gregorian 400-year cycle, bounded even for LLONG_MIN/MAX
     timestamps. March starts the year so the leap day is at its end. */
  long long z = days + 719468;
  long long era = (z >= 0 ? z : z - 146096) / 146097;
  unsigned doe = (unsigned)(z - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  long long y = (long long)yoe + era * 400;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  int day = (int)(doy - (153 * mp + 2) / 5 + 1);
  int month = (int)mp + (mp < 10 ? 3 : -9);
  y += month <= 2;
  if (y - 1900 < (-2147483647LL - 1) || y - 1900 > 2147483647LL) {
    errno = EOVERFLOW;
    return -1;
  }
  res->tm_year = (int)(y - 1900);
  res->tm_mon = month - 1;
  res->tm_mday = day;
  int yday = day - 1;
  for (int m = 0; m < month - 1; m++) yday += __days_in_month(m, __is_leap((int)(y % 400)));
  res->tm_yday = yday;
  return 0;
}

struct tm *gmtime(const time_t *timep) {
  if (__secs_to_tm(*timep, &__gmtime_buf)) return NULL;
  __gmtime_buf.tm_isdst = 0;
  __gmtime_buf.tm_gmtoff = 0;
  __gmtime_buf.tm_zone = "UTC";
  return &__gmtime_buf;
}

/* gmtime_r (POSIX) — the reentrant twin. OWNED BY docs/archive/0325 Group A, which
   is where the overlap with docs/archive/0382 gap 4 was resolved; both tickets
   record it. CPython calls it unconditionally (Python/pytime.c), and it is
   the form portable code should prefer: gmtime's static buffer is shared
   with every other caller in the process. */
struct tm *gmtime_r(const time_t *timep, struct tm *result) {
  if (__secs_to_tm(*timep, result)) return NULL;
  result->tm_isdst = 0;
  result->tm_gmtoff = 0;
  result->tm_zone = "UTC";
  return result;
}

/* ---- timezone state (POSIX tzset, docs/archive/0325 Group A) -----------------
 *
 * OWNED BY docs/archive/0325 Group A; docs/archive/0382 gap 6 names the same symbol and
 * defers to it. Both tickets record the split.
 *
 * There is no tz database here. Fixed-offset TZ strings are handled below;
 * otherwise the host supplies an offset (__timezone_offset) including its DST
 * rule. For this fallback tzname carries a NUMERIC abbreviation in the
 * modern tzdata style ("+0530", "-08", "UTC"), not an alphabetic one: we do
 * not know that a -08:00 offset is called "PST", and inventing that would be
 * worse than reporting the offset we actually have. */
long timezone = 0;      /* seconds WEST of UTC — note the sign is inverted
                           relative to tm_gmtoff, as POSIX specifies */
int daylight = 0;
char *tzname[2] = { "UTC", "UTC" };

static char __tzname_buf[8];

/* Render a UTC offset (seconds EAST, i.e. tm_gmtoff sign) as a zone
   abbreviation. Static storage, like glibc's tzname strings. */
static const char *__tzname_for(long off) {
  if (off == 0) return "UTC";
  int neg = off < 0;
  long a = neg ? -off : off;
  int hh = (int)(a / 3600), mm = (int)((a % 3600) / 60);
  int i = 0;
  __tzname_buf[i++] = neg ? '-' : '+';
  __tzname_buf[i++] = (char)('0' + hh / 10);
  __tzname_buf[i++] = (char)('0' + hh % 10);
  if (mm) {
    __tzname_buf[i++] = (char)('0' + mm / 10);
    __tzname_buf[i++] = (char)('0' + mm % 10);
  }
  __tzname_buf[i] = 0;
  return __tzname_buf;
}

/* Fixed-offset POSIX TZ strings use seconds WEST of UTC. Named zones and
   DST rules still use the host offset; no zoneinfo database is installed.
   Parse the current environment on each conversion, as if tzset was called. */
static int __fixed_tz(long *east, char *name) {
  const char *tz = getenv("TZ");
  if (!tz) return 0;
  if (*tz == ':') tz++;
  if (!*tz) { *east = 0; strcpy(name, "UTC"); return 1; }
  int n = 0;
  if (*tz == '<') {
    tz++;
    while (*tz && *tz != '>' && n < 63) name[n++] = *tz++;
    if (*tz++ != '>') return 0;
  } else {
    while (((*tz >= 'A' && *tz <= 'Z') || (*tz >= 'a' && *tz <= 'z')) && n < 63)
      name[n++] = *tz++;
  }
  name[n] = 0;
  if (n < 3) return 0;
  if (!*tz && (!strcmp(name, "UTC") || !strcmp(name, "GMT"))) { *east = 0; return 1; }
  int sign = 1;
  if (*tz == '-' || *tz == '+') { if (*tz == '-') sign = -1; tz++; }
  int parts[3] = {0,0,0};
  for (int i = 0; i < 3; i++) {
    if (*tz < '0' || *tz > '9') return 0;
    int digits = 0;
    while (*tz >= '0' && *tz <= '9') {
      if (++digits > 2) return 0;
      parts[i] = parts[i] * 10 + (*tz++ - '0');
    }
    if (parts[i] > (i == 0 ? 24 : 59)) return 0;
    if (*tz != ':' || i == 2) break;
    tz++;
  }
  if (*tz) return 0; /* A DST suffix is not a fixed-offset zone. */
  *east = -sign * (parts[0]*3600 + parts[1]*60 + parts[2]);
  return 1;
}
static long __local_offset(time_t t) {
  long east;
  char name[64];
  return __fixed_tz(&east, name) ? east : __timezone_offset(t);
}
static const char *__local_zone(long offset) {
  static char name[64];
  long east;
  return __fixed_tz(&east, name) ? name : __tzname_for(offset);
}
static int __local_isdst(void) {
  long east;
  char name[64];
  return __fixed_tz(&east, name) ? 0 : -1;
}

/* Publish the fixed TZ setting, or the host zone state when no supported
   fixed-offset setting is present. */
void tzset(void) {
  time_t now = __time_now();
  long off;
  static char fixed_name[64];
  if (__fixed_tz(&off, fixed_name)) {
    timezone = -off;
    daylight = 0;
    tzname[0] = tzname[1] = fixed_name;
    return;
  }
  off = __timezone_offset(now);
  timezone = -off;                 /* POSIX: seconds WEST of UTC */
  tzname[0] = (char *)__tzname_for(off);
  /* daylight is "nonzero if DST is EVER in effect for this zone". We cannot
     see the rule, only offsets, so probe two instants six months apart: if
     they differ, the zone observes DST somewhere in the year. */
  long off_jan = __timezone_offset(now - 15768000);   /* -182.5 days */
  long off_jul = __timezone_offset(now + 15768000);
  daylight = (off_jan != off_jul) ? 1 : 0;
  tzname[1] = daylight ? (char *)__tzname_for(off_jan > off_jul ? off_jan : off_jul)
                       : tzname[0];
}

static struct tm __localtime_buf;

struct tm *localtime(const time_t *timep) {
  long offset = __local_offset(*timep);
  if ((offset > 0 && *timep > 9223372036854775807LL - offset) ||
      (offset < 0 && *timep < (-9223372036854775807LL - 1) - offset)) {
    errno = EOVERFLOW; return NULL;
  }
  time_t local = *timep + offset;
  if (__secs_to_tm(local, &__localtime_buf)) return NULL;
  __localtime_buf.tm_isdst = __local_isdst();
  __localtime_buf.tm_gmtoff = offset;
  __localtime_buf.tm_zone = __local_zone(offset);
  return &__localtime_buf;
}

struct tm *localtime_r(const time_t *timep, struct tm *result) {
  long offset = __local_offset(*timep);
  if ((offset > 0 && *timep > 9223372036854775807LL - offset) ||
      (offset < 0 && *timep < (-9223372036854775807LL - 1) - offset)) {
    errno = EOVERFLOW; return NULL;
  }
  time_t local = *timep + offset;
  if (__secs_to_tm(local, result)) return NULL;
  result->tm_isdst = __local_isdst();
  result->tm_gmtoff = offset;
  result->tm_zone = __local_zone(offset);
  return result;
}

/* Days from civil date (Howard Hinnant's era-based closed form; exact for
   the whole long long year range under C's truncating division). Closed
   form matters: the old per-year loop was O(|year-1970|), and the libc-test
   exercises tm_year == INT_MAX (ticket #113), where a loop is a hang and
   int year arithmetic is a wrong answer. m is 1-12; day and the caller's
   h/m/s are linear, so out-of-range values normalize arithmetically. */
static long long __days_from_civil(long long y, int m, long long d) {
  y -= m <= 2;
  long long era = (y >= 0 ? y : y - 399) / 400;
  unsigned long long yoe = (unsigned long long)(y - era * 400);        /* [0,399] */
  unsigned long long doy = (153ULL * (unsigned)(m + (m > 2 ? -3 : 9)) + 2) / 5;
  unsigned long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long long)doe + (d - 1) - 719468;
}

/* Broken-down fields -> seconds since the epoch, fields read as UTC (no
   zone consulted). The ONE fields->secs arithmetic: mktime, timegm and
   strftime %s all go through it. */
static long long __tm_to_secs_utc(const struct tm *tp) {
  long long y = (long long)tp->tm_year + 1900;
  int m = tp->tm_mon;
  if (m < 0 || m >= 12) {
    long long adj = m / 12;
    m %= 12;
    if (m < 0) { m += 12; adj--; }
    y += adj;
  }
  long long days = __days_from_civil(y, m + 1, (long long)tp->tm_mday);
  return days * 86400 + tp->tm_hour * 3600LL + tp->tm_min * 60LL + tp->tm_sec;
}

time_t mktime(struct tm *tp) {
  /* Read fields as local wall-clock time, then apply TZ or the host offset. */
  time_t secs = __tm_to_secs_utc(tp);
  long offset = __local_offset(secs);
  secs -= offset;

  /* Fill in derived fields by converting back */
  struct tm *tmp = localtime(&secs);
  if (!tmp) return (time_t)-1;
  *tp = *tmp;

  return secs;
}

/* timegm (BSD/glibc, docs/archive/0382 gap 5 / docs/archive/0325 Group B) — mktime's UTC
   twin. Owned here alongside the rest of the time surface. NOT
   mktime-minus-the-offset: mktime consults the zone for the INSTANT it is
   computing, so subtracting an offset afterwards is wrong across a DST
   boundary. This does the calendar arithmetic directly in UTC. */
time_t timegm(struct tm *tp) {
  time_t secs = __tm_to_secs_utc(tp);

  /* Like mktime, normalise the caller's struct from the result. */
  struct tm norm;
  if (!gmtime_r(&secs, &norm)) return (time_t)-1;
  *tp = norm;
  return secs;
}

static char __asctime_buf[32];

static const char *__wday_abbr[] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char *__mon_abbr[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

char *asctime(const struct tm *tp) {
  sprintf(__asctime_buf, "%s %s %2d %02d:%02d:%02d %d\n",
      __wday_abbr[tp->tm_wday], __mon_abbr[tp->tm_mon],
      tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec,
      tp->tm_year + 1900);
  return __asctime_buf;
}

char *ctime(const time_t *timep) {
  return asctime(localtime(timep));
}

/* asctime_r/ctime_r (POSIX) — reentrant twins writing into a caller buffer.
   POSIX requires the buffer to be at least 26 bytes. Named in the docs/archive/0350
   zip-harness gap list alongside gmtime_r (docs/archive/0382). */
char *asctime_r(const struct tm *tp, char *buf) {
  sprintf(buf, "%s %s %2d %02d:%02d:%02d %d\n",
      __wday_abbr[tp->tm_wday], __mon_abbr[tp->tm_mon],
      tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec,
      tp->tm_year + 1900);
  return buf;
}

char *ctime_r(const time_t *timep, char *buf) {
  struct tm tmp;
  return asctime_r(localtime_r(timep, &tmp), buf);
}

/* Set when a conversion did not fit: C99 7.23.3.5 then requires strftime to
   return 0, which callers (gawk's strftime builtin) use to grow the buffer. */
static _Thread_local int __strftime_overflow;
static void __ap_str(char *s, size_t max, size_t *pos, const char *src) {
  while (*src && *pos + 1 < max) {
    s[*pos] = *src;
    (*pos)++;
    src++;
  }
  if (*src) __strftime_overflow = 1;
}

static void __ap_int(char *s, size_t max, size_t *pos, int val, int width) {
  char buf[16];
  int len = 0;
  int neg = 0;
  int v = val;
  if (v < 0) { neg = 1; v = -v; }
  if (v == 0) { buf[len++] = '0'; }
  else { while (v > 0) { buf[len++] = '0' + v % 10; v /= 10; } }
  /* pad with zeros */
  int total = len + neg;
  while (total < width) { __ap_str(s, max, pos, "0"); total++; }
  if (neg) __ap_str(s, max, pos, "-");
  int i;
  for (i = len - 1; i >= 0; i--) {
    char c[2];
    c[0] = buf[i];
    c[1] = 0;
    __ap_str(s, max, pos, c);
  }
}

/* Year-shaped number under the C23/musl %[+0][width] rules (%C/%F/%G/%Y).
   width < 0 = no explicit width: zero-pad to defw counting the sign, and —
   when defplus (Y/G/F-year, per Austin #739) — prefix '+' once the value
   outgrows 4 digits. Explicit width: pad (sign first) to exactly width;
   the implicit '+' is suppressed, the '+' FLAG emits one only when the
   digits leave room for it. Derived from vendor/libc-test's strftime rows,
   which encode musl's behaviour. */
static void __ap_llw(char *s, size_t max, size_t *pos, long long val,
                     int defw, int width, int plus, int defplus) {
  char buf[24];
  int len = 0;
  int neg = val < 0;
  unsigned long long v = neg ? -(unsigned long long)val : (unsigned long long)val;
  do { buf[len++] = (char)('0' + (int)(v % 10)); v /= 10; } while (v);
  char sign = 0;
  int total;
  if (width < 0) {
    if (neg) sign = '-';
    else if (defplus && len > defw) sign = '+';
    total = defw;
  } else {
    if (neg) sign = '-';
    else if (plus && len < width) sign = '+';
    total = width;
  }
  if (sign) { char sc[2] = { sign, 0 }; __ap_str(s, max, pos, sc); }
  int have = len + (sign ? 1 : 0);
  while (have < total) { __ap_str(s, max, pos, "0"); have++; }
  while (len > 0) { char dc[2] = { buf[--len], 0 }; __ap_str(s, max, pos, dc); }
}

/* Leap test on a struct tm year value (offset 1900); long long so
   tm_year == INT_MAX can't wrap. Divisibility survives truncating
   division's sign, so negative (proleptic) years are exact too. */
static int __tm_year_leap(long long tyear) {
  long long y = tyear + 1900;
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

/* ISO-8601 week number from yday/wday/year (musl's algorithm). %V and
   %g/%G MUST agree, so all three route through here. The subtractions
   stay nonnegative in the branches that reach them: the dec31 form only
   runs when val==0 (early January, yday < 7), the jan1 form only when
   val==53 (late December, yday > 360). */
static int __iso_week(const struct tm *tp) {
  int val = (tp->tm_yday + 7 - (tp->tm_wday + 6) % 7) / 7;
  /* If Jan 1 is just 1-3 days past Monday, the previous week is also in
     this year. */
  if ((tp->tm_wday + 371 - tp->tm_yday - 2) % 7 <= 2) val++;
  if (!val) {
    val = 52;
    /* If Dec 31 of the previous year is a Thursday, or a Friday of a
       leap year, the previous year has 53 weeks. */
    int dec31 = (tp->tm_wday + 7 - tp->tm_yday - 1) % 7;
    if (dec31 == 4 || (dec31 == 5 && __tm_year_leap((long long)tp->tm_year - 1)))
      val = 53;
  } else if (val == 53) {
    /* If Jan 1 is not a Thursday, and not a Wednesday of a leap year,
       this year has only 52 weeks. */
    int jan1 = (tp->tm_wday + 371 - tp->tm_yday) % 7;
    if (jan1 != 4 && (jan1 != 3 || !__tm_year_leap(tp->tm_year)))
      val = 1;
  }
  return val;
}

static const char *__wday_full[] = {
  "Sunday", "Monday", "Tuesday", "Wednesday",
  "Thursday", "Friday", "Saturday"
};
static const char *__mon_full[] = {
  "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December"
};

size_t strftime(char *s, size_t max, const char *fmt, const struct tm *tp) {
  if (max == 0) return 0;
  size_t pos = 0;
  __strftime_overflow = 0;

  while (*fmt && pos + 1 < max) {
    if (*fmt != '%') {
      s[pos++] = *fmt++;
      continue;
    }
    fmt++; /* skip % */
    /* C23/musl: optional '+' or '0' flag and field width, honoured by
       %C/%F/%G/%Y (musl zeroes the width for every other conversion). */
    int __fmt_plus = 0;
    int __fmt_width = -1;
    if ((*fmt == '+' || *fmt == '0') && fmt[1] >= '0' && fmt[1] <= '9') {
      __fmt_plus = (*fmt == '+');
      fmt++;
    }
    if (*fmt >= '0' && *fmt <= '9') {
      __fmt_width = 0;
      while (*fmt >= '0' && *fmt <= '9') {
        __fmt_width = __fmt_width * 10 + (*fmt - '0');
        fmt++;
      }
    }
    /* C99 E/O locale-alternative modifiers: no locales here, plain forms */
    if (*fmt == 'E' || *fmt == 'O') fmt++;
    switch (*fmt) {
    case 'C': /* century; C23 allows %[+0][width]C */
      __ap_llw(s, max, &pos, ((long long)tp->tm_year + 1900) / 100, 2,
               __fmt_width, __fmt_plus, 0);
      break;
    case 'Y':
      /* musl prints a '+' for years beyond 4 digits; an explicit width
         suppresses it unless the '+' flag asks (see __ap_llw) */
      __ap_llw(s, max, &pos, (long long)tp->tm_year + 1900, 4,
               __fmt_width, __fmt_plus, 1);
      break;
    case 'F': { /* %Y-%m-%d; an explicit width pads the YEAR so the whole
                   string lands on the width (musl: %F is %+4Y-%m-%d) */
      int __yw = __fmt_width >= 0 ? (__fmt_width > 6 ? __fmt_width - 6 : 1) : -1;
      __ap_llw(s, max, &pos, (long long)tp->tm_year + 1900, 4,
               __yw, __fmt_plus, 1);
      __ap_str(s, max, &pos, "-");
      __ap_int(s, max, &pos, tp->tm_mon + 1, 2);
      __ap_str(s, max, &pos, "-");
      __ap_int(s, max, &pos, tp->tm_mday, 2);
      break;
    }
    case 'g':
    case 'G': { /* ISO-8601 week-based year; must agree with %V */
      long long gy = (long long)tp->tm_year + 1900;
      int wn = __iso_week(tp);
      if (tp->tm_yday < 3 && wn != 1) gy--;
      else if (tp->tm_yday > 360 && wn == 1) gy++;
      if (*fmt == 'g') {
        long long g2 = gy % 100;
        if (g2 < 0) g2 = -g2;
        __ap_int(s, max, &pos, (int)g2, 2);
      } else {
        __ap_llw(s, max, &pos, gy, 4, __fmt_width, __fmt_plus, 1);
      }
      break;
    }
    case 'V': __ap_int(s, max, &pos, __iso_week(tp), 2); break;
    case 'm': __ap_int(s, max, &pos, tp->tm_mon + 1, 2); break;
    case 'd': __ap_int(s, max, &pos, tp->tm_mday, 2); break;
    case 'H': __ap_int(s, max, &pos, tp->tm_hour, 2); break;
    case 'M': __ap_int(s, max, &pos, tp->tm_min, 2); break;
    case 'S': __ap_int(s, max, &pos, tp->tm_sec, 2); break;
    case 'a': __ap_str(s, max, &pos, __wday_abbr[tp->tm_wday]); break;
    case 'A': __ap_str(s, max, &pos, __wday_full[tp->tm_wday]); break;
    case 'b': __ap_str(s, max, &pos, __mon_abbr[tp->tm_mon]); break;
    case 'B': __ap_str(s, max, &pos, __mon_full[tp->tm_mon]); break;
    case 'e': /* day of month, space-padded */
      if (tp->tm_mday < 10) __ap_str(s, max, &pos, " ");
      __ap_int(s, max, &pos, tp->tm_mday, tp->tm_mday < 10 ? 1 : 2);
      break;
    case 'c':
      __ap_str(s, max, &pos, __wday_abbr[tp->tm_wday]);
      __ap_str(s, max, &pos, " ");
      __ap_str(s, max, &pos, __mon_abbr[tp->tm_mon]);
      __ap_str(s, max, &pos, " ");
      /* C/POSIX %c: day is space-padded (as by %e), not zero-padded */
      if (tp->tm_mday < 10) __ap_str(s, max, &pos, " ");
      __ap_int(s, max, &pos, tp->tm_mday, tp->tm_mday < 10 ? 1 : 2);
      __ap_str(s, max, &pos, " ");
      __ap_int(s, max, &pos, tp->tm_hour, 2);
      __ap_str(s, max, &pos, ":");
      __ap_int(s, max, &pos, tp->tm_min, 2);
      __ap_str(s, max, &pos, ":");
      __ap_int(s, max, &pos, tp->tm_sec, 2);
      __ap_str(s, max, &pos, " ");
      __ap_llw(s, max, &pos, (long long)tp->tm_year + 1900, 4, -1, 0, 1);
      break;
    case 'I': {
      int h12 = tp->tm_hour % 12;
      __ap_int(s, max, &pos, h12 == 0 ? 12 : h12, 2);
      break;
    }
    case 'p': __ap_str(s, max, &pos, tp->tm_hour < 12 ? "AM" : "PM"); break;
    case 'r': { /* %I:%M:%S %p */
      int h12 = tp->tm_hour % 12;
      __ap_int(s, max, &pos, h12 == 0 ? 12 : h12, 2);
      __ap_str(s, max, &pos, ":");
      __ap_int(s, max, &pos, tp->tm_min, 2);
      __ap_str(s, max, &pos, ":");
      __ap_int(s, max, &pos, tp->tm_sec, 2);
      __ap_str(s, max, &pos, tp->tm_hour < 12 ? " AM" : " PM");
      break;
    }
    case 'j': __ap_int(s, max, &pos, tp->tm_yday + 1, 3); break;
    case 'w': __ap_int(s, max, &pos, tp->tm_wday, 1); break;
    case 'u': __ap_int(s, max, &pos, tp->tm_wday == 0 ? 7 : tp->tm_wday, 1); break;
    case 'y': { /* last two digits of the year; long long so a near-INT_MAX
                   tm_year can't wrap, absolute value for negative years
                   (musl) */
      long long yv = ((long long)tp->tm_year + 1900) % 100;
      if (yv < 0) yv = -yv;
      __ap_int(s, max, &pos, (int)yv, 2);
      break;
    }
    case 'U': __ap_int(s, max, &pos, (tp->tm_yday + 7 - tp->tm_wday) / 7, 2); break;
    case 'W': __ap_int(s, max, &pos, (tp->tm_yday + 7 - (tp->tm_wday ? tp->tm_wday - 1 : 6)) / 7, 2); break;
    case 'x': {
      __ap_int(s, max, &pos, tp->tm_mon + 1, 2);
      __ap_str(s, max, &pos, "/");
      __ap_int(s, max, &pos, tp->tm_mday, 2);
      __ap_str(s, max, &pos, "/");
      long long xv = ((long long)tp->tm_year + 1900) % 100;
      if (xv < 0) xv = -xv;
      __ap_int(s, max, &pos, (int)xv, 2);
      break;
    }
    case 'T': /* %H:%M:%S — same rendering as %X */
    case 'X':
      __ap_int(s, max, &pos, tp->tm_hour, 2);
      __ap_str(s, max, &pos, ":");
      __ap_int(s, max, &pos, tp->tm_min, 2);
      __ap_str(s, max, &pos, ":");
      __ap_int(s, max, &pos, tp->tm_sec, 2);
      break;
    case 'Z': { /* zone abbreviation: the tm's own zone (localtime/gmtime set
                   it), else the published tzname for its DST flag */
      const char *zone = tp->tm_zone;
      if (!zone) { tzset(); zone = tzname[tp->tm_isdst > 0 ? 1 : 0]; }
      if (zone) __ap_str(s, max, &pos, zone);
      break;
    }
    case 'z': { /* +hhmm from tm_gmtoff (localtime fills it in; gmtime's is 0) */
      long off = tp->tm_gmtoff;
      char zbuf[8];
      long a = off < 0 ? -off : off;
      snprintf(zbuf, sizeof zbuf, "%c%02ld%02ld",
               off < 0 ? '-' : '+', a / 3600, (a % 3600) / 60);
      __ap_str(s, max, &pos, zbuf);
      break;
    }
    case 's': { /* Read fields as UTC minus tm_gmtoff (musl semantics),
       independent of the current TZ setting. This round-trips both gmtime
       and localtime results, including when TZ changed after conversion.
       See docs/archive/0310 for the original compatibility decision. */
      char sbuf[24];
      snprintf(sbuf, sizeof sbuf, "%lld", __tm_to_secs_utc(tp) - tp->tm_gmtoff);
      __ap_str(s, max, &pos, sbuf);
      break;
    }
    case '%': s[pos++] = '%'; break;
    case 'n': s[pos++] = '\n'; break;
    case 't': s[pos++] = '\t'; break;
    case '\0': goto done;
    default:
      s[pos++] = '%';
      if (pos + 1 < max) s[pos++] = *fmt;
      break;
    }
    fmt++;
  }
done:
  s[pos] = '\0';
  if (*fmt || __strftime_overflow) return 0;
  return pos;
}

/* ---- strptime (ticket #113 / docs/archive/0307) ------------------------------
 *
 * The parse twin of strftime, over the SAME conversion vocabulary and name
 * tables (the tickets group them for that reason). POSIX XSI set plus the
 * glibc extensions the libc-test corpus exercises: %F, %s, %z. Rules:
 *  - fields not named by the format are left untouched (POSIX);
 *  - whitespace in the format matches zero or more input whitespace;
 *  - conversions skip leading input whitespace and accept an optional
 *    maximum field width (glibc shape: %2d);
 *  - %y is relative: 69-99 -> 19xx, 00-68 -> 20xx, and combines with an
 *    explicit %C wherever the two appear (glibc semantics);
 *  - %s reads the epoch count as UTC (gmtime_r), the parse-side face of
 *    the #116 decision. This remains independent of the current TZ setting.
 */

static int __pt_isspace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\v' ||
         c == '\f' || c == '\r';
}

/* Up-to-maxw digits -> *out, range-checked; skips leading whitespace.
   Returns the advanced cursor, or 0 on no digits / out of range. */
static const char *__pt_num(const char *s, int maxw, int lo, int hi, int *out) {
  while (__pt_isspace(*s)) s++;
  int v = 0, n = 0;
  while (n < maxw && *s >= '0' && *s <= '9') {
    v = v * 10 + (*s - '0');
    s++;
    n++;
  }
  if (n == 0 || v < lo || v > hi) return 0;
  *out = v;
  return s;
}

/* Case-insensitive prefix match; returns the matched length (0 = no). */
static int __pt_prefix_ci(const char *s, const char *name) {
  int n = 0;
  while (name[n]) {
    char a = s[n], b = name[n];
    if (a >= 'A' && a <= 'Z') a += 32;
    if (b >= 'A' && b <= 'Z') b += 32;
    if (a != b) return 0;
    n++;
  }
  return n;
}

/* Longest case-insensitive match against full names + abbreviations. */
static const char *__pt_name(const char *s, const char *const *full,
                             const char *const *abbr, int count, int *out) {
  int best = 0, bi = -1;
  for (int i = 0; i < count; i++) {
    int n = __pt_prefix_ci(s, full[i]);
    if (n > best) { best = n; bi = i; }
    n = __pt_prefix_ci(s, abbr[i]);
    if (n > best) { best = n; bi = i; }
  }
  if (bi < 0) return 0;
  *out = bi;
  return s + best;
}

char *strptime(const char *s, const char *fmt, struct tm *tm) {
  int century = -1, relyear = -1, want_ampm = -1;
  for (; *fmt; fmt++) {
    if (__pt_isspace(*fmt)) {
      while (__pt_isspace(*s)) s++;
      while (__pt_isspace(fmt[1])) fmt++;
      continue;
    }
    if (*fmt != '%') {
      if (*s != *fmt) return 0;
      s++;
      continue;
    }
    fmt++;
    /* optional maximum field width (glibc) */
    int w = -1;
    if (*fmt >= '1' && *fmt <= '9') {
      w = 0;
      while (*fmt >= '0' && *fmt <= '9') { w = w * 10 + (*fmt - '0'); fmt++; }
    }
    if (*fmt == 'E' || *fmt == 'O') fmt++;  /* locale modifiers: plain forms */
    int v;
    const char *r;
    switch (*fmt) {
    case 'a': case 'A':
      if (!(s = __pt_name(s, __wday_full, __wday_abbr, 7, &v))) return 0;
      tm->tm_wday = v;
      break;
    case 'b': case 'B': case 'h':
      if (!(s = __pt_name(s, __mon_full, __mon_abbr, 12, &v))) return 0;
      tm->tm_mon = v;
      break;
    case 'c': /* what our strftime %c writes */
      if (!(r = strptime(s, "%a %b %e %H:%M:%S %Y", tm))) return 0;
      s = r;
      break;
    case 'C':
      if (!(s = __pt_num(s, w < 0 ? 2 : w, 0, 99, &v))) return 0;
      century = v;
      break;
    case 'd': case 'e':
      if (!(s = __pt_num(s, w < 0 ? 2 : w, 1, 31, &v))) return 0;
      tm->tm_mday = v;
      break;
    case 'D':
      if (!(r = strptime(s, "%m/%d/%y", tm))) return 0;
      s = r;
      break;
    case 'F': /* glibc: %Y-%m-%d */
      if (!(r = strptime(s, "%Y-%m-%d", tm))) return 0;
      s = r;
      break;
    case 'H':
      if (!(s = __pt_num(s, w < 0 ? 2 : w, 0, 23, &v))) return 0;
      tm->tm_hour = v;
      break;
    case 'I':
      if (!(s = __pt_num(s, w < 0 ? 2 : w, 1, 12, &v))) return 0;
      tm->tm_hour = v;
      break;
    case 'j':
      if (!(s = __pt_num(s, w < 0 ? 3 : w, 1, 366, &v))) return 0;
      tm->tm_yday = v - 1;
      break;
    case 'm':
      if (!(s = __pt_num(s, w < 0 ? 2 : w, 1, 12, &v))) return 0;
      tm->tm_mon = v - 1;
      break;
    case 'M':
      if (!(s = __pt_num(s, w < 0 ? 2 : w, 0, 59, &v))) return 0;
      tm->tm_min = v;
      break;
    case 'n': case 't':
      while (__pt_isspace(*s)) s++;
      break;
    case 'p':
      if (__pt_prefix_ci(s, "AM")) { want_ampm = 0; s += 2; }
      else if (__pt_prefix_ci(s, "PM")) { want_ampm = 1; s += 2; }
      else return 0;
      break;
    case 'r':
      if (!(r = strptime(s, "%I:%M:%S %p", tm))) return 0;
      s = r;
      break;
    case 'R':
      if (!(r = strptime(s, "%H:%M", tm))) return 0;
      s = r;
      break;
    case 'S':
      if (!(s = __pt_num(s, w < 0 ? 2 : w, 0, 60, &v))) return 0;
      tm->tm_sec = v;
      break;
    case 's': { /* glibc: seconds since the epoch. Read as UTC — the parse
                   side of the #116 decision (see strftime %s). */
      long long sec = 0;
      int n = 0, neg = 0;
      while (__pt_isspace(*s)) s++;
      if (*s == '-') { neg = 1; s++; }
      while (*s >= '0' && *s <= '9') { sec = sec * 10 + (*s - '0'); s++; n++; }
      if (!n) return 0;
      if (neg) sec = -sec;
      time_t t = (time_t)sec;
      gmtime_r(&t, tm);
      break;
    }
    case 'T':
      if (!(r = strptime(s, "%H:%M:%S", tm))) return 0;
      s = r;
      break;
    case 'u':
      if (!(s = __pt_num(s, w < 0 ? 1 : w, 1, 7, &v))) return 0;
      tm->tm_wday = v % 7;
      break;
    case 'U': case 'W': /* week number: parsed and range-checked, but there
                           is no field to store it in without a year+wday
                           resolution pass; POSIX permits ignoring it */
      if (!(s = __pt_num(s, w < 0 ? 2 : w, 0, 53, &v))) return 0;
      break;
    case 'w':
      if (!(s = __pt_num(s, w < 0 ? 1 : w, 0, 6, &v))) return 0;
      tm->tm_wday = v;
      break;
    case 'x':
      if (!(r = strptime(s, "%m/%d/%y", tm))) return 0;
      s = r;
      break;
    case 'X':
      if (!(r = strptime(s, "%H:%M:%S", tm))) return 0;
      s = r;
      break;
    case 'y':
      if (!(s = __pt_num(s, w < 0 ? 2 : w, 0, 99, &v))) return 0;
      relyear = v;
      break;
    case 'Y':
      if (!(s = __pt_num(s, w < 0 ? 4 : w, 0, w < 0 ? 9999 : 99999999, &v)))
        return 0;
      tm->tm_year = v - 1900;
      break;
    case 'z': { /* [+-]hh[[:]mm], or Z for UTC -> tm_gmtoff */
      while (__pt_isspace(*s)) s++;
      if (*s == 'Z') { s++; tm->tm_gmtoff = 0; break; }
      int zsign;
      if (*s == '+') zsign = 1;
      else if (*s == '-') zsign = -1;
      else return 0;
      s++;
      int hh, mm = 0;
      if (!(s = __pt_num(s, 2, 0, 23, &hh))) return 0;
      if (*s == ':') {
        s++;
        if (!(s = __pt_num(s, 2, 0, 59, &mm))) return 0;
      } else if (*s >= '0' && *s <= '9') {
        if (!(s = __pt_num(s, 2, 0, 59, &mm))) return 0;
      }
      tm->tm_gmtoff = zsign * (hh * 3600L + mm * 60L);
      break;
    }
    case 'Z': { /* no zone database: accept an alphabetic zone name or the
                   numeric +hhmm shape our tzname publishes; no field set */
      int n = 0;
      if (*s == '+' || *s == '-') {
        s++;
        n++;
        while (*s >= '0' && *s <= '9') { s++; n++; }
      } else {
        while ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z')) {
          s++;
          n++;
        }
      }
      if (!n) return 0;
      break;
    }
    case '%':
      if (*s != '%') return 0;
      s++;
      break;
    default:
      return 0;
    }
  }
  if (century >= 0 || relyear >= 0) {
    int yy;
    if (century >= 0) yy = century * 100 + (relyear >= 0 ? relyear : 0);
    else yy = relyear + (relyear <= 68 ? 2000 : 1900);
    tm->tm_year = yy - 1900;
  }
  if (want_ampm >= 0) {
    if (tm->tm_hour == 12) tm->tm_hour = 0;
    if (want_ampm) tm->tm_hour += 12;
  }
  return (char *)s;
}

__import double __clock_ms(int clk_id);

int clock_gettime(clockid_t clk_id, struct timespec *tp) {
  /* One host sample in milliseconds (CLOCK_REALTIME is epoch-anchored and
     whole milliseconds, CLOCK_MONOTONIC never goes backwards). Both fields
     come from that one value, so they always describe the same instant. */
  double ms = __clock_ms(clk_id);
  long long sec = (long long)(ms / 1000.0);
  long nsec = (long)((ms - (double)sec * 1000.0) * 1e6 + 0.5);
  if (nsec > 999999999L) nsec = 999999999L;
  if (nsec < 0) nsec = 0;
  tp->tv_sec = sec;
  tp->tv_nsec = nsec;
  return 0;
}

/* clock_getres (POSIX, docs/archive/0325 Group A — CPython's pytime.c calls it
   inside an unguarded HAVE_CLOCK_GETTIME block, so there is no way to
   configure around it).
 *
 * These are the granularities of the clock SOURCES, reported conservatively
 * rather than optimistically:
 *   CLOCK_REALTIME  — Date.now(), integer milliseconds. Exactly 1ms.
 *   CLOCK_MONOTONIC — performance.now(), a sub-millisecond double. Hosts
 *                     deliberately CLAMP it (browsers to 5us-100us as an
 *                     anti-timing-attack measure, Node much finer), so the
 *                     honest floor is the resolution the API expresses, 1us,
 *                     and a caller must not read it as a promise of
 *                     distinguishable consecutive samples. */
/* clock_nanosleep (POSIX, docs/archive/0325 Group B). RETURNS an error number and
   does NOT set errno — the same inversion as posix_fallocate. TIMER_ABSTIME
   sleeps until a deadline rather than for a duration, which is the whole
   reason timemodule.c wants it: a relative sleep recomputed in userspace
   drifts, and re-arming after a signal overshoots. */
int clock_nanosleep(clockid_t clk_id, int flags, const struct timespec *req,
                    struct timespec *rem) {
  if (!req || req->tv_nsec < 0 || req->tv_nsec > 999999999L) return EINVAL;
  if (clk_id != CLOCK_REALTIME && clk_id != CLOCK_MONOTONIC) return EINVAL;

  long long sec = req->tv_sec;
  long nsec = req->tv_nsec;

  if (flags & TIMER_ABSTIME) {
    struct timespec now;
    if (clock_gettime(clk_id, &now) != 0) return EINVAL;
    sec -= now.tv_sec;
    nsec -= now.tv_nsec;
    if (nsec < 0) { nsec += 1000000000L; sec -= 1; }
    /* A deadline already past is not an error: it returns immediately. */
    if (sec < 0) return 0;
  } else if (sec < 0) {
    return EINVAL;
  }

  if (__nanosleep((long)sec, nsec) != 0) return EINTR;
  /* rem is only meaningful for a relative sleep interrupted by a signal;
     __nanosleep does not report a partial remainder, so report none. */
  if (rem && !(flags & TIMER_ABSTIME)) { rem->tv_sec = 0; rem->tv_nsec = 0; }
  return 0;
}

int clock_getres(clockid_t clk_id, struct timespec *res) {
  long ns;
  if (clk_id == CLOCK_REALTIME) ns = 1000000L;
  else if (clk_id == CLOCK_MONOTONIC) ns = 1000L;
  else { errno = EINVAL; return -1; }
  if (res) { res->tv_sec = 0; res->tv_nsec = ns; }
  return 0;
}
  