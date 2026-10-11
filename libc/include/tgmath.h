
#pragma once
#include <math.h>

/* Type-generic macros for <math.h> functions (C11 7.25) */
/* Each macro dispatches to the float variant for float arguments, */
/* and the double variant otherwise.                               */

/* Unary float/double */
#define fabs(x)      _Generic((x), float: fabsf,      default: fabs)(x)
#define ceil(x)      _Generic((x), float: ceilf,      default: ceil)(x)
#define floor(x)     _Generic((x), float: floorf,     default: floor)(x)
#define trunc(x)     _Generic((x), float: truncf,     default: trunc)(x)
#define nearbyint(x) _Generic((x), float: nearbyintf, default: nearbyint)(x)
#define rint(x)      _Generic((x), float: rintf,      default: rint)(x)
#define sqrt(x)      _Generic((x), float: sqrtf,      default: sqrt)(x)
#define sin(x)       _Generic((x), float: sinf,       default: sin)(x)
#define cos(x)       _Generic((x), float: cosf,       default: cos)(x)
#define tan(x)       _Generic((x), float: tanf,       default: tan)(x)
#define asin(x)      _Generic((x), float: asinf,      default: asin)(x)
#define acos(x)      _Generic((x), float: acosf,      default: acos)(x)
#define atan(x)      _Generic((x), float: atanf,      default: atan)(x)
#define sinh(x)      _Generic((x), float: sinhf,      default: sinh)(x)
#define cosh(x)      _Generic((x), float: coshf,      default: cosh)(x)
#define tanh(x)      _Generic((x), float: tanhf,      default: tanh)(x)
#define asinh(x)     _Generic((x), float: asinhf,     default: asinh)(x)
#define acosh(x)     _Generic((x), float: acoshf,     default: acosh)(x)
#define atanh(x)     _Generic((x), float: atanhf,     default: atanh)(x)
#define exp(x)       _Generic((x), float: expf,       default: exp)(x)
#define expm1(x)     _Generic((x), float: expm1f,     default: expm1)(x)
#define log(x)       _Generic((x), float: logf,       default: log)(x)
#define log2(x)      _Generic((x), float: log2f,      default: log2)(x)
#define log10(x)     _Generic((x), float: log10f,     default: log10)(x)
#define log1p(x)     _Generic((x), float: log1pf,     default: log1p)(x)
#define cbrt(x)      _Generic((x), float: cbrtf,      default: cbrt)(x)
#define round(x)     _Generic((x), float: roundf,     default: round)(x)

/* Binary float/double — dispatch on (x)+(y) so mixed float/double promotes */
#define fdim(x, y)      _Generic((x)+(y), float: fdimf,      default: fdim)(x, y)
#define fmin(x, y)      _Generic((x)+(y), float: fminf,      default: fmin)(x, y)
#define fmax(x, y)      _Generic((x)+(y), float: fmaxf,      default: fmax)(x, y)
#define copysign(x, y)  _Generic((x)+(y), float: copysignf,  default: copysign)(x, y)
#define fmod(x, y)      _Generic((x)+(y), float: fmodf,      default: fmod)(x, y)
#define pow(x, y)       _Generic((x)+(y), float: powf,       default: pow)(x, y)
#define atan2(y, x)     _Generic((y)+(x), float: atan2f,     default: atan2)(y, x)
#define hypot(x, y)     _Generic((x)+(y), float: hypotf,     default: hypot)(x, y)
#define nextafter(x, y) _Generic((x)+(y), float: nextafterf, default: nextafter)(x, y)

/* ldexp: second arg is always int, dispatch on first arg only */
#define ldexp(x, n)     _Generic((x), float: ldexpf,    default: ldexp)(x, n)
  