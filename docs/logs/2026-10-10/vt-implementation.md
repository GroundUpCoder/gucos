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

Implementation reviewer returned five concrete findings against core cb13bec and
hosting 2afeb196: initialized SDL loss guard bypass, WebGPU configure stealing
visibility/focus, missing relative deltas, redirect identity lost during hosting
verification, and a weak focus-release assertion. Author corrections are being
validated; approval has not transferred to changed artifacts.

Author direct WebGPU acceptance passed under job
0652bcf9-9af7-419a-a10f-56d2fe1fb019, runtime
4cb8ccdd6b0c1e5a6dc923fb6a7c64a7fdcce4ea4c3780bf5aa05add9479e389:
two direct WebGPU applications through SDL_GetWGPUSurface, viewport resize,
one application crash while the other device keeps presenting, and terminal
recovery. This does not by itself exercise device loss. Log
/tmp/wasm-posix-webgpu-vt.log.

Author correction gates passed on runtime
2a9cca447a35af0d0d8894ea4b50cb254ce20821753af8ae70a1ad5060da2a0f:
- Browser job 4c82be76-8938-4960-9b0d-a611a25de7f9: strengthened SDL held-key
  and button release assertion, direct WebGPU two-process test, complete runtime
  package suite, historical storage boundary, real public GitHub build/export/reload.
- Node function-level GPU/DOM fault probes: initialized software/renderer presents
  reject lost devices before further submits, configure emits a size update,
  invalid/stale window binding is refused, relative deltas are retained.
  These probes are mocks, not actual device-loss browser evidence.
- X.Org source build and installed independent Xlib client passed under browser
  job e7da3dd6-9b9a-481b-84ee-6a03ef2cba63 on preceding runtime 4cb8ccdd…;
  527 outputs, no host compilation, local HTTP runtime repository fixture.
  Log /tmp/wasm-posix-x11-runtime4.log. Real twm/toolkit acceptance on current
  runtime is underway in packages repository; public X11 discovery still pending.

Prepared tests/live-core.mjs as the mandatory hosting wrapper browser command.
It checks exact public runtime identity, fresh binary install, reload, guest
compiler, two direct WebGPU VTs and terminal, then a separate fresh-profile public
source build/reload. This gate has not yet run against production; it must run
while the deployment wrapper retains operator.lock.
