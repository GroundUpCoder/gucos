# gucOS agent thread UI

This app ports cc's thread presentation into a gucOS VT. The `cc/` subtree
contains copied components, styles, and their support code. The source commit
and SHA256s are recorded in `docs/research/agent-vt-1.0.3/cc-source-provenance.json`
(relative to the repository root). Builds use this checked-in copy, never the
sibling repository.

The composer, message collapse/copy bar, non-text block grouping, thinking,
tool calls/results, JSON trees, Markdown, code blocks, syntax highlighting,
ABC scores and reader-size styles come from cc. `ContentBlockGroups.tsx` is
extracted from its ChatSheet. `GuestTextBlock.tsx` adapts file links, media and
file viewing/editing to the gucOS filesystem. The app controller, conversation
list, provider metadata and settings are gucOS-specific; cc's server-only
coordination, billing, account and fork services are not implemented here.
The composer adds guest-disk draft persistence. Browser speech input remains
available where supported; cc server transcription is not connected.

`agent.mjs` implements the flat conversation schema, persistence queue,
DeepSeek SSE and tool loop. `main.tsx` owns views. The parent page exposes only
its gucOS session bridge to the same-origin app iframe. The iframe isolates
cc's CSS from terminal and SDL VTs. The UI bundle is an immutable hashed
release asset. Runtime does not read the host filesystem.

Tests: `tests/agent-state.mjs`, `tests/terminal-vt-kernel.mjs`,
`tests/agent-vt.mjs`, `tests/agent-recovery.mjs`, `tests/agent-live.mjs` and
`tests/agent-patch-upgrade.mjs`. Browser tests require the shared machine-job
browser lane. `agent-live` reads an operator credential solely to provision
an isolated test browser's guest disk; it never changes release assets.
