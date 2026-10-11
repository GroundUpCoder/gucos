# gucos migration and production acceptance

The user authorized completion of the repository and hosting migration after
checkpointing the existing porting work. Core and packages are now public at
GroundUpCoder/gucos and GroundUpCoder/gucos-packages. The original worktrees and
their ignored evidence remain preserved; paused porting work was not discarded
or declared accepted.

## Exact production identities

- Core baseline: 8311e115d80665a409f64dc0de52404dde5b0fed, platform 1.0.1.
- Reviewed migration implementation: f6e839c (with initial migration1209631).
- Runtime: 19609f47c28f9d81c12ad6525d65347057ad5ad2ddec5eb48f5762a922d72441.
- Core release manifest SHA256: ae142d8388c619a46b21f0bc3e2474e9a9c903c89237df5683012f9894d51da8.
- Packages reviewed implementation: 32443c0ab147252f6000a9192c91043050593561.
- Relocated package manifests: 9474fca; catalog pin commit: 9390bab.
- Preview deployment: f85fd6eb-8ae3-4f56-aaf8-ce7ddbfbf3df.
- Production deployment: f65c4256-a53e-447a-8498-c4e744a7c73d.
- Production URL: https://os.groundupcoder.com/.

The independent reviewer approved exact core/package implementations before
publication and checked final HTTP/browser evidence. All 70 catalog-selected
manifests differ only in repository URL substitution. Source inputs, binary
bytes, versions, output inventories and compatibility requirements are unchanged.

## Executed validation

- Scheduled build produced the runtime above using only the core checkout.
- Serial browser suite passed fresh boot, compiler/Make/archive tools, package
  source/binary operations and recovery, historical disk/reset preservation,
  SDL virtual terminals, and two direct WebGPU applications.
- Preview and production live-core tests each passed fresh-profile boot,
  isolation/runtime identity, public jsmn binary installation, source build,
  persistence/reload, terminal operation and two GPU VTs. Old repository/host
  requests were blocked and asserted absent.
- All 33 public core files were fetched and matched exact byte lengths/SHA256.
  COOP/COEP/CORP and cache policy passed; missing assets and operator manifests
  returned 404. Explicit 404.html disables Pages' implicit SPA fallback.
- Package publication checker verified all 70 packages and 398 unique objects
  (849,528,930 bytes), first from retained Git objects, then anonymously over HTTP.
  Every published object passed its pinned hash and size. This does not assert
  a new source build or interactive execution of every one of the70 packages.
- Package compatibility preparation independently loaded the relocated baseline:
  17 desktop packages, 803 output hashes. Its full historical GUI suite was not
  rerun as part of this URL/hosting migration.

Raw logs and HTTP/Cloudflare receipts are alongside this document; their hashes
are in gucos-migration-checksums.json. A preliminary probe using Python's default
urllib user agent received an edge 403; explicit release-check user agents and
actual Chromium passed. No bot/WAF settings were changed.

## Preserved work and operations

Public checkpoint branches retain packaging-evolution a2544c2,
packaging-generations 2f14f7d, and package snake-packaging 82ab4db. These are
unfinished snapshots, not production releases. The new main branches own future
work. The global machine-job symlink now points to gucos's byte-identical copied
scheduler, preserving the shared queue and one-browser limit.

The Pages project, exact os.groundupcoder.com custom domain and CNAME were created.
Existing apex/domain services and Netguc routing were not changed. The old .net
site remains available for access to origin-bound disks; no disk migration occurs.
The separate josephkimgpt/gucos-packages-old repository is private and archived.

GitHub collaborator access supports pushes but not archiving the original
GroundUpCoder/wasm-posix repositories. Both archive API attempts returned 404;
the owner was asked to complete that final repository-settings action. Archive
status must be verified separately rather than inferred from this release.
