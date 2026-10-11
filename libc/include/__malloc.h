
#pragma once
#include <stddef.h>
__require_source("__malloc.c");

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t new_size);
void *aligned_alloc(size_t alignment, size_t size);

struct __heap_info {
  long heap_start;
  long heap_end;
  long total_bytes;
  long free_blocks;
  long free_bytes;
  long largest_free;
};
void __inspect_heap(struct __heap_info *info);
  