# Runtime package repositories

Core builds and hosting deployments have no dependency on a packages checkout.
The browser reads configured catalogs only for explicit package operations.
`gucman repo add NAME URL`, `repo remove NAME`, and `repo list` manage persistent
sources. URLs are HTTPS (loopback HTTP is allowed for development). Catalogs and
objects are fetched without credentials. The default raw GitHub catalog is
`https://raw.githubusercontent.com/GroundUpCoder/wasm-posix-packages/main/catalog.json`.

A catalog has `format:1` and `packages`, an object mapping names to refs with
`url`, `sha256`, and `size`. Publishers should use immutable commit-addressed URLs.
Each selected manifest is hash/size checked. One command snapshots source settings
and catalogs; no mid-command catalog refresh occurs. A configured source outage is
reported, and mutations refuse partial discovery. Search shows available results
but exits nonzero if any source failed. Search is a literal case-sensitive substring.

A manifest has `format:1`, `name`, `version`, `dependencies`, `files`, `build`,
`test`, `outputs`, `licenses`, and optionally `runtime` and `binary`.
Names use lowercase letters, digits, dots, plus and hyphen. Files contain `path`
plus URL/hash/size refs. Recipe and test are paths among those input files.
Outputs contain prefix-relative `path` and decimal `mode` 420 or 493. Licenses list
output paths beneath `share/licenses/NAME/`; at least one is required.
Binary is a URL/hash/size ref to a USTAR archive of prefix-relative outputs.
The runtime must equal the current OS runtime for binary installation.

Build runs declared inputs in a private source directory with `DESTDIR` staging
and `PREFIX=/usr/local`. The test runs before publication and should test staged
outputs via `$DESTDIR`. Recipes can use guest shell, cc, Make and declared build
inputs; raw upstream archives can be declared as ordinary files and unpacked by
the trusted recipe. Host C++ translation must arrive as explicitly prepared inputs.
Recipes execute as the guest user, with access to all their files. This is not a
hostile-code sandbox. Adding a source trusts its catalog; explicit build trusts its
recipe. Hashes prove correspondence to that catalog, not independent author identity.

Dependencies are package names from the declaring source unless qualified with
SOURCE:NAME. Only configured sources are usable. Cycles and mixed origins for the
same package are refused. V1 uses exact selected manifest refs, not version-range
solving. All closure paths are checked before executing recipes. Dependencies
commit first and remain installed if a later consumer fails.

Publication is per package. Public files are owned symlinks into a retained release
through an atomic current link. Unmanaged files and other package owners are refused.
V1 updates must retain the same output path set; removals and changing path sets need
future explicit transaction support. Source identity is the exact catalog URL,
not its local label. Changing source URL cannot silently replace an installed package.

A publication journal plus a global lock blocks interrupted transactions. After
confirming the original guest process is no longer alive, `gucman recover` completes
a prepared transaction. It refuses live lock owners or incomplete/corrupt preparation
requiring inspection. Old releases remain; there is no automatic garbage collection.
Trusted recipes may themselves change guest files: publication rollback does not
undo arbitrary recipe effects.

Object downloads are capped at 256MiB, catalogs at 8MiB, with 30-second header and
idle deadlines. Redirect final URLs must not downgrade HTTPS. There is no network
proxy in the OS host. Upstream and artifact endpoints must permit browser CORS.
The initial catalog is source-only jsmn; a prebuilt requires separately publishing
its binary ref. Runtime changes currently invalidate binary compatibility conservatively.
