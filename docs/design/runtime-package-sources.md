# Standalone OS and runtime package sources

The core builds and runs without any packages checkout or package network request.
Netguc hosts only the immutable core distribution. Browser users configure remote
package sources, discover packages, explicitly build recipes or install binaries.

## Ownership and provenance

Import the fixed bootstrap from wasm-posix-old minimal at 0b9b3245f. Preserve
licenses and source offers. Keep one authoritative compiler/runtime/libc copy.
Bootstrap includes shell, utilities, Make, cc, ar and HTTP fetch. Keep the reviewed
SDK boundary available for later workers, but core assembly never discovers apps.
The old release and browser storage remain preserved. New disk identity isolates
this new distribution from the historical os2 installation.

## Runtime package interface

Default source named groundupcoder points to the public raw GitHub packages catalog.
Persist sources in /etc/gucman/sources.d as data, seeded only on a new disk.
gucman repo add/remove/list manages sources. list/search aggregate all configured
sources and report unavailable sources; duplicate names require source:name.
info/install/build accept qualified names. Installed records retain source identity;
no silent switch of origin during updates. Removing a source leaves installed files.

Use a versioned static catalog with package manifest URL, SHA256 and byte size.
Catalog manifests reference immutable recipe/source/binary objects by URL/hash/size.
Metadata is data, never evaluated as shell. HTTP allowed only loopback for tests;
normal repositories require HTTPS. Validate paths and names before filesystem use.
Fetch into temporary files, verify size/hash before use, and cache content by hash.
The catalogue supplies exact manifest refs; publisher pins commit-addressed URLs.
An install/build resolves a fixed catalog snapshot and dependency closure before
execution. Same-repository dependencies by default; explicit source:name allowed.
Detect cycles and duplicate package names across selected closure before mutation.
No dependency resolution by arbitrarily choosing highest version across sources.

Install consumes binaries only. Build runs explicit source recipes in guest ash,
with declared inputs in a private build directory and DESTDIR staging. Tests run
before publication into /usr/local. Recipes are trusted programs, not a sandbox.
Package manifests declare outputs and licenses. Publish through checked per-package
release directories and owned public symlinks, with a transaction lock and atomic
current-link change. Reject unmanaged or other-package file conflicts. Retain old
release on failure. Builds needing host C++ preparation must declare prepared source
objects; browser does not pretend to run host Clang. Source/build objects are hosted
where browser CORS permits; no core proxy or host package checkout is involved.

## Packages repository

Own manifests, upstream identities, recipes, patches, tests and generated static
catalog metadata. Start with jsmn. No source checkout is required to build core or
serve netguc/os. Generic publisher tooling may accept explicit input directories;
never auto-discover the sibling repository. Binary publication and source hosting
are separate from core deployment. Keep package records extensible and versioned.
Private repository cannot serve anonymous clients: public visibility or a public
mirror needs a user decision; do not embed GitHub credentials in the core.

## Hosting

Netguc/os accepts a built core distribution plus a manifest of exact hashes. No
package catalog, recipe or binary store is a deployment input. Preserve historical
/os2 and its objects while serving the new OS at /. Retain old releases; switch a
new dedicated current symlink atomically with rollback. Keep COOP/COEP, correct MIME,
no-cache entrypoint and immutable hashed assets. No service/backend or new DNS.
Review exact design and implementation before deployment; validate Caddy, check
origin/public bytes and browser persistence. Do not migrate historical user disks.

## Acceptance

Build with sibling packages unavailable; detached build with no parent checkout.
Browser fresh/reload compiler/Make/OPFS checks. Two distinct HTTP origins provide
CORS-enabled fixture repositories: combined search, duplicate refusal, explicit
selection, add/remove persistence, source build, binary installation, dependency
closure/cycle/refusal, bad hashes, conflicting ownership and failure preservation.
Tests must use browser gucman; host recipe execution is not a substitute.
Hosting checks prove only core assets are required, preserve historical paths and
exercise rollback. Document exact tested scope, gaps and review receipts.

## Core stability

Stable versioned syscall/object/libc/package interfaces reduce changes. UI/core
build identity is separate from compatibility. Begin conservatively with exact
runtime binary matching; broaden compatibility only with demonstrated contracts.
No claim that arbitrary upstream software works, or that core fixes end forever.

## Review clarifications

Source identity v1 is the exact HTTPS catalog URL, independent of the local label.
Installed packages retain it. Reusing a label for a different URL cannot upgrade
those packages; removal never uninstalls. A source is trusted by explicit repo add
(or the documented default); TLS authenticates its delivery, hashes bind its objects,
not independent publisher identity. Catalog snapshots and source URLs are copied
at command start. Cross-source dependencies require explicit source:name declarations
and are displayed in the resolved plan before build; they only use configured sources.
No hidden sources or default fallback. Source recipe execution has the same guest
filesystem authority as the user. This warning is emitted by build; tests and recipe
failures preserve package-manager publication but cannot undo arbitrary recipe writes.

Transactions are per package, dependencies commit in dependency order and may remain
installed if a later consumer fails. All closure ownership conflicts are preflighted.
A global mkdir lock excludes concurrent commands; crash leaves it for explicit
recovery, never PID guessing. Each publication retains a pending record with new
release identity and old link; recovery completes or rolls back exact owned links.
V1 refuses upgrades changing the public output path set. Old release directories
remain; no garbage collection. Test failure/interrupt at link preparation and current
switch, collisions, two concurrent commands, and source-URL replacement.

New identities are wasm-posix-core-v1, wasm-posix-core-v1.journal and
wasm-posix.core.files. Neither init nor reset may touch historical
wasm-posix-os2-v1, its journal, wasm-posix.os2.files or legacy busybox disk/editor keys. Preserve prototype wasm-posix-minimal-v1,
wasm-posix-minimal-v1.journal and wasm-posix.minimal.files too; sentinel tests
cover their initialization/reset preservation.
Core independence means boot/reload while package origins are unavailable; it does
not promise reload while the core hosting origin is offline. No service worker.

The default URL is
https://raw.githubusercontent.com/GroundUpCoder/wasm-posix-packages/main/catalog.json.
Source objects/manifests should use commit-addressed URLs. Fetches are credential-free,
bounded to 8MiB catalogs and 256MiB objects, with transport deadlines. Only HTTPS and
loopback HTTP input URLs accepted. HTTPS redirects must remain HTTPS and contain no
credentials; actual final URL is checked by the fetch helper. Public delivery needs
an actual-origin browser test before claiming anonymous acceptance. Repository
visibility remains a user-owned choice; fixtures do not prove GitHub delivery.

Hosting uses a separate /home/ubuntu/.gucchat/os/core tree, immutable releases by
manifest digest and current symlink. The existing OS operator.lock serializes all
mutations, but historical package state is never an input or modified. A core-only
pending record binds old/new release and route hashes; interrupted operations refuse
new deploys until explicit reconciliation. Private staging validates complete payload
hashes and source tips; exact reviewed grants bind artifact and Caddy fragment.
Preserve every existing /os2 route and its storage; only / changes to new index,
with new /assets URLs. Route is activated after private-origin verification,
Caddy validation/reload and public smoke; failed validation never switches route.
Rollback restores prior root handling and/or the prior core symlink, with distinct
browser storage preserved. This replaces only the relevant hosting instructions
after exact review, not historical package publication rules.
