# Package progress, platform 1.0.1

Implemented generic terminal progress in gucman and its private fetch helper:
initial response, known/unknown download sizes, 500 ms byte updates, package
counts, file copy/checksum counts and install phases. Progress uses stderr to
preserve captured output contracts. Successful staged checksum chatter is hidden;
failures retain details and cannot activate a corrupt release. ABI epochs remain
unchanged. No package repo, package artifact or hosting code edits by this work.

Tests: controlled incremental download, cache reuse, failure paths and corrupt
pending recovery; generic repository and smoke regressions; fresh JWM with actual
keyboard input and recording; existing 1.0.0 browser disk upgrading to 1.0.1.
Source and precise results/provenance are in docs/research/package-progress-1.0.1/.

Candidate first visible feedback was 135 ms. The X11 archive download took 2.83 s,
while copying 527 files took about 52.1 s and checksum preparation another 11 s.
This establishes that much of the wait is local installation work. No deeper
CPU/filesystem/process-spawn attribution or speedup claim is made.

Guarded deployment completed with live-core and actual live JWM acceptance
passing in job e5ad8c36-e32b-47a9-bb3b-d9f32a958208. Active release is db46e7c8,
runtime 61a87f88; no pending deployment remains. Live first visible feedback
147 ms, total install 130.77 s. Four archives downloaded in 4.47 s aggregate;
file-copy intervals totaled 74.61 s. This is a single recorded observation,
not a speedup benchmark. Retained continuous original video and 8× GIF/MP4.

Tagged implementation 2721a05 as v1.0.1. Updated package documentation to describe
progress and corrected its stale equal-runtime-hash wording to reference the
established platform compatibility contract. Application ABI and package bytes
remain unchanged; existing browser disks receive the manager/private helper.
