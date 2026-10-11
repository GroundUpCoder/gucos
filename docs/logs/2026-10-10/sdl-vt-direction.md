# SDL VT direction and public package check

The user confirmed wasm-posix-packages is public and selected SDL-only core
graphics with terminal/graphical VTs and package-owned X11/window management.
Recorded the target in AGENTS.md and docs/design/sdl-virtual-terminals.md.
Graphics removal/VT implementation has not been performed in this checkpoint.

Executed the real GitHub browser test under machine-job browser reservation
8375fcb7-eb79-42c9-8b89-5bc0b04f1dd1: node tools/build.mjs followed by
node tests/github-package.mjs. It passed public raw GitHub discovery, source
build, export and reload. Build reported runtime
0e26576176880aa87d2757730cd105dd4c2c1428063ce31e58ed78e499238b52.
The test does not establish installation from a publicly published prebuilt
binary or production hosting deployment. Those remain separate acceptance work.

Checked current fullscreen stubs and inherited WM code, and read the old X11
adapter and its README. No X11 browser acceptance was rerun. In particular,
GLX/DRI/MIT-SHM support and native performance parity are not claimed.
