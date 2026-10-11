# closefrom for tmux: author implementation and evidence

Author work was executed through SSH agentone in /home/ubuntu/git/gucos-closefrom,
branch ipc-closefrom, based on integration 4c5e33f812295ce12fdb67a50e6f4ec230bf0d5f.
The model/tool orchestrator is not claimed to execute remotely.
No integration or tmux candidate edits, deployment, publication, or version changes.
All 63 integration dist hashes still match the preceding frozen published-binary
acceptance evidence. This work has not undergone the parent's independent review.

## Implementation

BSD void closefrom(int lowfd) is a canonical external function in __posix.c,
declared by unistd.h. Negative thresholds become zero. __closefrom host import
enumerates occupied descriptors rather than scanning a guessed maximum.
RPC FS_CLOSEFROM is 0x0443 (distinct from existing 0x0440-42, 0x050b/c and 0x0108).
The kernel helper is also used by spawn CLOSEFROM and removes fd flags,
drops descriptor roots before OFD unref, and uses existing resource cleanup.
The process-local sealed image descriptor map is closed separately.
BlockFS, MountFS and Node host implementations enumerate their real tables;
BlockFS closefrom removes legacy console entries even though close historically
preserves them. Node host reuses its final pipe-aware close implementation.

Each process receives a descriptor-cache epoch SAB, shared by its threads and
fresh for a fork child. closefrom increments it. RemoteFS checks it before
cached reads/seeks/pipe accesses and discards local fd/read/pipe cache entries
after a change. Remaining valid descriptors fall back to their broker owner.
No process-shared mmap feature is added.

## Executed acceptance

Build reservation cfd313c0-decb-40b0-8417-c46494a4993d:
corepack pnpm install --frozen-lockfile; node tools/libc-manifest.mjs;
node tests/closefrom-kernel.mjs; node tools/build.mjs. Passed.

The initial narrow kernel test used a sealed-reader double. It was strengthened
after that run to use a real readonly BlockFS image. Final kernel test passed in
browser reservation ec7926c1-86e7-4198-a7ef-fceab7b34345:
sparse fd 2000000000, shared thread table and epoch, fd flags, local sealed-image
closure and lower fd survival, queued SCM_RIGHTS reclamation, console closure.
The guest runner then failed before compiling/executing a fixture because it
omitted the usual initial shell command that initializes shell.busybox.
The test runner was fixed; no runtime source or rebuild was needed.

Final browser reservation e4d453f6-87b1-4586-be31-241253fa1a66:
node tests/closefrom.mjs. Actual browser/guest results:
- CLOSEFROM-SYMBOL-PASS (dlsym from libc.so.1; fixture does not include unistd.h)
- CLOSEFROM sparse/fork/flock PASS
- CLOSEFROM pipe EOF PASS
- CLOSEFROM thread/negative PASS
- CLOSEFROM-PASS

Guest fixture uses dup2 at fd 1000007 and 1000030, checks lower descriptors,
fd flags, child/parent isolation, retained versus last flock OFD release,
pipe EOF, pthread shared-table closure and negative threshold including stdio.
Kernel probes run actual methods in a VM with minimal process/backend records;
they are not guest execution. Sealed local coverage uses a real BlockFS image.
The active browser process runtime uses broker descriptors, including /usr opens.

Runtime: e1a1d35aab446d25eb19126cec01e17e5508b672bc16f73b4cbb5e0a01f3b8a8
Libc digest: f45a2be27599f654782c341da2a0180c9cd45b62bfe236099b68f16ed376f6a3

Exact logs, build identity, dist hash inventory and original runner failure are
under docs/research/ipc-patch/closefrom/. Handwritten runners/fixtures are in tests/.

## Limits and review points

No getdtablesize or close_range was added: there is no finite configured fd
limit to report truthfully. Existing unlimited getrlimit/setrlimit behavior is
unchanged; tmux preflight must use the available closefrom directly.
This is not a tmux lifecycle acceptance result or a broad regression-suite run.
Native Node host and MountFS wiring were inspected but not separately executed
as guest targets. No claim of new local-sealed descriptor sharing across workers:
the existing local sealed adapter is process-local; active threaded browser
processes use the kernel-owned shared descriptor map.
Direct embedders must propagate the new fdEpoch option (as both bundled worker
factories now do) to invalidate per-worker fast caches across shared-table closure.
An in-flight read/write racing closefrom may complete as with close; this change
does not cancel already submitted I/O or impose new fd limits.
