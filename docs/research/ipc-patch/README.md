# Additive IPC patch candidate

This work targets a patch release after acceptance, not a claim of full POSIX
or Linux IPC support. The tested integrated candidate still reports 1.0.1.
Its runtime is
`baae3b7591a51a357d4fba3fc8e96135790025d61a6c98453e28100ea99f1aad`.

## Supported subset and boundaries

| Facility | Interface family | Platform mapping |
| --- | --- | --- |
| Unix descriptor passing | Unix/POSIX socket messages, SCM_RIGHTS | References to broker-owned open file descriptions; shared offsets and status survive sender close |
| Local datagram and sequenced-packet sockets | Unix socket APIs | Bounded kernel queues preserve packet boundaries, including empty packets |
| flock | BSD API, also available on Linux | Ownership follows the open file description through dup, fork and descriptor transfer |
| eventfd | Linux API | Broker-owned 64-bit counter, poll readiness and interruptible waits |
| Named semaphores | POSIX API | Kernel-owned named objects and process handles; existing four-byte sem_t layout retained |

This design does not require shared guest virtual addresses or GPU support.
It does not add general MAP_SHARED, System V shared memory, process-shared
unnamed semaphores, or a graphics broker. Named semaphore persistence ends
with the kernel session. Transferring sealed process-local /usr descriptors
fails with EOPNOTSUPP rather than reopening a different file description.

The socket contract and Linux/FreeBSD portability choices are documented in
`../unix-socket-ipc/README.md`. Service details and limitations are recorded in
`../../logs/2026-10-11/ipc-services.md`. New APIs require consumers built for a
runtime that supplies them. Unrecompiled old inline semaphore routines cannot
be assumed to understand newly introduced named handles.

## Evidence collected so far

All execution took place on agentone through machine-job. Independent review
reports in `review/` close four source findings; they are separate from the
execution evidence. Integrated guest checks passed for sockets and terminal
descriptor transfer, flock/eventfd/named semaphores, dynamic libc symbol lookup,
the adapted Linux/BSD probes, and the existing POSIX regression.

`integrated-browser-initial.log` preserves a terminal fixture failure caused
by its own cleanup SIGHUP. `integrated-terminal-retest.log` records the pass
after ignoring SIGHUP only during fixture cleanup, on the unchanged runtime.
`integrated-followup/` retains the remaining guest results and checksums.

`retained-abi/` records execution of unchanged historical executables and a
DSO, relinking against an old object and archive, hash preservation, and a
successful dynamic consumer after reload. This tests retained ABI artifacts;
it is not old-browser-disk migration or proof about every published package.

`published-jsmn/` records a fresh-profile binary installation of
`groundupcoder:jsmn` version `1.1.0-1` on the integrated candidate. Its manifest
declares legacy runtime `2a9cca…`; the unchanged executable passed before and
after reload. Exact fetched catalog, manifest and archive bytes are retained.
The catalog was read from mutable main, not a verified commit pin. Coverage is
one published package, not the entire catalog.

Tmux is the integration demo. Its dependency consumer checks passed with
exported bootstrap artifacts; the later closefrom candidate also passed its
clean build and interactive lifecycle, recorded below. No release or deployment
is established by these candidate results.

The adapted upstream probes remain one-off research evidence. They are not
added to the recurring core test gates; selected handwritten regressions live
under tests/.

## Tmux integration blocker: closing inherited descriptors

The package worker reports that integrated-07 compiled all tmux C translation
units, then failed linking getdtablesize from upstream's closefrom fallback.
The current core reports no finite open-descriptor limit and does not expose
descriptor enumeration through procfs. A guessed scan bound would leave high
inherited descriptors open. Existing spawn closefrom actions already enumerate
the kernel fd map, but do not provide a callable API in forked processes.

The resulting closefrom work was assigned to an isolated remote checkout.
Its requirements cover both broker descriptors and sealed process-local descriptors,
preserve descriptors below the threshold, and perform normal ownership cleanup.
The tested baae3b75 candidate does not contain this addition. Tmux lifecycle was
blocked pending the implementation, review, and guest validation below. No finite
descriptor limit or descriptor-count result is fabricated for the package.

### Reviewed closefrom candidate

The implementation is now committed as remote `d7d91ac` (local `63a553f`).
Runtime `e1a1d35aab446d25eb19126cec01e17e5508b672bc16f73b4cbb5e0a01f3b8a8`
contains the earlier integrated changes plus closefrom. Focused kernel and guest
tests passed for sparse descriptors, fork isolation, pipe EOF, flock ownership,
shared-thread closure, negative thresholds and canonical libc symbol lookup.
The six changed src/libc files matched the independent review snapshot hashes.
The report is retained in `review/closefrom-review.md`.

All five existing guest checks (sockets, services, terminal, symbols and
platform-posix) passed again on the frozen new runtime in reservation
`fb504328-8e9c-4a4d-9ad9-723cc066ab06`; evidence is in
`closefrom-regressions/`. All 63 frozen runtime file hashes remained unchanged.
The candidate was supplied explicitly to the tmux worker for a clean recipe
build and lifecycle testing, which subsequently passed as recorded below.

Scope limit: cache invalidation does not close sibling workers' local sealed
descriptor tables. Shipped browser workers do not enable that configuration;
these results must not be generalized to it. No finite fd limit, getdtablesize
or close_range API was added.

## Tmux flagship acceptance and patch preparation

The external package worker completed a clean tmux 3.5a build and interactive
lifecycle on e1a1d35a in browser reservation
`9beb82a4-86df-4a22-9804-473b2b5619fa`. Recipe `206a49b` links retained dependency
archives with --enable-fork and uses dynamic libc-fork.so.1. The clean build,
version smoke, input/Backspace, two panes, resize, detach with continued child
progress, reattach with unchanged shell PIDs, pane exit and final session exit
all passed. Reattach means the same running browser runtime, not surviving a
browser reload. Package evidence commit is `b8178c0`; the report and selected
machine results are copied under `tmux-acceptance/`. Complete evidence and the
exported artifact remain in the explicitly identified package checkout.

Core metadata now prepares platform 1.0.2 and runtime 0.5.9, preserving compiler
0.5.13 and object/runtime ABI versions 3/5. Feature declarations are unix-rights,
unix-dgram, unix-seqpacket, flock, eventfd, posix-named-semaphores and closefrom,
alongside all existing features. These describe the bounded contracts above,
not complete POSIX or Linux IPC compatibility. The final metadata-bearing build
and acceptance are pending; nothing has been published or deployed.
