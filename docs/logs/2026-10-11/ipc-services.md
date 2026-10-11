# IPC services implementation record

Work is confined to SSH host agentone, /home/ubuntu/git/gucos-ipc-services.
Starting commit: cdf19e035e08f77d2e4246d879a53fbe04a6a101, whose message is
“Baseline snapshot fc5f9baac0296eca486721fb6a5665e40c431b90”.
The executor/model is not claimed to run remotely. No internal subagents or
external implementation threads were created. No local checkout edits,
deployment, publishing, package-repository access, or version change.

## Implementation

- src/ipc-services.js owns flock queues, eventfd counters and named semaphore
  objects. Kernel hooks handle dispatch, read/write/readiness, final OFD release,
  cancellation, fork, exec and exit. Opcodes 0x0440–0x0442 are separate from
  socket opcodes. The build includes the module in owner and process payloads.
- BSD-style flock is whole-file advisory locking keyed by inode and owned by the
  open file description, independent of POSIX fcntl record locks. Separate opens
  compete, including within one process. dup and fork share locks; last release
  unlocks. Shared/exclusive conversion relinquishes the old lock before waiting.
  Nonblocking conflicts return EAGAIN/EWOULDBLOCK, invalid flags EINVAL.
- eventfd counters use BigInt, preserving every bit through UINT64_MAX-1.
  Reads return eight bytes, reset the counter or decrement by one in semaphore
  mode. Writes require eight bytes, reject UINT64_MAX, and wait for counter
  capacity. NONBLOCK, CLOEXEC, shared status flags, poll/select readiness and
  interruption use the descriptor infrastructure. fsync returns EINVAL.
- Named semaphores use broker object identities encoded as negative sem_t words.
  sem_t remains one int (four bytes). Nonnegative unnamed values retain their
  original atomic fast paths; process-shared unnamed sem_init remains ENOSYS.
  sem_open caches pointers and reference counts under a libc lock, coordinated
  with fork's existing lock protocol. Unlink detaches the name; outstanding
  handles keep their object, and recreate gets a new identity. Fork inherits
  handles; exec/exit release them. A post wakes one queued waiter.
- sem_timedwait uses an absolute CLOCK_REALTIME deadline for named and unnamed
  semaphores. An available token succeeds even with an expired deadline.
  Timers/cancellation remove pending requests without consuming future tokens.
  Named sem_getvalue returns the nonnegative current count.
- Blocking descriptor operations retain their OFD until completion/cancellation,
  including when another thread closes the descriptor. Signal restart handling
  follows the runtime's SA_RESTART result from signal delivery.

## Supported boundary and limitations

The shipped browser owner/process runtime is the integration target. Standalone
host execution without a broker reports ENOSYS through fallback imports. No new
process-shared mmap, graphics, socket implementation or package recipes.

Named objects persist for the lifetime of the running kernel, not across a
browser/kernel reboot, and are not represented as /dev/shm files. Names require
a leading slash, one component and at most 251 bytes after the slash. Creation
stores the umask-filtered mode. gucOS currently supports only uid/gid 0; there is
no claim of multi-user permission isolation. Unnamed wait's pre-existing signal
behavior is preserved. pthread cancellation remains unsupported by the baseline.

flock is supported on filesystem-backed descriptors; other kinds are rejected.
No network filesystem/NFS/SMB lock emulation. Conversion is not atomic and no
deadlock detection is promised for flock. Eventfd has no kernel producer that
can cause the Linux kernel-only UINT64_MAX overflow notification, and no proc
fdinfo extension. Eventfd lseek follows this runtime's non-file ESPIPE policy,
rather than Linux's noop_llseek behavior. No epoll is added here.

No claim that tmux itself has been built or run in this checkout. That package
and the separate socket implementation require coordinator integration and
flagship acceptance after merging.

## Validation and environment

All dependency bootstrap, builds and tests use
/home/ubuntu/.local/bin/machine-job from this checkout. Browser-containing
commands reserve --kind browser; non-browser builds/probes reserve --kind build.
Playwright launches one browser at a time; these tests do not use a parallel
test runner.

Dependency bootstrap: corepack pnpm install --frozen-lockfile succeeded.
pnpm reported ignored optional esbuild/workerd build scripts; the core build
does not require them. The first core build succeeded. Initial browser launch
failed before guest execution because Chromium lacked libatk and other shared
libraries; a second launch exposed a transitive libavahi dependency.
Ubuntu packages were downloaded and extracted under .ipc-browser-deps/
(checkout-local Git exclude), with LD_LIBRARY_PATH set per invocation. No apt
system installation was performed by this thread.

Handwritten tests:
- tests/ipc-services-kernel.mjs: actual Kernel and IPCServices methods, with
  response capture and minimal process/FS doubles; verifies in-flight OFDs,
  cancellation, unlink/recreate and final object/reference reclamation.
- tests/fixtures/ipc-services.c driven by tests/ipc-services.mjs: compiled in
  the browser guest, exercising flock ownership/conversion/blocking/signals,
  eventfd counter bounds/readiness/blocking/signals, named semaphore identity,
  unlink, threads, fork, exec, timeouts, and unchanged unnamed semaphore ABI.

The kernel probe passed twice, including after integrating OFD retention.
The compiled-C probe initially failed because its invocation omitted the runtime's
required --enable-fork flag; the runner was corrected. The corrected probe
compiled the fixture and validated its 255,895-byte WebAssembly module.
This probe did not execute the guest.

Final browser results and exact reproduction commands are recorded below after
execution.

## Semantic references consulted

- Linux flock(2): https://man7.org/linux/man-pages/man2/flock.2.html
- Linux eventfd(2): https://man7.org/linux/man-pages/man2/eventfd.2.html
- Linux eventfd implementation (read-only inspection; not copied):
  https://raw.githubusercontent.com/torvalds/linux/master/fs/eventfd.c
- POSIX sem_open:
  https://pubs.opengroup.org/onlinepubs/7990949875/functions/sem_open.html

These references guided compatibility; test results, not references, establish
what this implementation actually executed.

## Final build evidence

Remote scheduler job e6367db2-efd5-46c1-89c1-b821c365c687 exited 0.
Its command, from the checkout, was:

```sh
/home/ubuntu/.local/bin/machine-job run --kind build --timeout 1800 --   sh -c "node .ipc-browser-deps/compile-fixture.mjs && node tools/build.mjs"
```

The disposable compile probe used the bundled compiler with
compilerOptions.enableFork=true, then constructed a WebAssembly.Module from the
result. The committed browser runner independently compiles the same C fixture
with cc --enable-fork and executes it.

Runtime: 796f2f551a68cc120b4f3b6b02c4409a4e384088ac786d8ef82af4655cac1999
Libc digest: df1ef4a58c60b835fb5ebe4dec0a2fd69b155566612d9fad4caeac2a6d970fa6

Kernel probe output (jobs 80512fd6-c901-4fdb-a568-b7eefaa14a3f and
ee0842c1-fc74-416e-bb96-8f4cbdc6fd6a):

```text
IPC-KERNEL-PASS: in-flight OFDs, cancellation, unlink and handle reclamation
```

Reproduce this probe with:

```sh
/home/ubuntu/.local/bin/machine-job run --kind build --timeout 1800 --   node tests/ipc-services-kernel.mjs
```
