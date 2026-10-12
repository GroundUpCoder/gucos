# Terminal VTs and persistent DeepSeek conversations

User scope: add terminal VTs and agent conversation VTs, use the actual cc
thread components and collapse behavior, persist a flat block sequence, test,
commit, push and deploy. The user clarified that the credential path is inside
gucOS, not the host. The user also required patch updates by default and
explicit approval for any minor or major OS version change; this policy is
now recorded in AGENTS.md. The candidate is 1.0.3.

Implemented independent shell terminals on the existing owner/kernel/disk,
explicit terminal selection at boot, inherited launch-session cleanup and
request-specific errors. Retained existing createTty default replacement
semantics, disk namespaces and ABI epochs.

Copied cc UI sources into apps/agent-ui/cc with source hashes and commit
provenance. Ported the actual composer, block renderers, non-text grouping,
collapse/copy controls, Markdown, code/ABC rendering, JSON tree and styles.
Guest adapters provide file links/media/editing. The surrounding controller
and provider-specific metadata are local; cc's server coordination features
are not claimed to be implemented. Dependency license notices are emitted
with the platform notices.

Conversations use journal-flushed atomic file replacement and flat blocks
with response and turn IDs. Drafts, partial streams, tool intent/results,
provider usage and interruption markers persist. Usage subobjects are encoded
as a JSON string with useful counters also flattened. Restore never executes
a saved tool call. Export/import provides a separate backup path.

Initial validation found and fixed a browser fetch receiver binding error.
Acceptance polling was changed to explicitly await guest filesystem reads
rather than rely on Promise-valued waitForFunction predicates. Reload testing
flushes the test effect before interruption so absence of a replay can be
checked against durable input. Initial successful evidence is retained; final
candidate results and deployment receipts are recorded under
`docs/research/agent-vt-1.0.3/`.

The live provider test provisions an operator credential into a disposable
browser's gucOS disk. The application itself reads only the guest credential
path. The test never exports the credential, guest disk or browser profile.
Authenticated streaming and an actual Bash file write/read have passed.
Controlled-provider tests independently exercise exact block grouping,
continuation, draft restore and cancellation. Upgrade checks from retained
1.0.2 preserve user data and existing executable, DSO, object/archive hashes.

The graphical VT regression caught a narrow-taskbar pointer interception bug.
The taskbar now scrolls as a whole, preserving clickable tab widths; the
regression subsequently passed. The frontend build now requires the pinned
pnpm dependencies, which is documented in README.md and was explicitly
flagged to the user. Browser disk and application ABI compatibility are unchanged.

Final local acceptance passed for runtime
`89250c7b2d43fbb9845431a61e01f7a06cb5b7a1da6c026193a60c578bd6cf9d`.
The retained candidate is `releases/agent-vt-1.0.3`. Source commit `0d0cfd0`
was pushed before deployment. Exact preview `8a05d8dd.gucos.pages.dev` passed
HTTP and full browser acceptance. The identical directory was uploaded as
production `00021b47-9818-4094-ad25-1768f3f6569e`, then the same checks passed on
https://os.groundupcoder.com. Authenticated DeepSeek tool execution was verified
at both URLs. The Cloudflare API confirms the canonical deployment and active
domain. A follow-up production resource audit identified console failures as
analytics URLs intentionally denied by the test harness; application checks
passed. The release remains a patch with no ABI epoch or disk namespace change.

The original credential decision was updated with the user's exact direct
answer and cancelled as redundant, so it does not remain a stale pending
question. No delegated decision answer was substituted.
