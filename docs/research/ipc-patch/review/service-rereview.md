# IPC service fixes re-review

Reviewed exact commit 8c62bfbc0108d131bb941daa48468212a6ab816b in agentone /home/ubuntu/git/gucos-ipc-integration. Scope: original findings 3/4 only. Read the committed service-snapshot.json provenance record; no access to or edits of the author's working tree. Reviewer cc-meta metadata confirms codex/gpt-6-astra.

No remaining defect identified in these fixes. Findings 3 and 4 can close at source-review level. This does not establish built-artifact or guest acceptance.

## Finding 3 — symbol retention traced through the actual build/link code

- libc/src/__semaphore.c:1-5 defines the implementation macros before the headers, so eventfd_read/eventfd_write and semaphore wrappers become ordinary external definitions in this canonical translation unit. Consumer headers still use static inline; sem_t remains one int.
- The same translation unit now includes the __import declarations for flock and eventfd. libc/manifest.json includes __semaphore.c in sources and does not skip it.
- src/compile.js:81-96 builds relocatable objects with whole-program gcSections disabled. :140-151 supplies preserveHostImports:true. The per-TU optimizer roots external definitions (src/compiler.js:6895-6898) and skips imported-function pruning with that option (:6950). Thus the uncalled flock/eventfd declarations survive object construction; the new public wrapper definitions also survive.
- src/compile.js:158-164 supplies all nongraphics canonical archive members as explicit shared-libc inputs, with shared:true and runtime:true. __semaphore.c matches none of the exclusion prefixes, so this is not dependent on an application reference pulling its archive member.
- src/objects.js:625 roots retained host imports for runtime DSOs; :635 roots public dynamic definitions. The only publish exclusions at :617-618 are fork/clone names, not these IPC names. :828-836 writes both definitions and retained imports into dynamic symbol metadata.

Consequently the source rules retain and publish flock, eventfd, eventfd_read, eventfd_write, sem_open, sem_close, sem_unlink, sem_timedwait, sem_init, sem_destroy, sem_post, sem_trywait, sem_wait and sem_getvalue in canonical libc.so.1 and libc-fork.so.1. This is a source-derived conclusion, not an inspected output symbol table or executed dlsym result.

The new guest fixture only includes dlfcn.h/stdio.h and names IPC APIs as strings. It does not create direct IPC imports. It opens libc.so.1 explicitly; the browser cc driver's default shared-libc path is consistent with this probe. The fixture checks the previously missing names and several adjacent APIs. It checks symbol lookup, not the calling ABI or functional behavior of each API, and it does not separately probe libc-fork.so.1. These remain execution-coverage limits, not defects in the retention fix.

## Finding 4 — failed-child handle ownership

src/kernel.js:2613-2620 now calls _ipc.release(pcb.pid) in the asynchronous pre-fork-ready crash branch, complementing the synchronous createWorker failure cleanup at :2361 and ordinary exit at :5846. src/ipc-services.js:113-121 clones exactly the parent's counts, then release removes the child's handle map and subtracts those counts. collect preserves linked objects and any remaining references/waiters; otherwise it reclaims the semaphore. Parent/sibling handles are not deleted.

The branch marks the PCB zombie and removes it in the same kernel turn. The later worker onExit handler (:2366-2368) only enters ordinary exit for STATE_RUNNING, and release itself is harmless when that pid no longer has a handle map. No new double-decrement identified. The merged cancellation path also retains both socket-reference cleanup and IPC cancellation (:5795-5797).

The injected regression invokes the actual kernel crashed-message path with a simulated child PCB, checks removal of its service handles, then unlinks/releases the parent's handle and checks final semaphore reclamation. Its reported pass is coordinator evidence; I read the test but did not rerun it.

## Evidence and limits

Exact committed source and diff snapshots are retained on agentone at /tmp/gucos-service-rereview-29lv1qdj/ with SHA256SUMS. All copied snapshot entries were SHA256-verified locally. Exact diff SHA256: 82954102d5c414f03d4366fc2a362004d272da15d9ed5e25b59af076295c6c11.

No local or remote tests, builds, author edits, deployment or publication performed. Combined build and guest tests were pending per the request; no completion inferred. Prior terminal findings 1/2 remain source-closed, without runtime evidence implied.
