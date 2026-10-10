# Standalone OS boundaries

- This checkout owns the browser runtime, compiler/libc, fixed bootstrap, generic
  package client and core-only build/release tools. Do not discover or read sibling
  wasm-posix-packages during build or deployment.
- The packages repository owns application recipes, sources, patches and catalog.
  Only explicit browser package operations consume those remote inputs.
- Preserve wasm-posix-old and its historical worktrees as references. Retired
  implementation threads are not owners of this fresh repository.
- Documentation belongs under docs/. Record exact provenance and tested scope.
- Use machine-job for builds/browser tests. One browser job at a time.
- Netguc hosting changes follow its local instructions and independent review.
- Never silently migrate historical browser disks. New core uses wasm-posix-core-v1,
  its .journal and wasm-posix.core.files. Preserve os2, minimal and legacy namespaces.
