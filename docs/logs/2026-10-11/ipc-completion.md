# IPC 1.0.2 implementation and acceptance complete

The authorized IPC subset and tmux flagship port are implemented and accepted.
Core changes were fast-forwarded into local main at 841937e. Package changes
are isolated at b190dbf1a90c3bff6d4f1a58e73ddbf9a1d2ee84 on ipc/tmux-1.0.2,
worktree /Users/jku/git/gucos-packages-ipc-final, based on clean package main
b0796d4. All 153 imported paths and executable modes match remote package
902362be55080a8208fcd4b0642c78f2f8d47d5c. Parallel porting work is untouched.

Final core runtime:
be3a9ce7817c876e8d9bc900556fea1fbaeb4f0d5d4e03003ab24a7ad3b600bb.
Frozen distribution on agentone:
/home/ubuntu/git/gucos-core-snapshots/ipc-1.0.2-be3a9ce7.
Platform 1.0.2, runtime 0.5.9, compiler 0.5.13; runtime/object ABIs remain 5/3.

All builds and tests ran on agentone under machine-job. The external author
threads executed the guest tests; independent reviewer thread
01a129d1-a91d-7e5c-87c9-bb762e741018 reviewed source, logs and hashes without
rerunning tests. Local work was editing, Git integration, and evidence/hash
inspection only. All thread/model provenance is retained with the reports.

Final acceptance passed metadata/build, focused kernel tests, six guest core
checks, retained executable/DSO execution and object/archive relinking, published
jsmn installation/reload, and tmux package-client installation plus full tested
interactive lifecycle. Tmux install verified 2,963 output hashes and exact
dependency manifests. The actual compatibility checker rejected supplied
1.0.1 metadata; this did not boot an older runtime. Tmux reconnect preserved
pane PIDs within one running browser runtime, not across a browser reload.

Independent source review findings and package metadata findings are closed.
The final evidence audit found no actionable blocker and verified that the
install driver actually invoked gucman rather than directly extracting files.
Reports, historical failures, runtime identities and original artifact receipts
remain under docs/research/ipc-patch and the scoped package research tree.

Known limits remain explicit: no general shared mmap/full IPC promise; named
semaphore persistence is kernel-lifetime; SCM_RIGHTS requires broker-owned
descriptors; cross-worker sealed-descriptor sharing is unsupported; the tmux
port treats VERASE literally without disabled-control-character semantics.
The package requires 1.0.2 and the new capabilities. Existing package acceptance
is representative evidence, not proof of every published package combination.

Nothing was pushed, published, deployed, or added to a production catalog.
The implementation and validated artifacts are ready for a separate release
operation. Package main is unchanged; the scoped branch is ready to integrate.
