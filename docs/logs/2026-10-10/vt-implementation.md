# SDL VT implementation — author work in progress

Continuation coordinator is external cc-meta thread
01a12401-30cf-7004-a932-af576a939e08; metadata observed executor codex,
model gpt-6-astra. Existing design reviewer
01a123d5-f441-7cc3-829a-cb511f15f8c7 approved the concrete design delta at document
SHA256 2581a1789dc9c85fda84cf3870c3c38a62004ad0ce20645f183ed88f4df08f1d.
Implementation review and deployment are still pending.

Executed author work:
- Removed kernel WM surface/scene/protocol implementation and page desktop chrome.
- Added one process-owned transferred canvas per graphical VT, terminal switching,
  process-local SDL/WebGPU bridge validation, bounded JSPI present pacing and
  invocation cleanup. Retained generic sockets, filesystem, process and audio code.
- Retained SDL3-compatible shim, not upstream SDL. Restored missing
  tools/libc-manifest.mjs from old repository 0b9b3245fe11a3bc677d81113cba90cd0ae80446.
- Build and browser smoke passed; compile/Make/archive/persistence/reload evidence
  /tmp/wasm-posix-vt-smoke.log.
- Browser graphics acceptance passed on build runtime
  50a81028db7ffa06e149d27228afbd3df6879618c28a47054e76c22d3ec217cb,
  job cfda0533-2c67-4d2b-b91f-42ff4eff16e8,
  /tmp/wasm-posix-vt-graphics3.log: two SDL applications, single visible VT,
  terminal switch, key release, viewport resize, hidden frame progress 10–30
  measured frames over 2.2 seconds, crash/exit cleanup and absent /run/wm.sock.
  This exercises process-owned WebGPU software-surface blitting, not yet a direct
  WebGPU application/device-loss acceptance. Default Playwright headless shell
  lacked an adapter; full Chromium with --enable-unsafe-webgpu passed.

Hosting author corrections remain in netguc-os-core. Existing uncommitted helper
rewrite preserved and extended for reused payload permissions. Python helper and
composed-wrapper fault tests pass; real Caddy serving-user validation is separate.
No production route/pointer mutation or deployment has been performed.

Packages: historical X11 source archive imported into packages repository from
old commit above; archive source bytes retained, guest recipe adapts absolute
build paths. Runtime guest build/acceptance is underway. Public package binary
publication/install, full X11 acceptance and final independent reviews remain.
