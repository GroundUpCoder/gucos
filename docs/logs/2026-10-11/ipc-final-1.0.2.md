# Final 1.0.2 IPC candidate

All edits/builds/tests were executed through SSH agentone in the isolated
/home/ubuntu/git/gucos-ipc-final checkout, branch ipc-final-1.0.2, based d7d91ac.
No deployment/publication, package/catalog edits, or frozen candidate changes.
Model execution is not claimed remote.

## Metadata and exact provenance

Metadata commit: 68df0b3e2603aa0149555fa98950ab210422a796.
Only tools/versions.json and tools/platform.json differ in runtime inputs from
d7d91ac. Runtime is 0.5.9; platform 1.0.2; compiler stays 0.5.13; ABI stays
objects 3/runtime 5. Added features: unix-rights, unix-dgram, unix-seqpacket,
flock, eventfd, posix-named-semaphores, closefrom. Existing features remain.

Read-only comparison with local parent commit 2057b5709f4736952b87d77bf7993bdd5233d37e
confirmed exact bytes:
- versions SHA256 4937c740f9366f10900712e9d2ae4f65632ec2ac8c658a5b91682fac88b15ce5
- platform SHA256 cf9f87f2beeb2edd7d3183209ffc5fd4b9f97163c1735988d4ab71968e0f5d52

An initial build appended feature names after existing features. The parent
grouped them after unix-stream. Both had identical feature membership; after
observing the byte difference, reordered to the parent's exact bytes and rebuilt.
Initial runtime 9f99c457 is NOT the final candidate. Both build logs are retained.
The snapshot preserves all 65 runtime files in the build output, including
content-addressed assets retained across the two builds; its full manifest is
authoritative.

## Frozen final candidate

/home/ubuntu/git/gucos-core-snapshots/ipc-1.0.2-be3a9ce7

Runtime:
be3a9ce7817c876e8d9bc900556fea1fbaeb4f0d5d4e03003ab24a7ad3b600bb

SHA256:
- build.json: 9c79b607023e413a42158b56c9e1a24a9f70c8e2bc106410c0098ec8d535ebda
- index.html: b6c9ded8be3d6b242d266c45190add39328e9d0a319efcba048daa61ad1bae71
- SHA256SUMS: fb28d465482889f9308af15d600610aceef4dbe40f88e7ab6d3a13f84a7fef3c

Snapshot files/directories were made read-only. Before/after inventories include
the manifest itself and all payload files; all hashes remained unchanged.

## Executed gates

Build-only reservation 68637765-3c62-4ba2-b52c-a251346862d5 passed dependency
bootstrap, platform-metadata and focused socket/service/terminal/closefrom kernel
tests, including failed-fork named-semaphore cleanup. These run actual methods
with test process records; they are not guest execution.
Final build reservation 5d3ae2e3-8c3c-44de-a8d7-0c0f8538e825 passed platform-metadata
again and built the exact final metadata.

One browser reservation 4ccb5c98-a7e6-405e-bcb4-af67b6f71e9b passed sequentially:
1. IPC sockets, including transferred terminal.
2. IPC services: flock, eventfd, named semaphores.
3. Terminal regression.
4. Canonical IPC libc symbol lookup.
5. closefrom focused guest and canonical symbol lookup.
6. platform-posix.
7. Retained-artifact ABI: unchanged old executable/DSO, archive and object relinks,
   hash preservation, reload and unchanged dynamic consumer.
8. Published groundupcoder:jsmn 1.1.0-1 binary install, execution and reload.

The prior handwritten ABI/jsmn runners were adapted only for explicit checkout,
dist and expected runtime. Regression wrappers force the frozen directory and
verify both build.json runtime and shell toolchain identity. No GPU/source-package
build or broad upstream repetition. Guest compilation was limited to requested
fixtures and ABI consumer relinks.

Published catalog response SHA256:
2e985f731fddd7a03abb48c7b57d4ef94ce998f2e28c8b50a2a4f51a95a980c7.
Manifest SHA256:
aa46ef646bbf3907c5e2fc2091f87d32926b43c3c32cdc6c3c6c2f8c89dda79e.
Archive SHA256:
62a4b0e98744881284d52d278abb06ff2feec6794baa067a3cfa047638a3d4af.
Installed and reloaded jsmn-example SHA256:
a9c7a4a06264b1f10e7aae2d7cd6dac2bdaa9a11d8eb2364d78cfa2bfcf2197d.
Actual URLs, response bytes and identities are preserved; mutable catalog main
is not represented as an inferred commit pin.

## Scope limits

Retained-artifact testing is not old-disk migration. Published binary acceptance
covers jsmn only. No graphics acceptance is claimed. The fd epoch does not
invalidate sibling worker-local sealed descriptor tables; shipped browser
workers do not enable that local sealed fast path.

The coordinator reported tmux lifecycle PASS on prior e1a1d35a. This thread did
not run tmux; final-candidate artifact lifecycle retest belongs to package worker.
No current statement claims that retest completed.

Exact evidence is committed under docs/research/ipc-patch/final-1.0.2 and remains
at /tmp/gucos-ipc-final-acceptance-0vjl204x on agentone.
