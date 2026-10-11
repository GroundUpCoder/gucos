// POSIX process-associated record locks (fcntl F_SETLK/F_SETLKW/F_GETLK).
// Keys are inode numbers, owners are process ids, ranges are half open with
// Infinity for "to end of file". Adapted from doom-percent's file-locks.js.
self.FileLocks = class FileLocks {
  constructor() {
    this.files = new Map();   // key -> [{owner, type, start, end}], type 0 read, 1 write
    this.waiters = [];        // {key, lock, cid, resume}
  }
  overlaps(x, lock) { return x.owner !== lock.owner && x.start < lock.end && lock.start < x.end && (x.type === 1 || lock.type === 1); }
  conflict(key, lock) { return (this.files.get(key) || []).find((x) => this.overlaps(x, lock)) || null; }
  // Apply a lock (type 2 unlocks). Returns false when another owner holds a
  // conflicting lock; the caller decides whether to wait.
  set(key, lock) {
    if (lock.type !== 2 && this.conflict(key, lock)) return false;
    const result = [];
    for (const old of this.files.get(key) || []) {
      if (old.owner !== lock.owner || old.end <= lock.start || old.start >= lock.end) result.push(old);
      else {
        if (old.start < lock.start) result.push({ ...old, end: lock.start });
        if (old.end > lock.end) result.push({ ...old, start: lock.end });
      }
    }
    if (lock.type !== 2) result.push({ owner: lock.owner, type: lock.type, start: lock.start, end: lock.end });
    result.sort((a, b) => a.owner - b.owner || a.start - b.start);
    const merged = [];
    for (const x of result) {
      const last = merged[merged.length - 1];
      if (last && last.owner === x.owner && last.type === x.type && last.end === x.start) last.end = x.end;
      else merged.push(x);
    }
    if (merged.length) this.files.set(key, merged); else this.files.delete(key);
    this.wake();
    return true;
  }
  // Grant every waiter whose lock no longer conflicts.
  wake() {
    for (const waiter of this.waiters.slice()) {
      if (this.conflict(waiter.key, waiter.lock)) continue;
      const i = this.waiters.indexOf(waiter);
      if (i < 0) continue;
      this.waiters.splice(i, 1);
      this.set(waiter.key, waiter.lock);
      waiter.resume();
    }
  }
  // Would waiting for `lock` close a cycle of owners waiting on each other?
  deadlocks(key, lock) {
    const reaches = (owner, seen) => {
      if (owner === lock.owner) return true;
      if (seen.has(owner)) return false;
      seen.add(owner);
      return this.waiters.filter((w) => w.lock.owner === owner)
        .some((w) => (this.files.get(w.key) || []).some((x) => this.overlaps(x, w.lock) && reaches(x.owner, seen)));
    };
    return (this.files.get(key) || []).some((x) => this.overlaps(x, lock) && reaches(x.owner, new Set()));
  }
  // Park until the lock can be granted; resume() runs after it has been set.
  wait(key, lock, cid, resume) {
    this.waiters.push({ key, lock, cid, resume });
  }
  cancel(cid) {
    for (const w of this.waiters.slice()) if (w.cid === cid) this.waiters.splice(this.waiters.indexOf(w), 1);
  }
  // Closing any descriptor of a file drops the process's locks on it (POSIX);
  // exit drops them all. Waiters of the owner on those files are abandoned
  // and told through onAbandon(waiter).
  release(owner, key, onAbandon) {
    for (const [name, locks] of this.files) {
      if (key !== undefined && name !== key) continue;
      const remaining = locks.filter((x) => x.owner !== owner);
      if (remaining.length) this.files.set(name, remaining); else this.files.delete(name);
    }
    for (const w of this.waiters.slice()) {
      if (w.lock.owner === owner && (key === undefined || w.key === key)) {
        this.waiters.splice(this.waiters.indexOf(w), 1);
        if (onAbandon) onAbandon(w);
      }
    }
    this.wake();
  }
  held(key) { return (this.files.get(key) || []).slice(); }
};
