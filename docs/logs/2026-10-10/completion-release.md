# Standalone core release completion

User continuation explicitly authorized implementation, independent review,
integration, pushes and deployment to os.groundupcoder.net. No DNS, credential,
old-repository or historical-disk migration was required or performed.

## Reviewed identities

- Core implementation: 9521dc7496b6fea5b6d52e4a4603495c227a0f57.
- Runtime: 2a9cca447a35af0d0d8894ea4b50cb254ce20821753af8ae70a1ad5060da2a0f.
- Release manifest: b93d001b36a4624c1f093da6021f989dbf63e7ce05923d5f8ba6b26d9e49f7c5.
- Initial hosting detached: 4e8fa55d70773796d8ede8e6ff950c0e9396e9d7.
- Hosting composed main: 0f8497d32861698efa5643cd9969a1a9c511e5e1;
  tree 8ec8ce4819090314d1283de8781e210c2fbcda7a equals detached tree.
- Route SHA256: e18b20eed8181db0b99e9365bc4a37b6665f4dbe60a7573305ccd71607f90ba1.

Author thread 01a12401-30cf-7004-a932-af576a939e08 and external independent
reviewers 01a123d5-f441-7cc3-829a-cb511f15f8c7 (design) and
01a123e0-2fb3-7521-9fa3-e936e0997405 (implementation) are actual cc-meta
threads; metadata verified Codex gpt-6-astra. No internal sub-agents used.
Design review approved document SHA256
2581a1789dc9c85fda84cf3870c3c38a62004ad0ce20645f183ed88f4df08f1d.
Implementation reviewer independently ran host regression and hosting Python
gates, verified composed-tree equivalence and reviewed the exact release's 33
files, hashes, byte identity with dist and runtime identity against source.
Browser evidence below was executed by the author, not the reviewer.

## Implementation and acceptance trace

| Requirement | Implementation | Executed author evidence |
| --- | --- | --- |
| SDL graphical VTs, terminal retained, no kernel WM | cb13bec, b1e4b16; kernel/system/host/UI | virtual-terminals browser: two apps, switch, resize, held input release, crash/exit, bounded hidden cadence, no wm.sock |
| Process-owned direct GPU | host WebGPU + SDL bridge | two actual GPU apps, resize and one-process crash; host mock loss and stale-handle regression |
| Runtime package discovery/build/install | generic manager; scalable closure helper | runtime-packages recovery/outage/path conflicts; public GitHub source build/export/reload |
| Historical disks preserved | distinct core namespace | storage-boundary browser gate |
| Software X11 and userland WM | packages X11/toolkit/twm recipes | guest builds; independent Xlib clients; twm decoration, browser drag, hidden client progress |
| Guarded core-only hosting | netguc os/core-release.py and deploy-core.py | helper and deployment fault injections; actual Caddy serving-user route test |

Focused core browser suite passed under job
4c82be76-8938-4960-9b0d-a611a25de7f9, log
/tmp/wasm-posix-review-browser.log. Earlier compiler/Make/archive/persistence
smoke passed in /tmp/wasm-posix-vt-smoke.log. Full twm guest fixture acceptance
passed in /tmp/wasm-posix-x11-twm.log on the release runtime.

## Release status

Core and hosting approved tips pushed; server hosting checkout fast-forwarded to
0f8497d3. Release copied to private /tmp and manifest hash verified there.
The first activation attempt failed on public probe HTTP 403 from Cloudflare.
Automatic rollback restored files, then retained its journal on failed public
header verification. Under operator.lock, an explicit identified probe with
Accept: */* verified original 307 root redirect and exact /os2/ bytes/headers at
origin and public URL; Caddy was validated/reloaded and finish-rollback completed.
Evidence: /tmp/wasm-posix-production-deploy.log and
/tmp/wasm-posix-rollback-reconciled.log. No Cloudflare configuration changed.
Detached correction 70480e04069ed06f67c3ea1826bc5f8fecc30129 added explicit probe
headers and rejects invalid preflight status/headers before release mutation.
Reviewer independently approved it and composed main
eb5f19fa8dcc179973e4c47bbda8a985b063f4a1 (tree
c02e924ce75b225cca41424224a88ca48ef58320; exact detached equivalence).
Both Python gates passed independently and by the author. Approved main was
pushed and fast-forwarded on the server before retry.

Packages publication tip d68be1f751090a2fdaaff3f9040ac76cbb3d188a was independently
approved and pushed. Fresh public pinned binary install/reload passed job
91b48d6e-508a-4e8e-93f9-a47a200ce02a (/tmp/wasm-posix-public-binary.log).
Corrected public X11/toolkit/twm guest build and interaction acceptance PASSED,
job be2d7afa-6585-4420-8847-7e62b98e21a5 (/tmp/wasm-posix-public-x11.log).
It consumed the public catalog and pinned raw GitHub inputs, ignored a poison
object planted at the historical scratch path, and passed decoration, pointer
drag and hidden-client progress tests. This test used local reviewed dist with
real public package inputs.

The second deployment COMPLETED successfully using hosting eb5f19fa and the
unchanged approved release b93d001b. The wrapper held operator.lock across
activation, validated Caddy reload, exact origin/public payload hashes and
isolation headers, unchanged historical /os2/ response identity, and mandatory
live browser acceptance. Browser job 65aafd7c-a5e9-4416-9b76-ff5c673c079a loaded
https://os.groundupcoder.net in fresh profiles and passed:

- Exact runtime identity and crossOriginIsolated.
- Public jsmn binary installation and reload persistence.
- Guest compiler, two independent WebGPU VTs, and terminal recovery.
- Public jsmn source build and reload persistence in a second fresh profile.

Log /tmp/wasm-posix-production-deploy2.log ends with PASS LIVE and action complete.
Post-completion server inspection confirmed core/current points to
releases/b93d001b36a4624c1f093da6021f989dbf63e7ce05923d5f8ba6b26d9e49f7c5,
no core/pending.json, expected route hash, and historical current unchanged.
The completed last-release journal retains rollback to the previous root route
and absent core pointer. Old /os2/ release and browser disk namespaces remain.
Production state log: /tmp/wasm-posix-production-state.log.

Actual /etc/caddy/Caddyfile is a snapshot symlink ending
Caddyfile.bangkok-d5e0d8ff7e0fc04b059b31f0f17bf45cbf46f405; it imports
/home/ubuntu/.gucchat/os/route/*.caddy. Only the OS route is replaced by the
transaction. Historical current before activation is
releases/2ec2182f8a2836bb4bc42d7ad32eb1415e8c86c4056ee825e6d4e5eb31c79053;
old route SHA256 is 795e054d96160b58dfe2ffea59365254678d48b9cd38312c5991c56bfee157a7.
Unrelated local HERMES/research edits and server untracked files were preserved.

## Tested scope and limits

The SDL implementation is the compatible shim and existing WebGPU bridge, not
full upstream SDL. X11 is software rendering with a fixed-size display and no
GLX/DRI/MIT-SHM. The toolkit package supplies twm dependencies, not xterm or all
historical graphical applications. GPU loss rejection was tested with a mock;
actual browsers tested independent devices and process crash, not physical GPU
failure. These tests do not establish full X compatibility or native performance.

Reviewer noted an inherited nonblocking source-offer provenance defect: UTF-8
serialization changes one author-name byte sequence in Make src/vmsify.c:17.
The offered archive matches the reviewed build but that file is not byte-preserved.
