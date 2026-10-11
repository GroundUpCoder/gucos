# Tmux flagship acceptance — 2026-10-11

Outcome: clean tmux 3.5a recipe and actual interactive lifecycle PASS. No deploy,
publication or catalog mutation. All builds/tests/browser work ran on SSH
agentone using /home/ubuntu/.local/bin/machine-job. Author thread
01a129b8-ba13-7d36-b0bb-066591a700b1: verified codex / gpt-6-astra.

## Exact inputs

- Upstream tmux 3.5a: https://github.com/tmux/tmux/releases/download/3.5a/tmux-3.5a.tar.gz
- Upstream release SHA256: 16216bd0877170dfcc64157085ba9013610b12b082548c7c9542cc0103198951.
- Reviewed core: /home/ubuntu/git/gucos-core-snapshots/ipc-closefrom-e1a1d35a.
- Runtime: e1a1d35aab446d25eb19126cec01e17e5508b672bc16f73b4cbb5e0a01f3b8a8.
- build.json SHA256: 689b0702f578325dbc52ee4684d555e82ba84469118acf2280ca55655674ae0f.
- index.html SHA256: 5c2bb848d9b3c358b988d18e85e7d9b9498ad12c9264934510e5b23427199903.
- Full candidate payload verification and frozen inputs: integrated11 dist-files.json;
  original supplied SHA256SUMS retained under integrated08.
- Recipe tested: commit 206a49b. Source and exact build.sh hashes are in frozen inputs.
- Dependencies: original libevent 2.1.12 / ncurses 6.5 bootstrap artifacts remain
  identified as built on runtime19609f47c28f9d81c12ad6525d65347057ad5ad2ddec5eb48f5762a922d72441.
  No dependency source rebuild or identity relabeling. Static archive code is
  linked into the fork-enabled consumer; core libc-fork.so.1 stays dynamic.

## Executed acceptance

Reservation 9beb82a4-86df-4a22-9804-473b2b5619fa: fresh browser profile, dependency
consumers recompiled and passed (libevent static/shared, ncurses narrow/wide),
clean tmux source extraction/configure/all objects/link, tmux -V = tmux 3.5a.
Final linker reported 3317 functions / 8548041 bytes. Configure detected real
closefrom and selected tmux-256color. No ABI validator bypass.

Lifecycle finished 09:14:03 UTC: two panes (%0 PID6, %1 PID8), real terminal
attach/input/Backspace erase, detach, continued tick growth (5 to15 bytes),
resize to110x34 terminal /110x33 tmux window, reconnect and input in the other
pane with unchanged shell PIDs, exit one pane while the remaining shell stayed
usable, then exit the final shell and confirm session absence. Reconnect here
means reattaching to the live server in the same browser runtime; browser reload
persistence of running processes was not claimed. No GPU acceptance required.

Reservation fa32ca46-e7e2-49e5-b228-8bfa2db041ba exported installed tmux outputs
from the retained guest disk after acceptance, without rebuilding:
08efddf8191d41b41299ce8df494c981bb1e0bf216152dfccdc618f0190bd879.tar.
Archive, output inventory, source identity and receipt are in artifact-export/.

## Limits and retained failures

Integrated07 missing descriptor cleanup, integrated08 wrong gate filename,
integrated09 missing fork flag, and integrated10 nonfork-DSO/fork-consumer
contract rejection remain retained. The final archive linkage fixes this new
consumer without claiming old nonfork DSOs are fork-compatible.

Disabled terminal-control-character sentinels remain unsupported by this port;
VERASE is treated literally. Absent ONLRET/ECHOCTL/ECHOKE appear only in removed
clear masks. Malformed imsg errors map to EINVAL when EBADMSG is absent. Only the
finite-quota imsg preflight is removed for documented unlimited semantics;
allocation/recvmsg errors and descriptor cleanup remain. No general POSIX or
all-tmux-feature conformance claim. Parent owns overall integration/release.
