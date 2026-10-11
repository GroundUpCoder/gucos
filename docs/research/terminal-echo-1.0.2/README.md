# Terminal control echo patch

The user requested the gucOS-side fix and a platform patch after gucode 0.1.2
was published. The application release did not deploy core. This patch extends
the pending, previously accepted IPC 1.0.2 release; it retains runtime ABI 5,
object ABI 3 and compiler 0.5.13. Application recipes/catalogs are not build or
deployment inputs and were not read from a packages checkout.

## Defect and correction

The kernel line discipline echoed control input verbatim. For example, an
arrow key in a canonical reader inserted ESC-[D into the input buffer but
echoed a cursor-left command to the display. The visible cursor and stored
input then disagreed. Ctrl+A is a line-editor command, not a canonical-kernel
cursor-motion command; gucode 0.1.2 supplies that application editing behavior.

New TTYs and PTY slaves enable ECHOCTL (0x200 in this libc's existing flag
layout). Control bytes display as caret notation, including ESC as ^[ and DEL
as ^?, with tab/newline retaining layout behavior. The original input bytes
are preserved. Applications can clear ECHOCTL, and ECHO-off raw editors remain
unaffected. Canonical erase/kill account for two-cell caret echoes, including
with IUTF8 disabled. Signal echo honors the flag; newline echo honors OPOST
and ONLCR. No kernel cursor editor or GNU Readline implementation was added.

The behavior was checked against Linux's primary
[n_tty echo implementation](https://github.com/torvalds/linux/blob/master/drivers/tty/n_tty.c)
(retrieved 2026-10-12). The implementation here uses the existing gucOS Tty
model; no Linux code was copied.

## Exact candidate and tests

Final runtime: cc3227370c2ccfb2a5ed7ea97627dbd512ec19abfacd7bf2f5ca8dc27421f309.
Build, release and Pages manifests are retained here. The ready upload is
releases/terminal-echo-1.0.2-final/site. Build/test logs contain machine-job IDs;
browser jobs ran serially on the local Mac using Chromium/JSPI.

- Kernel regressions cover byte-preserving control echo, canonical erase/kill,
  Unicode widths, signal echo, ECHOCTL opt-out, raw/ECHO-off behavior, and both
  brokered input and the standalone shared ring.
- Guest C exercises real openpty/tcgetattr/tcsetattr/read/write/select paths,
  exact echo bytes and original delivered bytes. This is the PTY path used by
  terminal emulators; this core patch did not rerun the full X11 package suite.
- Real VT keyboard acceptance checks ArrowLeft, Ctrl+A/E, exact delivered
  bytes, the rendered xterm.js buffer, and recovery to the shell. The final
  driver enters the interactive shell through its actual UI, focuses the
  terminal textarea, and waits for the startup overlay to hide. See
  terminal-keyboard-final.log and local-echo.png.
- gates.json records all 13 candidate browser checks passing: terminal echo,
  disk upgrade, IPC sockets/services/terminal/symbols, closefrom, POSIX,
  compiler/Make/archive smoke, package manager, historical storage boundaries,
  virtual terminals, and WebGPU. The stricter rendered-line terminal test
  was then rerun separately on the identical runtime.
- The 1.0.1-to-1.0.2 upgrade preserves a user marker and retained executable,
  DSO, object and archive hashes. Old executables/DSOs run; objects/archives
  relink; a second reload passes. Retained artifact provenance remains the
  original platform 1.0.0 build, not a claim they were rebuilt on 1.0.1.
- Public gucode 0.1.2 installs and survives reload on the candidate. Real
  Ctrl+A/E edits produce /exit, and the normal shell works afterward. Only
  local slash commands and a synthetic test key are used; no provider call.
- live-core passes against the local candidate with real public jsmn binary
  install and source build, reload persistence and two independent GPU VTs.
  Its PASS LIVE label names the test; this run used LIVE_CORE_URL=local and
  is not production deployment evidence. See public-package-local.log.

Initial failures and the superseded candidate are retained under
initial-candidate/. The first build needed regenerated libc metadata. Early
keyboard harnesses incorrectly launched a standalone process or left focus
on the terminal tab button; the initial screenshot also showed bootstrap
prompt redraws. These harness errors were corrected rather than weakening
assertions. A whitespace-only header cleanup changed the final build identity;
all release gates were rerun against the final runtime above.

## Publication status

The patch is implemented, built and tested. No production upload has occurred.
CLOUDFLARE_ACCOUNT_ID/CLOUDFLARE_API_TOKEN are absent from this shell and Wrangler
reports no authenticated session. The existing credential location has been
requested; preview deployment, preview acceptance, production deployment and
production acceptance remain pending. No credentials or provider keys are
included in this evidence.
