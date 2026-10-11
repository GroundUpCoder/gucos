# IPC patch work: authorized scope and execution

User direction: implement efficient, honest Unix IPC facilities with tmux as the
flagship package. Graphics, general shared mappings, and full IPC compatibility
are excluded. Ship as an additive patch, provisionally targeting 1.0.2; defer
1.1 until the accumulated interfaces have sufficient confidence. Preserve the
1.x binary/object contracts and existing published packages.

## Scope

First implementation units are Unix socket descriptor passing, local datagram
and sequenced-packet sockets, BSD flock, Linux eventfd, and POSIX named
semaphores. Audit existing stream/pipe readiness, descriptor lifetime, and
terminal/process behavior through tmux. Other good-fit facilities remain survey
items: FIFO completeness, message queues, peer credentials, pselect, and
readiness APIs. Do not silently equate this list with full Linux/BSD semantics.

No general MAP_SHARED, shmat, process-shared synchronization in separately mapped
guest memory, DMA-BUF, GPU broker, or graphics extensions. Unsupported operations
must fail honestly. New APIs may require rebuilt consumers without invalidating
old binary execution or relinking contracts.

## Validation policy

All builds and tests for this task run on agentone through its installed
machine-job scheduler. Browser-containing commands reserve the browser lane;
non-browser builds use the general lane. No GPU is needed for this work. Browser
tests still verify actual guest Wasm execution and terminal behavior.

Adapt selected Linux/BSD tests as one-off confidence probes, preserving original
source/version/license, adaptations, results, and limitations under
docs/research/ipc-patch/. Do not adopt broad upstream suites as recurring gates.
Retain only selected high-signal handwritten regressions. Native Linux reference
results are not evidence that the guest implementation passes.

Tmux acceptance should exercise terminal descriptor transfer, server startup,
multiple panes, input and resize, detach/reattach, continued child progress,
client/server exit, and terminal recovery. Existing published binaries must be
tested unchanged against the candidate. Application recipes remain in packages;
core assembly must not discover package checkout inputs.

## Actual setup and provenance

- Staged core commit fc5f9baac0296eca486721fb6a5665e40c431b90 on agentone as
  /home/ubuntu/git/gucos-ipc, with a separate services checkout. The remote Git
  baseline is an explicitly labelled snapshot, not the original commit object.
- Installed existing cc-meta CLI on agentone and verified a read-only project
  settings request. No server deployment or credential copying was needed.
- Remote dependency bootstrap completed with npm install --no-package-lock;
  npm ci was unavailable because the tracked snapshot has no package lock.
- Native Linux SCM_RIGHTS probe passed shared-offset and sender-close checks.
  This was a native Python probe, not a guest or cross-process acceptance test.
- Agentone scheduler reported two CPUs, 8192 MiB configured memory, and no jobs
  during initial inspection; Chromium installations were present.

Real cc worker threads (all executor codex, model gpt-6-astra, verified from
thread metadata):

- Socket implementation: 01a129b7-42ba-7f97-8d1e-ef30d3ae607d.
- Locks/notification services: 01a129b7-d01e-7062-ad28-bcdca22c4b54.
- Tmux package port: 01a129b8-ba13-7d36-b0bb-066591a700b1.

These threads execute tool work over SSH; model inference is not claimed to run
on agentone. Integration, review, version bump, and publication remain pending.
Concurrent existing package porting work is preserved in its own checkout.

## Integrated candidate checkpoint

The remote integration checkout is `/home/ubuntu/git/gucos-ipc-integration`.
Its completed source build produced runtime
`baae3b7591a51a357d4fba3fc8e96135790025d61a6c98453e28100ea99f1aad`,
preserved separately at
`/home/ubuntu/git/gucos-core-snapshots/ipc-integrated-baae3b75`.
This candidate still identifies as 1.0.1; the proposed patch version has not
been applied or published.

Independent source review found four issues: controlling-terminal checks on
received descriptors, per-descriptor window-size queries, canonical shared-libc
IPC symbols, and named-semaphore cleanup on failed fork initialization. Fixes
`7d49a48` and `8c62bfb` were independently re-reviewed; all four findings are
closed at source level. Reports are retained under
`docs/research/ipc-patch/review/`. This is not runtime acceptance.

Remote focused kernel tests passed, including detached terminal reads and
failed-child cleanup. The integrated browser reservation
`c5532f48-48dd-4751-b47a-f82c88b30bd5` remains queued behind the existing tmux
dependency build and worker checks. Its requested checks cover socket/services
APIs, transferred terminals, dynamic symbol lookup, adapted upstream probes and
existing POSIX behavior. No result is inferred from queue admission. Tmux
lifecycle, unchanged published-package execution and retained-object relinking
remain acceptance work.
