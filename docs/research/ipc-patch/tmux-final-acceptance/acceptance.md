# Final1.0.2 package-client and tmux lifecycle acceptance

PASS: actual fresh-profile gucman install, exact installed-byte validation and
interactive lifecycle on explicitly supplied final core. No build/repack,
production catalog edit, deployment, publication or package-branch import.

## Candidate and execution

Core /home/ubuntu/git/gucos-core-snapshots/ipc-1.0.2-be3a9ce7.
Runtime be3a9ce7817c876e8d9bc900556fea1fbaeb4f0d5d4e03003ab24a7ad3b600bb.
Platform1.0.2, runtime0.5.9, compiler0.5.13, runtime ABI5/object ABI3.
Supplied build.json SHA2569c79b607023e413a42158b56c9e1a24a9f70c8e2bc106410c0098ec8d535ebda,
index.html SHA256b6c9ded8be3d6b242d266c45190add39328e9d0a319efcba048daa61ad1bae71,
SHA256SUMS SHA256fb28d465482889f9308af15d600610aceef4dbe40f88e7ab6d3a13f84a7fef3c
and every listed payload were verified before staging. Original supplied core
metadata and sums are retained in final-install01; this run's dist-files.json
pins the copied core and private fixture bytes.

SSH agentone machine-job browser reservation a4001b8a-1c3f-499e-a8b1-90e455c687d5
started09:50:43 UTC with5400-second deadline. Client acceptance finished10:34:11;
lifecycle finished10:34:27. The job released its reservation normally.
Previous1200-second attempt expired at ncurses1175/2928; its failure is retained
in final-install01. This successful retry used a completely fresh profile.

## Actual package-client checks

The private localhost test catalog included only libevent/ncurses/tmux. All
catalog manifest hashes and binary hashes were pinned. Real gucman recursively
resolved and validated the dependency graph, platform eligibility, output
ownership and licenses, then installed all packages. Installed manifests exactly
matched frozen fixture manifests; independent sha256sum checks matched all2963
outputs (libevent31, ncurses2928, tmux4). gucman installed reported versions:
libevent2.1.12-1, ncurses6.5.0-2, tmux3.5a-1. tmux-V returned tmux3.5a.

Metadata7b1d205/7cf0b1b requires platform1.0.2, posix-core/pthreads/unix-stream/
unix-rights/flock/closefrom, unchanged target/ABI fields, exact tested dependency
versions. The actual guc-json compatible checker rejected version1.0.1 metadata
with binary platform version mismatch. This is a direct real checker invocation
on the final runtime, not a claim that an old core was booted.

## Unchanged artifacts and provenance

- libevent archive295f06ac0d3e6e02774eb3f60ba68a136b7f93b00b665e37555a2385e33ee594
- ncurses archive4771ab00e7086c2be9815c4be1181551ec4ed455489b0ff4256effaad4570617
- tmux archive08efddf8191d41b41299ce8df494c981bb1e0bf216152dfccdc618f0190bd879

Dependencies retain their original19609f47 build identity; tmux retains its
e1a1d35a build identity. Nothing was retagged as compiled on finalbe3a9ce7.
Original receipts and frozen manifests are retained alongside this report.
Payloads were neither recompiled nor repacked. Earlier clean recipe evidence
and upstream tmux3.5a provenance remain in integrated11.

## Interactive result and limits

Actual attach, terminal input and Backspace erase, two panes (%0/PID6, %1/PID8),
detach with continued tick growth (5 to15 bytes), resize110x34 terminal/110x33
window, reconnect to unchanged shell PIDs, other-pane input, one-pane exit with
surviving usable shell, and final shell/session exit all passed. Reconnect means
reattach within the running browser runtime, not running-process persistence
across a browser reload. The same port limitations documented in integrated11
remain; no broad POSIX or all-tmux-feature conformance claim.

No production package catalog was touched. Independent review and parent-selected
isolated destination branch govern integration; parent owns overall release.
