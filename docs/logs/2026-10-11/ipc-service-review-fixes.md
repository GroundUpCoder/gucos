# Integrated services and independent review fixes

Coordinator captured services worker source without changing its checkout;
per-file hashes and attribution are in docs/research/ipc-patch/service-snapshot.json.
Snapshot commit 6cd7972 was integrated as 516ff02 after socket and terminal fixes.
Cancellation retains both socket-held references and IPC service cleanup.

Review finding 3: canonical __semaphore.c now includes flock/eventfd declarations
and emits external definitions of semaphore and eventfd convenience APIs while
consumer headers retain their existing inline implementations and sem_t layout.
Added a guest dlsym probe that does not include the API headers itself, so it
cannot accidentally supply the missing names through its own direct imports.

Review finding 4: fork-child initialization crash path releases cloned semaphore
handles. Focused injected-crash kernel regression passes on combined source.

Remote machine-job build reservation f7e3547d-f541-4e29-878e-cc6e15495a2a passed
socket lifecycle, service lifecycle, failed-fork cleanup and terminal kernel
regressions. These are actual kernel methods with simulated process records,
not guest execution. Full build was running at this checkpoint; guest dynamic
lookup, terminal regression and combined runtime acceptance remain pending.
