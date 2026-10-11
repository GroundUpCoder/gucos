# Standalone OS boundaries

- This checkout owns the browser runtime, compiler/libc, fixed bootstrap, generic
  package client and core-only build/release tools. Do not discover or read sibling
  gucos-packages during build or deployment.
- The packages repository owns application recipes, sources, patches and catalog.
  Only explicit browser package operations consume those remote inputs.
- The user authorized removal of all local wasm-posix* checkouts on 2026-10-11.
  Local Git history and source changes were preserved in
  /Users/jku/git/.gucos-retired-backup-2026-10-11 before removal. Retired
  implementation threads are not owners of this fresh repository.
- Documentation belongs under docs/. Record exact provenance and tested scope.
- Before core deployment, read docs/DEPLOYMENT.md for the existing Pages project,
  credential location, preview/production validation and rollback procedure.
- Use machine-job for builds/browser tests. One browser job at a time.
- Start all new threads and tailcalls with the default account (no account suffix),
  not pub or another suffixed account. Existing threads may remain on their
  current accounts.
- Netguc hosting changes follow its local instructions and independent review.
- Target graphics boundary: SDL is the only application-facing graphics API in
  core. Each graphical VT owns one presentation surface; terminal VTs remain
  available. X servers, window managers and graphical toolkits belong in runtime
  packages. Remove inherited kernel desktop/window policy rather than extending
  it. This is the target architecture, not a claim that migration is complete.
- GPU applications run directly in their own VTs and own their GPU devices.
  Retain the SDL3-compatible shim, WebGPU bindings and SDL/WebGPU surface bridge
  as the starting implementation. Do not add X-specific GPU sharing, pixmap
  interoperability or a shared GPU broker for hypothetical accelerated X clients.
  X11 initially uses software rendering and presentation as a userland package;
  any later compositing work must fit the generic core interface.
- Never silently migrate historical browser disks. New core uses wasm-posix-core-v1,
  its .journal and wasm-posix.core.files. Preserve os2, minimal and legacy namespaces.
