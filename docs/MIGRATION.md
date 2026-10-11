# gucos repository and hosting migration

Core source baseline: wasm-posix 8311e115d80665a409f64dc0de52404dde5b0fed,
the accepted platform 1.0.1 implementation. Git history and release tags are
retained. The private operational checkpoint commit add5211 was not imported
into public main; its local backups remain preserved.

The branding, default package repository and deployment host change. The
wasm32-wasm-posix platform target, ABI epochs, legacy-runtime compatibility and
wasm-posix-core-v1 browser disk identities remain stable. The branding source
change gives this build a new runtime digest, not a new ABI.

Unfinished work is preserved as `checkpoint/packaging-evolution` (a2544c2) and
`checkpoint/packaging-generations` (2f14f7d), not merged into the production
release. Those branches retain their historical names/URLs until resumed and
rebased onto gucos main. They are not accepted releases. Their existing original
worktrees and ignored test artifacts remain intact. Porting workers remain paused.

The standalone scheduler files were copied byte-for-byte from wasm-posix-old
0b9b3245fe11a3bc677d81113cba90cd0ae80446, tools/machine-job.mjs,
tools/job-scheduler.mjs and tools/process-tree.mjs. The shared queue directory
and lane identity intentionally retain their old names to prevent independent
schedulers from running competing browser jobs.

Historical logs and provenance retain their original repository paths, URLs and
checksums. Active runtime and publication paths use the new repositories. Exact
migration validation and deployment receipts are recorded under docs/logs/.
