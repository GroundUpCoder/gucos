
#include <__malloc.h>
#include <__thread.h>
#include <unistd.h>
#include <errno.h>   // POSIX: the malloc family sets ENOMEM on failure

static __lock_t __malloc_lock = __LOCK_INIT;
void __malloc_fork_lock(int acquire) { if (acquire) __lock_acquire(&__malloc_lock); else __lock_release(&__malloc_lock); }
static void *__malloc_unlocked(size_t size);
static void __free_unlocked(void *ptr);
static void *__calloc_unlocked(size_t count, size_t size);
static void *__realloc_unlocked(void *ptr, size_t new_size);
static void *__aligned_alloc_unlocked(size_t alignment, size_t size);
void *malloc(size_t size) { __lock_acquire(&__malloc_lock); void *p = __malloc_unlocked(size); __lock_release(&__malloc_lock); return p; }
void free(void *ptr) { __lock_acquire(&__malloc_lock); __free_unlocked(ptr); __lock_release(&__malloc_lock); }
void *calloc(size_t count, size_t size) { __lock_acquire(&__malloc_lock); void *p = __calloc_unlocked(count, size); __lock_release(&__malloc_lock); return p; }
void *realloc(void *ptr, size_t new_size) { __lock_acquire(&__malloc_lock); void *p = __realloc_unlocked(ptr, new_size); __lock_release(&__malloc_lock); return p; }
void *aligned_alloc(size_t alignment, size_t size) { __lock_acquire(&__malloc_lock); void *p = __aligned_alloc_unlocked(alignment, size); __lock_release(&__malloc_lock); return p; }

// TLSF (Two-Level Segregated Fit) allocator
//
// Block layout (8-byte header):
//   +0: size_and_flags (long) - bits[31:3]=block size/8, bit0=FREE, bit1=PREV_FREE
//   +4: prev_phys (long) - address of previous physical block
// Free blocks additionally store at payload:
//   +8: next_free (long)
//   +12: prev_free (long)

#define FREE_BIT      1
#define PREV_FREE_BIT 2
#define FLAG_BITS     3

#define BLOCK_OVERHEAD  8
#define MIN_BLOCK_SIZE  16
#define BLOCK_ALIGN     8

#define SL_LOG2   4
#define SEARCH_ROUND(size) ((size) + (1 << (31 - __builtin_clz((int)(size)) - SL_LOG2)) - 1)
#define SL_COUNT  (1 << SL_LOG2)
#define FL_SHIFT  4
#define FL_MAX    30
#define FL_COUNT  (FL_MAX - FL_SHIFT + 1)

static long fl_bitmap;
static long sl_bitmap[FL_COUNT];
static long free_heads[FL_COUNT * SL_COUNT];
static long pool_start;
static long pool_end;
static long last_block;
static int  initialized;

static unsigned long block_size(long block) {
  return (unsigned long)(*(long *)block & ~FLAG_BITS);
}

static int block_is_free(long block) {
  return *(long *)block & FREE_BIT;
}

static int block_prev_is_free(long block) {
  return *(long *)block & PREV_FREE_BIT;
}

static long block_prev_phys(long block) {
  return *(long *)(block + 4);
}

static long block_next_phys(long block) {
  return block + block_size(block);
}

static long block_payload(long block) {
  return block + BLOCK_OVERHEAD;
}

static long payload_to_block(long payload) {
  return payload - BLOCK_OVERHEAD;
}

static long block_get_next_free(long block) {
  return *(long *)(block + 8);
}

static void block_set_next_free(long block, long nf) {
  *(long *)(block + 8) = nf;
}

static long block_get_prev_free(long block) {
  return *(long *)(block + 12);
}

static void block_set_prev_free(long block, long pf) {
  *(long *)(block + 12) = pf;
}

// mapping_insert: floor mapping for insertion
static void mapping_insert(long size, int *fl, int *sl) {
  if (size < (1 << (FL_SHIFT + 1))) {
    *fl = 0;
    *sl = (int)((size - MIN_BLOCK_SIZE) >> 3);
  } else {
    int t = 31 - __builtin_clz((int)size);
    *sl = (int)((size >> (t - SL_LOG2)) & (SL_COUNT - 1));
    *fl = t - FL_SHIFT;
  }
}

// mapping_search: ceiling mapping for search (rounds up)
static void mapping_search(long size, int *fl, int *sl) {
  long rounded = SEARCH_ROUND(size);
  mapping_insert(rounded, fl, sl);
}

static void insert_free_block(long block) {
  int fl, sl;
  long sz = block_size(block);
  mapping_insert(sz, &fl, &sl);

  long head = free_heads[fl * SL_COUNT + sl];
  block_set_next_free(block, head);
  block_set_prev_free(block, 0);
  if (head) block_set_prev_free(head, block);
  free_heads[fl * SL_COUNT + sl] = block;

  fl_bitmap = fl_bitmap | (1 << fl);
  sl_bitmap[fl] = sl_bitmap[fl] | (1 << sl);
}

static void remove_free_block(long block) {
  int fl, sl;
  long sz = block_size(block);
  mapping_insert(sz, &fl, &sl);

  long nf = block_get_next_free(block);
  long pf = block_get_prev_free(block);
  if (nf && block_get_prev_free(nf) != block) {
    write(2, "Corrupted heap: free list broken (next->prev != cur)\n", sizeof("Corrupted heap: free list broken (next->prev != cur)\n") - 1);
    __wasm(void, (), op 0x00);
  }
  if (pf && block_get_next_free(pf) != block) {
    write(2, "Corrupted heap: free list broken (prev->next != cur)\n", sizeof("Corrupted heap: free list broken (prev->next != cur)\n") - 1);
    __wasm(void, (), op 0x00);
  }
  if (nf) block_set_prev_free(nf, pf);
  if (pf) block_set_next_free(pf, nf);
  else {
    free_heads[fl * SL_COUNT + sl] = nf;
    if (!nf) {
      sl_bitmap[fl] = sl_bitmap[fl] & ~(1 << sl);
      if (!sl_bitmap[fl])
        fl_bitmap = fl_bitmap & ~(1 << fl);
    }
  }
}

static long find_suitable_block(int *fl, int *sl) {
  // Search current SL bitmap from sl upward
  long sl_map = sl_bitmap[*fl] & (~0L << *sl);
  if (!sl_map) {
    // Search FL bitmap from fl+1 upward
    long fl_map = fl_bitmap & (~0L << (*fl + 1));
    if (!fl_map) return 0;
    *fl = __builtin_ctz((int)fl_map);
    sl_map = sl_bitmap[*fl];
  }
  *sl = __builtin_ctz((int)sl_map);
  return free_heads[*fl * SL_COUNT + *sl];
}

static long merge_prev(long block) {
  if (block_prev_is_free(block)) {
    long prev = block_prev_phys(block);
    remove_free_block(prev);
    long new_size = block_size(prev) + block_size(block);
    *(long *)prev = (*(long *)prev & FLAG_BITS) | new_size;
    // Update prev_phys of next physical block
    long next = block_next_phys(prev);
    if (next < pool_end)
      *(long *)(next + 4) = prev;
    if (block == last_block)
      last_block = prev;
    block = prev;
  }
  return block;
}

static long merge_next(long block) {
  long next = block_next_phys(block);
  if (next < pool_end && block_is_free(next)) {
    remove_free_block(next);
    long new_size = block_size(block) + block_size(next);
    *(long *)block = (*(long *)block & FLAG_BITS) | new_size;
    // Update prev_phys of block after next
    long after = block_next_phys(block);
    if (after < pool_end)
      *(long *)(after + 4) = block;
    if (next == last_block)
      last_block = block;
  }
  return block;
}

static void split_block(long block, long needed) {
  long remainder_size = block_size(block) - needed;
  if (remainder_size >= MIN_BLOCK_SIZE) {
    // Resize current block
    *(long *)block = (*(long *)block & FLAG_BITS) | needed;
    // Create remainder block
    long rem = block + needed;
    *(long *)rem = remainder_size | FREE_BIT;
    *(long *)(rem + 4) = block;
    // Update next block's prev_phys
    long next = rem + remainder_size;
    if (next < pool_end)
      *(long *)(next + 4) = rem;
    if (block == last_block)
      last_block = rem;
    insert_free_block(rem);
    // Set PREV_FREE on successor
    next = block_next_phys(block);
    if (next < pool_end)
      *(long *)next = *(long *)next | PREV_FREE_BIT;
  }
}

static void block_mark_used(long block) {
  *(long *)block = *(long *)block & ~FREE_BIT;
  // Clear PREV_FREE on next physical block
  long next = block_next_phys(block);
  if (next < pool_end)
    *(long *)next = *(long *)next & ~PREV_FREE_BIT;
}

static void block_mark_free(long block) {
  *(long *)block = *(long *)block | FREE_BIT;
  // Set PREV_FREE on next physical block
  long next = block_next_phys(block);
  if (next < pool_end)
    *(long *)next = *(long *)next | PREV_FREE_BIT;
}

static void init_pool(void) {
  pool_start = __builtin(heap_base);
  // Align pool_start to BLOCK_ALIGN
  pool_start = (pool_start + BLOCK_ALIGN - 1) & ~(BLOCK_ALIGN - 1);
  pool_end = pool_start;
  last_block = 0;
  fl_bitmap = 0;
  int i = 0;
  while (i < FL_COUNT) { sl_bitmap[i] = 0; i = i + 1; }
  i = 0;
  while (i < FL_COUNT * SL_COUNT) { free_heads[i] = 0; i = i + 1; }
  initialized = 1;
}

static int grow_pool(long needed) {
  // needed includes BLOCK_OVERHEAD
  long new_end = pool_end + needed;
  // Align to page boundary for wasm memory.grow
  long pages = (new_end + 65535) / 65536;
  if (pages > __builtin(memory_size)) {
    long grow = pages - __builtin(memory_size);
    if (__builtin(memory_grow, grow) == (size_t)-1)
      return 0;
  }
  // Create a new block at pool_end
  long block = pool_end;
  long block_sz = new_end - pool_end;
  // Round up so mapping_search can find this block
  block_sz = SEARCH_ROUND(block_sz);
  // Round up to alignment
  block_sz = (block_sz + BLOCK_ALIGN - 1) & ~(BLOCK_ALIGN - 1);
  new_end = pool_end + block_sz;
  // Re-check pages after rounding
  pages = (new_end + 65535) / 65536;
  if (pages > __builtin(memory_size)) {
    long grow = pages - __builtin(memory_size);
    if (__builtin(memory_grow, grow) == (size_t)-1)
      return 0;
  }

  *(long *)block = block_sz | FREE_BIT;
  *(long *)(block + 4) = last_block;
  pool_end = new_end;

  // If last block is free, merge
  if (last_block && block_is_free(last_block)) {
    // Set prev_free bit so merge_prev works
    *(long *)block = *(long *)block | PREV_FREE_BIT;
    last_block = block;
    block = merge_prev(block);
  } else {
    last_block = block;
  }

  insert_free_block(block);
  return 1;
}

static long adjust_request(long size) {
  // Add overhead and ensure minimum size
  long adjusted = size + BLOCK_OVERHEAD;
  if (adjusted < MIN_BLOCK_SIZE) adjusted = MIN_BLOCK_SIZE;
  // Align up
  adjusted = (adjusted + BLOCK_ALIGN - 1) & ~(BLOCK_ALIGN - 1);
  return adjusted;
}

static void *__malloc_unlocked(size_t size) {
  /* malloc(0) returns a unique, freeable, non-NULL pointer (glibc/musl/BSD/
     Darwin/MSVC all do) — NULL is reserved for genuine failure, which always
     reports ENOMEM. Zero falls through: adjust_request() clamps to
     MIN_BLOCK_SIZE, so the pointer comes from the pool and free() accepts
     it. Reversed from NULL-on-zero by #554 (jku ruling: match the majority
     ecosystem — NetSurf-class callers treat NULL as OOM). */
  if (size > 0x40000000L) { errno = ENOMEM; return (void *)0; }

  if (!initialized) init_pool();

  long adjusted = adjust_request((long)size);

  int fl, sl;
  mapping_search(adjusted, &fl, &sl);
  if (fl >= FL_COUNT) {
    // Too large even for search
    if (!grow_pool(adjusted)) { errno = ENOMEM; return (void *)0; }
    mapping_search(adjusted, &fl, &sl);
  }

  long block = find_suitable_block(&fl, &sl);
  if (!block) {
    if (!grow_pool(adjusted)) { errno = ENOMEM; return (void *)0; }
    mapping_search(adjusted, &fl, &sl);
    block = find_suitable_block(&fl, &sl);
    if (!block) { errno = ENOMEM; return (void *)0; }
  }

  remove_free_block(block);
  split_block(block, adjusted);
  block_mark_used(block);

  return (void *)block_payload(block);
}

static void __free_unlocked(void *ptr) {
  if (!ptr) return;

  long block = payload_to_block((long)ptr);

  // Bounds check
  if (block < pool_start || block >= pool_end) {
    write(2, "free: double free detected\n", sizeof("free: double free detected\n") - 1);
    __wasm(void, (), op 0x00);
  }
  // Double-free detection: block must not already be free
  if (block_is_free(block)) {
    write(2, "free: double free detected\n", sizeof("free: double free detected\n") - 1);
    __wasm(void, (), op 0x00);
  }

  block_mark_free(block);
  block = merge_prev(block);
  block = merge_next(block);
  insert_free_block(block);
}

static void *__calloc_unlocked(size_t count, size_t size) {
  if (size != 0 && count > 0x40000000L / size) { errno = ENOMEM; return (void *)0; }
  size_t total = count * size;
  void *p = malloc(total);
  if (p) __builtin(memory_fill, p, 0, total);
  return p;
}

static void *__realloc_unlocked(void *ptr, size_t new_size) {
  if (!ptr) return malloc(new_size);
  /* realloc(p, 0) keeps the block and returns p (non-NULL, freeable), like
     musl/BSD/Darwin — NOT glibc/MSVC's free-and-return-NULL. C23 made the
     zero-size case UB precisely because free-and-NULL is indistinguishable
     from OOM-with-p-still-live; keeping NULL <=> failure across all three
     allocators makes the p = realloc(p, n) pattern safe at n == 0 (#554). */
  // Reject sizes malloc itself would reject; also guards adjust_request below
  // from overflowing a huge request down to MIN_BLOCK_SIZE.
  if (new_size > 0x40000000L) { errno = ENOMEM; return (void *)0; }

  long block = payload_to_block((long)ptr);
  long old_payload = block_size(block) - BLOCK_OVERHEAD;

  // If new size fits in current block, keep it
  if (new_size <= (size_t)old_payload) return ptr;

  // Grow in place by absorbing the next physical block when it is free and the
  // combined size satisfies the request; avoids the malloc+copy+free round-trip.
  // Mirrors the reference TLSF tlsf_realloc.
  long adjusted = adjust_request((long)new_size);
  long next = block_next_phys(block);
  if (next < pool_end && block_is_free(next) &&
      block_size(block) + block_size(next) >= adjusted) {
    merge_next(block);
    split_block(block, adjusted);
    block_mark_used(block);
    return ptr;
  }

  // Allocate new, copy, free old
  void *new_ptr = malloc(new_size);
  if (!new_ptr) return (void *)0;
  __builtin(memory_copy, new_ptr, ptr, old_payload);
  free(ptr);
  return new_ptr;
}

static void *__aligned_alloc_unlocked(size_t alignment, size_t size) {
  // C11 7.22.3.1: alignment must be a supported alignment, size a multiple of alignment
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) return (void *)0;
  if (size % alignment != 0) return (void *)0;
  // TLSF malloc returns 8-byte aligned memory (BLOCK_ALIGN == 8).
  // Alignments up to 8 are satisfied directly. Larger extended alignments are
  // not supported (the compiler rejects _Alignas > max_align_t == 8).
  if (alignment > 8) return (void *)0;
  return malloc(size);
}

void __inspect_heap(struct __heap_info *info) {
  if (!initialized) init_pool();
  info->heap_start = pool_start;
  info->heap_end = pool_end;
  info->total_bytes = pool_end - pool_start;
  long fb = 0;
  long fby = 0;
  long lf = 0;
  int f = 0;
  while (f < FL_COUNT) {
    int s = 0;
    while (s < SL_COUNT) {
      long b = free_heads[f * SL_COUNT + s];
      while (b) {
        long sz = block_size(b) - BLOCK_OVERHEAD;
        fb = fb + 1;
        fby = fby + sz;
        if (sz > lf) lf = sz;
        b = block_get_next_free(b);
      }
      s = s + 1;
    }
    f = f + 1;
  }
  info->free_blocks = fb;
  info->free_bytes = fby;
  info->largest_free = lf;
}
  