# gucOS 1.0.3 agent and terminal VT acceptance

Candidate runtime:
`89250c7b2d43fbb9845431a61e01f7a06cb5b7a1da6c026193a60c578bd6cf9d`.

The complete final local suite passed under the shared browser scheduler:

```
node tools/machine-job.mjs run --kind browser --timeout 1800 -- sh -c \
  'node tools/build.mjs && node tests/agent-acceptance.mjs local-shipping'
```

`local-shipping/` contains logs and screenshots for the exact candidate:

- Kernel terminal isolation and existing terminal echo/fault probes.
- Core smoke, compiler, Make, archive, repository binary/source installation.
- Upgrade from the retained accepted 1.0.2 release: user marker, executable,
  DSO, object/archive hashes and relinking, plus a second reload.
- Two SDL VTs, resize, keyboard release, hidden progress and exit/crash recovery.
- Two independent shell VTs, shared files, terminal close cleanup.
- Actual cc rendering and collapse controls, missing-key UX, real shell tools
  with controlled provider SSE, flat history/draft persistence, reload,
  continuation and backup import with a distinct ID.
- Stop during an actual shell command, reload during another command, explicit
  unknown-result restoration and no repeated side effects.
- Authenticated direct-browser DeepSeek streaming and actual Bash file
  creation/readback, using an isolated guest credential file. No credential,
  browser profile or guest disk is retained in this evidence.

`release.json` records retained release/manifest identities, the credential
scan and previous production rollback target. `upgrade.json` is the final
candidate upgrade result. `cc-source-provenance.json` identifies all copied
cc files by upstream commit and SHA256, including the explicit adapter list.
`cors-preflight.txt` is an unauthenticated live preflight response.

Earlier candidate evidence is deliberately retained under `local/`,
`local-final/`, `local-release/`, `live-provider/`, and
`upgrade-initial-8fbb.json`. In particular, `local/virtual-terminals.log`
records the taskbar interception failure repaired before final acceptance.
These earlier artifacts are not evidence for the shipped runtime.

Preview and production validation have not yet been recorded at this source
commit. Deployment will upload the same retained directory after exact-preview
HTTP and browser acceptance; subsequent evidence commits record the receipts.
