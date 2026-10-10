# Application boundary stabilization

User authorized the complete pre-1.0 work after reviewing the implemented POSIX
and SDL3 subset. Deployment remains gated on independent review and browser
acceptance of the exact release. The parallel package author owns package-only
changes; this checkout never discovers package sources during build/deployment.

Implemented in this change:

- Descriptor-relative at-family operations resolved and executed within a single
  broker request, preserving directory identity across rename; validate flags.
- Explicit pthread unsupported errors, valid attribute checks, and registered LIFO
  cleanup on explicit thread exit without changing existing pthread layouts.
- Single-user ownership target validation and errors for unavailable identity
  changes; access-mode validation and execute checks.
- SDL composition/editing events, UTF-8 commits, held-key release during composition,
  consistent unsupported window flag/fullscreen behavior, and preserved text-event
  ownership during event copies.
- Whole-platform SemVer identity distinct from compiler, ABI epochs and hashes;
  target/minimum version/features/object and runtime checks; named legacy baseline;
  dependency constraints and conservative installed reverse-dependency checks.
- Core libc refresh on existing core disks with downloads verified before writes
  and metadata published last; incomplete upgrades retry before running programs.
- Real startup download/remaining/byte display; fixed size propagation and stale
  partial-byte reports, plus accessible dismissal and retry behavior.

Evidence and limitations:

- Independent external reviewer: thread 01a123e0-2fb3-7521-9fa3-e936e0997405,
  Codex gpt-6-astra. Static audit and lightweight probes identified ownership,
  cleanup, SDL flags, stuck IME key release and access-mode defects; author fixes
  and browser tests are separate from that review.
- Preserved artifacts under docs/research/platform-1.0/retained-artifacts were
  generated using the exact deployed baseline 2a9cca447a35af0d0d8894ea4b50cb254ce20821753af8ae70a1ad5060da2a0f.
  Provenance records source text and hashes. Subsequent upgrade tests read these
  bytes and verify hashes rather than rebuilding them to suit the candidate.
- IME automation exercises browser composition events, including Korean text and
  modifier release. It does not establish physical OS IME/mobile keyboard support.
  Candidate rectangle placement is explicitly outside the current profile.
- Fixes to header-inline/static libc code cannot retroactively change old binaries;
  preserving execution compatibility does not claim otherwise.

Final validation, release identity and deployment receipt will be appended after
the required gates finish. No completed deployment is asserted by this draft.

## Frozen candidate core acceptance

Runtime/build `c4411d720bede49ead5c1de69bfe7b1247656a146b72d991d209883606e300fb`,
release manifest `b7b6e813e8dd462b5c7415ab8d03dd57588a86ccf80a944174aa18a70e96a1ef`,
immutable export `/tmp/wasm-posix-platform-c4411d72`.

Author executed machine browser job `c5e8f386-ee02-490f-9bfa-3cb7c08ffd18`:
all ten tests in tests/platform-gates.mjs passed. Exact runtime, individual test
statuses and stdout/stderr are committed under docs/research/platform-1.0/ as
core-gates.json and core-gates.log. This includes unchanged baseline executable
and DSO execution, retained archive relinking, interrupted runtime publication
recovery, new APIs on an upgraded disk, real negative package-manager integration,
startup streaming/failures/retries, filesystem namespace preservation, SDL and GPU
VT isolation. GPU device-loss failure injection is a function probe, not proof of
a real GPU hardware reset. Browser IME tests remain synthetic composition events.

Build-only job `ede72110-e60e-4c65-b288-3e58b247fea5` passed strict platform/dependency
metadata tests, recorded 12 emitted module import/export inventories and 137 header
hashes, and exported the above immutable release. The inventory is review evidence,
not a full mechanical ABI signature/conformance certification.

Expanded public-package compatibility and final release approval are pending at
this checkpoint. Production has not been changed by this work yet.
