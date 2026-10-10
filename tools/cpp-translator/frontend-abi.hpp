// The destination C compiler places variadic values in eight-byte-rounded slots.
// Make this part of the Clang producer ABI, before target-specific va_arg lowering.
// LLVM va_start/va_copy/va_end still describe the lifetime of the cursor itself.
template<class T> static T __wasm_posix_va_arg(__builtin_va_list &cursor) {
  static_assert(__is_arithmetic(T) || __is_pointer(T) || __is_enum(T),
                "aggregate variadic value requires an ABI adapter");
  T value;
  __builtin_memcpy(&value, cursor, sizeof(T));
  cursor = (char *)cursor + ((sizeof(T) + 7) & ~(__SIZE_TYPE__)7);
  return value;
}
#define __builtin_va_arg(cursor, type) __wasm_posix_va_arg<type>(cursor)
