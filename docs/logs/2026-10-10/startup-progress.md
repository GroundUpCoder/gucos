# Startup download visibility

User explicitly authorized a core startup progress display and deployment.
Scope: core UI and worker progress events only; no package/hosting edits,
no download scheduling changes, disk migration, or changes to hash verification.
Lightweight release: existing static deployment transaction, independent exact
implementation/payload review before push/deployment. Author owns this checkout;
base 94357cb. Package author remains confined to its repository.

The worker computes required download URLs from actual disk state, deduplicates
aliases, and reports waiting/downloading byte counts/verifying/ready/failed.
The panel reports completed/total/remaining, then shell-start phase. A footer
button reopens the history; failures remain visible with reload retry. The initial
HTML supplies a loading state before scripts unpack. Content uses textContent;
375px layout and polite count announcements included. Cached transfers are labeled
as downloads/checks rather than falsely claiming network cache misses.

Builds use existing runtime identity semantics: every source change produces a
new exact package runtime key. No compatibility bypass is introduced. Existing
installed programs remain on disk; source recipes can target the new runtime.
Acceptance and exact release identities will be appended after execution.

Author browser suite passed on first reviewed implementation: fresh 14 unique
assets, persisted reload 5 helpers, exact remaining counts, actual shell/input,
375px screenshot, HTTP/checksum failure with retry; compiler/Make/archive,
package recovery/binary fixture and historical storage preservation all passed.
Job cdb67e1f-d2fc-4bb5-af91-3852d4fd5c4d; /tmp/wasm-posix-startup-acceptance.log.

Independent reviewer 01a123e0-2fb3-7521-9fa3-e936e0997405 (Codex gpt-6-astra)
found no bootstrap correctness blocker, but requested actual streamed-byte UI
coverage, focus restoration on hide, and release acceptance of explicit old
binary rejection followed by source build on the new exact runtime. Incorporated
all three. No package manager compatibility policy was changed. Final acceptance
and exact payload review pending at this entry.
