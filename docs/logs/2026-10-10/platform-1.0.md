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

## Public packages and deployment

Primary author executed the package thread's reviewed compatibility runner against
this frozen candidate, using run platform-1-0-c4411d72. All 14 stages passed:
17 original public packages, all 803 original output hashes before and after
execution, old Lua/Zstandard/Xaw/Xpm library relinking, JWM feature/drag tests,
X clients, xterm browser keyboard/PTY, launcher and final reload. No package
binaries or manifests were rebuilt/retagged. The report SHA256 is
2976bcc9857f6fc8b1e8a256c24c4b5460ebe2029e258fc5eddae19d194c0f80.
Copied exact report and hash-verified stage logs into the packages/ evidence
subdirectory. Harness ownership remains the external package thread; execution
of this candidate run was performed by the primary author.

Independent reviewer approved exact core commit
f0fa450cd6390c2278756b03071791e5dd211419 and the above immutable payload. The review
independently verified all 33 release files and source-derived runtime identity,
reran the metadata gate, and inspected (not reran) the ten core browser results.
Approval was conditional on unchanged-package acceptance before activation and
mandatory live checks before transaction completion. Both conditions were met.

Core was pushed, then the unchanged netguc hosting wrapper completed activation
under operator.lock. Live browser job c1359165-1cf7-44ef-8423-c14333b36a4e passed
public old-binary install, source build, terminal/two WebGPU VTs and persistence;
it also passed normal-root startup display, HTTP/checksum failures and retries.
The wrapper verified origin/public artifact hashes and isolation headers and
unchanged historical /os2 responses. It exited zero with action complete.
Production current is releases/b7b6e813e8dd462b5c7415ab8d03dd57588a86ccf80a944174aa18a70e96a1ef;
no pending journal remained. Hosting commit/route are unchanged. Exact deployment
output is committed in the research directory.

An additional fresh-profile live X11/JWM/xterm smoke is running after activation;
its outcome will be recorded separately. Do not infer its result from the local
candidate's complete package acceptance.

## Live desktop and 1.0.0 completion

The primary author ran the external package author's reviewed live smoke
(commit 8701f20), after activation, against https://os.groundupcoder.net/ in a
fresh profile. Run platform-1-0-live-c4411d72 passed all six stages: exact candidate
identity, original public X11/widget/JWM/xterm binary installation, actual JWM
and xterm keyboard-through-PTY interaction, typed guest-file persistence after
reload, and all 722 original desktop output hashes. Exact report and hash-verified
logs are preserved under docs/research/platform-1.0/live-desktop/.

All five authorized stabilization steps are complete: parallel baseline package
work was preserved, the supported boundary is documented, compatibility uses
platform/dependency requirements instead of universal build-hash equality,
retained artifacts passed execution/relinking/upgrade gates, and the reviewed
1.0.0 implementation is deployed. The original public package archives remain
unchanged. New platform-aware manifests are supported; existing publications
continue through the documented legacy-baseline mapping. No claim is made that
all existing public package manifests were rewritten to the new schema.

The platform contract is now marked released. Release tag v1.0.0 will identify
the same implementation plus these final evidence records; documentation-only
completion changes do not change the deployed runtime or release manifest.
