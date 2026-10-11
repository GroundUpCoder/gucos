# IPC review terminal fixes

Integration base: socket implementation ee68a971 and evidence ef5a450, remote
agentone checkout /home/ubuntu/git/gucos-ipc-integration. Independent reviewer
01a129d1-a91d-7e5c-87c9-bb762e741018 identified inherited terminal behavior exposed
by descriptor transfer. Parent implements these fixes separately from authors.

- Immediate and parked terminal reads apply SIGTTIN only when pcb.tty is the
  descriptor terminal. A detached server may read a received client terminal.
- Add internal TIOCGWINSZ RPC 0x0108; preserve existing public import signature.
  RemoteFS resolves the actual descriptor, including PTY masters. Invalid fds
  fail EBADF, nonterminal descriptors ENOTTY. Standalone host fallback unchanged.
- Add focused kernel regression and guest transfer/winsize regression.

Executed on agentone through machine-job build lane: kernel test passed under
reservation d1f087c4-82d7-44e5-9912-c73dfbe0166f. This uses actual kernel methods
with a simulated process record, not guest execution. First test attempt failed
because the harness omitted required createWorker; setup corrected and rerun.
Guest test and combined browser acceptance pending. No claim of tmux execution.

Other independent findings (libc dynamic exports and failed fork-child semaphore
cleanup) remain to be resolved with the service implementation before release.

## Integrated browser evidence

Runtime `baae3b7591a51a357d4fba3fc8e96135790025d61a6c98453e28100ea99f1aad` passed guest socket/PTY and service fixtures in reservation `c5532f48-48dd-4751-b47a-f82c88b30bd5`. The terminal fixture then failed its marker assertion. Its cleanup closed a PTY whose foreground group it had assigned to itself, triggering SIGHUP before the final marker. The test now ignores SIGHUP only during cleanup. Reservation `309f9b70-2071-4edd-9a2c-9d24e8d57e4b` reran the guest terminal fixture successfully (`IPC-TERMINAL-PASS`) on the unchanged runtime. Both output logs are preserved under docs/research/ipc-patch. This validates transferred terminal size and detached-child input for the fixture; tmux lifecycle remains pending.
