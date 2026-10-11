// Shared and private memory mappings owned by the kernel (docs/MAPPINGS.md).
//
// Physical pages live in one SharedArrayBuffer arena every process worker
// can see. Each process has a page table, also shared, whose entries carry
// the physical page index, the protection bits and an EOF-valid bit; the
// compiled program translates addresses at or above BASE through it
// without any RPC. File pages are shared by inode and file offset, so two
// processes (or a process and a thread) mapping the same range see the
// same bytes. Adapted from doom-percent's mapped-files.js.
self.MappedFiles = class MappedFiles {
  static BASE = 0xF0000000;          // first mapped address
  static PAGE = 4096;
  static PAGES = 65536;              // 256 MiB of mapping address space per process
  static ARENA_PAGES = 16384;        // 64 MiB shared arena
  static PROT_READ = 1;
  static PROT_WRITE = 2;
  static VALID = 4;
  constructor(fs) {
    this.fs = fs;
    this.arena = new SharedArrayBuffer(MappedFiles.ARENA_PAGES * MappedFiles.PAGE);
    this.bytes = new Uint8Array(this.arena);
    this.free = Array.from({ length: MappedFiles.ARENA_PAGES }, (_, i) => MappedFiles.ARENA_PAGES - 1 - i);
    this.owners = new Map();         // pid -> {table: Int32Array over a SAB, pages: Map(page -> item)}
    this.files = new Map();          // ino -> {ino, fd, pages: Map(offset -> physical), refs, writable}
  }
  error(errno, text) { throw Object.assign(new Error(text), { errno }); }
  owner(pid) {
    let o = this.owners.get(pid);
    if (!o) { o = { table: new Int32Array(new SharedArrayBuffer(MappedFiles.PAGES * 4)), pages: new Map() }; this.owners.set(pid, o); }
    return o;
  }
  // What a process worker needs to translate addresses.
  state(pid) { const o = this.owner(pid); return { arena: this.arena, table: o.table.buffer }; }
  has(pid) { return this.owners.has(pid); }
  // ---- kernel-side file access through a private descriptor ----
  fileSize(file) { const st = this.fs.fstat(file.fd); return st ? st.size : 0; }
  readPage(file, at) {
    const out = new Uint8Array(MappedFiles.PAGE);
    const save = this.fs.lseek(file.fd, 0, 1);
    if (this.fs.lseek(file.fd, at, 0) === null) { this.fs.lseek(file.fd, save, 0); return out; }
    const n = this.fs.read(file.fd, out, MappedFiles.PAGE);
    this.fs.lseek(file.fd, save, 0);
    if (n !== null && n < MappedFiles.PAGE) out.fill(0, n);
    return out;
  }
  writeBytes(file, at, data) {
    const save = this.fs.lseek(file.fd, 0, 1);
    if (this.fs.lseek(file.fd, at, 0) === null) { this.fs.lseek(file.fd, save, 0); return; }
    const n = this.fs.write(file.fd, data, data.length);
    this.fs.lseek(file.fd, save, 0);
    if (n === null || n !== data.length) this.error(this.fs._lastError || 'EIO', 'Cannot flush mapped file');
  }
  physical(data) {
    if (!this.free.length) this.error('ENOMEM', 'Shared mapping arena exhausted');
    const index = this.free.pop();
    this.bytes.fill(0, index * MappedFiles.PAGE, (index + 1) * MappedFiles.PAGE);
    if (data) this.bytes.set(data, index * MappedFiles.PAGE);
    return { index, refs: 0 };
  }
  publish(owner, page, item) {
    const valid = !item.file || item.offset < this.fileSize(item.file);
    Atomics.store(owner.table, page, (item.physical.index << 12) | item.prot | (valid ? MappedFiles.VALID : 0));
  }
  // desc: {bfsFd, accmode, ino} for a file mapping, null for anonymous.
  map(pid, a, desc) {
    const length = a.length >>> 0, offset = a.offset;
    if (!length || offset < 0 || offset % MappedFiles.PAGE || !Number.isSafeInteger(offset)) this.error('EINVAL', 'Invalid mapping length or offset');
    if (a.prot & ~3) this.error('ENOTSUP', 'Executable mappings are unavailable');
    if ((a.flags & ~0x23) || ![1, 2].includes(a.flags & 3)) this.error('ENOTSUP', 'Unsupported mapping flags');
    const anonymous = !!(a.flags & 0x20), shared = (a.flags & 3) === 1;
    if (!anonymous && !desc) this.error('EBADF', 'Invalid mapping descriptor');
    const readable = desc && desc.accmode !== 1, writable = desc && desc.accmode !== 0;
    if (!anonymous && (!readable || (shared && (a.prot & MappedFiles.PROT_WRITE) && !writable))) this.error('EACCES', 'Mapping access denied');
    const owner = this.owner(pid), count = Math.ceil(length / MappedFiles.PAGE);
    if (count > MappedFiles.PAGES) this.error('ENOMEM', 'Mapping too large');
    let start = -1, run = 0;
    for (let p = 1; p < MappedFiles.PAGES; p++) {   // page 0 stays unmapped so BASE itself is never a valid address
      run = owner.pages.has(p) ? 0 : run + 1;
      if (run === count) { start = p - count + 1; break; }
    }
    if (start < 0) this.error('ENOMEM', 'Mapping address space exhausted');
    let file = null;
    if (!anonymous) {
      file = this.files.get(desc.ino);
      if (!file) {
        const fd = this.fs.dup(desc.bfsFd);
        if (fd === null || fd < 0) this.error('EIO', 'Cannot pin the mapped file');
        file = { ino: desc.ino, fd, writable, pages: new Map(), refs: 0 };
        this.files.set(desc.ino, file);
      } else if (writable && !file.writable) {
        const fd = this.fs.dup(desc.bfsFd);
        if (fd !== null && fd >= 0) { this.fs.close(file.fd); file.fd = fd; file.writable = true; }
      }
      this.flush(file);
    }
    try {
      for (let i = 0; i < count; i++) {
        const at = offset + i * MappedFiles.PAGE;
        let physical = shared && file ? file.pages.get(at) : null;
        if (!physical) {
          physical = this.physical(file ? this.readPage(file, at) : null);
          if (shared && file) file.pages.set(at, physical);
        }
        physical.refs++;
        if (file) file.refs++;
        const item = { physical, file, offset: at, shared, prot: a.prot, writable: anonymous || !shared || writable };
        owner.pages.set(start + i, item);
        this.publish(owner, start + i, item);
      }
    } catch (e) {
      this.unmap(pid, MappedFiles.BASE + start * MappedFiles.PAGE, count * MappedFiles.PAGE);
      if (file && file.refs === 0 && this.files.get(file.ino) === file) {
        this.fs.close(file.fd); this.files.delete(file.ino);
      }
      throw e;
    }
    return (MappedFiles.BASE + start * MappedFiles.PAGE) >>> 0;
  }
  range(address, length) {
    address >>>= 0;
    if (address < MappedFiles.BASE || address % MappedFiles.PAGE || length <= 0 || address - MappedFiles.BASE + length > MappedFiles.PAGES * MappedFiles.PAGE) this.error('EINVAL', 'Invalid mapping range');
    return [(address - MappedFiles.BASE) / MappedFiles.PAGE, Math.ceil(length / MappedFiles.PAGE)];
  }
  // Write every shared page of a file back where it differs from the file.
  flush(file) {
    if (!file || !file.pages.size) return;
    const size = this.fileSize(file);
    for (const [at, p] of file.pages) {
      const length = Math.min(MappedFiles.PAGE, size - at);
      if (length <= 0) continue;
      const data = this.bytes.slice(p.index * MappedFiles.PAGE, p.index * MappedFiles.PAGE + length);
      const old = this.readPage(file, at).subarray(0, length);
      let same = true;
      for (let i = 0; i < length; i++) if (data[i] !== old[i]) { same = false; break; }
      if (!same) this.writeBytes(file, at, data);
    }
  }
  unmap(pid, address, length) {
    const [start, count] = this.range(address, length), owner = this.owner(pid);
    const files = new Set();
    for (let i = start; i < start + count; i++) { const item = owner.pages.get(i); if (item && item.shared && item.file) files.add(item.file); }
    for (const file of files) this.flush(file);
    for (let i = start; i < start + count; i++) {
      const item = owner.pages.get(i);
      if (!item) continue;
      Atomics.store(owner.table, i, 0);
      owner.pages.delete(i);
      if (--item.physical.refs === 0) {
        this.free.push(item.physical.index);
        if (item.shared && item.file) item.file.pages.delete(item.offset);
      }
      if (item.file && --item.file.refs === 0) { this.fs.close(item.file.fd); this.files.delete(item.file.ino); }
    }
    return 0;
  }
  protect(pid, address, length, prot) {
    if (prot & ~3) this.error('ENOTSUP', 'Executable mappings are unavailable');
    const [start, count] = this.range(address, length), owner = this.owner(pid);
    for (let i = start; i < start + count; i++) {
      const item = owner.pages.get(i);
      if (!item) this.error('ENOMEM', 'Unmapped protection range');
      if ((prot & MappedFiles.PROT_WRITE) && !item.writable) this.error('EACCES', 'Writable mapping denied');
    }
    for (let i = start; i < start + count; i++) { const item = owner.pages.get(i); item.prot = prot; this.publish(owner, i, item); }
    return 0;
  }
  sync(pid, address, length, flags) {
    if ((flags & ~7) || (flags & 5) === 5 || !(flags & 5)) this.error('EINVAL', 'Invalid msync flags');
    const [start, count] = this.range(address, length), owner = this.owner(pid), files = new Set();
    for (let i = start; i < start + count; i++) {
      const item = owner.pages.get(i);
      if (!item) this.error('ENOMEM', 'Unmapped sync range');
      if (item.shared && item.file) files.add(item.file);
    }
    for (const file of files) {
      this.flush(file);
      if ((flags & 4) && this.fs.fsync(file.fd) === null) this.error(this.fs._lastError || 'EIO', 'Cannot sync mapped file');
    }
    return 0;
  }
  // Exit and exec drop every mapping; exec keeps the table for the new image.
  release(pid, keepTable) {
    const owner = this.owners.get(pid);
    if (!owner) return;
    if ([...this.owners].some(([other,value])=>other!==pid && value===owner)) {
      this.owners.delete(pid);
      if(keepTable)this.owner(pid);
      return;
    }
    for (const p of Array.from(owner.pages.keys())) this.unmap(pid, MappedFiles.BASE + p * MappedFiles.PAGE, MappedFiles.PAGE);
    if (!keepTable) this.owners.delete(pid);
  }
  // CLONE_VM: one address space, including private mappings, until exec/exit.
  share(parent, child) { this.owners.set(child,this.owner(parent)); }
  // Fork: shared pages are shared, private pages are copied now.
  clone(parent, child) {
    const source = this.owners.get(parent);
    if (!source) return;
    const copies = Array.from(source.pages.values()).filter((x) => !x.shared).length;
    if (copies > this.free.length) this.error('ENOMEM', 'Not enough memory to fork private mappings');
    const target = this.owner(child);
    for (const [p, old] of source.pages) {
      const item = { ...old };
      if (!old.shared) item.physical = this.physical(this.bytes.subarray(old.physical.index * MappedFiles.PAGE, (old.physical.index + 1) * MappedFiles.PAGE));
      item.physical.refs++;
      if (item.file) item.file.refs++;
      target.pages.set(p, item);
      this.publish(target, p, item);
    }
  }
  // ---- coherence with ordinary file I/O ----
  mapped(ino) { return this.files.get(ino) || null; }
  // Before a read or write of the file: mapped writes must be visible.
  beforeIO(ino) { const file = this.files.get(ino); if (file) this.flush(file); }
  // After a write or truncate of the file: pages must show the new bytes.
  afterWrite(ino) {
    const file = this.files.get(ino);
    if (!file) return;
    const size = this.fileSize(file);
    for (const [at, p] of file.pages) {
      const page = this.readPage(file, at);
      this.bytes.set(page, p.index * MappedFiles.PAGE);
      if (size < at + MappedFiles.PAGE) this.bytes.fill(0, p.index * MappedFiles.PAGE + Math.max(0, size - at), (p.index + 1) * MappedFiles.PAGE);
    }
    for (const owner of this.owners.values()) for (const [page, item] of owner.pages) if (item.file === file) this.publish(owner, page, item);
  }
};
