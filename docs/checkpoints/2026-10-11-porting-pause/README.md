# Porting pause before gucos migration

The user authorized a checkpoint and pause on 2026-10-11. Repository migration,
new publication and deployment remain on hold. Both implementation workers
acknowledged that only a new explicit user instruction can resume their work.

## Preserved state

Exact local commits, branches and checkpoint refs are in `repositories.json`.
All five worktrees were retained. No tracked implementation edits were pending.
The untracked coordination notice was copied; local node_modules were left intact.

| Worktree | Checkpoint | State |
| --- | --- | --- |
| wasm-posix | 8311e11 | Existing core main, unchanged by porting pause |
| wasm-posix-packages | 123f6d5 | GTK public acceptance and SDL binary/media evidence; MicroPython reserved-path fix |
| wasm-posix-packages-snake | 82ab4db | Desktop launcher and latest JWM menu-coordinate test fix |
| wasm-posix-packaging | a2544c2 | Installer batching and performance tests, not deployed |
| wasm-posix-generations | 2f14f7d | Experimental package-generation selector, not accepted or deployed |

Checkpoint refs under `refs/checkpoints/2026-10-11-porting-pause/` preserve each
tip independently of later branch movement. Full `--all` Git bundles for both
repositories were created and verified under
`/Users/jku/git/.gucos-migration-checkpoints/2026-10-11-porting-pause/`.
These include all worktree branches and checkpoint refs, including unpublished
commits. `backup.json` records bundle and copied evidence sizes and SHA256 hashes.
No commits were pushed by this checkpoint operation.

The existing test-results directories remain in place (approximately 28 GiB in
packages and 2.1 GiB in packages-snake). They include ignored browser profiles and
artifacts and are NOT covered by Git bundles. Do not delete these worktrees or
their ignored files during migration. Relevant `/tmp/wasm-posix*.log` and `.json`
files were copied into the backup's `temporary-evidence/` directory, including
the synthetic installation benchmark and progress/recovery test output.

## Work stopped and remaining gates

- GTK public GUI/install/consumer/reload passed; recorded migration count is 50
  live-accepted additions, separate from the original packages and Snake.
- SDL gfx/image/mixer source and binary/media passes are recorded. Publication
  review and public acceptance remain separate unfinished gates.
- MicroPython's initial run failed with `reserved package state` before compilation.
  Commit 123f6d5 moves its executable outside reserved core state; rerun required.
- Tcl/Tk reservation ececb1b4-91d7-4c20-8fe3-865accdd4392 was terminated during Tcl
  build. Its log records the closed browser. Treat this as interrupted, not a
  completed Tcl/Tk acceptance. Start a fresh bounded run when authorized.
- Desktop attempt 2 failed after JWM startup. Commit 82ab4db changes Snake menu
  selection to live window geometry. Queued attempt 3 reservation
  df3464c5-27c8-4c63-bb17-ad6a7edd5b12 was canceled before running.
- Installer progress tests and the synthetic 529-file benchmark passed; roughly
  66 seconds before and 12 seconds after. This is one workload, not a general
  package performance guarantee. No performance candidate was deployed.
- Generation-selector runtime test reservation
  7b81e31a-00ef-470e-a093-9c705b8f9a0d was canceled while queued. Generation and
  older-core compatibility/recovery acceptance remains unfinished.

## Pause verification

The workers are 01a12557-7667-7b91-b77f-df9b68cb3658 (package migration) and
01a128d4-8971-7330-b53a-8f5b2e638998 (packaging/desktop). A late historical
coordination message restarted the first worker after its initial stop; it was
stopped again and both threads received explicit pause instructions. Their
acknowledgments are retained in `migration-pause.json` and `packaging-pause.json`.
The reviewer checked during shutdown was already idle. No live cc-meta wake
handles remained at inspection. Unrelated project work was not stopped.

`scheduler-before.json` and `scheduler-after.json` record job shutdown; the latter
has no jobs. `running-threads.json` confirms both workers idle at final verification.
No new build or browser acceptance was run by this checkpoint operation.

On resumption, consult these refs and local evidence rather than cloning only
remote main: remote repositories do not contain all of the preserved work.
