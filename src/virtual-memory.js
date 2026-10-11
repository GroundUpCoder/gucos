// Process-side address translation for mapped memory (docs/MAPPINGS.md).
// Addresses at or above BASE index the process page table; everything
// below is ordinary linear memory. The compiled program calls __vm_load,
// __vm_store, __vm_copy and __vm_fill for accesses that may be mapped; the
// host runtime uses read/write/readString for pointers it receives.
self.VirtualMemory = class VirtualMemory {
  static BASE = 0xF0000000;
  static PAGE = 4096;
  static SIZES = [1, 1, 2, 2, 4, 4, 8, 8, 4, 8];
  // `memory` is a WebAssembly.Memory or a function returning one (the
  // instance that owns it may not exist yet when the imports are built).
  constructor(memory, state) {
    this.memory = memory;
    this.table = new Int32Array(state.table);
    this.shared = new Uint8Array(state.arena);
    this.buffer = null; this.u8 = null;
  }
  refresh() {
    const buffer = (typeof this.memory === 'function' ? this.memory() : this.memory).buffer;
    if (buffer !== this.buffer) { this.buffer = buffer; this.u8 = new Uint8Array(buffer); }
    return this.u8;
  }
  fault(text) { throw new WebAssembly.RuntimeError(text); }
  // A view of `length` bytes at address, or null when the range crosses a
  // mapped page boundary (the caller then goes byte-wise).
  span(address, length, write) {
    address >>>= 0;
    if (address < VirtualMemory.BASE) {
      const u8 = this.refresh();
      if (address + length > u8.length) this.fault('Invalid memory access at 0x' + address.toString(16));
      return u8.subarray(address, address + length);
    }
    const page = (address - VirtualMemory.BASE) >>> 12;
    if (page >= this.table.length) this.fault('Invalid mapped address 0x' + address.toString(16));
    const entry = Atomics.load(this.table, page);
    if (!(entry & (write ? 2 : 1)) || !(entry & 4)) this.fault((write ? 'Write to' : 'Read from') + ' an unmapped, protected or truncated address 0x' + address.toString(16));
    const offset = address & 4095;
    if (length > 4096 - offset) return null;
    const at = (entry >>> 12) * 4096 + offset;
    return this.shared.subarray(at, at + length);
  }
  read(address, length) {
    address >>>= 0;
    const span = this.span(address, length, false);
    if (span) return span.slice();
    const out = new Uint8Array(length);
    for (let i = 0; i < length;) {
      const n = Math.min(length - i, 4096 - ((address + i) & 4095));
      out.set(this.span(address + i, n, false), i);
      i += n;
    }
    return out;
  }
  write(address, data) {
    address >>>= 0;
    const spans = [];
    for (let i = 0; i < data.length;) {
      const n = Math.min(data.length - i, 4096 - ((address + i) & 4095));
      spans.push([this.span(address + i, n, true), i, n]);
      i += n;
    }
    for (const [span, i, n] of spans) span.set(data.subarray(i, i + n));
  }
  readStringBytes(address, limit = Infinity) {
    address >>>= 0;
    const chunks = [];
    let length = 0;
    while (length < limit) {
      const at = address + length;
      if (at > 0xffffffff) this.fault('String crosses the address-space boundary');
      const available = at < VirtualMemory.BASE ? this.refresh().length - at : 4096 - (at & 4095);
      if (available <= 0) this.fault('Unterminated string at 0x' + address.toString(16));
      const span = this.span(at, Math.min(available, limit - length), false);
      const zero = span.indexOf(0), part = zero < 0 ? span : span.subarray(0, zero);
      chunks.push(part); length += part.length;
      if (zero >= 0) break;
    }
    if (chunks.length === 1) return chunks[0].slice();
    const result = new Uint8Array(length);
    let offset = 0;
    for (const chunk of chunks) { result.set(chunk, offset); offset += chunk.length; }
    return result;
  }
  readString(address) {
    return new TextDecoder().decode(this.readStringBytes(address));
  }
  // Scalar access as C-ABI bits in an i64; types 0-7 are signed and unsigned
  // 8, 16, 32 and 64 bit integers, 8 is float and 9 double. Aligned mapped
  // accesses use Atomics so another worker never observes a torn value.
  load(address, type) {
    const size = VirtualMemory.SIZES[type];
    const span = this.span(address, size, false), bytes = span || this.read(address, size);
    const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    if (span && bytes.buffer instanceof SharedArrayBuffer && bytes.byteOffset % size === 0 && type !== 8 && type !== 9) {
      const arrays = [Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array, Uint32Array, BigInt64Array, BigUint64Array];
      const v = Atomics.load(new arrays[type](bytes.buffer, bytes.byteOffset, 1), 0);
      return typeof v === 'bigint' ? BigInt.asIntN(64, v) : BigInt(v);
    }
    switch (type) {
      case 0: return BigInt(dv.getInt8(0));
      case 1: return BigInt(dv.getUint8(0));
      case 2: return BigInt(dv.getInt16(0, true));
      case 3: return BigInt(dv.getUint16(0, true));
      case 4: return BigInt(dv.getInt32(0, true));
      case 5: return BigInt(dv.getUint32(0, true));
      case 6: case 7: return dv.getBigInt64(0, true);
      case 8: return BigInt(dv.getInt32(0, true));      // float bits, sign-extended like i32
      case 9: return dv.getBigInt64(0, true);           // double bits
    }
    this.fault('Invalid mapped access type');
  }
  store(address, value, type) {
    const size = VirtualMemory.SIZES[type];
    const bytes = new Uint8Array(size), dv = new DataView(bytes.buffer);
    switch (size) {
      case 1: dv.setUint8(0, Number(BigInt.asUintN(8, value))); break;
      case 2: dv.setUint16(0, Number(BigInt.asUintN(16, value)), true); break;
      case 4: dv.setUint32(0, Number(BigInt.asUintN(32, value)), true); break;
      default: dv.setBigUint64(0, BigInt.asUintN(64, value), true); break;
    }
    const span = this.span(address, size, true);
    if (span && span.buffer instanceof SharedArrayBuffer && span.byteOffset % size === 0) {
      const arrays = { 1: Uint8Array, 2: Uint16Array, 4: Uint32Array, 8: BigUint64Array };
      const getters = { 1: 'getUint8', 2: 'getUint16', 4: 'getUint32', 8: 'getBigUint64' };
      Atomics.store(new arrays[size](span.buffer, span.byteOffset, 1), 0, dv[getters[size]](0, true));
    } else this.write(address, bytes);
  }
  copy(dst, src, length) {
    dst >>>= 0; src >>>= 0; length >>>= 0;
    if (!length) return;
    if (dst < VirtualMemory.BASE && src < VirtualMemory.BASE) { const u8 = this.refresh(); u8.copyWithin(dst, src, src + length); return; }
    this.write(dst, this.read(src, length));
  }
  fill(dst, byte, length) {
    dst >>>= 0; length >>>= 0;
    if (!length) return;
    if (dst < VirtualMemory.BASE) { this.refresh().fill(byte & 255, dst, dst + length); return; }
    this.write(dst, new Uint8Array(length).fill(byte & 255));
  }
  // The four intrinsics the compiler emits for possibly-mapped accesses.
  imports() {
    return {
      __vm_load: (address, type) => this.load(address, type),
      __vm_store: (address, value, type) => { this.store(address, value, type); },
      __vm_copy: (dst, src, length) => { this.copy(dst, src, length); },
      __vm_fill: (dst, byte, length) => { this.fill(dst, byte, length); },
    };
  }
};
