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

Tmux is the intended integration demo. Its dependency consumer checks pass
with exported bootstrap artifacts on the integrated runtime, but tmux build
and interactive lifecycle acceptance remain pending. No release or deployment is established
by these candidate results.

The adapted upstream probes remain one-off research evidence. They are not
added to the recurring core test gates; selected handwritten regressions live
under tests/.
