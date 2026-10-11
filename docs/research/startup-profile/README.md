# Live startup profile — 2026-10-10

Measured directly by this thread against https://os.groundupcoder.net/ (normal
interactive startup, not ?shell=bootstrap), core c8c6206 / runtime
2a9cca447a35af0d0d8894ea4b50cb254ce20821753af8ae70a1ad5060da2a0f.
Chromium 153.0.8010.12, headless, on the local Mac, with no network or CPU
throttling and no request interception. Three fresh browser contexts, each followed
by a reload retaining HTTP cache and its newly populated OPFS disk. This does not
measure the user's machine or a disk containing installed application packages.

## Results

Time from navigation start to the first actual BusyBox `~ $ ` prompt:

| Run | Fresh context, no CPU trace | Reload, no CPU trace | Fresh, with CPU trace | Reload, with CPU trace |
|---|---:|---:|---:|---:|
| 1 | 2.738 s | 0.601 s | 4.809 s | 0.586 s |
| 2 | 2.979 s | 0.256 s | 4.409 s | 0.582 s |
| 3 | 2.828 s | 0.332 s | 3.889 s | 0.308 s |
| Median | **2.828 s** | **0.332 s** | 4.409 s | 0.582 s |

The heavy trace run is slower; tracing overhead and network/run-order variation
are not separately isolated. Use the no-CPU-trace measurements for baseline
latency. Small readiness/worker-message instrumentation remains in both runs.
The recorded `wallMs` also includes a deliberate 100 ms settling delay and test
polling; the table uses timestamped prompt messages instead.

## Findings

1. **Bootstrap asset transfer is the dominant cold-start wait.** In all three
   traced fresh boots, the libc downloads are parallel, followed by serialized
   ash, BusyBox, Make, five package helpers, cc and ar fetches. From the start of
   libc fetching to completion of the last binary, the chain spans 3.12–3.34 s
   in the instrumented runs. Recorded bootstrap binary transfers total about
   9 MB, excluding worker script loads that this trace's ResourceSendRequest
   summary does not enumerate. This is not a complete page network-byte total.
2. **Large binaries arrive uncompressed.** Trace response headers for libc,
   ash, BusyBox and Make show no Content-Encoding. The large libc archive
   response, for example, has Content-Length 1,400,238 and Cloudflare cache HIT.
   CDN caching is already working; it does not eliminate client transfer time.
3. **The source confirms the serialized critical path.** system-owner.js seeds
   ash, then BusyBox, then Make before it launches the shell. repository-boot.js
   fetches its five helpers in an awaited loop. Tools not immediately needed
   for a prompt are nevertheless installed before one can appear.
4. **Persistent storage is secondary, not free.** Fresh owner-worker CPU
   profiles sample about 226–233 ms in JournalStore's writeAll and 18–19 ms in
   crc. Samples in synchronous write calls can include blocking I/O, so these
   are sampled time attribution, not pure arithmetic CPU time. The worker is
   otherwise predominantly idle while the network chain runs. No main-page
   long task >=50 ms was observed in these runs. This does not imply workers
   have no long tasks or that native Wasm compilation has zero cost.
5. **Warm startup is mostly navigation wait.** With the disk and cache populated,
   the no-CPU-trace runs take only about 45–50 ms from creating the filesystem
   worker to the prompt. Document/server/network wait and page setup dominate
   the remaining 0.26–0.60 s. Package companions are still fetched and rewritten
   on every boot, but these requests hit browser cache in the measured reloads.
6. **shell.ready is not OS readiness.** It becomes true when the page API is
   available, before BusyBox startup. The initial exploratory probe stopped at
   that incorrect boundary; its data is preserved as early-ui-* and excluded
   from the startup table. Corrected profiling waits for busybox.ready AND the
   actual prompt. Existing functional tests invoke commands after page readiness;
   this finding does not invalidate their command assertions, but shell.ready
   alone cannot be used as a startup benchmark.

## Optimization priorities (proposals, not implemented)

- Fetch independent bootstrap assets with bounded concurrency, preserving hash
  checks and coherent runtime publication.
- Serve compressed binary assets or a compressed bootstrap bundle; measure the
  transfer/decompression tradeoff before choosing a format.
- Investigate deferring Make/static libc and other development-only tools until
  needed, while preserving offline/self-hosting behavior. This is an architectural
  choice, not necessary merely to parallelize downloads.
- Then profile/optimize fresh-disk writes; expose truthful startup progress and
  maintain a prompt-ready cold/warm benchmark.

No runtime, package, hosting or production changes were made for this profiling.

## Evidence and reproduction

- profile.mjs: live-site probe. Run through machine-job --kind browser.
  PROFILE_TRACE=0 disables the Chromium CPU/timeline trace.
- baseline-results.json: no-CPU-trace timings (job 36d1051d-7600-468c-8a4b-30b2d55df87f).
- results.json, trace.json.gz: corrected timing and Chromium trace (job
  72840c98-09b7-42af-a4f7-7644c43fab48). Decompress trace for Perfetto/DevTools.
- analyze.py, analysis.json: asset-name mapping, network requests and sampled
  profiles. Asset mapping comes from local reviewed dist/index.html. Network
  trace offsets are relative to ResourceSendRequest, which can occur later than
  navigation start; use results.json for navigation-relative timing.
- early-ui-results.json, early-ui-trace.json.gz: initial exploratory probe,
  excluded from conclusions about full OS startup.
- SHA256SUMS: supporting evidence checksums. Fresh means fresh browser storage
  and HTTP cache, not cold DNS, cold CDN, or a rebooted operating system.
