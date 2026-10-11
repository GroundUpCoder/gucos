
#pragma once
#include <stdio.h>
int __freading(FILE *stream);
int __fwriting(FILE *stream);
size_t __freadahead(FILE *stream);
size_t __fpending(FILE *stream);
void __fpurge(FILE *stream);
