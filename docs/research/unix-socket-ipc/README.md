# Unix socket IPC candidate

All edits and execution for this candidate occurred through SSH in
/home/ubuntu/git/gucos-ipc on agentone. Model inference did not run there.
The starting checkout was clean at cdf19e035e08f77d2e4246d879a53fbe04a6a101,
whose commit message identifies it as the staged snapshot of
fc5f9baac0296eca486721fb6a5665e40c431b90. The original commit object was not
present in that snapshot repository.

## Implemented contract

SCM_RIGHTS transfers references to existing brokered open file descriptions.
Offsets and status flags remain shared; received descriptor flags start clear
unless MSG_CMSG_CLOEXEC is requested. Passing a descriptor neither closes the
sender's descriptor nor depends on it remaining open.

Queued rights hold OFD references under kernel pseudo-holder PID 0. For pipe
ends this also prevents unsafe SPSC promotion. Blocked sends retain their
rights, and blocked socket operations retain the socket OFD. Interruptions,
process exit, and waiter cancellation release those holds. Receive installs
new descriptor references before releasing queue references.

A stream control record is attached to the first accepted byte of its send.
A partial send transfers rights exactly once. Receive stops at the end of the
first encountered control-bearing write, keeping separate ancillary groups
separate. Plain reads and receives without control space dispose consumed
rights. MSG_PEEK creates independent received descriptors while retaining the
queued references (Linux behavior). Control truncation delivers the fitting
prefix, sets MSG_CTRUNC, and releases the remainder on consumption.

Cycle collection traces from non-queue OFD holders through socket receive
queues and listeners' pending receive queues. Peer links do not retain the
peer OFD. Unreachable queues release their ancillary references. Process fd
maps are not used as roots because close/exec/exit can still contain an entry
whose OFD reference has just been released.

AF_UNIX SOCK_DGRAM supports socketpair, pathname bind, connect, sendto and
recvfrom with sender pathname. SOCK_SEQPACKET supports socketpair and the
existing pathname listen/connect/accept path. Packet writes are atomic,
truncating reads discard the rest of that packet, MSG_TRUNC reports the full
packet length, and empty packets retain readiness and message boundaries.
Bytes and metadata have separate bounds: 64 KiB queue capacity, 60000-byte
packet payload, 256 queued records, and 253 rights per send.

Existing imports/opcodes and C structure layouts remain intact. New imports
are __sock_pair_type, __sock_send_control and __sock_recv_control. New RPC
opcodes are SOCK_SENDMSG (0x050b) and SOCK_RECVMSG (0x050c).
The platform version and ABI version were not changed.

## Explicit limits and portability choices

- Process-local sealed-image (/usr) descriptors have no kernel OFD to share.
  SCM_RIGHTS rejects them with EOPNOTSUPP. Reopening a separate description
  would violate shared-offset semantics.
- Only SCM_RIGHTS is accepted as ancillary data. Credential passing,
  MSG_OOB, abstract Unix addresses and writable socket options remain
  unsupported. Existing SO_PEERCRED is a virtual-session identity.
- Datagram AF_UNSPEC disconnect and Linux automatic abstract binding are
  unsupported. Pathname bind follows the existing fresh-socket restriction.
- SHUT_RD preserves the existing ability to drain queued stream data and
  rights. Closing the receiving socket disposes unread rights. This matches
  the observed Linux probe behavior; FreeBSD's shutdown-disposal test expects
  earlier release.
- MSG_PEEK follows Linux descriptor duplication, not the inspected FreeBSD
  test's no-descriptor-on-peek expectation.
- The existing descriptor allocator has no RLIMIT_NOFILE enforcement; this
  change does not add per-process descriptor limits.
- This is a core candidate. No tmux application acceptance, services-worker
  integration, publication or deployment is claimed here.

## Retained tests versus one-off evidence

Only tests/ipc-socket-kernel.mjs, tests/ipc-sockets.mjs and its handcrafted C
fixture are ongoing regressions. The kernel test uses the actual RPC/OFD code
with simulated process channels so it can assert internal reference release.

This directory is evidence, not a new upstream test suite. The downloaded
Linux and FreeBSD source files retain their upstream licenses. sources.json
records retrieval time, URLs and exact SHA-256 digests. adapted-probes.c is a
small local harness using selected behavioral cases:

- Linux scm_rights.c: self_ref, triangle, cross_edge, backtrack_from_scc and
  mixed_lowpoint ownership graphs; stream and datagram, plus a local
  seqpacket extension.
- FreeBSD unix_passfd_test.c: send_and_close, truncated_rights and
  send_and_shutdown, with the shutdown oracle explicitly moved to final
  close to preserve the Linux/gucOS queued-read behavior.

The guest graph adaptation adds a pipe-writer EOF oracle to observe resource
reclamation. Native Linux may defer graph collection, so the native build
defines NATIVE_LINUX and does not assert immediate graph reclamation.
Passing that native build proves syscall acceptance and the non-graph cases,
not Linux kernel object counts or immediate garbage collection.

Execution results and exact commands are recorded in the development journal
under docs/logs/2026-10-11/.

