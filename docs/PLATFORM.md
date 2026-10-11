# wasm-posix application platform

Status: 1.0.0, originally released 2026-10-10 at https://os.groundupcoder.net/.
Current hosting and repository ownership: https://os.groundupcoder.com/ and
GroundUpCoder/gucos. The migration preserves this platform contract.
The compatibility, package, upgrade and live-browser gates passed; exact evidence
and deployment identity are recorded in `docs/logs/2026-10-10/platform-1.0.md`.

## Identity and compatibility

The target is `wasm32-wasm-posix`, not WASI, Linux ELF or native SDL3. Executables
are WebAssembly modules using this runtime's imports and metadata. The public
platform version uses Major.Minor.Patch independently of the compiler version,
immutable build SHA-256 and individual artifact SHA-256 hashes.

A stable package requiring M.m.p runs on the same target and major M when the
host version is at least M.m.p, its required features are available, its ABI
requirements match, and all declared dependencies match. Minor releases add
compatible functionality. Patches fix behavior without intentionally breaking
specified interfaces. Removing/changing a promised interface requires a major
release. Bug-for-bug compatibility with unspecified behavior is not promised.
Prerelease requirements are not accepted by the stable package comparator.

The current object ABI epoch is 3 and runtime ABI epoch is 5. These are internal
format/protocol identifiers, not platform major versions. Keeping them unchanged
is necessary but is not by itself proof of compatibility. Within platform 1.x,
the supported baseline must remain accepted even if internal readers evolve.
Executables and dynamic libraries have a runtime contract; retained objects and
static archives additionally have a relinking contract. A compiler upgrade does
not by itself require rebuilding applications.

The pre-contract build
`2a9cca447a35af0d0d8894ea4b50cb254ce20821753af8ae70a1ad5060da2a0f`
is the sole legacy compatibility baseline. Its build hash remains provenance;
it is not required to equal the current build. Unknown historical baselines fail
closed. Existing inline/static code keeps its original semantics: upgrading the
host cannot repair ownership no-ops or old cleanup macros embedded in an old
binary. Execution compatibility is distinct from adopting corrected libc code.

## ABI that must remain stable

* wasm32 little-endian pointers and C `int`/`long`/`size_t` are 32 bits;
  `long long`, `off_t`, `time_t` and `double` are 64 bits. Public headers define
  alignment, structs, numeric constants and signedness. Changing an existing
  public layout/constant is an ABI change even if an import name is unchanged.
* Preserve existing imports and their WebAssembly signatures; extensions are
  additive. The namespace, shared-memory/table conventions, thread record,
  TLS, exceptions, stack/unwind, process/fork snapshots and dynamic relocation
  conventions are part of the runtime contract, not arbitrary build details.
* Preserve accepted custom `wasmposix.obj` and `wasmposix.dylink` metadata
  readers, archive linking, dynamic symbol lookup and existing libc SONAMEs.
  Metadata format numbers are separate from ABI epochs.
* Browser execution requires cross-origin isolation, shared WebAssembly memory,
  Atomics, WebAssembly exception handling and JSPI. GPU support additionally
  requires usable WebGPU. Browser permissions/user activation still govern
  clipboard, pointer lock and audio; API availability is not a permission grant.
* Core-owned libc archives, DSOs and producer metadata are refreshed together
  when their digest changes. All downloads are verified before publication.
  Application files and the existing core disk namespace are preserved.

## POSIX profile

This is a defined implementation subset, not POSIX certification. The source of
public declarations and numeric/layout definitions is `libc/include`; membership
in a header alone does not promise that every option works.

| Family | Supported behavior and bounds |
| --- | --- |
| Files | `open`, `creat`, `close`, `read`, `write`, `pread`, `pwrite`, `lseek`, `stat`, `lstat`, `fstat`, `truncate`, `ftruncate`, `fsync`, `fdatasync`, `dup`, `dup2`; directories, rename, hard/symbolic links, access, modes and umask |
| Directory descriptors | `openat`, `fstatat`, `mkdirat`, `unlinkat`, `renameat`, `faccessat`, `linkat`, `symlinkat`, `readlinkat`, `fchmodat`, `fchownat`, `futimesat`; resolution and operation share one broker request and never change cwd; open directory identity follows rename |
| at flags | `AT_FDCWD`; stat/chown `AT_SYMLINK_NOFOLLOW`; unlink `AT_REMOVEDIR`; link `AT_SYMLINK_FOLLOW`; access `AT_EACCESS` (identical root identity). Unknown flags fail `EINVAL`; chmod of a symlink fails `ENOTSUP`. Relative operations on an unlinked directory whose namespace path no longer exists are outside this profile |
| Processes | `fork`, `execve` and exec wrappers, `posix_spawn`/`posix_spawnp`, waiting, process groups, sessions, pipes and shell execution; cooperative WebAssembly execution, not native instruction-level preemption |
| Terminals | PTYs, termios and terminal process-group integration; supported terminal ioctls only, no arbitrary native devices |
| Signals | Process signals, actions/masks, waiting and timers, delivered at runtime safe points; thread-directed nonzero signals are restricted |
| pthread | Creation, join, detach, mutexes, conditions, once, TLS keys/destructors, rwlocks, barriers and spinlocks. Cleanup handlers execute LIFO on explicit `pthread_exit` or requested pop |
| pthread exclusions | Cancellation and cancellation configuration return `ENOTSUP` (invalid configuration values return `EINVAL`); `pthread_testcancel` cannot report an error and is an inert cancellation point because cancellation is unavailable. Process-shared mutex/spin attributes and thread names are rejected. No asynchronous cancellation guarantee |
| Local IPC | `AF_UNIX`/`SOCK_STREAM`, bind/listen/accept/connect/socketpair, shutdown, send/recv and vector messages; no datagrams, abstract namespace, ancillary descriptor passing or writable socket options |
| Network | Browser HTTP fetch is a separate extension. AF_INET/AF_INET6 sockets, TCP, UDP and DNS lookup are outside the POSIX profile; numeric address parsing is not network connectivity |
| Memory | malloc family; mmap private read/write anonymous storage or file snapshots, page-aligned offsets and whole-allocation unmap. Shared mappings, protection changes, executable pages and fixed-address replacement fail |
| Ownership | One identity (UID/GID zero). Ownership calls validate targets; root/unchanged ownership succeeds, nonzero ownership changes fail. No multiuser security boundary |
| C/locale | Standard C runtime facilities as implemented by the public headers; C/POSIX and C.UTF-8 locales. Other locale aliases do not imply locale-specific collation/formatting. Timestamps are stored at second granularity |

The generic browser fetch, WebGPU and SDL surface bridge are explicit platform
extensions. Runtime internals, kernel RPC numbers, JavaScript globals and DOM
markup are not application APIs.

## SDL3 profile

The application-facing API follows the named SDL3 interfaces implemented by the
shim, not the entire upstream library. One graphical process owns one VT surface;
X servers, window managers and toolkits remain packages.

* `SDL_Init` accepts VIDEO, AUDIO, EVENTS and GAMEPAD; other subsystem requests
  fail. GAMEPAD does not imply an implemented raw JOYSTICK subsystem.
* Window creation accepts FULLSCREEN, HIDDEN, BORDERLESS and RESIZABLE. Fullscreen
  means the VT presentation policy, not exclusive monitor/browser fullscreen.
  Changing that policy after creation fails; querying/setting native desktop
  position, native embedding, exclusive modes, minimization, extra top-level
  windows, unsupported graphics flags and custom desktop policy are unavailable.
* Surface creation, pixel/palette operations, clipping, color/alpha modulation,
  blitting/conversion, window surfaces, 2D renderers/textures and render targets
  form the rendering subset. Surface colorspaces are unknown/default or sRGB;
  RLE is an optional optimization and remains off. Damage rectangles may repaint
  the entire surface. No promise of the upstream renderer's every format/option.
* Keyboard, mouse, wheel, relative mouse, focus and text input are supported.
  Composition updates become `SDL_EVENT_TEXT_EDITING`; commits become UTF-8
  `SDL_EVENT_TEXT_INPUT`, only while `SDL_StartTextInput` is active. Browser DOM
  composition does not expose every native selection detail: updates report the
  whole composition as the selected span. Candidate rectangle placement remains
  explicitly unsupported. Automated synthetic composition is not evidence of a
  physical OS IME or mobile keyboard acceptance test.
* Audio supports playback, queueing, mixing and supported format conversion;
  recording and unsupported stream operations fail. Browser activation rules
  apply. Gamepads use the browser mapping, with at most eight devices.
* GPU programs use the WebGPU bindings and `sdl3webgpu.h` bridge, own their devices,
  and present into their VT. SDL_CreateGPUDevice, OpenGL, Vulkan and Metal are not
  included. No X-specific GPU broker is part of this contract.
* SDL helper modules (including image/font helpers) do not imply complete
  SDL_image or SDL_ttf compatibility.

## Package requirements

A new binary manifest keeps format 1 and adds:

```json
{
  "platform": {
    "target": "wasm32-wasm-posix",
    "minVersion": "1.0.0",
    "runtimeAbi": 5,
    "objectAbi": 3,
    "features": ["posix-core", "sdl3-vt"]
  },
  "dependencies": ["library"],
  "dependencyRequirements": {
    "library": {"version": "2.4.1-1"}
  }
}
```

`gucman platform` prints the running platform identity and available interfaces.

`objectAbi` is required by the publisher for objects/static libraries and optional
for executable-only packages. Available feature names are in `tools/platform.json`.
Dependency requirements accept exact `version`, exact manifest `sha256`, or stable
SemVer `minVersion`/`maxExclusive` bounds (all specified conditions must match).
Upstream package versions need not be SemVer when pinned exactly. Every dependency
in a new platform-aware manifest must have a requirement. The resolver checks the
whole selected graph before installing anything. Installed reverse dependencies
are checked too; recorded dependency manifest hashes conservatively require a
dependent to be reinstalled in the same transaction when those bytes change. This is validation of a selected
catalog graph, not a search solver choosing among multiple historical versions.
Legacy manifests retain their format and baseline mapping; publishers should add
explicit requirements when issuing new releases without rebuilding unchanged
binary bytes.

Catalog/object hashes and sizes remain mandatory integrity checks. Source recipes
are trusted guest programs, not a hostile-code sandbox. Existing package ownership,
transaction recovery and fixed output-set upgrade restrictions continue to apply.
