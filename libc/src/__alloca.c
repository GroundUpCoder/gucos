
void *alloca(long size) {
  return __builtin(alloca, size);
}
  