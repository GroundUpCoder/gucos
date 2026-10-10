// Vendored from GroundUpCoder/c-compiler for the wasm-posix BusyBox broker.
// kernel.js — the process control plane (owner side). Design: docs/KERNEL.md.
//
// This is the per-SYSTEM half of the OS: process table, spawn/wait/kill,
// signal routing, and (in later phases) tty line discipline and pipe
// rendezvous. host.js is the per-PROCESS half — it runs inside every process
// worker and knows nothing about other processes; its `spawnHooks` seam is
// how a process reaches this kernel.
//
// Implemented phases (see KERNEL.md "Implementation phases"):
//   Phase 1 — process table (pid/ppid/pgid/sid, RUNNING/ZOMBIE, reaping,
//     orphan reparenting to pid 1, pid-1 exit halts the system); the
//     per-process kernel page (SAB) + JSON block-RPC transport; KernelClient
//     plugging into host.js's spawnHooks seam; spawn/wait/kill/compile.
//   Phase 2 (docs/archive/0001) — asynchronous signal delivery: SIGPEND/SIGBLOCK
//     live on the kernel page, host.js claims deliverable bits at libc safe
//     points and runs C handlers via the __sig_dispatch export; blocking
//     WAIT interrupts with EINTR (krpc-intr); SIGCHLD on child exit; the
//     ordered exit handshake (OP.EXIT).
//   Phase 3 (docs/archive/0002) — the Tty object: kernel-side line discipline,
//     termios RPCs, control chars as fg-pgroup signals (Ctrl-C = SIGINT).
//   Phase 4 (docs/archive/0003) — pipes as OFDs (PIPE_CREATE + kernel-side buffers,
//     blocking read/write via deferred RPCs, EOF/EPIPE + SIGPIPE, select
//     readiness) and job control (STOPPED state, cooperative stop at safe
//     points via KP_FLAGS.STOP, SIGCONT resume, WUNTRACED/WCONTINUED,
//     SIGTTIN for background tty readers).
//
// Environment: plain JS, no build step, Node + browser (same discipline as
// host.js/compiler.js — no Node-only APIs without a typeof-guard). The
// Kernel/KernelClient classes are environment-neutral (SAB + Atomics only);
// worker creation is an injected capability. nodeCreateWorker() is the
// tested Node reference factory; the browser factory ships with the os/
// page (OS.md "Reference build") where it can actually be exercised.

'use strict';

/* ============================================================
 * Kernel page — the per-process SAB shared kernel <-> process.
 *
 * Int32 words (all access via Atomics):
 *   [0] KP_DOORBELL   seq counter; the kernel bumps + notifies it on ANY
 *                     event for this process (RPC response now; signals,
 *                     child-state changes, tty/pipe readiness in later
 *                     phases). Every process-side blocking loop parks here.
 *   [1] KP_SIGPEND    pending-signal bitmask, bit (sig-1) — matches libc's
 *                     sigset_t convention. Kernel ORs bits in; libc claims
 *                     them with Atomics.and at dispatch (Phase 2).
 *   [2] KP_SIGBLOCK   blocked mask (libc writes via sigprocmask; Phase 2).
 *   [3] KP_FLAGS      bit0 STOP-requested; bit1 in-sigdispatch (Phase 2/4).
 *   [4] KP_RPC_STATE  RPC_IDLE / RPC_REQUEST / RPC_DONE.
 *   [5] KP_RPC_OP     opcode of the in-flight request.
 *   [6] KP_RPC_LEN    payload byte length (request, then response).
 *   [7] KP_RPC_KIND   payload encoding: RPCK_JSON | RPCK_RAW.
 *   [8..] payload     UTF-8 JSON (request, then response, in place),
 *                     up to KP_PAYLOAD_CAP — the page tail past it holds:
 *   [N-18,N-17] fork park request/acknowledgment generations (zero = released).
 *   [N-16..N-5] the vDSO block (docs/archive/0179) — kernel-written, process-read
 *                      state PUBLISHED instead of served (KERNEL.md "What
 *                      may leave the kernel"). One seqlock word guards the
 *                      rest: the kernel (the single writer, one thread by
 *                      construction) bumps KP_VD_SEQ odd, stores the
 *                      fields, bumps it even; readers retry on odd or
 *                      changed seq (KernelClient._vdsoRead) and fall back
 *                      to the RPC — still the source of truth — after a
 *                      bounded spin. Republished at spawn, SETPGID, SETSID,
 *                      reparent-to-init, and setScreen (fan-out).
 *     [N-16] KP_VD_SEQ        seqlock version (even = stable)
 *     [N-15] KP_VD_PID        own pid
 *     [N-14] KP_VD_PPID       parent pid (tracks reparenting, unlike the
 *                             spawn-time static)
 *     [N-13] KP_VD_PGID       process-group id
 *     [N-12] KP_VD_SID        session id
 *     [N-11] KP_VD_BOOT_LO    boot instant (kernel _bootMs, ms since epoch,
 *     [N-10] KP_VD_BOOT_HI    unsigned lo/hi halves) — uptime with no RPC
 *     [N-9]  KP_VD_SCREEN_W   screen dims (ctor default, then setScreen)
 *     [N-8]  KP_VD_SCREEN_H
 *     [N-7..N-5] reserved (published zero)
 *   [N-4] KP_VSYNC_ARMED  vsync-waiter count (docs/archive/0169): the process side
 *                      Atomics.add's it BEFORE parking on KP_VSYNC_SEQ and
 *                      subtracts on resolve; the compositor's park decision
 *                      re-reads it AFTER publishing KP_COMP_PARKED — the
 *                      Dekker pair that makes a lost waiter impossible.
 *   [N-3] KP_COMP_PARKED  1 = the compositor's rAF is parked, no ticks are
 *                      coming (docs/archive/0169). The process side re-reads it
 *                      after publishing ARMED / a present's seq bump and
 *                      posts {type:'want-frame'} when set — the doorbell
 *                      that wakes the on-demand compositor.
 *   [N-2] KP_VSYNC_EN  1 = the embedder broadcasts vsync ticks (set once
 *                      at spawn from Kernel({vsync}); docs/archive/0100).
 *   [N-1] KP_VSYNC_SEQ tick counter — vsyncTick() bumps + notifies it per
 *                      compositor frame; host.js's surface backend paces
 *                      SDL frame loops off it. No ticks (hidden tab) =
 *                      SDL apps park at their next frame boundary.
 *
 * One RPC in flight per process by construction: the process worker is
 * parked on the doorbell for the duration of every call.
 * ============================================================ */
var KP_DOORBELL = 0;
var KP_SIGPEND = 1;
var KP_SIGBLOCK = 2;
var KP_FLAGS = 3;
var KP_RPC_STATE = 4;
var KP_RPC_OP = 5;
var KP_RPC_LEN = 6;
var KP_RPC_KIND = 7;               // payload encoding: RPCK_JSON | RPCK_RAW
var KP_PAYLOAD_OFF = 32;           // byte offset of the payload region
var KP_SIZE = 64 * 1024;           // bounded RPC page; compiler output is paged
var KP_VSYNC_ARMED = (KP_SIZE >> 2) - 4;   // tail words (docs/archive/0169): vsync
var KP_COMP_PARKED = (KP_SIZE >> 2) - 3;   // waiter count + compositor-parked flag
var KP_VSYNC_EN = (KP_SIZE >> 2) - 2;   // tail words (docs/archive/0100): vsync
var KP_VSYNC_SEQ = (KP_SIZE >> 2) - 1;  // advertise flag + tick counter
// vDSO block (docs/archive/0179): seqlock-published kernel state, see layout above.
var KP_VD_SEQ      = (KP_SIZE >> 2) - 16;
var KP_VD_PID      = (KP_SIZE >> 2) - 15;
var KP_VD_PPID     = (KP_SIZE >> 2) - 14;
var KP_VD_PGID     = (KP_SIZE >> 2) - 13;
var KP_VD_SID      = (KP_SIZE >> 2) - 12;
var KP_VD_BOOT_LO  = (KP_SIZE >> 2) - 11;
var KP_VD_BOOT_HI  = (KP_SIZE >> 2) - 10;
var KP_VD_SCREEN_W = (KP_SIZE >> 2) - 9;
var KP_VD_SCREEN_H = (KP_SIZE >> 2) - 8;
var KP_FORK_REQ = (KP_SIZE >> 2) - 18;
var KP_FORK_ACK = (KP_SIZE >> 2) - 17;
var KP_PAYLOAD_CAP = KP_SIZE - KP_PAYLOAD_OFF - 72;   // payload stops short of the
                                   // 18 tail words (fork handshake, vDSO block and vsync words)
/* Bulk-lane chunk sizes — DERIVED from KP_PAYLOAD_CAP, never restated
 * (docs/archive/0235). Byte-stream RPCs move at most one payload per round trip;
 * each lane reserves framing headroom under the cap and rounds down to its
 * historical granule, so a future page-size change reaches every chunking
 * site from this one line:
 *  - KP_FS_CHUNK — RemoteFS read/write (FS_WRITE frames a 4-byte fd;
 *    granule 10000 → 60000 today). The pty ONLCR whole-or-block proof
 *    asserts against it at PTY_OUT_CAP below.
 *  - KP_HOOK_CHUNK — the host.js-owned clipboard/http staging lanes
 *    (frame headers ≤ 12 bytes; granule 16K → 49152 today). Published
 *    across the process boundary as spawnHooks().payloadChunk, so host.js
 *    never restates the kernel-page layout.
 *  - KP_DIR_PAGE — FS_OPENDIR/FS_READDIR entry pages (docs/archive/0241). A JSON
 *    lane, so the budget bounds the MEASURED per-entry bytes (UTF-8 of
 *    each entry's JSON, +1 for the array comma); 64 bytes reserves the
 *    {"entries":[…],"more":N} reply envelope. Kernel-internal — the
 *    client just drains `more` cursors, it never needs the number. */
var KP_FS_CHUNK = Math.floor((KP_PAYLOAD_CAP - 4) / 10000) * 10000;
var KP_HOOK_CHUNK = Math.floor((KP_PAYLOAD_CAP - 16) / 16384) * 16384;
var KP_DIR_PAGE = KP_PAYLOAD_CAP - 64;
if (KP_FS_CHUNK < 4096 || KP_HOOK_CHUNK < 4096) {
  throw new Error('kernel page too small for the bulk-lane chunk derivations');
}

var RPC_IDLE = 0, RPC_REQUEST = 1, RPC_DONE = 2;
var KF_STOP = 1;                   // KP_FLAGS bit0: park at the next safe point
var RPCK_JSON = 0, RPCK_RAW = 1;   // RAW: fs read/write bulk bytes — no JSON,
                                   // no structured clone, one memcpy each way

/* Opcode space (docs/KERNEL.md): 0x00xx process, 0x01xx tty, 0x02xx pipes,
 * 0x03xx misc, 0x04xx brokered fs, 0x05xx AF_UNIX sockets, 0x06xx HTTP
 * transport (docs/archive/0172, fetch-backed), 0x1xxx reserved for WM surfaces.
 * Only the ops the current phase implements are dispatched; the rest
 * respond ENOSYS. */
var OP = {
  SPAWN: 0x0001,
  WAIT: 0x0002,
  KILL: 0x0003,
  EXIT: 0x0004,      // ordered exit handshake (no response; kernel tears down)
  SETPGID: 0x0005,
  GETPGID: 0x0006,
  SETSID: 0x0007,
  SIGDISP: 0x0008,
  SIGMASK: 0x0009,   // reserved: Phase 2
  GETSID: 0x000A,    // libc getsid() (docs/archive/0043 — pgrep -s 0 wants it)
  // Interval timers (docs/archive/0044): ONE real-time timer per process (POSIX
  // ITIMER_REAL); expiry posts SIGALRM through the ordinary SIGPEND path.
  // ms resolution over the wire; VIRTUAL/PROF answer EINVAL (no CPU
  // accounting — fail loud, documented).
  SETITIMER: 0x000B,
  GETITIMER: 0x000C,
  TCGETATTR: 0x0101,
  TCSETATTR: 0x0102,
  TCGETPGRP: 0x0103,
  TCSETPGRP: 0x0104,
  // Ptys (docs/archive/0020): PTY_CREATE makes a master/slave pair — the slave is
  // a full Tty (line discipline reused verbatim); TIOCSWINSZ is the master
  // side's resize (winsize words + SIGWINCH to the pty's fg pgroup).
  TIOCSWINSZ: 0x0105,
  PTY_CREATE: 0x0106,
  PTY_CONTROL: 0x0107,
  // 0x02xx pipes. Post-0009 only CREATE is an opcode: the design doc's
  // PIPE_REF/CLOSE/WAIT/NOTIFY are subsumed by the kernel-owned fd layer
  // (OFD refcounts + FS_READ/FS_WRITE/FS_CLOSE + the doorbell).
  PIPE_CREATE: 0x0201,
  // SPSC fast-path doorbell (docs/archive/0181): a fast end that committed a ring
  // op while the peer's PR_RWAIT/PR_WWAIT flag was up rings the kernel so
  // the parked peer (FS_WAIT / a brokered stream op) is re-served. With
  // {epipe:1} the caller hit PRF_RGONE locally and asks for its own
  // SIGPIPE (write-to-closed-pipe semantics, one RPC on the error path).
  PIPE_KICK: 0x0202,
  COMPILE: 0x0301,
  // System clipboard (docs/archive/0090): ONE kernel-held slot {fmt, bytes} so
  // copy/paste crosses processes and survives the writer exiting (Win95
  // semantics — one slot, no history). fmt 1 = UTF-8 text; the tag exists
  // so CF_BITMAP / file lists (docs/archive/0092) can ride the same slot later.
  // Payloads chunk through the 64KB kernel page: SET is a RAW request
  // [u32 fmt][u32 last][u32 off][bytes...] staged per-pcb and committed
  // only on last (a dying writer never leaves a torn slot); GET is JSON
  // {fmt, off, peek} -> RAW [i32 total][chunk], total -1 when empty or the
  // stored format differs. Cross-chunk reads are not snapshot-atomic by
  // design (single slot, last-write-wins; the C side retries on growth).
  // peek 1 = availability probe (__clip_has), always the cached slot; a
  // DATA read at off 0 may park on the embedder's host-clipboard refresh
  // (opts.onClipRead — the clipboard seam, see the OP.CLIP_GET dispatch).
  CLIP_SET: 0x0302,
  CLIP_GET: 0x0303,
  // Egress (docs/archive/0398): gucOS -> host file transfer. RAW request, textual —
  // a "download\n" | "saveas\n" disposition header line ("clipboard\n" is
  // RESERVED for a future file-flavored host clipboard write), then one
  // absolute path per '\n'-terminated line (the FO_CLIP_FMT=2 list shape).
  // The wire carries PATHS; the kernel — the fs owner — materializes bytes
  // exactly once, synchronously in this dispatch (lone file -> its bytes;
  // directory or multi-selection -> ONE store-only zip, symlinks preserved
  // as symlink entries; a LONE symlink follows to its target), and hands
  // exactly one artifact {name, bytes} to opts.onEgress. Reply {} or a
  // pre-acceptance errno: EINVAL (bad header / relative path / empty list),
  // E2BIG (list over EG_REQ_MAX), ENOENT/EACCES (walk), EFBIG (lstat-summed
  // size over EGRESS_MAX, decided BEFORE any byte is read), ENOSYS (no
  // embedder hook — egress without a host is meaningless, fail loud).
  EGRESS: 0x0304,
  // 0x04xx — the brokered filesystem (KERNEL.md "fd/data-plane amendment").
  FS_OPEN: 0x0401, FS_CLOSE: 0x0402, FS_READ: 0x0403, FS_WRITE: 0x0404,
  FS_LSEEK: 0x0405, FS_STAT: 0x0406, FS_LSTAT: 0x0407, FS_FSTAT: 0x0408,
  FS_ACCESS: 0x0409, FS_UNLINK: 0x040A, FS_RENAME: 0x040B, FS_MKDIR: 0x040C,
  FS_RMDIR: 0x040D, FS_LINK: 0x040E, FS_SYMLINK: 0x040F, FS_READLINK: 0x0410,
  FS_FTRUNCATE: 0x0411, FS_CHMOD: 0x0412, FS_FCHMOD: 0x0413, FS_CHDIR: 0x0414,
  FS_GETCWD: 0x0415, FS_DUP: 0x0416, FS_DUP2: 0x0417, FS_OPENDIR: 0x0418,
  FS_REALPATH: 0x0419, FS_UTIME: 0x041A, FS_FUTIME: 0x041B, FS_ISATTY: 0x041C,
  FS_SELECT: 0x041D, FS_FCNTL_DUPFD: 0x041E, FS_FSYNC: 0x041F,
  // FS_READDIR (docs/archive/0241): the big-directory page fetch. FS_OPENDIR
  // returns the first page of entries under KP_DIR_PAGE; when more remain
  // it parks the open backend handle behind a per-process cursor id
  // (reply `more`), and FS_READDIR {dir} drains it page by page — the
  // final page carries no `more` and closes the handle. 0x0421 because
  // FS_WAIT below predates it at 0x0420.
  FS_READDIR: 0x0421,
  FS_FDFLAGS: 0x0423,
  FS_PREAD: 0x042E, FS_PWRITE: 0x042F,
  FS_GETFL: 0x042B, FS_SETFL: 0x042C, FS_POLL: 0x042D,
  // Mapped memory and record locks (docs/MAPPINGS.md): mmap/munmap/mprotect/
  // msync over broker-owned shared pages, and fcntl F_GETLK/F_SETLK/F_SETLKW.
  FS_MMAP: 0x0426, FS_MUNMAP: 0x0427, FS_MPROTECT: 0x0428, FS_MSYNC: 0x0429,
  FS_FCNTL_LOCK: 0x042A,
  // FS_WATCH (file watching): create a PATH-KEYED watch fd — one watch per
  // fd, close is removal. { path, mask, flags } -> { fd }. The fd reads
  // packed fsw_event records (os/fswatch.h MUST MATCH the FSW table below),
  // is readable in FS_SELECT/FS_WAIT like any other OFD kind, and answers
  // EAGAIN when the queue is dry (WAIT-first contract — watch fds never
  // block). Watches key on the LEXICALLY-canonical absolute path, so an
  // editor's tmp+rename-over save lands on the watched path as one
  // settled-write event and the watch SURVIVES — the inotify per-inode
  // rename-over-save trap is structurally absent. flags is reserved
  // (FSWF_RECURSIVE spec'd as a prefix compare; wired on first consumer).
  FS_WATCH_OPEN: 0x0422,
  // Unified wait (docs/archive/0178): ONE deferred park over fds ⊕ the input ring
  // ⊕ a timeout, interruptible by signals (krpc-intr → EINTR) — the only
  // sanctioned way to sleep on multiple sources (KERNEL.md single-writer
  // rule). Readiness-check and park are atomic kernel-side, so the
  // check→park lost-wakeup class (the 0168 kick + pre-park select era)
  // is structurally impossible.
  FS_WAIT: 0x0420,
  // 0x05xx — AF_UNIX sockets (docs/archive/0008). Stream-only; data flows through
  // FS_READ/FS_WRITE/FS_CLOSE/FS_SELECT like every other OFD kind.
  SOCK_SOCKET: 0x0501, SOCK_BIND: 0x0502, SOCK_LISTEN: 0x0503,
  SOCK_ACCEPT: 0x0504, SOCK_CONNECT: 0x0505, SOCK_PAIR: 0x0506,
  SOCK_SHUTDOWN: 0x0507, SOCK_QUERY: 0x0508, SOCK_SEND: 0x0509, SOCK_RECV: 0x050a,
  // 0x06xx — HTTP transport (docs/archive/0172; fd-shaped since docs/archive/0417).
  // Fetch-shaped, one transfer per OFD: HTTP_BODY stages an optional request
  // body (RAW [u32 off][bytes], contiguous like CLIP_SET), HTTP_OPEN (JSON
  // {method,url,headers[],headersMs,idleMs}) consumes it, kicks off the
  // embedder's fetch, and returns {fd} at once — an ORDINARY fd whose OFD
  // (kind 'http') owns the transfer, so it joins FS_SELECT/FS_WAIT, drains
  // via FS_READ (EAGAIN when dry — never parks), and dies via FS_CLOSE
  // (last release aborts the fetch). HTTP_STATUS is NON-blocking: EAGAIN
  // before headers, {status,headers} after (and it consumes the status —
  // the readability leg it satisfies re-arms only on body progress). Two
  // kernel deadlines bound every transfer (headers + idle; expiry =
  // ETIMEDOUT on the error leg). Not a socket layer (the browser can't
  // do raw TCP); TLS is the fetch stack's. Semantics: docs/archive/0172 +
  // docs/archive/0417 + the "HTTP transport" section in KERNEL.md.
  // 0x0604 (HTTP_READ) and 0x0605 (HTTP_CLOSE) are RETIRED by 0417 —
  // FS_READ/FS_CLOSE serve their roles; the opcodes are never reused.
  // HTTP_ERROR (#392) is the error-TEXT peek: {fd} -> {error, errno} — the
  // transfer's recorded failure text ('' while healthy), never consuming,
  // callable in any state while the fd lives. The C surface only saw errno
  // before this; the precise transport diagnostic (a CORS denial, a dead
  // bridge, a bad URL) died in a host-side console.error.
  HTTP_BODY: 0x0601, HTTP_OPEN: 0x0602, HTTP_STATUS: 0x0603,
  HTTP_ERROR: 0x0606,
  // 0x2xxx — the audio mixer (docs/archive/0017; design: WM.md "Audio mixing").
  // Control plane only: PCM rides the per-process source ring SABs and the
  // one page-owned output ring — never RPCs. AUDIO_GAIN (docs/archive/0048, the
  // control panel's volume): master output gain in percent, 0..200;
  // gain < 0 queries. Applied in audioPump before the clamp.
  AUDIO_OPEN: 0x2001, AUDIO_CLOSE: 0x2002, AUDIO_GAIN: 0x2003, AUDIO_CLEAR: 0x2004,
  // 0x21xx — gamepad (#607). Control-plane query only: pad STATE rides the
  // input ring (WMEV.GAMEPAD_*); this fetches the one thing an 8-word ring
  // record cannot carry, the device name string. {id} -> {name} (JSON, the
  // GETSID shape); ENODEV for an id the kernel never allocated.
  PAD_NAME: 0x2101,
};

/* strace (docs/archive/0046): the decode table IS the OP table — opcode names come
 * from the constants above, so a new opcode traces by construction. */
var OP_NAMES = {};
for (var opName in OP) OP_NAMES[OP[opName]] = opName;

/* FS_WATCH event vocabulary — MUST MATCH os/fswatch.h. The record stream a
 * watch fd reads is packed little-endian:
 *   [u16 len][u8 type][u8 flags][name... NUL, padded so len % 4 == 0]
 * `name` is the watch-relative child name for dir-watch events (RENAME:
 * "old\0new\0" — ONE record, both names, no inotify cookie protocol),
 * empty for self events. FSW_MODIFY (every write/ftruncate — mid-write
 * chatter) is OPT-IN: the default mask is the settled set, inverting
 * inotify's trap where IN_MODIFY is the obvious-looking wrong choice.
 * FSW_CLOSE_WRITE is the settled write: a dirty open-file-description's
 * LAST release, or a rename landing complete content at the path. */
var FSW_CLOSE_WRITE = 1;   // content at the path is complete — safe to react
var FSW_CREATE = 2;        // entry appeared (create/mkdir/symlink/link/move-in of a dir)
var FSW_DELETE = 3;        // entry disappeared (unlink/rmdir/move-out)
var FSW_RENAME = 4;        // same-dir rename under a dir watch (both names)
var FSW_SELF_GONE = 5;     // the watched path itself unlinked/renamed away (watch stays armed)
var FSW_MODIFY = 6;        // each write/ftruncate — opt-in
var FSW_OVERFLOW = 7;      // queue overflowed: the consumer must rescan
var FSWF_ISDIR = 1;        // record flag: the subject is a directory
var FSW_MASK_DEFAULT =     // everything except MODIFY (the settled set)
  (1 << FSW_CLOSE_WRITE) | (1 << FSW_CREATE) | (1 << FSW_DELETE) |
  (1 << FSW_RENAME) | (1 << FSW_SELF_GONE) | (1 << FSW_OVERFLOW);
var FSW_QUEUE_CAP = 128;   // per-watch-fd bound; overflow = clear + latch

/* Wait options / status packing — must match <sys/wait.h>. */
var WNOHANG = 0x01, WUNTRACED = 0x02, WCONTINUED = 0x08;

/* Interval timers (docs/archive/0044) — must match <sys/time.h>. Only the
 * real-time flavor exists (workers run on their own OS threads, so there
 * is no per-process CPU accounting to back VIRTUAL/PROF). */
var ITIMER_REAL = 0;
function W_EXITCODE(code) { return (code & 0xff) << 8; }
function W_TERMSIG(sig) { return sig & 0x7f; }
function W_STOPCODE(sig) { return ((sig & 0xff) << 8) | 0x7f; }
var W_CONTINUED_STATUS = 0xffff;

/* Pipes (docs/archive/0003): kernel-side buffers — rendezvous, not bulk data.
 * PIPE_ATOMIC mirrors POSIX PIPE_BUF (writes that small never interleave:
 * they defer whole rather than land partially). */
var PIPE_CAP = 64 * 1024;
var PIPE_ATOMIC = 512;

/* ---- SPSC pipe rings (docs/archive/0181; KERNEL.md "single-writer rule") ----
 * A pipe whose creator posted a ring SAB ({type:'pipe-sab'}, the audio-sab
 * handshake) keeps its bytes in that ring IN EVERY MODE — the kernel's own
 * stream ops read/write the ring through the _pipeAvail/_pipeTake/_pipePut
 * accessors, so promotion and demotion move no data and need no locks (a
 * demotion-time ring→buf drain would race a mid-flight fast reader).
 *
 * Header words (32-byte header, data area after; every word has ONE writer):
 *   PR_MODE   kernel-owned   the one-way ladder LATENT → FAST → DEMOTED.
 *                            FAST = both ends single-holder: the holders
 *                            memcpy + Atomics locally, zero data RPCs.
 *   PR_HEAD   consumer-owned free-running u32 byte index (reader end).
 *   PR_TAIL   producer-owned free-running u32 byte index (writer end).
 *   PR_FLAGS  kernel-owned   PRF_RGONE/PRF_WGONE — close/exit are always
 *                            brokered, so the kernel latches peer-gone here
 *                            and the fast peer discovers EOF/EPIPE locally.
 *   PR_RWAIT/ kernel-owned   "a waiter is parked interested in this end"
 *   PR_WWAIT                 (any mode; set BEFORE the park's readiness
 *                            rescan, cleared by _cancelWaiter). A fast peer
 *                            that commits while the flag is up sends
 *                            PIPE_KICK — the suppressed-doorbell protocol:
 *                            flag-then-rescan vs commit-then-check-flag
 *                            cross under SC atomics, so a wake can be
 *                            redundant but never lost (the 0168 lesson).
 *
 * Mode moves only while every process that could fast-op the flipped role
 * is parked in the very RPC doing the flip: promotion fires on a holder
 * REMOVAL (the closer/exiter is parked; not at create — the creator's
 * spawns would demote a promoted pipe and burn the one-way ladder, so
 * same-process self-pipes deliberately stay brokered), demotion fires on
 * spawn inheritance (the only event that adds a holder; the spawner is
 * parked). A straggler fast op that loaded FAST just before a demotion
 * commits harmlessly: its role's end stayed single-holder, so the kernel
 * only exercises that role while the same process is parked in an RPC —
 * at most one producer and one consumer touch the ring at any instant. */
var PIPE_RING_HDR = 32;
var PR_MODE = 0, PR_HEAD = 1, PR_TAIL = 2, PR_FLAGS = 3, PR_RWAIT = 4, PR_WWAIT = 5;
var PR_LATENT = 0, PR_FAST = 1, PR_DEMOTED = 2;
var PRF_RGONE = 1, PRF_WGONE = 2;
// A ringed pipe's capacity IS its ring (pipe.cap follows the SAB the
// creator allocated): 256K, 4× the brokered PIPE_CAP — every park/kick
// cycle moves a whole ring, so a bigger elastic buffer means fewer wake
// RPCs per MB. POSIX only mandates PIPE_BUF (512) atomicity, not a size.
var PIPE_RING_BYTES = PIPE_RING_HDR + 256 * 1024;

function pipeRingViews(sab) {
  return { sab: sab, i32: new Int32Array(sab, 0, PIPE_RING_HDR >> 2),
           u8: new Uint8Array(sab, PIPE_RING_HDR),
           cap: sab.byteLength - PIPE_RING_HDR };
}
function pipeRingAvail(ring) {
  return (Atomics.load(ring.i32, PR_TAIL) - Atomics.load(ring.i32, PR_HEAD)) >>> 0;
}
/* Producer side: copy `bytes` in at TAIL (caller checked the space) and
 * commit. Free-running indices; the data area wraps. */
function pipeRingPut(ring, bytes) {
  var tail = Atomics.load(ring.i32, PR_TAIL) >>> 0;
  var off = tail % ring.cap;
  var first = Math.min(bytes.length, ring.cap - off);
  ring.u8.set(bytes.subarray(0, first), off);
  if (first < bytes.length) ring.u8.set(bytes.subarray(first), 0);
  Atomics.store(ring.i32, PR_TAIL, (tail + bytes.length) | 0);
}
/* Consumer side: copy `n` bytes out from HEAD into `dst` (caller checked
 * avail; dst.length >= n) and commit. */
function pipeRingTakeInto(ring, dst, n) {
  var head = Atomics.load(ring.i32, PR_HEAD) >>> 0;
  var off = head % ring.cap;
  var first = Math.min(n, ring.cap - off);
  dst.set(ring.u8.subarray(off, off + first), 0);
  if (first < n) dst.set(ring.u8.subarray(0, n - first), first);
  Atomics.store(ring.i32, PR_HEAD, (head + n) | 0);
}
function pipeRingTake(ring, n) {
  var out = new Uint8Array(n);
  pipeRingTakeInto(ring, out, n);
  return out;
}

/* HTTP transport (docs/archive/0172): per-transfer body backpressure threshold.
 * The async fetch reader pauses once this many bytes are queued and resumes
 * when an FS_READ drains below it — bounded kernel memory regardless of how
 * fast the network delivers vs how slowly the C consumer reads. */
var HTTP_BUF_CAP = 256 * 1024;

/* HTTP deadlines (docs/archive/0417): kernel DEFAULTS, so a caller that sets
 * nothing is still bounded. HTTP_OPEN overrides per transfer: headersMs > 0
 * replaces the headers deadline (the response headers must arrive within
 * it); idleMs > 0 replaces the idle deadline (the body must deliver at
 * least one byte within it, measured only while the kernel is actually
 * waiting on the network — a backpressure pause stops the clock), and
 * idleMs < 0 DISABLES it (a server-sent-event stream is legitimately
 * silent for long stretches). The headers deadline is never disableable.
 * Expiry aborts the fetch and fails the transfer with ETIMEDOUT — a
 * distinguishable error, and a consumable: the fd reports readable and
 * the reader collects it. */
var HTTP_HEADERS_MS = 30 * 1000;
var HTTP_IDLE_MS = 120 * 1000;

/* ---- strace formatting (docs/archive/0046) ----
 * Pure text: one strace-flavored line per RPC — NAME(k=v, ...) = result.
 * Strings/arrays/previews are capped so a traced `cat` of a big file stays
 * readable and the trace pipe stays small; the caps are presentation only
 * (nothing round-trips through these). */
var TRACE_STR_MAX = 64;    // per-string cap in request args
var TRACE_DATA_MAX = 32;   // read/write data preview cap (strace's -s default)
var TRACE_ARR_MAX = 8;     // per-array element cap (argv/envp)

function traceQuoteByte(c) {
  if (c === 34) return '\\"';
  if (c === 92) return '\\\\';
  if (c === 10) return '\\n';
  if (c === 9) return '\\t';
  if (c === 13) return '\\r';
  if (c < 32 || c > 126) return '\\x' + (c | 256).toString(16).slice(-2);
  return String.fromCharCode(c);
}

function traceStr(s, max) {
  var body = '';
  var n = Math.min(s.length, max);
  // ASCII-only fallback by design: non-ASCII units render as \xNN escapes
  // (strace output is a diagnostic byte dump, not a text surface).
  for (var i = 0; i < n; i++) body += traceQuoteByte(s.charCodeAt(i) & 0xff);
  return '"' + body + '"' + (s.length > max ? '...' : '');
}

function traceBytes(u8, max) {
  var body = '';
  var n = Math.min(u8.length, max);
  for (var i = 0; i < n; i++) body += traceQuoteByte(u8[i]);
  return '"' + body + '"' + (u8.length > max ? '...' : '');
}

function traceVal(v, depth) {
  if (v === null || v === undefined) return 'NULL';
  var t = typeof v;
  if (t === 'number' || t === 'boolean') return '' + v;
  if (t === 'string') return traceStr(v, TRACE_STR_MAX);
  if (v instanceof Uint8Array) return '<' + v.length + ' bytes>';
  if (Array.isArray(v)) {
    if (depth <= 0) return '[...]';
    var parts = [];
    for (var i = 0; i < v.length && i < TRACE_ARR_MAX; i++) parts.push(traceVal(v[i], depth - 1));
    if (v.length > TRACE_ARR_MAX) parts.push('...+' + (v.length - TRACE_ARR_MAX));
    return '[' + parts.join(', ') + ']';
  }
  if (t === 'object') {
    if (depth <= 0) return '{...}';
    var ps = [];
    for (var k in v) ps.push(k + '=' + traceVal(v[k], depth - 1));
    return '{' + ps.join(', ') + '}';
  }
  return '?';
}

/* Request args, formatted EAGERLY at dispatch (a RAW payload is a view into
 * the kernel page — the response reuses that region, so nothing may hold it
 * past the dispatch turn). */
function traceArgs(op, req) {
  if (req && req.raw) {
    // RAW-request ops: FS_WRITE [u32 fd][bytes...], CLIP_SET (docs/archive/0090)
    // [u32 fmt][u32 last][u32 off][bytes...].
    var raw = req.raw;
    if (op === OP.CLIP_SET && raw.length >= 12) {
      var cdv = new DataView(raw.buffer, raw.byteOffset, raw.length);
      return 'fmt=' + cdv.getUint32(0, true) + ', last=' + cdv.getUint32(4, true) +
        ', off=' + cdv.getUint32(8, true) +
        ', data=' + traceBytes(raw.subarray(12), TRACE_DATA_MAX) +
        ', count=' + (raw.length - 12);
    }
    if (op === OP.FS_WRITE && raw.length >= 4) {
      var fd = raw[0] | (raw[1] << 8) | (raw[2] << 16) | (raw[3] << 24);
      var data = raw.subarray(4);
      return 'fd=' + fd + ', data=' + traceBytes(data, TRACE_DATA_MAX) +
        ', count=' + data.length;
    }
    return 'raw=<' + raw.length + ' bytes>';
  }
  if (!req || typeof req !== 'object') return '';
  var parts = [];
  for (var k in req) parts.push(k + '=' + traceVal(req[k], 2));
  return parts.join(', ');
}

function traceResult(resp, rawBytes) {
  if (rawBytes) {
    return '' + rawBytes.length +
      (rawBytes.length ? ' ' + traceBytes(rawBytes, TRACE_DATA_MAX) : '');
  }
  if (resp && resp.errno) return '-1 ' + resp.errno;
  if (!resp) return '0';
  var keys = Object.keys(resp);
  if (keys.length === 0) return '0';
  if (keys.length === 1 && typeof resp[keys[0]] === 'number') return '' + resp[keys[0]];
  return traceVal(resp, 2);
}

/* Ptys (docs/archive/0020): the slave→master output direction. Sized so a whole
 * worst-case slave write always fits EVENTUALLY: RemoteFS caps writes at
 * KP_FS_CHUNK bytes and OPOST/ONLCR at most doubles them, so the
 * whole-or-block discipline (a \r\n must never split across a full buffer)
 * can always be satisfied by a draining master. The proof is enforced, not
 * prose (docs/archive/0235): a page-layout change that grows KP_FS_CHUNK past the
 * margin fails loud at load instead of silently rotting the discipline. */
var PTY_OUT_CAP = 256 * 1024;
if (KP_FS_CHUNK * 2 > PTY_OUT_CAP) {
  throw new Error('PTY_OUT_CAP must hold one ONLCR-doubled RemoteFS write chunk (2*KP_FS_CHUNK)');
}

/* AF_UNIX sockets (docs/archive/0008): a connection is two pipe-shaped directions
 * (same fields, same waiter queues), so the entire blocking/EOF/EPIPE/
 * select machinery is the pipe machinery. */
function sockDir() {
  return { socket: true, buf: [], cap: PIPE_CAP, rOpen: true, wOpen: true,
           readWaiters: [], writeWaiters: [] };
}
var S_IFSOCK_MODE = 0o140000;

/* ============================================================
 * Audio mixer (docs/archive/0017; design: WM.md "Audio mixing — the kernel sound
 * server"). Ring layout — MUST MATCH host.js createSharedAudioBuffer
 * (16-byte Int32 header + PCM ring):
 *   [0] AU_WPOS    writePos, masked mod capacity (producer-only cursor)
 *   [1] AU_QUEUED  queuedBytes (producer Atomics.add, consumer Atomics.sub —
 *                  the publication cell; shared callback rings maintain a
 *                  consumer-owned read cursor, reset by AUDIO_CLEAR)
 *   [2] AU_PLAYING source rings: written by the PROCESS (SDL3 devices open
 *                  paused; resume sets 1) — the mixer skips paused rings.
 *                  Output ring: written by the page receiver on resume.
 *   [3] reserved
 * Source rings are process-allocated and registered via AUDIO_OPEN (the
 * SAB rides {type:'audio-sab'} immediately before the RPC — the wm-sabs
 * FIFO handshake). The output ring is kernel-allocated (audioInit) and
 * fixed f32 stereo AU_OUT_FREQ; the system page plays it with system-audio.js.
 * SDL audio format words — MUST MATCH <SDL3/SDL_audio.h>. */
var AU_WPOS = 0, AU_QUEUED = 1, AU_PLAYING = 2;
var AU_HDR_BYTES = 16;
var AU_OUT_FREQ = 48000, AU_OUT_CHANNELS = 2;
var AU_FMT_F32 = 0x8120, AU_FMT_S32 = 0x8020, AU_FMT_S16 = 0x8010,
    AU_FMT_S8 = 0x8008, AU_FMT_U8 = 0x0008;
var AU_TARGET_MS = 80;                 // output queue depth the pump tops up to
var AU_OUT_RING_BYTES = 256 * 1024;   // default output ring capacity (~0.68s)

/* Signal numbering + default actions — must match <signal.h> /
 * __sig_default_action in libc. Kind mirror (__on_sigdisp): 0=DFL 1=IGN
 * 2=HANDLER. */
var SIG = {
  HUP: 1, INT: 2, QUIT: 3, ILL: 4, TRAP: 5, ABRT: 6, BUS: 7, FPE: 8,
  KILL: 9, USR1: 10, SEGV: 11, USR2: 12, PIPE: 13, ALRM: 14, TERM: 15,
  CHLD: 17, CONT: 18, STOP: 19, TSTP: 20, TTIN: 21, TTOU: 22, URG: 23,
  WINCH: 28,
};
var NSIG = 32;
var SIG_NAMES = {};
for (var sigName in SIG) SIG_NAMES[SIG[sigName]] = 'SIG' + sigName;
var DISP_DFL = 0, DISP_IGN = 1, DISP_HANDLER = 2;
/* 0=terminate 1=ignore 2=stop 3=continue (libc's __sig_default_action). */
function sigDefaultAction(sig) {
  if (sig === SIG.CHLD || sig === SIG.URG || sig === SIG.WINCH) return 1;
  if (sig === SIG.CONT) return 3;
  if (sig === SIG.STOP || sig === SIG.TSTP || sig === SIG.TTIN || sig === SIG.TTOU) return 2;
  return 0;
}

var textEncoder = new TextEncoder();
var textDecoder = new TextDecoder('utf-8');

function writePayload(i32, u8, obj) {
  var bytes = textEncoder.encode(JSON.stringify(obj === undefined ? {} : obj));
  if (bytes.length > KP_PAYLOAD_CAP) return false;
  u8.set(bytes, KP_PAYLOAD_OFF);
  Atomics.store(i32, KP_RPC_LEN, bytes.length);
  Atomics.store(i32, KP_RPC_KIND, RPCK_JSON);
  return true;
}

function writeRawPayload(i32, u8, bytes) {
  if (bytes.length > KP_PAYLOAD_CAP) return false;
  u8.set(bytes, KP_PAYLOAD_OFF);
  Atomics.store(i32, KP_RPC_LEN, bytes.length);
  Atomics.store(i32, KP_RPC_KIND, RPCK_RAW);
  return true;
}

function readPayload(i32, u8) {
  var len = Atomics.load(i32, KP_RPC_LEN);
  if (Atomics.load(i32, KP_RPC_KIND) === RPCK_RAW) {
    var raw = new Uint8Array(len);
    raw.set(u8.subarray(KP_PAYLOAD_OFF, KP_PAYLOAD_OFF + len));
    return { raw: raw };
  }
  if (len <= 0) return {};
  // Copy out of the SAB before decoding (TextDecoder rejects SAB views).
  var copy = new Uint8Array(len);
  copy.set(u8.subarray(KP_PAYLOAD_OFF, KP_PAYLOAD_OFF + len));
  return JSON.parse(textDecoder.decode(copy));
}

/* ============================================================
 * KernelClient — the process-side stub. Lives in the process worker,
 * speaks block-RPC over the kernel page, and adapts to host.js's
 * existing spawnHooks seam so runModule needs no changes.
 *
 *   var client = new KernelClient(kernelPageSab, postToKernel);
 *   runModule({ ..., spawnHooks: client.spawnHooks() });
 *
 * postToKernel(msg, transferList?) must deliver msg to the kernel's message
 * handler for this process (worker postMessage in both browser and Node;
 * the optional transfer list carries gpu-transport frame bitmaps).
 * ============================================================ */
function KernelClient(sab, postToKernel) {
  this._i32 = new Int32Array(sab);
  this._u8 = new Uint8Array(sab);
  this._post = postToKernel;
}

/* Synchronous block-RPC: returns the response object ({errno: 'NAME'} on
 * failure — the names flow into ctx.setErrnoName via the hooks contract).
 * `interruptible` ops (WAIT) additionally watch for deliverable signals
 * while parked: the first one posts a krpc-intr message and the kernel
 * answers EINTR (or the already-raced real result — both are fine; the
 * signal delivers at the caller's next safe point). */
/* Raw-payload variant (FS_WRITE): `bytes` land in the payload region as-is
 * (op-specific layout); the response comes back through the same
 * readPayload (JSON or raw). */
KernelClient.prototype.callRaw = function (op, bytes, interruptible) {
  if (!writeRawPayload(this._i32, this._u8, bytes)) return { errno: 'E2BIG' };
  return this._finish(op, interruptible);
};

KernelClient.prototype.call = function (op, req, interruptible) {
  if (!writePayload(this._i32, this._u8, req)) return { errno: 'E2BIG' };
  return this._finish(op, interruptible);
};

// A browser process must release its JS event loop while waiting for I/O:
// DOM input and WebGPU device/present callbacks use that same worker queue.
// One logical C stack owns this RPC slot throughout the JSPI suspension.
KernelClient.prototype.callAsync = async function (op, req, interruptible) {
  if (!writePayload(this._i32, this._u8, req)) return { errno: 'E2BIG' };
  this._stopWait();
  var i32=this._i32, sentIntr=false;
  Atomics.store(i32,KP_RPC_OP,op);
  Atomics.store(i32,KP_RPC_STATE,RPC_REQUEST);
  this._post({type:'krpc'});
  for (;;) {
    this.forkWait();
    var seq=Atomics.load(i32,KP_DOORBELL);
    if(Atomics.load(i32,KP_RPC_STATE)===RPC_DONE)break;
    if(interruptible&&!sentIntr&&this.pending()) {
      sentIntr=true;this._post({type:'krpc-intr'});
    }
    var waiter=Atomics.waitAsync(i32,KP_DOORBELL,seq);
    if(waiter.async)await waiter.value;
  }
  var resp=readPayload(i32,this._u8);
  Atomics.store(i32,KP_RPC_STATE,RPC_IDLE);
  return resp;
};

KernelClient.prototype._finish = function (op, interruptible) {
  this._stopWait();          // a stopped process issues no new syscalls
  var i32 = this._i32;
  Atomics.store(i32, KP_RPC_OP, op);
  Atomics.store(i32, KP_RPC_STATE, RPC_REQUEST);
  this._post({ type: 'krpc' });
  // Park on the doorbell until the kernel marks the RPC done. Read the seq
  // BEFORE checking state: if the kernel completes in between, wait() sees a
  // stale seq and returns 'not-equal' immediately — no lost wakeup. The
  // doorbell also fires for non-RPC events (signal posts, child changes), so
  // spurious wakes just re-check state and re-park.
  var sentIntr = false;
  for (;;) {
    this.forkWait();
    var seq = Atomics.load(i32, KP_DOORBELL);
    if (Atomics.load(i32, KP_RPC_STATE) === RPC_DONE) break;
    if (interruptible && !sentIntr && this.pending()) {
      sentIntr = true;
      this._post({ type: 'krpc-intr' });
    }
    Atomics.wait(i32, KP_DOORBELL, seq);
  }
  var resp = readPayload(i32, this._u8);
  Atomics.store(i32, KP_RPC_STATE, RPC_IDLE);
  return resp;
};

/* Deliverable = pending and not blocked. */
KernelClient.prototype.pending = function () {
  return Atomics.load(this._i32, KP_SIGPEND) & ~Atomics.load(this._i32, KP_SIGBLOCK);
};

/* Vsync broadcast (docs/archive/0100). vsyncEnabled: the kernel advertised a real
 * frame clock at spawn (and this engine can await a SAB word). vsyncWait:
 * resolves on the next compositor tick. Tracks the last-delivered seq so a
 * tick that landed while the frame callback ran resolves immediately (rAF
 * catch-up semantics) instead of costing a whole extra frame. */
KernelClient.prototype.vsyncEnabled = function () {
  return typeof Atomics.waitAsync === 'function' &&
         Atomics.load(this._i32, KP_VSYNC_EN) === 1;
};

KernelClient.prototype.vsyncWait = function () {
  var i32 = this._i32;
  var cur = Atomics.load(i32, KP_VSYNC_SEQ);
  if (this._vsyncSeen === undefined) this._vsyncSeen = cur;
  if (cur !== this._vsyncSeen) {           // missed tick(s): fire now
    this._vsyncSeen = cur;
    return Promise.resolve();
  }
  var self = this;
  // On-demand compositor (docs/archive/0169): publish the waiter FIRST, then
  // re-read the parked flag and ring the doorbell if set. This is one half
  // of the Dekker pair — the compositor stores PARKED before re-reading
  // ARMED — so either it sees our count and stays armed, or we see its
  // flag and post the wake; a silent strand (waitAsync registration is
  // passive, the kernel can't see it) is impossible either way.
  Atomics.add(i32, KP_VSYNC_ARMED, 1);
  if (Atomics.load(i32, KP_COMP_PARKED) === 1) this._post({ type: 'want-frame' });
  var r = Atomics.waitAsync(i32, KP_VSYNC_SEQ, cur);
  if (!r.async) {                          // 'not-equal': tick raced the wait
    Atomics.sub(i32, KP_VSYNC_ARMED, 1);
    self._vsyncSeen = Atomics.load(i32, KP_VSYNC_SEQ);
    return Promise.resolve();
  }
  return r.value.then(function () {
    Atomics.sub(i32, KP_VSYNC_ARMED, 1);
    self._vsyncSeen = Atomics.load(i32, KP_VSYNC_SEQ);
  });
};

/* Synchronous vsync park (#500, SDL_SetRenderVSync): block the PROCESS
 * WORKER until KP_VSYNC_SEQ reaches `target` (signed-diff compare, wrap
 * safe), returning the seq actually observed. This is vsyncWait's blocking
 * twin for the software renderer's paced present — a worker may legally
 * block, and the tick notify wakes Atomics.wait directly (no event loop).
 * Discipline per park chunk, in order:
 *   - STOP: park UNARMED in _stopWait instead — a SIGSTOPped vsync app must
 *     release the compositor (KP_VSYNC_ARMED 0 -> compKeepAlive lets the rAF
 *     park; docs/archive/0169), not pin it awake. Resumes the vsync park on SIGCONT.
 *   - deliverable pending signal: give up the park (return the current seq)
 *     so the import returns and dispatch runs at the safe point — FS_WAIT's
 *     EINTR shape. The caller re-baselines on the returned seq, so an early
 *     return never accumulates tick debt.
 *   - ARMED count + parked-flag doorbell around the wait: the same Dekker
 *     pair vsyncWait implements — without it a paced app strands against the
 *     on-demand parked compositor. 1s chunks bound stop/signal latency while
 *     ticks are stopped (hidden tab = parked here, the honest pause). */
KernelClient.prototype.vsyncWaitUntil = function (target) {
  var i32 = this._i32;
  for (;;) {
    var cur = Atomics.load(i32, KP_VSYNC_SEQ);
    if (((cur - target) | 0) >= 0) return cur;
    if (Atomics.load(i32, KP_FLAGS) & KF_STOP) { this._stopWait(); continue; }
    if (this.pending()) return cur;
    Atomics.add(i32, KP_VSYNC_ARMED, 1);
    if (Atomics.load(i32, KP_COMP_PARKED) === 1) this._post({ type: 'want-frame' });
    Atomics.wait(i32, KP_VSYNC_SEQ, cur, 1000);
    Atomics.sub(i32, KP_VSYNC_ARMED, 1);
  }
};

/* Job control (docs/archive/0003): park while the kernel asserts STOP, until
 * SIGCONT clears the flag (SIGKILL terminates the worker outright). Runs at
 * the two safe-point families a process is guaranteed to hit: entry to
 * every kernel RPC (_finish — i.e. every brokered syscall) and sigpoll
 * (host.js's env-import return probe, when __sig_dispatch is exported). */
KernelClient.prototype.forkWait = function () {
  var i32 = this._i32;
  var request;
  while ((request=Atomics.load(i32, KP_FORK_REQ))) {
    Atomics.store(i32, KP_FORK_ACK, request);
    Atomics.wait(i32, KP_FORK_REQ, request);
  }
  Atomics.store(i32, KP_FORK_ACK, 0);
};
KernelClient.prototype._stopWait = function () {
  this.forkWait();
  var i32 = this._i32;
  while (Atomics.load(i32, KP_FLAGS) & KF_STOP) {
    var seq = Atomics.load(i32, KP_DOORBELL);
    if (!(Atomics.load(i32, KP_FLAGS) & KF_STOP)) break;
    Atomics.wait(i32, KP_DOORBELL, seq);
  }
};

/* Atomically claim all deliverable pending signals; returns the claimed
 * mask (0 if none). Blocked bits stay pending until sigmask() unblocks. */
KernelClient.prototype.sigpoll = function () {
  this._stopWait();
  var i32 = this._i32;
  for (;;) {
    var p = Atomics.load(i32, KP_SIGPEND);
    var take = p & ~Atomics.load(i32, KP_SIGBLOCK);
    if (!take) return 0;
    if (Atomics.compareExchange(i32, KP_SIGPEND, p, p & ~take) === p) return take;
  }
};

/* Publish the libc's blocked mask so the kernel honors it for default
 * actions and so sigpoll leaves blocked bits pending. */
KernelClient.prototype.sigmask = function (mask) {
  Atomics.store(this._i32, KP_SIGBLOCK, mask | 0);
};

/* ---- vDSO block (docs/archive/0179): seqlock reader ---- */

/* Read the kernel-page words `indices` as ONE consistent snapshot: retry
 * while the seq word is odd (write in progress) or moved across the reads.
 * Returns the values array, or null when the page is unpublished (seq 0 —
 * a KernelClient over a page no kernel ever stamped) or after a bounded
 * spin — callers fall back to the RPC, which stays the source of truth. */
KernelClient.prototype._vdsoRead = function (indices) {
  var i32 = this._i32;
  for (var tries = 0; tries < 64; tries++) {
    var s1 = Atomics.load(i32, KP_VD_SEQ);
    if (s1 === 0) return null;
    if (s1 & 1) continue;
    var out = [];
    for (var k = 0; k < indices.length; k++) out.push(Atomics.load(i32, indices[k]));
    if (Atomics.load(i32, KP_VD_SEQ) === s1) return out;
  }
  return null;
};

/* Live parent pid — tracks orphan reparenting to init, unlike the static
 * ppid minted at spawn. null on an unpublished page (caller keeps its
 * static). */
KernelClient.prototype.getppid = function () {
  var r = this._vdsoRead([KP_VD_PPID]);
  return r ? r[0] : null;
};

/* Milliseconds since the kernel booted, computed against the published
 * boot instant — no RPC. null on an unpublished page. */
KernelClient.prototype.uptimeMs = function () {
  var r = this._vdsoRead([KP_VD_BOOT_LO, KP_VD_BOOT_HI]);
  if (!r) return null;
  return Date.now() - ((r[1] >>> 0) * 4294967296 + (r[0] >>> 0));
};

/* The real screen dims (ctor default, then setScreen) — the virtual display geometry
 * via EV_SCREEN, readable by any process. null on an unpublished page. */
KernelClient.prototype.screen = function () {
  var r = this._vdsoRead([KP_VD_SCREEN_W, KP_VD_SCREEN_H]);
  return r ? { w: r[0], h: r[1] } : null;
};

/* Park on the doorbell until a signal is deliverable ('signal') or the
 * timeout elapses ('timeout'). ms undefined/null → wait forever.
 *
 * Timed parks compensate the OS timed-wait leeway (#492): Atomics.wait
 * timeouts wake late by a timer-coalescing leeway — measured on macOS as a
 * deterministic min(request/2, 10ms), which made usleep(16000) really sleep
 * 24ms. Requesting 2/3 of the remainder against a monotonic deadline makes a
 * full-leeway wake land ON the deadline while an on-time wake shrinks the
 * remainder by 1/3 (log-bounded wakes, ~1.2 per park measured); the final
 * sub-ms request bounds residual overshoot at ~0.5ms. 'timed-out' from a
 * shortened wait no longer means the deadline passed — only the monotonic
 * re-check decides. The thread stays parked throughout; never a spin. */
KernelClient.prototype.park = function (ms) {
  var i32 = this._i32;
  var deadline = (ms === undefined || ms === null) ? null : performance.now() + ms;
  for (;;) {
    var seq = Atomics.load(i32, KP_DOORBELL);
    if (this.pending()) return 'signal';
    if (deadline === null) {
      Atomics.wait(i32, KP_DOORBELL, seq);
    } else {
      var left = deadline - performance.now();
      if (left <= 0) return 'timeout';
      Atomics.wait(i32, KP_DOORBELL, seq, left > 1 ? left * (2 / 3) : left);
    }
  }
};

/* Adapter to host.js's spawnHooks seam (createSpawn's contract). The Phase 2
 * members (sigpoll/sigmask/park/exit) light up host.js's safe-point signal
 * delivery, interruptible sleeps, and the ordered exit handshake. */
KernelClient.prototype.spawnHooks = function () {
  var self = this;
  return {
    spawn: function (spec) { return self.call(OP.SPAWN, spec); },
    wait: function (pid, options) { return self.call(OP.WAIT, { pid: pid, options: options }, true); },
    kill: function (pid, sig) { return self.call(OP.KILL, { pid: pid, sig: sig }); },
    setpgid: function (pid, pgid) { return self.call(OP.SETPGID, { pid: pid, pgid: pgid }); },
    setsid: function () { return self.call(OP.SETSID, {}); },
    // vDSO fast path (docs/archive/0179): the caller's OWN pgid/sid are published
    // on the kernel page, so getpgrp()/getpgid(0)/getsid(0) make zero RPCs.
    // Foreign pids — and a failed seqlock read — still ask the kernel, the
    // source of truth. The self-pid check rides the same snapshot as the
    // value, so a mid-read setpgid can't pair one field with the other's
    // past.
    getpgid: function (pid) {
      var v = self._vdsoRead([KP_VD_PID, KP_VD_PGID]);
      if (v && ((pid | 0) === 0 || (pid | 0) === v[0])) return { pgid: v[1] };
      return self.call(OP.GETPGID, { pid: pid });
    },
    getsid: function (pid) {
      var v = self._vdsoRead([KP_VD_PID, KP_VD_SID]);
      if (v && ((pid | 0) === 0 || (pid | 0) === v[0])) return { sid: v[1] };
      return self.call(OP.GETSID, { pid: pid });
    },
    // Interval timers (docs/archive/0044): ms over the wire; the libc converts
    // timeval <-> ms and owns the sub-ms round-up.
    setitimer: function (which, valueMs, intervalMs) {
      return self.call(OP.SETITIMER, { which: which, valueMs: valueMs, intervalMs: intervalMs });
    },
    getitimer: function (which) { return self.call(OP.GETITIMER, { which: which }); },
    sigdisp: function (sig, kind) { self.call(OP.SIGDISP, { sig: sig, kind: kind }); },
    compile: function (argv, cwd, chunked) { return self.call(OP.COMPILE, { argv: argv, cwd: cwd, chunked: !!chunked }); },
    // System clipboard (docs/archive/0090): one kernel slot; host.js owns the
    // chunking (payloads pre-framed per the OP table's RAW layouts).
    // payloadChunk is the max chunk for those staging lanes (clipboard +
    // http body) — derived kernel-side from KP_PAYLOAD_CAP (docs/archive/0235)
    // so host.js never restates the kernel-page layout.
    payloadChunk: KP_HOOK_CHUNK,
    clipSet: function (bytes) { return self.callRaw(OP.CLIP_SET, bytes); },
    // peek 1 = availability probe (__clip_has): always served from the
    // cached slot — a data read (peek 0, off 0) may park on the embedder's
    // host-clipboard refresh (the clipboard seam; see OP.CLIP_GET).
    clipGet: function (fmt, off, peek) {
      return self.call(OP.CLIP_GET, { fmt: fmt, off: off, peek: peek ? 1 : 0 });
    },
    // Gamepad name query (#607) — backs host.js __sdl_gamepad_name.
    padName: function (id) { return self.call(OP.PAD_NAME, { id: id | 0 }); },
    // Egress (docs/archive/0398): the pre-framed textual disposition+path list
    // (host.js createEgress composes it; layout in the OP table). One RPC,
    // one artifact; the kernel materializes and hands it to the embedder.
    egress: function (bytes) { return self.callRaw(OP.EGRESS, bytes); },
    // HTTP transport (docs/archive/0172; fd-shaped since docs/archive/0417): host.js's
    // createHttp drives these; the libcurl veneer (0173) sits on top.
    // httpBody stages request-body chunks (RAW [u32 off][bytes]); httpOpen
    // returns {fd} — body drain and close ride the ordinary fs hooks
    // (FS_READ/FS_CLOSE); httpStatus is non-blocking (EAGAIN before
    // headers), so neither op parks and neither needs the interruptible
    // flag.
    httpBody: function (bytes) { return self.callRaw(OP.HTTP_BODY, bytes); },
    httpOpen: function (spec) { return self.call(OP.HTTP_OPEN, spec); },
    httpStatus: function (fd) { return self.call(OP.HTTP_STATUS, { fd: fd }); },
    httpError: function (fd) { return self.call(OP.HTTP_ERROR, { fd: fd }); },
    forkWait: function () { self.forkWait(); },
    sigpoll: function () { return self.sigpoll(); },
    sigmask: function (mask) { self.sigmask(mask); },
    park: function (ms) { return self.park(ms); },
    // Unified wait (docs/archive/0178): { r: [fds], ring: 0/1, timeoutMs } →
    // { why: 0 timeout / 1 fd / 2 ring } or { errno: 'EINTR' } on a posted
    // signal (interruptible like FS_SELECT). host.js's __wait import and
    // the surface backend own the ring drain around it.
    waitMulti: function (req) { return self.call(OP.FS_WAIT, req, true); },
    exit: function (status) { return self.call(OP.EXIT, { code: status }); },
    // Phase 3 tty control plane (line discipline lives kernel-side). The fd
    // rides along since 0020 (ptys): the kernel resolves it through the fd
    // table to THE tty it names (slave pty vs system tty), falling back to
    // the process's attached tty for old callers / ring mode.
    ttyGetattr: function (fd) { return self.call(OP.TCGETATTR, { fd: fd }); },
    ttySetattr: function (fd, actions, t) {
      return self.call(OP.TCSETATTR, {
        fd: fd, actions: actions, iflag: t.iflag, oflag: t.oflag,
        cflag: t.cflag, lflag: t.lflag, cc: t.cc,
      });
    },
    ttyGetpgrp: function (fd) { return self.call(OP.TCGETPGRP, { fd: fd }); },
    ttySetpgrp: function (fd, pgid) { return self.call(OP.TCSETPGRP, { fd: fd, pgid: pgid }); },
    screen: function () { return self.screen(); },
    // Audio mixer (docs/archive/0017): the process-allocated source ring rides the
    // FIFO channel immediately before the RPC that names it (wm-sabs shape).
    audioOpen: function (freq, format, channels, sab, offset, capacity) {
      self._post({ type: 'audio-sab', sab: sab });
      return self.call(OP.AUDIO_OPEN, { freq: freq, format: format, channels: channels,
        offset: offset, capacity: capacity });
    },
    audioClose: function (aid, discard) { return self.call(OP.AUDIO_CLOSE, { aid: aid, discard: !!discard }); },
    audioClear: function (aid) { return self.call(OP.AUDIO_CLEAR, { aid: aid }); },
    // Master gain (docs/archive/0048): percent 0..200, gain < 0 queries.
    audioGain: function (gain) { return self.call(OP.AUDIO_GAIN, { gain: gain }); },
  };
};

/* ============================================================
 * Tty — the terminal as a kernel object (docs/KERNEL.md Phase 3).
 *
 * The tty SAB is the same ring format host.js's BlockFS stdin path already
 * consumes (SI_* header, 32 bytes, ring after) — the kernel is simply the
 * producer where the page used to be, and the LINE DISCIPLINE moves here:
 * canonical-mode editing (erase/kill/EOF), echo, ICRNL, and ISIG control
 * chars routed as signals to the FOREGROUND PROCESS GROUP (VINTR -> SIGINT:
 * Ctrl-C finally means something). A signal char also flushes the INPUT
 * queue — edit buffer AND queued cooked type-ahead — unless NOFLSH is set
 * (#433, POSIX 11.1.9); the OUTPUT-queue half of that flush (pty
 * slave->master buffer) is a recorded descope, no observed symptom.
 * The UI bridge stays dumb — raw bytes in
 * via tty.input(), echo/output bytes out via the output callback; a
 * scripted bridge (tests, agents) and xterm.js are two consumers of the
 * same byte protocol (OS.md "agent-friendly by construction").
 *
 * v1 scope (documented limits): ONE tty per system, attached to every
 * process; single-ACTIVE-reader assumption (the ring consume path is not
 * multi-consumer-atomic — real shells park in waitpid while the fg child
 * reads, so this holds in practice; brokered mode formalizes it with
 * SIGTTIN for background readers, see the FS_READ dispatch);
 * VEOF on an empty line is sticky EOF, not transient.
 * ============================================================ */

/* SI_* header layout — MUST match host.js (BlockFS setStdinSab). */
var SI_SEQ = 0, SI_AVAIL = 1, SI_WRITEPOS = 2, SI_READPOS = 3,
    SI_EOF = 4, SI_COLS = 5, SI_ROWS = 6, SI_TERMIOS = 7;
var SI_HDR_BYTES = 32;

/* termios bits the line discipline consults — MUST match <termios.h>. */
var T_ICRNL = 0x100, T_INLCR = 0x40, T_IGNCR = 0x80,
    T_IUTF8 = 0x4000;                                          // c_iflag
var T_OPOST = 0x1, T_ONLCR = 0x2;                              // c_oflag
var T_ECHOE = 0x2, T_ECHOK = 0x4, T_ECHO = 0x8, T_ECHONL = 0x10,
    T_ISIG = 0x80, T_ICANON = 0x100, T_NOFLSH = 0x80000000;    // c_lflag
var V_EOF = 0, V_ERASE = 3, V_KILL = 5, V_INTR = 8, V_QUIT = 9,
    V_SUSP = 10, V_START = 12, V_STOP = 13, V_MIN = 16, V_TIME = 17;
var NCCS = 20;
var TCSAFLUSH = 2;

function defaultCc() {
  var cc = new Array(NCCS).fill(0);
  cc[V_EOF] = 4;      // ^D
  cc[V_ERASE] = 127;  // DEL
  cc[V_KILL] = 21;    // ^U
  cc[V_INTR] = 3;     // ^C
  cc[V_QUIT] = 28;    // ^\
  cc[V_SUSP] = 26;    // ^Z
  cc[V_START] = 17;   // ^Q
  cc[V_STOP] = 19;    // ^S
  cc[V_MIN] = 1;
  return cc;
}

function Tty(kernel, opts) {
  this._kernel = kernel;
  this._output = opts.output || function () {};
  var ringSize = opts.ringSize || 64 * 1024;
  this.sab = new SharedArrayBuffer(SI_HDR_BYTES + ringSize);
  this._i32 = new Int32Array(this.sab, 0, 8);
  this._ring = new Uint8Array(this.sab, SI_HDR_BYTES, ringSize);
  Atomics.store(this._i32, SI_COLS, opts.cols || 80);
  Atomics.store(this._i32, SI_ROWS, opts.rows || 24);
  this.fgPgid = 0;              // set at boot; tcsetpgrp moves it
  this._line = [];              // canonical-mode edit buffer
  // Brokered mode (the fd/data-plane amendment): cooked bytes queue here
  // and the kernel serves deferred FS_READ RPCs from it — the SAB ring is
  // unused for data (the header still carries winsize for TIOCGWINSZ).
  // Ring mode (standalone pages) keeps the Phase-3 behavior.
  this._brokered = false;
  this._cooked = [];
  this._eofFlag = false;        // transient VEOF (^D): consumed by ONE read
  this._hupFlag = false;        // hangup (pty master gone): latched EOF forever
  this.waiters = [];            // pids with a deferred FS_READ on THIS tty, FIFO
                                // (per-instance since 0020: ptys mean many ttys)
  // Bridge-declared: a human terminal is attached, so std fd 1/2 should be
  // tty-kind (isatty true -> shells prompt). See Kernel._stdOfds.
  this.interactiveOut = !!opts.interactiveOut;
  this.termios = {
    iflag: T_ICRNL | T_IUTF8,
    oflag: T_OPOST | T_ONLCR,
    cflag: 0xB00,               // CS8|CREAD (matches the legacy canned value)
    lflag: T_ISIG | T_ICANON | T_ECHO | T_ECHOE | T_ECHOK,
    cc: defaultCc(),
  };
  this._publishModeWord();
}

/* Legacy 3-bit mode word (icanon/echo/opost) — kept current so pre-kernel
 * page observers keep working; nothing kernel-side reads it back. */
Tty.prototype._publishModeWord = function () {
  var t = this.termios;
  var mode = ((t.lflag & T_ICANON) ? 1 : 0) | ((t.lflag & T_ECHO) ? 2 : 0)
    | ((t.oflag & T_OPOST) ? 4 : 0);
  Atomics.store(this._i32, SI_TERMIOS, mode);
};

/* Wake anything parked on the ring futex (readers re-scan; also rung by the
 * kernel when it posts a signal, so a blocked read can turn into EINTR). */
Tty.prototype.wakeReaders = function () {
  Atomics.add(this._i32, SI_SEQ, 1);
  Atomics.notify(this._i32, SI_SEQ);
};

/* Commit cooked bytes: brokered mode queues them for deferred FS_READ
 * RPCs; ring mode writes the shared ring. Overflow drops (like a real tty
 * input queue) — loudly, via the kernel log. */
Tty.prototype._push = function (bytes) {
  if (this._brokered) {
    for (var bi = 0; bi < bytes.length; bi++) this._cooked.push(bytes[bi]);
    this._kernel._ttyNotify(this);
    return;
  }
  var i32 = this._i32, ring = this._ring, size = ring.length;
  var free = size - Atomics.load(i32, SI_AVAIL);
  var n = Math.min(bytes.length, free);
  if (n < bytes.length) this._kernel._log('tty: input ring overflow, dropping ' + (bytes.length - n) + ' bytes');
  var wp = Atomics.load(i32, SI_WRITEPOS);
  for (var k = 0; k < n; k++) ring[(wp + k) % size] = bytes[k];
  Atomics.store(i32, SI_WRITEPOS, (wp + n) % size);
  Atomics.add(i32, SI_AVAIL, n);
  this.wakeReaders();
};

Tty.prototype._echo = function (bytes) {
  if (bytes.length) this._output(Uint8Array.from(bytes));
};

/* Terminal display width of a code point: 2 for East-Asian Wide/Fullwidth
 * blocks, 1 for everything else INCLUDING combining marks (the D5 ruling:
 * a combining mark renders as its own spacing cell, so its erase is one
 * cell). MUST MATCH os/wcwidth.h `wcwidth_cp` (the terminal's cell-layout
 * twin) — change both together or a wide char erases the wrong number of
 * cells. Condensed Unicode 15.0 EAW W/F table. */
var WCWIDTH_WIDE = [
  [0x1100, 0x115F], [0x2329, 0x232A], [0x2E80, 0x303E], [0x3041, 0x33FF],
  [0x3400, 0x4DBF], [0x4E00, 0x9FFF], [0xA000, 0xA4CF], [0xA960, 0xA97F],
  [0xAC00, 0xD7A3], [0xF900, 0xFAFF], [0xFE10, 0xFE19], [0xFE30, 0xFE6B],
  [0xFF00, 0xFF60], [0xFFE0, 0xFFE6], [0x16FE0, 0x16FE4], [0x17000, 0x187F7],
  [0x18800, 0x18CD5], [0x1B000, 0x1B2FF], [0x1F300, 0x1F64F],
  [0x1F680, 0x1F6FF], [0x1F900, 0x1F9FF], [0x1FA70, 0x1FAFF],
  [0x20000, 0x2FFFD], [0x30000, 0x3FFFD],
];
function wcwidthCp(cp) {
  if (cp < 0x1100) return 1;
  for (var i = 0; i < WCWIDTH_WIDE.length; i++)
    if (cp >= WCWIDTH_WIDE[i][0] && cp <= WCWIDTH_WIDE[i][1]) return 2;
  return 1;
}

/* Remove one CHARACTER from the tail of the canonical edit buffer. With
 * IUTF8 set (Linux semantics; gucOS default) trailing UTF-8 continuation
 * bytes fall with their lead byte, so one ERASE deletes a whole multi-byte
 * character; with it clear the pop stays byte-wise (the historical mode).
 * Returns the DISPLAY width of what fell (1 or 2 cells): the caller echoes
 * one [BS SP BS] triple per cell, so a double-width CJK char visually
 * erases both of its cells (Unicode Phase D; both term.c and xterm.js
 * render such chars across 2 cells). Byte-wise mode always reports 1. */
Tty.prototype._popChar = function (t) {
  if (!(t.iflag & T_IUTF8)) { this._line.pop(); return 1; }
  var tail = [];
  while (this._line.length > 1 && (this._line[this._line.length - 1] & 0xC0) === 0x80)
    tail.unshift(this._line.pop());
  var lead = this._line.pop();
  // Decode the popped bytes into a code point (the sequence was intact
  // when typed; anything malformed just reports width 1).
  var cp;
  if (lead < 0x80 && tail.length === 0) cp = lead;
  else if ((lead & 0xE0) === 0xC0 && tail.length === 1) cp = lead & 0x1F;
  else if ((lead & 0xF0) === 0xE0 && tail.length === 2) cp = lead & 0x0F;
  else if ((lead & 0xF8) === 0xF0 && tail.length === 3) cp = lead & 0x07;
  else return 1;
  for (var i = 0; i < tail.length; i++) cp = (cp << 6) | (tail[i] & 0x3F);
  return wcwidthCp(cp);
};

Tty.prototype._echoNl = function () {
  var t = this.termios;
  this._echo((t.oflag & T_OPOST) && (t.oflag & T_ONLCR) ? [13, 10] : [10]);
};

Tty.prototype._signalFg = function (sig) {
  // Route by pgid directly — NOT via kill(-pgid): a foreground pgid of 1
  // would encode as kill(-1), which POSIX reserves for "every process".
  if (this.fgPgid > 0) this._kernel._killPgid(this.fgPgid, sig);
};

/* Discard queued COOKED input (the tcflush(TCIFLUSH) core, shared by the
 * ISIG signal chars and TCSAFLUSH — #433). Brokered mode is trivially safe:
 * _cooked lives on the kernel thread and a parked FS_READ waiter holds no
 * snapshot of it. Ring mode: a reader PARKED on the SI_SEQ futex is safe
 * too — it re-loads SI_AVAIL fresh after every wake, so zeroing AVAIL
 * FIRST means any reader arriving mid-flush sees "empty" and parks rather
 * than reading a torn index pair. The one exposed window is a reader
 * already inside its avail>0 consume (loaded AVAIL, not yet subtracted) at
 * the flush instant: its unconditional Atomics.sub can leave AVAIL negative
 * — the same few-instruction race TCSAFLUSH has accepted since Phase 3
 * ("flush during concurrent reads is undefined", POSIX agrees), not
 * widened here, and unreachable in the OS proper (brokered). */
Tty.prototype._flushInput = function () {
  if (this._brokered) { this._cooked.length = 0; return; }
  var i32 = this._i32;
  Atomics.store(i32, SI_AVAIL, 0);
  Atomics.store(i32, SI_READPOS, Atomics.load(i32, SI_WRITEPOS));
};

/* Raw bytes from the UI bridge (keystrokes). String or Uint8Array. */
Tty.prototype.input = function (data) {
  var bytes = typeof data === 'string' ? textEncoder.encode(data) : data;
  var t = this.termios;
  var raw = [];
  for (var i = 0; i < bytes.length; i++) {
    var b = bytes[i];
    if (b === 13) {
      if (t.iflag & T_IGNCR) continue;
      if (t.iflag & T_ICRNL) b = 10;
    } else if (b === 10 && (t.iflag & T_INLCR)) {
      b = 13;
    }
    if (t.lflag & T_ISIG) {
      var sig = b === t.cc[V_INTR] ? SIG.INT
        : b === t.cc[V_QUIT] ? SIG.QUIT
        : b === t.cc[V_SUSP] ? SIG.TSTP : 0;
      if (sig) {
        if (t.lflag & T_NOFLSH) {
          // NOFLSH (POSIX): a signal char flushes nothing — pending raw
          // bytes commit, the edit buffer and cooked queue survive.
          if (raw.length) { this._push(raw); raw = []; }
        } else {
          // POSIX: INTR/QUIT/SUSP flush the input queue — the edit buffer,
          // raw bytes accumulated this burst, AND completed type-ahead
          // already sitting in the cooked queue (#433: without the queue
          // flush, a line typed + Entered during a running command replays
          // into the next read after ^C). Output-queue flush is a recorded
          // descope (pty slave->master buffer; no observed symptom).
          raw = [];
          this._line.length = 0;
          this._flushInput();
        }
        if (t.lflag & T_ECHO) { this._echo([94, 64 + (b & 31)]); this._echoNl(); } // ^C style
        this._signalFg(sig);
        continue;
      }
    }
    if (t.lflag & T_ICANON) {
      if (b === t.cc[V_ERASE]) {
        if (this._line.length) {
          var ew = this._popChar(t);   // display cells (2 for CJK-wide)
          if ((t.lflag & T_ECHO) && (t.lflag & T_ECHOE))
            while (ew-- > 0) this._echo([8, 32, 8]);
        }
        continue;
      }
      if (b === t.cc[V_KILL]) {
        if ((t.lflag & T_ECHO) && (t.lflag & T_ECHOK)) {
          while (this._line.length) {
            var kw = this._popChar(t);
            while (kw-- > 0) this._echo([8, 32, 8]);
          }
        } else {
          this._line.length = 0;
        }
        continue;
      }
      if (b === t.cc[V_EOF]) {
        if (this._line.length) { this._push(this._line); this._line = []; }
        else this.eof();                             // transient one-shot EOF (0163)
        continue;
      }
      if (b === 10) {
        this._line.push(10);
        if (t.lflag & (T_ECHO | T_ECHONL)) this._echoNl();
        this._push(this._line);
        this._line = [];
        continue;
      }
      this._line.push(b);
      if (t.lflag & T_ECHO) this._echo([b]);
    } else {
      raw.push(b);
      if (t.lflag & T_ECHO) this._echo([b]);
    }
  }
  if (raw.length) this._push(raw);
};

/* The bridge reports a resize; the fg pgroup learns via SIGWINCH. */
Tty.prototype.resize = function (cols, rows) {
  var changed = Atomics.load(this._i32, SI_COLS) !== cols ||
    Atomics.load(this._i32, SI_ROWS) !== rows;
  Atomics.store(this._i32, SI_COLS, cols);
  Atomics.store(this._i32, SI_ROWS, rows);
  if (changed) this._signalFg(SIG.WINCH);
};

/* End of input. Two distinct conditions POSIX termios keeps separate (0163):
 *   - VEOF (^D on an empty line): TRANSIENT. It makes the CURRENT read return
 *     0, then it's gone — the next read blocks for fresh input. So a REPL
 *     exiting on ^D must not cascade EOF into the shell reading the same tty.
 *   - hangup (pty master close / agent closed stdin): PERMANENT. Reads latch
 *     at EOF forever (the session is over; SIGHUP fired separately).
 * Pass permanent=true for the hangup path; VEOF omits it. */
Tty.prototype.eof = function (permanent) {
  if (permanent) this._hupFlag = true; else this._eofFlag = true;
  Atomics.store(this._i32, SI_EOF, 1);
  if (this._brokered) { this._kernel._ttyNotify(this); return; }
  this.wakeReaders();
};

/* Consume the transient VEOF as a read delivers its 0-byte EOF — the hangup
 * flag stays latched. Called by the brokered read-service sites right before
 * they respond with an empty buffer. */
Tty.prototype._consumeEof = function () {
  if (this._hupFlag) return;
  this._eofFlag = false;
  Atomics.store(this._i32, SI_EOF, 0);
};

/* Brokered readiness (select) and read service. */
Tty.prototype.readable = function () {
  return this._cooked.length > 0 || this._eofFlag || this._hupFlag;
};
Tty.prototype.take = function (count) {
  var n = Math.min(count, this._cooked.length);
  return Uint8Array.from(this._cooked.splice(0, n));
};

Tty.prototype.getattr = function () {
  var t = this.termios;
  return { iflag: t.iflag, oflag: t.oflag, cflag: t.cflag, lflag: t.lflag, cc: t.cc.slice() };
};

Tty.prototype.setattr = function (actions, t) {
  var wasCanon = (this.termios.lflag & T_ICANON) !== 0;
  this.termios.iflag = t.iflag >>> 0;
  this.termios.oflag = t.oflag >>> 0;
  this.termios.cflag = t.cflag >>> 0;
  this.termios.lflag = t.lflag >>> 0;
  if (Array.isArray(t.cc)) {
    for (var i = 0; i < NCCS; i++) this.termios.cc[i] = (t.cc[i] | 0) & 0xff;
  }
  if (actions === TCSAFLUSH) {
    // Discard unread input (concurrency rules: _flushInput's comment).
    this._line = [];
    this._flushInput();
  } else if (wasCanon && !(this.termios.lflag & T_ICANON) && this._line.length) {
    // Leaving canonical mode mid-line (Linux n_tty semantics, the 0171 wedge
    // class): the un-terminated edit buffer becomes readable NOW. Stranding
    // it splits a typed line straddling a shell's cooked window -> its line
    // editor's raw switch — the head is lost and the surviving tail executes
    // (or opens an unbalanced quote and wedges the shell in PS2 forever).
    var part = this._line;
    this._line = [];
    this._push(part);
  }
  this._publishModeWord();
  this.wakeReaders();
};

/* ============================================================
 * Kernel — the process table and RPC dispatcher (owner side).
 *
 * Injected capabilities (all optional except createWorker):
 *   createWorker(procSpec) -> handle   REQUIRED. Creates the process worker.
 *       procSpec: { pid, ppid, pgid, path, argv, envp, cwd, actions, flags,
 *                   image (ArrayBuffer|null), module (WebAssembly.Module|null),
 *                   kernelPage (SAB) }
 *       Exactly one of image/module is non-null: module is the compiled-
 *       Module cache hit (docs/archive/0037 — structured-clone it to the worker);
 *       image is the raw-bytes path for everything uncacheable.
 *       handle:   { postMessage(msg), onMessage(fn), onExit(fn), terminate() }
 *   loadImage(path) -> bytes | Promise<bytes> | null   resolve a spawn path
 *       to a wasm image (null -> ENOENT). The reference OS page reads its
 *       BlockFS; tests inject a map.
 *   compile(argv, cwd, {signal, pid}) -> result | Promise<result>   backs
 *       /bin/cc's __compile. Result is {exitCode, stdout, stderr} or {errno}.
 *       The signal aborts on interruption or exit. Absent -> ENOSYS.
 *   onOutput(pid, fd, bytes)   a process wrote to fd 1/2 (bytes: Uint8Array).
 *   onHalt(status)             pid 1 exited; `status` is its wait-status.
 *   log(msg)                   kernel diagnostics (default: silent).
 *
 * Worker messages the kernel understands (the bootstrap's side of the
 * contract): {type:'krpc'} — an RPC request is in the kernel page;
 * {type:'out', fd, bytes} — stdout/stderr traffic; {type:'exited', code} —
 * runModule resolved; {type:'crashed', error} — runModule rejected.
 * ============================================================ */
var STATE_RUNNING = 'running', STATE_STOPPED = 'stopped', STATE_ZOMBIE = 'zombie';

// #785: owner-issued mounted identity, not a caller/worker allocation.
function checkedVolumeDev(dev) {
  if (!Number.isInteger(dev)) throw new TypeError('filesystem identity: dev must be an integer');
  if (dev < 1 || dev > 0xffffffff) throw new RangeError('filesystem identity: dev outside uint32 mounted range');
  return dev;
}
function checkedRoPrefix(prefix) {
  if (typeof prefix !== 'string' || prefix[0] !== '/' || prefix.indexOf('\0') !== -1 || roNormalize(prefix) !== prefix)
    throw new TypeError('filesystem identity: RO prefix must be canonical and absolute');
  return prefix;
}
function ownerRoImage(fs, image) {
  if (image == null) return null;
  if (typeof image !== 'object' || !(image.sab instanceof SharedArrayBuffer))
    throw new TypeError('filesystem identity: RO image requires a SharedArrayBuffer');
  var prefix = checkedRoPrefix(image.prefix);
  var supplied = Object.prototype.hasOwnProperty.call(image, 'dev');
  if (supplied) checkedVolumeDev(image.dev);
  // Capability absence disables the WHOLE local read path. No guessed dev.
  if (!fs || typeof fs.getVolumeIdentity !== 'function') {
    if (supplied) throw new TypeError('filesystem identity: no owner for supplied RO dev');
    return null;
  }
  var identity = fs.getVolumeIdentity(prefix);
  if (!identity || !Object.prototype.hasOwnProperty.call(identity, 'dev')) {
    if (supplied) throw new TypeError('filesystem identity: no owner for supplied RO dev');
    return null;
  }
  var dev = checkedVolumeDev(identity.dev);
  if (identity.readonly !== true) throw new TypeError('filesystem identity: RO mount must be readonly');
  if (supplied && image.dev !== dev) throw new TypeError('filesystem identity: contradictory RO dev');
  if (Object.prototype.hasOwnProperty.call(identity, 'leaf') && typeof identity.leaf !== 'boolean')
    throw new TypeError('filesystem identity: owner leaf coverage must be boolean');
  // Missing topology authority or any nested mount disables ALL local reads,
  // including symlink targets and local ENOENT answers beneath the prefix.
  if (identity.leaf !== true) return null;
  return Object.freeze({ prefix: prefix, sab: image.sab, dev: dev, leaf: true });
}

function Kernel(opts) {
  if (!opts || typeof opts.createWorker !== 'function') {
    throw new Error('Kernel: a createWorker capability is required');
  }
  this._createWorker = opts.createWorker;
  this._loadImage = opts.loadImage || function () { return null; };
  this._compile = opts.compile || null;
  this._onOutput = opts.onOutput || function () {};
  this._onHalt = opts.onHalt || function () {};
  this._keepAlive = !!opts.keepAlive;
  this._onProcessExit = opts.onProcessExit || function () {};
  // Audio pump gate (docs/IDLE-POWER.md "audioPump gate"): fired on every
  // AUDIO_OPEN so an embedder that parks its pump interval while the stream
  // table is empty can re-arm it. Disarm is the embedder's call — poll
  // audioStreamCount() after a pump (opens are the only RPC-visible
  // transition; pause/resume is SAB-only, so ANY table entry must keep the
  // pump running or an unpause could never be noticed).
  this._onAudioStream = opts.onAudioStream || function () {};
  // Host-clipboard bridge (ticket #79): fired at every process-side CLIP_SET
  // COMMIT with the new slot ({fmt, bytes}, null on clear) so an embedder can
  // mirror gucOS copies out to a host clipboard. Embedder-side writes
  // (Kernel.clipSet) deliberately do NOT fire it — a bidirectional bridge
  // would loop on its own echo.
  this._onClipboard = opts.onClipboard || null;
  // Deferred CLIP_GET (the clipboard seam): the INBOUND twin of onClipboard.
  // When present, a data read of the slot (CLIP_GET off 0, peek 0) PARKS the
  // calling process and fires this hook with (done, pid) — pid is the parked
  // consumer, so the embedder can scope any refresh-freshness cache per
  // consumer (#562: a pid-blind time window served one process's refresh to
  // the NEXT process's first paste); the embedder
  // refreshes the slot from the host clipboard (the browser page runs
  // navigator.clipboard.readText inside the still-live activation of the
  // keystroke/tap that triggered the paste) and calls done(), and the parked
  // read is served from the possibly-just-updated slot. The consumer is
  // held, never the input stream — key order/repeat/modifier tracking are
  // untouched; the pasting app just blocks one host round-trip like any
  // blocking read. The embedder OWNS the always-done guarantee (kernel-
  // worker.js backstops with a timeout). No hook (boot.js, unit tests,
  // standalone embedders) = the synchronous pre-seam path, byte-identical.
  this._onClipRead = opts.onClipRead || null;
  // Egress (docs/archive/0398): the OUTBOUND file hop. Fired from the OP.EGRESS
  // dispatch with the finished artifact — (dispo, name, bytes): dispo the
  // disposition word ('download' | 'saveas'), name the sanitized artifact
  // name, bytes ONE Uint8Array (a file's bytes, or a store-only zip for a
  // directory/multi-selection). Exactly one artifact per call, always. The
  // browser embedder posts it to the page (transferred) where the still-live
  // activation of the initiating gesture performs the download / save-picker
  // act; boot.js --egress-dir writes it to a host directory (the headless
  // twin). No hook = ENOSYS to the caller — deliberately NO process-local
  // fallback (egress without a host is meaningless; the no-zombie rule).
  this._onEgress = opts.onEgress || null;
  this._log = opts.log || function () {};
  // Durable storage (docs/STORAGE.md): called before every RPC response so
  // the embedder can schedule a journal commit of whatever the request
  // changed. Absent for embedders without a persistent disk.
  this._commit = opts.commit || null;
  // Record locks and shared mappings (docs/MAPPINGS.md). The classes live in
  // file-locks.js and mapped-files.js; a kernel loaded without them (unit
  // tests of other subsystems) reports ENOSYS for those requests.
  this._locks = typeof FileLocks !== 'undefined' ? new FileLocks() : null;
  this._mapped = (opts.fs && typeof MappedFiles !== 'undefined') ? new MappedFiles(opts.fs) : null;
  this._procs = new Map();   // pid -> PCB
  this._nextPid = 1;
  this._halted = false;
  this._tty = null;
  // The brokered filesystem (KERNEL.md "fd/data-plane amendment"): with an
  // opts.fs BlockFS instance, the kernel owns per-process fd tables over a
  // system-wide open-file-description table, and processes reach the fs via
  // 0x04xx RPCs (host.js RemoteFS). Without opts.fs, processes keep their
  // own private in-process fs (the standalone/Phase-1 arrangement).
  this._fs = opts.fs || null;
  this._readCache = this._fs && this._fs.readCapability && opts.fileReadCache !== false ? new FileReadCache(this) : null;
  this._brokered = !!opts.fs;
  // Process-side read-only /usr (docs/archive/0180): the embedder ships the sealed
  // system image as ONE SharedArrayBuffer plus its mount prefix
  // (opts.roImage = { prefix: '/usr', sab }); every spawn forwards it and
  // the process worker mounts it locally (host.js SabByteStore + createV4
  // readonly), so reads under the prefix never cross the RPC boundary —
  // immutable data serves itself (KERNEL.md single-writer rule). Brokered
  // only: standalone processes already own a private in-process fs.
  this._roImage = ownerRoImage(this._fs, opts.roImage);
  // System clipboard (docs/archive/0090): { fmt, bytes: Uint8Array } or null.
  // Kernel-owned so it outlives the copying process; see OP.CLIP_SET.
  this._clipboard = null;
  this._vsync = false;
  this._compParked = false;
  // Boot instant — /proc/uptime's zero and the base for per-process
  // start_time (procfs, docs/archive/0043).
  this._bootMs = Date.now();
  // procfs (docs/archive/0043): a ProcFS volume in the mount table renders THIS
  // kernel's process table. Bound here so embedders just add
  // `'/proc': new ProcFS()` to their MountFS mounts — nothing to wire.
  if (this._fs && Array.isArray(this._fs._mounts)) {
    for (var mi = 0; mi < this._fs._mounts.length; mi++) {
      var mfs = this._fs._mounts[mi].fs;
      if (mfs instanceof ProcFS) mfs._kernel = this;
    }
  }
  // The compiled-Module cache (docs/archive/0037; generalized to writable volumes
  // by #188): spawn compiles each binary once and ships the
  // WebAssembly.Module in the spawn message (Modules structured-clone
  // across workers; Instances don't — and the engine's JIT state follows
  // the Module object, so one shared Module is also what makes warm spawns
  // warm). Keyed by the fs's moduleKey: an immutable `prefix:ino` on a
  // read-only volume (contents can't change for the mount's lifetime — no
  // invalidation to get wrong), a VALIDATED `prefix:ino:size:mtime` on a
  // writable one (every term read through the store per spawn, so a
  // rewritten binary derives a DIFFERENT key and a stale Module can never
  // be hit). One entry per spawned path: _modulePathKey remembers the key
  // a path last spawned under, and when the derivation moves the old entry
  // is deleted — recompiles REPLACE their entry instead of leaking a dead
  // Module per `cc -o`. Values are Promises (racing spawns of the same
  // binary share one compile); a Promise resolving null marks "uncacheable
  // after all" (compile error, Modules don't clone on this
  // tier).
  this._libraryBytes = new Map(); // versioned immutable bytes, LRU, at most 64 MiB
  this._libraryBytesSize = 0;
  this._libraryBytesHits = 0;
  this._moduleCache = new Map();   // moduleKey -> Promise<Module|null>
  this._modulePathKey = new Map(); // spawn path -> its last moduleKey
  this._moduleCacheHits = 0;
  this._moduleCacheMisses = 0;
  this._moduleCloneOk = undefined; // one-shot structuredClone(Module) probe
  this._ofds = new Map();    // ofdId -> { id, kind:'file'|'tty'|'out'|'null'|'pipe'|'socket'|'ptm', refs, bfsFd?, ch?, pipe?, end?, st?, rx?, tx?, path?, backlog?, pending?, acceptWaiters?, tty?, pty? }
                             // 'tty' with pty set = a pty SLAVE (0020);
                             // 'ptm' = a pty master.
  this._nextOfd = 1;
  this._ptys = new Map();
  this._nextPty = 0;
  this._std = null;          // lazy singleton OFDs for default stdio
  this._watches = new Map(); // FS_WATCH: ofdId -> 'watch' OFD. size === 0 is
                             // the whole subsystem's cost gate — an
                             // unwatched system pays nothing per mutation.
  this._sockBinds = new Map(); // resolved path -> listener/bound socket ofdId
  this._kernelSockServers = new Map(); // resolved path -> onConnect(peer, pcb)
                             // — KERNEL-owned AF_UNIX endpoints (sockServe;
                             // docs/archive/0014). Checked before _sockBinds.
  this._screen = {w:1024,h:768};
  this._padNames = new Map();
  // Audio mixer (docs/archive/0017; WM.md "Audio mixing"). Streams register via
  // AUDIO_OPEN; the pump mixes them into the one output ring (audioInit).
  this._audioStreams = new Map(); // aid -> stream (see _audioRpc AUDIO_OPEN)
  this._audioGain = 1;            // master output gain (docs/archive/0048, AUDIO_GAIN)
  this._nextAid = 1;
  this._audioOut = null;          // { sab, control, f32, cap, freq, channels }
  // HTTP transport (docs/archive/0172). Defaults to the embedder's fetch (browser
  // worker or Node ≥18 global); opts.fetch overrides (a fake fetch in tests).
  // Passing `fetch: null` EXPLICITLY disables network — HTTP_OPEN answers
  // ENOSYS (standalone pages stay offline); omitting it uses the global.
  // NB fetch must be BOUND: browsers brand-check fetch's receiver, so calling
  // it as this._fetch(...) with the Kernel as `this` throws Illegal invocation
  // before any request (Node's undici doesn't check — the Node suite can't
  // catch this; the browser HTTP e2e does).
  this._fetch = ('fetch' in opts) ? opts.fetch
    : (typeof fetch !== 'undefined' ? fetch.bind(globalThis) : null);
}

Kernel.prototype._makeOfd = function (kind, extra) {
  var o = { id: this._nextOfd++, kind: kind, refs: 0 };
  if (extra) for (var k in extra) o[k] = extra[k];
  this._ofds.set(o.id, o);
  return o;
};

/* Ref an OFD for a process. For ringed pipe ends this also maintains the
 * per-end holder map (pid → fd count) that drives the SPSC mode ladder
 * (docs/archive/0181): the moment an end gains a SECOND holder process — spawn
 * inheritance is the only event that can (the spawner is parked in the
 * spawn RPC, so the flip races nothing) — a FAST pipe demotes, finally.
 * pid 0 is the kernel pseudo-holder (the strace write-end ref): it blocks
 * promotion, keeping traced pipes brokered so every byte shows in the
 * trace (docs/archive/0181's documented strace rule). */
Kernel.prototype._ofdRef = function (o, pid) {
  o.refs++;
  if (o.kind !== 'pipe' || !o.holders) return;
  o.holders.set(pid, (o.holders.get(pid) || 0) + 1);
  var ring = o.pipe.ring;
  if (ring && o.holders.size > 1 && Atomics.load(ring.i32, PR_MODE) === PR_FAST) {
    Atomics.store(ring.i32, PR_MODE, PR_DEMOTED);   // one-way; bytes stay in the ring
  }
};

/* Promotion check (docs/archive/0181), run when a pipe end LOSES a holder: with
 * exactly one holder process per end (and no kernel pseudo-holder, no
 * traced holder, both ends open) the pipe is single-producer/single-
 * consumer and the holders may move bytes through the ring themselves.
 * Deliberately NOT run at create: the creator's spawns would demote the
 * fresh promotion and burn the one-way ladder — so a pipeline promotes at
 * the parent's post-spawn closes, and a same-process self-pipe (no spawn,
 * no close) stays brokered for life (documented limit). */
/* A threaded process (docs/THREADS.md) keeps its pipes brokered: the SPSC
 * ring assumes one worker per end. Demotion is one-way, like _ofdRef's. */
Kernel.prototype._pipeDemote = function (pipe) {
  if (pipe.ring && Atomics.load(pipe.ring.i32, PR_MODE) === PR_FAST) Atomics.store(pipe.ring.i32, PR_MODE, PR_DEMOTED);
};
Kernel.prototype._pipeMaybePromote = function (pipe) {
  var ring = pipe.ring;
  if (!ring || pipe.drain || !pipe.rOpen || !pipe.wOpen) return;
  if (Atomics.load(ring.i32, PR_MODE) !== PR_LATENT) return;
  var ro = pipe.rofd, wo = pipe.wofd;
  if (!ro || !wo || ro.holders.size !== 1 || wo.holders.size !== 1) return;
  if ((ro.statusFlags | wo.statusFlags) & 0x800) return;
  var pids = [];
  ro.holders.forEach(function (n, pid) { pids.push(pid); });
  wo.holders.forEach(function (n, pid) { pids.push(pid); });
  var procs = this._procs;
  if (pids.some(function (pid) { var p = procs.get(pid); return p && p.threaded; })) return;
  for (var i = 0; i < pids.length; i++) {
    var hp = this._procs.get(pids[i]);
    if (!hp || hp.trace) return;   // pid 0 (kernel) or a traced holder: stay brokered
  }
  // Waiters parked from the brokered era keep waiting — raise their end's
  // flag so the peer's first FAST commit kicks them (the flag joins the
  // waiter's flagged list for _cancelWaiter's cleanup).
  var self = this;
  var flagQueued = function (pidList, op, word) {
    for (var qi = 0; qi < pidList.length; qi++) {
      var qp = self._channelOf(pidList[qi]);
      if (qp && qp.waiter && qp.waiter.op === op && qp.waiter.pipe === pipe) {
        self._pipeFlagWait(qp.waiter, pipe, word);
      }
    }
  };
  flagQueued(pipe.readWaiters, 'piperead', PR_RWAIT);
  flagQueued(pipe.writeWaiters, 'pipewrite', PR_WWAIT);
  Atomics.store(ring.i32, PR_MODE, PR_FAST);
};

Kernel.prototype._ofdUnref = function (id, pid) {
  var o = this._ofds.get(id);
  if (!o) return;
  if (this._readCache) this._readCache.detach(o);
  // Every descriptor close, including dup2 replacement and close-on-exec,
  // drops this process's record locks even when the OFD still has references.
  if (this._locks && o.kind === 'file' && o.ino !== undefined && pid !== undefined) {
    var kernel = this;
    this._locks.release(pid, o.ino, function (w) {
      var ch = kernel._channelOf(w.cid);
      if (ch) { ch.waiter = null; kernel._respond(ch, { errno: 'EBADF' }); }
    });
  }
  if (o.kind === 'pipe' && o.holders && pid !== undefined) {
    var hc = o.holders.get(pid);
    if (hc !== undefined) {
      if (hc <= 1) o.holders.delete(pid); else o.holders.set(pid, hc - 1);
    }
    if (o.refs > 1) this._pipeMaybePromote(o.pipe);   // end survives: SPSC check
  }
  if (--o.refs > 0) return;
  this._ofds.delete(id);
  if (o.kind === 'file') {
    // FS_WATCH settled write: a dirty open file description's LAST release
    // means the writer is done with it — write-then-close settles exactly
    // once here, and dup'd/spawn-inherited descriptors can't fire a
    // premature settle (strictly better than inotify's any-close-of-a-
    // write-fd IN_CLOSE_WRITE).
    if (o.dirty && this._watches.size) {
      this._watchEmit(o.path, FSW_CLOSE_WRITE, FSW_CLOSE_WRITE, false);
    }
    this._fs.close(o.bfsFd);
  }
  else if (o.kind === 'watch') this._watches.delete(o.id);
  else if (o.kind === 'http') {
    // Last release of an HTTP transfer fd (docs/archive/0417): abort the fetch.
    // close(2), a dup'd twin's death, and _exitProcess's fd sweep all
    // funnel here — teardown needs no dedicated transfer sweep.
    this._httpDestroy(o.xfer);
  }
  else if (o.kind === 'ptm') {
    // The terminal is gone (0020): SIGHUP to the pty's fg pgroup (POSIX —
    // this is how closing the terminal window ends the session), parked
    // slave writers get EIO, slave readers drain to EOF.
    var pt = o.pty;
    pt.out.rOpen = false;
    this._ptys.delete(pt.id);
    this._pipeNotify(pt.out);
    if (pt.tty.fgPgid > 0) this._killPgid(pt.tty.fgPgid, SIG.HUP);
    pt.tty.eof(true);                            // hangup: latched EOF (0163)
  } else if (o.kind === 'tty') {
    // A pty slave gone everywhere (0020) — the system tty's std OFDs keep
    // living in _std, so only pty slaves carry o.pty here: master reads
    // see EOF once the buffered output drains.
    if (o.pty && --o.pty.slaves === 0) { o.pty.out.wOpen = false; this._pipeNotify(o.pty.out); }
  }
  else if (o.kind === 'pipe') {
    // Last reference to this end anywhere in the system: the peers must
    // learn (readers see EOF, writers see EPIPE + SIGPIPE). Ringed pipes
    // ALSO latch the flag word — close is always brokered, so a FAST peer
    // discovers EOF/EPIPE at its next local ring op with zero RPCs.
    if (o.end === 'read') o.pipe.rOpen = false; else o.pipe.wOpen = false;
    if (o.pipe.ring) {
      Atomics.or(o.pipe.ring.i32, PR_FLAGS, o.end === 'read' ? PRF_RGONE : PRF_WGONE);
    }
    this._pipeNotify(o.pipe);
  } else if (o.kind === 'socket') {
    if (o.path && this._sockBinds.get(o.path) === o.id) this._sockBinds.delete(o.path);
    if (o.st === 'conn') {
      // Both directions lose this side: the peer reads EOF and writes EPIPE.
      o.rx.rOpen = false; o.tx.wOpen = false;
      this._pipeNotify(o.rx); this._pipeNotify(o.tx);
    } else if (o.st === 'listening' || o.st === 'bound') {
      // Unregister the rendezvous (only if it still points here — a
      // rebind after unlink may have replaced the entry).
      if (this._sockBinds.get(o.path) === o.id) this._sockBinds.delete(o.path);
      if (o.st === 'listening') {
        // Never-accepted queued connections: their client ends must learn.
        for (var pi = 0; pi < o.pending.length; pi++) {
          var pc = o.pending[pi];
          pc.rx.rOpen = false; pc.tx.wOpen = false;
          this._pipeNotify(pc.rx); this._pipeNotify(pc.tx);
        }
        o.pending.length = 0;
      }
    }
  }
};

Kernel.prototype._stdOfds = function () {
  var interactive = !!(this._tty && this._tty.interactiveOut);
  if (!this._std || this._stdInteractive !== interactive ||
      !this._ofds.has(this._std.in_.id) || !this._ofds.has(this._std.out.id) || !this._ofds.has(this._std.err.id)) {
    this._stdInteractive = interactive;
    // interactiveOut (a createTty opt, set by the UI bridge): fd 1/2 are
    // THE TTY, like a real login terminal — isatty(1) turns true and
    // shells go interactive (prompts, line editing, job control). Without
    // it (piped/CI runs) stdout stays a plain output channel and program
    // output is byte-clean. Writes route identically either way; only the
    // OFD kind (and thus isatty) differs.
    var ttyStd = this._tty && this._tty.interactiveOut;
    this._std = {
      in_: this._makeOfd(this._tty ? 'tty' : 'null', { ch: 0, tty: this._tty }),
      out: this._makeOfd(ttyStd ? 'tty' : 'out', { ch: 1, tty: this._tty }),
      err: this._makeOfd(ttyStd ? 'tty' : 'out', { ch: 2, tty: this._tty }),
    };
  }
  return this._std;
};

/* Absolute path in `pcb`'s working directory (BlockFS normalizes . and ..
 * on absolute paths; the kernel only supplies the base). */
Kernel.prototype._pathFor = function (pcb, p) {
  if (typeof p !== 'string' || p.length === 0) return p;
  if (p.charCodeAt(0) === 47) return p;
  return (pcb.cwd === '/' ? '' : pcb.cwd) + '/' + p;
};

/* Create the system tty (call BEFORE boot; v1: one tty, attached to every
 * process). opts: { cols, rows, ringSize, output(bytes) } — output receives
 * echo/control bytes for the UI bridge to render; process stdout still
 * flows through onOutput. Returns the Tty (input/resize/eof/sab). */
Kernel.prototype.createTty = function (opts) {
  this._tty = new Tty(this, opts || {});
  this._tty._brokered = this._brokered;
  return this._tty;
};

/* Boot the system: spawn pid 1 (init). spec: {path, argv, envp, cwd}.
 * Resolves to init's pid (1); rejects if the image can't be loaded. */
Kernel.prototype.boot = function (spec) {
  var self = this;
  return this._spawn(null, {
    path: spec.path,
    argv: spec.argv || [spec.path],
    envp: spec.envp || [],
    cwd: spec.cwd || '/',
    actions: [],
    flags: 0,
    pgid: 0,
  }).then(function (r) {
    if (r.errno) throw new Error('boot: ' + r.errno);
    if (self._tty && !self._tty.fgPgid) {
      var init = self._procs.get(r.pid);
      self._tty.fgPgid = init ? init.pgid : r.pid;
    }
    return r.pid;
  });
};

Kernel.prototype.process = function (pid) { return this._procs.get(pid) || null; };
Kernel.prototype.processCount = function () { return this._procs.size; };

/* Spawn a kernel-owned service (docs/archive/0014: the /bin/wm autostart): no
 * parent, own session, auto-reaped on exit (ppid 0 in _exitProcess — the
 * kernel never waits). Resolves to the pid, or 0 on failure (a missing
 * /bin/wm must not break boot: kernel-chrome is the fallback policy). */
Kernel.prototype.service = function (spec) {
  return this._spawn(null, {
    path: spec.path,
    argv: spec.argv || [spec.path],
    envp: spec.envp || [],
    cwd: spec.cwd || '/',
    actions: [],
    flags: 0,
    pgid: 0,
  }).then(function (r) { return r.errno ? 0 : r.pid; });
};

/* Host-clipboard bridge (ticket #79): the embedder-side twin of CLIP_SET —
 * set (or clear: fmt 0 / empty bytes) the one system clipboard slot from
 * outside the process world, so a host clipboard can feed gucOS pastes.
 * Same one-slot last-write-wins semantics as a process commit (an in-flight
 * CLIP_SET stage is untouched — it commits over this later, as any second
 * writer would). Does NOT fire onClipboard: that hook reports process-side
 * copies TO the embedder; echoing its own write back would loop the bridge. */
Kernel.prototype.clipSet = function (fmt, bytes) {
  this._clipboard = ((fmt | 0) && bytes && bytes.length)
    ? { fmt: fmt | 0, bytes: Uint8Array.from(bytes) } : null;
};
/* The current slot ({fmt, bytes} or null) — embedder/test introspection. */
Kernel.prototype.clipGet = function () { return this._clipboard; };

/* ---- process creation ---- */

Kernel.prototype._spawn = function (parent, spec, depth) {
  var self = this;
  if (this._halted) return Promise.resolve({ errno: 'ESRCH' });
  if (!spec || typeof spec.path !== 'string') return Promise.resolve({ errno: 'EFAULT' });
  if(spec.flags & 64){
    let cwd=spec.cwd ?? parent?.cwd ?? '/';
    for(const action of spec.actions || [])if(action.op===4){
      cwd=this._pathFor({cwd},action.path);
      const st=this._fs.stat(cwd);
      if(!st)return Promise.resolve({errno:this._fs._lastError || 'ENOENT'});
      if((st.mode&0xf000)!==0x4000)return Promise.resolve({errno:'ENOTDIR'});
    }
    spec={...spec,path:this._pathFor({cwd},spec.path)};
  }
  if(spec.prepareExec || (spec.flags & 64)){
    const st=this._fs.stat(spec.path);
    if(!st)return Promise.resolve({errno:this._fs._lastError||'ENOENT'});
    if((st.mode&0xf000)!==0x8000 || !(st.mode&0o111))return Promise.resolve({errno:'EACCES'});
  }
  // Module cache (docs/archive/0037, #188): compute the key BEFORE the image read,
  // in the same synchronous turn (both embedders' loadImage is sync, and fs
  // mutations arrive as whole RPC turns), so the identity the cache stores
  // is the identity the bytes were read under — no window for a concurrent
  // write or rename to slip between them. A cache hit skips loadImage
  // entirely (zero fs work per spawn): moduleKey just stat'ed the path,
  // which IS the existence check, and the key's own terms (RO-volume
  // immutability, or ino+size+mtime on a writable volume) prove the cached
  // compile still matches the file.
  var mkey = this._imageCacheKey(spec.path);
  if (mkey) {
    // One entry per path (#188): a changed file derives a NEW key — that
    // alone is what correctness rests on — and the path's previous entry
    // is dropped so recompiles replace instead of accumulate. A hardlink
    // alias still reaching the old contents just re-misses and recompiles.
    var prevKey = this._modulePathKey.get(spec.path);
    if (prevKey !== undefined && prevKey !== mkey) this._moduleCache.delete(prevKey);
    this._modulePathKey.set(spec.path, mkey);
  }
  var cached = mkey ? this._moduleCache.get(mkey) : null;
  if (cached) {
    this._moduleCacheHits++;
    return cached.then(function (module) {
      // Cached null = "uncacheable after all" (engine-rejected
      // bytes / no clone support): fall through to the bytes path.
      return module ? self._spawnImage(parent, spec, null, module)
        : self._spawnBytes(parent, spec, null, depth);
    });
  }
  return this._spawnBytes(parent, spec, mkey, depth);
};

/* The bytes leg of _spawn: read the image, compile-and-cache when mkey says
 * it's immutable, then hand off. A `#!` image re-dispatches to its
 * interpreter instead (docs/archive/0065); `depth` counts those hops. */
Kernel.prototype._spawnBytes = function (parent, spec, mkey, depth) {
  var self = this;
  return Promise.resolve(this._loadImage(spec.path)).then(function (image) {
    if (!image) return { errno: 'ENOENT' };
    var u8 = image instanceof Uint8Array ? image : new Uint8Array(image);
    if (u8.length >= 2 && u8[0] === 0x23 && u8[1] === 0x21) {   // "#!"
      return self._spawnShebang(parent, spec, u8, depth | 0);
    }
    return self._moduleFor(mkey, image).then(function (module) {
      return self._spawnImage(parent, spec, image, module);
    });
  });
};

/* Shebang exec (docs/archive/0065): a text image starting "#!" runs its interpreter
 * line, Unix-style — `./foo` and a desktop double-click work on shell
 * scripts with no explicit `sh`. The interpreter line is `#!` + path + at
 * most ONE optional argument (the rest of the line verbatim, per Linux — no
 * word splitting), capped at SHEBANG_MAX bytes. The re-dispatched argv is
 * [interp, optarg?, scriptPath, ...origArgv[1:]] (the script path replaces
 * the caller's argv[0], per execve(2)); everything else in the spec — envp,
 * cwd, fd actions, pgroup flags — carries over unchanged, so the
 * interpreter lands exactly where the script would have. Depth caps a
 * script→script→… chain. The errno stays ENOEXEC where Linux reports ELOOP:
 * that started as a libc gap (no ELOOP in <errno.h>) but is now a deliberate,
 * asserted divergence — docs/archive/0340 added `#define ELOOP 40`, and the contract
 * is pinned by test_kernel.js's 'shebang cycle -> ENOEXEC';
 * non-`#!` bytes never reach here, so WASM binaries are untouched. */
var SHEBANG_MAX = 256;        // interpreter-line budget (Linux BINPRM_BUF_SIZE)
var SHEBANG_MAX_DEPTH = 4;    // interpreter-is-itself-a-script hops

Kernel.prototype._spawnShebang = function (parent, spec, u8, depth) {
  if (depth >= SHEBANG_MAX_DEPTH) return Promise.resolve({ errno: 'ENOEXEC' });
  var end = -1;
  var lim = Math.min(u8.length, SHEBANG_MAX);
  for (var i = 2; i < lim; i++) { if (u8[i] === 10) { end = i; break; } }
  if (end < 0) return Promise.resolve({ errno: 'ENOEXEC' });  // no newline in budget
  var line = '';
  for (var j = 2; j < end; j++) line += String.fromCharCode(u8[j]);
  line = line.replace(/\r$/, '').replace(/^[ \t]+/, '').replace(/[ \t]+$/, '');
  if (!line) return Promise.resolve({ errno: 'ENOEXEC' });    // "#!\n"
  var sp = line.search(/[ \t]/);
  var interp = sp < 0 ? line : line.slice(0, sp);
  var optarg = sp < 0 ? '' : line.slice(sp).replace(/^[ \t]+/, '');
  // A relative interpreter resolves against the CHILD's cwd (the lookup the
  // spec's cwd implies); shebang interpreters are conventionally absolute.
  if (interp.charCodeAt(0) !== 47) {
    var cwd = (spec.cwd !== null && spec.cwd !== undefined) ? spec.cwd
      : (parent ? parent.cwd : '/');
    interp = (cwd === '/' ? '' : cwd) + '/' + interp;
  }
  var argv = [interp];
  if (optarg) argv.push(optarg);
  argv.push(spec.path);
  var orig = spec.argv;
  if (orig && orig.length > 1) argv = argv.concat(orig.slice(1));
  var nspec = Object.assign({}, spec, { path: interp, argv: argv });
  return this._spawn(parent, nspec, (depth | 0) + 1);
};

/* moduleKey through the kernel fs, or null (no fs / fs without the hook /
 * an uncacheable path). Never throws — an fs error just means "don't
 * cache". */
Kernel.prototype._imageCacheKey = function (path) {
  if (!this._fs || typeof this._fs.moduleKey !== 'function') return null;
  try { return this._fs.moduleKey(path); } catch (e) { return null; }
};

/* Resolve a spawn image to a shippable pre-compiled Module, or null (keep
 * the bytes path). Cache-hit or compile-once per moduleKey; the cached
 * Promise dedupes racing spawns of the same binary. Tiers where Modules don't
 * structured-clone retain the bytes path. */
Kernel.prototype._moduleFor = function (mkey, image) {
  if (!mkey) return Promise.resolve(null);
  var cached = this._moduleCache.get(mkey);
  if (cached) { this._moduleCacheHits++; return cached; }
  this._moduleCacheMisses++;
  var self = this;
  var bytes = image instanceof Uint8Array ? image : new Uint8Array(image);
  // Compile options MUST MATCH host.js runModule's.
  var p = WebAssembly.compile(bytes, { builtins: ['js-string'], importedStringConstants: '#' }).then(function (mod) {
    if (self._moduleCloneOk === undefined) {
      if (typeof structuredClone === 'function') {
        try { structuredClone(mod); self._moduleCloneOk = true; }
        catch (e) { self._moduleCloneOk = false; }
      } else {
        self._moduleCloneOk = true;   // no probe available; postMessage decides
      }
    }
    return self._moduleCloneOk ? mod : null;
  }, function (e) {
    // A binary the engine rejects: ship bytes and let the process worker
    // surface the real compile error to its own caller.
    self._log('module cache: compile failed for ' + mkey + ': ' + (e && e.message));
    return null;
  });
  this._moduleCache.set(mkey, p);
  return p;
};

// Read identity and bytes in the same synchronous filesystem turn. Filesystems
// without versioned keys bypass this cache. LRU bounds memory across app launches.
Kernel.prototype._libraryImage = function (path, key) {
  const cached=key&&this._libraryBytes.get(key);
  if(cached) {
    this._libraryBytesHits++;
    this._libraryBytes.delete(key);this._libraryBytes.set(key,cached);
    return cached;
  }
  const image=this._loadImage(path);
  if(!image||!key)return image;
  const source=image instanceof Uint8Array?image:new Uint8Array(image);
  const limit=64*1024*1024;
  if(source.byteLength>limit)return source;
  // Loader-owned immutable snapshots. Sharing avoids repeated structured-clone
  // byte copies; these buffers are never exposed as writable process memory.
  const bytes=typeof SharedArrayBuffer==='function'?new Uint8Array(new SharedArrayBuffer(source.byteLength)):new Uint8Array(source.byteLength);
  bytes.set(source);
  while(this._libraryBytesSize+bytes.byteLength>limit) {
    const oldest=this._libraryBytes.keys().next().value;
    this._libraryBytesSize-=this._libraryBytes.get(oldest).byteLength;
    this._libraryBytes.delete(oldest);
  }
  this._libraryBytes.set(key,bytes);this._libraryBytesSize+=bytes.byteLength;
  return bytes;
};

Kernel.prototype.moduleCacheStats = function () {
  return {
    libraryBytes: this._libraryBytesSize,
    libraryByteEntries: this._libraryBytes.size,
    libraryByteHits: this._libraryBytesHits,
    entries: this._moduleCache.size,
    hits: this._moduleCacheHits,
    misses: this._moduleCacheMisses,
  };
};

/* The tail of _spawn once the image — and, on a cache hit, its pre-compiled
 * Module — is in hand: pid allocation, fd inheritance, worker creation. */
// A program that maps memory imports the address-translation intrinsics
// (docs/MAPPINGS.md); the kernel then gives it a page table.
function moduleUsesMappings(module) {
  try { return WebAssembly.Module.imports(module).some(function (i) { return i.name === '__vm_load'; }); }
  catch (e) { return false; }
}
Kernel.prototype._spawnImage = function (parent, spec, image, module) {
  var self = this;
  // strace (0046) needs the kernel-owned fd layer (the trace sink is a pipe
  // OFD); a no-fs kernel fails the request loudly rather than not tracing.
  if (typeof spec.trace === 'number' && spec.trace >= 0 && !self._brokered) {
    return { errno: 'ENOSYS' };
  }
  if ((spec.flags & 64) && !module) return {errno:'ENOEXEC'};
  if (spec.prepareExec) {
    if (!module) return {errno:'ENOEXEC'};
    return {path:spec.path,argv:spec.argv,envp:spec.envp,module:module};
  }
  var pid = self._nextPid++;
  // spec.flags bit0 = "set process group" (posix_spawn normalizes
  // POSIX_SPAWN_SETPGROUP to 1u); pgid 0 means "own pid" per POSIX.
  var pgid = (spec.flags & 1) ? (spec.pgid > 0 ? spec.pgid : pid)
    : (parent ? parent.pgid : pid);
  var pcb = {
    pid: pid,
    autoReap: !!(spec.flags & 128),
    cid: pid,                      // channel id for waiter queues (threads: -tid)
    ppid: parent ? parent.pid : 0,
    pgid: pgid,
    sid: parent ? parent.sid : pid,
    state: STATE_RUNNING,
    // Identity for /proc (docs/archive/0043): comm/cmdline render from these;
    // startMs backs stat field 22 (start_time) and ps -l's STIME/ELAPSED.
    path: spec.path,
    argv: (spec.argv && spec.argv.length) ? spec.argv.slice() : [spec.path],
    startMs: Date.now(),
    exit: 0,                       // wait-status once ZOMBIE
    pendingStop: 0,                // stop signal not yet reported via WUNTRACED
    pendingCont: false,            // continue not yet reported via WCONTINUED
    children: new Set(),
    envp: spec.envp !== null && spec.envp !== undefined ? spec.envp
      : (parent ? parent.envp : []),
    cwd: spec.cwd !== null && spec.cwd !== undefined ? spec.cwd
      : (parent ? parent.cwd : '/'),
    sigdisp: new Int8Array(NSIG),  // __on_sigdisp mirror; all DFL initially
    itimer: null,                  // ITIMER_REAL (docs/archive/0044): {expiresAt, intervalMs, timer}
                                   // — not inherited (POSIX), cleared at exit
    // ONE deferred RPC at a time (the worker is parked):
    // {op:'wait',sel,options} | {op:'ttyread',count} | {op:'select',r,w,timer}
    // | {op:'piperead',pipe,count} | {op:'pipewrite',pipe,data}
    // | {op:'clipget'} (the clipboard seam: parked on onClipRead's done)
    waiter: null,
    fdFlags: new Map(),
    module: module || null, image: module ? null : image,
    fds: new Map(),                // procFd -> ofdId (brokered mode)
    dirRpc: null,                  // cursor -> {dh, pending} parked FS_OPENDIR
                                   // pages mid-drain (docs/archive/0241)
    dirRpcNext: 1,
    page: null, i32: null, u8: null,
    worker: null,
    tty: parent ? parent.tty : self._tty, // inherited controlling terminal
    audios: new Set(),             // aids owned by this process (docs/archive/0017)
    _audioPendingSab: null,        // SAB from 'audio-sab' awaiting AUDIO_OPEN
    _httpStage: null,              // staged request body awaiting HTTP_OPEN
    trace: null,                   // strace (0046): { ofdId, pipe, follow, drops, cur }
  };
  var sab = new SharedArrayBuffer(KP_SIZE);
  pcb.page = sab;
  pcb.i32 = new Int32Array(sab);
  pcb.u8 = new Uint8Array(sab);
  // Advertise the vsync source (docs/archive/0100) before the worker exists —
  // host.js reads the flag once at SDL-backend construction.
  if (self._vsync) Atomics.store(pcb.i32, KP_VSYNC_EN, 1);
  // Spawn-while-parked (docs/archive/0169, the KP_VSYNC_EN precedent): stamp the
  // parked flag so the new process's first present/vsync-arm rings the
  // doorbell instead of trusting a page word that was never written. Spawn
  // and the compositor's park run on the same worker thread, so the stamp
  // can't race a park loop mid-iteration.
  if (self._compParked) Atomics.store(pcb.i32, KP_COMP_PARKED, 1);
  // vDSO block (docs/archive/0179): publish before the worker exists so the first
  // process-side read never sees the zero-filled page.
  self._vdsoPublish(pcb);
  if ((spec.forkSnapshot || spec.sharedChild) && parent) {
    pcb.sigdisp.set(parent.sigdisp);
    Atomics.store(pcb.i32,KP_SIGBLOCK,spec.forkBlocked ?? Atomics.load(parent.i32,KP_SIGBLOCK));
    pcb.fdFlags=new Map(parent.fdFlags);
  }

  let spawnState;
  if (!spec.forkSnapshot && !spec.sharedChild) {
    const unmaskable = (1 << (SIG.KILL-1)) | (1 << (SIG.STOP-1));
    const blocked = ((spec.sigmask ?? (parent ? Atomics.load(parent.i32,KP_SIGBLOCK) : 0)) & ~unmaskable) >>> 0;
    let ignored = 0;
    for (let sig=1; sig<NSIG; sig++) {
      if(parent?.sigdisp[sig] === DISP_IGN && !((spec.sigdefault || 0) & (1 << (sig-1)))) {
        pcb.sigdisp[sig] = DISP_IGN; ignored |= 1 << (sig-1);
      }
    }
    Atomics.store(pcb.i32,KP_SIGBLOCK,blocked);
    spawnState = {blocked,ignored:ignored>>>0,umask:spec.umask};
  }

  // Brokered fd table: full POSIX inheritance (every parent fd, sharing
  // the open file descriptions), then the spawn file_actions in order.
  // The kernel IS the parent's fd table, so no translation is needed —
  // the payoff of the fd/data-plane amendment.
  if (self._brokered) {
    var inherit = parent ? parent.fds : null;
    if (inherit) {
      inherit.forEach(function (ofdId, fd) {
        var o = self._ofds.get(ofdId);
        if (o) { self._ofdRef(o, pid); pcb.fds.set(fd, ofdId); if(parent.fdFlags.has(fd))pcb.fdFlags.set(fd,parent.fdFlags.get(fd)); }
      });
    } else {
      var std = self._stdOfds();
      std.in_.refs++; pcb.fds.set(0, std.in_.id);
      std.out.refs++; pcb.fds.set(1, std.out.id);
      std.err.refs++; pcb.fds.set(2, std.err.id);
    }
    // strace (docs/archive/0046): spec.trace names a pipe WRITE end in the PARENT's
    // fd table (host.js only forwards it under spawn flags bit1, so old
    // binaries with the 32-byte spec can't set it by accident). The kernel
    // takes its own ref — the tracer's read end sees EOF exactly at tracee
    // teardown. flags bit2 = follow: descendants inherit the same pipe and
    // every line gets a [pid N] prefix. Attached BEFORE the fd-actions run
    // (docs/archive/0181): an action-close can shrink a pipe end to single-holder
    // and fire the SPSC promotion check — the kernel pseudo-holder ref must
    // already be there so a traced pipe never even transiently promotes.
    if (typeof spec.trace === 'number' && spec.trace >= 0) {
      var trId = parent ? parent.fds.get(spec.trace | 0) : undefined;
      var trO = trId === undefined ? null : self._ofds.get(trId);
      if (!trO || trO.kind !== 'pipe' || trO.end !== 'write') {
        pcb.fds.forEach(function (id) { self._ofdUnref(id, pid); });
        return { errno: 'EBADF' };
      }
      self._ofdRef(trO, 0);   // kernel pseudo-holder: a traced pipe never promotes
      pcb.trace = { ofdId: trO.id, pipe: trO.pipe,
                    follow: (spec.flags & 4) !== 0, drops: 0, cur: null };
    } else if (parent && parent.trace && parent.trace.follow) {
      self._ofdRef(self._ofds.get(parent.trace.ofdId), 0);
      pcb.trace = { ofdId: parent.trace.ofdId, pipe: parent.trace.pipe,
                    follow: true, drops: 0, cur: null };
    }
    var actions = spec.actions || [];
    for (var ai = 0; ai < actions.length; ai++) {
      var a = actions[ai];
      var fail = null;
      if (a.op === 0) {           // DUP2: child fd `arg` -> child fd `fd`
        var srcId = pcb.fds.get(a.arg);
        if (srcId === undefined) fail = 'EBADF';
        else {
          self._ofdRef(self._ofds.get(srcId), pid);
          var oldId = pcb.fds.get(a.fd);
          if (oldId !== undefined) self._ofdUnref(oldId, pid);
          pcb.fds.set(a.fd, srcId);pcb.fdFlags.delete(a.fd);
        }
      } else if (a.op === 1) {    // OPEN path at fd (arg = oflag)
        var aAbs = self._pathFor(pcb, a.path);
        var aExisted = true;
        if (self._watches.size && (a.arg & 0x40)) aExisted = self._fs.stat(aAbs) !== null;
        var bfsFd = self._fs.open(aAbs, a.arg | 0, (a.mode & ~(spec.umask ?? 0)) | 0);
        if (bfsFd === null) fail = self._fs._lastError || 'EIO';
        else {
          // FS_WATCH: a shell redirect (`cmd > file`) is a write like any
          // other — record the path and the truncate/create dirty bit so
          // the close settles (same rules as the FS_OPEN arm).
          var no = self._makeOfd('file', { bfsFd: bfsFd, path: aAbs,
                                           accmode: a.arg & 3, statusFlags: a.arg & (3 | 0x400 | 0x800) });
          if ((a.arg & 0x200) || !aExisted) no.dirty = true;
          if (!aExisted) self._watchEmit(aAbs, FSW_CREATE, FSW_CREATE, false);
          no.refs++;
          var prevId = pcb.fds.get(a.fd);
          if (prevId !== undefined) self._ofdUnref(prevId, pid);
          pcb.fds.set(a.fd, no.id);pcb.fdFlags.set(a.fd,(a.arg & 0x80000)?1:0);
        }
      } else if (a.op === 2) {    // CLOSE
        var cId = pcb.fds.get(a.fd);
        if (cId !== undefined) { self._ofdUnref(cId, pid); pcb.fds.delete(a.fd); }
      } else if (a.op === 3) {    // CLOSEFROM: drop every child fd >= a.fd
        // posix_spawn_file_actions_addclosefrom_np. The whole point is that
        // the caller does NOT know which fds are open — only the fd-table
        // owner does, which is exactly this side. Enumerate first, then
        // mutate: pcb.fds is being edited underneath the walk.
        var cfDrop = [];
        pcb.fds.forEach(function (id, fd) { if (fd >= a.fd) cfDrop.push(fd); });
        for (var cfi = 0; cfi < cfDrop.length; cfi++) {
          self._ofdUnref(pcb.fds.get(cfDrop[cfi]), pid);
          pcb.fds.delete(cfDrop[cfi]);
        }
      }
      if (a.op === 4) {
        const path=self._pathFor(pcb,a.path),st=self._fs.stat(path);
        if(!st)fail=self._fs._lastError || 'ENOENT';
        else if((st.mode&0xf000)!==0x4000)fail='ENOTDIR';
        else pcb.cwd=path;
      }
      if (fail) {
        pcb.fds.forEach(function (id) { self._ofdUnref(id, pid); });
        // The trace ref attached above (pre-actions since docs/archive/0181) is
        // not in pcb.fds — release it or the kernel's write-end ref leaks.
        if (pcb.trace) { self._ofdUnref(pcb.trace.ofdId, 0); pcb.trace = null; }
        return { errno: fail };
      }
    }
    // Attach a PTY for explicit spawn actions (0020); fork/clone inherit the
    // controlling terminal even if it was detached by setsid. A child whose fd 0 is a pty
    // slave lives on THAT pty — termios/pgrp RPC fallback, control-char
    // signals, and the winsize SAB handed to the worker below all follow.
    // The first attach claims the foreground (the terminal app spawns its
    // shell as a pgroup leader; the shell then owns tcsetpgrp).
    var o0id = pcb.fds.get(0);
    var o0 = o0id === undefined ? null : self._ofds.get(o0id);
    if (!spec.forkSnapshot && !spec.sharedChild && o0 && o0.kind === 'tty' && o0.pty) {
      pcb.tty = o0.tty;
      if (!pcb.tty.fgPgid) pcb.tty.fgPgid = pcb.pgid;
    }
  }

  if (!spec.forkSnapshot && !spec.sharedChild) {
    for (const [fd,flags] of pcb.fdFlags) if ((flags&1) && pcb.fds.has(fd)) {
      self._ofdUnref(pcb.fds.get(fd),pid);pcb.fds.delete(fd);
    }
    pcb.fdFlags.clear();
  }
  self._procs.set(pid, pcb);
  if (parent) parent.children.add(pid);
  // SPSC pipe rings (docs/archive/0181): every ringed pipe fd in the child's
  // FINAL table (post-actions) ships its ring SAB, whatever the current
  // mode — the PR_MODE word, not process-local knowledge, gates fast ops,
  // so a pipe promoted AFTER this spawn (the parent's closes) just starts
  // moving bytes locally.
  var pipeRings = null;
  if (self._brokered) {
    pcb.fds.forEach(function (ofdId, fd) {
      var po = self._ofds.get(ofdId);
      if (po && po.kind === 'pipe' && po.pipe.ring) {
        (pipeRings = pipeRings || []).push({ fd: fd, end: po.end, sab: po.pipe.ring.sab });
      }
    });
  }
  // Mapped memory (docs/MAPPINGS.md): a program whose module imports the
  // translation intrinsics gets a page table; a fork child inherits the
  // parent's mappings (shared pages shared, private pages copied).
  pcb.vm = null;
  var procSpec = {
    forkSnapshot: spec.forkSnapshot || null,
    sharedChild: spec.sharedChild || null,
    execState: spawnState,
    vm: pcb.vm ? self._mapped.state(pid) : null,
    pid: pid, ppid: pcb.ppid, pgid: pcb.pgid,
    path: spec.path,
    argv: pcb.argv,
    envp: pcb.envp,
    cwd: pcb.cwd,
    actions: spec.actions || [],   // brokered: already applied kernel-side above
    flags: spec.flags | 0,
    // Cache hit ships the Module and DROPS the bytes (they'd be a dead
    // multi-MB clone per spawn — runModule never touches bytes when a
    // pre-compiled C module arrives).
    image: module ? null : image,
    module: module || null,
    kernelPage: sab,
    ttySab: pcb.tty ? pcb.tty.sab : null,
    brokered: self._brokered,
    fileReadCache: self._readCache?self._readCache.v.sab:null,
    ro: self._roImage,   // process-side read-only volume (docs/archive/0180)
    pipeRings: pipeRings,   // SPSC pipe rings for inherited fds (docs/archive/0181)
  };
  try {
    if (self._mapped) {
      if (spec.sharedChild && parent && parent.vm) { self._mapped.share(parent.pid,pid); pcb.vm=true; }
      else if (spec.forkSnapshot && parent && parent.vm) { self._mapped.clone(parent.pid, pid); pcb.vm = true; }
      else if (module && moduleUsesMappings(module)) { self._mapped.owner(pid); pcb.vm = true; }
    }
    procSpec.vm = pcb.vm ? self._mapped.state(pid) : null;
    pcb.worker = self._createWorker(procSpec);
  } catch (e) {
    self._procs.delete(pid);
    if (parent) parent.children.delete(pid);
    pcb.fds.forEach(function(id){self._ofdUnref(id,pid);});
    if(pcb.trace)self._ofdUnref(pcb.trace.ofdId,0);
    if (self._mapped) self._mapped.release(pid);
    self._log('spawn: createWorker failed: ' + (e && e.message));
    return { errno: e.errno || 'EAGAIN' };
  }
  pcb.worker.onMessage(function (msg) { self._onWorkerMessage(pcb, msg); });
  pcb.worker.onExit(function () {
    // Channel death without an 'exited' message = abnormal termination.
    if (pcb.state === STATE_RUNNING) self._exitProcess(pcb, W_TERMSIG(SIG.SEGV));
  });
  return { pid: pid };
};

/* ---- worker message handling ---- */

// Bulk memory travels by structured clone, not the small synchronous RPC page.
// The caller is JSPI-suspended until this transition is acknowledged.
Kernel.prototype._releaseFork = function(pcb) {
  for(const c of pcb.forkChannels||[]) {
    Atomics.store(c.i32,KP_FORK_REQ,0);Atomics.notify(c.i32,KP_FORK_REQ);
  }
  pcb.forkChannels=null;pcb.forkOwner=null;
};
Kernel.prototype._processControl = async function(pcb,op,data,caller=pcb) {
  if(pcb.state===STATE_ZOMBIE)return {errno:'ESRCH'};
  if(op==='load-library') {
    // Read the identity and bytes in one synchronous filesystem turn, just as
    // for executable images. A changed file gets a fresh compiled module.
    for(const candidate of data.candidates||[]) {
      const path=this._pathFor(pcb,candidate);
      const stat=this._fs.stat(path);
      if(!stat)continue;
      if((stat.mode&0xf000)!==0x8000||stat.size<8||stat.size>64*1024*1024)
        return {error:'Invalid shared library size or type: '+path};
      const mkey=this._imageCacheKey(path);
      const image=this._libraryImage(path,mkey);
      if(!image)continue;
      const bytes=image instanceof Uint8Array?image:new Uint8Array(image);
      const canonical=this._fs.realpathPhysical?.(path)||path;
      if(mkey) {
        const prev=this._modulePathKey.get(path);
        if(prev!==undefined&&prev!==mkey)this._moduleCache.delete(prev);
        this._modulePathKey.set(path,mkey);
      }
      const module=await this._moduleFor(mkey,bytes);
      return {library:{bytes,path:canonical,module,metadata:module?self.DynamicLoader?.metadataText(module):null}};
    }
    return {};
  }
  if(pcb._sharedParent && (op==='fork-prepare' || op==='thread'))return {errno:'EOPNOTSUPP'};
  if(op==='fork-prepare') {
    if(pcb.forkOwner)return {errno:'EAGAIN'};
    pcb.forkOwner=caller;
    const epoch=pcb.forkEpoch=((pcb.forkEpoch||0)+1)|0;
    if(!epoch)pcb.forkEpoch=1;
    const channels=[pcb,...(pcb.threads?.values()||[])].filter(c=>c!==caller);
    pcb.forkChannels=channels;
    for(const c of channels){
      Atomics.store(c.i32,KP_FORK_REQ,pcb.forkEpoch);
      Atomics.add(c.i32,KP_DOORBELL,1);Atomics.notify(c.i32,KP_DOORBELL);
    }
    const deadline=Date.now()+2000;
    while(channels.some(c=>c.worker&&Atomics.load(c.i32,KP_FORK_ACK)!==pcb.forkEpoch)) {
      if(Date.now()>=deadline||pcb.state===STATE_ZOMBIE){this._releaseFork(pcb);return {errno:'EAGAIN'};}
      await new Promise(resolve=>setTimeout(resolve,2));
    }
    return {};
  }
  if(op==='fork-release') { if(pcb.forkOwner===caller)this._releaseFork(pcb);return {}; }
  if(op==='shared-exec-ready'){this._releaseSharedChild(pcb);return {};}
  if(op==='shared-child'){
    if(pcb.threads?.size || pcb._sharedParent || pcb.audios.size || pcb.dirRpc?.size)
      return {errno:'EOPNOTSUPP'};
    if(!data.child?.memory || !pcb.module)return {errno:'EINVAL'};
    const r=this._spawnImage(pcb,{path:pcb.path,argv:pcb.argv,envp:pcb.envp.slice(),cwd:pcb.cwd,
      sharedChild:data.child,forkBlocked:Atomics.load(caller.i32,KP_SIGBLOCK)},null,pcb.module);
    if(r.errno)return r;
    return await new Promise(resolve=>{const child=this._procs.get(r.pid);child._sharedParent=pcb;child._sharedDone=resolve;});
  }
  if(op==='fork'){
    if(!pcb.module||!data.snapshot)return {errno:'ENOSYS'};
    if(pcb.audios.size||pcb.dirRpc?.size)return {errno:'EOPNOTSUPP'};
    if(pcb.forkOwner!==caller)return {errno:'EAGAIN'};
    const r=this._spawnImage(pcb,{path:pcb.path,argv:pcb.argv,envp:pcb.envp.slice(),cwd:pcb.cwd,forkSnapshot:data.snapshot,forkBlocked:Atomics.load(caller.i32,KP_SIGBLOCK)},null,pcb.module);
    if(r.errno)return r;
    return await new Promise(resolve=>{this._procs.get(r.pid)._forkReady=resolve;});
  }
  if(op==='thread'){
    if(!data.module||!data.memory)return {errno:'EINVAL'};
    if(pcb.forkOwner)return {errno:'EAGAIN'};
    return this._createThread(pcb,data,caller);
  }
  if(op==='exec-prepare'){
    if(!data.path)return {errno:'ENOENT'};
    const spec={path:this._pathFor(pcb,data.path),argv:data.argv,envp:data.envp,prepareExec:true};
    const result=await this._spawn(pcb,spec);
    if(result.errno)return result;
    if(pcb.state===STATE_ZOMBIE)return {errno:'ESRCH'};
    result.token=(pcb._execToken||0)+1;pcb._execToken=result.token;
    pcb._execPrepared=result;
    return result;
  }
  if(op==='exec-commit'){
    const image=pcb._execPrepared;
    if(!image||image.token!==data.token)return {errno:'EINVAL'};
    pcb._execPrepared=null;
    for(const [fd,flags] of pcb.fdFlags)if((flags&1)&&pcb.fds.has(fd)){
      this._ofdUnref(pcb.fds.get(fd),pcb.pid);pcb.fds.delete(fd);
    }
    pcb.fdFlags.clear();
    if(pcb.dirRpc){pcb.dirRpc.forEach(d=>this._fs.closedir(d.dh));pcb.dirRpc=null;}
    let ignored=0;
    for(let s=1;s<NSIG;s++){
      if(pcb.sigdisp[s]===1)ignored|=1<<(s-1);
      else pcb.sigdisp[s]=0;
    }
    this._killThreads(pcb);pcb.threaded=false;
    pcb.path=image.path;pcb.argv=image.argv.slice();pcb.envp=image.envp.slice();pcb.module=image.module;pcb.image=null;
    // Mappings do not survive exec; the new image gets a fresh table if it maps.
    if(this._mapped){this._mapped.release(pcb.pid);pcb.vm=null;if(moduleUsesMappings(image.module)){this._mapped.owner(pcb.pid);pcb.vm=true;}}
    // Same PCB, kernel page, PID, children, cwd, pgroup, session and timers.
    const pipeRings=[];
    pcb.fds.forEach((id,fd)=>{const o=this._ofds.get(id);if(o?.kind==='pipe'&&o.pipe.ring)pipeRings.push({fd,end:o.end,sab:o.pipe.ring.sab});});
    return {blocked:Atomics.load(pcb.i32,KP_SIGBLOCK),ignored:ignored,pipeRings,vm:pcb.vm?this._mapped.state(pcb.pid):null};
  }
  return {errno:'ENOSYS'};
};

/* ---- threads (docs/THREADS.md) ----
 * A thread is a worker on the process's shared memory with its own kernel
 * page, so its RPCs and blocking waits are independent of the main thread's.
 * The channel object inherits the process record: fd table, cwd, signal
 * dispositions and job-control state read and write through to the process,
 * while the RPC page, worker handle and parked waiter are per thread. */
var THREAD_PROCESS_FIELDS = ['state', 'cwd', 'pgid', 'sid', 'exit', 'tty', 'itimer', 'pendingStop', 'pendingCont',
  'wantFrame', 'trace', '_pipePendingSab', '_execPrepared', '_execToken', '_forkReady', 'threaded'];
Kernel.prototype._createThread = function (pcb, data, caller) {
  var self = this;
  if (!pcb.threads) pcb.threads = new Map();
  var tid = this._nextTid = (this._nextTid || 0) + 1;
  var sab = new SharedArrayBuffer(KP_SIZE);
  var th = Object.create(pcb);
  THREAD_PROCESS_FIELDS.forEach(function (f) {
    Object.defineProperty(th, f, { get: function () { return pcb[f]; }, set: function (v) { pcb[f] = v; }, enumerable: true });
  });
  th.tid = tid; th.cid = -tid; th.isThread = true; th.__proc = pcb; th.i32 = new Int32Array(sab); th.u8 = new Uint8Array(sab);
  th.waiter = null; th.dirRpc = null; th._httpStage = null; th.clipStage = null; th.worker = null;
  th._audioPendingSab = null;
  Atomics.store(th.i32, KP_SIGBLOCK, Atomics.load((caller || pcb).i32, KP_SIGBLOCK));
  if (self._vsync) Atomics.store(th.i32, KP_VSYNC_EN, 1);
  self._vdsoPublish(th);
  // Pipe rings are single-producer/single-consumer per worker; a threaded
  // process keeps every pipe brokered so two workers never share a ring.
  pcb.threaded = true;
  pcb.fds.forEach(function (id) { var o = self._ofds.get(id); if (o && o.kind === 'pipe' && o.pipe.ring) self._pipeDemote(o.pipe); });
  var spec = {
    thread: { record: data.record, size: data.size, tid: tid, memory: data.memory, extraMemories:data.extraMemories, dynamic:data.dynamic, dynamicWorker:data.dynamicWorker, dynamicModules:data.dynamicModules },
    vm: pcb.vm ? self._mapped.state(pcb.pid) : null,
    pid: pcb.pid, tid: tid, ppid: pcb.ppid, pgid: pcb.pgid, path: pcb.path, argv: pcb.argv, envp: pcb.envp, cwd: pcb.cwd,
    module: data.module, image: null, kernelPage: sab, ttySab: pcb.tty ? pcb.tty.sab : null,
    brokered: self._brokered, ro: self._roImage, fileReadCache:self._readCache?self._readCache.v.sab:null, pipeRings: [], flags: 0, actions: [],
  };
  try { th.worker = self._createWorker(spec); }
  catch (e) { self._log('thread: createWorker failed: ' + (e && e.message)); return { errno: 'EAGAIN' }; }
  pcb.threads.set(tid, th);
  th.worker.onMessage(function (msg) { self._onThreadMessage(pcb, th, msg); });
  th.worker.onExit(function () { self._threadGone(pcb, th); });
  return { tid: tid };
};
Kernel.prototype._threadGone = function (pcb, th) {
  if (!pcb.threads || !pcb.threads.has(th.tid)) return;
  pcb.threads.delete(th.tid);
  this._cancelWaiter(th);
  if (th.dirRpc) { var fs = this._fs; th.dirRpc.forEach(function (d) { fs.closedir(d.dh); }); th.dirRpc = null; }
  if (th.worker) { try { th.worker.terminate(); } catch (e) {} th.worker = null; }
};
Kernel.prototype._killThreads = function (pcb) {
  if (!pcb.threads) return;
  var self = this;
  Array.from(pcb.threads.values()).forEach(function (th) { self._threadGone(pcb, th); });
};
Kernel.prototype._onThreadMessage = function (pcb, th, msg) {
  if (!msg || pcb.state === STATE_ZOMBIE) return;
  switch (msg.type) {
    case 'krpc': this._dispatchRpc(th); break;
    case 'audio-sab': th._audioPendingSab = msg.sab || null; break;
    case 'krpc-intr':
      if (th.waiter) { this._cancelWaiter(th); this._respond(th, { errno: 'EINTR' }); }
      break;
    case 'out': this._onOutput(pcb.pid, msg.fd, msg.bytes); break;
    case 'process-control':
      // Fork snapshots the calling thread; exec in a fork child uses its main channel.
      if (['thread','fork-prepare','fork','fork-release','load-library'].includes(msg.op)) {
        this._processControl(pcb, msg.op, msg.data, th).then(function (r) { if (th.worker) th.worker.postMessage({ type: 'process-result', id: msg.id, result: r }); }).catch(function (e) { if (th.worker) th.worker.postMessage({ type: 'process-result', id: msg.id, result: { errno: 'EIO', error: e.message } }); });
      } else if (th.worker) th.worker.postMessage({ type: 'process-result', id: msg.id, result: { errno: 'EPERM' } });
      break;
    case 'exited': this._exitProcess(pcb, W_EXITCODE(msg.code | 0)); break;
    case 'thread-exited':
      if (Object.prototype.hasOwnProperty.call(th, 'exitRequested')) this._exitProcess(pcb, th.exitRequested);
      else this._threadGone(pcb, th);
      break;
    case 'crashed':
      this._log('pid ' + pcb.pid + ' thread ' + th.tid + ' crashed: ' + msg.error);
      this._exitProcess(pcb, W_TERMSIG(SIG.SEGV));
      break;
    case 'start-failed':
      this._log('pid ' + pcb.pid + ' thread ' + th.tid + ' could not start: ' + msg.error);
      this._threadGone(pcb, th);
      break;
    default: break;   // window/audio traffic is main-thread only
  }
};

Kernel.prototype._onWorkerMessage = function (pcb, msg) {
  // STOPPED still accepts messages: a krpc/exit can race the stop, and
  // dropping the krpc would deadlock the parked worker awaiting a response.
  if (!msg || pcb.state === STATE_ZOMBIE) return;
  switch (msg.type) {
    case 'process-control':
      this._processControl(pcb,msg.op,msg.data).then(r=>{
        if(pcb.worker)pcb.worker.postMessage({type:'process-result',id:msg.id,result:r});
      }).catch(e=>{
        this._log('process transition: '+e.message);
        if(pcb.worker)pcb.worker.postMessage({type:'process-result',id:msg.id,result:{errno:'EIO'}});
      });break;
    case 'fork-ready':
      if(pcb._forkReady){pcb._forkReady({pid:pcb.pid});pcb._forkReady=null;}break;
    case 'krpc': this._dispatchRpc(pcb); break;
    // A parked interruptible RPC (WAIT / tty FS_READ / FS_SELECT / the
    // unified FS_WAIT / pipe FS_READ/FS_WRITE) noticed a deliverable
    // signal: answer EINTR if it's still registered. If the real
    // result raced in first, the waiter is already gone — ignore, the signal
    // delivers at the caller's next safe point anyway.
    case 'krpc-intr':
      if (pcb.waiter) { this._cancelWaiter(pcb); this._respond(pcb, { errno: 'EINTR' }); }
      break;
    case 'out': this._onOutput(pcb.pid, msg.fd, msg.bytes); break;
    // Audio (docs/archive/0017): the source-ring SAB precedes its AUDIO_OPEN on the
    // same FIFO channel — the wm-sabs handshake, verbatim.
    case 'audio-sab':
      if (msg.sab) pcb._audioPendingSab = msg.sab;
      break;
    // SPSC pipe ring (docs/archive/0181): precedes its PIPE_CREATE, same handshake.
    case 'pipe-sab':
      if (msg.sab) pcb._pipePendingSab = msg.sab;
      break;
    case 'exited': this._exitProcess(pcb, W_EXITCODE(msg.code | 0)); break;
    case 'start-failed':
      this._log('pid ' + pcb.pid + ' could not start: ' + msg.error);
      this._exitProcess(pcb, W_EXITCODE(127));
      break;
    case 'crashed':
      if(pcb._forkReady){
        this._log('fork child could not initialize: '+msg.error);
        pcb._forkReady({errno:'EAGAIN'});pcb._forkReady=null;
        const parent=this._procs.get(pcb.ppid);if(parent)parent.children.delete(pcb.pid);
        pcb.worker.terminate();pcb.fds.forEach(id=>this._ofdUnref(id,pcb.pid));
        if(pcb.trace)this._ofdUnref(pcb.trace.ofdId,0);
        this._procs.delete(pcb.pid);pcb.state=STATE_ZOMBIE;return;
      }
      this._log('pid ' + pcb.pid + ' crashed: ' + msg.error);
      this._exitProcess(pcb, W_TERMSIG(SIG.SEGV));
      break;
    default: this._log('pid ' + pcb.pid + ': unknown message ' + msg.type);
  }
};

/* ---- strace (docs/archive/0046): per-pid syscall-RPC trace ----
 * pcb.trace = { ofdId, pipe, follow, drops, cur } — attached at spawn from
 * spec.trace (a pipe WRITE end in the parent's fd table; the kernel holds
 * its own ref until exit, so the tracer reading the other end sees EOF
 * exactly when the tracee is gone). Every RPC appends one decoded line:
 * the request formats at dispatch (pcb.trace.cur), the line lands at
 * response time — deferred RPCs (parked reads/waits) trace at completion.
 * With the flag off the cost is one falsy check per dispatch/respond. */
Kernel.prototype._traceLine = function (pcb, line, force) {
  var tr = pcb.trace;
  var pipe = tr.pipe;
  if (!pipe.rOpen || !pipe.wOpen) return;     // tracer gone: stop emitting
  // The kernel must never block: a full pipe (tracer not draining) drops
  // the line and says so at exit rather than growing without bound. The
  // exit markers ride `force` — bounded, and the drop notice must not
  // itself drop.
  if (!force && this._pipeAvail(pipe) > pipe.cap) { tr.drops++; return; }
  if (tr.follow) line = '[pid ' + pcb.pid + '] ' + line;
  var bytes = textEncoder.encode(line + '\n');
  if (pipe.ring) {
    // A ring is a hard cap (the JS buf could overshoot for `force` lines):
    // truncate the final markers rather than corrupt the ring. Trace pipes
    // never promote (the kernel's write-end ref is a pseudo-holder), so
    // this stays the kernel's single-producer lane.
    var rfree = this._pipeFree(pipe);
    if (bytes.length > rfree) {
      if (!force) { tr.drops++; return; }
      bytes = bytes.subarray(0, rfree);
    }
    if (bytes.length) pipeRingPut(pipe.ring, bytes);
  } else {
    for (var i = 0; i < bytes.length; i++) pipe.buf.push(bytes[i]);
  }
  this._pipeNotify(pipe);
};

Kernel.prototype._traceRpc = function (pcb, resp, rawBytes) {
  var cur = pcb.trace.cur;
  pcb.trace.cur = null;
  this._traceLine(pcb, cur.name + '(' + cur.args + ') = ' + traceResult(resp, rawBytes));
};

/* Final trace lines + the kernel's write-end ref release (=> reader EOF).
 * Runs once per traced pcb, from _exitProcess. */
Kernel.prototype._traceExit = function (pcb, status) {
  var tr = pcb.trace;
  var cur = tr.cur;
  tr.cur = null;
  if (cur) {
    // EXIT never gets a response by design; anything else died mid-RPC.
    this._traceLine(pcb, cur.name + '(' + cur.args + ')' +
      (cur.name === 'EXIT' ? '' : ' = <unfinished>'), true);
  }
  if (tr.drops) {
    this._traceLine(pcb, '+++ ' + tr.drops + ' trace lines dropped (pipe full) +++', true);
  }
  if (W_TERMSIG(status) !== 0 && (status & 0xff) !== 0x7f) {
    this._traceLine(pcb, '+++ killed by ' +
      (SIG_NAMES[W_TERMSIG(status)] || 'signal ' + W_TERMSIG(status)) + ' +++', true);
  } else {
    this._traceLine(pcb, '+++ exited with ' + ((status >> 8) & 0xff) + ' +++', true);
  }
  pcb.trace = null;
  this._ofdUnref(tr.ofdId, 0);
};

// Keep the compiler's existing result in the broker and transfer bounded pages.
// 4096 UTF-16 units per stream fit even JSON's worst-case six-byte escaping.
// Do not split a surrogate pair: each page is independently UTF-8 encoded.
Kernel.prototype._compileOutput = function (pcb) {
  var pending = pcb.compileOutput;
  if (!pending) { this._respond(pcb, { errno: 'EINVAL' }); return; }
  var r = pending.result;
  function take(name, cursor) {
    var text = r[name] || '', at = pending[cursor], end = Math.min(at + 4096, text.length);
    if (end < text.length && text.charCodeAt(end - 1) >= 0xd800 && text.charCodeAt(end - 1) <= 0xdbff) end--;
    pending[cursor] = end;
    return text.slice(at, end);
  }
  var stdout = take('stdout', 'out'), stderr = take('stderr', 'err');
  var more = pending.out < (r.stdout || '').length || pending.err < (r.stderr || '').length;
  if (!more) pcb.compileOutput = null;
  this._respond(pcb, { exitCode: r.exitCode, stdout: stdout, stderr: stderr, more: more });
};

Kernel.prototype._respond = function (pcb, resp) {
  if (this._commit) {
    try { this._commit(); }
    catch (e) { this._log('disk commit failed: ' + (e && e.message)); }
  }
  if (pcb.trace && pcb.trace.cur) this._traceRpc(pcb, resp, null);
  if (!writePayload(pcb.i32, pcb.u8, resp)) {
    writePayload(pcb.i32, pcb.u8, { errno: 'ENOMEM' });
  }
  Atomics.store(pcb.i32, KP_RPC_STATE, RPC_DONE);
  this._ring(pcb);
};

/* Bump + notify a process's doorbell (any-event wakeup). */
Kernel.prototype._ring = function (pcb) {
  Atomics.add(pcb.i32, KP_DOORBELL, 1);
  Atomics.notify(pcb.i32, KP_DOORBELL);
};

Kernel.prototype._dispatchRpc = function (pcb) {
  var self = this;
  if (Atomics.load(pcb.i32, KP_RPC_STATE) !== RPC_REQUEST) return; // stale ping
  var op = Atomics.load(pcb.i32, KP_RPC_OP);
  var req;
  try { req = readPayload(pcb.i32, pcb.u8); }
  catch (e) { this._respond(pcb, { errno: 'EFAULT' }); return; }
  if (pcb.trace) {
    // Args format NOW (RAW payloads alias the kernel page); the line lands
    // when the response does — see _traceRpc.
    pcb.trace.cur = {
      name: OP_NAMES[op] || '0x' + op.toString(16),
      args: traceArgs(op, req),
    };
  }
  switch (op) {
    case OP.SPAWN:
      this._spawn(pcb, req).then(function (r) { self._respond(pcb, r); });
      break;
    case OP.WAIT: this._wait(pcb, req.pid | 0, req.options | 0); break;
    case OP.KILL: this._respond(pcb, this.kill(req.pid | 0, req.sig | 0, pcb)); break;
    // Acknowledge exit so the worker can unwind its Wasm/JSPI invocation.
    // Terminating it inside the parked syscall retains Wasm memories in
    // Chromium under process churn. The final exited message tears down the
    // process, after all earlier output and runtime cleanup have completed.
    case OP.EXIT:
      pcb.exitRequested = W_EXITCODE(req.code | 0);
      this._respond(pcb, {});
      break;
    case OP.SIGDISP:
      if (req.sig > 0 && req.sig < NSIG) pcb.sigdisp[req.sig] = req.kind | 0;
      this._respond(pcb, {});
      break;
    case OP.SETPGID: this._respond(pcb, this._setpgid(pcb, req.pid | 0, req.pgid | 0)); break;
    case OP.GETPGID: {
      var t = (req.pid | 0) === 0 ? pcb : this._procs.get(req.pid | 0);
      this._respond(pcb, t ? { pgid: t.pgid } : { errno: 'ESRCH' });
      break;
    }
    case OP.SETSID:
      if (pcb.pgid === pcb.pid) { this._respond(pcb, { errno: 'EPERM' }); break; }
      pcb.pgid = pcb.pid; pcb.sid = pcb.pid; pcb.tty = null;
      this._vdsoPublish(pcb);
      this._respond(pcb, { sid: pcb.sid });
      break;
    case OP.GETSID: {
      var tSid = (req.pid | 0) === 0 ? pcb : this._procs.get(req.pid | 0);
      this._respond(pcb, tSid ? { sid: tSid.sid } : { errno: 'ESRCH' });
      break;
    }
    case OP.PAD_NAME: {
      // Gamepad name query (#607): the one pad datum an 8-word ring record
      // cannot carry. Names outlive disconnect (bounded cache) so an open
      // SDL handle can still answer SDL_GetGamepadName after REMOVED.
      var pnm = this._padNames.get(req.id | 0);
      this._respond(pcb, pnm !== undefined ? { name: pnm } : { errno: 'ENODEV' });
      break;
    }
    // Interval timers (docs/archive/0044): pure kernel-side bookkeeping over the
    // existing SIGPEND delivery machinery.
    case OP.SETITIMER:
      this._respond(pcb, this._setitimer(pcb, req.which | 0, req.valueMs | 0, req.intervalMs | 0));
      break;
    case OP.GETITIMER:
      this._respond(pcb, (req.which | 0) !== ITIMER_REAL ? { errno: 'EINVAL' }
        : this._itimerRemaining(pcb));
      break;
    // Termios/pgrp RPCs resolve the tty THROUGH the caller's fd (0020:
    // ptys mean many ttys); fd-less requests and ring mode fall back to
    // the process's attached tty (_ttyForFd).
    case OP.TCGETATTR: {
      var gAt = this._ttyForFd(pcb, req.fd);
      this._respond(pcb, gAt ? gAt.getattr() : { errno: 'ENOTTY' });
      break;
    }
    case OP.TCSETATTR: {
      var sAt = this._ttyForFd(pcb, req.fd);
      if (!sAt) { this._respond(pcb, { errno: 'ENOTTY' }); break; }
      sAt.setattr(req.actions | 0, req);
      this._respond(pcb, {});
      break;
    }
    case OP.TCGETPGRP: {
      var gPg = this._ttyForFd(pcb, req.fd);
      this._respond(pcb, gPg ? { pgid: gPg.fgPgid } : { errno: 'ENOTTY' });
      break;
    }
    case OP.TCSETPGRP: {
      var sPg = this._ttyForFd(pcb, req.fd);
      if (!sPg) { this._respond(pcb, { errno: 'ENOTTY' }); break; }
      if (!(req.pgid > 0)) { this._respond(pcb, { errno: 'EINVAL' }); break; }
      sPg.fgPgid = req.pgid | 0;
      this._respond(pcb, {});
      break;
    }
    // The master side's resize (0020): winsize words + SIGWINCH to the
    // pty's fg pgroup (Tty.resize, shared with the system tty's bridge).
    case OP.TIOCSWINSZ: {
      var wTty = this._ttyForFd(pcb, req.fd);
      if (!wTty) { this._respond(pcb, { errno: 'ENOTTY' }); break; }
      wTty.resize(req.cols | 0, req.rows | 0);
      this._respond(pcb, {});
      break;
    }
    case OP.PTY_CREATE: {
      // Ptys (docs/archive/0020): the slave is a FULL Tty — line discipline,
      // termios, control-char signal routing, and the deferred-read
      // machinery are reused verbatim; only the byte endpoints differ
      // (the master fd stands where the UI bridge does for the system
      // tty). Echo and slave output land in `out` (pipe-shaped), so
      // master reads and select ride _streamRead/_pipeNotify unchanged.
      if (!this._brokered) { this._respond(pcb, { errno: 'ENOSYS' }); break; }
      var pty = this._newPty(false);
      var mO = this._makeOfd('ptm', { pty: pty, accmode: 2 });
      var sO = this._ptySlaveOfd(pty);
      mO.refs++; sO.refs++;
      var pmfd = 0; while (pcb.fds.has(pmfd)) pmfd++;
      pcb.fds.set(pmfd, mO.id);
      var psfd = 0; while (pcb.fds.has(psfd)) psfd++;
      pcb.fds.set(psfd, sO.id);
      this._respond(pcb, { mfd: pmfd, sfd: psfd });
      break;
    }
    case OP.PTY_CONTROL: {
      var po = this._ofds.get(pcb.fds.get(req.fd | 0));
      if (!po) { this._respond(pcb, { errno: 'EBADF' }); break; }
      if (req.request === 4) {
        if (po.kind !== 'tty') { this._respond(pcb,{errno:'ENOTTY'}); break; }
        var target = po.tty || this._tty;
        if (pcb.sid !== pcb.pid || (pcb.tty && pcb.tty !== target) ||
            (target.session && target.session !== pcb.sid)) {
          this._respond(pcb,{errno:'EPERM'}); break;
        }
        pcb.tty = target; target.session = pcb.sid; target.fgPgid = pcb.pgid;
      } else {
        if (po.kind !== 'ptm') { this._respond(pcb,{errno:'ENOTTY'}); break; }
        if (req.request === 1) { this._respond(pcb,{value:po.pty.id}); break; }
        if (req.request === 3) po.pty.locked = false;
        else if (req.request !== 2) { this._respond(pcb,{errno:'EINVAL'}); break; }
      }
      this._respond(pcb,{value:0}); break;
    }
    case OP.PIPE_CREATE: {
      // Pipes are just another OFD kind (the fd/data-plane amendment's
      // payoff): two descriptions over one kernel-side buffer, riding the
      // same fd tables, inheritance, fd_actions, FS_READ/WRITE/CLOSE and
      // select paths as files. Brokered mode only — standalone pages keep
      // host.js's in-process pipes.
      if (!this._brokered) { this._respond(pcb, { errno: 'ENOSYS' }); break; }
      var pipe = {
        buf: [], cap: PIPE_CAP, rOpen: true, wOpen: true,
        readWaiters: [], writeWaiters: [],   // pids with a deferred RPC, FIFO
      };
      // SPSC ring (docs/archive/0181): the creator posts the ring SAB immediately
      // before this RPC ({type:'pipe-sab'} — the audio-sab handshake; the
      // kernel can't hand an SAB to a parked worker). A ringed pipe keeps
      // its bytes in the ring in every mode; no SAB (fake workers, old
      // embedders) = the JS buf and permanent brokered service.
      var prSab = pcb._pipePendingSab || null;
      pcb._pipePendingSab = null;
      if (prSab && prSab.byteLength > PIPE_RING_HDR) {
        pipe.ring = pipeRingViews(prSab);
        pipe.cap = pipe.ring.cap;
      }
      var ro = this._makeOfd('pipe', { pipe: pipe, end: 'read', holders: new Map() });
      var wo = this._makeOfd('pipe', { pipe: pipe, end: 'write', holders: new Map() });
      pipe.rofd = ro; pipe.wofd = wo;   // the promotion check reads both ends
      this._ofdRef(ro, pcb.pid); this._ofdRef(wo, pcb.pid);
      var rfd = 0; while (pcb.fds.has(rfd)) rfd++;
      pcb.fds.set(rfd, ro.id);
      var wfd = 0; while (pcb.fds.has(wfd)) wfd++;
      pcb.fds.set(wfd, wo.id);
      this._respond(pcb, { rfd: rfd, wfd: wfd });
      break;
    }
    case OP.PIPE_KICK: {
      // The SPSC doorbell (docs/archive/0181): a fast end committed a ring op
      // while the peer's parked flag was up — re-serve parked waiters and
      // rescan selects. {epipe:1} = the caller hit PRF_RGONE locally and
      // wants its own SIGPIPE (deliver BEFORE responding so the pending
      // bit is up when the write's import returns — brokered ordering).
      var ko = null;
      var kid = pcb.fds.get(req.fd | 0);
      if (kid !== undefined) ko = this._ofds.get(kid);
      if (!ko || ko.kind !== 'pipe') { this._respond(pcb, { errno: 'EBADF' }); break; }
      if (req.epipe) this._deliver(pcb, SIG.PIPE);
      this._respond(pcb, {});
      this._pipeNotify(ko.pipe);
      break;
    }
    case OP.CLIP_SET: {
      // RAW request [u32 fmt][u32 last][u32 off][bytes...] (see the OP
      // table). off 0 opens a fresh per-pcb staging buffer; each chunk must
      // land exactly at the staged length; last commits to the one slot.
      var cs = req.raw;
      if (!cs || cs.length < 12) { this._respond(pcb, { errno: 'EFAULT' }); break; }
      var cdv = new DataView(cs.buffer, cs.byteOffset, cs.length);
      var cfmt = cdv.getUint32(0, true);
      var clast = cdv.getUint32(4, true);
      var coff = cdv.getUint32(8, true);
      if (coff === 0) pcb.clipStage = { fmt: cfmt, parts: [], len: 0 };
      var stage = pcb.clipStage;
      if (!stage || stage.fmt !== cfmt || coff !== stage.len) {
        pcb.clipStage = null;
        this._respond(pcb, { errno: 'EINVAL' });
        break;
      }
      var cbytes = cs.subarray(12);              // readPayload copied already
      if (cbytes.length) { stage.parts.push(cbytes); stage.len += cbytes.length; }
      if (clast) {
        var joined = new Uint8Array(stage.len);
        for (var cpi = 0, cpo = 0; cpi < stage.parts.length; cpi++) {
          joined.set(stage.parts[cpi], cpo);
          cpo += stage.parts[cpi].length;
        }
        // fmt 0 (or an empty commit) clears the slot — EmptyClipboard.
        this._clipboard = (cfmt && joined.length) ? { fmt: cfmt, bytes: joined } : null;
        pcb.clipStage = null;
      }
      this._respond(pcb, {});
      // Host-clipboard bridge (ticket #79): report the commit AFTER the
      // reply — the copying process is never held hostage by the mirror.
      if (clast && this._onClipboard) this._onClipboard(this._clipboard);
      break;
    }
    case OP.CLIP_GET: {
      // JSON {fmt, off, peek} -> RAW [i32 total][chunk]; total -1 =
      // unavailable. peek 1 (SDL_HasClipboardText / fo_clip_has — menu
      // graying, Paste gates) ALWAYS serves the cached slot: an
      // availability probe must never trigger a host clipboard read (on
      // iOS that would raise the paste callout per menu open). A DATA read
      // at off 0 defers through onClipRead when the embedder registered
      // one — see the hook comment in the ctor; chunk continuations
      // (off > 0) never re-refresh mid-read.
      var gfmt = req.fmt | 0, goff = req.off | 0;
      if (this._onClipRead && !req.peek && goff === 0) {
        var cw = { op: 'clipget' };
        pcb.waiter = cw;
        var cself = this;
        var cdone = function () {
          // The waiter re-check every deferred service does: the process
          // may have exited while parked (waiter dropped at _exitProcess),
          // and a buggy embedder may call done twice.
          if (pcb.waiter !== cw) return;
          cself._cancelWaiter(pcb);
          cself._clipServe(pcb, gfmt, goff);
        };
        // The consumer's pid rides along (#562): the embedder's freshness
        // window is per-CONSUMER — a size-then-read pair from one process
        // shares one refresh, while a DIFFERENT process's first read always
        // re-reads the host clipboard (first paste fresh by construction).
        try { this._onClipRead(cdone, pcb.pid); }
        catch (e) {
          this._log('onClipRead threw: ' + (e && e.message));
          cdone();   // never wedge the parked reader on a broken hook
        }
        break;
      }
      this._clipServe(pcb, gfmt, goff);
      break;
    }
    case OP.EGRESS: {
      this._egress(pcb, req.raw || null);
      break;
    }
    case OP.COMPILE: {
      if (req.chunked && req.argv === null) {
        this._compileOutput(pcb);
        break;
      }
      pcb.compileOutput = null;
      if (!this._compile) { this._respond(pcb, { errno: 'ENOSYS' }); break; }
      var abort = new AbortController();
      var waiter = { op: 'compile', abort: abort };
      pcb.waiter = waiter;
      Promise.resolve().then(function () {
        return self._compile(req.argv || [], req.cwd || '/', { signal: abort.signal, pid: pcb.pid });
      }).then(function (r) {
        if (pcb.waiter !== waiter) return; // interrupted or exited; ignore late results
        pcb.waiter = null;
        if (req.chunked && r && !r.errno) {
          pcb.compileOutput = { result: r, out: 0, err: 0 };
          self._compileOutput(pcb);
        } else self._respond(pcb, r || { errno: 'EIO' });
      }, function (e) {
        if (pcb.waiter !== waiter) return;
        pcb.waiter = null;
        self._log('compile hook threw: ' + (e && e.message));
        self._respond(pcb, { errno: 'EIO' });
      });
      break;
    }
    default:
      if ((op & 0xff00) === 0x0400) { this._fsRpc(pcb, op, req); break; }
      if ((op & 0xff00) === 0x0500) { this._sockRpc(pcb, op, req); break; }
      if ((op & 0xff00) === 0x0600) { this._httpRpc(pcb, op, req); break; }
      if ((op & 0xf000) === 0x2000) { this._audioRpc(pcb, op, req); break; }
      this._respond(pcb, { errno: 'ENOSYS' });
  }
};

/* Serve a CLIP_GET reply from the slot as it stands NOW — the shared tail
 * of the synchronous path and the deferred-refresh resume. */
Kernel.prototype._clipServe = function (pcb, gfmt, goff) {
  var clip = this._clipboard;
  var total = (clip && clip.fmt === gfmt) ? clip.bytes.length : -1;
  var chunk = (total > 0 && goff >= 0 && goff < total)
    ? clip.bytes.subarray(goff, goff + Math.min(total - goff, KP_PAYLOAD_CAP - 4))
    : new Uint8Array(0);
  var gresp = new Uint8Array(4 + chunk.length);
  new DataView(gresp.buffer).setInt32(0, total, true);
  gresp.set(chunk, 4);
  this._respondRaw(pcb, gresp);
};

/* ---- Egress (docs/archive/0398): the OP.EGRESS materializer + store-only zip ----
 * The wire carries paths; bytes exist only on the kernel->embedder leg. The
 * whole flow is synchronous inside one dispatch turn (OPFS SyncAccessHandle
 * reads are sync in the kernel worker; EGRESS_MAX bounds the blocked time),
 * so the pre-read lstat walk and the read phase cannot race a mutation. */

var EGRESS_MAX = 256 * 1024 * 1024;   // lstat-summed refusal cap, decided
                                      // BEFORE any byte is read (EFBIG). 2x
                                      // the DROP_MAX ingress precedent: a zip
                                      // of a seeded game dir plausibly tops
                                      // 128 MB. If mobile memory pressure
                                      // ever bites, this constant moves — no
                                      // wire change (docs/archive/0398 design).
var EG_REQ_MAX = 8192 + 16;           // os/egress.h EG_MAX path-list cap plus
                                      // the disposition header word (E2BIG)
var EG_ZIP_MAX_ENTRIES = 65535;       // the pre-zip64 EOCD u16 entry count —
                                      // more entries can't be represented, so
                                      // refuse as EFBIG rather than truncate

/* Abort the walk with an errno (caught at the _egress dispatch). */
function egErr(errno) {
  var e = new Error('egress: ' + errno);
  e.egErrno = errno;
  throw e;
}

/* Artifact naming: dropFile's basename/control-strip sanitize (see
 * kernel-worker.js), inverted for the host direction. */
function egName(p) {
  var name = String(p || '').split('/').pop()
    .replace(/[\x00-\x1f\x7f]/g, '').trim();
  return (!name || name === '.' || name === '..') ? 'egress' : name;
}

/* CRC-32 (the zip polynomial), table built on first use. */
var CRC32_TAB = null;
function crc32(bytes) {
  if (!CRC32_TAB) {
    CRC32_TAB = new Int32Array(256);
    for (var n = 0; n < 256; n++) {
      var c = n;
      for (var k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
      CRC32_TAB[n] = c;
    }
  }
  var crc = -1;
  for (var i = 0; i < bytes.length; i++)
    crc = CRC32_TAB[(crc ^ bytes[i]) & 0xff] ^ (crc >>> 8);
  return (crc ^ -1) >>> 0;
}

/* MS-DOS date/time pair from epoch seconds (UTC — deterministic across
 * hosts); the format starts at 1980, earlier stamps clamp to 1980-01-01. */
function egDosStamp(sec) {
  var d = new Date((sec || 0) * 1000);
  if (d.getUTCFullYear() < 1980) return { date: (1 << 5) | 1, time: 0 };
  return {
    date: ((d.getUTCFullYear() - 1980) << 9) | ((d.getUTCMonth() + 1) << 5) | d.getUTCDate(),
    time: (d.getUTCHours() << 11) | (d.getUTCMinutes() << 5) | (d.getUTCSeconds() >> 1),
  };
}

/* Store-only zip writer. entries: [{name, mode, mtime, data}] — a name
 * ending '/' is a directory entry (empty data), a symlink entry's data is
 * its target (the Info-ZIP convention: Unix mode in the central directory's
 * external attributes, S_IFLNK marks the entry — macOS and Linux unzip both
 * restore it as a link). Deflate would be an INTERNAL upgrade here — the
 * wire and the onEgress hook shape would not change (docs/archive/0398 design). */
function zipStore(entries) {
  var locParts = [], cenParts = [], offset = 0;
  for (var i = 0; i < entries.length; i++) {
    var e = entries[i];
    var nameBytes = textEncoder.encode(e.name);
    var data = e.data || new Uint8Array(0);
    var crc = crc32(data);
    var stamp = egDosStamp(e.mtime);
    var isDir = e.name.charCodeAt(e.name.length - 1) === 0x2f;
    var loc = new Uint8Array(30 + nameBytes.length);
    var lv = new DataView(loc.buffer);
    lv.setUint32(0, 0x04034b50, true);        // local file header
    lv.setUint16(4, 20, true);                // version needed
    lv.setUint16(6, 0x0800, true);            // flags: UTF-8 names
    lv.setUint16(8, 0, true);                 // method: store
    lv.setUint16(10, stamp.time, true);
    lv.setUint16(12, stamp.date, true);
    lv.setUint32(14, crc, true);
    lv.setUint32(18, data.length, true);      // csize == usize (store)
    lv.setUint32(22, data.length, true);
    lv.setUint16(26, nameBytes.length, true);
    lv.setUint16(28, 0, true);                // extra len
    loc.set(nameBytes, 30);
    var cen = new Uint8Array(46 + nameBytes.length);
    var cv = new DataView(cen.buffer);
    cv.setUint32(0, 0x02014b50, true);        // central directory header
    cv.setUint16(4, 0x031E, true);            // made by: Unix, spec 3.0
    cv.setUint16(6, 20, true);
    cv.setUint16(8, 0x0800, true);
    cv.setUint16(10, 0, true);
    cv.setUint16(12, stamp.time, true);
    cv.setUint16(14, stamp.date, true);
    cv.setUint32(16, crc, true);
    cv.setUint32(20, data.length, true);
    cv.setUint32(24, data.length, true);
    cv.setUint16(28, nameBytes.length, true);
    // extra/comment/disk/internal-attr words stay 0
    // External attrs: the Unix mode word high, the DOS dir bit low.
    cv.setUint32(38, ((e.mode >>> 0) << 16 | (isDir ? 0x10 : 0)) >>> 0, true);
    cv.setUint32(42, offset, true);           // local header offset
    cen.set(nameBytes, 46);
    locParts.push(loc, data);
    cenParts.push(cen);
    offset += loc.length + data.length;
  }
  var cenSize = 0;
  for (var c2 = 0; c2 < cenParts.length; c2++) cenSize += cenParts[c2].length;
  var eocd = new Uint8Array(22);
  var ev = new DataView(eocd.buffer);
  ev.setUint32(0, 0x06054b50, true);
  ev.setUint16(8, entries.length, true);      // entries this disk
  ev.setUint16(10, entries.length, true);     // entries total
  ev.setUint32(12, cenSize, true);
  ev.setUint32(16, offset, true);             // central dir offset
  var out = new Uint8Array(offset + cenSize + 22);
  var pos = 0;
  for (var p1 = 0; p1 < locParts.length; p1++) { out.set(locParts[p1], pos); pos += locParts[p1].length; }
  for (var p2 = 0; p2 < cenParts.length; p2++) { out.set(cenParts[p2], pos); pos += cenParts[p2].length; }
  out.set(eocd, pos);
  return out;
}

Kernel.prototype._egress = function (pcb, raw) {
  // ENOSYS is decided FIRST: no embedder hook (or no kernel fs) means
  // egress has no host side at all — fail loud, never half-accept.
  if (!this._onEgress || !this._fs) { this._respond(pcb, { errno: 'ENOSYS' }); return; }
  if (!raw || !raw.length) { this._respond(pcb, { errno: 'EINVAL' }); return; }
  if (raw.length > EG_REQ_MAX) { this._respond(pcb, { errno: 'E2BIG' }); return; }
  var lines = textDecoder.decode(raw).split('\n');
  var dispo = lines[0];
  // 'clipboard' stays RESERVED (see the OP table) — EINVAL until the web
  // platform grows a file-flavored clipboard write.
  if (dispo !== 'download' && dispo !== 'saveas') { this._respond(pcb, { errno: 'EINVAL' }); return; }
  var paths = [];
  for (var i = 1; i < lines.length; i++) {
    if (lines[i] === '') continue;                 // trailing newline
    if (lines[i].charCodeAt(0) !== 0x2f) { this._respond(pcb, { errno: 'EINVAL' }); return; }
    paths.push(lines[i]);
  }
  if (paths.length === 0) { this._respond(pcb, { errno: 'EINVAL' }); return; }
  var art;
  try { art = this._egressMaterialize(paths); }
  catch (e) {
    if (!e || !e.egErrno) this._log('egress materialize threw: ' + (e && e.message));
    this._respond(pcb, { errno: (e && e.egErrno) || 'EIO' });
    return;
  }
  // The artifact is handed over BEFORE the reply: errno 0 means "accepted,
  // materialized, AND delivered to the embedder". Only the page-side act
  // itself can fail later (a blocked download), and the page logs that.
  try { this._onEgress(dispo, art.name, art.bytes); }
  catch (e2) {
    this._log('onEgress threw: ' + (e2 && e2.message));
    this._respond(pcb, { errno: 'EIO' });
    return;
  }
  this._respond(pcb, {});
};

/* Whole-file read on the kernel-side fs (size known from the pre-read walk;
 * same dispatch turn, so the size cannot have moved). */
Kernel.prototype._egressReadFile = function (abs, size) {
  var fs = this._fs;
  var fd = fs.open(abs, 0, 0);
  if (fd === null) egErr(fs._lastError || 'EIO');
  var buf = new Uint8Array(size), off = 0;
  while (off < buf.length) {
    var n = fs.read(fd, buf.subarray(off), buf.length - off);
    if (n === null) { fs.close(fd); egErr(fs._lastError || 'EIO'); }
    if (n === 0) break;
    off += n;
  }
  fs.close(fd);
  return off === buf.length ? buf : buf.subarray(0, off);
};

/* Walk one root into zip-entry METADATA — lstat-driven, no file bytes yet
 * (EFBIG comes from summed lstat sizes before any read). Symlinks inside a
 * tree are preserved as symlink ENTRIES (fo_copy's copy-as-link, os/
 * fileops.h, carried across the boundary); directories get explicit
 * entries so empty ones survive; entry order is sorted for deterministic
 * zip bytes. */
Kernel.prototype._egressWalk = function (abs, zipPath, list, state) {
  var fs = this._fs;
  var st = fs.lstat(abs);
  if (st === null) egErr(fs._lastError || 'ENOENT');
  var fmt = st.mode & 0o170000;
  if (list.length >= EG_ZIP_MAX_ENTRIES) egErr('EFBIG');
  if (fmt === 0o120000) {                        // symlink -> symlink entry
    var lbuf = new Uint8Array(4096);
    var n = fs.readlink(abs, lbuf, lbuf.length);
    if (n === null) egErr(fs._lastError || 'EIO');
    state.total += n;
    if (state.total > EGRESS_MAX) egErr('EFBIG');
    list.push({ zipPath: zipPath, kind: 'link', mode: st.mode, mtime: st.mtime,
                target: lbuf.slice(0, n) });
    return;
  }
  if (fmt === 0o040000) {                        // directory
    list.push({ zipPath: zipPath + '/', kind: 'dir', mode: st.mode, mtime: st.mtime });
    var dh = fs.opendir(abs);
    if (dh === null) egErr(fs._lastError || 'EIO');
    var names = [];
    for (var ent = fs.readdir(dh); ent !== null; ent = fs.readdir(dh)) {
      if (ent.name === '.' || ent.name === '..') continue;
      names.push(ent.name);
    }
    fs.closedir(dh);
    names.sort();
    for (var i = 0; i < names.length; i++)
      this._egressWalk(abs + '/' + names[i], zipPath + '/' + names[i], list, state);
    return;
  }
  if (fmt !== 0o100000) egErr('EINVAL');         // nothing else crosses
  state.total += st.size;
  if (state.total > EGRESS_MAX) egErr('EFBIG');
  list.push({ zipPath: zipPath, kind: 'file', mode: st.mode, mtime: st.mtime,
              abs: abs, size: st.size });
};

/* paths -> ONE artifact {name, bytes}. A single regular file crosses as
 * itself; a single directory as <basename>.zip; N>1 paths as
 * gucOS-selection.zip. A LONE symlink follows to its target (downloading a
 * Desktop launcher means "give me the thing"; dangling -> ENOENT) — inside
 * a zip, symlinks stay symlinks. */
Kernel.prototype._egressMaterialize = function (paths) {
  var fs = this._fs;
  var roots = [];                                // [{abs, root}] zip roots
  if (paths.length === 1) {
    var abs = paths[0];
    var st = fs.stat(abs);                       // FOLLOWS the lone symlink
    if (st === null) egErr(fs._lastError || 'ENOENT');
    var fmt1 = st.mode & 0o170000;
    if (fmt1 === 0o100000) {
      if (st.size > EGRESS_MAX) egErr('EFBIG');
      return { name: egName(abs), bytes: this._egressReadFile(abs, st.size) };
    }
    if (fmt1 !== 0o040000) egErr('EINVAL');
    // One dir -> <basename>.zip. Resolve through a lone symlink so the walk
    // archives the TARGET tree, but keep the artifact's name from the link.
    var real;
    try { real = fs.realpathPhysical(abs); } catch (e) { real = null; }
    if (real === null) egErr(fs._lastError || 'ENOENT');
    roots.push({ abs: real, root: egName(abs) });
  } else {
    // Selected roots keep their own basenames; a clash between two roots
    // gets the dropFile '-N' suffix (zip member names must not collide).
    var used = Object.create(null);
    for (var i = 0; i < paths.length; i++) {
      var base = egName(paths[i]);
      var root = base;
      for (var s = 1; used[root]; s++) root = base + '-' + s;
      used[root] = true;
      roots.push({ abs: paths[i], root: root });
    }
  }
  var list = [], state = { total: 0 };
  for (var r = 0; r < roots.length; r++)
    this._egressWalk(roots[r].abs, roots[r].root, list, state);
  var entries = [];
  for (var j = 0; j < list.length; j++) {
    var it = list[j];
    entries.push({
      name: it.zipPath, mode: it.mode, mtime: it.mtime,
      data: it.kind === 'file' ? this._egressReadFile(it.abs, it.size)
        : it.kind === 'link' ? it.target : null,
    });
  }
  var name = paths.length === 1 ? egName(paths[0]) + '.zip' : 'gucOS-selection.zip';
  return { name: name, bytes: zipStore(entries) };
};

Kernel.prototype._respondRaw = function (pcb, bytes) {
  if (pcb.trace && pcb.trace.cur) this._traceRpc(pcb, null, bytes);
  if (!writeRawPayload(pcb.i32, pcb.u8, bytes)) {
    writePayload(pcb.i32, pcb.u8, { errno: 'ENOMEM' });
  }
  Atomics.store(pcb.i32, KP_RPC_STATE, RPC_DONE);
  this._ring(pcb);
};

/* ---- the brokered filesystem (0x04xx) ----
 * One BlockFS instance (this._fs) serves every process; per-process fd maps
 * point at shared open file descriptions. A 'file' OFD is backed by one
 * BlockFS fd of the kernel instance — its position IS the shared offset, so
 * dup/inheritance get POSIX open-file-description semantics for free, and
 * BlockFS's tested unlink-while-open refcounts become system-global. */
/* One page of directory entries under the payload cap (docs/archive/0241).
 * FS_OPENDIR reads the first page; when the NEXT entry would push the
 * measured JSON bytes past KP_DIR_PAGE, the open backend handle parks in
 * pcb.dirRpc behind a cursor id (reply `more`) with that entry carried as
 * `pending` — nothing is re-read or lost — and FS_READDIR {dir} continues
 * it. The final page carries no `more` and closes the handle, so the
 * ENOMEM degrade at _respond is unreachable for directory listings while
 * the general oversize guard stays for every other op. Handles are held
 * per-backend (BlockFS/ProcFS/MountFS all keep stateful dir handles), so
 * pagination covers every volume kind with zero backend change. Release
 * points: exhaustion here, a bad-cursor EBADF, and _exitProcess (a client
 * that dies mid-drain leaks nothing — the fd discipline). */
Kernel.prototype._readdirPage = function (pcb, dh, pending) {
  var fs = this._fs;
  var entries = [], bytes = 0;
  var ent = pending !== null ? pending : fs.readdir(dh);
  while (ent !== null) {
    var e = { ino: ent.ino, type: ent.type, name: ent.name };
    // Measured, not estimated: names are UTF-8 and JSON-escaped. The
    // entries.length guard keeps forward progress if a single entry ever
    // exceeded the budget (impossible at NAME_MAX-scale names, but a wedge
    // is worse than one oversize page attempt).
    var cost = textEncoder.encode(JSON.stringify(e)).length + 1;
    if (bytes + cost > KP_DIR_PAGE && entries.length > 0) {
      if (pcb.dirRpc === null) pcb.dirRpc = new Map();
      var cur = pcb.dirRpcNext++;
      pcb.dirRpc.set(cur, { dh: dh, pending: e });
      return { entries: entries, more: cur };
    }
    entries.push(e);
    bytes += cost;
    ent = fs.readdir(dh);
  }
  fs.closedir(dh);
  return { entries: entries };
};

// Named and openpty-created terminals share the same data plane and lifetime.
Kernel.prototype._newPty = function (locked) {
  var self = this;
  var out = {buf:[],cap:PTY_OUT_CAP,rOpen:true,wOpen:true,readWaiters:[],writeWaiters:[]};
  var tty = new Tty(this,{output:function(bytes) {
    if (!out.rOpen || !out.wOpen) return;
    for (var i=0;i<bytes.length;i++) out.buf.push(bytes[i]);
    self._pipeNotify(out);
  }});
  tty._brokered = true;
  var pt = {id:this._nextPty++,out:out,tty:tty,locked:locked,slaves:0};
  tty.pty = pt;
  this._ptys.set(pt.id,pt);
  return pt;
};
Kernel.prototype._ptySlaveOfd = function(pt) {
  pt.slaves++;pt.out.wOpen=true;
  return this._makeOfd('tty',{tty:pt.tty,pty:pt,accmode:2});
};

Kernel.prototype._fsRpc = function (pcb, op, req) {
  try { return this._fsRpcBody(pcb,op,req); }
  finally { if(this._readCache)this._readCache.resume(); }
};
Kernel.prototype._fsRpcBody = function (pcb, op, req) {
  var self = this;
  var fs = this._fs;
  if (!fs) { this._respond(pcb, { errno: 'ENOSYS' }); return; }
  var eFs = function () { return { errno: fs._lastError || 'EIO' }; };
  var ofdOf = function (fd) {
    var id = pcb.fds.get(fd | 0);
    return id === undefined ? null : self._ofds.get(id) || null;
  };
  var allocFd = function (min) {
    var fd = min | 0;
    while (pcb.fds.has(fd)) fd++;
    return fd;
  };
  var P = function (p) { return self._pathFor(pcb, p); };
  var r;

  switch (op) {
    case OP.FS_OPEN: {
      // FS_WATCH bookkeeping: record the path on every file OFD (cheap —
      // a string; the settle emit canonicalizes lazily, gated on live
      // watches), pre-stat O_CREAT existence only when watches are live
      // (the parent-dir CREATE event), and mark O_TRUNC / fresh-create
      // dirty so `echo -n > file` — truncate or create, zero writes —
      // still settles at close. The O_TRUNC test is deliberately on the
      // FLAG, not on the access mode: O_RDONLY|O_TRUNC really does empty
      // the file here (the Linux answer — see BlockFS.open's #641 block),
      // so it owes a watcher the same settle a writable truncate does.
      var oAbs = P(req.path);
      var special = null;
      if (oAbs === '/dev/ptmx') {
        if ((req.flags & 3) !== 2) { this._respond(pcb,{errno:'EINVAL'});return; }
        special = this._makeOfd('ptm',{pty:this._newPty(true),accmode:2});
      } else if (/^\/dev\/pts\/[0-9]+$/.test(oAbs)) {
        var pt = this._ptys.get(Number(oAbs.slice(9)));
        if (!pt) { this._respond(pcb,{errno:'ENOENT'});return; }
        if (pt.locked) { this._respond(pcb,{errno:'EIO'});return; }
        special = this._ptySlaveOfd(pt);
        if (!(req.flags & 0x100) && pcb.sid === pcb.pid && !pcb.tty && !pt.tty.session) {
          pcb.tty=pt.tty;pt.tty.session=pcb.sid;pt.tty.fgPgid=pcb.pgid;
        }
      } else if (oAbs === '/dev/tty') {
        if (!pcb.tty) { this._respond(pcb,{errno:'ENXIO'});return; }
        special = pcb.tty.pty ? this._ptySlaveOfd(pcb.tty.pty) : this._makeOfd('tty',{tty:pcb.tty});
      }
      if (special) {
        special.accmode=req.flags&3;special.statusFlags=req.flags&(3|0x800);
        special.refs++;
        var tfd=allocFd(0);pcb.fds.set(tfd,special.id);pcb.fdFlags.set(tfd,(req.flags&0x80000)?1:0);
        this._respond(pcb,{fd:tfd});return;
      }

      var oExisted = true;
      if (this._watches.size && (req.flags & 0x40)) oExisted = fs.stat(oAbs) !== null;
      var bfsFd = fs.open(oAbs, req.flags | 0, req.mode | 0);
      if (bfsFd === null) { this._respond(pcb, eFs()); return; }
      // accmode (docs/archive/0376): the OFD carries flags & O_ACCMODE; FS_READ/
      // FS_WRITE enforce it. FS_DUP/FS_DUP2/spawn DUP2 share the OFD, so
      // the mode rides every duplicate (POSIX open-file-description rule).
      var o = this._makeOfd('file', { bfsFd: bfsFd, path: oAbs,
                                      accmode: req.flags & 3, statusFlags: req.flags & (3 | 0x400 | 0x800) });
      var stO = fs.fstat(bfsFd);
      o.ino = stO ? stO.ino : 0;   // record-lock and mapping identity
      if (this._mapped && (req.flags & 0x200)) this._mapped.afterWrite(o.ino);
      if ((req.flags & 0x200) || !oExisted) o.dirty = true;
      o.refs++;
      var fd = allocFd(0);
      pcb.fds.set(fd, o.id);pcb.fdFlags.set(fd,(req.flags & 0x80000)?1:0);
      if (!oExisted) this._watchEmit(oAbs, FSW_CREATE, FSW_CREATE, false);
      var readCache=this._readCache&&req.readCache&&!pcb.trace?this._readCache.attach(o,stO):null;
      if(this._readCache)this._readCache.resume();
      this._respond(pcb, { fd: fd, readCache: readCache });
      return;
    }
    case OP.FS_CLOSE: {
      var id = pcb.fds.get(req.fd | 0);
      if (id === undefined) { this._respond(pcb, { errno: 'EBADF' }); return; }
      this._ofdUnref(id, pcb.pid);
      pcb.fds.delete(req.fd | 0);pcb.fdFlags.delete(req.fd | 0);
      this._respond(pcb, {});
      return;
    }
    case OP.FS_READ: {
      var o1 = ofdOf(req.fd);
      if (!o1) { this._respond(pcb, { errno: 'EBADF' }); return; }
      var count = Math.min(req.count | 0, KP_PAYLOAD_CAP);
      if (o1.kind === 'file') {
        // Access mode (docs/archive/0376): O_WRONLY can't read. Belt-and-braces
        // with the same check in BlockFS — the kernel OFD is the layer a
        // non-BlockFS embedder fs would rely on.
        if (o1.accmode === 1) { this._respond(pcb, { errno: 'EBADF' }); return; }
        var buf = new Uint8Array(count);
        if (this._mapped) this._mapped.beforeIO(o1.ino);   // mapped writes are visible to read()
        var readStart=o1.readCache?o1.readCache.cap.entry.position:0;
        var n = fs.read(o1.bfsFd, buf, count);
        if (n === null) { this._respond(pcb, eFs()); return; }
        if(this._readCache){this._readCache.fill(o1,readStart,count,buf,n);this._readCache.resume();}
        this._respondRaw(pcb, buf.subarray(0, n));
        return;
      }
      if (o1.kind === 'null') { this._respondRaw(pcb, new Uint8Array(0)); return; }
      if (o1.kind === 'watch') {
        // FS_WATCH drain: whole packed records only. Empty queue is EAGAIN
        // — watch fds are inherently non-blocking; the contract is
        // WAIT/select first, then drain until EAGAIN, then act once.
        var wrec = this._watchDrain(o1, count);
        if (wrec === null) { this._respond(pcb, { errno: 'EAGAIN' }); return; }
        if (wrec === false) { this._respond(pcb, { errno: 'EINVAL' }); return; }
        this._respondRaw(pcb, wrec);
        return;
      }
      if (o1.kind === 'http') {
        // HTTP body drain (docs/archive/0417): bytes when queued, 0 at clean EOF,
        // the error when failed, EAGAIN when dry — NEVER a park. http fds
        // are inherently non-blocking like watch fds: __wait doesn't name
        // the ready fd, so a woken caller finds the ready transfer by
        // trying, and try-read-until-EAGAIN is that discipline. A drain
        // below the cap resumes a backpressure-paused fetch reader.
        var hx = o1.xfer;
        if (hx.bytes > 0) {
          this._respondRaw(pcb, this._httpDrain(hx, count));
          if (hx.paused && hx.bytes < hx.cap) this._httpPump(hx);
          return;
        }
        if (hx.error !== null) { this._respond(pcb, { errno: hx.errno || 'EIO', error: hx.error }); return; }
        if (hx.done) { this._respondRaw(pcb, new Uint8Array(0)); return; }   // clean EOF
        this._respond(pcb, { errno: 'EAGAIN' });
        return;
      }
      if (o1.kind === 'pipe') {
        if (o1.end !== 'read') { this._respond(pcb, { errno: 'EBADF' }); return; }
        this._streamRead(pcb, o1.pipe, count, o1.statusFlags & 0x800);
        return;
      }
      if (o1.kind === 'socket') {
        if (o1.st !== 'conn') { this._respond(pcb, { errno: 'ENOTCONN' }); return; }
        this._streamRead(pcb, o1.rx, count, o1.statusFlags & 0x800);
        return;
      }
      if (o1.kind === 'ptm') {
        // Master read: post-line-discipline output + echo, pipe-shaped.
        this._streamRead(pcb, o1.pty.out, count, o1.statusFlags & 0x800);
        return;
      }
      if (o1.kind === 'tty') {
        var tty = o1.tty || pcb.tty;
        if (!tty) { this._respondRaw(pcb, new Uint8Array(0)); return; }
        // POSIX read(fd, buf, 0): return 0 immediately, no data transfer, no
        // parking (0253) — and before the job-control check, since a
        // zero-length read moves no bytes (the simple conforming choice).
        if (count === 0) { this._respondRaw(pcb, new Uint8Array(0)); return; }
        // Job control (docs/archive/0003): a background pgroup reading the tty gets
        // SIGTTIN (stop class); if it's ignored or blocked, POSIX says the
        // read fails with EIO instead. The read itself returns EINTR — after
        // SIGCONT the libc caller retries, now (typically) in the foreground.
        if (tty.fgPgid > 0 && pcb.pgid !== tty.fgPgid) {
          if (pcb.sigdisp[SIG.TTIN] === DISP_IGN ||
              (Atomics.load(pcb.i32, KP_SIGBLOCK) & (1 << (SIG.TTIN - 1)))) {
            this._respond(pcb, { errno: 'EIO' });
            return;
          }
          this._killPgid(pcb.pgid, SIG.TTIN);
          this._respond(pcb, { errno: 'EINTR' });
          return;
        }
        if (tty._cooked.length > 0) { this._respondRaw(pcb, tty.take(count)); return; }
        if (tty._eofFlag || tty._hupFlag) { tty._consumeEof(); this._respondRaw(pcb, new Uint8Array(0)); return; }
        if (o1.statusFlags & 0x800) { this._respond(pcb, {errno:'EAGAIN'}); return; }
        pcb.waiter = { op: 'ttyread', tty: tty, count: count };   // served by _ttyNotify
        tty.waiters.push(pcb.cid);
        return;
      }
      this._respond(pcb, { errno: 'EBADF' });           // 'out'
      return;
    }
    case OP.FS_WRITE: {
      // Raw request: [u32 fd][bytes...]
      var rawq = req.raw;
      if (!rawq || rawq.length < 4) { this._respond(pcb, { errno: 'EFAULT' }); return; }
      var wfd = rawq[0] | (rawq[1] << 8) | (rawq[2] << 16) | (rawq[3] << 24);
      var data = rawq.subarray(4);
      var o2 = ofdOf(wfd);
      if (!o2) { this._respond(pcb, { errno: 'EBADF' }); return; }
      if (o2.kind === 'file') {
        // Access mode (docs/archive/0376): O_RDONLY can't write — the corruption
        // half; a read-only fd used to silently mutate the file.
        if (o2.accmode === 0) { this._respond(pcb, { errno: 'EBADF' }); return; }
        if (this._mapped) this._mapped.beforeIO(o2.ino);
        var wn = fs.write(o2.bfsFd, data, data.length);
        if (wn === null) { this._respond(pcb, eFs()); return; }
        if (wn > 0) {
          if (this._mapped) this._mapped.afterWrite(o2.ino);   // mapped pages show the written bytes
          o2.dirty = true;               // FS_WATCH: settles at last close
          if (this._watches.size) this._watchEmit(o2.path, FSW_MODIFY, FSW_MODIFY, false);
        }
        this._respond(pcb, { n: wn });
        return;
      }
      if (o2.kind === 'watch' || o2.kind === 'http') { this._respond(pcb, { errno: 'EBADF' }); return; }
      if (o2.kind === 'pipe') {
        if (o2.end !== 'write') { this._respond(pcb, { errno: 'EBADF' }); return; }
        this._streamWrite(pcb, o2.pipe, data, o2.statusFlags & 0x800);
        return;
      }
      if (o2.kind === 'socket') {
        if (o2.st !== 'conn') { this._respond(pcb, { errno: 'ENOTCONN' }); return; }
        this._streamWrite(pcb, o2.tx, data, o2.statusFlags & 0x800);
        return;
      }
      if (o2.kind === 'out') { this._onOutput(pcb.pid, o2.ch, data.slice()); this._respond(pcb, { n: data.length }); return; }
      if (o2.kind === 'ptm') {
        // Master write = terminal keystrokes: feed the slave's line
        // discipline (echo/signals/canonical editing for free). Input
        // never blocks — an overfull cooked queue drops, like a real tty.
        o2.pty.tty.input(data);
        this._respond(pcb, { n: data.length });
        return;
      }
      if (o2.kind === 'tty') {
        if (o2.pty) { this._ptySlaveWrite(pcb, o2.pty, data, o2.statusFlags & 0x800); return; }
        this._onOutput(pcb.pid, o2.ch || 1, data.slice());
        this._respond(pcb, { n: data.length });
        return;
      }
      this._respond(pcb, { n: data.length });           // 'null'
      return;
    }
    case OP.FS_PREAD:
    case OP.FS_PWRITE: {
      var positionedWrite = op === OP.FS_PWRITE;
      var pfd, poffset, pbuf, pcount;
      if (positionedWrite) {
        if (!req.raw || req.raw.length < 12) { this._respond(pcb,{errno:'EFAULT'}); return; }
        var pdv=new DataView(req.raw.buffer,req.raw.byteOffset,req.raw.byteLength);
        pfd=pdv.getInt32(0,true); poffset=pdv.getFloat64(4,true);
        pbuf=req.raw.subarray(12); pcount=pbuf.length;
      } else {
        pfd=req.fd; poffset=req.offset; pcount=Math.min(req.count>>>0,KP_FS_CHUNK); pbuf=new Uint8Array(pcount);
      }
      var pofd=ofdOf(pfd);
      if(!pofd){this._respond(pcb,{errno:'EBADF'});return;}
      if(pofd.kind!=='file'){this._respond(pcb,{errno:'ESPIPE'});return;}
      if(!Number.isSafeInteger(poffset)||poffset<0){this._respond(pcb,{errno:'EINVAL'});return;}
      if(positionedWrite ? pofd.accmode===0 : pofd.accmode===1){this._respond(pcb,{errno:'EBADF'});return;}
      if(this._mapped)this._mapped.beforeIO(pofd.ino);
      var pmethod=positionedWrite?'pwrite':'pread';
      if(!fs[pmethod]){this._respond(pcb,{errno:'ENOTSUP'});return;}
      var pn=fs[pmethod](pofd.bfsFd,pbuf,pcount,poffset);
      if(pn===null){this._respond(pcb,eFs());return;}
      if(positionedWrite){
        if(pn>0){
          if(this._mapped)this._mapped.afterWrite(pofd.ino);
          pofd.dirty=true;
          if(this._watches.size)this._watchEmit(pofd.path,FSW_MODIFY,FSW_MODIFY,false);
        }
        this._respond(pcb,{n:pn});
      } else this._respondRaw(pcb,pbuf.subarray(0,pn));
      return;
    }
    case OP.FS_LSEEK: {
      var o3 = ofdOf(req.fd);
      if (!o3) { this._respond(pcb, { errno: 'EBADF' }); return; }
      if (o3.kind !== 'file') { this._respond(pcb, { errno: 'ESPIPE' }); return; }
      r = fs.lseek(o3.bfsFd, req.offset, req.whence | 0);
      this._respond(pcb, r === null ? eFs() : { offset: r });
      return;
    }
    case OP.FS_STAT: r = fs.stat(P(req.path)); this._respond(pcb, r === null ? eFs() : { st: r }); return;
    case OP.FS_LSTAT: r = fs.lstat(P(req.path)); this._respond(pcb, r === null ? eFs() : { st: r }); return;
    case OP.FS_FSTAT: {
      var o4 = ofdOf(req.fd);
      if (!o4) { this._respond(pcb, { errno: 'EBADF' }); return; }
      if (o4.kind === 'file') {
        r = fs.fstat(o4.bfsFd);
        this._respond(pcb, r === null ? eFs() : { st: r });
      // L82 / #788: anonymous resources still share legacy (dev,ino)=(0,0).
      // Explicit metadata keeps serializers strict; this does not repair identity.
      } else if (o4.kind === 'pipe') {
        this._respond(pcb, { st: { dev: 0, ino: 0, mode: 0x1000 | 0o600, nlink: 1, size: this._pipeAvail(o4.pipe), atime: 0, mtime: 0, ctime: 0, rdev: 0 } });
      } else if (o4.kind === 'socket') {
        this._respond(pcb, { st: { dev: 0, ino: 0, mode: S_IFSOCK_MODE | 0o777, nlink: 1, size: o4.st === 'conn' ? o4.rx.buf.length : 0, atime: 0, mtime: 0, ctime: 0, rdev: 0 } });
      } else {
        // Character device (tty / console / null).
        this._respond(pcb, { st: { dev: 0, ino: 0, mode: 0x2000 | 0o666, nlink: 1, size: 0, atime: 0, mtime: 0, ctime: 0, rdev: 0 } });
      }
      return;
    }
    case OP.FS_ACCESS: r = fs.access(P(req.path), req.mode | 0); this._respond(pcb, r === null ? eFs() : {}); return;
    // The mutating arms below feed FS_WATCH after success — every runtime
    // fs mutation flows through this dispatch (the choke point), so one
    // emit per arm covers the whole system. All emits are gated on live
    // watches inside the helpers; an unwatched system pays nothing.
    case OP.FS_UNLINK: {
      var ulAbs = P(req.path);
      r = fs.unlink(ulAbs);
      if (r !== null) this._watchEmit(ulAbs, FSW_SELF_GONE, FSW_DELETE, false);
      this._respond(pcb, r === null ? eFs() : {});
      return;
    }
    case OP.FS_RENAME: {
      var rnFrom = P(req.from), rnTo = P(req.to);
      r = fs.rename(rnFrom, rnTo, req.noReplace === true);
      if (r !== null) this._watchRename(rnFrom, rnTo);
      this._respond(pcb, r === null ? eFs() : {});
      return;
    }
    case OP.FS_MKDIR: {
      var mkAbs = P(req.path);
      r = fs.mkdir(mkAbs, req.mode | 0);
      if (r !== null) this._watchEmit(mkAbs, FSW_CREATE, FSW_CREATE, true);
      this._respond(pcb, r === null ? eFs() : {});
      return;
    }
    case OP.FS_RMDIR: {
      var rdAbs = P(req.path);
      r = fs.rmdir(rdAbs);
      if (r !== null) this._watchEmit(rdAbs, FSW_SELF_GONE, FSW_DELETE, true);
      this._respond(pcb, r === null ? eFs() : {});
      return;
    }
    case OP.FS_LINK: {
      var lnTo = P(req.to);
      r = fs.link(P(req.from), lnTo);
      if (r !== null) this._watchEmit(lnTo, FSW_CREATE, FSW_CREATE, false);
      this._respond(pcb, r === null ? eFs() : {});
      return;
    }
    case OP.FS_SYMLINK: {
      var slAbs = P(req.path);
      r = fs.symlink(req.target, slAbs);
      if (r !== null) this._watchEmit(slAbs, FSW_CREATE, FSW_CREATE, false);
      this._respond(pcb, r === null ? eFs() : {});
      return;
    }
    case OP.FS_READLINK: {
      // BlockFS.readlink is buffer-style (POSIX shape); the RPC carries the
      // target as a string. PATH_MAX here is BlockFS's path component budget.
      var lbuf = new Uint8Array(4096);
      r = fs.readlink(P(req.path), lbuf, lbuf.length);
      this._respond(pcb, r === null ? eFs()
        : { target: new TextDecoder().decode(lbuf.subarray(0, r)) });
      return;
    }
    case OP.FS_FTRUNCATE: {
      var o5 = ofdOf(req.fd);
      if (!o5 || o5.kind !== 'file') { this._respond(pcb, { errno: 'EBADF' }); return; }
      // POSIX: ftruncate needs a fd open for writing (docs/archive/0376).
      if (o5.accmode === 0) { this._respond(pcb, { errno: 'EINVAL' }); return; }
      if (this._mapped) this._mapped.beforeIO(o5.ino);
      r = fs.ftruncate(o5.bfsFd, req.size | 0);
      if (r !== null) {
        if (this._mapped) this._mapped.afterWrite(o5.ino);
        o5.dirty = true;                 // FS_WATCH: settles at last close
        if (this._watches.size) this._watchEmit(o5.path, FSW_MODIFY, FSW_MODIFY, false);
      }
      this._respond(pcb, r === null ? eFs() : {});
      return;
    }
    case OP.FS_FSYNC: {
      // fsync/fdatasync: only file OFDs reach the store (whole-image flush —
      // fsync may flush more than asked); tty/pipe/socket/null are a
      // harmless 0, matching the in-process env's no-validation behavior.
      var oSync = ofdOf(req.fd);
      if (!oSync) { this._respond(pcb, { errno: 'EBADF' }); return; }
      if (oSync.kind === 'file' && this._mapped) this._mapped.beforeIO(oSync.ino);
      if (oSync.kind === 'file' && fs.fsync(oSync.bfsFd) === null) {
        this._respond(pcb, eFs());
        return;
      }
      this._respond(pcb, {});
      return;
    }
    case OP.FS_CHMOD: r = fs.chmod(P(req.path), req.mode | 0); this._respond(pcb, r === null ? eFs() : {}); return;
    case OP.FS_FCHMOD: {
      var o6 = ofdOf(req.fd);
      if (!o6 || o6.kind !== 'file') { this._respond(pcb, { errno: 'EBADF' }); return; }
      r = fs.fchmod(o6.bfsFd, req.mode | 0);
      this._respond(pcb, r === null ? eFs() : {});
      return;
    }
    case OP.FS_UTIME: r = fs.utime(P(req.path), req.atime, req.mtime); this._respond(pcb, r === null ? eFs() : {}); return;
    case OP.FS_FUTIME: {
      var o7 = ofdOf(req.fd);
      if (!o7 || o7.kind !== 'file') { this._respond(pcb, { errno: 'EBADF' }); return; }
      r = fs.futime(o7.bfsFd, req.atime, req.mtime);
      this._respond(pcb, r === null ? eFs() : {});
      return;
    }
    case OP.FS_CHDIR: {
      if (req.fd !== undefined) {
        var directory = ofdOf(req.fd);
        if (!directory) { this._respond(pcb,{errno:'EBADF'}); return; }
        if (directory.kind !== 'file') { this._respond(pcb,{errno:'ENOTDIR'}); return; }
        var directoryPath = fs.directoryPath(directory.bfsFd);
        if (directoryPath === null) { this._respond(pcb,eFs()); return; }
        pcb.cwd=directoryPath;
        this._respond(pcb,{}); return;
      }
      var abs = P(req.path);
      var resolved;
      try { resolved = fs._resolvePath(abs); } catch (e) { resolved = null; }
      if (!resolved) { this._respond(pcb, { errno: fs._lastError || 'ENOENT' }); return; }
      var st = fs.stat(resolved);
      if (st === null) { this._respond(pcb, eFs()); return; }
      if ((st.mode & 0xF000) !== 0x4000) { this._respond(pcb, { errno: 'ENOTDIR' }); return; }
      pcb.cwd = resolved;
      this._respond(pcb, {});
      return;
    }
    case OP.FS_GETCWD: this._respond(pcb, { cwd: pcb.cwd }); return;
    case OP.FS_DUP: {
      var o8 = ofdOf(req.fd);
      if (!o8) { this._respond(pcb, { errno: 'EBADF' }); return; }
      this._ofdRef(o8, pcb.pid);
      var dfd = allocFd(0);
      pcb.fds.set(dfd, o8.id);pcb.fdFlags.delete(dfd);
      this._respond(pcb, { fd: dfd });
      return;
    }
    case OP.FS_DUP2: {
      var o9 = ofdOf(req.fd);
      if (!o9) { this._respond(pcb, { errno: 'EBADF' }); return; }
      if ((req.fd | 0) !== (req.newfd | 0)) {
        this._ofdRef(o9, pcb.pid);
        var prev = pcb.fds.get(req.newfd | 0);
        if (prev !== undefined) this._ofdUnref(prev, pcb.pid);
        pcb.fds.set(req.newfd | 0, o9.id);pcb.fdFlags.delete(req.newfd | 0);
      }
      this._respond(pcb, { fd: req.newfd | 0 });
      return;
    }
    case OP.FS_SETFL:
    case OP.FS_GETFL: {
      const o = ofdOf(req.fd);
      if (!o) { this._respond(pcb, {errno:'EBADF'}); return; }
      let flags = o.statusFlags ?? o.accmode ??
        (o.kind === 'pipe' ? (o.end === 'write' ? 1 : 0) :
         o.ch !== undefined ? (o.ch === 0 ? 0 : 1) :
         ['tty','ptm','socket'].includes(o.kind) ? 2 : 0);
      if (op === OP.FS_SETFL) {
        // File status belongs to the OFD, shared by dup/fork and pthreads.
        flags = (flags & ~(0x400 | 0x800)) | (req.flags & (0x400 | 0x800));
        if (o.kind === 'file') {
          if (!fs.setfl) { this._respond(pcb, {errno:'ENOTSUP'}); return; }
          if (fs.setfl(o.bfsFd, flags) === null) { this._respond(pcb, eFs()); return; }
        }
        o.statusFlags = flags;
        // Fast pipe endpoints don't consult OFD flags. Keep nonblocking
        // pipes brokered; the promotion check also excludes these OFDs.
        if (o.kind === 'pipe' && (flags & 0x800)) this._pipeDemote(o.pipe);
      }
      this._respond(pcb, {flags}); return;
    }
    case OP.FS_FDFLAGS: {
      const fd=req.fd|0;
      if(!pcb.fds.has(fd)){this._respond(pcb,{errno:'EBADF'});return;}
      if(req.set)pcb.fdFlags.set(fd,req.flags&1);
      this._respond(pcb,{flags:pcb.fdFlags.get(fd)||0});return;
    }
    case OP.FS_FCNTL_LOCK: {
      // POSIX record locks (docs/MAPPINGS.md): cmd 5 F_GETLK, 6 F_SETLK, 7
      // F_SETLKW; type 0 read, 1 write, 2 unlock; ranges resolve against
      // the descriptor's offset or the file size for SEEK_CUR/SEEK_END.
      if (!this._locks) { this._respond(pcb, { errno: 'ENOSYS' }); return; }
      var oL = ofdOf(req.fd);
      if (!oL || oL.kind !== 'file') { this._respond(pcb, { errno: oL ? 'EINVAL' : 'EBADF' }); return; }
      var lType = req.type | 0, lWhence = req.whence | 0, lCmd = req.cmd | 0;
      if (lType < 0 || lType > 2 || lWhence < 0 || lWhence > 2 || lCmd < 5 || lCmd > 7) { this._respond(pcb, { errno: 'EINVAL' }); return; }
      var lBase = lWhence === 0 ? 0 : lWhence === 1 ? (fs.lseek(oL.bfsFd, 0, 1) || 0) : ((fs.fstat(oL.bfsFd) || {}).size || 0);
      var lStart = lBase + Number(req.start || 0), lLen = Number(req.len || 0);
      var lo = lLen >= 0 ? lStart : lStart + lLen, hi = lLen === 0 ? Infinity : (lLen > 0 ? lStart + lLen : lStart);
      if (!(lo >= 0) || !(hi > lo)) { this._respond(pcb, { errno: 'EINVAL' }); return; }
      var lock = { owner: pcb.pid, type: lType, start: lo, end: hi };
      if (lCmd === 5) {
        var cf = lType === 2 ? null : this._locks.conflict(oL.ino, lock);
        this._respond(pcb, cf ? { type: cf.type, start: cf.start, len: cf.end === Infinity ? 0 : cf.end - cf.start, pid: cf.owner } : { type: 2 });
        return;
      }
      if ((lType === 0 && oL.accmode === 1) || (lType === 1 && oL.accmode === 0)) { this._respond(pcb, { errno: 'EBADF' }); return; }
      if (this._locks.set(oL.ino, lock)) { this._respond(pcb, {}); return; }
      if (lCmd === 6) { this._respond(pcb, { errno: 'EAGAIN' }); return; }
      if (this._locks.deadlocks(oL.ino, lock)) { this._respond(pcb, { errno: 'EDEADLK' }); return; }
      pcb.waiter = { op: 'lock' };
      this._locks.wait(oL.ino, lock, pcb.cid, function () { pcb.waiter = null; self._respond(pcb, {}); });
      return;
    }
    case OP.FS_MMAP: {
      if (!this._mapped || !pcb.vm) { this._respond(pcb, { errno: 'ENOSYS' }); return; }
      var mDesc = null;
      if (!(req.flags & 0x20)) {
        var oM = ofdOf(req.fd);
        if (!oM) { this._respond(pcb, { errno: 'EBADF' }); return; }
        if (oM.kind !== 'file') { this._respond(pcb, { errno: 'ENODEV' }); return; }
        mDesc = { bfsFd: oM.bfsFd, accmode: oM.accmode, ino: oM.ino };
      }
      if(this._readCache)this._readCache.invalidate();
      try { this._respond(pcb, { address: this._mapped.map(pcb.pid, { length: req.length >>> 0, offset: Number(req.offset || 0), prot: req.prot | 0, flags: req.flags | 0 }, mDesc) }); }
      catch (e) { this._respond(pcb, { errno: e.errno || 'EIO' }); }
      return;
    }
    case OP.FS_MUNMAP: case OP.FS_MPROTECT: case OP.FS_MSYNC: {
      if (!this._mapped || !pcb.vm) { this._respond(pcb, { errno: 'ENOSYS' }); return; }
      try {
        if (op === OP.FS_MUNMAP) this._mapped.unmap(pcb.pid, req.address >>> 0, req.length >>> 0);
        else if (op === OP.FS_MPROTECT) this._mapped.protect(pcb.pid, req.address >>> 0, req.length >>> 0, req.prot | 0);
        else this._mapped.sync(pcb.pid, req.address >>> 0, req.length >>> 0, req.flags | 0);
        this._respond(pcb, {});
      } catch (e) { this._respond(pcb, { errno: e.errno || 'EIO' }); }
      return;
    }
    case OP.FS_FCNTL_DUPFD: {
      var oA = ofdOf(req.fd);
      if (!oA) { this._respond(pcb, { errno: 'EBADF' }); return; }
      this._ofdRef(oA, pcb.pid);
      var mfd = allocFd(req.min | 0);
      pcb.fds.set(mfd, oA.id);pcb.fdFlags.set(mfd,req.cloexec?1:0);
      this._respond(pcb, { fd: mfd });
      return;
    }
    case OP.FS_OPENDIR: {
      var dh = fs.opendir(P(req.path));
      if (dh === null) { this._respond(pcb, eFs()); return; }
      this._respond(pcb, this._readdirPage(pcb, dh, null));
      return;
    }
    case OP.FS_READDIR: {
      var dcur = pcb.dirRpc !== null ? pcb.dirRpc.get(req.dir | 0) : undefined;
      if (dcur === undefined) { this._respond(pcb, { errno: 'EBADF' }); return; }
      pcb.dirRpc.delete(req.dir | 0);
      this._respond(pcb, this._readdirPage(pcb, dcur.dh, dcur.pending));
      return;
    }
    case OP.FS_REALPATH: {
      // PHYSICAL realpath — every symlink component resolved against the real
      // kernel-side fs (docs/archive/0263). Chattiness is nil: the walk's lstat/
      // readlink hops stay kernel-local, so a brokered realpath is ONE RPC.
      var rp;
      try { rp = fs.realpathPhysical(P(req.path)); } catch (e) { rp = null; }
      this._respond(pcb, rp ? { path: rp } : { errno: fs._lastError || 'ENOENT' });
      return;
    }
    case OP.FS_ISATTY: {
      var oB = ofdOf(req.fd);
      this._respond(pcb, { tty: oB && (oB.kind === 'tty' || oB.kind === 'ptm') ? 1 : 0 });
      return;
    }
    case OP.FS_POLL: {
      if (req.resultOffset !== undefined) {
        var result = pcb.pollResult;
        if (!result) { this._respond(pcb, {errno:'EINVAL'}); return; }
        var offset = req.resultOffset >>> 0;
        this._respond(pcb, {revents:result.slice(offset,offset+1024)});
        if (offset + 1024 >= result.length) pcb.pollResult = null;
        return;
      }
      if (req.start) pcb.pollStage = [];
      if (req.chunk) {
        if (!Object.prototype.hasOwnProperty.call(pcb,'pollStage') || !pcb.pollStage) {
          this._respond(pcb, {errno:'EINVAL'}); return;
        }
        pcb.pollStage.push(...req.chunk);
        this._respond(pcb, {}); return;
      }
      var fds = req.fds || pcb.pollStage || [];
      pcb.pollStage = null;
      var pw = {op:'poll', fds:fds, r:[], w:[], timer:null};
      fds.forEach(function(p) { if(p[0]>=0) { pw.r.push(p[0]); pw.w.push(p[0]); } });
      this._waitFlagRings(pcb,pw);
      var pr = this._pollScan(pcb,fds);
      if (pr.count || req.timeoutMs === 0) {
        this._clearFlags(pw); this._pollRespond(pcb,pr); return;
      }
      if (req.timeoutMs >= 0) pw.timer=setTimeout(function() {
        if(pcb.waiter===pw) { self._cancelWaiter(pcb); self._pollRespond(pcb,self._pollScan(pcb,fds)); }
      },req.timeoutMs);
      pcb.waiter=pw;
      return;
    }
    case OP.FS_SELECT: {
      // Flag-BEFORE-scan (docs/archive/0181): raise the ring parked-peer flags,
      // THEN scan. A fast commit that the scan missed loads the flag after
      // its commit (SC order) and kicks — the orders cross, so the wake is
      // never lost. An immediate answer drops the flags via _clearFlags.
      var w = { op: 'select', r: req.r || [], w: req.w || [], e: req.e || [], timer: null };
      this._waitFlagRings(pcb, w);
      var ready = this._selectScan(pcb, w.r, w.w, w.e);
      if (ready.errno || ready.count > 0 || req.timeoutMs === 0) {
        this._clearFlags(w);
        this._respond(pcb, ready);
        return;
      }
      if (req.timeoutMs !== null && req.timeoutMs !== undefined) {
        w.timer = setTimeout(function () {
          if (pcb.waiter === w) {
            self._cancelWaiter(pcb);
            self._respond(pcb, self._selectScan(pcb, w.r, w.w, w.e));
          }
        }, req.timeoutMs);
      }
      pcb.waiter = w;                                   // completed by _ttyNotify
      return;
    }
    case OP.FS_WAIT: {
      // Unified wait (docs/archive/0178): FS_SELECT readiness over req.r/req.w,
      // PLUS the process's input ring as a wake source (req.ring). Response
      // { why } — 0 timeout, 1 fd (r/w lists attached), 2 ring; a posted
      // signal completes the park as EINTR through the ordinary
      // interruptible-RPC path. The caller's contract is re-poll-on-any-
      // return, so a wake never has to be exact — only never lost.
      // No-fs kernels answer ENOSYS like every 0x04xx op (the fd layer is
      // the point); single-source ring waits stay on the raw pumpWait
      // futex tier and don't come through here.
      // NB waiter op is 'uwait' — 'wait' is taken by a parked waitpid
      // (OP.WAIT, answered by _exitProcess). Ring flags go up BEFORE the
      // readiness scan (the FS_SELECT comment above — same lost-wake rule).
      var uw = { op: 'uwait', r: req.r || [], w: req.w || [], ring: !!req.ring, timer: null };
      this._waitFlagRings(pcb, uw);
      var wReady = this._selectScan(pcb, uw.r, uw.w);
      var ringReady = false;
      if (wReady.errno || wReady.count > 0 || ringReady || req.timeoutMs === 0) {
        this._clearFlags(uw);
        this._respond(pcb, wReady.errno ? wReady : wReady.count > 0 ? { why: 1, r: wReady.r, w: wReady.w }
          : { why: ringReady ? 2 : 0 });
        return;
      }
      if (req.timeoutMs !== null && req.timeoutMs !== undefined) {
        uw.timer = setTimeout(function () {
          if (pcb.waiter === uw) {
            self._cancelWaiter(pcb);
            self._respond(pcb, { why: 0 });
          }
        }, req.timeoutMs);
      }
      pcb.waiter = uw;     // fd side: _recheckSelects; ring side: _waitRingWake
      return;
    }
    case OP.FS_WATCH_OPEN: {
      // FS_WATCH: one watch per fd (inotify's wd layer deliberately
      // dropped — fds are cheap here and FS_WAIT takes fd lists, so
      // "several watches" is several fds; with the wd goes its whole
      // failure cluster: wd→path maps, wd reuse, the IN_IGNORED race).
      // The path must exist at creation (catches typos loudly) but the
      // watch tolerates later absence: SELF_GONE fires and the watch
      // stays ARMED — a delete-and-recreate or rename-over editor save
      // keeps notifying, because the PATH is the identity, not the inode.
      if (req.flags | 0) { this._respond(pcb, { errno: 'EINVAL' }); return; }
      var wCanon = this._watchCanon(P(req.path));
      var wSt = fs.stat(wCanon);
      if (wSt === null) { this._respond(pcb, eFs()); return; }
      var wOfd = this._makeOfd('watch', {
        path: wCanon,
        isDir: (wSt.mode & 0xF000) === 0x4000,
        mask: (req.mask | 0) || FSW_MASK_DEFAULT,
        queue: [],
        overflowed: false,
      });
      wOfd.refs++;
      this._watches.set(wOfd.id, wOfd);
      var wNewFd = allocFd(0);
      pcb.fds.set(wNewFd, wOfd.id);
      this._respond(pcb, { fd: wNewFd });
      return;
    }
    default: this._respond(pcb, { errno: 'ENOSYS' });
  }
};

/* ---- FS_WATCH (path-keyed file watching; ticket #75) ----
 * The primitive inotify structurally couldn't be: gucOS's single-writer
 * kernel sees EVERY runtime fs mutation as one RPC in the _fsRpc dispatch
 * above, with the path present as a string — so watches key on canonical
 * PATHS, events carry names, a rename is one record with both names, and
 * the editor rename-over-save trap (write tmp + rename onto the target —
 * a per-inode watch dies with the old inode) is gone: the rename lands
 * FSW_CLOSE_WRITE on the watched destination path and the watch survives.
 * Delivery is the existing OFD machinery — a 'watch' OFD is readable in
 * _selectScan (FS_SELECT / the 0178 unified FS_WAIT), drains via FS_READ,
 * dies via FS_CLOSE/_ofdUnref. No new blocking mechanism.
 *
 * Canonicalization is LEXICAL (cwd-join + dot-collapse — fs._resolvePath):
 * the fs surface has no physical resolver (realpath is lexical-only in
 * this flavor, docs/archive/0263), so a write through a symlink alias attributes
 * to the alias's path — the documented residual twin of inotify's
 * hardlink-alias case, harmless to path-consistent consumers. When 0263
 * lands a physical resolver, _watchCanon is the ONE seam to upgrade. */
Kernel.prototype._watchCanon = function (abs) {
  try { var p = this._fs._resolvePath(abs); if (p) return p; } catch (e) {}
  return abs;
};

/* Queue one record on one watch (mask-gated, tail-coalesced). Overflow
 * policy: CLEAR the queue and latch — after a loss the survivors would be
 * a misleading partial prefix; one honest FSW_OVERFLOW ("rescan") beats a
 * plausible-looking lie. The kernel never blocks a mutating RPC on a slow
 * watcher (the strace 0046 principle). Returns true if readiness changed
 * in a way the caller should _recheckSelects for. */
Kernel.prototype._watchQueue = function (w, type, flags, name) {
  if (!(w.mask & (1 << type))) return false;
  if (w.overflowed) return false;              // latched: one OVERFLOW pending
  var q = w.queue;
  var tail = q.length ? q[q.length - 1] : null;
  if (tail && tail.t === type && tail.f === flags && tail.n === name) return false;
  if (q.length >= FSW_QUEUE_CAP) {
    q.length = 0;
    w.overflowed = true;
    if (w.cb) this._watchCbArm(w);
    return true;
  }
  q.push({ t: type, f: flags, n: name });
  if (w.cb) this._watchCbArm(w);
  return true;
};

/* Embedder watches (watchPath below) have no fd to drain: the queued
 * records collapse into ONE deferred callback per event batch. The
 * setTimeout(0) matters twice — a watcher must never run inside the
 * mutating RPC's dispatch, and a burst (tmp write + rename-over) fires
 * the callback once, after the batch settled. */
Kernel.prototype._watchCbArm = function (w) {
  if (w.cbArmed) return;
  w.cbArmed = true;
  setTimeout(function () {
    w.cbArmed = false;
    w.queue.length = 0;
    w.overflowed = false;
    w.cb();
  }, 0);
};

/* Embedder-side path watch: the SAME per-mutation choke FS_WATCH rides,
 * minus the fd machinery — the embedder (kernel-worker) registers a
 * callback on an absolute path and gets one coalesced "this path changed"
 * call per settled batch (no records: the consumer re-reads the file, the
 * cfgstore way). Exact-path semantics only (PATH-keyed like FS_WATCH, so
 * the path may not exist yet and a rename-over save keeps notifying);
 * lifetime is the kernel's — there is no unwatch, embedder watches are
 * boot-scoped by design. First user: the display-density bridge
 * (os/kernel-worker.js watches the display cfgstore layers and re-posts
 * the effective zoom to the page). */
Kernel.prototype.watchPath = function (absPath, cb) {
  this._embWatchN = (this._embWatchN || 0) + 1;
  this._watches.set('embedder:' + this._embWatchN, {
    path: this._watchCanon(absPath),
    isDir: false,
    mask: FSW_MASK_DEFAULT,
    queue: [],
    overflowed: false,
    cb: cb,
    cbArmed: false,
  });
};

/* An embedder-side WRITE participating in the watch contract: the FSW
 * choke lives in the RPC dispatch, so a direct kfs write from the embedder
 * (e.g. kernel-worker persisting the page's zoom control) is invisible to
 * watchers — both process FS_WATCH fds and watchPath — unless it announces
 * the settled content here. */
Kernel.prototype.notifySettled = function (absPath) {
  this._watchEmit(absPath, FSW_CLOSE_WRITE, FSW_CLOSE_WRITE, false);
};

/* Fan a mutation at `absPath` out to the live watches: selfType lands on
 * an exact-path watch (empty name), childType on a watch of the parent
 * directory (the leaf as the record name). Gated on _watches.size — no
 * canonicalization, no iteration on an unwatched system. */
Kernel.prototype._watchEmit = function (absPath, selfType, childType, isDir) {
  if (this._watches.size === 0 || !absPath) return;
  var p = this._watchCanon(absPath);
  var cut = p.lastIndexOf('/');
  var parent = cut <= 0 ? '/' : p.slice(0, cut);
  var leaf = p.slice(cut + 1);
  var fl = isDir ? FSWF_ISDIR : 0;
  var woke = false;
  var self = this;
  this._watches.forEach(function (w) {
    if (w.path === p) { if (self._watchQueue(w, selfType, fl, '')) woke = true; }
    else if (w.isDir && w.path === parent) { if (self._watchQueue(w, childType, fl, leaf)) woke = true; }
  });
  if (woke) this._recheckSelects();
};

/* Rename arrives as ONE RPC carrying both names, so it emits as one story
 * per watcher — no inotify cookie pairing, no unpaired halves:
 *  - exact watch on the source        -> FSW_SELF_GONE (watch stays armed)
 *  - exact watch on the destination   -> FSW_CLOSE_WRITE (rename publishes
 *    COMPLETE content by definition — the rename-over-save settle; a dir
 *    renamed in is FSW_CREATE + ISDIR instead)
 *  - dir watch seeing both sides      -> ONE FSW_RENAME record "old\0new\0"
 *  - dir watch seeing one side        -> FSW_DELETE (out) or the settle/
 *    CREATE (in) — each watcher gets a coherent story for ITS directory. */
Kernel.prototype._watchRename = function (fromAbs, toAbs) {
  if (this._watches.size === 0) return;
  var from = this._watchCanon(fromAbs), to = this._watchCanon(toAbs);
  var st = this._fs.lstat(to);
  var isDir = !!st && (st.mode & 0xF000) === 0x4000;
  var fl = isDir ? FSWF_ISDIR : 0;
  var fc = from.lastIndexOf('/'), tc = to.lastIndexOf('/');
  var fParent = fc <= 0 ? '/' : from.slice(0, fc), fLeaf = from.slice(fc + 1);
  var tParent = tc <= 0 ? '/' : to.slice(0, tc), tLeaf = to.slice(tc + 1);
  var woke = false;
  var self = this;
  this._watches.forEach(function (w) {
    var hit = false;
    if (w.path === from) hit = self._watchQueue(w, FSW_SELF_GONE, fl, '') || hit;
    if (w.path === to) {
      hit = self._watchQueue(w, isDir ? FSW_CREATE : FSW_CLOSE_WRITE, fl, '') || hit;
    }
    if (w.isDir) {
      var f = w.path === fParent, t = w.path === tParent;
      if (f && t) hit = self._watchQueue(w, FSW_RENAME, fl, fLeaf + '\u0000' + tLeaf) || hit;
      else if (f) hit = self._watchQueue(w, FSW_DELETE, fl, fLeaf) || hit;
      else if (t) hit = self._watchQueue(w, isDir ? FSW_CREATE : FSW_CLOSE_WRITE, fl, tLeaf) || hit;
    }
    if (hit) woke = true;
  });
  if (woke) this._recheckSelects();
};

/* Drain queued records into one packed FS_READ reply (whole records only).
 * null = nothing pending (EAGAIN); false = the first record doesn't fit
 * the caller's buffer (EINVAL, the inotify rule — callers size for
 * 4 + NAME_MAX). An overflow latch drains as a single FSW_OVERFLOW record
 * and clears (the queue was already cleared at latch time). */
Kernel.prototype._watchDrain = function (w, cap) {
  var pack = function (t, f, n) {
    var nb = new TextEncoder().encode(n);
    var len = (4 + nb.length + 1 + 3) & ~3;    // header + name + NUL, 4-aligned
    var rec = new Uint8Array(len);
    rec[0] = len & 0xff; rec[1] = (len >> 8) & 0xff;
    rec[2] = t; rec[3] = f;
    rec.set(nb, 4);
    return rec;
  };
  if (w.overflowed) {
    var orec = pack(FSW_OVERFLOW, 0, '');
    if (orec.length > cap) return false;
    w.overflowed = false;
    w.queue.length = 0;
    return orec;
  }
  if (w.queue.length === 0) return null;
  var chunks = [], total = 0;
  while (w.queue.length) {
    var e = w.queue[0];
    var rec = pack(e.t, e.f, e.n);
    if (total + rec.length > cap) break;
    w.queue.shift();
    chunks.push(rec);
    total += rec.length;
  }
  if (chunks.length === 0) return false;
  var out = new Uint8Array(total), off = 0;
  for (var i = 0; i < chunks.length; i++) { out.set(chunks[i], off); off += chunks[i].length; }
  return out;
};

/* ---- AF_UNIX sockets (0x05xx; docs/archive/0008) ----
 * Stream sockets over the pipe machinery: a connection is a pair of
 * pipe-shaped directions (client tx == server rx and vice versa); bind is a
 * real S_IFSOCK node in BlockFS plus an entry in the kernel's rendezvous
 * map; connect never blocks (the queued connection's buffers are usable
 * before accept — data simply waits). Data plane and select ride the
 * existing FS_READ/FS_WRITE/FS_CLOSE/FS_SELECT paths via the 'socket' OFD
 * kind. Brokered mode only, like PIPE_CREATE. */
Kernel.prototype._sockRpc = function (pcb, op, req) {
  var self = this;
  var fs = this._fs;
  if (!this._brokered) { this._respond(pcb, { errno: 'ENOSYS' }); return; }
  var ofdOf = function (fd) {
    var id = pcb.fds.get(fd | 0);
    return id === undefined ? null : self._ofds.get(id) || null;
  };
  var attach = function (target, o) {          // new OFD -> lowest free fd of `target`
    o.refs++;
    var fd = 0;
    while (target.fds.has(fd)) fd++;
    target.fds.set(fd, o.id);
    return fd;
  };
  var sockOf = function (fd) {                 // 'socket'-kind OFD or null+respond
    var o = ofdOf(fd);
    if (!o) { self._respond(pcb, { errno: 'EBADF' }); return null; }
    if (o.kind !== 'socket') { self._respond(pcb, { errno: 'ENOTSOCK' }); return null; }
    return o;
  };

  switch (op) {
    case OP.SOCK_SOCKET: {
      var no = this._makeOfd('socket', { st: 'fresh' });
      this._respond(pcb, { fd: attach(pcb, no) });
      return;
    }
    case OP.SOCK_BIND: {
      var ob = sockOf(req.fd); if (!ob) return;
      if (ob.st !== 'fresh') { this._respond(pcb, { errno: 'EINVAL' }); return; }
      var abs = this._pathFor(pcb, String(req.path || ''));
      // The socket node is a real S_IFSOCK inode (mknod: no data extent) —
      // ls/stat/test -S see it; EEXIST is POSIX's EADDRINUSE here.
      if (fs.mknod(abs, S_IFSOCK_MODE | 0o777, 0) !== 0) {
        var be = fs._lastError || 'EIO';
        this._respond(pcb, { errno: be === 'EEXIST' ? 'EADDRINUSE' : be });
        return;
      }
      var resolved;
      try { resolved = fs._resolvePath(abs); } catch (e) { resolved = abs; }
      ob.st = 'bound';
      ob.path = resolved || abs;
      ob.boundName = String(req.path || '');
      this._sockBinds.set(ob.path, ob.id);
      this._respond(pcb, {});
      return;
    }
    case OP.SOCK_LISTEN: {
      var ol = sockOf(req.fd); if (!ol) return;
      if (ol.st === 'conn') { this._respond(pcb, { errno: 'EISCONN' }); return; }
      if (ol.st === 'fresh') { this._respond(pcb, { errno: 'EDESTADDRREQ' }); return; }
      var bl = req.backlog | 0;
      if (bl < 1) bl = 1; if (bl > 128) bl = 128;
      if (ol.st === 'bound') { ol.st = 'listening'; ol.pending = []; ol.acceptWaiters = []; }
      ol.backlog = bl; ol.listenPid = pcb.pid;
      this._respond(pcb, {});
      return;
    }
    case OP.SOCK_CONNECT: {
      var oc = sockOf(req.fd); if (!oc) return;
      if (oc.st === 'conn') { this._respond(pcb, { errno: 'EISCONN' }); return; }
      if (oc.st !== 'fresh' && oc.st !== 'bound') { this._respond(pcb, { errno: 'EINVAL' }); return; }
      var cabs = this._pathFor(pcb, String(req.path || ''));
      var cres;
      try { cres = fs._resolvePath(cabs); } catch (e) { cres = null; }
      var cst = cres ? fs.stat(cres) : null;
      if (!cst) { this._respond(pcb, { errno: fs._lastError || 'ENOENT' }); return; }
      if ((cst.mode & 0xF000) !== S_IFSOCK_MODE) { this._respond(pcb, { errno: 'ECONNREFUSED' }); return; }
      // Kernel-owned endpoints (sockServe, docs/archive/0014) rendezvous first: the
      // kernel holds the server half of the crossed pair natively — bytes
      // drain to the handler via the _pipeNotify drain hook, no PCB involved.
      var ksrv = this._kernelSockServers.get(cres);
      if (ksrv) {
        var ka = sockDir(), kb = sockDir();        // ka: client->kernel, kb: kernel->client
        oc.st = 'conn'; oc.rx = kb; oc.tx = ka; oc.peerPath = String(req.path || ''); oc.peerPid = 0;
        var kpeer = this._kernelPeer(ka, kb, pcb);
        this._respond(pcb, {});
        try { ksrv(kpeer, pcb); } catch (e) { this._log('sockServe handler: ' + (e && e.message)); kpeer.close(); }
        return;
      }
      var lid = this._sockBinds.get(cres);
      var lo = lid === undefined ? null : this._ofds.get(lid);
      if (!lo || lo.st !== 'listening') { this._respond(pcb, { errno: 'ECONNREFUSED' }); return; }
      // Serve a parked accept directly; otherwise queue within the backlog.
      var served = false;
      while (lo.acceptWaiters.length) {
        var apid = lo.acceptWaiters[0];
        var apcb = this._channelOf(apid);
        if (!apcb || !apcb.waiter || apcb.waiter.op !== 'accept' || apcb.waiter.lofd !== lo) {
          lo.acceptWaiters.shift();
          continue;
        }
        served = true;
        break;
      }
      if (!served && lo.pending.length >= lo.backlog) {
        this._respond(pcb, { errno: 'ECONNREFUSED' });
        return;
      }
      var a = sockDir(), b = sockDir();            // a: client->server, b: server->client
      oc.st = 'conn'; oc.rx = b; oc.tx = a; oc.peerPath = lo.boundName || lo.path; oc.peerPid = lo.listenPid;
      if (served) {
        var acc = this._channelOf(lo.acceptWaiters[0]);
        this._cancelWaiter(acc);
        var so = this._makeOfd('socket', { st: 'conn', rx: a, tx: b, localPath: lo.boundName || lo.path, peerPath: oc.boundName || '', peerPid: pcb.pid });
        this._respond(acc, { fd: attach(acc, so) });
      } else {
        lo.pending.push({ rx: a, tx: b, localPath: lo.boundName || lo.path, peerPath: oc.boundName || '', peerPid: pcb.pid });
        this._recheckSelects();                    // listener became read-ready
      }
      this._respond(pcb, {});
      return;
    }
    case OP.SOCK_ACCEPT: {
      var oa = sockOf(req.fd); if (!oa) return;
      if (oa.st !== 'listening') { this._respond(pcb, { errno: 'EINVAL' }); return; }
      if (oa.pending.length) {
        var conn = oa.pending.shift();
        var ao = this._makeOfd('socket', { st: 'conn', rx: conn.rx, tx: conn.tx, localPath: conn.localPath, peerPath: conn.peerPath, peerPid: conn.peerPid });
        this._respond(pcb, { fd: attach(pcb, ao) });
        return;
      }
      if (oa.statusFlags & 0x800) { this._respond(pcb, {errno:'EAGAIN'}); return; }
      pcb.waiter = { op: 'accept', lofd: oa };     // served by SOCK_CONNECT
      oa.acceptWaiters.push(pcb.cid);
      return;
    }
    case OP.SOCK_PAIR: {
      var pa = sockDir(), pb = sockDir();
      var e0 = this._makeOfd('socket', { st: 'conn', rx: pb, tx: pa, peerPid: pcb.pid });
      var e1 = this._makeOfd('socket', { st: 'conn', rx: pa, tx: pb, peerPid: pcb.pid });
      this._respond(pcb, { fd0: attach(pcb, e0), fd1: attach(pcb, e1) });
      return;
    }
    case OP.SOCK_SEND: {
      var raw = req.raw;
      if (!raw || raw.length < 8) { this._respond(pcb, {errno:'EFAULT'}); return; }
      var view = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
      var sendSocket = sockOf(view.getInt32(0,true)); if (!sendSocket) return;
      var flags = view.getInt32(4,true);
      if (flags & ~(0x40|0x4000)) { this._respond(pcb, {errno:'EOPNOTSUPP'}); return; }
      if (sendSocket.st !== 'conn') { this._respond(pcb, {errno:'ENOTCONN'}); return; }
      this._streamWrite(pcb, sendSocket.tx, raw.subarray(8), !!((sendSocket.statusFlags & 0x800) || (flags & 0x40)), !!(flags & 0x4000));
      return;
    }
    case OP.SOCK_RECV: {
      var recvSocket = sockOf(req.fd); if (!recvSocket) return;
      if (req.flags & ~(0x40|2)) { this._respond(pcb, {errno:'EOPNOTSUPP'}); return; }
      if (recvSocket.st !== 'conn') { this._respond(pcb, {errno:'ENOTCONN'}); return; }
      this._streamRead(pcb, recvSocket.rx, Math.min(req.count >>> 0, KP_FS_CHUNK), !!((recvSocket.statusFlags & 0x800) || (req.flags & 0x40)), !!(req.flags & 2));
      return;
    }
    case OP.SOCK_QUERY: {
      var oq = sockOf(req.fd); if (!oq) return;
      if (req.query === 'credentials') {
        if (oq.st !== 'conn' || oq.peerPid === undefined) { this._respond(pcb, {errno:'ENOTCONN'}); return; }
        // Single-user virtual session: identities match getuid/getgid, not host OS users.
        this._respond(pcb, {pid:oq.peerPid,uid:0,gid:0}); return;
      }
      if (req.query === 'name') {
        if (req.peer && oq.st !== 'conn') { this._respond(pcb, {errno:'ENOTCONN'}); return; }
        this._respond(pcb, {path:req.peer ? (oq.peerPath || '') : (oq.boundName || oq.localPath || '')});
        return;
      }
      if (req.level !== 1 || req.set) { this._respond(pcb, {errno:'ENOPROTOOPT'}); return; }
      var value;
      switch (req.option) {
        case 3: value = 1; break; // SO_TYPE: AF_UNIX stream
        case 4: value = 0; break; // SO_ERROR: connect completes synchronously
        case 7: value = oq.tx ? oq.tx.cap : PIPE_CAP; break;
        case 8: value = oq.rx ? oq.rx.cap : PIPE_CAP; break;
        case 30: value = oq.st === 'listening' ? 1 : 0; break; // SO_ACCEPTCONN
        default: this._respond(pcb, {errno:'ENOPROTOOPT'}); return;
      }
      this._respond(pcb, {value:value}); return;
    }
    case OP.SOCK_SHUTDOWN: {
      var os = sockOf(req.fd); if (!os) return;
      if (os.st !== 'conn') { this._respond(pcb, { errno: 'ENOTCONN' }); return; }
      var how = req.how | 0;
      if (how < 0 || how > 2) { this._respond(pcb, { errno: 'EINVAL' }); return; }
      // Shutdown is connection-global (unlike close, which is per-reference).
      if (how !== 1) { os.rx.rOpen = false; this._pipeNotify(os.rx); }   // SHUT_RD/RDWR
      if (how !== 0) { os.tx.wOpen = false; this._pipeNotify(os.tx); }   // SHUT_WR/RDWR
      this._respond(pcb, {});
      return;
    }
    default: this._respond(pcb, { errno: 'ENOSYS' });
  }
};

/* ---- kernel-owned AF_UNIX endpoints (docs/archive/0014) ----
 * The kernel as a native socket peer: sockServe(path, onConnect) plants a
 * S_IFSOCK inode and registers the path; a process connect() then yields a
 * `peer` object on the kernel side instead of queueing on a listener.
 * The connection is the same crossed pipe-shaped pair as any socket, so
 * the client side blocks/selects/EOFs through the unchanged machinery; the
 * kernel side never parks — arriving bytes fire peer.onData via the
 * _pipeNotify drain hook, peer-gone fires peer.onClose once.
 *
 * peer.send() ignores the direction's cap: kernel replies (a screenshot is
 * megabytes) buffer in full and the client reads them out in chunks. The
 * peer is trusted system software (wm/wmctl) — a reader that never reads
 * costs kernel memory, not correctness. */

Kernel.prototype.sockServe = function (path, onConnect) {
  if (!this._brokered) throw new Error('sockServe: needs the kernel-owned fs');
  var fs = this._fs;
  // Parent dir + socket node, idempotent across reboots over one image
  // (the node persists in BlockFS; the registration doesn't).
  var slash = path.lastIndexOf('/');
  if (slash > 0) fs.mkdir(path.slice(0, slash), 0o755);      // EEXIST is fine
  if (fs.mknod(path, S_IFSOCK_MODE | 0o777, 0) !== 0) {
    if (fs._lastError !== 'EEXIST' ||
        (fs.unlink(path), fs.mknod(path, S_IFSOCK_MODE | 0o777, 0) !== 0)) {
      throw new Error('sockServe ' + path + ': ' + fs._lastError);
    }
  }
  var resolved;
  try { resolved = fs._resolvePath(path); } catch (e) { resolved = path; }
  this._kernelSockServers.set(resolved || path, onConnect);
};

Kernel.prototype._kernelPeer = function (recvDir, sendDir, clientPcb) {
  var self = this;
  var peer = {
    onData: null,               // (Uint8Array) — set by the endpoint handler
    onClose: null,              // () — client hung up (fires once)
    send: function (bytes) {
      if (!sendDir.rOpen || !sendDir.wOpen) return false;
      for (var i = 0; i < bytes.length; i++) sendDir.buf.push(bytes[i]);
      self._pipeNotify(sendDir);                  // serve the client's park
      return true;
    },
    close: function () {
      sendDir.wOpen = false; recvDir.rOpen = false;
      self._pipeNotify(sendDir); self._pipeNotify(recvDir);
    },
  };
  recvDir.drain = function (chunk) { if (peer.onData) peer.onData(chunk); };
  recvDir.onEof = function () { if (peer.onClose) peer.onClose(); };
  return peer;
};

/* Readiness for FS_SELECT: files and non-pipe write interest are always
 * ready; a tty read is ready when cooked bytes or EOF are waiting; a pipe
 * read is ready on data or writer-gone EOF, a pipe write on free space or
 * reader-gone (the write then surfaces EPIPE). */

function audioFormatInfo(format) {
  switch (format | 0) {
    case AU_FMT_F32: return { bytes: 4, decode: function (dv, off) { return dv.getFloat32(off, true); } };
    case AU_FMT_S32: return { bytes: 4, decode: function (dv, off) { return dv.getInt32(off, true) / 2147483648; } };
    case AU_FMT_S16: return { bytes: 2, decode: function (dv, off) { return dv.getInt16(off, true) / 32768; } };
    case AU_FMT_S8:  return { bytes: 1, decode: function (dv, off) { return dv.getInt8(off) / 128; } };
    case AU_FMT_U8:  return { bytes: 1, decode: function (dv, off) { return (dv.getUint8(off) - 128) / 128; } };
    default: return null;
  }
}

Kernel.prototype._audioRpc = function (pcb, op, req) {
  switch (op) {
    case OP.AUDIO_OPEN: {
      var sab = pcb._audioPendingSab;
      pcb._audioPendingSab = null;
      var freq = req.freq | 0, channels = req.channels | 0;
      var fmt = audioFormatInfo(req.format);
      if (!sab || sab.byteLength <= AU_HDR_BYTES || !fmt ||
          freq < 4000 || freq > 192000 || (channels !== 1 && channels !== 2)) {
        this._respond(pcb, { errno: 'EINVAL' }); break;
      }
      var sharedMemory = req.capacity !== undefined;
      var offset = sharedMemory ? req.offset : 0;
      var cap = sharedMemory ? req.capacity : sab.byteLength - AU_HDR_BYTES;
      if (!Number.isSafeInteger(offset) || offset < 0 || offset % 4 !== 0 ||
          !Number.isSafeInteger(cap) || cap <= 0 || offset + AU_HDR_BYTES + cap > sab.byteLength) {
        this._respond(pcb, { errno: 'EINVAL' }); break;
      }
      var frameBytes = fmt.bytes * channels;
      // A frame must never straddle the ring wrap (samples are read with a
      // DataView at a linear offset) — require frame-aligned capacity.
      if (cap % frameBytes !== 0) { this._respond(pcb, { errno: 'EINVAL' }); break; }
      var aid = this._nextAid++;
      this._audioStreams.set(aid, {
        aid: aid, pid: pcb.pid,
        control: new Int32Array(sab, offset, 4),
        dv: new DataView(sab, offset + AU_HDR_BYTES, cap),
        sharedMemory: sharedMemory,
        readPos: 0,     // shared rings register empty; consumer owns this cursor
        cap: cap, freq: freq, channels: channels,
        sampleBytes: fmt.bytes, frameBytes: frameBytes, decode: fmt.decode,
        frac: 0,        // fractional resample cursor, in source frames [0, 1)
        dying: false,   // close/exit marked; drains dry, then reclaimed
      });
      pcb.audios.add(aid);
      this._onAudioStream();
      // #529-A: report the mixer sink's native spec alongside the aid — a
      // read-only fact (the output ring is fixed F32/stereo/AU_OUT_FREQ),
      // no transport policy. host.js answers SDL's destination-format
      // query from it; production audio bytes are unchanged.
      this._respond(pcb, { aid: aid, sinkFormat: AU_FMT_F32,
                           sinkChannels: AU_OUT_CHANNELS, sinkFreq: AU_OUT_FREQ });
      break;
    }
    case OP.AUDIO_CLOSE: {
      var s = this._audioStreams.get(req.aid | 0);
      if (!s || s.pid !== pcb.pid) { this._respond(pcb, { errno: 'EINVAL' }); break; }
      pcb.audios.delete(s.aid);
      if (req.discard) this._audioStreams.delete(s.aid);
      else this._audioMarkDying(s);
      this._respond(pcb, {});
      break;
    }
    case OP.AUDIO_CLEAR: {
      var s = this._audioStreams.get(req.aid | 0);
      if (!s || s.pid !== pcb.pid || !s.sharedMemory) {
        this._respond(pcb, { errno: 'EINVAL' }); break;
      }
      // The caller holds its stream mutex, so no producer runs during this
      // RPC. Kernel pumping and reset are serialized on the owner event loop.
      s.readPos = Atomics.load(s.control, AU_WPOS) % s.cap;
      s.frac = 0;
      Atomics.store(s.control, AU_QUEUED, 0);
      this._respond(pcb, {});
      break;
    }
    case OP.AUDIO_GAIN: {
      // Master output gain (docs/archive/0048): percent, clamped 0..200 (unity
      // 100). A negative request queries. Deliberately NOT per-process —
      // the volume slider is a system control, like the physical knob.
      var g = req.gain | 0;
      if (g >= 0) this._audioGain = Math.min(200, g) / 100;
      this._respond(pcb, { gain: Math.round(this._audioGain * 100) });
      break;
    }
    default: this._respond(pcb, { errno: 'ENOSYS' });
  }
};

/* Close/exit path: drain what's queued (sfx tails finish), drop what can't
 * drain — a paused ring will never be consumed, and with no output ring the
 * pump never runs. Never wedge the mixer on a dead process. */
Kernel.prototype._audioMarkDying = function (s) {
  s.dying = true;
  // A process-owned Wasm allocation may be immediately reused after close.
  // Only independent SAB allocations can safely retain a draining tail.
  if (s.sharedMemory || !this._audioOut || !Atomics.load(s.control, AU_PLAYING) ||
      Atomics.load(s.control, AU_QUEUED) <= 0) {
    this._audioStreams.delete(s.aid);
  }
};

Kernel.prototype.audioInit = function (opts) {
  opts = opts || {};
  var cap = (opts.bufferSize | 0) || AU_OUT_RING_BYTES;
  var sab = new SharedArrayBuffer(AU_HDR_BYTES + cap);
  this._audioOut = {
    sab: sab, control: new Int32Array(sab, 0, 4),
    f32: null, u8: new Uint8Array(sab, AU_HDR_BYTES, cap),
    cap: cap, freq: AU_OUT_FREQ, channels: AU_OUT_CHANNELS,
  };
  return { sab: sab, bufferSize: cap, freq: AU_OUT_FREQ,
           channels: AU_OUT_CHANNELS, format: AU_FMT_F32 };
};

/* Live stream count — the embedder's pump-disarm probe (dying streams
 * count: they still need pumps to drain). */
Kernel.prototype.audioStreamCount = function () {
  return this._audioStreams.size;
};

/* Test/debug view of the stream table. */
Kernel.prototype.audioList = function () {
  var out = [];
  this._audioStreams.forEach(function (s) {
    out.push({ aid: s.aid, pid: s.pid, freq: s.freq, channels: s.channels,
               dying: s.dying, queued: Atomics.load(s.control, AU_QUEUED) });
  });
  return out;
};

/* The mix. Tops the output ring up to AU_TARGET_MS (or maxFrames when the
 * caller bounds it — tests), producing only as many frames as the MOST-
 * available active stream can fill: a starved app pads with silence next
 * to a healthy one, but a lone app never has silence manufactured ahead
 * of data that is about to arrive. Per stream: linear-interp resample on
 * a persistent fractional cursor, mono duplicated, float sum, clamp.
 * Everything here is deterministic math over the SABs. */
Kernel.prototype.audioPump = function (maxFrames) {
  var out = this._audioOut;
  if (!out) return 0;

  // Reclaim dying streams that went dry or paused since the last pump.
  var self = this;
  var dead = null;
  this._audioStreams.forEach(function (s) {
    if (s.dying && (!Atomics.load(s.control, AU_PLAYING) ||
                    Atomics.load(s.control, AU_QUEUED) <= 0)) {
      (dead = dead || []).push(s.aid);
    }
  });
  if (dead) dead.forEach(function (aid) { self._audioStreams.delete(aid); });

  var outFrameBytes = 4 * out.channels;              // f32 interleaved
  var queued = Atomics.load(out.control, AU_QUEUED);
  if (queued < 0) { Atomics.store(out.control, AU_QUEUED, 0); queued = 0; }
  var targetBytes = Math.min(out.cap,
    Math.floor(AU_TARGET_MS * out.freq / 1000) * outFrameBytes);
  var wantFrames = Math.floor(Math.min(out.cap - queued, targetBytes - queued) / outFrameBytes);
  if (maxFrames !== undefined) wantFrames = Math.min(wantFrames, maxFrames | 0);
  if (wantFrames <= 0) return 0;

  // Snapshot each active stream: how many output frames can it back?
  var active = [];
  var mostAvail = 0;
  var spent = null;   // dying streams whose tail can't back one more frame
  this._audioStreams.forEach(function (s) {
    if (!Atomics.load(s.control, AU_PLAYING)) return;   // paused: skip, keep queued
    var qb = Atomics.load(s.control, AU_QUEUED);
    if (qb < 0) { Atomics.store(s.control, AU_QUEUED, 0); qb = 0; }  // clear() race heal
    var srcFrames = Math.floor(qb / s.frameBytes);
    var ratio = s.freq / out.freq;
    var avail = srcFrames > 0 ? Math.floor((srcFrames - s.frac) / ratio) : 0;
    if (avail <= 0) {
      // A dying stream that can't back another output frame is DRY: at a
      // non-integer resample ratio the fractional cursor strands the last
      // source frame(s) forever (floor((srcFrames - frac)/ratio) hits 0
      // with queued > 0), so waiting for queued == 0 would leak the
      // stream — one-shot clips (PlaySound, docs/archive/0094) hit this on
      // every play. Live streams keep the tail: the producer's next push
      // makes it mixable again.
      if (s.dying) (spent = spent || []).push(s.aid);
      return;
    }
    // Legacy independent SABs retain their original cursor derivation and
    // direct-clear protocol. Shared callback rings use the consumer cursor.
    var wpos = Atomics.load(s.control, AU_WPOS) % s.cap;
    // Shared callback rings cannot derive this from two publication cells:
    // a producer may be suspended after WPOS but before QUEUED is published.
    var readBase = s.sharedMemory ? s.readPos : ((wpos - qb) % s.cap + s.cap) % s.cap;
    active.push({ s: s, srcFrames: srcFrames, ratio: ratio, readBase: readBase, avail: avail });
    if (avail > mostAvail) mostAvail = avail;
  });
  if (spent) spent.forEach(function (aid) { self._audioStreams.delete(aid); });
  if (mostAvail === 0) return 0;

  var frames = Math.min(wantFrames, mostAvail);
  var mixL = new Float32Array(frames);
  var mixR = new Float32Array(frames);

  for (var ai = 0; ai < active.length; ai++) {
    var a = active[ai], s = a.s;
    var n = Math.min(frames, a.avail);
    var pos = s.frac;
    for (var i = 0; i < n; i++) {
      var i0 = Math.floor(pos);
      var t = pos - i0;
      var i1 = i0 + 1 < a.srcFrames ? i0 + 1 : i0;   // clamp lookahead at the edge
      var off0 = (a.readBase + i0 * s.frameBytes) % s.cap;
      var off1 = (a.readBase + i1 * s.frameBytes) % s.cap;
      var l0 = s.decode(s.dv, off0), l1 = s.decode(s.dv, off1);
      var L = l0 + (l1 - l0) * t, R;
      if (s.channels === 2) {
        var r0 = s.decode(s.dv, off0 + s.sampleBytes);
        var r1 = s.decode(s.dv, off1 + s.sampleBytes);
        R = r0 + (r1 - r0) * t;
      } else {
        R = L;
      }
      mixL[i] += L;
      mixR[i] += R;
      pos += a.ratio;
    }
    // Consume whole source frames; the fractional remainder carries over.
    var consumed = Math.min(Math.floor(pos), a.srcFrames);
    s.frac = pos - consumed;
    if (consumed > 0) {
      if (s.sharedMemory) s.readPos = (s.readPos + consumed * s.frameBytes) % s.cap;
      Atomics.sub(s.control, AU_QUEUED, consumed * s.frameBytes);
    }
  }

  // Write f32 interleaved into the output ring (the __sdl_queue_audio
  // producer discipline: fill, then advance writePos masked mod cap, then
  // publish via queuedBytes).
  var bytes = frames * outFrameBytes;
  var chunk = new Uint8Array(bytes);
  var cdv = new DataView(chunk.buffer);
  var gain = this._audioGain;                        // master gain (0048)
  for (var f = 0; f < frames; f++) {
    cdv.setFloat32(f * outFrameBytes, Math.max(-1, Math.min(1, mixL[f] * gain)), true);
    cdv.setFloat32(f * outFrameBytes + 4, Math.max(-1, Math.min(1, mixR[f] * gain)), true);
  }
  var owpos = Atomics.load(out.control, AU_WPOS) % out.cap;
  var first = Math.min(bytes, out.cap - owpos);
  out.u8.set(chunk.subarray(0, first), owpos);
  if (first < bytes) out.u8.set(chunk.subarray(first), 0);
  Atomics.store(out.control, AU_WPOS, (owpos + bytes) % out.cap);
  Atomics.add(out.control, AU_QUEUED, bytes);
  return frames;
};


/* One readiness model for poll, select and the unified wait. Error and
 * hangup bits describe the resource and are reported without event interest. */
Kernel.prototype.setScreen = function(w,h) {
  this._screen = {w:Math.max(1,w|0),h:Math.max(1,h|0)};
  var self=this;this._procs.forEach(function(pcb){self._vdsoPublish(pcb);});
};
Kernel.prototype._pollState = function (pcb, fd) {
  var id = pcb.fds.get(fd | 0), o = id === undefined ? null : this._ofds.get(id);
  if (!o) return 0x20; // POLLNVAL
  var readable = false, writable = false, extra = 0;
  if (o.kind === 'pipe') {
    if (o.end === 'read') {
      readable = this._pipeAvail(o.pipe) > 0 || !o.pipe.wOpen;
      if (!o.pipe.wOpen) extra |= 0x10;
    } else {
      writable = this._pipeFree(o.pipe) >= PIPE_ATOMIC;
      if (!o.pipe.rOpen) extra |= 8;
    }
  } else if (o.kind === 'socket') {
    if (o.st === 'conn') {
      readable = this._pipeAvail(o.rx) > 0 || !o.rx.wOpen || !o.rx.rOpen;
      writable = this._pipeFree(o.tx) > 0 || !o.tx.wOpen || !o.tx.rOpen;
      if (!(o.rx.wOpen && o.rx.rOpen) && !(o.tx.wOpen && o.tx.rOpen)) {
        extra |= 0x10;
        writable = false; // POSIX POLLHUP and POLLOUT are mutually exclusive.
      }
    } else if (o.st === 'listening') readable = o.pending.length > 0;
    else { readable = writable = true; }
  } else if (o.kind === 'ptm') {
    readable = this._pipeAvail(o.pty.out) > 0 || !o.pty.out.wOpen;
    writable = true;
    if (!o.pty.out.wOpen) extra |= 0x10;
  } else if (o.kind === 'tty') {
    var tty = o.tty || pcb.tty;
    readable = !tty || tty.readable();
    writable = !o.pty || this._pipeFree(o.pty.out) > 0;
    if (tty && tty._hupFlag) extra |= 0x10;
  } else if (o.kind === 'watch') readable = o.queue.length > 0 || o.overflowed;
  else if (o.kind === 'http') {
    var x = o.xfer;
    readable = (x.status !== null && !x.statusConsumed) || x.bytes > 0 || x.done || x.error !== null;
    if (x.error !== null) extra |= 8;
  } else if (o.kind === 'out') writable = true;
  else readable = writable = true; // regular files and /dev/null
  return extra | (readable ? 0x41 : 0) | (writable ? 4 : 0);
};
Kernel.prototype._pollScan = function (pcb, fds) {
  var self = this, count = 0;
  var revents = fds.map(function (p) {
    var re = p[0] < 0 ? 0 : self._pollState(pcb, p[0]) & (p[1] | 0x38);
    if (re) count++;
    return re;
  });
  return {count:count, revents:revents};
};
Kernel.prototype._pollRespond = function (pcb, result) {
  // Results can exceed the RPC page. Keep the completed snapshot while the
  // caller copies it out; never rescan between result chunks.
  if (result.revents.length > 1024) {
    pcb.pollResult = result.revents;
    this._respond(pcb, {count:result.count, revents:result.revents.slice(0,1024)});
  } else { pcb.pollResult = null; this._respond(pcb, result); }
};
Kernel.prototype._selectScan = function (pcb, rfds, wfds, efds) {
  var self = this, r = [], w = [], e = [], invalid = false;
  function scan(fds, mask, out) {
    (fds || []).forEach(function (fd) {
      var state = self._pollState(pcb, fd);
      if (state & 0x20) invalid = true;
      else if (state & mask) out.push(fd);
    });
  }
  scan(rfds, 1 | 8 | 0x10, r); scan(wfds, 4 | 8, w); scan(efds, 2, e);
  return invalid ? {errno:'EBADF'} : {count:r.length+w.length+e.length,r:r,w:w,e:e};
};

/* Cooked tty data / EOF arrived: serve deferred reads FIFO, then re-check
 * deferred selects with tty interest.
 *
 * Serve-time eligibility (job control): a STOPPED process's parked read
 * stays parked and consumes nothing — otherwise a Ctrl-Z'd `cat` steals
 * the shell's next typed line (found by test_jobctl_tty_e2e). And a read
 * whose pgroup lost the tty since it parked (`bg` on a stopped reader)
 * gets the same SIGTTIN/EIO treatment as the FS_READ dispatch-time check:
 * the input belongs to the foreground pgroup, not to whoever parked
 * first. */
Kernel.prototype._ttyNotify = function (tty) {
  var i = 0;
  while (i < tty.waiters.length) {
    var pid = tty.waiters[i];
    var pcb = this._channelOf(pid);
    if (!pcb || !pcb.waiter || pcb.waiter.op !== 'ttyread' ||
        pcb.waiter.tty !== tty) { tty.waiters.splice(i, 1); continue; }
    if (pcb.state === STATE_STOPPED) { i++; continue; }  // parked, not a consumer
    if (tty.fgPgid > 0 && pcb.pgid !== tty.fgPgid) {
      // Backgrounded since it parked. _cancelWaiter drops it from this
      // queue, so the loop continues at the same index.
      this._cancelWaiter(pcb);
      if (pcb.sigdisp[SIG.TTIN] === DISP_IGN ||
          (Atomics.load(pcb.i32, KP_SIGBLOCK) & (1 << (SIG.TTIN - 1)))) {
        this._respond(pcb, { errno: 'EIO' });
      } else {
        this._killPgid(pcb.pgid, SIG.TTIN);
        this._respond(pcb, { errno: 'EINTR' });
      }
      continue;
    }
    if (tty._cooked.length > 0) {
      var count = pcb.waiter.count;
      this._cancelWaiter(pcb);                           // splices index i out
      this._respondRaw(pcb, tty.take(count));
    } else if (tty._eofFlag || tty._hupFlag) {
      tty._consumeEof();
      this._cancelWaiter(pcb);
      this._respondRaw(pcb, new Uint8Array(0));
    } else {
      break;                                             // no data for anyone eligible
    }
  }
  this._recheckSelects();
};

/* Re-scan every deferred select — and the fd side of every unified WAIT
 * (docs/archive/0178) — after any readiness change (tty bytes, pipe data/space,
 * pipe end closed, a connection queued on a listener). */
Kernel.prototype._recheckSelects = function () {
  var self = this;
  function recheck(pcb) {
    if (!pcb.waiter) return;
    if (pcb.waiter.op === 'poll') {
      var result=self._pollScan(pcb,pcb.waiter.fds);
      if(result.count) { self._cancelWaiter(pcb); self._pollRespond(pcb,result); }
    } else if (pcb.waiter.op === 'select' || pcb.waiter.op === 'uwait') {
      var ready = self._selectScan(pcb, pcb.waiter.r, pcb.waiter.w, pcb.waiter.e);
      if (ready.errno || ready.count > 0) {
        var isWait = pcb.waiter.op === 'uwait';
        self._cancelWaiter(pcb);
        self._respond(pcb, isWait && !ready.errno ? { why: 1, r: ready.r, w: ready.w } : ready);
      }
    }
  }
  this._procs.forEach(function (pcb) {
    recheck(pcb);
    // Each pthread has its own RPC channel and waiter. Descriptor tables
    // are shared, but waking the main thread cannot complete a worker's wait.
    if (pcb.threads) pcb.threads.forEach(recheck);
  });
};

/* Resolve the tty an fd names (0020): pty slave OFDs carry their Tty,
 * masters resolve to the same Tty (termios on either end reaches the
 * pair's line discipline, like Linux); the system tty's std OFDs fall
 * through o.tty to the attached tty. Ring mode (no fd table) and fd-less
 * requests (older callers, tests) fall back to the attached tty; an fd
 * that names a non-tty resolves null (the caller answers ENOTTY). */
Kernel.prototype._ttyForFd = function (pcb, fd) {
  if (this._brokered && fd !== undefined && fd !== null) {
    var id = pcb.fds.get(fd | 0);
    var o = id === undefined ? null : this._ofds.get(id);
    if (o) {
      if (o.kind === 'ptm') return o.pty.tty;
      if (o.kind === 'tty') return o.tty || pcb.tty;
      return null;
    }
  }
  return pcb.tty;
};

/* Pty slave write (0020) — process output to the terminal. OPOST output
 * processing per the slave's termios (ONLCR: \n -> \r\n), then whole-or-
 * block into the master direction: a partial landing could split the
 * expansion, and the byte count reported to the writer must be in PRE-
 * processed bytes (PTY_OUT_CAP guarantees a whole write always fits once
 * the master drains). Master gone -> EIO, no SIGPIPE (pty semantics; the
 * fg pgroup already got SIGHUP at master close). */
Kernel.prototype._ptySlaveWrite = function (pcb, pty, data, nonblock) {
  var dir = pty.out;
  if (!dir.rOpen) { this._respond(pcb, { errno: 'EIO' }); return; }
  var t = pty.tty.termios;
  var processed;
  if ((t.oflag & T_OPOST) && (t.oflag & T_ONLCR)) {
    var out = [];
    for (var i = 0; i < data.length; i++) {
      if (data[i] === 10) out.push(13);
      out.push(data[i]);
    }
    processed = Uint8Array.from(out);
  } else {
    processed = data.slice();          // copy out of the reused SAB payload
  }
  if (dir.buf.length + processed.length <= dir.cap) {
    for (var k = 0; k < processed.length; k++) dir.buf.push(processed[k]);
    this._respond(pcb, { n: data.length });
    this._pipeNotify(dir);
    return;
  }
  if (nonblock) { this._respond(pcb, {errno:'EAGAIN'}); return; }
  pcb.waiter = { op: 'pipewrite', pipe: dir, data: processed,
                 whole: true, n: data.length, ptyw: true };
  dir.writeWaiters.push(pcb.cid);
};

/* Pipe storage accessors (docs/archive/0181): a ringed pipe keeps its bytes in
 * the shared ring IN EVERY MODE (see the PR_* block); everything else
 * (sockets, ptys, ring-less pipes — fake-worker tests) keeps the JS buf.
 * Every kernel touch point goes through these so the two storages can't
 * diverge. */
Kernel.prototype._pipeAvail = function (dir) {
  return dir.ring ? pipeRingAvail(dir.ring) : dir.buf.length;
};
Kernel.prototype._pipeFree = function (dir) {
  return dir.ring ? dir.ring.cap - pipeRingAvail(dir.ring) : dir.cap - dir.buf.length;
};
Kernel.prototype._pipeTake = function (dir, n) {
  if (dir.ring) return pipeRingTake(dir.ring, n);
  return Uint8Array.from(dir.buf.splice(0, n));
};
Kernel.prototype._pipePut = function (dir, data) {
  if (dir.ring) { pipeRingPut(dir.ring, data); return; }
  for (var i = 0; i < data.length; i++) dir.buf.push(data[i]);
};

/* Park-side half of the suppressed-doorbell protocol: latch "someone is
 * parked on this end" in the ring header BEFORE the caller's readiness
 * rescan, and record the word on the waiter so _cancelWaiter (the single
 * wake choke point — serve, EINTR, timeout, kill) clears it. Flag order vs
 * the fast peer's commit-then-check is what makes wakes lossless. */
Kernel.prototype._pipeFlagWait = function (waiter, dir, word) {
  if (!dir.ring) return;
  Atomics.store(dir.ring.i32, word, 1);
  (waiter.flagged = waiter.flagged || []).push({ i32: dir.ring.i32, word: word });
};

/* Raise parked-peer flags for every ringed pipe end a select/uwait names
 * (mode-independent: a brokered-mode flag is just never read). Runs BEFORE
 * the readiness scan; _clearFlags/_cancelWaiter drop them. */
Kernel.prototype._waitFlagRings = function (pcb, waiter) {
  var self = this;
  var flagList = function (fds, end, word) {
    for (var i = 0; i < fds.length; i++) {
      var id = pcb.fds.get(fds[i] | 0);
      var o = id === undefined ? null : self._ofds.get(id);
      if (o && o.kind === 'pipe' && o.end === end) self._pipeFlagWait(waiter, o.pipe, word);
    }
  };
  flagList(waiter.r || [], 'read', PR_RWAIT);
  flagList(waiter.w || [], 'write', PR_WWAIT);
};

Kernel.prototype._clearFlags = function (waiter) {
  if (!waiter.flagged) return;
  for (var i = 0; i < waiter.flagged.length; i++) {
    Atomics.store(waiter.flagged[i].i32, waiter.flagged[i].word, 0);
  }
  waiter.flagged = null;
};

/* Blocking stream ops shared by pipes and connected sockets — `dir` is one
 * pipe-shaped direction. Deferred waiters register under the pipe op names
 * so _pipeNotify/_cancelWaiter serve both kinds unchanged. */
Kernel.prototype._streamRead = function (pcb, dir, count, nonblock, peek) {
  // POSIX read(fd, buf, 0): return 0 immediately, never park — even on an
  // empty stream with a live writer (the 0252 R1 class, kernel-brokered
  // side: pipes / sockets / pty master). BEFORE any avail check or waiter
  // enqueue, matching the host.js R1 short-circuit.
  if (count === 0) { this._respondRaw(pcb, new Uint8Array(0)); return; }
  var avail = this._pipeAvail(dir);
  if (avail > 0) {
    this._respondRaw(pcb, peek ? Uint8Array.from(dir.buf.slice(0, Math.min(count, avail))) : this._pipeTake(dir, Math.min(count, avail)));
    this._pipeNotify(dir);                        // space freed: writers may proceed
    return;
  }
  if (!dir.wOpen || !dir.rOpen) { this._respondRaw(pcb, new Uint8Array(0)); return; }  // EOF
  if (nonblock) { this._respond(pcb, {errno:'EAGAIN'}); return; }
  pcb.waiter = { op: 'piperead', pipe: dir, count: count, peek: !!peek };  // served by _pipeNotify
  this._pipeFlagWait(pcb.waiter, dir, PR_RWAIT);
  dir.readWaiters.push(pcb.cid);
  // Flag-then-rescan: a fast writer that committed between the avail check
  // above and the flag store sees no flag — so look again now that it's up.
  if (dir.ring && pipeRingAvail(dir.ring) > 0) this._pipeNotify(dir);
};

Kernel.prototype._streamWrite = function (pcb, dir, data, nonblock, noSignal) {
  // A zero-length write on a pipe/socket is unspecified by POSIX; gucOS
  // takes the Linux answer for both ("null write succeeds", fs/pipe.c;
  // AF_UNIX's send loop never iterates for len 0): respond 0 BEFORE the
  // EPIPE/SIGPIPE check and before any space check — a full stream must
  // never park a zero-length writer. (#644)
  if (data.length === 0) { this._respond(pcb, { n: 0 }); return; }
  if (!dir.rOpen || !dir.wOpen) {
    // POSIX: write to a pipe/socket nobody reads = EPIPE + SIGPIPE (default
    // action terminates — the `yes | head` pipeline death). !wOpen is the
    // socket shutdown(SHUT_WR) case: the direction's write side is gone
    // even though this OFD is still referenced.
    if (!noSignal) this._deliver(pcb, SIG.PIPE);
    // Publish pending SIGPIPE before waking the writer. Otherwise its import
    // can return and poll signals before the kernel records the signal.
    this._respond(pcb, { errno: 'EPIPE' });
    return;
  }
  var free = this._pipeFree(dir);
  if (free === 0 || (!dir.socket && data.length <= PIPE_ATOMIC && data.length > free)) {
    if (nonblock) { this._respond(pcb, {errno:'EAGAIN'}); return; }
    // Full (or a PIPE_ATOMIC-small write that would split): block.
    // Copy the bytes OUT of the SAB payload region — it's reused.
    pcb.waiter = { op: 'pipewrite', pipe: dir, data: data.slice(), noSignal: !!noSignal };
    this._pipeFlagWait(pcb.waiter, dir, PR_WWAIT);
    dir.writeWaiters.push(pcb.cid);
    if (dir.ring && dir.ring.cap - pipeRingAvail(dir.ring) > free) this._pipeNotify(dir);
    return;
  }
  var n = Math.min(free, data.length);
  this._pipePut(dir, data.subarray(0, n));
  this._respond(pcb, { n: n });
  this._pipeNotify(dir);                          // data arrived: readers may proceed
};

/* Pipe state changed (write, read, end closed): serve deferred readers
 * (data / EOF) and writers (space / EPIPE+SIGPIPE) until nothing more
 * moves, then re-check deferred selects. Serving a read frees space and
 * serving a write supplies data, so loop until a full pass makes no
 * progress. Reentrancy (a SIGPIPE death unrefs fds and re-enters) is safe:
 * every service re-checks pcb.waiter before acting. */
/* Waiter queues (pipes, ttys, listening sockets) hold channel ids: a pid
 * for a process, -tid for a thread channel (docs/THREADS.md). */
Kernel.prototype._channelOf = function (cid) {
  if (cid >= 0) return this._procs.get(cid);
  var found = null;
  this._procs.forEach(function (p) { if (!found && p.threads) { var th = p.threads.get(-cid); if (th) found = th; } });
  return found;
};

Kernel.prototype._pipeNotify = function (pipe) {
  var progress = true;
  while (progress) {
    progress = false;
    // Kernel-held read end (sockServe): no PCB ever parks a read here — the
    // arriving bytes drain to the endpoint's handler instead. Draining frees
    // space, so the writeWaiters pass below still serves a blocked client.
    if (pipe.drain && pipe.buf.length) {
      pipe.drain(Uint8Array.from(pipe.buf.splice(0, pipe.buf.length)));
      progress = true;
    }
    var ri = 0;
    while (ri < pipe.readWaiters.length) {
      var rpcb = this._channelOf(pipe.readWaiters[ri]);
      if (!rpcb || !rpcb.waiter || rpcb.waiter.op !== 'piperead' || rpcb.waiter.pipe !== pipe) {
        pipe.readWaiters.splice(ri, 1);
        continue;
      }
      // Serve-time eligibility (#647): a STOPPED process's parked read stays
      // parked and consumes nothing — the _ttyNotify rule (a Ctrl-Z'd reader
      // must not steal bytes a running reader behind it in the FIFO should
      // get, and bytes handed to a stopped-then-killed process are destroyed
      // where the pipe would have kept them). _contProcess re-notifies this
      // stream on resume, so the skip can never strand the waiter. Writers
      // are deliberately NOT gated: a parked write's bytes were committed at
      // the write() call, so completing it while stopped loses nothing and
      // unblocks running readers.
      if (rpcb.state === STATE_STOPPED) { ri++; continue; }
      var ravail = this._pipeAvail(pipe);
      if (ravail > 0) {
        var count = rpcb.waiter.count, peek = rpcb.waiter.peek;
        this._cancelWaiter(rpcb);                       // splices index ri out
        this._respondRaw(rpcb, peek ? Uint8Array.from(pipe.buf.slice(0, Math.min(count, ravail))) : this._pipeTake(pipe, Math.min(count, ravail)));
        progress = true;
      } else if (!pipe.wOpen || !pipe.rOpen) {
        this._cancelWaiter(rpcb);
        this._respondRaw(rpcb, new Uint8Array(0));     // EOF
        progress = true;
      } else {
        break;                                          // no data for anyone eligible
      }
    }
    while (pipe.writeWaiters.length) {
      var wpcb = this._channelOf(pipe.writeWaiters[0]);
      if (!wpcb || !wpcb.waiter || wpcb.waiter.op !== 'pipewrite' || wpcb.waiter.pipe !== pipe) {
        pipe.writeWaiters.shift();
        continue;
      }
      var ww = wpcb.waiter;
      if (!pipe.rOpen || !pipe.wOpen) {   // !wOpen: shutdown(SHUT_WR) raced a parked write
        this._cancelWaiter(wpcb);
        if (ww.ptyw) {
          // Pty slave writer, master gone: EIO, no SIGPIPE (_ptySlaveWrite).
          this._respond(wpcb, { errno: 'EIO' });
        } else {
          if (!ww.noSignal) this._deliver(wpcb, SIG.PIPE);
          this._respond(wpcb, { errno: 'EPIPE' });
        }
        progress = true;
        continue;
      }
      var data = ww.data;
      var free = this._pipeFree(pipe);
      if (free === 0 || (!pipe.socket && data.length <= PIPE_ATOMIC && data.length > free) ||
          (ww.whole && data.length > free)) break;
      this._cancelWaiter(wpcb);
      var n = ww.whole ? data.length : Math.min(free, data.length);
      this._pipePut(pipe, data.subarray(0, n));
      this._respond(wpcb, { n: ww.n !== undefined ? ww.n : n });
      progress = true;
    }
  }
  // Kernel-held read end: surface peer-gone EOF to the handler exactly once.
  if (pipe.drain && !pipe.wOpen && !pipe._eofSeen) {
    pipe._eofSeen = true;
    if (pipe.onEof) pipe.onEof();
  }
  this._recheckSelects();
};

/* ---- HTTP transport (0x06xx; docs/archive/0172, fd-shaped since docs/archive/0417) ----
 * Fetch-backed HTTP for processes. The kernel owns the fetch; the process
 * drives it through three ops plus the ordinary fd layer. A transfer is
 * fetch-shaped: request (method, url, headers, optional whole body) ->
 * response headers -> streamed body -> clean EOF or error. HTTP_OPEN
 * returns an ORDINARY FD whose OFD (kind 'http') owns the transfer, so any
 * number of transfers multiplex against pipes, watches, the input ring and
 * a timeout through one FS_SELECT/FS_WAIT: readable iff a CONSUMABLE is
 * pending (_selectScan), body drains via FS_READ (EAGAIN when dry — http
 * fds NEVER park; the contract is WAIT first, then consume until EAGAIN,
 * the watch-fd discipline), close via FS_CLOSE (_ofdUnref's last release
 * aborts the fetch — teardown is _exitProcess's ordinary fd sweep).
 * HTTP_STATUS is non-blocking (EAGAIN before headers) and consumes the
 * status. The body queue is a chunk list with kernel-side backpressure
 * (the async reader pauses past HTTP_BUF_CAP), the same bounded-buffer
 * discipline as pipes. Two kernel deadlines bound every transfer (headers
 * + idle; see HTTP_HEADERS_MS above; expiry = ETIMEDOUT on the error leg).
 * Needs the fd layer: no opts.fs -> HTTP_OPEN answers ENOSYS, like every
 * 0x04xx op. NOT a socket layer; TLS is the fetch stack's. */
Kernel.prototype._httpRpc = function (pcb, op, req) {
  switch (op) {
    case OP.HTTP_BODY: {
      // RAW [u32 off][bytes...] — stage the request body contiguously (off 0
      // opens a fresh stage), mirroring CLIP_SET. Consumed by HTTP_OPEN.
      var hb = req.raw;
      if (!hb || hb.length < 4) { this._respond(pcb, { errno: 'EFAULT' }); break; }
      var bdv = new DataView(hb.buffer, hb.byteOffset, hb.length);
      var boff = bdv.getUint32(0, true);
      var bbytes = hb.subarray(4);
      var stage = pcb._httpStage;
      if (boff === 0) { stage = pcb._httpStage = { parts: [], len: 0 }; }
      if (!stage || boff !== stage.len) { pcb._httpStage = null;
        this._respond(pcb, { errno: 'EINVAL' }); break; }
      if (bbytes.length) { stage.parts.push(bbytes.slice()); stage.len += bbytes.length; }
      this._respond(pcb, {});
      break;
    }
    case OP.HTTP_OPEN: {
      // The transfer is an fd, so both halves are required: the embedder's
      // fetch AND the fd layer (no opts.fs -> no fd table to live in).
      if (!this._fetch || !this._fs) { pcb._httpStage = null; this._respond(pcb, { errno: 'ENOSYS' }); break; }
      var method = (req.method || 'GET') + '';
      var url = (req.url || '') + '';
      var hdrs = Array.isArray(req.headers) ? req.headers : [];
      // Deadlines: > 0 overrides the kernel default; idleMs < 0 disables
      // the idle deadline (headers stays bounded no matter what).
      var headersMs = (req.headersMs | 0) > 0 ? (req.headersMs | 0) : HTTP_HEADERS_MS;
      var idleMs = (req.idleMs | 0) > 0 ? (req.idleMs | 0)
        : ((req.idleMs | 0) < 0 ? 0 : HTTP_IDLE_MS);
      // Consume any staged body (join the chunks).
      var body = null, st = pcb._httpStage; pcb._httpStage = null;
      if (st && st.len) {
        body = new Uint8Array(st.len);
        for (var pi = 0, po = 0; pi < st.parts.length; pi++) { body.set(st.parts[pi], po); po += st.parts[pi].length; }
      }
      var xfer = {
        status: null, headers: null,          // set when response headers arrive
        statusConsumed: false,                // HTTP_STATUS spends the status leg
        chunks: [], bytes: 0, cap: HTTP_BUF_CAP,
        done: false, error: null, errno: null, aborted: false, paused: false,
        ac: (typeof AbortController !== 'undefined') ? new AbortController() : null,
        reader: null,
        idleMs: idleMs, hdrTimer: null, idleTimer: null,
      };
      var ho = this._makeOfd('http', { xfer: xfer });
      ho.refs++;
      var hfd = 0;
      while (pcb.fds.has(hfd)) hfd++;
      pcb.fds.set(hfd, ho.id);
      this._httpStart(xfer, method, url, hdrs, body, headersMs);
      this._respond(pcb, { fd: hfd });
      break;
    }
    case OP.HTTP_STATUS: {
      var sid = pcb.fds.get(req.fd | 0);
      var so = sid === undefined ? null : this._ofds.get(sid);
      if (!so || so.kind !== 'http') { this._respond(pcb, { errno: 'EBADF' }); break; }
      var xs = so.xfer;
      if (xs.error !== null && xs.status === null) {
        this._respond(pcb, { errno: xs.errno || 'EIO', error: xs.error }); break;
      }
      if (xs.status !== null) {
        // Spend the status leg of readability. Without this bit,
        // headers-arrived is permanent and a caller waiting for the first
        // body byte would find the fd readable forever (docs/archive/0417).
        xs.statusConsumed = true;
        this._respond(pcb, { status: xs.status, headers: xs.headers || '' }); break;
      }
      this._respond(pcb, { errno: 'EAGAIN' });   // not yet — WAIT on the fd
      break;
    }
    case OP.HTTP_ERROR: {
      // The error-TEXT peek (#392): hand the transfer's recorded failure
      // text to the process. Non-consuming and stateless — callable before,
      // during and after HTTP_STATUS/FS_READ have reported the errno — so a
      // consumer can always enrich its errno with the transport's own words
      // (CURLOPT_ERRORBUFFER is the first customer). '' while healthy.
      var eid = pcb.fds.get(req.fd | 0);
      var eo = eid === undefined ? null : this._ofds.get(eid);
      if (!eo || eo.kind !== 'http') { this._respond(pcb, { errno: 'EBADF' }); break; }
      var ex = eo.xfer;
      // {error} only — the caller already holds the errno from the failing
      // HTTP_STATUS/FS_READ, and an errno key here would read as an RPC
      // failure to the host-side hook plumbing.
      this._respond(pcb, { error: ex.error !== null ? ex.error : '' });
      break;
    }
    default:
      this._respond(pcb, { errno: 'ENOSYS' });
  }
};

/* Compose a fetch rejection's honest text (#392). Node's fetch buries the
 * useful part (connect ECONNREFUSED host:port, ENOTFOUND, a TLS verdict) in
 * err.cause and rejects with a bare "fetch failed" on top — surface both,
 * skipping the cause when the top-level message already carries it (the
 * net-bridge wrapper digs its own cause out; ticket #393). */
function _fetchErrorText(err) {
  var msg = (err && err.message) ? (err.message + '') : 'fetch failed';
  var cause = err && err.cause && err.cause.message ? (err.cause.message + '') : '';
  if (cause && msg.indexOf(cause) < 0) msg += ' (' + cause + ')';
  return msg;
}

/* Flatten a fetch Headers object into a capped "name: value\n" blob. Order
 * and casing are whatever fetch yields (not wire-faithful — documented).
 * A server-sent `x-guc-final-url` is dropped: that name is the transport's
 * synthetic final-URL line (#359, prepended in _httpStart), and an upstream
 * copy in the same blob would make it ambiguous. This is the one choke
 * point both modes flatten through (bridge pairs arrive via the wrapper's
 * forEach shim), so the filter covers direct and bridge alike. */
Kernel.prototype._httpFlattenHeaders = function (headers) {
  var out = '';
  try {
    headers.forEach(function (v, k) {
      if ((k + '').toLowerCase() === 'x-guc-final-url') return;
      if (out.length < 64 * 1024) out += k + ': ' + v + '\n';
    });
  } catch (e) {}
  return out;
};

/* Kick off the fetch. Resolves headers (or a pre-body error) then streams the
 * body through the reader under backpressure. Aborted transfers drop silently
 * (close / process teardown already reclaimed them); a deadline-expired
 * transfer keeps its ETIMEDOUT (the error guard on both handlers — the
 * AbortError rejection the expiry provokes must not overwrite it). Every
 * state change that adds a consumable ends in _recheckSelects, which is the
 * whole wake path now: nothing parks on a transfer, only on the fd. */
Kernel.prototype._httpStart = function (xfer, method, url, headerList, body, headersMs) {
  var self = this;
  var pairs = [];
  for (var i = 0; i < headerList.length; i++) {
    var line = headerList[i] + '';
    var c = line.indexOf(':');
    if (c > 0) pairs.push([line.slice(0, c).trim(), line.slice(c + 1).trim()]);
  }
  var init = { method: method, headers: pairs, redirect: 'follow', credentials: 'omit' };
  if (xfer.ac) init.signal = xfer.ac.signal;
  if (body && body.length) init.body = body;
  if (headersMs > 0) {
    xfer.hdrTimer = setTimeout(function () {
      xfer.hdrTimer = null;
      self._httpExpire(xfer, 'response headers deadline expired (' + headersMs + 'ms)');
    }, headersMs);
  }
  var p;
  try { p = Promise.resolve(this._fetch(url, init)); }
  catch (e) { p = Promise.reject(e); }        // synchronous throw (bad URL)
  p.then(function (resp) {
    if (xfer.aborted || xfer.error !== null) return;
    if (xfer.hdrTimer) { clearTimeout(xfer.hdrTimer); xfer.hdrTimer = null; }
    xfer.status = resp.status;
    // #359: surface the post-redirect FINAL URL as one synthetic header
    // line. Direct mode reads the real Response.url; the bridge wrapper
    // mirrors it as a .url property on its synthetic response. Absent
    // either (a bare test fetch), the request url is the honest answer.
    // PREPENDED so the line always survives the 64 KB flatten cap; the
    // intermediate hops stay unknowable — platform fetch follows
    // redirects opaquely in both modes (permanent ceiling, documented).
    var finalUrl = (typeof resp.url === 'string' && resp.url) ? resp.url : url;
    xfer.headers = 'x-guc-final-url: ' + finalUrl + '\n'
      + self._httpFlattenHeaders(resp.headers);
    xfer.reader = (resp.body && typeof resp.body.getReader === 'function') ? resp.body.getReader() : null;
    if (!xfer.reader) {                        // no streamable body (HEAD, empty)
      xfer.done = true; self._recheckSelects(); return;
    }
    self._httpIdleArm(xfer);
    self._httpPump(xfer);
    self._recheckSelects();
  }, function (err) {
    if (xfer.aborted || xfer.error !== null) return;
    if (xfer.hdrTimer) { clearTimeout(xfer.hdrTimer); xfer.hdrTimer = null; }
    xfer.error = _fetchErrorText(err);
    // An embedder fetch WRAPPER may pin a POSIX errno NAME on its rejection
    // (err.errno, a string) — the net-bridge wrapper tags a configured-but-
    // unreachable bridge ENETUNREACH (ticket #349; ruling in NETWORK.md
    // Tier 2.5). The typeof check keeps Node system errors out: their
    // .errno is a NUMBER, and their EIO mapping is documented behavior.
    xfer.errno = (err && typeof err.errno === 'string') ? err.errno : 'EIO';
    self._recheckSelects();
  });
};

/* A deadline fired: fail the transfer with ETIMEDOUT — distinguishable from
 * a connect error (EIO) and from clean EOF — and abort the fetch. The fd
 * stays open: the expiry is a CONSUMABLE (the error leg of readability), so
 * a parked FS_WAIT wakes and the reader collects it. */
Kernel.prototype._httpExpire = function (xfer, msg) {
  if (xfer.aborted || xfer.done || xfer.error !== null) return;
  xfer.error = msg;
  xfer.errno = 'ETIMEDOUT';
  if (xfer.idleTimer) { clearTimeout(xfer.idleTimer); xfer.idleTimer = null; }
  if (xfer.ac) { try { xfer.ac.abort(); } catch (e) {} }
  if (xfer.reader) {
    try { var cp = xfer.reader.cancel(); if (cp && cp.catch) cp.catch(function () {}); }
    catch (e) {}
  }
  this._recheckSelects();
};

/* (Re-)arm the idle deadline: the body must deliver a byte within it. Runs
 * only while the kernel is actually waiting on the network — a backpressure
 * pause CLEARS it (that gap is the consumer's, not the server's) and the
 * resume re-arms. idleMs 0 = disabled (HTTP_OPEN idleMs < 0). */
Kernel.prototype._httpIdleArm = function (xfer) {
  var self = this;
  if (xfer.idleTimer) { clearTimeout(xfer.idleTimer); xfer.idleTimer = null; }
  if (!xfer.idleMs || xfer.aborted || xfer.done || xfer.error !== null) return;
  xfer.idleTimer = setTimeout(function () {
    xfer.idleTimer = null;
    self._httpExpire(xfer, 'body idle deadline expired (' + xfer.idleMs + 'ms)');
  }, xfer.idleMs);
};

/* Read one chunk under backpressure: pause past cap (an FS_READ drain
 * resumes us), else pull, queue, wake parked waiters, and continue. */
Kernel.prototype._httpPump = function (xfer) {
  var self = this;
  if (xfer.aborted || xfer.done || xfer.error !== null) return;
  if (xfer.bytes >= xfer.cap) {
    // Backpressure pause: the coming gap is the consumer's, not the
    // server's — the idle clock stops; the drain that resumes us re-arms.
    xfer.paused = true;
    if (xfer.idleTimer) { clearTimeout(xfer.idleTimer); xfer.idleTimer = null; }
    return;
  }
  if (xfer.paused) { xfer.paused = false; this._httpIdleArm(xfer); }
  xfer.reader.read().then(function (r) {
    if (xfer.aborted || xfer.error !== null) return;
    if (r.done) {
      xfer.done = true;
      if (xfer.idleTimer) { clearTimeout(xfer.idleTimer); xfer.idleTimer = null; }
      self._recheckSelects();
      return;
    }
    var chunk = r.value;                       // Uint8Array
    if (chunk && chunk.length) {
      xfer.chunks.push(chunk); xfer.bytes += chunk.length;
      self._httpIdleArm(xfer);                 // a byte arrived: restart the clock
    }
    self._recheckSelects();
    self._httpPump(xfer);
  }, function (err) {
    if (xfer.aborted || xfer.error !== null) return;
    xfer.error = (err && err.message) ? _fetchErrorText(err) : 'stream read failed';
    xfer.errno = 'EIO';
    if (xfer.idleTimer) { clearTimeout(xfer.idleTimer); xfer.idleTimer = null; }
    self._recheckSelects();
  });
};

/* Pull up to `count` bytes off the chunk queue (clamped to the page payload).
 * Slices the head chunk when it straddles the boundary — no per-byte copies. */
Kernel.prototype._httpDrain = function (xfer, count) {
  var want = Math.min(count, KP_PAYLOAD_CAP, xfer.bytes);
  var out = new Uint8Array(want), o = 0;
  while (o < want && xfer.chunks.length) {
    var head = xfer.chunks[0];
    var take = Math.min(head.length, want - o);
    out.set(head.subarray(0, take), o); o += take;
    if (take === head.length) xfer.chunks.shift();
    else xfer.chunks[0] = head.subarray(take);
    xfer.bytes -= take;
  }
  return out;
};

/* Abort + free a transfer — the http OFD's last-release action (_ofdUnref:
 * FS_CLOSE, a dup'd twin's death, or _exitProcess's fd sweep). Never
 * wedges: aborting is idempotent, and nothing ever parks ON a transfer
 * (http fds never park), so there is no waiter to strand. */
Kernel.prototype._httpDestroy = function (xfer) {
  xfer.aborted = true;
  if (xfer.hdrTimer) { clearTimeout(xfer.hdrTimer); xfer.hdrTimer = null; }
  if (xfer.idleTimer) { clearTimeout(xfer.idleTimer); xfer.idleTimer = null; }
  if (xfer.ac) { try { xfer.ac.abort(); } catch (e) {} }
  // cancel() returns a promise that REJECTS when the stream already errored
  // (e.g. a mid-stream drop) — swallow it, or it surfaces as an unhandled
  // rejection and crashes the embedder. The sync try/catch can't catch the
  // async rejection, so attach a .catch.
  if (xfer.reader) {
    try { var cp = xfer.reader.cancel(); if (cp && cp.catch) cp.catch(function () {}); }
    catch (e) {}
  }
};

/* ---- vDSO block (docs/archive/0179) ----
 * Publish the kernel-written, process-read words on a pcb's page under the
 * seqlock (odd = write in progress). The kernel event loop is the single
 * writer by construction, so two Atomics.add bumps are the whole protocol;
 * the seqlock only protects READERS on the process thread from a torn
 * multi-word view. Call after ANY mutation of a published field. */
Kernel.prototype._vdsoPublish = function (pcb) {
  var i32 = pcb.i32;
  if (!i32) return;
  Atomics.add(i32, KP_VD_SEQ, 1);          // odd: readers back off
  Atomics.store(i32, KP_VD_PID, pcb.pid);
  Atomics.store(i32, KP_VD_PPID, pcb.ppid);
  Atomics.store(i32, KP_VD_PGID, pcb.pgid);
  Atomics.store(i32, KP_VD_SID, pcb.sid);
  Atomics.store(i32, KP_VD_BOOT_LO, this._bootMs >>> 0);
  Atomics.store(i32, KP_VD_BOOT_HI, Math.floor(this._bootMs / 4294967296));
  Atomics.store(i32, KP_VD_SCREEN_W, this._screen.w);
  Atomics.store(i32, KP_VD_SCREEN_H, this._screen.h);
  Atomics.add(i32, KP_VD_SEQ, 1);          // even: stable
};

/* ---- process groups ----
 * setpgid(pid, pgid): pid 0 = the caller, pgid 0 = "target's own pid".
 * POSIX scoping kept simple for one user: the target must be the caller or
 * one of its children, in the same session. (Latent since Phase 1 — the
 * dispatch existed but nothing defined this until the shell port's libc
 * wrappers landed, docs/archive/0005.) */
Kernel.prototype._setpgid = function (pcb, pid, pgid) {
  var t = (pid === 0) ? pcb : this._procs.get(pid);
  if (!t || t.state === STATE_ZOMBIE) return { errno: 'ESRCH' };
  if (t !== pcb && !pcb.children.has(t.pid)) return { errno: 'ESRCH' };
  if (t.sid !== pcb.sid) return { errno: 'EPERM' };
  t.pgid = (pgid > 0) ? pgid : t.pid;
  this._vdsoPublish(t);
  return {};
};

/* ---- wait / reap ---- */

function waitSelectorMatch(sel, waiterPcb, childPcb) {
  if (sel > 0) return childPcb.pid === sel;
  if (sel === -1) return true;
  if (sel === 0) return childPcb.pgid === waiterPcb.pgid;
  return childPcb.pgid === -sel;
}

Kernel.prototype._wakeChildWaiters = function(parent) {
  for(const channel of [parent,...(parent.threads?.values()||[])]) {
    if(channel.waiter?.op!=='wait')continue;
    const {sel,options}=channel.waiter;
    this._cancelWaiter(channel);
    this._wait(channel,sel,options);
  }
};

Kernel.prototype._wait = function (pcb, sel, options) {
  var candidates = [];
  var self = this;
  pcb.children.forEach(function (cpid) {
    var c = self._procs.get(cpid);
    if (c && !c.autoReap && waitSelectorMatch(sel, pcb, c)) candidates.push(c);
  });
  if (candidates.length === 0) { this._respond(pcb, { errno: 'ECHILD' }); return; }
  for (var i = 0; i < candidates.length; i++) {
    if (candidates[i].state === STATE_ZOMBIE) {
      var z = candidates[i];
      this._reap(z);
      this._respond(pcb, { pid: z.pid, status: z.exit });
      return;
    }
  }
  // Job control (docs/archive/0003): unreported stop/continue transitions satisfy a
  // wait that asked for them; each transition is reported exactly once.
  for (var j = 0; j < candidates.length; j++) {
    var c = candidates[j];
    if ((options & WUNTRACED) && c.state === STATE_STOPPED && c.pendingStop) {
      var ssig = c.pendingStop;
      c.pendingStop = 0;
      this._respond(pcb, { pid: c.pid, status: W_STOPCODE(ssig) });
      return;
    }
    if ((options & WCONTINUED) && c.pendingCont) {
      c.pendingCont = false;
      this._respond(pcb, { pid: c.pid, status: W_CONTINUED_STATUS });
      return;
    }
  }
  if (options & WNOHANG) { this._respond(pcb, { pid: 0, status: 0 }); return; }
  pcb.waiter = { op: 'wait', sel: sel, options: options };  // answered by _exitProcess
};

/* Drop a deferred RPC registration (answered, interrupted, or the process
 * died) — clears timers and tty wait-queue membership. */
Kernel.prototype._cancelWaiter = function (pcb) {
  var w = pcb.waiter;
  pcb.waiter = null;
  if (!w) return;
  if (w.abort) w.abort.abort();
  if (w.timer) clearTimeout(w.timer);
  // SPSC rings (docs/archive/0181): drop the parked-peer flags this wait raised.
  // Every wake path funnels here (serve, EINTR, timeout, exit), so a flag
  // can't outlive its waiter; a fast peer that already read it just sends
  // one redundant PIPE_KICK.
  this._clearFlags(w);
  if (w.op === 'ttyread') {
    var i = w.tty.waiters.indexOf(pcb.cid);
    if (i >= 0) w.tty.waiters.splice(i, 1);
  } else if (w.op === 'piperead' || w.op === 'pipewrite') {
    var q = w.op === 'piperead' ? w.pipe.readWaiters : w.pipe.writeWaiters;
    var j = q.indexOf(pcb.cid);
    if (j >= 0) q.splice(j, 1);
  } else if (w.op === 'accept') {
    var k = w.lofd.acceptWaiters.indexOf(pcb.cid);
    if (k >= 0) w.lofd.acceptWaiters.splice(k, 1);
  } else if (w.op === 'lock') {
    if (this._locks) this._locks.cancel(pcb.cid);
  }
};

Kernel.prototype._reap = function (zombie) {
  this._procs.delete(zombie.pid);
  var parent = this._procs.get(zombie.ppid);
  if (parent) parent.children.delete(zombie.pid);
};

/* ---- exit ---- */

Kernel.prototype._releaseSharedChild = function(pcb) {
  if(pcb._sharedDone){const done=pcb._sharedDone;pcb._sharedDone=null;pcb._sharedParent=null;done({pid:pcb.pid});}
};

Kernel.prototype._exitProcess = function (pcb, status) {
  pcb.compileOutput = null;
  if (pcb.isThread) pcb = pcb.__proc;   // exit() from any thread ends the whole process
  if (pcb.state === STATE_ZOMBIE) return;
  if(pcb._forkReady){pcb._forkReady({pid:pcb.pid});pcb._forkReady=null;}
  pcb.state = STATE_ZOMBIE;
  pcb.exit = status;
  if (pcb.trace) this._traceExit(pcb, status);   // strace (0046): final lines + EOF
  this._cancelWaiter(pcb);
  this._itimerClear(pcb);        // interval timers die with the process (0044)
  if (pcb.worker) { try { pcb.worker.terminate(); } catch (e) {} }
  pcb.worker = null;
  this._releaseSharedChild(pcb);
  this._killThreads(pcb);
  // Release every fd — this is what makes SIGKILL leak-free under the
  // brokered fs: the kernel, not the dead worker, owns the descriptions,
  // so unlink-while-open lifetimes complete even on a hard kill.
  var self0 = this;
  // Mappings and record locks die with the process (docs/MAPPINGS.md);
  // shared pages are written back first, so a killed writer loses nothing.
  if (this._mapped) { try { this._mapped.release(pcb.pid); } catch (e) { this._log('exit: mapping release failed: ' + (e && e.message)); } }
  if (this._locks) this._locks.release(pcb.pid, undefined, function (w) { var ch = self0._channelOf(w.cid); if (ch) { ch.waiter = null; self0._respond(ch, { errno: 'EBADF' }); } });
  pcb.fds.forEach(function (ofdId) { self0._ofdUnref(ofdId, pcb.pid); });
  pcb.fds.clear();
  // Parked readdir cursors (docs/archive/0241): a client that died mid-pagination
  // must not leak the backend dir handle — same discipline as fds.
  if (pcb.dirRpc !== null) {
    pcb.dirRpc.forEach(function (d) { self0._fs.closedir(d.dh); });
    pcb.dirRpc = null;
  }
  // Audio streams (docs/archive/0017): same discipline — mark dying; the pump
  // drains queued tails then reclaims (paused/no-output drop immediately).
  if (pcb.audios.size) {
    Array.from(pcb.audios).forEach(function (aid) {
      var s = self0._audioStreams.get(aid);
      if (s) self0._audioMarkDying(s);
    });
  }
  pcb.audios.clear();
  pcb._audioPendingSab = null;
  // HTTP transfers are fds (docs/archive/0417): the fd sweep above already
  // aborted every live fetch — no dedicated transfer sweep exists.
  pcb._httpStage = null;

  var self = this;
  // Reparent children (running AND zombie) to init.
  var init = this._procs.get(1);
  pcb.children.forEach(function (cpid) {
    var c = self._procs.get(cpid);
    if (!c) return;
    c.ppid = self._keepAlive && (!init || init === pcb) ? 0 : 1;
    if(c.ppid === 0 && c.state === STATE_ZOMBIE)self._reap(c);
    self._vdsoPublish(c);        // getppid tracks the reparent (docs/archive/0179)
    if (init && init !== pcb) {
      init.children.add(cpid);
      // A reparented zombie may satisfy init's pending wait immediately.
      if (c.state === STATE_ZOMBIE && init.waiter && init.waiter.op === 'wait' &&
          waitSelectorMatch(init.waiter.sel, init, c)) {
        self._cancelWaiter(init);
        self._reap(c);
        self._respond(init, { pid: c.pid, status: c.exit });
      }
    }
  });
  pcb.children.clear();

  this._onProcessExit(pcb.pid, status);
  if (pcb.pid === 1 && !this._keepAlive) {
    this._halted = true;
    this._onHalt(status);
    this._procs.delete(1);
    return;
  }

  if (pcb.autoReap || (pcb.pid === 1 && this._keepAlive)) { this._reap(pcb); return; }
  var parent = this._procs.get(pcb.ppid);
  if (parent && parent.state === STATE_RUNNING) {
    // Post SIGCHLD BEFORE answering a parked wait: the parent wakes on the
    // response and races through its safe point immediately — the pending
    // bit must already be visible so the handler runs before waitpid
    // returns to the program. (Default-ignore drops it; a stray krpc-intr
    // triggered by the post is ignored once the waiter is answered.)
    this._deliver(parent, SIG.CHLD);
    this._wakeChildWaiters(parent);
    this._ring(parent);
  } else {
    // Parent already gone: we were (or just became) init's child; init will
    // reap us, or we ride out as a zombie under it. Kernel-owned services
    // (ppid 0, Kernel.service) have no parent to ever wait: auto-reap.
    if (!this._procs.get(1) || pcb.ppid === 0) this._reap(pcb);
  }
};

/* ---- kill ----
 * Routing per docs/KERNEL.md: pid > 0 exact; pid 0 = sender's pgroup;
 * pid < -1 = pgroup -pid; pid -1 unsupported (EPERM) — no "everyone" in v1.
 * Action = disposition mirror: HANDLER -> post SIGPEND bit (delivery is
 * Phase 2; the bit + doorbell are already correct); IGN -> drop; DFL ->
 * default action (terminate class ends the process; ignore class drops;
 * stop/continue classes are Phase 4 — dropped with a log for now).
 * SIGKILL/SIGSTOP never consult the mirror. `sender` is the calling PCB
 * (null for kernel/embedder-initiated kills). */
Kernel.prototype.kill = function (pid, sig, sender) {
  // Signal 0 is the POSIX existence probe: route + error-check only,
  // deliver nothing (kill(2) — hush's `kill -0 PID` rides on it).
  if (!(sig >= 0 && sig < NSIG)) return { errno: 'EINVAL' };
  var targets = [];
  var self = this;
  if (pid > 0) {
    var t = this._procs.get(pid);
    // STOPPED processes are valid targets (SIGCONT/SIGKILL must reach them);
    // zombies are not (they only await reaping).
    if (!t || t.state === STATE_ZOMBIE) return { errno: 'ESRCH' };
    targets.push(t);
  } else if (pid === -1) {
    return { errno: 'EPERM' };
  } else {
    var pgid = pid === 0 ? (sender ? sender.pgid : 0) : -pid;
    if (this._killPgid(pgid, sig) === 0) return { errno: 'ESRCH' };
    return {};
  }
  if (sig === 0) return {};
  for (var i = 0; i < targets.length; i++) this._deliver(targets[i], sig);
  return {};
};

/* Deliver sig to every live (running or stopped) member of a pgroup;
 * returns the member count. Used by pgroup kill() and by the tty's
 * control-char routing. */
Kernel.prototype._killPgid = function (pgid, sig) {
  var targets = [];
  this._procs.forEach(function (p) {
    if (p.state !== STATE_ZOMBIE && p.pgid === pgid) targets.push(p);
  });
  if (sig === 0) return targets.length;           // probe: count, deliver nothing
  for (var i = 0; i < targets.length; i++) this._deliver(targets[i], sig);
  return targets.length;
};

Kernel.prototype._deliver = function (pcb, sig) {
  // strace (0046): signal arrivals interleave with the RPC lines.
  if (pcb.trace) this._traceLine(pcb, '--- ' + (SIG_NAMES[sig] || 'signal ' + sig) + ' ---');
  if (sig === SIG.KILL) { this._exitProcess(pcb, W_TERMSIG(sig)); return; }
  // SIGCONT resumes a stopped process REGARDLESS of disposition (POSIX);
  // any handler then delivers normally through the mirror below.
  if (sig === SIG.CONT) this._contProcess(pcb);
  // SIGSTOP is uncatchable and never consults the mirror.
  if (sig === SIG.STOP) { this._stopProcess(pcb, sig); return; }
  var disp = pcb.sigdisp[sig];
  if (disp === DISP_IGN) return;
  if (disp === DISP_HANDLER) {
    // Post the pending bit; libc claims it at its next safe point and runs
    // the handler. Also wake the tty ring: a read blocked on the SI_SEQ
    // futex must re-scan, notice the pending signal, and turn into EINTR.
    Atomics.or(pcb.i32, KP_SIGPEND, 1 << (sig - 1));
    this._ring(pcb);
    if (pcb.tty) pcb.tty.wakeReaders();
    return;
  }
  switch (sigDefaultAction(sig)) {
    case 0:
      // Terminate — but honor the target's published blocked mask: a blocked
      // fatal signal stays pending; libc applies the default action when
      // sigprocmask unblocks it (it kill-selfs, so the termsig still
      // round-trips as WIFSIGNALED).
      if (Atomics.load(pcb.i32, KP_SIGBLOCK) & (1 << (sig - 1))) {
        Atomics.or(pcb.i32, KP_SIGPEND, 1 << (sig - 1));
        this._ring(pcb);
        if (pcb.tty) pcb.tty.wakeReaders();
      } else {
        this._exitProcess(pcb, W_TERMSIG(sig));
      }
      break;
    case 1: break;                                           // ignore
    case 2: this._stopProcess(pcb, sig); break;              // TSTP/TTIN/TTOU at DFL
    default: break;                                          // CONT: resumed above
  }
};

/* ---- interval timers (docs/archive/0044) ----
 * One kernel-side ITIMER_REAL per process; expiry posts SIGALRM through
 * _deliver (so disposition, blocking, and the DFL-terminate action all
 * behave exactly like any other signal — the acceptance "no handler
 * installed terminates" falls out of the disposition mirror). Delivery is
 * cooperative like all signals: a pure-compute loop observes SIGALRM only
 * at its next safe point (settled 0001 caveat). Wall-clock, so a STOPPED
 * process's timer keeps running (POSIX ITIMER_REAL is real time); the
 * pending bit then delivers after SIGCONT. it_interval reloads from "now"
 * at each expiry (setTimeout latency doesn't accumulate into a backlog of
 * SIGALRMs — one pending bit is all the SAB can represent anyway). */

Kernel.prototype._setitimer = function (pcb, which, valueMs, intervalMs) {
  if (which !== ITIMER_REAL) return { errno: 'EINVAL' };
  var old = this._itimerRemaining(pcb);
  this._itimerClear(pcb);
  if (valueMs > 0) this._itimerArm(pcb, valueMs, Math.max(0, intervalMs));
  return old;
};

Kernel.prototype._itimerRemaining = function (pcb) {
  if (!pcb.itimer) return { valueMs: 0, intervalMs: 0 };
  return {
    valueMs: Math.max(1, pcb.itimer.expiresAt - Date.now()),  // armed reads >0 (POSIX: 0 means disarmed)
    intervalMs: pcb.itimer.intervalMs,
  };
};

Kernel.prototype._itimerArm = function (pcb, valueMs, intervalMs) {
  var self = this;
  pcb.itimer = {
    expiresAt: Date.now() + valueMs,
    intervalMs: intervalMs,
    timer: setTimeout(function () { self._itimerFire(pcb); }, valueMs),
  };
};

Kernel.prototype._itimerFire = function (pcb) {
  if (pcb.state === STATE_ZOMBIE || !pcb.itimer) return;   // raced an exit/cancel
  var interval = pcb.itimer.intervalMs;
  // Re-arm BEFORE delivering: a handler's getitimer sees the reloaded
  // value, and if delivery terminates the process (DFL) _exitProcess
  // clears the fresh timer along with everything else.
  if (interval > 0) this._itimerArm(pcb, interval, interval);
  else pcb.itimer = null;
  this._deliver(pcb, SIG.ALRM);
};

Kernel.prototype._itimerClear = function (pcb) {
  if (!pcb.itimer) return;
  clearTimeout(pcb.itimer.timer);
  pcb.itimer = null;
};

/* ---- job control (docs/archive/0003) ----
 * Stop is cooperative, like signal delivery: the kernel sets KP_FLAGS.STOP
 * and rings; the process parks inside KernelClient.sigpoll at its next safe
 * point (so a pure-compute loop stops only at its next env import — same
 * caveat as catchable signals; SIGKILL remains the backstop). A process
 * already parked in a deferred RPC simply stays parked; if the RPC completes
 * while stopped, the worker runs to the next safe point and parks there. */

Kernel.prototype._stopProcess = function (pcb, sig) {
  if (pcb.state !== STATE_RUNNING) return;             // already stopped, or a zombie
  pcb.state = STATE_STOPPED;
  pcb.pendingStop = sig;                               // unreported for WUNTRACED
  pcb.pendingCont = false;
  Atomics.or(pcb.i32, KP_FLAGS, KF_STOP);
  this._ring(pcb);
  if (pcb.threads) pcb.threads.forEach(function (th) { Atomics.or(th.i32, KP_FLAGS, KF_STOP); Atomics.add(th.i32, KP_DOORBELL, 1); Atomics.notify(th.i32, KP_DOORBELL); });
  if (pcb.tty) pcb.tty.wakeReaders();                  // ring-mode reads re-scan -> safe point
  this._jobNotifyParent(pcb, 'stop');
};

Kernel.prototype._contProcess = function (pcb) {
  if (pcb.state !== STATE_STOPPED) return;
  pcb.state = STATE_RUNNING;
  pcb.pendingStop = 0;
  pcb.pendingCont = true;                              // unreported for WCONTINUED
  Atomics.and(pcb.i32, KP_FLAGS, ~KF_STOP);
  this._ring(pcb);                                     // wakes the stop-park
  if (pcb.threads) pcb.threads.forEach(function (th) { Atomics.and(th.i32, KP_FLAGS, ~KF_STOP); Atomics.add(th.i32, KP_DOORBELL, 1); Atomics.notify(th.i32, KP_DOORBELL); });
  this._jobNotifyParent(pcb, 'cont');
  // A read parked across the stop was deliberately skipped by the serve
  // loops (#647: a stopped process is parked, not a consumer) — re-run the
  // stream's notify now so data or EOF that arrived during the stop is
  // served at resume. Without this a resumed pipe reader hangs forever on
  // bytes already sitting in the buffer (a tty self-heals on the next
  // keystroke; a pipe has no such heartbeat).
  var w = pcb.waiter;
  if (w) {
    if (w.op === 'piperead' || w.op === 'pipewrite') this._pipeNotify(w.pipe);
    else if (w.op === 'ttyread') this._ttyNotify(w.tty);
  }
};

/* SIGCHLD + a parked wait answer for a stop/continue transition — the same
 * ordering discipline as _exitProcess (pending bit visible before the wait
 * response wakes the parent). */
Kernel.prototype._jobNotifyParent = function (pcb, kind) {
  var parent = this._procs.get(pcb.ppid);
  if (!parent || parent.state === STATE_ZOMBIE) return;
  this._deliver(parent, SIG.CHLD);
  this._wakeChildWaiters(parent);
  this._ring(parent);
};

/* ============================================================
 * RemoteFS — the process-side client of the brokered filesystem.
 *
 * Implements the same JS method surface (names, arguments, null +
 * _lastError conventions) that BlockFS.prototype.toWasmEnv dispatches to
 * via `this.`, so the wasm env is built by REUSING toWasmEnv over this
 * object: BLOCK_FS.BlockFS.prototype.toWasmEnv.call(remoteFs, ctx). Two
 * env entries need overriding afterwards (isatty and __select_impl — their
 * toWasmEnv versions consult in-process state that doesn't exist here);
 * everything else flows through these methods as RPCs.
 *
 * Process-side read-only volume (docs/archive/0180): with opts.roFs (a LOCAL
 * BlockFS over the kernel-shipped system-image SAB, createV4 readonly) +
 * opts.roPrefix, absolute paths that lexically resolve under the prefix
 * are served IN-PROCESS — zero RPCs for the chattiest startup traffic
 * (fonts, configs, assets under /usr/share). Correctness rules:
 * - A symlink escaping the sealed volume (the /usr/local -> /var/local
 *   class) aborts the local walk (__mountEscape via the MountFS hooks) and
 *   the op retries brokered — the fast path only serves walks that stay
 *   inside the sealed volume. Local errors otherwise are FINAL (the sealed
 *   volume is complete: ENOENT under /usr is real).
 * - Write-intent opens and all path mutators stay brokered: the kernel's
 *   walk owns the EROFS-after-walk rule (docs/archive/0040).
 * - Relative paths stay brokered (the kernel owns the cwd).
 * - Local fds live at RO_FD_BASE+ so they can't collide with kernel fds;
 *   the kernel never sees them. The two places one must become
 *   kernel-visible — dup2 to a low fd, spawn DUP2 file-actions (hush's
 *   `cmd < /usr/...` redirect journal) — promote it: a temporary brokered
 *   O_RDONLY twin of the same file at the same offset (_roPromote).
 * - Documented limits: a local fd not named in any spawn action behaves
 *   as if close-on-exec (children inherit kernel fds only); select/WAIT
 *   can't name a local fd (the fd number exceeds FD_SETSIZE — regular
 *   files are always ready anyway). dup/dup2 within the local space share
 *   the offset like kernel OFDs (BlockFS _dupEntry reuses the entry).
 * ============================================================ */
var RO_FD_BASE = 0x100000;   // local (process-served) fd/dir-handle space

/* Lexical collapse of an ABSOLUTE path (the MountFS._resolvePath algorithm
 * without a cwd — relative paths never reach the fast path). */
function roNormalize(path) {
  var parts = path.split('/'), out = [];
  for (var i = 0; i < parts.length; i++) {
    var p = parts[i];
    if (p === '' || p === '.') continue;
    if (p === '..') { out.pop(); continue; }
    out.push(p);
  }
  return '/' + out.join('/');
}

/* Bounded kernel page cache for ordinary read-only descriptors. Data is shared
 * across opens/processes; offsets remain the backend's actual OFD position.
 * A worker commits a read/seek with one generation-tagged offset CAS. Owner
 * access clears the enable bit first, so brokered operations and descriptor
 * replacement cannot race a local offset update. No worker holds a lock that
 * could strand the owner if that worker is killed.
 *
 * Store notifications invalidate before mutations, including non-RPC writes.
 * Readers copy into bounded scratch, validate page stamps and the store epoch,
 * then commit the offset and copy out. Writable mapped files stay brokered:
 * their stores bypass ByteStore until beforeIO flushes them.
 */
var RFC_PAGE = 4096, RFC_PAGES = 1024, RFC_FDS = 4096;
var RFC_ENABLE = 1n << 63n, RFC_POS = 0xffffffffn, RFC_MAX = (1n << 63n) - 1n;
function readCacheViews(sab) {
  var at = 64;
  var ofds = new BigUint64Array(sab, at, RFC_FDS); at += RFC_FDS * 8;
  var stamps = new BigUint64Array(sab, at, RFC_PAGES * 2); at += RFC_PAGES * 16;
  var meta = new Uint32Array(sab, at, RFC_PAGES * 8); at += RFC_PAGES * 32;
  return {sab:sab, epoch:new BigUint64Array(sab,0,1), ofds:ofds,
          stamps:stamps, meta:meta, data:new Uint8Array(sab,at)};
}
function readCacheSlot(dev, ino, page) {
  return (Math.imul(dev, 0x9e3779b1) ^ ino ^ Math.floor(ino / 4294967296) ^ Math.imul(page, 0x85ebca6b)) & (RFC_PAGES - 1);
}
function FileReadCache(kernel) {
  this.kernel=kernel;
  this.v=readCacheViews(new SharedArrayBuffer(64+RFC_FDS*8+RFC_PAGES*(48+RFC_PAGE)));
  Atomics.store(this.v.epoch,0,1n);
  this.free=Array.from({length:RFC_FDS},(_,i)=>RFC_FDS-1-i);
  this.generations=new Uint32Array(RFC_FDS);
  this.stores=new WeakSet(); this.touched=new Set(); this.stamp=1n;
}
FileReadCache.prototype.invalidate=function(){
  var n=Atomics.load(this.v.epoch,0);
  if(n)Atomics.store(this.v.epoch,0,n===RFC_MAX?0n:n+1n);
};
FileReadCache.prototype.attach=function(o,st){
  var fs=this.kernel._fs;
  if(!fs.readCapability || !st || st.size>Number(RFC_POS))return null;
  var cap=fs.readCapability(o.bfsFd);
  if(!cap || !this.free.length)return null;
  // A backend entry already bound by another OFD must retain its existing
  // authority; don't install a second position cell on the same entry.
  if(Object.getOwnPropertyDescriptor(cap.entry,'position')?.get)return null;
  var slot=this.free.pop(),gen=++this.generations[slot];
  if(gen>0x7fffffff)return null; // retire rather than wrap an ABA generation
  var self=this,base=BigInt(gen)<<32n,position=cap.entry.position;
  if(!Number.isSafeInteger(position)||position<0||position>Number(RFC_POS)){this.free.push(slot);return null;}
  if(!this.stores.has(cap.store)){
    this.stores.add(cap.store);cap.store.watchWrites(function(){self.invalidate();});
  }
  var r={slot:slot,base:base,cap:cap,ofd:o,active:true,dev:st.dev||0,ino:st.ino};
  o.readCache=r;
  Atomics.store(this.v.ofds,slot,base|BigInt(position));
  Object.defineProperty(cap.entry,'position',{configurable:true,enumerable:true,
    get:function(){self.touched.add(r);return Number(Atomics.and(self.v.ofds,slot,~RFC_ENABLE)&RFC_POS);},
    set:function(p){
      if(!Number.isSafeInteger(p)||p<0||p>Number(RFC_POS)){self.detach(o,p);return;}
      self.touched.add(r);Atomics.store(self.v.ofds,slot,base|BigInt(p));
    }});
  this.touched.add(r);
  return {slot:slot,generation:gen,dev:r.dev,ino:r.ino};
};
FileReadCache.prototype.detach=function(o,position){
  var r=o.readCache;if(!r)return;
  var p=Atomics.and(this.v.ofds,r.slot,~RFC_ENABLE)&RFC_POS;
  r.active=false;this.touched.delete(r);o.readCache=null;
  Object.defineProperty(r.cap.entry,'position',{configurable:true,enumerable:true,writable:true,value:position===undefined?Number(p):position});
  this.free.push(r.slot);
};
FileReadCache.prototype.resume=function(){
  for(var r of this.touched)if(r.active)Atomics.or(this.v.ofds,r.slot,RFC_ENABLE);
  this.touched.clear();
};
FileReadCache.prototype.fill=function(o,start,count,readBytes,readCount){
  var r=o.readCache,fs=this.kernel._fs,v=this.v;
  if(!r || !count || !Atomics.load(v.epoch,0))return;
  // Mappings use a separate coherent arena. A writable file mapping can
  // change without a ByteStore notification; use the established broker.
  if(this.kernel._mapped?.files.get(o.ino)?.writable)return;
  var st=fs.fstat(o.bfsFd);if(!st||st.size>Number(RFC_POS))return;
  var atime=st.atime*1000+(st.atimeNsec||0)/1e6;
  if(!r.cap.readonly && (atime<=st.mtime*1000+(st.mtimeNsec||0)/1e6 || atime<=st.ctime*1000+(st.ctimeNsec||0)/1e6))return;
  var first=Math.floor(start/RFC_PAGE),last=Math.floor((start+Math.max(1,Math.min(count,Math.max(0,st.size-start)))-1)/RFC_PAGE);
  for(var page=first;page<=last;page++){
    var slot=readCacheSlot(r.dev,r.ino,page),m=slot*8,s=slot*2;
    Atomics.store(v.stamps,s,0n);
    var bytes=v.data.subarray(slot*RFC_PAGE,(slot+1)*RFC_PAGE);
    var pageStart=page*RFC_PAGE,pageCount=Math.min(RFC_PAGE,Math.max(0,st.size-pageStart));
    var n;
    if(readBytes && pageStart>=start && pageStart+pageCount<=start+readCount){
      n=pageCount;bytes.set(readBytes.subarray(pageStart-start,pageStart-start+n));
    }else n=fs.pread(o.bfsFd,bytes,RFC_PAGE,pageStart);
    if(n===null)continue;
    if(this.stamp===RFC_MAX){Atomics.store(v.epoch,0,0n);return;}
    v.meta[m]=r.dev;v.meta[m+1]=r.ino>>>0;v.meta[m+2]=Math.floor(r.ino/4294967296);
    v.meta[m+3]=page;v.meta[m+4]=n;v.meta[m+5]=st.size;
    Atomics.store(v.stamps,s+1,Atomics.load(v.epoch,0));
    Atomics.store(v.stamps,s,this.stamp++);
  }
};
function remoteReadCacheRead(remote,fd,buf,count){
  var v=remote._readCache,cap=remote._fdTable[fd]?.readCache;
  if(!v||!cap||!Number.isInteger(count)||count<0||count>KP_FS_CHUNK||buf.length<count)return undefined;
  var expected=RFC_ENABLE|(BigInt(cap.generation)<<32n);
  for(var attempt=0;attempt<3;attempt++){
    var word=Atomics.load(v.ofds,cap.slot);
    if((word&~RFC_POS)!==expected)return undefined;
    if(!count)return 0;
    var epoch=Atomics.load(v.epoch,0);if(!epoch)return undefined;
    var start=Number(word&RFC_POS),done=0,n=count,stamps=[];
    var scratch=remote._readScratch||(remote._readScratch=new Uint8Array(KP_FS_CHUNK));
    while(done<n){
      var pos=start+done,page=Math.floor(pos/RFC_PAGE),inside=pos%RFC_PAGE;
      var slot=readCacheSlot(cap.dev,cap.ino,page),m=slot*8,s=slot*2;
      var stamp=Atomics.load(v.stamps,s);
      if(!stamp||Atomics.load(v.stamps,s+1)!==epoch||v.meta[m]!==cap.dev||v.meta[m+1]!==cap.ino>>>0||v.meta[m+2]!==Math.floor(cap.ino/4294967296)||v.meta[m+3]!==page)return undefined;
      n=Math.min(n,Math.max(0,v.meta[m+5]-start));
      var take=Math.min(n-done,RFC_PAGE-inside);
      if(take<0||inside+take>v.meta[m+4]&&take)return undefined;
      scratch.set(v.data.subarray(slot*RFC_PAGE+inside,slot*RFC_PAGE+inside+take),done);
      stamps.push([s,stamp]);done+=take;
      if(!take)break;
    }
    if(start+done>Number(RFC_POS))return undefined;
    if(Atomics.load(v.epoch,0)!==epoch||stamps.some(function(p){return Atomics.load(v.stamps,p[0])!==p[1];}))continue;
    if(Atomics.compareExchange(v.ofds,cap.slot,word,expected|BigInt(start+done))!==word)continue;
    buf.set(scratch.subarray(0,done));return done;
  }
  return undefined;
}
function remoteReadCacheSeek(remote,fd,offset,whence){
  var v=remote._readCache,cap=remote._fdTable[fd]?.readCache;
  if(!v||!cap||!Number.isSafeInteger(offset)||(whence!==0&&whence!==1))return undefined;
  var expected=RFC_ENABLE|(BigInt(cap.generation)<<32n);
  for(var attempt=0;attempt<3;attempt++){
    var word=Atomics.load(v.ofds,cap.slot);if((word&~RFC_POS)!==expected)return undefined;
    var pos=offset+(whence===1?Number(word&RFC_POS):0);
    if(!Number.isSafeInteger(pos)||pos<0||pos>Number(RFC_POS))return undefined;
    if(Atomics.compareExchange(v.ofds,cap.slot,word,expected|BigInt(pos))===word)return pos;
  }
  return undefined;
}

function RemoteFS(client, opts) {
  this._c = client;
  this._readCache=opts && opts.fileReadCache?readCacheViews(opts.fileReadCache):null;
  this._lastError = null;
  // Presence markers for kernel fds ({type:'remote'}, set as fds are handed
  // out). fds 0-2 need NO entry: toWasmEnv's console fast path is gated on
  // the POSITIVE `console` capability only BlockFS's own default entries
  // carry (code-debt CD27), so stdio here falls through to this.write /
  // this.read — the FS_WRITE/FS_READ RPCs — with no decoy to plant.
  this._fdTable = [];
  this._dirs = [];              // opendir snapshots: {entries, pos}
  // Read (as an always-null guard) by toWasmEnv's tty imports over this
  // object (__tcsetattr, isatty pre-override) — stdin flows via FS_READ
  // RPCs, so no ring path may ever engage (docs/archive/0011).
  this._stdinSab = null;
  this._stdinCtrl = null;       // winsize words only (TIOCGWINSZ)
  this._pipes = new Map();      // fd -> ring views {i32,u8,cap,sab,end} (docs/archive/0181)
  // The read-only fast path (docs/archive/0180; header comment above).
  this._ro = null;
  var hasRoDev = opts && Object.prototype.hasOwnProperty.call(opts, 'roDev');
  if (hasRoDev) {
    checkedVolumeDev(opts.roDev);
    checkedRoPrefix(opts.roPrefix);
    if (!opts.roFs || opts.roFs._readonly !== true)
      throw new TypeError('filesystem identity: local RO reader must be readonly');
  }
  var hasRoLeaf = opts && Object.prototype.hasOwnProperty.call(opts, 'roLeaf');
  if (hasRoLeaf && typeof opts.roLeaf !== 'boolean')
    throw new TypeError('filesystem identity: RO leaf coverage must be boolean');
  // Manual embedders must attest that this prefix has no descendant mounts.
  // Without that trusted owner assertion, use the broker for every operation.
  if (hasRoDev && opts.roLeaf === true) {
    var roFs = opts.roFs, prefix = opts.roPrefix;
    var owns = function (full) {
      if (prefix === '/') return full;
      if (full === prefix) return '/';
      if (full.lastIndexOf(prefix + '/', 0) === 0) return full.slice(prefix.length);
      return null;
    };
    // Mount hooks (the MountFS wiring, docs/archive/0026): in-volume symlink
    // targets resolve in the FULL namespace; foreign ones throw
    // __mountEscape, which _roLocal turns into the brokered fallback.
    roFs._mountPrefix = prefix;
    roFs._mountOwns = owns;
    this._ro = { fs: roFs, prefix: prefix, dev: opts.roDev, owns: owns,
                 fds: new Map() };   // localFd -> { path } (full-namespace)
  }
}

/* The fast-path gate: volume-relative path when `path` is absolute and
 * lexically under the prefix, else null (brokered). A '..' climbing out
 * ('/usr/../etc') collapses first and correctly fails the prefix test. */
RemoteFS.prototype._roRel = function (path) {
  if (this._ro === null || typeof path !== 'string' || path.charAt(0) !== '/') return null;
  return this._ro.owns(roNormalize(path));
};

RemoteFS.prototype._roFd = function (fd) {
  return this._ro !== null && this._ro.fds.has(fd);
};

/* Run a local-volume path op; a cross-volume symlink escape retries
 * brokered. Local errors are final — propagate via _lastError. */
RemoteFS.prototype._roLocal = function (fn, brokered) {
  try {
    var r = fn();
    if (r === null) this._lastError = this._ro.fs._lastError || 'EIO';
    return r;
  } catch (e) {
    if (e && e.__mountEscape) return brokered();
    throw e;
  }
};

/* Dispatch a path op: local when the path gates in, else brokered. */
RemoteFS.prototype._roPath = function (path, localFn, brokeredFn) {
  var rel = this._roRel(path);
  if (rel === null) return brokeredFn();
  var self = this;
  return this._roLocal(function () { return localFn(self._ro.fs, rel); }, brokeredFn);
};

/* Materialize a kernel-side twin of a local fd (same file, same offset) —
 * the one place a local fd becomes kernel-visible (dup2 to a low fd, spawn
 * DUP2 file-actions). O_RDONLY re-open of the recorded full-namespace
 * path: the volume is immutable, so the twin is the same file by
 * construction. Caller closes the twin when done. */
RemoteFS.prototype._roPromote = function (fd) {
  var meta = this._ro.fds.get(fd);
  var pos = this._ro.fs.lseek(fd - RO_FD_BASE, 0, 1 /* SEEK_CUR */);
  var r = this._ok(this._c.call(OP.FS_OPEN, { path: meta.path, flags: 0, mode: 0 }));
  if (r === null) return null;
  this._fdTable[r.fd] = { type: 'remote' };
  if (pos) {
    var s = this._ok(this._c.call(OP.FS_LSEEK, { fd: r.fd, offset: pos, whence: 0 }));
    if (s === null) { this.close(r.fd); return null; }
  }
  return r.fd;
};

/* Wrap the spawnHooks spawn() so DUP2 file-actions whose SOURCE is a local
 * fd (hush's `cmd < /usr/...` redirect journal — pv_open3 really opened in
 * the parent, locally) cross into the kernel's fd table: each such fd gets
 * a temporary brokered twin the child inherits, the action is rewritten to
 * name it, and the parent closes the twin after the spawn returns (the
 * child holds its own refs). The twin is also closed CHILD-side by an
 * appended CLOSE action — unless some action already targets that fd
 * number, in which case the fd there is no longer the twin and closing it
 * would destroy the caller's redirect. Identity without the RO volume. */
RemoteFS.prototype.wrapSpawnHooks = function (hooks) {
  if (this._ro === null) return hooks;
  var self = this;
  var inner = hooks.spawn;
  hooks.spawn = function (spec) {
    var actions = spec.actions || [];
    if (self._ro.fds.size === 0 || actions.length === 0) return inner(spec);
    var temps = null;   // localFd -> kernel twin
    for (var i = 0; i < actions.length; i++) {
      var a = actions[i];
      if (a.op !== 0 || !self._roFd(a.arg)) continue;
      if (temps === null) temps = new Map();
      var k = temps.get(a.arg);
      if (k === undefined) {
        k = self._roPromote(a.arg);
        if (k === null) {
          var err = self._lastError || 'EBADF';
          temps.forEach(function (tfd) { self.close(tfd); });
          return { errno: err };
        }
        temps.set(a.arg, k);
      }
      actions[i] = { op: a.op, fd: a.fd, arg: k, path: a.path, mode: a.mode };
    }
    if (temps !== null) {
      temps.forEach(function (tfd) {
        var taken = actions.some(function (a) {
          return (a.op === 0 || a.op === 1) && a.fd === tfd;
        });
        if (!taken) actions.push({ op: 2, fd: tfd, arg: 0, path: null, mode: 0 });
      });
    }
    var r = inner(spec);
    if (temps !== null) temps.forEach(function (tfd) { self.close(tfd); });
    return r;
  };
  return hooks;
};

RemoteFS.prototype._setErr = function (name) { this._lastError = name; return null; };
RemoteFS.prototype._ok = function (resp) {
  return (resp && resp.errno) ? this._setErr(resp.errno) : resp;
};
/* Winsize-only wiring: keep _stdinSab null so no ring path ever engages. */
RemoteFS.prototype.setStdinSab = function (sab) {
  this._stdinCtrl = sab ? new Int32Array(sab, 0, 8) : null;
};

RemoteFS.prototype.open = function (path, flags, mode) {
  // RO fast path (docs/archive/0180): read-only opens of paths under the sealed
  // volume open locally. Write intent — access mode, O_CREAT|O_TRUNC|
  // O_APPEND (0x640) — stays brokered: the kernel's walk owns the
  // EROFS-after-walk rule and the /usr/local escape (docs/archive/0040).
  var rel = this._roRel(path);
  if (rel !== null && (flags & 3) === 0 && (flags & 0x640) === 0) {
    var self = this;
    return this._roLocal(function () {
      var volFd = self._ro.fs.open(rel, flags, mode);
      if (volFd === null) return null;
      self._ro.fds.set(RO_FD_BASE + volFd,
        { path: self._ro.prefix + (rel === '/' ? '' : rel) });
      return RO_FD_BASE + volFd;
    }, function () { return self._openBrokered(path, flags, mode); });
  }
  return this._openBrokered(path, flags, mode);
};
RemoteFS.prototype._openBrokered = function (path, flags, mode) {
  var r = this._ok(this._c.call(OP.FS_OPEN, { path: path, flags: flags, mode: mode, readCache:!!this._readCache }));
  if (r === null) return null;
  this._fdTable[r.fd] = { type: 'remote', readCache:r.readCache||null };
  return r.fd;
};
RemoteFS.prototype.close = function (fd) {
  if (this._roFd(fd)) {
    this._ro.fds.delete(fd);
    var lr = this._ro.fs.close(fd - RO_FD_BASE);
    if (lr === null) return this._setErr(this._ro.fs._lastError || 'EIO');
    return 0;
  }
  var r = this._ok(this._c.call(OP.FS_CLOSE, { fd: fd }));
  if (r === null) return null;
  delete this._fdTable[fd];
  this._pipes.delete(fd);
  return 0;
};
RemoteFS.prototype.read = function (fd, buf, count) {
  var cached=remoteReadCacheRead(this,fd,buf,count);if(cached!==undefined)return cached;
  if (this._roFd(fd)) {
    // Local regular-file read: whole count in one shot (no RPC chunk cap),
    // never parks (no EINTR concern — imports return promptly and the
    // env-import probe stays the signal safe point).
    var n0 = this._ro.fs.read(fd - RO_FD_BASE, buf, count);
    if (n0 === null) return this._setErr(this._ro.fs._lastError || 'EIO');
    return n0;
  }
  // SPSC fast path (docs/archive/0181): consume the shared ring locally while the
  // kernel says FAST. Drain-before-EOF (POSIX); empty parks in FS_WAIT.
  var pr = this._pipes.get(fd);
  if (pr && pr.end === 'read') {
    while (Atomics.load(pr.i32, PR_MODE) === PR_FAST) {
      var avail = pipeRingAvail(pr);
      if (avail > 0) {
        var rn = Math.min(count, avail);
        pipeRingTakeInto(pr, buf, rn);
        if (Atomics.load(pr.i32, PR_WWAIT)) this._c.call(OP.PIPE_KICK, { fd: fd });
        return rn;
      }
      if (Atomics.load(pr.i32, PR_FLAGS) & PRF_WGONE) return 0;   // EOF
      var rw = this._c.call(OP.FS_WAIT, { r: [fd], timeoutMs: null }, true);
      if (rw.errno) return this._setErr(rw.errno);   // EINTR: libc retries post-dispatch
    }
    // Demoted mid-stream (or never promoted): fall through brokered — the
    // kernel serves the very same ring, so nothing is lost.
  }
  // Interruptible: a tty read defers kernel-side; a posted signal turns it
  // into EINTR (regular files respond immediately, so it never fires).
  var r = this._c.call(OP.FS_READ, { fd: fd, count: Math.min(count, KP_FS_CHUNK) }, true);
  if (r.errno) {
    // The C surface only sees errno; keep the transport's real error text
    // visible (the ticket-#78 rule — http transfer failures carry one).
    if (r.error) console.error('read(fd ' + fd + '): ' + r.errno + ': ' + r.error);
    return this._setErr(r.errno);
  }
  if (!r.raw) return this._setErr('EIO');
  buf.set(r.raw);
  var got = r.raw.length;
  // Regular-file fill (docs/archive/0140): a single guest read() of a regular file
  // must return min(count, bytes-to-EOF) in ONE call — matching native/Node's
  // fs.readSync and the RO-volume fast path above — NOT one KP_FS_CHUNK-capped
  // chunk. Each FS_READ reply is bounded by the transport payload, so a
  // >KP_FS_CHUNK request came back short; we loop the RPC until the caller's
  // count is satisfied or EOF. Programs that trust a regular-file read to fill
  // (mGBA's unlooped 16 MB ROM load was the first victim — the zero-tailed ROM
  // derailed the emulated CPU) were silently truncated. SCOPE: regular files
  // ONLY — ttys/pipes/sockets/char-devices keep POSIX short-read semantics
  // (looping them would block on data that may never come), so we only fill
  // when the first chunk came back FULL (a short first chunk is already EOF)
  // and fstat confirms S_IFREG. The classification (one extra RPC) is paid
  // only on the rare >KP_FS_CHUNK read whose first chunk filled — ordinary
  // small reads never reach it.
  if (got < count && got === KP_FS_CHUNK && this._isRegularFd(fd)) {
    while (got < count) {
      var want = Math.min(count - got, KP_FS_CHUNK);
      var rr = this._c.call(OP.FS_READ, { fd: fd, count: want }, true);
      if (rr.errno) return got > 0 ? got : this._setErr(rr.errno);
      if (!rr.raw || rr.raw.length === 0) break;   // EOF
      buf.set(rr.raw, got);
      got += rr.raw.length;
      if (rr.raw.length < want) break;             // short chunk = EOF, done
    }
  }
  return got;
};
/* Regular-file test for read-fill scoping (docs/archive/0140): fstat the fd and
 * check S_IFREG. Every path-opened brokered fd is a 'file' OFD kernel-side,
 * but inherited/dup'd fds carry no process-side kind, so we ask the kernel. */
RemoteFS.prototype._isRegularFd = function (fd) {
  var r = this._ok(this._c.call(OP.FS_FSTAT, { fd: fd }));
  return !!(r && r.st && (r.st.mode & 0xF000) === 0x8000);
};
RemoteFS.prototype.write = function (fd, buf, count) {
  if (this._roFd(fd)) {
    // Same volume class the kernel serves for /usr fds: EROFS.
    var lw = this._ro.fs.write(fd - RO_FD_BASE, buf, count);
    if (lw === null) return this._setErr(this._ro.fs._lastError || 'EIO');
    return lw;
  }
  // SPSC fast path (docs/archive/0181): produce into the shared ring while FAST.
  // Whole-or-block for PIPE_ATOMIC-small writes (POSIX PIPE_BUF), partial
  // landings above it — the brokered _streamWrite rules, verbatim.
  var pw = this._pipes.get(fd);
  if (pw && pw.end === 'write') {
    // Zero-length write: 0 immediately — the _streamWrite rule (Linux
    // "null write succeeds"): never FS_WAIT on a full ring, never
    // EPIPE/SIGPIPE a gone reader. (#644)
    if (count === 0) return 0;
    while (Atomics.load(pw.i32, PR_MODE) === PR_FAST) {
      if (Atomics.load(pw.i32, PR_FLAGS) & PRF_RGONE) {
        // Reader gone: EPIPE, and the kernel deals us our SIGPIPE (the
        // {epipe:1} kick) so default-action death matches brokered runs.
        this._c.call(OP.PIPE_KICK, { fd: fd, epipe: 1 });
        return this._setErr('EPIPE');
      }
      var free = pw.cap - pipeRingAvail(pw);
      if (free === 0 || (count <= PIPE_ATOMIC && count > free)) {
        var ww = this._c.call(OP.FS_WAIT, { w: [fd], timeoutMs: null }, true);
        if (ww.errno) return this._setErr(ww.errno);
        continue;
      }
      var wn = Math.min(free, count);
      pipeRingPut(pw, buf.subarray(0, wn));
      if (Atomics.load(pw.i32, PR_RWAIT)) this._c.call(OP.PIPE_KICK, { fd: fd });
      return wn;
    }
  }
  var n = Math.min(count, KP_FS_CHUNK);
  var payload = new Uint8Array(4 + n);
  payload[0] = fd & 0xff; payload[1] = (fd >> 8) & 0xff;
  payload[2] = (fd >> 16) & 0xff; payload[3] = (fd >> 24) & 0xff;
  payload.set(buf.subarray(0, n), 4);
  // Interruptible: a full-pipe write defers kernel-side; a posted signal
  // turns it into EINTR (files/tty/out respond immediately — never fires).
  var r = this._ok(this._c.callRaw(OP.FS_WRITE, payload, true));
  return r === null ? null : r.n;
};
RemoteFS.prototype.pread = function(fd,buf,count,offset) {
  if(!Number.isSafeInteger(offset)||offset<0)return this._setErr('EINVAL');
  if(this._roFd(fd)) {
    var n=this._ro.fs.pread(fd-RO_FD_BASE,buf,count,offset);
    return n===null?this._setErr(this._ro.fs._lastError||'EIO'):n;
  }
  var r=this._c.call(OP.FS_PREAD,{fd:fd,count:Math.min(count,KP_FS_CHUNK),offset:offset},true);
  if(r.errno)return this._setErr(r.errno);
  if(!r.raw)return this._setErr('EIO');
  buf.set(r.raw);return r.raw.length;
};
RemoteFS.prototype.pwrite = function(fd,buf,count,offset) {
  if(!Number.isSafeInteger(offset)||offset<0)return this._setErr('EINVAL');
  if(this._roFd(fd))return this._setErr('EROFS');
  var n=Math.min(count,KP_FS_CHUNK-8),payload=new Uint8Array(12+n),dv=new DataView(payload.buffer);
  dv.setInt32(0,fd,true);dv.setFloat64(4,offset,true);payload.set(buf.subarray(0,n),12);
  var r=this._ok(this._c.callRaw(OP.FS_PWRITE,payload,true));
  return r===null?null:r.n;
};
RemoteFS.prototype.lseek = function (fd, offset, whence) {
  var cached=remoteReadCacheSeek(this,fd,offset,whence);if(cached!==undefined)return cached;
  if (this._roFd(fd)) {
    var lr = this._ro.fs.lseek(fd - RO_FD_BASE, offset, whence);
    if (lr === null) return this._setErr(this._ro.fs._lastError || 'EIO');
    return lr;
  }
  var r = this._ok(this._c.call(OP.FS_LSEEK, { fd: fd, offset: offset, whence: whence }));
  return r === null ? null : r.offset;
};
RemoteFS.prototype.stat = function (p) {
  var self = this;
  return this._roPath(p,
    function (fs, rel) { var st = fs.stat(rel); return st === null ? null : Object.assign({}, st, { dev: self._ro.dev }); },
    function () { var r = self._ok(self._c.call(OP.FS_STAT, { path: p })); return r && r.st; });
};
RemoteFS.prototype.lstat = function (p) {
  var self = this;
  return this._roPath(p,
    function (fs, rel) { var st = fs.lstat(rel); return st === null ? null : Object.assign({}, st, { dev: self._ro.dev }); },
    function () { var r = self._ok(self._c.call(OP.FS_LSTAT, { path: p })); return r && r.st; });
};
RemoteFS.prototype.fstat = function (fd) {
  if (this._roFd(fd)) {
    var lr = this._ro.fs.fstat(fd - RO_FD_BASE);
    if (lr === null) return this._setErr(this._ro.fs._lastError || 'EIO');
    return Object.assign({}, lr, { dev: this._ro.dev });
  }
  var r = this._ok(this._c.call(OP.FS_FSTAT, { fd: fd })); return r && r.st;
};
RemoteFS.prototype.access = function (p, mode) {
  var self = this;
  return this._roPath(p,
    function (fs, rel) { return fs.access(rel, mode); },
    function () { return self._ok(self._c.call(OP.FS_ACCESS, { path: p, mode: mode })) && 0; });
};
RemoteFS.prototype.unlink = function (p) { return this._ok(this._c.call(OP.FS_UNLINK, { path: p })) && 0; };
RemoteFS.prototype.rename = function (a, b, noReplace) { return this._ok(this._c.call(OP.FS_RENAME, { from: a, to: b, noReplace: noReplace === true })) && 0; };
RemoteFS.prototype.mkdir = function (p, mode) { return this._ok(this._c.call(OP.FS_MKDIR, { path: p, mode: mode })) && 0; };
RemoteFS.prototype.rmdir = function (p) { return this._ok(this._c.call(OP.FS_RMDIR, { path: p })) && 0; };
RemoteFS.prototype.link = function (a, b) { return this._ok(this._c.call(OP.FS_LINK, { from: a, to: b })) && 0; };
RemoteFS.prototype.symlink = function (target, p) { return this._ok(this._c.call(OP.FS_SYMLINK, { target: target, path: p })) && 0; };
/* Buffer-style like BlockFS.readlink — toWasmEnv is reused over RemoteFS,
 * so the signatures must match (the RPC itself carries a string). */
RemoteFS.prototype.readlink = function (p, buf, bufsize) {
  var self = this;
  return this._roPath(p,
    function (fs, rel) { return fs.readlink(rel, buf, bufsize); },
    function () {
      var r = self._ok(self._c.call(OP.FS_READLINK, { path: p }));
      if (r === null) return null;
      var bytes = new TextEncoder().encode(r.target);
      var n = Math.min(bytes.length, bufsize);
      for (var i = 0; i < n; i++) buf[i] = bytes[i];
      return n;
    });
};
RemoteFS.prototype.ftruncate = function (fd, size) {
  if (this._roFd(fd)) {   // same volume class the kernel serves: EROFS
    var lr = this._ro.fs.ftruncate(fd - RO_FD_BASE, size);
    return lr === null ? this._setErr(this._ro.fs._lastError || 'EIO') : 0;
  }
  return this._ok(this._c.call(OP.FS_FTRUNCATE, { fd: fd, size: size })) && 0;
};
RemoteFS.prototype.fsync = function (fd) {
  if (this._roFd(fd)) return 0;   // read-only volume: nothing to flush
  return this._ok(this._c.call(OP.FS_FSYNC, { fd: fd })) && 0;
};
RemoteFS.prototype.chmod = function (p, mode) { return this._ok(this._c.call(OP.FS_CHMOD, { path: p, mode: mode })) && 0; };
RemoteFS.prototype.fchmod = function (fd, mode) {
  if (this._roFd(fd)) {
    var lr = this._ro.fs.fchmod(fd - RO_FD_BASE, mode);
    return lr === null ? this._setErr(this._ro.fs._lastError || 'EIO') : 0;
  }
  return this._ok(this._c.call(OP.FS_FCHMOD, { fd: fd, mode: mode })) && 0;
};
RemoteFS.prototype.utime = function (p, a, m) { return this._ok(this._c.call(OP.FS_UTIME, { path: p, atime: a, mtime: m })) && 0; };
RemoteFS.prototype.futime = function (fd, a, m) {
  if (this._roFd(fd)) {
    var lr = this._ro.fs.futime(fd - RO_FD_BASE, a, m);
    return lr === null ? this._setErr(this._ro.fs._lastError || 'EIO') : 0;
  }
  return this._ok(this._c.call(OP.FS_FUTIME, { fd: fd, atime: a, mtime: m })) && 0;
};
RemoteFS.prototype.chdir = function (p) { return this._ok(this._c.call(OP.FS_CHDIR, { path: p })) && 0; };
RemoteFS.prototype.fchdir = function (fd) { return this._ok(this._c.call(OP.FS_CHDIR, { fd: fd })) && 0; };
RemoteFS.prototype.getcwd = function () {
  var r = this._ok(this._c.call(OP.FS_GETCWD, {}));
  return r === null ? null : r.cwd;
};
RemoteFS.prototype.dup = function (fd) {
  if (this._roFd(fd)) {
    var meta = this._ro.fds.get(fd);
    var nf = this._ro.fs.dup(fd - RO_FD_BASE);
    if (nf === null) return this._setErr(this._ro.fs._lastError || 'EIO');
    this._ro.fds.set(RO_FD_BASE + nf, { path: meta.path });
    return RO_FD_BASE + nf;
  }
  var r = this._ok(this._c.call(OP.FS_DUP, { fd: fd }));
  if (r === null) return null;
  this._fdTable[r.fd] = { type: 'remote' };
  var dupRing = this._pipes.get(fd);
  if (dupRing) this._pipes.set(r.fd, dupRing);   // same end, same ring (docs/archive/0181)
  return r.fd;
};
RemoteFS.prototype.dup2 = function (oldfd, newfd) {
  if (this._roFd(oldfd)) {
    if (newfd === oldfd) return newfd;
    if (newfd >= RO_FD_BASE) {   // stays in the local space
      if (this._roFd(newfd)) this._ro.fds.delete(newfd);   // implicit close
      var ln = this._ro.fs.dup2(oldfd - RO_FD_BASE, newfd - RO_FD_BASE);
      if (ln === null) return this._setErr(this._ro.fs._lastError || 'EIO');
      this._ro.fds.set(newfd, { path: this._ro.fds.get(oldfd).path });
      return newfd;
    }
    // Crossing into the kernel fd space (redirect onto stdio & co): promote
    // a brokered twin, land it at newfd, drop the twin.
    var k = this._roPromote(oldfd);
    if (k === null) return null;
    var pr = this._ok(this._c.call(OP.FS_DUP2, { fd: k, newfd: newfd }));
    this._ok(this._c.call(OP.FS_CLOSE, { fd: k }));
    delete this._fdTable[k];
    if (pr === null) return null;
    this._fdTable[pr.fd] = { type: 'remote' };
    this._pipes.delete(newfd);   // newfd is the promoted file now (docs/archive/0181)
    return pr.fd;
  }
  if (this._roFd(newfd)) {   // remote source lands on a local number
    this.close(newfd);       // POSIX implicit close of newfd
  }
  var r = this._ok(this._c.call(OP.FS_DUP2, { fd: oldfd, newfd: newfd }));
  if (r === null) return null;
  this._fdTable[r.fd] = { type: 'remote' };
  // Pipe-ring bookkeeping (docs/archive/0181): newfd now names oldfd's OFD.
  if (oldfd !== newfd) {
    var d2Ring = this._pipes.get(oldfd);
    if (d2Ring) this._pipes.set(newfd, d2Ring);
    else this._pipes.delete(newfd);
  }
  return r.fd;
};
RemoteFS.prototype.getfl = function(fd) {
  if (this._roFd(fd)) return this._roLocal(() => this._ro.fs.getfl(fd - RO_FD_BASE));
  var r=this._ok(this._c.call(OP.FS_GETFL,{fd:fd}));return r===null?null:r.flags;
};
RemoteFS.prototype.setfl = function(fd, flags) {
  if (this._roFd(fd)) return this._roLocal(() => this._ro.fs.setfl(fd - RO_FD_BASE, flags));
  return this._ok(this._c.call(OP.FS_SETFL, {fd:fd, flags:flags})) === null ? null : 0;
};
RemoteFS.prototype.fdflags = function(fd,flags,set) {
  var r=this._ok(this._c.call(OP.FS_FDFLAGS,{fd:fd,flags:flags,set:set}));return r===null?null:(set?0:r.flags);
};
RemoteFS.prototype.fcntl_dupfd = function (fd, min, cloexec) {
  if (this._roFd(fd)) {
    var meta = this._ro.fds.get(fd);
    var nf = this._ro.fs.fcntl_dupfd(fd - RO_FD_BASE, Math.max(0, min - RO_FD_BASE));
    if (nf === null) return this._setErr(this._ro.fs._lastError || 'EIO');
    this._ro.fds.set(RO_FD_BASE + nf, { path: meta.path });
    return RO_FD_BASE + nf;
  }
  var r = this._ok(this._c.call(OP.FS_FCNTL_DUPFD, { fd: fd, min: min, cloexec:!!cloexec }));
  if (r === null) return null;
  this._fdTable[r.fd] = { type: 'remote' };
  var fdRing = this._pipes.get(fd);
  if (fdRing) this._pipes.set(r.fd, fdRing);     // same end, same ring (docs/archive/0181)
  return r.fd;
};
RemoteFS.prototype.opendir = function (p) {
  // Local dir handles share the RO_FD_BASE offset discipline (the volume's
  // own handle table allocates small ints, remote snapshots index _dirs).
  var rel = this._roRel(p);
  if (rel !== null) {
    var self = this;
    return this._roLocal(function () {
      var h = self._ro.fs.opendir(rel);
      return h === null ? null : RO_FD_BASE + h;
    }, function () { return self._opendirBrokered(p); });
  }
  return this._opendirBrokered(p);
};
RemoteFS.prototype._opendirBrokered = function (p) {
  var r = this._ok(this._c.call(OP.FS_OPENDIR, { path: p }));
  if (r === null) return null;
  var entries = r.entries;
  // Big-dir pagination (docs/archive/0241): a `more` cursor means the kernel
  // parked the open handle — drain it page by page before snapshotting,
  // so readdir/closedir/pos semantics are unchanged for every caller.
  while (r.more !== undefined) {
    r = this._ok(this._c.call(OP.FS_READDIR, { dir: r.more }));
    if (r === null) return null;
    for (var i = 0; i < r.entries.length; i++) entries.push(r.entries[i]);
  }
  this._dirs.push({ entries: entries, pos: 0 });
  return this._dirs.length - 1;
};
RemoteFS.prototype.readdir = function (h) {
  if (this._ro !== null && h >= RO_FD_BASE) return this._ro.fs.readdir(h - RO_FD_BASE);
  var d = this._dirs[h];
  if (!d || d.pos >= d.entries.length) return null;
  return d.entries[d.pos++];
};
RemoteFS.prototype.closedir = function (h) {
  if (this._ro !== null && h >= RO_FD_BASE) { this._ro.fs.closedir(h - RO_FD_BASE); return 0; }
  this._dirs[h] = undefined; return 0;
};
RemoteFS.prototype._resolvePath = function (p) {
  var r = this._c.call(OP.FS_REALPATH, { path: p });
  return r.errno ? p : r.path;   // best effort, like the lexical resolver
};
/* realpath(3) — the FS_REALPATH RPC now resolves symlinks PHYSICALLY
 * kernel-side (docs/archive/0263). Unlike _resolvePath above (best-effort, swallows
 * errors for lexical callers), this surfaces the failure: null + _lastError so
 * the toWasmEnv realpath import can return NULL like glibc/the standalone
 * flavor. One RPC — the per-component walk stays inside the kernel. */
RemoteFS.prototype.realpathPhysical = function (p) {
  var r = this._c.call(OP.FS_REALPATH, { path: p });
  return r.errno ? this._setErr(r.errno) : r.path;
};
RemoteFS.prototype.isatty = function (fd) {
  if (this._roFd(fd)) return 0;   // a sealed-volume regular file
  var r = this._c.call(OP.FS_ISATTY, { fd: fd });
  return r.errno ? 0 : r.tty;
};
/* SPSC pipe fast path (docs/archive/0181) — the process side.
 *
 * pipe() allocates the ring SAB and posts it ahead of PIPE_CREATE (the
 * audio-sab handshake); inherited fds arrive pre-registered via
 * procSpec.pipeRings. Every data op gates on the ring's PR_MODE word:
 * FAST (the kernel promoted — both ends single-holder) means read/write
 * are a local memcpy + index commit, ZERO RPCs in steady state; anything
 * else falls through to the brokered RPC, which the kernel serves FROM
 * THE SAME RING, so a stale mode read is never wrong, only slower.
 * Blocking rides the 0178 unified FS_WAIT naming this fd (no bespoke
 * park); after a commit the peer's PR_RWAIT/PR_WWAIT flag says whether to
 * ring the PIPE_KICK doorbell. Reader-gone EPIPE raises SIGPIPE through
 * PIPE_KICK{epipe:1} — the kernel delivers to the caller, matching the
 * brokered write's EPIPE+signal ordering. */
RemoteFS.prototype.registerPipeRing = function (fd, end, sab) {
  var v = pipeRingViews(sab);
  v.end = end;
  this._pipes.set(fd, v);
  if (!this._fdTable[fd]) this._fdTable[fd] = { type: 'remote' };
};
RemoteFS.prototype.pipe = function () {
  var sab = null;
  try { sab = new SharedArrayBuffer(PIPE_RING_BYTES); } catch (e) { sab = null; }
  if (sab) this._c._post({ type: 'pipe-sab', sab: sab });
  var r = this._ok(this._c.call(OP.PIPE_CREATE, {}));
  if (r === null) return null;
  this._fdTable[r.rfd] = { type: 'remote' };
  this._fdTable[r.wfd] = { type: 'remote' };
  if (sab) {
    this.registerPipeRing(r.rfd, 'read', sab);
    this.registerPipeRing(r.wfd, 'write', sab);
  }
  return [r.rfd, r.wfd];
};
/* Ptys (docs/archive/0020): kernel pty pair -> [masterFd, slaveFd]; the terminal
 * app resizes the pair through the master (TIOCSWINSZ -> SIGWINCH). */
RemoteFS.prototype.openpty = function () {
  var r = this._ok(this._c.call(OP.PTY_CREATE, {}));
  if (r === null) return null;
  this._fdTable[r.mfd] = { type: 'remote' };
  this._fdTable[r.sfd] = { type: 'remote' };
  return [r.mfd, r.sfd];
};
RemoteFS.prototype.ptyControl = function(fd,request) {
  var r=this._ok(this._c.call(OP.PTY_CONTROL,{fd:fd,request:request}));
  return r === null ? null : r.value;
};
RemoteFS.prototype.setWinsize = function (fd, rows, cols) {
  return this._ok(this._c.call(OP.TIOCSWINSZ, { fd: fd, rows: rows, cols: cols })) && 0;
};
/* AF_UNIX sockets (docs/archive/0008). Data flows through read/write above; only
 * the control plane needs methods. accept is interruptible (it parks
 * kernel-side until a connect arrives — EINTR per POSIX). */
RemoteFS.prototype.sockSocket = function () {
  var r = this._ok(this._c.call(OP.SOCK_SOCKET, {}));
  if (r === null) return null;
  this._fdTable[r.fd] = { type: 'remote' };
  return r.fd;
};
RemoteFS.prototype.sockBind = function (fd, path) { return this._ok(this._c.call(OP.SOCK_BIND, { fd: fd, path: path })) && 0; };
RemoteFS.prototype.sockListen = function (fd, backlog) { return this._ok(this._c.call(OP.SOCK_LISTEN, { fd: fd, backlog: backlog })) && 0; };
RemoteFS.prototype.sockConnect = function (fd, path) { return this._ok(this._c.call(OP.SOCK_CONNECT, { fd: fd, path: path })) && 0; };
RemoteFS.prototype.sockAccept = function (fd) {
  var r = this._ok(this._c.call(OP.SOCK_ACCEPT, { fd: fd }, true));
  if (r === null) return null;
  this._fdTable[r.fd] = { type: 'remote' };
  return r.fd;
};
RemoteFS.prototype.sockPair = function () {
  var r = this._ok(this._c.call(OP.SOCK_PAIR, {}));
  if (r === null) return null;
  this._fdTable[r.fd0] = { type: 'remote' };
  this._fdTable[r.fd1] = { type: 'remote' };
  return [r.fd0, r.fd1];
};
RemoteFS.prototype.sockRecv = function (fd, buf, count, flags) {
  var r = this._ok(this._c.call(OP.SOCK_RECV, {fd:fd,count:Math.min(count,KP_FS_CHUNK),flags:flags}, true));
  if (r === null) return null;
  if (!r.raw) return this._setErr('EIO');
  buf.set(r.raw); return r.raw.length;
};
RemoteFS.prototype.sockSend = function (fd, buf, count, flags) {
  var n = Math.min(count,KP_FS_CHUNK), payload = new Uint8Array(8+n), view = new DataView(payload.buffer);
  view.setInt32(0,fd,true); view.setInt32(4,flags,true); payload.set(buf.subarray(0,n),8);
  var r = this._ok(this._c.callRaw(OP.SOCK_SEND,payload,true));
  return r === null ? null : r.n;
};
RemoteFS.prototype.sockQuery = function (fd, query, peer, level, option, set) {
  return this._ok(this._c.call(OP.SOCK_QUERY, {fd:fd,query:query,peer:!!peer,level:level,option:option,set:!!set}));
};
RemoteFS.prototype.sockShutdown = function (fd, how) { return this._ok(this._c.call(OP.SOCK_SHUTDOWN, { fd: fd, how: how })) && 0; };
/* FS_WATCH (ticket #75): a path-keyed watch fd. mask 0 = the kernel's
 * settled default; flags reserved (EINVAL when nonzero). The fd reads
 * packed fsw_event records (os/fswatch.h), EAGAIN when dry, and is
 * readable in select()/__wait like any other fd. Close removes the watch. */
RemoteFS.prototype.fsWatch = function (path, mask, flags) {
  var r = this._ok(this._c.call(OP.FS_WATCH_OPEN, { path: path, mask: mask | 0, flags: flags | 0 }));
  if (r === null) return null;
  this._fdTable[r.fd] = { type: 'remote' };
  return r.fd;
};

RemoteFS.prototype.pollImpl = function(ctx, asyncWait) {
  var self=this;
  function poll(ptr,nfds,timeout) {
    nfds >>>= 0; ptr >>>= 0;
    var mem=new DataView(ctx.getMemory().buffer);
    if (nfds && (!ptr || nfds > (mem.byteLength-ptr)/8)) { ctx.setErrnoName('EFAULT'); return -1; }
    var fds=[], local=new Map();
    for(var i=0;i<nfds;i++) {
      var fd=mem.getInt32(ptr+i*8,true), events=mem.getUint16(ptr+i*8+4,true);
      if(self._roFd(fd)){local.set(i,events&0x45);fds.push([-1,events]);}
      else fds.push([fd,events]);
    }
    var localCount=0;local.forEach(re=>{if(re)localCount++;});
    if(nfds>1024) {
      for(var i=0;i<nfds;i+=1024) {
        var chunk=self._c.call(OP.FS_POLL,{start:i===0,chunk:fds.slice(i,i+1024)});
        if(chunk.errno){ctx.setErrnoName(chunk.errno);return -1;}
      }
    }
    var response=self._c[asyncWait?'callAsync':'call'](OP.FS_POLL,{fds:nfds<=1024?fds:undefined,timeoutMs:localCount?0:timeout},true);
    return asyncWait?response.then(finish):finish(response);
    function finish(r) {
    if(r.errno){ctx.setErrnoName(r.errno);return -1;}
    var count=r.count+localCount;
    for(var offset=0;offset<nfds;offset+=1024) {
      if(offset)r=self._c.call(OP.FS_POLL,{resultOffset:offset});
      if(r.errno){ctx.setErrnoName(r.errno);return -1;}
      mem=new DataView(ctx.getMemory().buffer);
      for(var j=0;j<r.revents.length;j++)mem.setInt16(ptr+(offset+j)*8+6,local.get(offset+j)??r.revents[j],true);
    }
    return count;
    }
  }
  return asyncWait?new WebAssembly.Suspending(async(...args)=>{
    const result=await ctx.awaitImport(poll(...args));
    // Suspending objects bypass runModule's ordinary-function signal wrapper.
    ctx.deliverSignals?.();return result;
  }):poll;
};

/* The brokered __select_impl (replaces toWasmEnv's in-process scanner):
 * fd-set bitmaps <-> fd lists; readiness, blocking, and timeout are all
 * kernel-side; interruption surfaces as EINTR per POSIX. */
RemoteFS.prototype.selectImpl = function (ctx, asyncWait) {
  var self = this;
  function select(nfds, readfds_ptr, writefds_ptr, exceptfds_ptr,
    timeout_sec, timeout_usec, has_timeout) {
    var mem = new DataView(ctx.getMemory().buffer);
    function toList(ptr) {
      if (!ptr) return [];
      var out = [];
      for (var fd = 0; fd < nfds && fd < 64; fd++) {
        if (mem.getInt32(ptr + ((fd >> 5) * 4), true) & (1 << (fd & 31))) out.push(fd);
      }
      return out;
    }
    var req = {
      r: toList(readfds_ptr), w: toList(writefds_ptr), e: toList(exceptfds_ptr),
      timeoutMs: has_timeout ? (timeout_sec * 1000 + timeout_usec / 1000) : null,
    };
    var response = self._c[asyncWait?'callAsync':'call'](OP.FS_SELECT, req, true);
    return asyncWait?response.then(finish):finish(response);
    function finish(resp) {
    if (resp.errno) { ctx.setErrnoName(resp.errno); return -1; }
    var out = new DataView(ctx.getMemory().buffer);
    function writeList(ptr, list) {
      if (!ptr) return;
      var b = [0, 0];
      list.forEach(function (fd) { b[fd >> 5] |= (1 << (fd & 31)); });
      out.setInt32(ptr, b[0], true);
      out.setInt32(ptr + 4, b[1], true);
    }
    writeList(readfds_ptr, resp.r || []);
    writeList(writefds_ptr, resp.w || []);
    writeList(exceptfds_ptr, resp.e || []);
    return (resp.r ? resp.r.length : 0) + (resp.w ? resp.w.length : 0) + (resp.e ? resp.e.length : 0);
    }
  }
  return asyncWait?new WebAssembly.Suspending(async(...args)=>{
    const result=await ctx.awaitImport(select(...args));
    ctx.deliverSignals?.();return result;
  }):select;
};

/* ============================================================
 * nodeCreateWorker — the tested Node reference createWorker factory.
 *
 *   var kernel = new Kernel({
 *     createWorker: nodeCreateWorker({ hostPath, kernelPath }),
 *     ...
 *   });
 *
 * Each process worker: fresh worker_thread running BOOT_SOURCE, which loads
 * host.js, builds a KernelClient over the kernel page, gives the process a
 * private in-memory BlockFS (per-process fs is Phase 1 scope — a shared
 * OPFS store is the browser story, a shared SAB store is future Node work),
 * chdirs to the spawn cwd, and runs the image. stdout/stderr flow back as
 * {type:'out'} messages (bytes copied out of wasm memory first — postMessage
 * would otherwise clone the whole wasm heap or ship a detached view).
 * In brokered mode fd_actions were already applied kernel-side at spawn;
 * they still arrive in workerData for the standalone path's benefit.
 * ============================================================ */
var BOOT_SOURCE = [
  "'use strict';",
  "var wt = require('worker_threads');",
  "var wd = wt.workerData;",
  "var runModule = require(wd.hostPath);",
  "var K = require(wd.kernelPath);",
  "var BLOCK_FS = runModule.BLOCK_FS;",
  "var client = new K.KernelClient(wd.kernelPage, function (m, t) { wt.parentPort.postMessage(m, t); });",
  "// The KERNEL owns this worker's lifetime — every process end path goes",
  "// through _exitProcess -> worker.terminate() (exit handshake, SIGKILL,",
  "// crash) — so an empty event loop must not end it. Node exits a worker",
  "// whose loop drains, and a pending Atomics.waitAsync holds no ref, so a",
  "// process parked in vsyncWait (the ONE async parker: every other wait is",
  "// a synchronous Atomics.wait that blocks the thread with the loop still",
  "// owing nothing) would die mid-park as a silent channel-death SIGSEGV.",
  "// Browser workers idle on an empty loop; this pins the Node twin to the",
  "// same semantics. First exposed by boot.js --vsync (#424).",
  "setInterval(function () {}, 0x7fffffff);",
  "var fsFactory;",
  "var rfs = null;",
  "if (wd.brokered) {",
  "  // The brokered filesystem: the kernel serves every fs syscall; the env",
  "  // is toWasmEnv REUSED over a RemoteFS (same method surface), with the",
  "  // two in-process-state entries overridden. wd.ro (docs/archive/0180) is the",
  "  // sealed system image as an SAB — mounted locally so reads under its",
  "  // prefix never RPC.",
  "  var roFs = wd.ro",
  "    ? BLOCK_FS.createV4(new BLOCK_FS.SabByteStore(wd.ro.sab), { readonly: true })",
  "    : null;",
  "  rfs = new K.RemoteFS(client, Object.assign({fileReadCache:wd.fileReadCache}, roFs ? { roFs: roFs, roPrefix: wd.ro.prefix, roDev: wd.ro.dev, roLeaf: wd.ro.leaf } : {}));",
  "  // SPSC pipe rings for inherited fds (docs/archive/0181): fast ops gate on the",
  "  // ring's PR_MODE word, so registering a still-brokered ring is free.",
  "  (wd.pipeRings || []).forEach(function (p) { rfs.registerPipeRing(p.fd, p.end, p.sab); });",
  "  fsFactory = function (ctx) {",
  "    var env = BLOCK_FS.BlockFS.prototype.toWasmEnv.call(rfs, ctx);",
  "    env.__select_impl = rfs.selectImpl(ctx); env.__poll_impl = rfs.pollImpl(ctx);",
  "    env.isatty = function (fd) { return rfs.isatty(fd); };",
  "    return Promise.resolve({ c: env });",
  "  };",
  "} else {",
  "  // Standalone arrangement (no shared fs): private in-memory BlockFS.",
  "  var store = new BLOCK_FS.MemoryByteStore(1 << 20);",
  "  var bfs = BLOCK_FS.createV4(store);",
  "  if (wd.cwd && wd.cwd !== '/') { try { bfs.chdir(wd.cwd); } catch (e) {} }",
  "  fsFactory = function (ctx) { return Promise.resolve({ c: bfs.toWasmEnv(ctx) }); };",
  "}",
  "function envObj(envp) { var o = {}; (envp || []).forEach(function (s) {",
  "  var i = s.indexOf('='); if (i > 0) o[s.slice(0, i)] = s.slice(i + 1); }); return o; }",
  "function ship(fd) { return function (b) {",
  "  var u = (b instanceof Uint8Array) ? b : new Uint8Array(b);",
  "  wt.parentPort.postMessage({ type: 'out', fd: fd, bytes: u.slice() }); }; }",
  "var instantiated = false;",
  "runModule({",
  "  onReady: function () { instantiated = true; },",
  "  bytes: wd.image || undefined,",
  "  module: wd.module || undefined,   // pre-compiled Module (docs/archive/0037)",
  "  args: wd.argv,",
  "  env: envObj(wd.envp),",
  "  stdinSab: wd.ttySab || undefined,",
  "  blockFsFactory: fsFactory,",
  "  writeOut: ship(1),",
  "  writeErr: ship(2),",
  "  // rfs wraps spawn() so DUP2 file-actions naming local /usr fds promote",
  "  // to kernel twins (docs/archive/0180); identity when the RO volume is off.",
  "  spawnHooks: rfs ? rfs.wrapSpawnHooks(client.spawnHooks()) : client.spawnHooks(),",
  "  pid: wd.pid,",
  "  ppid: wd.ppid,",
  "  // Live ppid off the vDSO page (docs/archive/0179): tracks reparent-to-init.",
  "  getppid: function () { return client.getppid(); },",
  "}).then(function (code) {",
  "  wt.parentPort.postMessage({ type: 'exited', code: code });",
  "}, function (e) {",
  "  if (!instantiated) {",
  "    var diagnostic = new TextEncoder().encode('gucOS: could not start ' + wd.path + ': ' + String(e) + '\\n');",
  "    if (rfs) rfs.write(2, diagnostic, diagnostic.length);",
  "    else ship(2)(diagnostic);",
  "  }",
  "  wt.parentPort.postMessage({ type: instantiated ? 'crashed' : 'start-failed', error: String((e && e.stack) || e) });",
  "});",
].join('\n');

function nodeCreateWorker(config) {
  if (typeof process === 'undefined') {
    throw new Error('nodeCreateWorker: Node only (the browser factory ships with the os/ page)');
  }
  var hostPath = config.hostPath, kernelPath = config.kernelPath;
  var Worker = require('worker_threads').Worker;
  return function createWorker(procSpec) {
    // The image crosses as a plain ArrayBuffer clone (images are re-spawnable;
    // never transfer). The kernel page crosses as the SAB it is. On a module-
    // cache hit (docs/archive/0037) the image is null and the compiled Module
    // structured-clones instead — sharing the engine's compiled code.
    var image = procSpec.image;
    var imageBuf = image == null ? null
      : image instanceof Uint8Array
        ? image.buffer.slice(image.byteOffset, image.byteOffset + image.byteLength)
        : image;
    var w = new Worker(BOOT_SOURCE, {
      eval: true,
      workerData: {
        hostPath: hostPath,
        kernelPath: kernelPath,
        pid: procSpec.pid, ppid: procSpec.ppid, pgid: procSpec.pgid,
        path: procSpec.path, argv: procSpec.argv, envp: procSpec.envp,
        cwd: procSpec.cwd, actions: procSpec.actions, flags: procSpec.flags,
        image: imageBuf,
        module: procSpec.module || null,
        kernelPage: procSpec.kernelPage,
        ttySab: procSpec.ttySab || null,
        brokered: !!procSpec.brokered,
        // Read-only volume (docs/archive/0180): { prefix, sab } — the SAB shares.
        ro: procSpec.ro || null,
        // SPSC pipe rings (docs/archive/0181): [{fd, end, sab}] — the SABs share.
        pipeRings: procSpec.pipeRings || null,
        fileReadCache: procSpec.fileReadCache || null,
      },
      // Program stdout/stderr flow through {type:'out'} messages (writeOut/
      // writeErr are overridden in BOOT_SOURCE); the worker's own process
      // streams stay inherited so stray diagnostics remain visible.
    });
    return {
      postMessage: function (m) { w.postMessage(m); },
      onMessage: function (fn) { w.on('message', fn); },
      onExit: function (fn) { w.on('exit', fn); },
      terminate: function () { w.terminate(); },
    };
  };
}

/* ============================================================
 * ProcFS (docs/archive/0043) — a synthetic /proc volume.
 *
 * Implements exactly the fs-op surface MountFS routes to (open/read/stat/
 * readdir/…), generating Linux-format content from the LIVE kernel process
 * table so busybox ps/top/pgrep/pkill/uptime/free parse it unmodified. No
 * BlockFS backing, no on-disk format, nothing for fsck to check. Mount it
 * as `'/proc': new ProcFS()` in the MountFS table handed to Kernel({fs}) —
 * the Kernel constructor scans the mount table and binds itself (until a
 * kernel is bound the volume is an empty read-only tree).
 *
 * Semantics:
 * - Snapshot at open, like Linux: open() renders the whole file into a
 *   buffer; reads/lseek work that buffer. read_to_buf-style single-read
 *   consumers and fstat-then-read consumers both work (sizes are real,
 *   unlike Linux's size-0 procfs — host-side readFileBytes sizes via
 *   fstat).
 * - Read-only: every mutator answers EROFS (the 0040 convention); opens
 *   for writing answer EACCES like Linux procfs.
 * - What exists: /proc/<pid>/{stat,status,cmdline,comm} for every pcb in
 *   the table (zombies included, like Linux), plus uptime, loadavg,
 *   meminfo, stat, version. No /proc/self: the fs layer has no caller
 *   identity (fs ops carry a path, not a pid), and nothing shipped needs
 *   it (CONFIG_BUSYBOX_EXEC_PATH is hand-patched to /bin/sh).
 * - What's synthetic: per-process CPU time is not tracked (workers run on
 *   their own OS threads) — utime/stime are 0 and top's %CPU column is
 *   boring by design. VmSize/VmRSS are nominal constants (the kernel
 *   can't see worker heaps); meminfo is a fixed plausible table. Real:
 *   pids, ppid/pgid/sid, state (R running / S parked-in-RPC / T stopped /
 *   Z zombie), comm/cmdline from spawn argv, start_time and uptime from
 *   the kernel clock, loadavg's running/total counts and last-pid.
 * ============================================================ */

var PROC_HZ = 100;                 // jiffies/sec — matches bb_clk_tck()'s 100
var PROC_VMSIZE_KB = 4096;         // nominal VmSize (worker heaps invisible)
var PROC_VMRSS_KB = 1024;          // nominal VmRSS; 16 pages of 64KiB
var PROC_ROOT_FILES = ['loadavg', 'meminfo', 'stat', 'uptime', 'version'];
var PROC_PID_FILES = ['cmdline', 'comm', 'stat', 'status'];

function ProcFS() {
  this._kernel = null;             // bound by Kernel's mount-table scan
  this._lastError = '';
  this._fdTable = [];              // fd -> { buf, pos, ino } (dup shares)
  this._dirTable = [];             // handle -> { entries, pos, dotState }
  this._inos = new Map();          // path -> synthetic ino, stable per mount
  this._nextIno = 1;
}

ProcFS.prototype._setErr = function (name) {
  this._lastError = name;
  return null;
};

ProcFS.prototype._ino = function (path) {
  var ino = this._inos.get(path);
  if (!ino) { ino = this._nextIno++; this._inos.set(path, ino); }
  return ino;
};

/* Resolve a volume-relative path (MountFS hands them normalized) to
 * { dir: 'root' } | { dir: 'pid', pcb } | { file, pcb? } | null. */
ProcFS.prototype._lookup = function (path) {
  var parts = String(path).split('/').filter(function (p) { return p && p !== '.'; });
  if (parts.length === 0) return { dir: 'root' };
  var k = this._kernel;
  if (parts.length === 1) {
    if (PROC_ROOT_FILES.indexOf(parts[0]) >= 0) return { file: parts[0] };
    var pcb = /^\d+$/.test(parts[0]) && k ? k._procs.get(parts[0] | 0) : null;
    return pcb ? { dir: 'pid', pcb: pcb } : null;
  }
  if (parts.length === 2 && PROC_PID_FILES.indexOf(parts[1]) >= 0) {
    var pcb2 = /^\d+$/.test(parts[0]) && k ? k._procs.get(parts[0] | 0) : null;
    return pcb2 ? { file: parts[1], pcb: pcb2 } : null;
  }
  return null;
};

/* ---- content generators (Linux formats — see proc(5)) ---- */

ProcFS.prototype._comm = function (pcb) {
  var a0 = (pcb.argv && pcb.argv[0]) || pcb.path || '?';
  // A leading '-' is the login-shell argv[0] convention (docs/archive/0174 spawns
  // pid 1 / term shells as "-sh"), not part of the name: Linux comm comes
  // from the exec'd FILE, so a login sh still reads "sh" (and pgrep/pkill
  // by name keep matching).
  if (a0.charCodeAt(0) === 45 && a0.length > 1) a0 = a0.slice(1);
  var base = a0.slice(a0.lastIndexOf('/') + 1);
  return base.slice(0, 15) || '?';
};

ProcFS.prototype._stateChar = function (pcb) {
  if (pcb.state === STATE_ZOMBIE) return 'Z';
  if (pcb.state === STATE_STOPPED) return 'T';
  return pcb.waiter ? 'S' : 'R';
};

ProcFS.prototype._uptimeSec = function () {
  var k = this._kernel;
  return k ? Math.max(0, (Date.now() - k._bootMs) / 1000) : 0;
};

ProcFS.prototype._counts = function () {
  var running = 0, total = 0;
  this._kernel._procs.forEach(function (p) {
    if (p.state === STATE_ZOMBIE) return;
    total++;
    if (p.state === STATE_RUNNING && !p.waiter) running++;
  });
  return { running: running || 1, total: total };
};

ProcFS.prototype._render = function (hit) {
  var k = this._kernel, pcb = hit.pcb;
  if (pcb) return this._renderPid(hit.file, pcb);
  switch (hit.file) {
    case 'uptime': {
      var up = this._uptimeSec().toFixed(2);
      return up + ' ' + up + '\n';
    }
    case 'loadavg': {
      // Cooperative single-runqueue system: the load numbers stay 0.00;
      // running/total and last-pid are real.
      if (!k) return '0.00 0.00 0.00 0/0 0\n';
      var c = this._counts();
      return '0.00 0.00 0.00 ' + c.running + '/' + c.total + ' ' + (k._nextPid - 1) + '\n';
    }
    case 'meminfo':
      // Fixed plausible table (kB): the kernel can't see worker heaps.
      // SReclaimable/MemAvailable/Shmem are what busybox free reads;
      // MemTotal MUST stay nonzero (top divides by it).
      return 'MemTotal:        1048576 kB\n' +
        'MemFree:          786432 kB\n' +
        'MemAvailable:     786432 kB\n' +
        'Buffers:               0 kB\n' +
        'Cached:                0 kB\n' +
        'SwapCached:            0 kB\n' +
        'Shmem:                 0 kB\n' +
        'SwapTotal:             0 kB\n' +
        'SwapFree:              0 kB\n' +
        'Dirty:                 0 kB\n' +
        'Writeback:             0 kB\n' +
        'AnonPages:             0 kB\n' +
        'Mapped:                0 kB\n' +
        'Slab:                  0 kB\n' +
        'SReclaimable:          0 kB\n';
    case 'stat': {
      // One aggregate cpu line (idle accrues with the clock so interval
      // deltas divide cleanly), then the bookkeeping lines top ignores.
      var idle = Math.floor(this._uptimeSec() * PROC_HZ);
      var c2 = k ? this._counts() : { running: 0, total: 0 };
      if (!k) k = { _bootMs: Date.now(), _nextPid: 1 };   // unbound: zeros
      return 'cpu  0 0 0 ' + idle + ' 0 0 0 0\n' +
        'cpu0 0 0 0 ' + idle + ' 0 0 0 0\n' +
        'intr 0\n' +
        'ctxt 0\n' +
        'btime ' + Math.floor(k._bootMs / 1000) + '\n' +
        'processes ' + (k._nextPid - 1) + '\n' +
        'procs_running ' + c2.running + '\n' +
        'procs_blocked 0\n';
    }
    case 'version':
      return 'Linux version 6.6.0-wasm (root@localhost) (cc gucos) #1 ' +
        'almost-POSIX on WebAssembly\n';
  }
  return null;
};

/* /proc/<pid>/<file>. */
ProcFS.prototype._renderPid = function (file, pcb) {
  switch (file) {
    case 'cmdline': {
      // argv NUL-joined with a trailing NUL; empty for zombies, so
      // busybox's read_cmdline falls back to "[comm]" — like Linux.
      if (pcb.state === STATE_ZOMBIE) return '';
      var argv = (pcb.argv && pcb.argv.length) ? pcb.argv : [pcb.path || '?'];
      return argv.join('\0') + '\0';
    }
    case 'comm':
      return this._comm(pcb) + '\n';
    case 'status': {
      var sc = this._stateChar(pcb);
      var word = { R: 'running', S: 'sleeping', T: 'stopped (signal)', Z: 'zombie' }[sc];
      return 'Name:\t' + this._comm(pcb) + '\n' +
        'State:\t' + sc + ' (' + word + ')\n' +
        'Tgid:\t' + pcb.pid + '\n' +
        'Pid:\t' + pcb.pid + '\n' +
        'PPid:\t' + pcb.ppid + '\n' +
        'TracerPid:\t0\n' +
        'Uid:\t0\t0\t0\t0\n' +
        'Gid:\t0\t0\t0\t0\n' +
        'FDSize:\t' + (pcb.fds ? pcb.fds.size : 0) + '\n' +
        'Groups:\t0\n' +
        'NSpgid:\t' + pcb.pgid + '\n' +
        'NSsid:\t' + pcb.sid + '\n' +
        'VmSize:\t' + PROC_VMSIZE_KB + ' kB\n' +
        'VmRSS:\t' + PROC_VMRSS_KB + ' kB\n' +
        'Threads:\t1\n';
    }
    case 'stat': {
      // proc(5) field order; parsers key on the ')' before the state char,
      // so a comm with spaces/parens stays parseable the same way Linux's
      // does. Fields 14/15 (utime/stime) are 0 by design; field 22
      // (starttime, jiffies since boot) and 7 (tty_nr) are real.
      var ttyNr = pcb.tty ? ((4 << 8) | 1) : 0;             // tty1, or none
      var tpgid = pcb.tty && pcb.tty.fgPgid > 0 ? pcb.tty.fgPgid : -1;
      var startJiffies = Math.max(0,
        Math.floor(((pcb.startMs || this._kernel._bootMs) - this._kernel._bootMs) / (1000 / PROC_HZ)));
      var vsizeBytes = PROC_VMSIZE_KB * 1024;
      var rssPages = 16;                                    // ×64KiB pages = VmRSS
      return pcb.pid + ' (' + this._comm(pcb) + ') ' + this._stateChar(pcb) +
        ' ' + pcb.ppid + ' ' + pcb.pgid + ' ' + pcb.sid +
        ' ' + ttyNr + ' ' + tpgid +
        ' 0 0 0 0 0' +                                      // flags, faults
        ' 0 0 0 0' +                                        // utime stime cutime cstime
        ' 20 0 1 0' +                                       // priority nice threads itreal
        ' ' + startJiffies +
        ' ' + vsizeBytes + ' ' + rssPages +
        ' 4194304 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n'; // rsslim + cruft
      }
  }
  return null;
};

/* Render a file to bytes, or null (ENOENT). */
ProcFS.prototype._genBytes = function (hit) {
  var text = this._render(hit);
  if (text === null || text === undefined) return null;
  if (typeof TextEncoder !== 'undefined') return new TextEncoder().encode(text);
  var out = new Uint8Array(text.length);   // ASCII-only fallback: /proc content
  for (var i = 0; i < text.length; i++) out[i] = text.charCodeAt(i) & 0xff;  // is pure ASCII; TextEncoder-less envs only
  return out;
};

ProcFS.prototype._statObj = function (path, hit) {
  var now = Math.floor(Date.now() / 1000);
  if (hit.dir) {
    return { dev: 0, ino: this._ino(path), mode: 0x4000 | 0o555, nlink: 2, size: 0,
      atime: now, mtime: now, ctime: now, rdev: 0 };
  }
  var bytes = this._genBytes(hit);
  return { dev: 0, ino: this._ino(path), mode: 0x8000 | 0o444, nlink: 1,
    size: bytes ? bytes.length : 0, atime: now, mtime: now, ctime: now, rdev: 0 };
};

/* ---- path operations ---- */

ProcFS.prototype.open = function (path, flags, mode) {
  var hit = this._lookup(path);
  if (!hit) return this._setErr((flags & 0x40) ? 'EROFS' : 'ENOENT'); // O_CREAT
  if (hit.dir) return this._setErr('EISDIR');
  if ((flags & 3) !== 0 || (flags & 0x200)) return this._setErr('EACCES'); // write / O_TRUNC
  var buf = this._genBytes(hit);
  if (!buf) return this._setErr('ENOENT');
  var entry = { buf: buf, pos: 0, ino: this._ino(path) };
  for (var i = 0; i < this._fdTable.length; i++) {
    if (this._fdTable[i] === null) { this._fdTable[i] = entry; return i; }
  }
  this._fdTable.push(entry);
  return this._fdTable.length - 1;
};

ProcFS.prototype.stat = function (path) {
  var hit = this._lookup(path);
  return hit ? this._statObj(path, hit) : this._setErr('ENOENT');
};
ProcFS.prototype.lstat = ProcFS.prototype.stat;   // no symlinks in this tree

ProcFS.prototype.access = function (path, mode) {
  var hit = this._lookup(path);
  if (!hit) return this._setErr('ENOENT');
  if (mode & 2) return this._setErr('EROFS');     // W_OK
  return 0;
};

ProcFS.prototype.readlink = function (path) {
  return this._setErr(this._lookup(path) ? 'EINVAL' : 'ENOENT');
};

ProcFS.prototype.opendir = function (path) {
  var hit = this._lookup(path);
  if (!hit) return this._setErr('ENOENT');
  if (!hit.dir) return this._setErr('ENOTDIR');
  var self = this;
  var entries = [];
  if (hit.dir === 'root') {
    PROC_ROOT_FILES.forEach(function (n) {
      entries.push({ ino: self._ino('/' + n), type: 8, name: n });
    });
    if (this._kernel) {
      var pids = [];
      this._kernel._procs.forEach(function (p, pid) { pids.push(pid); });
      pids.sort(function (a, b) { return a - b; });
      pids.forEach(function (pid) {
        entries.push({ ino: self._ino('/' + pid), type: 4, name: String(pid) });
      });
    }
  } else {
    PROC_PID_FILES.forEach(function (n) {
      entries.push({ ino: self._ino('/' + hit.pcb.pid + '/' + n), type: 8, name: n });
    });
  }
  var d = { entries: entries, pos: 0, dotState: 0 };
  for (var i = 0; i < this._dirTable.length; i++) {
    if (this._dirTable[i] === null) { this._dirTable[i] = d; return i; }
  }
  this._dirTable.push(d);
  return this._dirTable.length - 1;
};

ProcFS.prototype.readdir = function (handle) {
  var d = (handle >= 0 && handle < this._dirTable.length) ? this._dirTable[handle] : null;
  if (!d) return this._setErr('EBADF');
  if (d.dotState < 2) {                            // '.' / '..' like BlockFS
    var dotName = d.dotState === 0 ? '.' : '..';
    d.dotState++;
    return { ino: 0, type: 4, name: dotName };
  }
  if (d.pos >= d.entries.length) return null;
  return d.entries[d.pos++];
};

ProcFS.prototype.closedir = function (handle) {
  var d = (handle >= 0 && handle < this._dirTable.length) ? this._dirTable[handle] : null;
  if (!d) return this._setErr('EBADF');
  this._dirTable[handle] = null;
  return 0;
};

/* ---- mutators: a read-only synthetic tree (0040 EROFS convention) ---- */

ProcFS.prototype._erofs = function () { return this._setErr('EROFS'); };
ProcFS.prototype.chmod = ProcFS.prototype._erofs;
ProcFS.prototype.utime = ProcFS.prototype._erofs;
ProcFS.prototype.mkdir = ProcFS.prototype._erofs;
ProcFS.prototype.mknod = ProcFS.prototype._erofs;
ProcFS.prototype.unlink = ProcFS.prototype._erofs;
ProcFS.prototype.remove = ProcFS.prototype._erofs;
ProcFS.prototype.rmdir = ProcFS.prototype._erofs;
ProcFS.prototype.rename = ProcFS.prototype._erofs;
ProcFS.prototype.link = ProcFS.prototype._erofs;
ProcFS.prototype.symlink = ProcFS.prototype._erofs;
ProcFS.prototype.ftruncate = ProcFS.prototype._erofs;
ProcFS.prototype.fchmod = ProcFS.prototype._erofs;
ProcFS.prototype.futime = ProcFS.prototype._erofs;

/* ---- fd operations ---- */

ProcFS.prototype._fdEntry = function (fd) {
  return (fd >= 0 && fd < this._fdTable.length) ? this._fdTable[fd] : null;
};

ProcFS.prototype.read = function (fd, buf, count) {
  var e = this._fdEntry(fd);
  if (!e) return this._setErr('EBADF');
  var n = Math.min(count | 0, buf.length, e.buf.length - e.pos);
  if (n <= 0) return 0;
  buf.set(e.buf.subarray(e.pos, e.pos + n));
  e.pos += n;
  return n;
};

ProcFS.prototype.write = function (fd, buf, count) {
  // Opens are read-only (open() refuses write modes), so any write is EBADF.
  return this._setErr('EBADF');
};

ProcFS.prototype.lseek = function (fd, offset, whence) {
  var e = this._fdEntry(fd);
  if (!e) return this._setErr('EBADF');
  var base = whence === 1 ? e.pos : whence === 2 ? e.buf.length : 0;
  var pos = base + (offset | 0);
  if (pos < 0) return this._setErr('EINVAL');
  e.pos = pos;
  return pos;
};

ProcFS.prototype.fstat = function (fd) {
  var e = this._fdEntry(fd);
  if (!e) return this._setErr('EBADF');
  var now = Math.floor(Date.now() / 1000);
  return { dev: 0, ino: e.ino, mode: 0x8000 | 0o444, nlink: 1, size: e.buf.length,
    atime: now, mtime: now, ctime: now, rdev: 0 };
};

ProcFS.prototype.fsync = function (fd) {
  return this._fdEntry(fd) ? 0 : this._setErr('EBADF');
};

ProcFS.prototype.close = function (fd) {
  if (!this._fdEntry(fd)) return this._setErr('EBADF');
  this._fdTable[fd] = null;
  return 0;
};

ProcFS.prototype.dup = function (fd) {
  var e = this._fdEntry(fd);
  if (!e) return this._setErr('EBADF');
  for (var i = 0; i < this._fdTable.length; i++) {
    if (this._fdTable[i] === null) { this._fdTable[i] = e; return i; }
  }
  this._fdTable.push(e);                           // shared entry = shared offset
  return this._fdTable.length - 1;
};

/* ---- environment exports (host.js discipline) ---- */
var KERNEL_EXPORTS = {
  Kernel: Kernel,
  ProcFS: ProcFS,
  KernelClient: KernelClient,
  Tty: Tty,
  RemoteFS: RemoteFS,
  nodeCreateWorker: nodeCreateWorker,
  OP: OP,
  SIG: SIG,
  KP_SIZE: KP_SIZE,
  KP_DOORBELL: KP_DOORBELL,
  KP_SIGPEND: KP_SIGPEND,
  KP_SIGBLOCK: KP_SIGBLOCK,
  KP_FLAGS: KP_FLAGS,
  KP_RPC_STATE: KP_RPC_STATE,
  KP_RPC_OP: KP_RPC_OP,
  KP_RPC_LEN: KP_RPC_LEN,
  KP_PAYLOAD_OFF: KP_PAYLOAD_OFF,
  RPC_IDLE: RPC_IDLE,
  RPC_REQUEST: RPC_REQUEST,
  RPC_DONE: RPC_DONE,
  KF_STOP: KF_STOP,
  KP_RPC_KIND: KP_RPC_KIND,
  KP_VSYNC_EN: KP_VSYNC_EN,
  KP_VSYNC_SEQ: KP_VSYNC_SEQ,
  KP_VSYNC_ARMED: KP_VSYNC_ARMED,
  KP_COMP_PARKED: KP_COMP_PARKED,
  KP_PAYLOAD_CAP: KP_PAYLOAD_CAP,
  KP_FS_CHUNK: KP_FS_CHUNK,       // bulk-lane chunks, derived (docs/archive/0235)
  KP_HOOK_CHUNK: KP_HOOK_CHUNK,
  // vDSO block (docs/archive/0179) — seqlock-published kernel state.
  KP_VD_SEQ: KP_VD_SEQ, KP_VD_PID: KP_VD_PID, KP_VD_PPID: KP_VD_PPID,
  KP_VD_PGID: KP_VD_PGID, KP_VD_SID: KP_VD_SID,
  KP_VD_BOOT_LO: KP_VD_BOOT_LO, KP_VD_BOOT_HI: KP_VD_BOOT_HI,
  KP_VD_SCREEN_W: KP_VD_SCREEN_W, KP_VD_SCREEN_H: KP_VD_SCREEN_H,
  RO_FD_BASE: RO_FD_BASE,   // process-served read-only fd space (docs/archive/0180)
  // SPSC pipe rings (docs/archive/0181) — header layout + mode ladder for tests.
  PIPE_RING_HDR: PIPE_RING_HDR, PIPE_RING_BYTES: PIPE_RING_BYTES,
  PR_MODE: PR_MODE, PR_HEAD: PR_HEAD, PR_TAIL: PR_TAIL, PR_FLAGS: PR_FLAGS,
  PR_RWAIT: PR_RWAIT, PR_WWAIT: PR_WWAIT,
  PR_LATENT: PR_LATENT, PR_FAST: PR_FAST, PR_DEMOTED: PR_DEMOTED,
  PRF_RGONE: PRF_RGONE, PRF_WGONE: PRF_WGONE,
  pipeRingViews: pipeRingViews, pipeRingAvail: pipeRingAvail,
  pipeRingPut: pipeRingPut, pipeRingTake: pipeRingTake,
  RPCK_JSON: RPCK_JSON,
  RPCK_RAW: RPCK_RAW,
  writePayload: writePayload,
  writeRawPayload: writeRawPayload,
  readPayload: readPayload,
  W_EXITCODE: W_EXITCODE,
  W_TERMSIG: W_TERMSIG,
  W_STOPCODE: W_STOPCODE,
  // Audio mixer (docs/archive/0017) — ring layout MUST MATCH host.js
  // createSharedAudioBuffer; format words MUST MATCH <SDL3/SDL_audio.h>.
  AU_WPOS: AU_WPOS, AU_QUEUED: AU_QUEUED, AU_PLAYING: AU_PLAYING,
  AU_HDR_BYTES: AU_HDR_BYTES,
  AU_OUT_FREQ: AU_OUT_FREQ, AU_OUT_CHANNELS: AU_OUT_CHANNELS,
  AU_FMT_F32: AU_FMT_F32, AU_FMT_S32: AU_FMT_S32, AU_FMT_S16: AU_FMT_S16,
  AU_FMT_S8: AU_FMT_S8, AU_FMT_U8: AU_FMT_U8,
  AU_TARGET_MS: AU_TARGET_MS,
};

if (typeof module !== 'undefined' && module.exports) {
  module.exports = KERNEL_EXPORTS;
} else if (typeof window !== 'undefined') {
  window.KERNEL = KERNEL_EXPORTS;
} else if (typeof self !== 'undefined') {
  self.KERNEL = KERNEL_EXPORTS;
}
