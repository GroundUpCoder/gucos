# Live 1.0.0 user experience measurements — 2026-10-10

Target: https://os.groundupcoder.net/ (normal interactive root URL).
The measurement script opens three independent Chromium contexts and reloads each
within the same context. Fresh means empty browser storage/cache; it does not
flush the operating system DNS cache or the server/CDN cache. No network or CPU
throttling is applied. The first context records an actual Playwright video.

Boot latency is navigation time origin to a rendered, ready BusyBox prompt.
The probe listens to xterm's render event, then waits two animation frames as a
paint approximation; this is not a hardware display-photodiode measurement.
Package latency starts at the browser's Enter keydown event. Typed command echo
and line breaks do not count as progress. The first new checksum, installation,
build or error line must exist both in this command's new output and in the
visible terminal buffer. Completion is the rendered return of the prompt; the
runner additionally asserts the expected successful install message. Install
measurements include video-recording overhead and are individual observations,
not statistical benchmarks. Commands use the site's default public repository.
JWM follows jsmn, and xterm follows JWM, so the latter may reuse dependencies.

An initial exploratory run was stopped because `clear` is unavailable in this
shell; stale screen contents could satisfy its progress detector. That run is
excluded from final results. The corrected script clears with ANSI escapes and
also requires new command output. Its exact source is `measure.mjs`.

The earlier complete live keyboard/PTY and reload acceptance is preserved in
`../platform-1.0/live-desktop/`. This run adds timings and visual recording; a
visible desktop screenshot alone is not a substitute for that keyboard test.

## Results

Browser: 153.0.8010.12. No page errors. All three installs succeeded, and the actual JWM desktop with xterm became visible.

| Operation | First actual progress | Completion |
| --- | ---: | ---: |
| fresh boot, median of 3 | — | 2.737 s (range 2.554–2.991) |
| reload boot, median of 3 | — | 0.356 s (range 0.307–0.657) |
| `gucman install jsmn` | 2.010 s | 2.178 s |
| `gucman install jwm` | 78.717 s | 108.447 s |
| `gucman install xterm` | 16.105 s | 27.891 s |

JWM installed x11-server, x11-toolkit, x11-widgets and JWM. xterm then reused those
dependencies. The first JWM response was the x11-server manifest checksum; the
first xterm response was `Already installed: x11-server`. These are observable
activity but not useful download percentages. Package fetching and preparation
remain silent until those messages. Source inspection confirms quiet catalog /
object fetching and no initial install phase message in manager.sh; this run does
not attribute every millisecond of silence to network rather than guest work.

Browser scheduler job: `e416c8f5-ac4b-4ad9-bdf9-474a67edb04a`.
Video conversion job: `2d4ef843-16ce-4dda-ac5c-08990570e3d2`.
The original WebM is continuous, approximately 157 seconds. `replay-12x.gif` and
`replay-12x.mp4` accelerate the entire recording 12×, without cutting out waits.
The MP4 is provided for inline chat playback; the requested GIF is also retained.
The boot's few seconds consequently pass quickly in this accelerated replay.

## Deployment verification

Direct read-only hosting check confirmed active release
`b7b6e813e8dd462b5c7415ab8d03dd57588a86ccf80a944174aa18a70e96a1ef`
and no pending deployment journal. HTTPS root returned 200 and COOP/COEP isolation
headers. Each browser navigation verified runtime
`c4411d720bede49ead5c1de69bfe7b1247656a146b72d991d209883606e300fb`.
Remote main and dereferenced v1.0.0 both resolved to
`7cf44cb8b39044767f6ca3d446713a06c976e6ca` before these evidence-only changes.
No production code or deployment was changed by this measurement work.
