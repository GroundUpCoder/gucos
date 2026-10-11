# Unix socket IPC implementation — 2026-10-11

Thread: 01a129b7-42ba-7f97-8d1e-ef30d3ae607d.
All project edits, dependency setup, builds and tests used SSH agentone and
/home/ubuntu/git/gucos-ipc. No local builds/tests, graphics work, package-source
build inputs, version change, publication or deployment.

Baseline: clean cdf19e035e08f77d2e4246d879a53fbe04a6a101, labeled as a snapshot
of fc5f9baac0296eca486721fb6a5665e40c431b90. The original object is absent.
Coordinator staging record confirmed snapshot intent. The services worker
01a129b7-d01e-7062-ad28-bcdca22c4b54 uses a separate checkout; coordination
reserved socket/OFD work here and left service-module integration to the
coordinator. Some cc-meta sends were refused while recipients were running;
successful status messages were sent to the coordinator.

## Change

SCM_RIGHTS queue ownership, shared OFD semantics, receive truncation and peek,
plain-read disposal, CLOEXEC, blocked-operation holds, bounded backpressure
and cyclic queue reclamation. Added pathname datagrams and sequenced packets.
Implementation and limits: docs/research/unix-socket-ipc/README.md.

New runtime operations and imports are additive; existing C structures and
imports remain. libc/manifest.json changes only its source digest.

## Executed checks

Every command below ran remotely from this checkout under
/home/ubuntu/.local/bin/machine-job. Build-only commands used --kind build;
browser-containing commands use --kind browser.

Passed:

- node --check src/kernel.js and node --check src/host.js.
- node tests/ipc-socket-kernel.mjs: IPC-SOCKET-KERNEL-PASS. Latest run includes
  blocked calls surviving concurrent descriptor close, empty-packet
  backpressure, datagram sender-only shutdown and dead-peer disconnect.
- cc -D_GNU_SOURCE tests/fixtures/ipc-sockets.c -o .cache/ipc/native-sockets
  followed by .cache/ipc/native-sockets: IPC-SOCKET-PASS on native Linux.
- cc -D_GNU_SOURCE -DNATIVE_LINUX
  docs/research/unix-socket-ipc/adapted-probes.c -o .cache/ipc/adapted-native
  followed by timeout 15 .cache/ipc/adapted-native:
  IPC-UPSTREAM-ADAPTATIONS-PASS: 15 graph variants + 3 FreeBSD cases.
  NATIVE_LINUX does not assert immediate graph garbage collection. The
  FreeBSD shutdown oracle was adapted to final close, explicitly documented.
- node docs/research/unix-socket-ipc/compile-probes.mjs:
  handcrafted fixture compiled/validated 148647 Wasm bytes with enableFork;
  adapted probe compiled/validated 37892 Wasm bytes without enableFork.
  This checks compilation/validation, not guest execution.
- node tools/libc-manifest.mjs and node tools/build.mjs completed. The
  pre-dead-peer-fix candidate runtime was
  15362529a44d83b3100c2acc2841a5c241160c18037802f18d554980e4a0fe2d.
  A rebuild after the final datagram error correction is running at this
  checkpoint; that prior runtime is not the final candidate identity.
- git diff --check.

Native edge probe observed: datagram peer close gives ECONNREFUSED (111),
then ENOTCONN (107); peer SHUT_RD gives EPIPE (32) on repeated sends.
The implementation distinguishes dead peer from read shutdown accordingly.

## Validation corrections and limits

The first kernel test harness lacked the constructor's createWorker
capability and failed before test execution; the harness was corrected.
An initial native graph probe expected immediate collection, which Linux did
not expose via the pipe EOF oracle. Another native probe blocked expecting
FreeBSD-style SHUT_RD disposal; its own scheduler job was terminated.
The revised native probe explicitly relaxes immediate graph collection and
checks final-close disposal. These are portability adaptations, not passing
results for the original FreeBSD/Linux suites.

The remote browser installation needed system libraries. Ubuntu 24.04's t64
package names were required; a first download using older names failed.
Required debs were downloaded and extracted under .cache/browser-libs using
the scheduler. No system package installation occurred.

The C fixture requires cc --enable-fork; the browser command uses that flag.

At this checkpoint, no guest/browser runtime result is claimed. The browser
job is queued behind the ongoing tmux dependency build and services-worker
test. It will use this command inside one browser reservation:

    export LD_LIBRARY_PATH="$PWD/.cache/browser-libs/root/usr/lib/x86_64-linux-gnu"
    node tools/build.mjs &&
    node tests/ipc-sockets.mjs &&
    node docs/research/unix-socket-ipc/run-probes.mjs &&
    node tests/platform-posix.mjs

The browser sessions are sequential and bounded to three minutes for each new
IPC probe. The existing platform POSIX test retains its own timeout.

Integration with flock/eventfd/named semaphores and tmux lifecycle acceptance
remain separate, unverified work. No internal sub-agent work was used.

