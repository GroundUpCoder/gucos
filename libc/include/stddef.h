
#pragma once
typedef unsigned long size_t; // Use long for all pointer-sized types
typedef long ptrdiff_t;
typedef int wchar_t;
typedef long double max_align_t;
#define NULL ((void *)0)
#define offsetof(type, member) ((size_t)&((type *)0)->member)
  