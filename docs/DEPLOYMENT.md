# Cloudflare Pages deployment

The `gucos` Pages project serves the core at https://os.groundupcoder.com/.
Packages are fetched only by explicit browser operations from the public
GroundUpCoder/gucos-packages repository. No package checkout is a build or
deployment input. No Netguc server, tunnel or Caddy installation is needed.

## Build and validate

Use Node 22 or newer, pnpm and Chromium with JSPI/shared memory. Install the
pinned tooling with `pnpm install --frozen-lockfile`; install Chromium with
`pnpm exec playwright install chromium` when absent.

```
node tools/machine-job.mjs run --kind build --lane wasm-posix --timeout 1800 -- node tools/build.mjs
node tools/machine-job.mjs run --kind browser --timeout 1800 -- node tests/smoke.mjs
node tools/machine-job.mjs run --kind browser --timeout 1800 -- node tests/runtime-packages.mjs
node tools/export-pages.mjs dist releases/RELEASE_NAME
```

The scheduler is included in this repository. It shares the existing machine-wide
queue and permits one browser job at a time. It does not read another checkout.
Build output retains complete bootstrap source offers and license notices.

## Upload

Supply `CLOUDFLARE_ACCOUNT_ID` and `CLOUDFLARE_API_TOKEN` through the environment.
The token needs account Pages Write. Domain setup additionally needs DNS edit on
groundupcoder.com. Never commit tokens. Project name defaults to `gucos` and can
be set through `CLOUDFLARE_PAGES_PROJECT`.

```
node tools/deploy-pages.mjs releases/RELEASE_NAME --preview
# Run live-core with LIVE_CORE_URL set to the preview origin and build.json runtime.
node tools/deploy-pages.mjs releases/RELEASE_NAME
node tools/machine-job.mjs run --kind browser --timeout 1800 -- node tests/live-core.mjs RUNTIME_SHA256
```

The exporter creates `site/` using only the verified current asset inventory,
adds isolation/cache headers and an explicit 404 page, and rejects assets over
25 MiB. The deployment tool verifies payload hashes and rejects unexpected
files. Only `site/` is uploaded; the operator's `release.json` is not public.
Content-addressed assets are immutable; entry documents revalidate. No Pages
Functions are needed. The first deployment uses direct upload, not Git builds.

Associate os.groundupcoder.com with this Pages project before creating its CNAME
to gucos.pages.dev. Preserve the existing apex site and all other DNS records.

## Rollback and data

Retain release directories and deployment IDs. Use the Pages dashboard's rollback
on a previously verified production deployment, or re-upload its retained release
with this tool. Then repeat the public HTTP/header and live-core checks.

The `.com` hostname has its own browser storage origin. Existing `.net` disks are
not transferred or rewritten; the old site remains available for access to them.
Disk/ABI identifiers remain unchanged within each origin. Preview origins have
separate storage too. This migration does not implement disk export/import.
