# Independent bounded closefrom source review

Reviewer thread 01a129d1-a91d-7e5c-87c9-bb762e741018; cc-meta metadata directly verified executor codex, model gpt-6-astra. No delegated reviewers.

Reviewed agentone /home/ubuntu/git/gucos-closefrom, branch ipc-closefrom, observed HEAD 4c5e33f812295ce12fdb67a50e6f4ec230bf0d5f plus the captured uncommitted changes. Capture UTC 2026-10-11T08:29:36.838051+00:00. The author tree was not edited. All tracked and nonignored untracked file contents were captured, including the new tests. HEAD, diff and status matched before/after, and every captured file was reread with matching SHA256. This is a stable double-read capture, not an atomic filesystem snapshot. Source/diff files are read-only in a separate temporary review directory. Copies were verified against all artifact SHA256 values before inspection.

Evidence on agentone: /tmp/gucos-closefrom-review-s90chynf/ with source.tar.gz, changes.patch, files.sha256, metadata.json and SHA256SUMS.

- source.tar.gz SHA256: 5997706f39dbae01d1da5432e2bee9ccbfa801ba9b036b14de0705913edbe922
- changes.patch SHA256: 6f4c6dad4ed8cbc96b433432990c2a0ea4d417a71e219552344952bed54577d0
- files.sha256 SHA256: 916be95892b0b0f7f79be93f661001c26d20d937eaccfe2cc6197855cad69a82

## Result

No blocking defect identified in the captured implementation for the shipped brokered browser runtime or for cleanup of a calling RemoteFS instance's own sealed-image descriptors. No tests or builds run by this reviewer. This conclusion applies only to these source bytes, not an author commit made later, an unspecified dist, or tmux acceptance.

## Source observations and conclusions

1. Descriptor enumeration and API: libc/include/unistd.h:19 declares void closefrom(int), with the external wrapper at libc/src/__posix.c:17-18 normalizing negative lowfd to zero. RPC 0x0443 is uniquely assigned in the captured opcode table. src/kernel.js:1790-1797 enumerates Array.from(pcb.fds), not an fd ceiling, deletes each selected map/flag entry, and releases exactly that descriptor reference. It also clears remaining selected fdFlags entries. Descriptors below lowfd stay present. Neither getdtablesize nor any guessed maximum is introduced. The void API does not propagate the private import's return value; it is a BSD-style void interface, not an int-returning extension.

2. Ownership: the path uses _ofdUnref(id, pcb.pid), so normal pipe EOF/EPIPE, flock final-OFD release, POSIX record-lock close behavior, socket queue discard and cycle collection remain in force. A surviving low duplicate, inherited descriptor in another process, queued SCM_RIGHTS reference or service-held OFD reference prevents premature final release. Closing all selected aliases drops one reference per fd. Closing a selected receive socket discards its queued rights through the existing socket teardown. No new independent counter or lock owner was introduced.

3. Fork/spawn/thread lifecycle: fork creates a distinct PCB fd map and epoch (src/kernel.js:2141, :2197-2201), sharing OFDs through ordinary refs. A child's closefrom therefore does not delete the parent's descriptors or flags. Spawn CLOSEFROM actions now use the same helper, fixing their previously omitted flag cleanup. Thread channel objects inherit the process's fd/flag maps and epoch (:2510); their worker specifications receive the same epoch buffer (:2529). The browser process bootstrap supplies it to RemoteFS on initial start and exec (src/system-process.js:27, :83). No independent thread fd table is created in the broker.

4. Broker cache invalidation: _closeFrom increments fdEpoch after the sweep. src/kernel.js:6385-6394 detects changes and clears worker-local _fdTable/_pipes. read, write and lseek check the epoch before read-cache or pipe-ring use (:6542-6544, :6618, :6691); other relevant local-fd operations pass through _roFd. Clearing all broker caches also evicts caches for surviving low fds, but those operations fall back to the broker rather than closing the descriptor. _ofdUnref separately detaches file-cache capabilities, and threading demotes SPSC pipes. No stale broker fast-path access after a completed cross-thread closefrom was established in this source pass.

5. Sealed and standalone cleanup: src/kernel.js:6521-6522 enumerates actual keys in the caller's _ro.fds, invokes its normal close path (removing the key and closing the underlying BlockFS fd), then requests the broker sweep. This also handles local dup aliases through normal BlockFS entry references. src/host.js:3357-3363 uses _releaseEntry directly so closefrom(0) removes the standalone BlockFS console slots despite ordinary close's historical console special case. The native-backed adapter enumerates its actual fdTable and uses its wrapped close (including pipe refcount/wakeup handling); MountFS enumerates its actual handle table and delegates close. No new integer-ceiling loop is used.

6. Canonical export: __posix.c is already a canonical libc source. closefrom is an external body, not a static header-only helper. buildLibc preserves __closefrom's host import (src/compile.js:140-151); buildSharedLibc includes __posix.c (:158-164); dynamic-link roots and metadata publication keep the external closefrom definition (src/objects.js:635, :828-836). The new import is implemented by the native filesystem adapter and by the BlockFS/RemoteFS environment method. The dlsym fixture avoids including unistd.h and invokes the resulting function pointer, so it does not itself supply closefrom through a header. Actual generated symbol tables and invocation were not checked by execution here.

## Important scope boundary: sealed descriptors across workers

The new epoch clears _fdTable and _pipes, not _ro.fds (src/kernel.js:6390). closefrom only sweeps its own RemoteFS object's sealed map (:6521). Thus if an embedder constructs multiple worker-local RO-enabled RemoteFS instances for one pid, worker B's closefrom cannot remove worker A's local sealed descriptors: A will still recognize and read them after observing the epoch. There is also no shared cutoff or descriptor identity in this four-byte epoch from which A could perform that cleanup.

This is not on the shipped browser process path: src/system-process.js:27/:83 passes no roFs/roDev/roLeaf, so its descriptors remain brokered. The Node reference bootstrap enables RO but does not wire the browser thread bootstrap. I therefore do not classify the hypothetical combined embedding as a blocker for the current browser candidate. Do not describe this patch as making worker-local sealed descriptors process-shared or validating their cross-thread closefrom behavior; that combination would require broker ownership or explicit shared close bookkeeping. The local sealed test exercises one RemoteFS instance only.

## Validation boundaries

Read tests/closefrom-kernel.mjs and both guest fixtures/runners; did not execute them. The kernel fixture covers sparse ids, fd flags, shared-map closure, explicit cache invalidation, single-instance sealed fd cleanup, queued rights and console slots. The guest fixture specifies fork isolation, flock lifetime, pipe EOF, thread closure and negative lowfd; the symbol fixture specifies dynamic lookup and a no-op high cutoff. Their presence is not a pass claim. This reviewer has no new dist/hash or tmux execution evidence. Parent retains integration ownership; tmux must await its explicit reviewed candidate dist/hash and remote acceptance. Earlier IPC findings remain source-closed.
