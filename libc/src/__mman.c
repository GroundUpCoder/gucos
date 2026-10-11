
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <__thread.h>
struct __private_mapping { void *address; void *allocation; size_t length; struct __private_mapping *next; };
static struct __private_mapping *__private_mappings;
static __lock_t __mapping_lock;
void *mmap(void *hint, size_t length, int prot, int flags, int fd, long long offset) {
  (void)hint;
  if (!length || offset < 0 || (offset & 4095)) { errno = EINVAL; return MAP_FAILED; }
  if (prot != (PROT_READ|PROT_WRITE) || (flags != MAP_PRIVATE && flags != (MAP_PRIVATE|MAP_ANONYMOUS))) { errno = ENOTSUP; return MAP_FAILED; }
  if (length > 0x7ffff000u) { errno = ENOMEM; return MAP_FAILED; }
  size_t rounded = (length + 4095) & ~4095u;
  if (!(flags & MAP_ANONYMOUS)) {
    struct stat st;
    if (fstat(fd, &st)) return MAP_FAILED;
    if (!S_ISREG(st.st_mode) || offset >= st.st_size || length > (unsigned long long)(st.st_size-offset)) { errno = ENOTSUP; return MAP_FAILED; }
  }
  struct __private_mapping *m = malloc(sizeof(*m));
  if (!m) { errno=ENOMEM; return MAP_FAILED; }
  m->allocation = malloc(rounded+4095);
  m->address = (void *)(((unsigned long)m->allocation+4095)&~4095u);
  if (!m->allocation) { free(m); errno=ENOMEM; return MAP_FAILED; }
  memset(m->address, 0, rounded);
  if (!(flags & MAP_ANONYMOUS)) {
    size_t done = 0;
    while (done < length) {
      ssize_t n = pread(fd, (char *)m->address + done, length-done, offset+done);
      if (n < 0 && errno == EINTR) continue;
      if (n <= 0) { int e = n < 0 ? errno : EIO; free(m->allocation); free(m); errno=e; return MAP_FAILED; }
      done += n;
    }
  }
  m->length=rounded;
  __lock_acquire(&__mapping_lock); m->next=__private_mappings; __private_mappings=m; __lock_release(&__mapping_lock);
  return m->address;
}
int munmap(void *address, size_t length) {
  if (!length || ((unsigned long)address & 4095) || length > 0x7ffff000u) { errno=EINVAL; return -1; }
  __lock_acquire(&__mapping_lock);
  struct __private_mapping **p=&__private_mappings;
  while (*p && (*p)->address != address) p=&(*p)->next;
  if (!*p || (*p)->length != ((length+4095)&~4095u)) { __lock_release(&__mapping_lock); errno=ENOTSUP; return -1; }
  struct __private_mapping *m=*p; *p=m->next;
  __lock_release(&__mapping_lock); free(m->allocation); free(m); return 0;
}
int mprotect(void *address, size_t length, int prot) {
  (void)address; (void)length; (void)prot; errno=ENOTSUP; return -1;
}
int msync(void *address, size_t length, int flags) {
  if (flags != MS_SYNC && flags != MS_ASYNC) { errno=EINVAL; return -1; }
  __lock_acquire(&__mapping_lock);
  struct __private_mapping *m=__private_mappings;
  while (m && ((unsigned long)address < (unsigned long)m->address || (unsigned long)address-(unsigned long)m->address > m->length || length > m->length-((unsigned long)address-(unsigned long)m->address))) m=m->next;
  __lock_release(&__mapping_lock);
  if (!m) { errno=ENOMEM; return -1; }
  return 0; /* Private writes never propagate to the file. */
}
  