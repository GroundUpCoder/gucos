# GNU Make 4.4.1

Source: https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz

SHA-256: `dd16fb1d67bfab79a72f5e8390735c49e3e8e70b4945a15ab1f81ddb78658fb3`

GNU Make is GPL-3.0-or-later; see COPYING. The bundled glob implementation
and header are LGPL-2.0-or-later; see COPYING.LIB and their source notices.
All bundled source files retain their upstream notices.

`bin.json` bootstraps make with the browser compiler. `config.h` describes
the available wasm-posix libc: fork/exec, signals, second-resolution file
timestamps, archive support, and the pipe jobserver. Guile, dynamic modules,
localization and the FIFO jobserver are disabled. The bundled getloadavg
fallback cannot enforce host load limits on this browser platform.

Source changes:

- `src/posixos.c`: retain the result of duplicating the jobserver pipe in a
  local. SIGCHLD may close/reset `job_rfd` during `fd_noinherit`; that is
  the normal wakeup protocol, not a failed `dup`. The deterministic regression
  compiles the actual helper and simulates that delivery during the fd call.

- `src/makeint.h`: declare `version_string` and `make_host` as pointers to
  const char, matching upstream `version.c`. The whole-program compiler
  checks cross-translation-unit declarations.
- `lib/glob.h`: copied verbatim from upstream `lib/glob.in.h`. Make uses its
  bundled glob implementation for GLOB_ALTDIRFUNC and directory caching.

Run `make --version` in the browser shell. The launcher builds and caches
make on first use. `npm run test:builds` checks incremental dependency
rebuilds, recursive jobserver limits with output synchronization, zlib's
upstream configure/build/test flow, and pigz built against that zlib.
