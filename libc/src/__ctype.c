
/* isascii/toascii (XSI, docs/archive/0325 Group A). Defined over the whole int
   range by design — no undefined behaviour for negative or large values. */
int isascii(int c) { return (unsigned)c < 128u; }
int toascii(int c) { return c & 0x7f; }

int isdigit(int c) { return c >= '0' && c <= '9'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int isalpha(int c) { return islower(c) || isupper(c); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isblank(int c) { return c == ' ' || c == '\t'; }
int iscntrl(int c) { return (c >= 0 && c < 32) || c == 127; }
int isprint(int c) { return c >= 32 && c <= 126; }
int isgraph(int c) { return c > 32 && c <= 126; }
int isspace(int c) {
  return c == ' ' || c == '\t' || c == '\n' ||
       c == '\r' || c == '\f' || c == '\v';
}
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
int isxdigit(int c) {
  return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
int tolower(int c) { return isupper(c) ? c + ('a' - 'A') : c; }
int toupper(int c) { return islower(c) ? c + ('A' - 'a') : c; }
  