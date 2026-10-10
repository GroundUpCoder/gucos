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
