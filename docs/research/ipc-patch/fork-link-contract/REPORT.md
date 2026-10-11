# Fork-enabled application dependency linkage

Recommendation to coordinator/package worker: first try the existing ABI-compatible
static archives at every relevant configure probe and final tmux link, retaining
--enable-fork on those links. Explicit .a paths avoid accidental .so selection.
Alternatively use -Wl,-Bstatic before dependency -l flags and -Wl,-Bdynamic
after them; this retains automatic libc-fork.so.1 selection. -static is broader:
it also suppresses automatic shared libc. Do not disable loader ABI checks.

## Read-only findings

compiler-driver.js: ordinary -l prefers .so before .a. With --enable-fork the app
selects libc-fork.so.1. -shared uses libc-fork.so.1 runtime contracts only when
--enable-fork is supplied to that DSO link.
compile.js: -c emits relocatable objects. objects.js links retained object WAST
and calls MemoryTrampoline.lower with final link compiler options. Therefore
ABI-compatible non-fork-produced .o/.a members are lowered anew at final fork link.
A DSO is already lowered: its import contracts are not rewritten by the app link.
dynamic-loader.js validates JSPI/native contracts against the actual provider.

The inspected libevent recipe emits both libevent.a and a DSO from the same
objects; neither object nor DSO commands specify --enable-fork.
The inspected ncurses recipe explicitly uses --without-shared (narrow and wide).
Thus that recipe does not establish that installed ncurses DSOs exist.
The tmux recipe uses --enable-fork and ordinary -l dependency discovery; existing
libevent.so can therefore be selected. This is a mechanism consistent with the
reported symptom, not identification of the exact DSO in the worker's live guest.

## Executed tiny reproduction

Agentone browser reservation 9227d299-fb9c-4a1c-be7a-b175b9f84b8a.
Frozen runtime e1a1d35aab446d25eb19126cec01e17e5508b672bc16f73b4cbb5e0a01f3b8a8.
One provider.o compiled WITHOUT --enable-fork; it calls fprintf and a callback.
All tests use the unchanged provider.o:
- Non-fork DSO + non-fork app: PROVIDER, PLAIN-42 (PASS).
- Its archive + final --enable-fork app: PROVIDER, FORK-42 (PASS);
  callback performs a real fork and waitpid, child exits normally.
- DSO RELINKED with --enable-fork + fork app: PROVIDER, FORK-42 (PASS).
- Original non-fork DSO + fork app: expected exit 127,
  JSPI execution contract mismatch: fprintf; relink the consumer against this library.

Captured metadata: original DSO execution.clone=false, invoke execution=jspi-v1,
engineImports=[fprintf]. Relinked DSO execution.clone=true, invoke execution=clone-v1,
engineImports=[].

## Scope

The smallest fix is archive selection and final-link lowering, not a speculative
dependency source rebuild. If retaining shared linkage, relink the affected DSOs
from existing compatible relocatable objects with --enable-fork and matching
providers, using separate fork-specific artifacts/sonames or isolated paths.
A fork-capable DSO is rejected in a non-fork process, so do not globally replace
DSOs used by ordinary non-fork consumers. A bare archive on a -shared link is not
a proven whole-archive export mechanism: use the complete original object list
(or properly extracted archive members) when relinking a dependency DSO.

Not all non-fork DSOs are inherently forbidden in a fork process: loader admission
allows them if their contracts match; this fprintf/callback case needs relowering.
Retained object ABI, memory ABI and signature checks remain authoritative.
The actual libevent/ncurses retained archives and tmux configure were not rebuilt
or executed by this investigation. Tiny inputs were compiled with the current
frozen compiler, not the old bootstrap producer. No claim that existing packages
are broken; the app switched execution models.

Only /tmp reproduction/evidence files and guest files were written. No core or
package source/recipe edits. No full build, deployment, or interrupted reservation.
All 63 snapshot payload hashes still match its manifest.
