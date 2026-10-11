// Crash-safe byte store for the OPFS disk (docs/STORAGE.md).
//
// The image stays in memory for reads; writes mark 4 KiB pages dirty. A
// commit appends one checksummed redo record (image length plus every dirty
// page) to the journal file and flushes it: from then on the change survives
// a crash. Pages reach the image file lazily at a checkpoint, which flushes
// the image and then empties the journal. Opening a disk replays every
// complete record in order before reading the image, so an interruption
// anywhere leaves the filesystem at the state of some committed record.
// Adapted from doom-percent's journal-store.js.
self.JournalStore = class JournalStore {
  static PAGE = 4096;
  static MAGIC = 0x4a465044;
  static VERSION = 2;
  static HEADER = 24;
  static CHECKPOINT_BYTES = 16 << 20;   // journal size that forces a checkpoint
  static crcTable = Uint32Array.from({ length: 256 }, (_, i) => { let c = i; for (let j = 0; j < 8; j++) c = (c >>> 1) ^ (c & 1 ? 0xedb88320 : 0); return c >>> 0; });
  static crc(bytes) { let c = 0xffffffff; for (let i = 0; i < bytes.length; i++) c = (c >>> 8) ^ JournalStore.crcTable[(c ^ bytes[i]) & 255]; return (c ^ 0xffffffff) >>> 0; }
  // Checksum of a record with its own checksum field zeroed.
  static checksum(bytes) {
    const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength), old = v.getUint32(20, true);
    v.setUint32(20, 0, true);
    const crc = JournalStore.crc(bytes);
    v.setUint32(20, old, true);
    return crc;
  }
  // image and journal are FileSystemSyncAccessHandle-like objects:
  // getSize, read(out, {at}), write(bytes, {at}), truncate, flush.
  constructor(image, journal) {
    this.image = image; this.journal = journal;
    this.failed = false;
    this.dirty = new Set();          // pages changed since the last commit
    this.pending = new Set();        // committed pages not yet in the image file
    this.journalEnd = 0;
    this.commits = 0;
    this.recover();
    this.imageLength = image.getSize();
    this.length = this.imageLength;
    this.committedLengthValue = this.imageLength;
    this.buffer = JournalStore.allocate(Math.max(this.length, JournalStore.PAGE));
    this.u8 = new Uint8Array(this.buffer);
    // Sync access handles refuse views over a resizable buffer, so file
    // reads and writes go through plain scratch buffers.
    this.page = new Uint8Array(JournalStore.PAGE);
    for (let at = 0; at < this.length; at += JournalStore.GROW) {
      const chunk = new Uint8Array(Math.min(JournalStore.GROW, this.length - at));
      if (image.read(chunk, { at }) !== chunk.length) throw new Error('Short disk read');
      this.u8.set(chunk, at);
    }
    this.dv = new DataView(this.u8.buffer);
  }
  writeAll(file, data, at) {
    const n = file.write(data, { at });
    if (n !== data.length) throw new Error(`Short disk write: ${n} of ${data.length} bytes at ${at} (${file === this.journal ? 'journal' : 'image'}, image ${this.length} bytes)`);
  }
  // Parse the records of a journal image; stops at the first incomplete or
  // corrupt record, which was never committed.
  static records(bytes) {
    const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength), PAGE = JournalStore.PAGE, ENTRY = PAGE + 8, out = [];
    let p = 0;
    while (p + JournalStore.HEADER <= bytes.length) {
      if (v.getUint32(p, true) !== JournalStore.MAGIC || v.getUint32(p + 4, true) !== JournalStore.VERSION) break;
      const count = v.getUint32(p + 16, true), length = v.getFloat64(p + 8, true), size = JournalStore.HEADER + count * ENTRY;
      if (p + size > bytes.length || !Number.isSafeInteger(length) || length < 0) break;
      const record = bytes.subarray(p, p + size);
      if (v.getUint32(p + 20, true) !== JournalStore.checksum(record)) break;
      const pages = [];
      let ok = true;
      for (let i = 0; i < count; i++) {
        const q = JournalStore.HEADER + i * ENTRY, at = new DataView(record.buffer, record.byteOffset + q, 8).getFloat64(0, true);
        if (!Number.isSafeInteger(at) || at < 0 || at % PAGE || at >= length) { ok = false; break; }
        pages.push({ at, data: record.subarray(q + 8, q + 8 + Math.min(PAGE, length - at)) });
      }
      if (!ok) break;
      out.push({ length, pages });
      p += size;
    }
    return out;
  }
  recover() {
    const size = this.journal.getSize();
    if (!size) return;
    const bytes = new Uint8Array(size), n = this.journal.read(bytes, { at: 0 });
    const records = n === size ? JournalStore.records(bytes) : [];
    for (const r of records) {
      this.image.truncate(r.length);
      for (const p of r.pages) this.writeAll(this.image, p.data, p.at);
    }
    if (records.length) this.image.flush();
    this.journal.truncate(0);
    this.journal.flush();
  }
  size() { return this.length; }
  // The image buffer. A resizable ArrayBuffer grows in place, so a large
  // image never needs a second copy of itself while it grows: the old
  // doubling copy failed around 1.5 GiB, where Chromium refuses a 3 GiB
  // allocation ("Array buffer allocation failed"). Untouched capacity is
  // only reserved address space, not memory. Without resizable buffers
  // (older engines) a plain buffer is used and grows by copying.
  static MAX_BYTES = 16 * 1024 * 1024 * 1024;
  static GROW = 64 << 20;
  static allocate(size) {
    try { return new ArrayBuffer(size, { maxByteLength: JournalStore.MAX_BYTES }); }
    catch (e) { return new ArrayBuffer(size); }
  }
  resize(n) {
    if (!Number.isSafeInteger(n) || n < 0) throw new Error('Invalid disk size');
    if (n <= this.length) return;
    if (n > this.buffer.byteLength) {
      const capacity = Math.ceil(n / JournalStore.GROW) * JournalStore.GROW;
      if (this.buffer.resizable && capacity <= this.buffer.maxByteLength) {
        this.buffer.resize(capacity);
      } else {
        const grown = new ArrayBuffer(Math.max(n, capacity));
        new Uint8Array(grown).set(this.u8);
        this.buffer = grown; this.u8 = new Uint8Array(grown); this.dv = new DataView(grown);
      }
    }
    this.length = n;
  }
  watchWrites(listener) { (this.writeListeners ||= new Set()).add(listener); }
  touch(at, len) {
    if (len > 0 && this.writeListeners) for (const listener of this.writeListeners) listener(at, len);
    const PAGE = JournalStore.PAGE;
    for (let p = Math.floor(at / PAGE), end = Math.floor((at + len - 1) / PAGE); p <= end; p++) this.dirty.add(p);
  }
  getUint32(at) { return this.dv.getUint32(at, true); }
  setUint32(at, value) {
    if (this.failed) throw new Error('Disk requires recovery');
    this.dv.setUint32(at, value, true); this.touch(at, 4);
  }
  getBytes(at, len) { return this.u8.slice(at, at + len); }
  setBytes(at, data) {
    if (this.failed) throw new Error('Disk requires recovery');
    if (!data.length) return;
    this.u8.set(data, at); this.touch(at, data.length);
  }
  // Append one record for every dirty page and flush the journal. Nothing
  // dirty and no size change is a no-op.
  commit() {
    if (this.failed) throw new Error('Disk requires recovery');
    if (!this.dirty.size && this.length === this.committedLength()) return false;
    const PAGE = JournalStore.PAGE, ENTRY = PAGE + 8;
    try {
      const pages = [...this.dirty].filter((i) => i * PAGE < this.length).sort((a, b) => a - b);
      const record = new Uint8Array(JournalStore.HEADER + pages.length * ENTRY), v = new DataView(record.buffer);
      v.setUint32(0, JournalStore.MAGIC, true); v.setUint32(4, JournalStore.VERSION, true);
      v.setFloat64(8, this.length, true); v.setUint32(16, pages.length, true);
      pages.forEach((index, i) => {
        const p = JournalStore.HEADER + i * ENTRY;
        v.setFloat64(p, index * PAGE, true);
        record.set(this.u8.subarray(index * PAGE, index * PAGE + PAGE), p + 8);
      });
      v.setUint32(20, JournalStore.checksum(record), true);
      this.writeAll(this.journal, record, this.journalEnd);
      this.journal.flush();
      this.journalEnd += record.length;
      for (const index of pages) this.pending.add(index);
      this.dirty.clear();
      this.committedLengthValue = this.length;
      this.commits++;
      if (this.journalEnd >= JournalStore.CHECKPOINT_BYTES) this.checkpoint();
      return true;
    } catch (e) {
      this.failed = true;
      throw e;
    }
  }
  committedLength() { return this.committedLengthValue; }
  // Write committed pages to the image, flush it, then empty the journal.
  // Uncommitted pages are committed first so the image never runs ahead of
  // the journal.
  checkpoint() {
    this.commit();
    if (!this.pending.size && this.committedLength() === this.imageLength && !this.journalEnd) return false;
    const PAGE = JournalStore.PAGE, length = this.committedLength();
    try {
      this.image.truncate(length);
      for (const index of [...this.pending].sort((a, b) => a - b)) {
        if (index * PAGE >= length) continue;
        const size = Math.min(PAGE, length - index * PAGE);
        this.page.set(this.u8.subarray(index * PAGE, index * PAGE + size));
        this.writeAll(this.image, this.page.subarray(0, size), index * PAGE);
      }
      this.image.flush();
      this.journal.truncate(0);
      this.journal.flush();
      this.journalEnd = 0;
      this.pending.clear();
      this.imageLength = length;
      return true;
    } catch (e) {
      this.failed = true;
      throw e;
    }
  }
  // BlockFS calls flush() for fsync: the change must be durable now.
  flush() { return this.commit(); }
};
