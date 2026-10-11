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
  Final rebuild after the datagram correction passed at runtime
  67433230c8e563573492201318ed15f8de0e6e68ea668cca2bf66a4a7e81e5d6.
  The prior runtime is retained here only as historical evidence.
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


## Committed checkpoint

Implementation commit: ee68a97179be5bc6e9c28035818715b6ee579081 on
ipc/unix-sockets. Final source build passed. Native host is Linux
6.17.0-1019-aws on Ubuntu 24.04.4.

A follow-up native datagram probe installed a SIGPIPE handler and sent after
peer SHUT_RD: errno 32, zero SIGPIPE deliveries. Sending from a fresh AF_UNIX
datagram socket returned errno 107. The implementation suppresses datagram
SIGPIPE and reports ENOTCONN for a missing destination.

Browser job 8eb14db7-b48e-4377-a7cd-d769f52950e2 remains queued. Its result
must be collected before any browser acceptance claim. The coordinator
received the implementation commit and was told this limitation explicitly.

## Terminal descriptor acceptance addition

Coordinator integrated through ef5a450 into the separate integration checkout.
The original implementation snapshot remains unchanged for independent review.

Added to the existing queued guest test: a forked process transfers both PTY
master and slave using SCM_RIGHTS; all original descriptors are closed and
the sender exits. The receiver verifies isatty, CLOEXEC, termios, shared
window size, and byte transfer in both directions. IPC-SOCKET-PTY-PASS is now
required by the browser runner. The runner also logs the runtime identity
fetched from the same test server's build.json.

This is an acceptance-fixture addition, not a kernel/runtime change.
The existing browser reservation and continuation remain in place.
Browser results remain pending at the time of this edit.

The expanded fixture passed native Linux execution with
IPC-SOCKET-PTY-PASS and IPC-SOCKET-PASS, and compiled/validated 173916 Wasm
bytes with enableFork. This is not browser execution evidence.

Both IPC browser runners now persist their outcome, actual served runtime,
fixture SHA-256 and output/error under test-results/ipc-sockets.json and
test-results/ipc-adapted.json. The earlier tool session ID is not accessible
from the new turn; the remote scheduler reservation remains queued and was
not replaced. These result artifacts let the continuation collect the actual
guest outcome without starting a duplicate reservation.
