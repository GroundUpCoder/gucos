Bounded tmux metadata re-review — 2026-10-11

Result: both previous metadata blockers are source-closed. No remaining defect identified within this two-finding scope. No tests, source edits, builds, installation or publication performed.

Exact package commit: 7cf0b1bf00899daf05ca14a20e3f943337bae890, including 7b1d205387b53bda53f6197577903a209228f122, repository agentone:/home/ubuntu/git/gucos-packages-ipc. Immutable committed file/diff capture: /tmp/tmux-metadata-rereview-2ncwkpqc on agentone, also copied locally. Captured files mode0444; this is not an OS immutability guarantee. Reviewer thread metadata again confirms executorType codex and model gpt-6-astra.

1. Dependency requirements closed: apps/tmux/recipe.json:8-15 now supplies exact version requirements libevent2.1.12-1 and ncurses6.5.0-2, matching those dependencies' recipe versions at the same commit. Actual client reference tools/guest-repository/json.c:108-115 requires a nonempty supported constraint and handles version with strcmp, not semver parsing. Therefore the revision suffixes are accepted by this code. minVersion/maxExclusive use compare_version and the strict X.Y.Z parser at lines82-89, which would reject these suffixes; choosing exact equality is correct. sha256 is optional and constrains the dependency manifest hash: json.c:107,112 and manager.sh:201-203 explicitly hash depmanifest and pass it as argv[5]. No exported archive hash belongs in that field, and its absence is not a defect when exact version is provided.

2. Platform eligibility closed: apps/tmux/recipe.json:16-28 now requires minVersion1.0.2 and posix-core,pthreads,unix-stream,unix-rights,flock,closefrom, retaining target wasm32-wasm-posix, runtimeAbi5,objectAbi3. Actual client json.c:95-103 checks the target, numeric version floor/major, ABI values and each feature against comma-delimited runtime features. This excludes pre1.0.2 binary installations and advertised platforms lacking required IPC capabilities. manager.sh:199 applies this compatibility gate to install; source-build mode skips it but still has the existing recipe closefrom configure gate. No broader runtime-compatibility conclusion is inferred.

Client reference provenance: read the actual C/shell implementation from the earlier stable closefrom capture /tmp/gucos-closefrom-review-s90chynf/source/tools/guest-repository, retained in this review under client-reference/ with hashes. This is the same reference used for the original findings; it is not asserted to be a newly decoded client from final be3a9ce7 dist. No simulated parser or locally executed validation substituted for source reading.

Evidence boundary: user reports final core be3a9ce7 passed six guest, retained ABI and published jsmn gates. This review did not execute or independently re-review those gates. Actual gucman installation and lifecycle on the final1.0.2 candidate remain pending and are not implied by source closure. Older e1a1d35a tmux execution evidence retains its original identity.

Hashes:
- source/apps/tmux/recipe.json SHA256: 0195b9d95abddd107e54ebd442d4ec791c2c970f6c672497dd38b068c6b07400
- changes.patch SHA256: bea3c913daafbdb8a7d1c1d56fd8f9b0a97a8e69deeaaca980cecc1f8b3f56f9
- SHA256SUMS preserves all remote captured inputs; LOCAL-SHA256SUMS also covers the actual client references and this report.
