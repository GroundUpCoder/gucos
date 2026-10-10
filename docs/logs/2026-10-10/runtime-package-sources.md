# Runtime package source implementation

User authorized the standalone core, netguc hosting and runtime-only package-source
architecture. Core imported from old minimal prototype; application recipes excluded.
Default source is raw GitHub, configured as persistent guest data. Multi-source
search, qualified selection, guest source builds and binary installs implemented.
First browser smoke and cross-origin package tests passed. Later recovery/storage
checks are still running; do not infer their result from the earlier pass.

Independent design reviewer: cc thread 01a123d5-f441-7cc3-829a-cb511f15f8c7,
verified Codex/gpt-6-astra. Design approved at SHA256
4057d9fc115dbcb7bd2314c9365b0295dcbf7461792c911be5fd363892d76244.
A later wording correction changes the new asset route from /core/assets to
/assets to match relative build URLs; exact implementation review must cover it.

Netguc isolated checkout /Users/jku/git/netguc-os-core at base d5e0d8ff;
unrelated original netguc dirty work preserved. Live inspection confirms old
root redirect /os2 and historical release2ec2182f. No live changes yet.

Packages checkout has source-only jsmn recipe and immutable commit-addressed refs.
Visibility remains private. Decision b0861cd4a7322f4c9ec10a04006c3be75c0208c002d93783e2314b3be865c477
asks the owner about public anonymous delivery; no visibility change executed.

Author follow-up: package origin isolation, ambiguity, URL-label reuse, source hash
refusal, cycles, ancestor path conflict preflight, same-URL aliases, per-package
failure preservation, actual killed publication recovery to version3, outage reload
and old-disk/editor reset preservation passed in browser job e5f29c8a.
Kernel fcntl lock replaces the prototype directory lock, including recovery.
The first fault harness wrongly required a nonzero shell.command result after a
signal; actual pending state showed the process had died. The corrected test
asserts dead owner, prepared journal, unchanged old version, then recovered new
version and cleared journal. Initial reset harness needed shell=bootstrap to avoid
colliding with the interactive foreground shell. Both failures were test harness
assumptions; no earlier pass was claimed for those checks.

GitHub API now reports packages repository public; this thread did not change
visibility. The obsolete visibility decision was cancelled, not answered on the
user's behalf. Anonymous live recipe acceptance is the next test after push.
