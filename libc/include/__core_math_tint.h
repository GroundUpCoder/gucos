/* Fast 192-bit arithmetic routines.

Copyright (c) 2023-2024 Paul Zimmermann and Alexei Sibidanov

This file is part of the CORE-MATH project
(https://core-math.gitlabpages.inria.fr/).

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdint.h>

/* wasm-posix: fixed three-limb representation; no compiler __int128 support.
   The operations below preserve CORE-MATH's truncation and error bounds. */
typedef struct { uint64_t h, m, l; int64_t ex; uint64_t sgn; } tint_t;
typedef struct { uint64_t hi, lo; } cm_u128;
static inline cm_u128 cm_mul64(uint64_t a, uint64_t b) {
  uint64_t a0=(uint32_t)a, a1=a>>32, b0=(uint32_t)b, b1=b>>32;
  uint64_t w0=a0*b0, t=a1*b0+(w0>>32), w1=(uint32_t)t, w2=t>>32;
  w1 += a0*b1;
  cm_u128 r={a1*b1+w2+(w1>>32), (w1<<32)|(uint32_t)w0};
  return r;
}

// ZERO is a tint_t representation of 0
static const tint_t ZERO = {.h = 0, .m = 0, .l = 0, .ex = -1076, .sgn = 0};

// ONE is a tint_t representation of 1
static const tint_t ONE = {
  .h = 0x8000000000000000, .m = 0, .l = 0, .ex = 1, .sgn = 0};

// PI is a tint_t representation of pi, with error < 2^-196.96
static const tint_t PI = {
  .h = 0xc90fdaa22168c234, .m = 0xc4c6628b80dc1cd1, .l = 0x29024e088a67cc74,
  .ex = 2, .sgn = 0};

// PI2 is a tint_t representation of pi/2, with error < 2^-197.96
static const tint_t PI2 = {
  .h = 0xc90fdaa22168c234, .m = 0xc4c6628b80dc1cd1, .l = 0x29024e088a67cc74,
  .ex = 1, .sgn = 0};

// Copy a tint_t value
static inline void cp_tint(tint_t *r, const tint_t *a) {
  *r = *a;
}

static inline int
is_normalized (const tint_t *a)
{
  if (a->h == 0 && a->m == 0 && a->l == 0)
    return 1;
  return a->h >> 63;
}

// Multiply two tint_t numbers, with error < 10 ulps or 2^-187.67 * |r|
// Overlap between r and a or b is allowed
static inline void
mul_tint (tint_t *r, const tint_t *a, const tint_t *b)
{
  r->ex = a->ex + b->ex;
  r->sgn = a->sgn ^ b->sgn;

  uint64_t ah=a->h, am=a->m, al=a->l, bh=b->h, bm=b->m, bl=b->l;
  cm_u128 rh=cm_mul64(ah,bh), rm1=cm_mul64(ah,bm), rm2=cm_mul64(am,bh);
  cm_u128 rl1=cm_mul64(ah,bl), rl2=cm_mul64(am,bm), rl3=cm_mul64(al,bh);
  r->h=rh.hi; r->m=rh.lo; r->l=rm1.lo;
  r->m+=rm1.hi; r->h+=r->m<rm1.hi;
  r->l+=rm2.lo;
  uint64_t cm=r->l<rm2.lo;
  r->m+=rm2.hi; r->h+=r->m<rm2.hi;
  uint64_t low=rl1.hi+rl2.hi, carry=low<rl1.hi;
  low+=rl3.hi; carry+=low<rl3.hi;
  cm+=carry;
  r->l+=low; cm+=r->l<low;
  r->m+=cm; r->h+=r->m<cm;

  /* Note: if one of the operands was zero, then r->h = r->m = r->l = 0,
     and the normalization keeps r=0. */
  if (!(r->h >> 63)) // normalize
  {
    r->h = (r->h << 1) | (r->m >> 63);
    r->m = (r->m << 1) | (r->l >> 63);
    r->l = r->l << 1;
    r->ex --;
  }

  /* We ignored the following terms, denoting B=2^64, related to r->l:
     am*bl + al*bm <= 2*(B-1)^2/B^2 = 2 - 4/B + 2/B^2
     al*bl <= (B-1)^2/B^3 = 1/B - 2/B^2 + 1/B^3
     And we truncated rl1+rl2+rl3:
     low(rl1) + low(rl2) + low(rl3) <= 3*(B-1)/B = 3-3/B
     This sums up to: 5 - 6/B + 1/B^3 < 5
     thus the rounding error is bounded by 5 ulps,
     and after normalization by 10 ulps. */
}

// Return non-zero if a = 0
static inline int
tint_zero_p (const tint_t *a)
{
  return a->h == 0;
}

static inline int cmp(int64_t a, int64_t b) { return (a > b) - (a < b); }
static inline int cmpu64(uint64_t a, uint64_t b) { return (a > b) - (a < b); }

// Compare the absolute values of a and b
// Return -1 if |a| < |b|
// Return  0 if |a| = |b|
// Return +1 if |a| > |b|
static inline int
cmp_tint_abs (const tint_t *a, const tint_t *b) {
  if (tint_zero_p (a))
    return tint_zero_p (b) ? 0 : -1;
  if (tint_zero_p (b))
    return +1;
  int c = cmp (a->ex, b->ex);
  if (c)
    return c;
  // now a->ex = b->ex
  c = cmpu64 (a->h, b->h);
  if (!c) c = cmpu64 (a->m, b->m);
  if (c)
    return c;
  return cmpu64 (a->l, b->l);
}

// shift right by k bits (only deal with the significand)
static inline void
rshift (tint_t *a, const tint_t *b, int k)
{
  uint64_t h=b->h,m=b->m,l=b->l;
  if (k == 0) { a->h=h; a->m=m; a->l=l; }
  else if (k < 64) { a->h=h>>k; a->m=(h<<(64-k))|(m>>k); a->l=(m<<(64-k))|(l>>k); }
  else if (k == 64) { a->h=0; a->m=h; a->l=m; }
  else if (k < 128) { a->h=0; a->m=h>>(k-64); a->l=(h<<(128-k))|(m>>(k-64)); }
  else if (k < 192) { a->h=0; a->m=0; a->l=h>>(k-128); }
  else { a->h=a->m=a->l=0; }
}

// Shift only the significand; locals preserve overlapping input/output.
static inline void lshift(tint_t *a, const tint_t *b, int k) {
  uint64_t h=b->h,m=b->m,l=b->l;
  if (k == 0) { a->h=h; a->m=m; a->l=l; }
  else if (k < 64) { a->h=(h<<k)|(m>>(64-k)); a->m=(m<<k)|(l>>(64-k)); a->l=l<<k; }
  else if (k == 64) { a->h=m; a->m=l; a->l=0; }
  else if (k < 128) { a->h=(m<<(k-64))|(l>>(128-k)); a->m=l<<(k-64); a->l=0; }
  else if (k < 192) { a->h=l<<(k-128); a->m=a->l=0; }
  else { a->h=a->m=a->l=0; }
}

// Unsigned three-limb subtraction, modulo 2^192. Input may alias output.
static inline void cm_sub(tint_t *r, const tint_t *a, const tint_t *b) {
  uint64_t l=a->l-b->l, borrow=a->l<b->l;
  uint64_t m=a->m-b->m, carry=a->m<b->m;
  carry |= m<borrow; m-=borrow;
  uint64_t h=a->h-b->h-carry;
  r->l=l; r->m=m; r->h=h;
}
static inline unsigned cm_clz(const tint_t *a) {
  return a->h ? __builtin_clzll(a->h) : a->m ? 64+__builtin_clzll(a->m) : 128+__builtin_clzll(a->l);
}

// Add two tint_t values, with error bounded by 2 ulps
static inline void
add_tint (tint_t *r, const tint_t *a, const tint_t *b)
{
  switch (cmp_tint_abs (a, b))
  {
  case 0: // |a| = |b|, return 0 or 2*a (exact)
    if (a->sgn ^ b->sgn) {
      cp_tint (r, &ZERO);
      return;
    }
    cp_tint (r, a);
    r->ex++;
    return;

  case -1: // |a| < |b|
    {
      // swap operands
      const tint_t *tmp = a; a = b; b = tmp;
      break; // fall through the case |a| > |b|
    }
  }

  // From now on, |a| > |b| thus a->ex >= b->ex
  tint_t t[1];
  uint64_t sh = a->ex - b->ex;
  rshift (t, b, sh);

  if (a->sgn ^ b->sgn) { // opposite signs, it's a subtraction
    cm_sub(t,a,t);
    uint64_t ex=cm_clz(t);
    if (ex <= 1 || sh == 0) {
      /* The maximal error of 1 ulp for the neglected low part of b is shifted
         by ex bits, thus contributes to < 2 ulps. And for sh=0, there is no
         neglected low part of b, thus the subtraction is exact. */
      lshift (r, t, ex);
      r->ex = a->ex - ex;
    }
    else { // ex >= 2 and sh >= 1
      /* Since we had a cancellation of ex bits with the neglected low part
         of b, we'll have a cancellation of at least ex bits if we don't
         neglect the low part of b, thus we can shift left a and b/2^sh by
         ex bits, and subtract. This case (ex >= 2) can only occur when sh=1,
         since for sh>=2 we have (up to the exponent) a >= 1 and b < 0.5,
         thus a-b >= 0.5, and the exponent decrease ex is at most 1. */
      lshift (t, b, ex - sh);
      lshift (r, a, ex);
      cm_sub(t,r,t);
      uint64_t ex1=cm_clz(t);
      lshift (r, t, ex1);
      r->ex = a->ex - (ex + ex1);
      /* Since we shifted b left in this case, there is no neglected bit of b,
         and the subtraction is exact. */
    }      
  }
  else { // same signs, it's an addition
    uint64_t ah=a->h, am=a->m, al=a->l;
    uint64_t rl=al+t->l, carry=rl<al;
    uint64_t rm=am+t->m, next=rm<am;
    next |= rm+carry<rm; rm+=carry;
    uint64_t rh=ah+t->h, ch=rh<ah;
    ch |= rh+next<rh; rh+=next;
    r->l=rl; r->m=rm; r->h=rh;
    if (ch) {
      r->ex = a->ex + 1;
      r->l = (rm<<63)|(rl>>1);
      r->m = (rh<<63)|(rm>>1);
      r->h = (ch<<63)|(rh>>1);
      /* the maximal error from the shifted part of b is now < 1/2 ulp(r),
         and in addition the low bit of r->_l that disappeared might give
         1/2 ulp(r), thus the total error is still < ulp(r) */
    }
    else
      r->ex = a->ex;
    // in the addition case, the error is bounded by ulp(r)
  }
  r->sgn = a->sgn;
}

// a <- x, assuming x is not NaN, Inf or 0
// This operation is exact
static inline void tint_fromd (tint_t *a, double x)
{
  d64u64 u = {.f = x};
  a->sgn = u.u >> 63;
  uint64_t ax = u.u & 0x7fffffffffffffffull;
  int64_t e = ax >> 52;
  if (__builtin_expect (e, 1)) { // normal
    // 1 has e=0x3ff
    a->ex = e - 0x3fe;
    a->h = (1ull << 63) | (ax << 11);
  }
  else { // subnormal
    // 2^-1074 has ax=1
    e = __builtin_clzll (ax);
    a->ex = -0x3f2 - e;
    a->h = ax << e;
  }
  a->m = a->l = 0;
}

// copied from ../exp/exp.c
static inline double as_ldexp(double x, int64_t i){
    d64u64 ix = {.f = x};
    ix.u += (uint64_t)i<<52;
    return ix.f;
}

// convert a to a double with correct rouding
// If err=0, we are converting a double value.
// Otherwise, err is a bound in ulps on the maximal error on a->l
// y,x are the inputs of atan2 (in case we can't round correctly)
static inline double
tint_tod (const tint_t *a, uint64_t err, double y, double x)
{
  if (a->ex >= 1025) // overflow: |a| >= 2^1024
    return a->sgn ? -0x1p1023 - 0x1p1023 : 0x1p1023 + 0x1p1023;
  if (a->ex <= -1074) // underflow: |a| < 2^-1074
  {
    if (a->ex < -1074) // |a| < 2^-1075
      return (a->sgn ? -0x1p-1074 : 0x1p-1074) * 0.5;
    // 2^-1075 <= |a| < 2^-1074
    int mid = a->h == (1ull << 63) && a->m == 0 && a->l == 0;
    // if mid, |a| = 2^-1075
    return (a->sgn ? -0x1p-1074 : 0x1p-1074) * (mid ? 0.5 : 0.75);
  }
#define MASK53 0x1ffffffffffffful
  uint64_t hh = a->h, mm = a->m, ll = a->l;
  int ex = a->ex;
  uint64_t low = hh & 0x7ff; // low 11 bits from a->h
  /* We can't determine the correct rounding when:
     (a) a->m = 0 and the low 10 bits of a->h are zero and a->l < err
     (b) a->m = 111...111 and the low 10 bits of a->h are 1 and
         a->l > 2^64 - err */
  if (__builtin_expect (mm == 0 || ~mm == 0, 0))
    if ((mm == 0 && (low == 0 || low == 0x400) && ll < err) ||
        (~mm == 0 && (low == 0x3ff || low == 0x7ff) && ~ll < err))
    {
      /* Upstream terminates on an unproved rounding case. Trap without
         pulling stdio and its global state into every math consumer. */
      __builtin_trap ();
    }
  if (ex <= -1022) // subnormal case
  {
    int sh = -1021 - ex; // 1 <= sh <= 52
    ll = (mm << (64 - sh)) | (ll >> sh) | (ll > 0);
    mm = (hh << (64 - sh)) | (mm >> sh);
    hh = hh >> sh;
    low = hh & 0x7ff;
    ex += sh;
  }
  double h = hh >> 11, l; // significant bits from a->h
  /* If err=0, we are converting a double value, thus low=0, and the
     conversion is exact. */
  if (err == 0)
    l = 0;
  else if (low < 0x400)
    l = 0.25; // round to zero
  else if (low > 0x400)
    l = 0.75; // round away
  else // low = 0x400
  {
    if (mm == 0 && ll == 0)
      l = 0.5; // middle case
    else
      l = 0.75; // round away
  }
  static const double S[2] = {1.0, -1.0};
  double s = S[a->sgn];
  h = fma (l, s, s * h);
  h *= 0x1p-52;
  // now -1021 <= ex <= 1024, thus 2^(ex-1) does not underflow/overflow
  return h * as_ldexp (1.0, ex - 1);
}

/* Put in r an approximation of 1/A, assuming A is not zero.
   Assuming 1 <= r <= 2, the absolute error is bounded by 2^-103.9.
   In terms of ulp, it is bounded by 2^87.1 * ulp(r). */
static inline void inv_tint (tint_t *r, const tint_t *A)
{
  tint_t q[1];
  double a = tint_tod (A, 0, 0, 0); // exact
  // To simplify the error analysis, we assume 0.5 <= a < 1
  int subnormal = fabs (a) < 0x1p-1022;
  if (subnormal)
    a *= 0x1p53;
  tint_fromd (r, 1.0 / a); // accurate to about 53 bits
  if (subnormal)
    r->ex += 53;
  /* We have 1 <= r <= 2, with |r - 1/a| < ulp(r) = 2^-52. */
  /* We use Newton's iteration: r1 = r0 + r0*(1-a*r0).
     Let e0 = 1-a*r0 and e1 = 1-a*r1 then we have e1 = e0^2.
     Since a < 1 and |r - 1/a| < 2^-52 we have e0 < 2^-52 thus e1 < 2^-104.
  */
  mul_tint (q, A, r);      // approximates a*r
  /* The rounding error in mul_tint is bounded by 10 ulps (on 192 bits),
     and a*r < 2, thus we have |q - ar| < 10*2^-191 < 2^-187.
     This error is multiplied by r below, thus contributes to < 2^-186. */
  q->sgn = 1 - q->sgn;     // -a*r
  add_tint (q, &ONE, q);   // approximates 1-a*r
  /* The rounding error in add_tint is bounded by 2 ulps (on 192 bits),
     and we have |round(1-a*r)| < 2^-51 thus this is bounded by 2*2^-243
     = 2^-242. This error is multiplied by r below, thus contributes to
     < 2^-241. */
  mul_tint (q, r, q);      // approximates r*(1-a*r)
  /* Since |q_in| < 2^-51 and r <= 2, we have |q| < 2^-50, and the rounding
     error on mul_tint is bounded by 10 ulps (on 192 bits), it is bounded
     by 10*2^-242 < 2^-238. */
  add_tint (r, r, q);
  /* The rounding error in add_tint is bounded by 2 ulps (on 192 bits),
     and we have |r| <= 2, thus it is bounded by 2*2^-191 = 2^-190. */

  /* The total error on r is bounded by:
   * 2^-104 for the mathematical error
   * 2^-186 + 2^-241 + 2^-238 + 2^-190 < 2^-185 for the rounding errors,
   thus by 2^-103.9.
   Since 1 <= r <= 2, this is bounded by 2^87.1 * ulp(r). */
}

/* Put in r an approximation of b/a, assuming a is not zero,
   with relative error bounded by 2^-185.53. */
static inline void div_tint (tint_t *r, tint_t *b, tint_t *a)
{
  tint_t Y[1], Z[1];
  // to simplify the error analysis, we assume 0.5 <= a, b < 1
  inv_tint (Y, a); // |Y - 1/a| < 2^-103.9, with Y <= 2
  mul_tint (r, Y, b); // r approximates b/a
  /* The rounding error of mul_tint is at most 10 ulps, thus since r <= 2
     this corresponds to < 2^187: |r - Y*b| < 2^187. Now |Y - 1/a|,
     thus |r - b/a| <= |r - Y*b| + b*|Y-1/a| < 2^187 + 2^-103.9 < 2^-103.89. */
  /* We use Karp-Markstein's trick: r1 = r0 + y*(b-a*r0).
     Let e0 = b-a*r0 and e1 = b-a*r1, we have e1 = e0*(1-a*y).
     Since |Y - 1/a| < 2^-103.9 and a < 1 we have |1-a*y| < 2^-103.9.
     Since |r - b/a| < 2^-103.89 and a < 1 we have |e0| < 2^-103.89.
     This gives |e1| < 2^-103.89 * 2^-103.9 < 2^-207. */
  mul_tint (Z, a, r);  // approximates a*r
  /* We have |z| <= 2 and the rounding error is bounded by 10 ulps,
     thus < 2^-187.67. This error is multiplied by Y <= 2 below,
     thus contributes to < 2^-186.67. */
  Z->sgn = 1 - Z->sgn; // -a*r
  add_tint (Z, b, Z);  // approximates b-a*r
  /* Since |b-a*r| < 2^-103.89, and |Zin-ar| < 2^-186.67, we have
     |b-Zin| < 2^-103.89 + 2^-186.67 < 2^-103.88, thus since the
     rounding error is at most 2 ulps, it is < 2*2^-295 = 2^-294.
     This error is multiplied by Y <= 2 below, thus contributes to < 2^-293. */
  mul_tint (Z, Y, Z);  // approximates y*(b-a*r)
  /* Since Y <= 2 and |Zin| < 2^-103.88, we have |Z| < 2^-102.88.
     The rounding error of mul_tint is thus bounded by 10*ulp(2^-102.88)
     = 10 * 2^-294 < 2^-290.67. */
  add_tint (r, r, Z);
  /* Since r <= 2, and the rounding error is bounded by 2 ulps,
     it is bounded by 2 * ulp(1.5) = 2 * 2^-191 = 2^-190. */

  /* The total error is bounded by:
   * 2^-207 for the mathematical error e1
   * 2^-186.67 + 2^-293 + 2^-290.67 + 2^-190
   This gives a bound of 2^-186.53 for 1/2 <= r <= 2,
   thus a relative error < 2^185.53. */
}

// same as div_tint, taking doubles, same relative error bound of 2^185.53
static inline void div_tint_d (tint_t *r, double b, double a)
{
  tint_t A[1], B[1];

  tint_fromd (A, a);
  tint_fromd (B, b);
  div_tint (r, B, A);
}
