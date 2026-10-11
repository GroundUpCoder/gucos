# Cloudflare Pages deployment

The `gucos` Pages project serves the core at https://os.groundupcoder.com/.
Packages are fetched only by explicit browser operations from the public
GroundUpCoder/gucos-packages repository. No package checkout is a build or
deployment input. No Netguc server, tunnel or Caddy installation is needed.

Run commands from this repository root. A normal update uploads an already
validated release to the existing project; it does not recreate the project or
change DNS. Building, pushing Git commits or publishing an application package
does not deploy the platform.

## Build and validate

Use Node 22 or newer, pnpm and Chromium with JSPI/shared memory. Install the
pinned tooling with `pnpm install --frozen-lockfile`; install Chromium with
`pnpm exec playwright install chromium` when absent.

Check `git status --short` and `tools/versions.json` before building. Record the
intended source commit and platform version. If libc headers/sources changed,
run `node tools/libc-manifest.mjs` and include the regenerated manifest in the
source commit. Run the tests appropriate to the change as well as the core
smoke/package checks below. Use a new release directory for each candidate.

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

On the maintainer's Mac, previous deployments load the existing token from
`~/.guc/creds/cloudflare-api-token`; a missing shell variable or Wrangler login
does not mean deployment credentials are absent. The existing account is
`2946eccd828b5588231dd11a300d3e7f`, project `gucos`. Pass the token only through
the child process environment, without printing or committing it:

```python
import os, pathlib, subprocess
env = dict(os.environ)
env['CLOUDFLARE_ACCOUNT_ID'] = '2946eccd828b5588231dd11a300d3e7f'
env['CLOUDFLARE_API_TOKEN'] = (
    pathlib.Path.home() / '.guc/creds/cloudflare-api-token'
).read_text().strip()
subprocess.run([
    'node', 'tools/deploy-pages.mjs', 'releases/RELEASE_NAME', '--preview'
], env=env, check=True)
```

This is the method used by migration thread
`01a12917-afca-7fbe-9205-04011b0b93e6` on 2026-10-11 and verified again on
2026-10-12. After preview acceptance, omit `--preview` to upload production.
If authentication fails, check that this file exists and that its token can
access the existing project before asking for new credentials. Do not print the
token while diagnosing access. Environment assignments in the Python example
apply only to its child process; repeat that wrapper for the production upload.

## Preview, production and acceptance

1. Export and retain the candidate directory, including `release.json`,
   `pages-release.json` and `build.json`. Upload it with `--preview` using the
   credential wrapper above. Retain the deployment output and its exact URL.
2. Validate that exact preview URL, not only the movable preview alias. Read
   the expected runtime hash from the **exported release's** `build.json`:

   ```sh
   release_dir=releases/RELEASE_NAME
   release_runtime=$(node -e 'console.log(JSON.parse(require("fs").readFileSync(process.argv[1])).runtime)' "$release_dir/build.json")
   LIVE_CORE_URL=https://DEPLOYMENT_ID.gucos.pages.dev \
     node tools/machine-job.mjs run --kind browser --timeout 1800 -- \
     node tests/live-core.mjs "$release_runtime"
   ```

   `live-core` checks runtime identity, cross-origin isolation, actual public
   package binary installation and source build, reload persistence, terminal
   recovery and two GPU applications. It uses fresh browser profiles. Also run
   release-specific acceptance on the preview; for the terminal patch:

   ```sh
   LIVE_CORE_URL=https://DEPLOYMENT_ID.gucos.pages.dev EXPECTED_RUNTIME="$release_runtime" \
     node tools/machine-job.mjs run --kind browser --timeout 1800 -- \
     sh -c 'node tests/terminal-echo.mjs && node tests/terminal-gucode.mjs'
   ```

3. Check HTTP delivery against `pages-release.json`: public file sizes/SHA256s,
   COOP `same-origin`, COEP `require-corp`, CORP `same-origin`, immutable caching
   for `/assets/*`, and `no-cache` for entry documents. `_headers` is Pages
   configuration, not a public file. Pages serves `index.html` at `/` and
   redirects `.html` URLs to extensionless paths. Missing objects and the
   operator manifests `/release.json` and `/pages-release.json` must return 404.
4. Only after preview checks pass, upload the **same retained directory** using
   the credential wrapper without `--preview`. Do not rebuild between preview
   and production. Retain the production deployment ID and previous accepted
   deployment/release for rollback.
5. Repeat the HTTP and browser checks against `https://os.groundupcoder.com`:

   ```sh
   LIVE_CORE_URL=https://os.groundupcoder.com EXPECTED_RUNTIME="$release_runtime" \
     node tools/machine-job.mjs run --kind browser --timeout 1800 -- \
     sh -c 'node tests/terminal-echo.mjs && node tests/terminal-gucode.mjs && node tests/live-core.mjs "$EXPECTED_RUNTIME"'
   ```

   The terminal/gucode checks are specific to this patch; retain `live-core`
   and substitute relevant tests for subsequent changes. Confirm the actual
   custom-domain runtime, not merely an upload-success message. An immediate
   HTTP failure can occur during propagation; inspect it and repeat the full
   audit before claiming success. Roll back if acceptance remains broken.
6. Record the source commit, platform/runtime identity, manifest hashes,
   deployment IDs, test commands/results and tested limits under `docs/`.
   Push the source/evidence and tag the accepted implementation. Mark a release
   deployed only after production acceptance passes.

The complete [1.0.2 release evidence](research/terminal-echo-1.0.2/README.md)
includes preview/production logs, Cloudflare receipts and HTTP audits. Its
`verify-http.py` is a historical verifier pinned to that release directory and
runtime; update those inputs before reusing it for a different release.

The exporter creates `site/` using only the verified current asset inventory,
adds isolation/cache headers and an explicit 404 page, and rejects assets over
25 MiB. The deployment tool verifies payload hashes and rejects unexpected
files. Only `site/` is uploaded; the operator's `release.json` is not public.
Content-addressed assets are immutable; entry documents revalidate. No Pages
Functions are needed. The first deployment uses direct upload, not Git builds.

For initial setup only: associate os.groundupcoder.com with this Pages project
before creating its CNAME to gucos.pages.dev. This is already configured.
Preserve the existing apex site and all other DNS records.

## Rollback and data

Retain release directories and deployment IDs. Use the Pages dashboard's rollback
on a previously verified production deployment, or re-upload its retained release
with this tool. Then repeat the public HTTP/header and live-core checks.
For a re-upload rollback, use the same credential wrapper with the previous
release directory and no `--preview`. Verify the previous release's expected
runtime, record the rollback deployment ID, and do not treat the failed
candidate as accepted or move its tag onto another implementation.

The `.com` hostname has its own browser storage origin. Existing `.net` disks are
not transferred or rewritten; the old site remains available for access to them.
Disk/ABI identifiers remain unchanged within each origin. Preview origins have
separate storage too. This migration does not implement disk export/import.
