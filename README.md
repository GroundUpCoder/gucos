# gucos

A standalone browser OS with a persistent filesystem, BusyBox shell, C compiler,
libc, GNU Make, archive tools and HTTP downloads. Applications are discovered,
built and installed by the browser from configured package repositories.

The application boundary and compatibility rules are documented in
[the platform contract](docs/PLATFORM.md).

Build and serve with Node (tested with 25.8.2):

```sh
node tools/build.mjs
node tools/serve.mjs
```

No npm install, package checkout or network access is required by the core build.
Open the printed local URL in a Chromium browser supporting JSPI and shared memory.
The server supplies the required isolation headers. The generated `dist/` contains
the OS, bootstrap source offers and license notices.

Inside the OS:

```sh
gucman repo list
gucman search jsmn
gucman build groundupcoder:jsmn
gucman install groundupcoder:jsmn
gucman repo add community https://example.org/catalog.json
gucman search editor
gucman installed
```

The default is GroundUpCoder/gucos-packages on raw GitHub. Anonymous delivery
requires that repository to be public. A missing repository does not prevent core
boot or execution of installed programs. `install` requires an explicitly published
compatible binary; `build` executes trusted source recipes with access to guest
files. Neither silently falls back to the other. Duplicate names require SOURCE:NAME.

Source settings persist under `/etc/gucman/sources.d/`. Removing a source preserves
installed files. See `docs/PACKAGES.md` for the wire format and current limits.

Automated browser tests need `pnpm install --frozen-lockfile` and Playwright Chromium.
Use the machine scheduler where installed:

```sh
machine-job run --kind browser --timeout 1800 -- node tests/smoke.mjs
machine-job run --kind browser --timeout 1800 -- node tests/runtime-packages.mjs
machine-job run --kind browser --timeout 1800 -- node tests/storage-boundary.mjs
```

Deployment uses a dedicated Cloudflare Pages project. See
[deployment and rollback](docs/DEPLOYMENT.md) and the
[migration record](docs/MIGRATION.md). Package publication is independent.
