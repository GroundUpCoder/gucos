# Authorized retired checkout cleanup

On 2026-10-11 the user explicitly authorized deleting all local wasm-posix*
checkouts, superseding the prior reference-preservation instruction. Inspection
found local-only history and uncommitted acceptance sources, so GitHub archival
was not treated as proof of a complete local backup.

Before deleting /Users/jku/git/wasm-posix* (including symlinks), preserved three
verified full-ref Git bundles, binary tracked diffs, nonignored untracked source
archives, and a path/status/removal inventory in
/Users/jku/git/.gucos-retired-backup-2026-10-11 (about 1.2 GiB).
Generated historical test-results, releases, assets and other retired artifacts
were removed under the user's authorization; this backup does not claim to
retain those ignored generated artifacts. No current gucos checkout, active
porting test profile, browser disk namespace, or remote repository was removed.

Observed free space increased from under 1 GiB to about 60 GiB after cleanup and
creation of the isolated gucode package-release worktree. machine-job continues
to point at /Users/jku/git/gucos/tools/machine-job.mjs.
