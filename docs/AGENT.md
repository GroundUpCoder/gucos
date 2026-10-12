# Terminals and agent conversations

Use **+ Terminal** to open an independent shell VT. All terminals, graphical
applications and agents share the existing gucOS disk. Alt+1 selects the
original terminal; Alt+2 through Alt+9 select subsequent visible VTs.
Closing a terminal stops the processes launched in that terminal.

Use **+ Agent**, then **New conversation**, to open an agent thread in its own
VT. Multiple conversations can stay open. Closing a conversation VT preserves
its history and draft. Open it again from the searchable conversations list.
Stop an active turn before closing its VT. Reload interrupts running turns;
reopening reports the interruption and does not repeat unfinished tool calls.

The DeepSeek key is a file **inside gucOS**:

```
/home/user/.guc/creds/deepseek-api-key
```

Use Agent settings to save it, or create `~/.guc/creds/deepseek-api-key` from a
guest terminal. The browser talks directly to `https://api.deepseek.com`.
No host filesystem access, credential in the public bundle, or key proxy is
involved. The default model is `deepseek-flash`; the model field is editable
while idle. The integration follows the provider's
[tool-call interface](https://api-docs.deepseek.com/guides/tool_calls/).

The agent can execute Bash commands, read and write files, and apply exact
text edits. Commands operate on this gucOS machine. Each command starts in
`/home/user`; include `cd` in commands needing another directory. Stop cancels
the provider request and the current command's process tree. Tool commands
have a default two-minute timeout, capped at ten minutes, and a one-million
character output limit. A turn pauses after fifty model/tool rounds.

## Persistence

Each conversation is an ordinary UTF-8 JSON file:

```
~/.guc/agent/threads/<uuid>.json
```

Format 1 has scalar thread metadata and a single flat `blocks` array. There
are no nested message/turn trees. Blocks have stable IDs, timestamps, roles,
`messageId` (provider exchange) and `turnId` (displayed assistant turn).
Text, thinking, tool use, tool result, image, info and usage blocks preserve
ordering. Tool arguments are JSON strings; tool/result association uses
`callId` plus `messageId`. Views derive their cc groups from the flat sequence.
Partial streamed blocks are retained with `state: incomplete`. Partial text
can be included on continuation; incomplete tool calls are never executed.

Agent saves refuse the OS memory-only fallback, so volatile sessions cannot
be mistaken for durable conversations. Saves serialize through a queue, replace a temporary file with rename, then
flush the existing journal. Intent is saved before executing tools and results
before the next model request. Streaming checkpoints occur at approximately
300 ms intervals. A sudden close may lose the latest uncheckpointed text or
keystrokes; committed conversation data remains restorable. Guest commands
have the filesystem's ordinary flush semantics. If reload occurs during a
command, its side effects may have happened even without a saved result;
restoration explicitly marks that uncertainty and does not replay the command.

Export downloads the flat JSON. Import validates format 1 and opens a copy
with a new conversation ID, preserving any existing conversation. Browser
storage is origin-local: preview, production and other devices have separate
disks. Clearing/resetting browser storage removes conversations with the rest
of that installation; keep exported backups when needed. Old disk namespaces
are neither read nor migrated by this feature.

## Version and compatibility

This is OS patch **1.0.3**, following **1.0.2**. Disk namespace, journal format,
compiler version and object/runtime ABI epochs are unchanged. The kernel adds
explicit terminal selection for new shells, preserving the previous default
terminal behavior for existing callers. Minor or major OS version changes
require explicit user approval.
