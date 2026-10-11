# Incremental package evolution

Status: authorized design, 2026-10-11; implementation and acceptance are recorded
per increment below. This document does not assert that proposed features ship.
Baseline: core8311e11, platform1.0.1. User requested Snake publication first,
then the preceding packaging recommendations, explicitly excluding .deb support.
Existing migration worker retains ownership of its package queue and artifacts.
All builds/browser gates use machine-job; browser jobs remain serial.

## Scope and contracts

Retain the core/package repository boundary, target wasm32-wasm-posix, platform
1.x execution/relinking compatibility, format1 manifests and explicit build versus
binary-install behavior. No Linux binary compatibility, hidden package discovery
at core boot/build/deploy, or historical-disk migration is introduced.

The ports workflow is source recipe -> staged outputs -> functional tests ->
immutable binary -> fresh install/reload -> reviewed catalog publication. Sources,
patches, preparation inputs, licenses and test scope belong in the packages repo.
A recipe is not a claim of acceptance. Normal users install reviewed binaries;
source builds remain explicit. Host-prepared C++ remains separately identified.

## Increment 1: desktop application contract

Use freedesktop .desktop Type=Application entries under share/applications.
Initial supported fields: Name, Comment, Exec, TryExec, Terminal, Categories,
Icon, Hidden, NoDisplay. Specify the supported Exec quoting/field-code profile;
reject unsupported constructs rather than interpreting desktop text as shell.
Use installed package ownership and bounded parsing. Emit escaped JWM XML and
launch arguments without shell expansion. Terminal entries use xterm when
available; direct SDL processes open their own VTs through existing behavior.
JWM integration belongs in a package, not kernel desktop policy. Preserve
existing package output sets when updating the JWM package. Entries are additive
and do not change application ABI. Snake ships an entry from its first release.
Acceptance: install/uninstall visibility, terminal Snake launch, SDL launch,
quotes/metacharacters and malformed entry rejection, no uninstalled suggestions.

## Review corrections (2026-10-11)

Independent reviewer01a123e0 reviewed initial draft SHA256
56d8decf01cae8dc25b741854ad89a95445d906c99946a7bbfa77fb3a0488a83.
The first generation increment is atomic binary selection, initially preserving
package owners and output sets. Prefix directory enumeration is a separate
namespace contract: precreated dangling links and delayed removal must not be
called a completely atomic filesystem-tree replacement. Source builds retain
explicit per-package publication until a private build view exists; do not
silently run a consumer against an old dependency while staging a new one.

Conversion must fence old managers: preserve an incomplete legacy pending marker
(with no ready file) after recovering any real legacy transaction, so1.0.1
mutations/recovery fail closed. New versioned state and a conversion journal
must distinguish this intentional fence from incomplete preparation. Old clients
may still list receipts or change source configuration; they must not publish
package files after conversion. Test actual old manager behavior, including
reload. This proposal still needs implementation review before claiming safety.

Defaults and saved games remain outside manager-controlled immutable releases.
For this increment, configuration preservation is a documented application
contract (share defaults, home copies), not automatic conffile merging. GC roots
include retained generations, pending preparation and current state. No claim
that users cannot edit writable release files; verify integrity before reuse.

Signing requires an exact byte envelope, source identity, key ID and persistent
trust policy. Alias removal cannot discard downgrade protection. Pinning a
historical signed snapshot is distinct from treating it as latest. Older clients
do not enforce a newly introduced signature policy.

Bulk-copy optimization is independent of generations and may ship earlier once
Snake and desktop work are accepted. Keep separate measurements for per-file
process overhead and duplicate download hashing.

## Increment 2: package-set generations

For binary operations, build a complete prospective installed set from the current set and requested
changes. Validate dependencies, reverse dependencies, origins and complete output
ownership before activation. Stage all changed packages and the environment view
before one current-generation switch. Preserve the old generation and exact
manifest receipts. Interrupted preparation cannot change the active environment;
interrupted finalization must be recoverable and idempotent.

Do not claim a symlink switch alone fixes already-running dynamic loads: running
programs can retain old code/data, and stable global paths can resolve new files.
The contract promises the selected installed set at activation, not process
snapshot isolation. Rollback changes installed package selection, not user data
or arbitrary effects from trusted source recipes.

Existing 1.0.1 core disks need an explicit, journaled package-state conversion
on a package mutation, preserving existing binaries, source identities and user
files. Old journal recovery must precede conversion. Never touch legacy OS disk
namespaces. Retain a readable compatibility view for existing receipt/pack tools.
Acceptance: dependent multi-package update, rejection leaves full old set, kills
before/after switch, old pending recovery, installed disk conversion and rollback.

## Increment 3: lifecycle and mutable data

Add remove (refuse remaining reverse dependents), file-set-changing upgrades,
generation listing/rollback and explicit garbage collection. Never delete
unmanaged paths or files merely because they collide with a planned output.
GC retains current/rollback roots and acts only on manager-owned immutable state.

Separate immutable package files from mutable configuration and application data.
Defaults live under share; initial user configuration uses explicit copy-on-first
use. Saves remain in the user's home. A managed configuration extension must keep
user edits and expose changed defaults separately; it cannot silently overwrite
edited files or promise data-schema rollback. Avoid arbitrary install scripts.
Acceptance: edited/default/deleted config cases, save preservation, conflicting
unmanaged files, dependency-protected removal, GC references and recovery.

## Increment 4: repository authentication

Sign catalog snapshots with explicit repository keys; hashes continue binding
manifest and artifact bytes. Key enrollment must be explicit and source-scoped;
no automatic trust of a key downloaded from the same untrusted catalog. Preserve
unsigned legacy sources with a clear policy, rather than silently changing the
trust of existing installed applications. Signing private keys never enter public
repositories, browser bundles or logs. Document rotation/revocation and replay
limits before making security claims. Test tampering, wrong key, source identity,
key rotation and signed-to-unsigned downgrade refusal for enrolled sources.

## Increment 5: installation performance

Measure the same immutable package on old and candidate runtimes. First replace
per-file shell subprocess loops with bounded bulk helpers for copying, modes and
checksum preparation. Preserve symlink-parent refusal, integrity checks and
progress. Measure copy/checksum phases and total time separately. A read-only
package filesystem is an optional later design only if measurements justify its
extra filesystem/format contract. No performance claim from changing format names.

## References

- https://docs.freebsd.org/en/books/handbook/ports/
- https://www.netbsd.org/docs/pkgsrc/pkgsrc.html
- https://specifications.freedesktop.org/desktop-entry/latest-single/
- https://nix.dev/manual/nix/2.35/package-management/profiles
- https://www.tinycorelinux.net/corebook.pdf
- docs/PLATFORM.md and docs/PACKAGES.md are the current shipped contracts.

## Completion ledger

- Snake: isolated package candidate prepared; source/gameplay/public gates pending.
- Desktop entries, generations, lifecycle, authentication, performance: design
  only. No implementation or deployment claimed by this initial record.
