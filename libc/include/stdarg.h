
#pragma once
typedef int *__va_elem;
typedef __va_elem va_list[1];
#define va_start(ap, param) __builtin_va_start(ap[0], param)
#define va_arg(ap, type) __builtin_va_arg(ap[0], type)
#define va_end(ap) __builtin_va_end(ap[0])
#define va_copy(dest, src) __builtin_va_copy(dest[0], src[0])
  