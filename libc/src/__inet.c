
/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <arpa/inet.h>
#include <errno.h>
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define UV_EINVAL (-EINVAL)
#define UV_ENOSPC (-ENOSPC)
#define UV_EAFNOSUPPORT (-EAFNOSUPPORT)
#define uv__strscpy(dst,src,size) strcpy(dst,src)

#define UV__INET_ADDRSTRLEN         16
#define UV__INET6_ADDRSTRLEN        46


static int inet_ntop4(const unsigned char *src, char *dst, size_t size);
static int inet_ntop6(const unsigned char *src, char *dst, size_t size);
static int inet_pton4(const char *src, unsigned char *dst);
static int inet_pton6(const char *src, unsigned char *dst);


static int numeric_ntop(int af, const void* src, char* dst, size_t size) {
  switch (af) {
  case AF_INET:
    return (inet_ntop4(src, dst, size));
  case AF_INET6:
    return (inet_ntop6(src, dst, size));
  default:
    return UV_EAFNOSUPPORT;
  }
  /* NOTREACHED */
}


static int inet_ntop4(const unsigned char *src, char *dst, size_t size) {
  static const char fmt[] = "%u.%u.%u.%u";
  char tmp[UV__INET_ADDRSTRLEN];
  int l;

  l = snprintf(tmp, sizeof(tmp), fmt, src[0], src[1], src[2], src[3]);
  if (l <= 0 || (size_t) l >= size) {
    return UV_ENOSPC;
  }
  uv__strscpy(dst, tmp, size);
  return 0;
}


static int inet_ntop6(const unsigned char *src, char *dst, size_t size) {
  /*
   * Note that int32_t and int16_t need only be "at least" large enough
   * to contain a value of the specified size.  On some systems, like
   * Crays, there is no such thing as an integer variable with 16 bits.
   * Keep this in mind if you think this function should have been coded
   * to use pointer overlays.  All the world's not a VAX.
   */
  char tmp[UV__INET6_ADDRSTRLEN], *tp;
  struct { int base, len; } best, cur;
  unsigned int words[sizeof(struct in6_addr) / sizeof(uint16_t)];
  int i;

  /*
   * Preprocess:
   *  Copy the input (bytewise) array into a wordwise array.
   *  Find the longest run of 0x00's in src[] for :: shorthanding.
   */
  memset(words, '\0', sizeof words);
  for (i = 0; i < (int) sizeof(struct in6_addr); i++)
    words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
  best.base = -1;
  best.len = 0;
  cur.base = -1;
  cur.len = 0;
  for (i = 0; i < (int) ARRAY_SIZE(words); i++) {
    if (words[i] == 0) {
      if (cur.base == -1)
        cur.base = i, cur.len = 1;
      else
        cur.len++;
    } else {
      if (cur.base != -1) {
        if (best.base == -1 || cur.len > best.len)
          best = cur;
        cur.base = -1;
      }
    }
  }
  if (cur.base != -1) {
    if (best.base == -1 || cur.len > best.len)
      best = cur;
  }
  if (best.base != -1 && best.len < 2)
    best.base = -1;

  /*
   * Format the result.
   */
  tp = tmp;
  for (i = 0; i < (int) ARRAY_SIZE(words); i++) {
    /* Are we inside the best run of 0x00's? */
    if (best.base != -1 && i >= best.base &&
        i < (best.base + best.len)) {
      if (i == best.base)
        *tp++ = ':';
      continue;
    }
    /* Are we following an initial run of 0x00s or any real hex? */
    if (i != 0)
      *tp++ = ':';
    /* Is this address an encapsulated IPv4? */
    if (i == 6 && best.base == 0 && (best.len == 6 ||
        (best.len == 7 && words[7] != 0x0001) ||
        (best.len == 5 && words[5] == 0xffff))) {
      int err = inet_ntop4(src+12, tp, sizeof tmp - (tp - tmp));
      if (err)
        return err;
      tp += strlen(tp);
      break;
    }
    tp += snprintf(tp, sizeof tmp - (tp - tmp), "%x", words[i]);
  }
  /* Was it a trailing run of 0x00's? */
  if (best.base != -1 && (best.base + best.len) == ARRAY_SIZE(words))
    *tp++ = ':';
  *tp++ = '\0';
  if ((size_t) (tp - tmp) > size)
    return UV_ENOSPC;
  uv__strscpy(dst, tmp, size);
  return 0;
}


static int numeric_pton(int af, const char* src, void* dst) {
  if (src == NULL || dst == NULL)
    return UV_EINVAL;

  switch (af) {
  case AF_INET:
    return (inet_pton4(src, dst));
  case AF_INET6: {
    int len;
    char tmp[UV__INET6_ADDRSTRLEN], *s, *p;
    s = (char*) src;
    p = strchr(src, '%');
    if (p != NULL) {
      s = tmp;
      len = p - src;
      if (len > UV__INET6_ADDRSTRLEN-1)
        return UV_EINVAL;
      memcpy(s, src, len);
      s[len] = '\0';
    }
    return inet_pton6(s, dst);
  }
  default:
    return UV_EAFNOSUPPORT;
  }
  /* NOTREACHED */
}


static int inet_pton4(const char *src, unsigned char *dst) {
  static const char digits[] = "0123456789";
  int saw_digit, octets, ch;
  unsigned char tmp[sizeof(struct in_addr)], *tp;

  saw_digit = 0;
  octets = 0;
  *(tp = tmp) = 0;
  while ((ch = *src++) != '\0') {
    const char *pch;

    if ((pch = strchr(digits, ch)) != NULL) {
      unsigned int nw = *tp * 10 + (pch - digits);

      if (saw_digit && *tp == 0)
        return UV_EINVAL;
      if (nw > 255)
        return UV_EINVAL;
      *tp = nw;
      if (!saw_digit) {
        if (++octets > 4)
          return UV_EINVAL;
        saw_digit = 1;
      }
    } else if (ch == '.' && saw_digit) {
      if (octets == 4)
        return UV_EINVAL;
      *++tp = 0;
      saw_digit = 0;
    } else
      return UV_EINVAL;
  }
  if (octets < 4)
    return UV_EINVAL;
  memcpy(dst, tmp, sizeof(struct in_addr));
  return 0;
}


static int inet_pton6(const char *src, unsigned char *dst) {
  static const char xdigits_l[] = "0123456789abcdef",
                    xdigits_u[] = "0123456789ABCDEF";
  unsigned char tmp[sizeof(struct in6_addr)], *tp, *endp, *colonp;
  const char *xdigits, *curtok;
  int ch, seen_xdigits;
  unsigned int val;

  memset((tp = tmp), '\0', sizeof tmp);
  endp = tp + sizeof tmp;
  colonp = NULL;
  /* Leading :: requires some special handling. */
  if (*src == ':')
    if (*++src != ':')
      return UV_EINVAL;
  curtok = src;
  seen_xdigits = 0;
  val = 0;
  while ((ch = *src++) != '\0') {
    const char *pch;

    if ((pch = strchr((xdigits = xdigits_l), ch)) == NULL)
      pch = strchr((xdigits = xdigits_u), ch);
    if (pch != NULL) {
      val <<= 4;
      val |= (pch - xdigits);
      if (++seen_xdigits > 4)
        return UV_EINVAL;
      continue;
    }
    if (ch == ':') {
      curtok = src;
      if (!seen_xdigits) {
        if (colonp)
          return UV_EINVAL;
        colonp = tp;
        continue;
      } else if (*src == '\0') {
        return UV_EINVAL;
      }
      if (tp + sizeof(uint16_t) > endp)
        return UV_EINVAL;
      *tp++ = (unsigned char) (val >> 8) & 0xff;
      *tp++ = (unsigned char) val & 0xff;
      seen_xdigits = 0;
      val = 0;
      continue;
    }
    if (ch == '.' && ((tp + sizeof(struct in_addr)) <= endp)) {
      int err = inet_pton4(curtok, tp);
      if (err == 0) {
        tp += sizeof(struct in_addr);
        seen_xdigits = 0;
        break;  /*%< '\\0' was seen by inet_pton4(). */
      }
    }
    return UV_EINVAL;
  }
  if (seen_xdigits) {
    if (tp + sizeof(uint16_t) > endp)
      return UV_EINVAL;
    *tp++ = (unsigned char) (val >> 8) & 0xff;
    *tp++ = (unsigned char) val & 0xff;
  }
  if (colonp != NULL) {
    /*
     * Since some memmove()'s erroneously fail to handle
     * overlapping regions, we'll do the shift by hand.
     */
    const int n = tp - colonp;
    int i;

    if (tp == endp)
      return UV_EINVAL;
    for (i = 1; i <= n; i++) {
      endp[- i] = colonp[n - i];
      colonp[n - i] = 0;
    }
    tp = endp;
  }
  if (tp != endp)
    return UV_EINVAL;
  memcpy(dst, tmp, sizeof tmp);
  return 0;
}

const char *inet_ntop(int family,const void *src,char *dst,socklen_t size) {
 int result=numeric_ntop(family,src,dst,size);
 if(result<0){errno=-result;return NULL;}return dst;
}
int inet_pton(int family,const char *src,void *dst) {
 if(family!=AF_INET&&family!=AF_INET6){errno=EAFNOSUPPORT;return -1;}
 if(!src||!dst){errno=EFAULT;return -1;}
 if(strchr(src,'%'))return 0;
 int result=numeric_pton(family,src,dst);
 return result==0?1:0;
}

int inet_aton(const char *text,struct in_addr *out){
 unsigned parts[4];int n=0;const char *p=text;
 if(!p||!*p)return 0;
 for(;;){
  if(n==4||*p<'0'||*p>'9')return 0;
  unsigned base=10,value=0,digits=0;
  if(*p=='0'){base=8;p++;digits=1;if(*p=='x'||*p=='X'){base=16;p++;digits=0;}}
  for(;;){unsigned d;if(*p>='0'&&*p<='9')d=*p-'0';else if(*p>='a'&&*p<='f')d=*p-'a'+10;else if(*p>='A'&&*p<='F')d=*p-'A'+10;else break;
   if(d>=base||value>(0xffffffffU-d)/base)return 0;value=value*base+d;digits++;p++;
  }
  if(!digits)return 0;parts[n++]=value;if(*p!='.')break;p++;
 }
 while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')p++;
 if(*p)return 0;
 unsigned value=parts[n-1];unsigned max=n==1?0xffffffffU:(1U<<(8*(5-n)))-1;
 if(value>max)return 0;
 for(int i=0;i<n-1;i++){if(parts[i]>255)return 0;value|=parts[i]<<(8*(3-i));}
 if(out)out->s_addr=htonl(value);return 1;
}

  