Independent bounded tmux package review — 2026-10-11

Outcome: two package-metadata blockers before package-manager installation/publication; no additional blocker found in the bounded source adaptations for the tested e1a1d35a runtime. These findings do not invalidate the worker's direct-build lifecycle result.

Scope and provenance

Reviewer thread 01a129d1-a91d-7e5c-87c9-bb762e741018 metadata verified executorType=codex, model=gpt-6-astra. Read-only review of explicitly authorized separate repository /home/ubuntu/git/gucos-packages-ipc on agentone. Recipe commit 206a49be1678b4234f1c450a31492f0fad51e5d4; evidence commit b8178c063ad86d915c2f0bb957571e2c5b2dd605. Between those commits apps/tmux changes only README and provenance, not recipe/source/build/test inputs. Untracked package-lock.json was observed and excluded.

Committed full tree captured with git archive in agentone /tmp/tmux-package-review-sbua0z0l/source.tar, SHA256 68b8c0ff8a606d91ccd0a942475057c8e01f17d4afddef1d6e4c6e2719d059ac. Remote SHA256SUMS, metadata.json, recipe-evidence.patch and recipe-history.txt preserve scope. Remote capture files are mode 0444, not an OS immutability guarantee. Local same-named temp directory contains scoped archives and extracted review files; LOCAL-SHA256SUMS records them. No author edits, tests, builds, deployment or publication performed.

Selected exact hashes:
- apps/tmux/source.tar: b6a4fe42a006fe7366eab9fa681eb868dee3dddaf13433f66d6577027276663c
- apps/tmux/build.sh: 92027977c1a2e2c750de3696eb9e3866de5abf3f9ca9dc9b7a671f2f3e386c3b
- apps/tmux/recipe.json: b15ac58b4bd54208ecd1667407f342eb31b75deb6b103b9c565ceddf1a0c9030
- integrated-11/acceptance.md: ee8559b0df0012201237c2d4f1b68bc8f66aacd2cafc065e049e71490a088877
- exported archive: 08efddf8191d41b41299ce8df494c981bb1e0bf216152dfccdc618f0190bd879
- exported bin/tmux: c945ee31ce59c9ad662a5215e7b2fa6d43e2880415b21d6ba56c57a535c2e0c1
All 12 entries in integrated-11/evidence-sha256.json and all three receipt frozen-input hashes/sizes match captured committed bytes. Upstream release URL/hash is recorded in provenance; I did not independently fetch/revalidate the original upstream compressed release.

Findings (source conclusions, not executed failures)

1. P2 — Missing dependency compatibility requirements reject normal package installation.
apps/tmux/recipe.json:4-8 declares libevent and ncurses but leaves dependencyRequirements empty. The package manifest preparation path copies recipe fields unchanged (tests/migration/prepare.py:41-46). The already-captured core package client calls dependency validation for each dependency (tools/guest-repository/manager.sh:203), and json.c:108-109 rejects a platform-bearing manifest without an entry with "missing dependency compatibility requirement". Therefore a manifest generated unchanged from this recipe fails dependency validation. Pin the intended dependency versions or other supported constraints before making an installable package. Static linkage does not remove the declared dependency validation or ncurses terminfo needs. Reference client bytes are preserved under core-reference/, copied from the earlier stable closefrom snapshot /tmp/gucos-closefrom-review-s90chynf/source, not asserted to be a fresh capture of the newer dist.

2. P2 — Minimum platform version admits cores missing required APIs.
apps/tmux/recipe.json:11-16 advertises minVersion 1.0.0, ABI 5/object ABI 3 and only posix-core/pthreads, despite build.sh:66-70 deliberately requiring newly available closefrom. The exported binary's wasmposix.dylink engineImports includes closefrom and needed is ["libc-fork.so.1"]. The package client's platform check compares version, ABI and declared features (reference json.c:90-103); unchanged ABI does not distinguish old cores missing these APIs. If this metadata is used for publication, it admits unsupported old runtimes: source builds hit the closefrom gate, and binary loading cannot resolve required APIs. Set the minimum to the first released core that contains the complete required IPC/terminal/closefrom support (the proposed 1.0.2 only once accepted), or use an enforced capability requirement. Do not widen acceptance to untested runtime hashes merely by changing this field.

Why lifecycle PASS does not cover these findings: docs/research/tmux-ipc/integrated-compile.mjs:16-25 directly extracts dependency archives into /usr/local and invokes build.sh/test.sh. It does not perform a package-manager install or exercise the manifest constraints. Acceptance accurately claims build/lifecycle, not package installation.

Bounded source/artifact assessment

- Forkpty: build.sh:9 copies upstream compat/forkpty-haiku.c. Its open/grantpt/unlockpt/ptsname/slave open, fork, setsid/TIOCSCTTY, winsize setup and dup2 path is appropriate for the available interfaces. Parent closes slave; failure label closes opened descriptors. Both actual fdforkpty callers (source/spawn.c:350-369 and source/job.c:112-116 inside source.tar) supply a non-null winsize; the upstream unconditional winsize ioctl is not a null-pointer bug on these callers. This is not approval as a general-purpose public forkpty implementation under all failure conditions.
- Closefrom: configure performs a link probe; build.sh:68 checks HAVE_CLOSEFROM and rejects the bounded fallback. Captured Makefile has HAVE_CLOSEFROM=1 and excludes closefrom.o from LIBOBJS. No invented finite descriptor limit.
- Imsg: build.sh:55-60 removes exactly the quota-preflight block at source/compat/imsg.c:80-86, retaining calloc, recvmsg errors, EINTR retry, extra-fd close and cleanup. Bundled getdtablecount/getdtablesize files still exist/compile but have no remaining active callers once closefrom fallback is excluded and the preflight is removed. Unlimited-model adaptation must be revisited if core enforces quotas.
- Fork/static linkage: build.sh:62-63 supplies --enable-fork and -Wl,-Bstatic through compile/link. Captured link command agrees. Static inspection of exported wasmposix.dylink metadata confirms execution.clone=true, sharedLibc=true and needed=["libc-fork.so.1"]. This supports archive-linked dependency code with dynamic fork libc; it does not make old nonfork DSOs fork-compatible or imply dependency source rebuilds.
- Terminal/error adaptations: build.sh:48 removes absent flags only from clear masks at source/tty.c:325-327. Line51 removes only the two VERASE disabled-sentinel conditions at source/tty-keys.c:865,980. Literal VERASE and lack of disabled-sentinel support are documented. EBADMSG-to-EINVAL preserves negative error returns, without pretending to provide a distinct errno. Base64 rewrites retain the algorithm and match the pinned K&R declarations. Source and compatibility notices are preserved.

Execution evidence and limits

The external author thread 01a129b8-ba13-7d36-b0bb-066591a700b1 records clean full tmux build and actual interactive lifecycle PASS on runtime e1a1d35aab446d25eb19126cec01e17e5508b672bc16f73b4cbb5e0a01f3b8a8, build/lifecycle reservation 9beb82a4-86df-4a22-9804-473b2b5619fa, and export reservation fa32ca46-e7e2-49e5-b228-8bfa2db041ba. I inspected the logs, result JSON and drivers, not a live rerun. Driver assertions cover two panes, real keyboard input/erase, detach with continuing tick output, resize, reattach with same pane PIDs, surviving other pane and final session absence. Retained dependency archives are honestly attributed to runtime19609f47; consumers were recompiled, dependencies were not source-rebuilt. Reconnect means same live runtime, not process persistence across reload. No general all-feature/POSIX conformance or proposed metadata-only 1.0.2 candidate acceptance is implied. Parent owns integration and subsequent runtime validation.
