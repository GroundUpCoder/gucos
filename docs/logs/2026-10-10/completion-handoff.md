# Complete minimal OS, SDL/WebGPU VTs, packages and deployment

## Authorization and final architecture

User explicitly requested completion of all discussed work, including deployment
to os.groundupcoder.net, and permitted tailcall. Continue implementation through
testing, independent review, integration and actual production acceptance; do not
stop after another plan or documentation checkpoint. Do not restart retired old
packaging workers. No migration of all historical packages is required by this
handoff: finish the agreed standalone/package/graphics architecture and demonstrate
X11 as a runtime package, keeping the historical repository preserved.

wasm-posix owns standalone browser OS/bootstrap/compiler/libc, generic runtime
package client, SDL3-compatible shim, WebGPU bindings and their surface bridge.
Graphics applications occupy separate VTs and own their GPU devices. Terminal VTs
remain available. Remove inherited kernel WM policy, socket protocol and desktop
chrome. X11 is pure userland, initially software rendering with SDL presentation.
No X-specific core API, GPU pixmap sharing or shared GPU broker. Full upstream SDL
is not required: inspected vendor/sdl3/PORT.md in the old repo explicitly packages
the shim. Do not claim its historical benchmarks compare upstream SDL to our shim.

wasm-posix-packages is public (user confirmed), consumed ONLY by explicit browser
package operations. Core assembly and hosting must not discover/read its checkout.
Multi-repository configuration and search already implemented. netguc/os hosts
only a core release. Preserve /os2 production and old browser storage namespaces.

## Local state and tested scope

Core /Users/jku/git/wasm-posix main through d1ebe52, clean before this handoff.
Commits: 96ac37a initial standalone import/runtime packages; 9a7f230 package
hardening/export/public GitHub test; 21ac985 SDL VT target; d1ebe52 process-owned
GPU constraint. Core main not pushed yet in this lineage; check remote before push.
Old repo /Users/jku/git/wasm-posix-old at 0b9b3245f imported from minimal/.
Old acceptance/perl worktrees preserved; do not disturb their user changes.
Scheduler executable symlink points to old tools/machine-job.mjs after rename.
Use machine-job for heavy work, one browser slot, proper build lanes.

Author executed standalone smoke (boot/compiler/Make/persistence), runtime package
tests (multi-source, hash, closure/path conflicts, binary fixture, recovery/outage),
and historical-storage boundary tests. Logs /tmp/wasm-posix-final-core-tests.log.
Real public GitHub browser discovery/build/export/reload passed under job
8375fcb7-eb79-42c9-8b89-5bc0b04f1dd1; log /tmp/wasm-posix-github-package.log.
Runtime 0e26576176880aa87d2757730cd105dd4c2c1428063ce31e58ed78e499238b52.
Actual guest export test-results/github-package/jsmn.tar and receipt.json.
Public binary install NOT yet tested/published. New VT graphics NOT implemented
or tested. Production unchanged. Do not promote author evidence to reviewer evidence.

Core files: src/app.js still imports desktop window behavior; src/kernel.js still
owns /run/wm.sock and compositor policy; libc/src/__SDL_video.c rejects fullscreen.
src/host.js createBrowserWebGPU and libc/src/__sdl3webgpu.c provide existing device
and surface bridge. libc/src/__SDL*.c are shim implementation. Build tool imports
core, no packages. Retain needed GPU/surface mechanisms while removing WM policy.
Test two SDL graphical VTs + terminal, input/focus release, resize/scaling, process
exit/crash recovery, background progress, GPU ownership and no hidden-VT spinning.
Do not freeze inactive X servers/processes or promise physical exclusive modes.

Old apps/x11-server/port/browser-screen.c already uses one SDL window, software
pixman and full-frame presentation. README documents absent GLX/DRI/MIT-SHM,
fixed-size display and local Unix sockets. No X11 acceptance rerun on new core.
Move/package sources and recipes in packages repo, not core. Any window manager
is a package. No claim of complete X compatibility or native performance parity.

## Runtime packages

/Users/jku/git/wasm-posix-packages main fdf59f9 pushed. 7957e76 holds jsmn recipes;
f7152b8 pinned manifest; 89938e8 catalog; fdf59f9 public README. Public catalog
https://raw.githubusercontent.com/GroundUpCoder/wasm-posix-packages/main/catalog.json.
Manifest uses immutable source URLs/hash/size; jsmn source-only currently. Use guest
export to publish a small pinned binary object/manifest and update catalog in
separate commits; validate real browser install from fresh disk and reload.

tools/guest-repository/manager.sh handles repo add/remove/list, search/info,
build/install/upgrade/recover/installed/pack. lock.c uses fcntl across exec to avoid
stale PID locks. Config /etc/gucman/sources.d, state /usr/local/lib/gucman; per-package
journal pending with checksum payload, atomic current switch and explicit recovery.
URL identity is trust identity; closure preflight catches ancestor collisions.
Recipes trusted, not sandboxed. Fetch HTTPS except loopback, credentials omitted,
final destination checked (no claim all browser-hidden redirect hops verified).
Binary requires exact runtime. pack exports installed declared files, no upload.
tests/runtime-packages.mjs kills real gucman at publication, asserts old/pending/dead
owner then recovery new version/journal cleared. Do not weaken it to command-exit only.

## Independent reviews

Read local netguc instructions and skills/netguc-app-creation.md in full before
hosting edits/integration. It requires independent design/implementation review.
Use cc-meta shell for all external coordination; no internal substitute reviewers.
Existing real Codex Astra threads (model verified earlier, verify on reuse):
- Design 01a123d5-f441-7cc3-829a-cb511f15f8c7: approved earlier package/hosting
  design 4057d9fc115dbcb7bd2314c9365b0295dcbf7461792c911be5fd363892d76244;
  new SDL VT direction should be included in updated review scope.
- Implementation 01a123e0-2fb3-7521-9fa3-e936e0997405: idle, changes required on
  core96ac37a and hosting3f875fce. Reuse this sole reviewer where practical.
  Core findings corrected in9a7f230: ancestor collisions, recovery lock, final URL,
  URL aliases, meaningful recovery assertions. Needs actual re-review/test evidence.
  Last response reviewed hosting only statically, no independent executed tests.

## Hosting worktree: unfinished author corrections

/Users/jku/git/netguc-os-core detached based d5e0d8ff7e0fc04b059b31f0f17bf45cbf46f405.
Commit3f875fce5e74953ff932fa277faf58474c3d9485 contains proposed Caddyfile,
os/core-release.py, test-core-release.py, test-routing.py, CLAUDE and journal.
UNCOMMITTED os/core-release.py rewrite corrects reviewer findings; not yet tested.
Do not reset it. Original /Users/jku/git/netguc has unrelated modified
skills/HERMES-AGENT.md and research files; preserve them during integration.

Reviewer six findings: staging dir0700 inaccessible Caddy; route restore outside
journal; interrupted next link blocks rollback; no rollback after complete;
served shared assets not verified on activate; missing payload/directory fsync.
Rewrite adds put() fsync + chmod, serving closure validation, begin phase recording
old/new route bytes/hashes BEFORE mutation, activate, rollback retaining journal,
finish-rollback after verified Caddy restore, last-release rollback after complete,
next reconciliation and inherited --lock-fd validation. Tests and docs STILL
describe initial API and need adaptation; inspect code carefully for further bugs.
Check reused release/asset permissions too, not only fresh staging.

Need deployment wrapper holding shared operator.lock across begin, activate,
Caddy validate/reload, public acceptance, complete; error path rollback,
Caddy restore/reload/verify, finish-rollback. No packages checkout input.
Validate actual staged payload readable by serving user, assets missing/tampered,
first/subsequent release rollback, interrupted next, route restoration, failure
journal persistence and fsync ordering. Production artifacts require exact review.

tools/export-release.mjs exports immutable core-only dist, release.json inventory
with hashes; core assets under /assets/HASH.ext. New Caddy root serves core/current
index; shared immutable assets core/assets. /os2 blocks unchanged byte-for-byte.
Existing production root redirects /os2; old current read-only verified as
releases/2ec2182f8a2836bb4bc42d7ad32eb1415e8c86c4056ee825e6d4e5eb31c79053.
Store /home/ubuntu/.gucchat/os; new core subdir, existing operator.lock.
Historical pending.json blocks deployment. No production mutation yet.

Author actual Caddy isolated temp route test on ssh netguc passed job111648e1,
log /tmp/netguc-os-core-routing.log, but fixture wasn't real stage/serving-user and
didn't exercise review failures. New Python helper rewrite still untested.
No local Caddy installed. Existing static hosting exception: do not use generic
deploy/deploy.sh os; follow os/CLAUDE and reviewed release procedure.

## Completion

Finish implementation and meaningful tests; exact independent review; integrate
and push core/packages/netguc without unrelated changes; deploy core to root host
with rollback evidence; verify public headers/assets, actual fresh browser boot,
terminal and GPU VTs, GitHub source/binary operations and reload, old /os2 and disks.
Document exact revisions, runtime/release identities, executed tests, live result
and any remaining explicit limits. No further user permission is needed for the
already requested deployment, but obey genuine material-choice/approval gates.
