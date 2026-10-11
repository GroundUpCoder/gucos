# Live startup profiling and package expansion

User requested restarting the existing external package author and profiling
startup. Sent cc-meta message 01a12456-a0f3-76cd-b7a5-57aa1d1428a0 to thread
01a12401-30cf-7004-a932-af576a939e08; confirmed running with Codex gpt-6-astra.
Instruction restricts all package work to wasm-posix-packages, prioritizes JWM,
requires real guest acceptance, and forbids modifying core/hosting for blockers.

This thread executed live Chromium profiling of normal interactive startup, with
three fresh/reload pairs plus three no-CPU-trace pairs. Baseline median prompt
latency 2.828 seconds fresh and 0.332 seconds reload. Bootstrap network serialization
and uncompressed binary transfer dominate; persistent writes are secondary.
Exact evidence, caveats, reproducible probe and proposals are preserved under
docs/research/startup-profile/. No optimization or deployment performed.
