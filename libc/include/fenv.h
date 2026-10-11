
#pragma once
#define FE_DIVBYZERO  1
#define FE_INEXACT    2
#define FE_INVALID    4
#define FE_OVERFLOW   8
#define FE_UNDERFLOW  16
#define FE_ALL_EXCEPT (FE_DIVBYZERO|FE_INEXACT|FE_INVALID|FE_OVERFLOW|FE_UNDERFLOW)
#define FE_TONEAREST  0
#define FE_DOWNWARD   1
#define FE_UPWARD     2
#define FE_TOWARDZERO 3
#define FE_DFL_ENV    ((const fenv_t *)0)
typedef unsigned int fexcept_t;
typedef unsigned int fenv_t;
static inline int feclearexcept(int e) { (void)e; return 0; }
static inline int fegetexceptflag(fexcept_t *f, int e) { (void)f; (void)e; return 0; }
static inline int feraiseexcept(int e) { (void)e; return 0; }
static inline int fesetexceptflag(const fexcept_t *f, int e) { (void)f; (void)e; return 0; }
static inline int fetestexcept(int e) { (void)e; return 0; }
static inline int fegetround(void) { return FE_TONEAREST; }
static inline int fesetround(int r) { (void)r; return 0; }
static inline int fegetenv(fenv_t *e) { (void)e; return 0; }
static inline int feholdexcept(fenv_t *e) { (void)e; return 0; }
static inline int fesetenv(const fenv_t *e) { (void)e; return 0; }
static inline int feupdateenv(const fenv_t *e) { (void)e; return 0; }
  