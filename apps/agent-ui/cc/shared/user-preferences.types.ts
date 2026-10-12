import { z } from 'zod/v4-mini';

export const UserPreferencesSchema = z.object({
  // The single project the home composer used to target. Kept as "the active
  // pin the composer currently targets" within pinnedProjectIds. For users who
  // predate pinnedProjectIds it is treated as the sole pin (backward compat).
  defaultProjectId: z.optional(z.nullable(z.string())),
  // The set of projects pinned to the home composer's project picker. When
  // absent, defaultProjectId (if any) is treated as the sole pin. null clears
  // the set via preferences/update. defaultProjectId names the active target
  // within this set.
  pinnedProjectIds: z.optional(z.nullable(z.array(z.string()))),
  voiceAutoSubmit: z.optional(z.boolean()),
  voiceSilenceAutoStop: z.optional(z.boolean()),
  // Speech-to-text engine: 'web-speech' (browser, on-device, free, default) or
  // 'elevenlabs-scribe' (server-proxied ElevenLabs Scribe — more accurate,
  // works where Web Speech is weak/absent, e.g. mobile Safari/Firefox).
  sttMechanism: z.optional(z.enum(['web-speech', 'elevenlabs-scribe'])),
  // Hard per-turn ceiling in hours, USER-WIDE (applies to every project). Any
  // turn older than this is stopped unconditionally by the watchdog sweep — no
  // probe, no triage. Absent/null = off (null clears it via preferences/update).
  // The UI suggests 3–6h. See notes/plans/cc-stalled-chat-watchdog.md.
  turnCeilingHours: z.optional(z.nullable(z.number())),
  // Turn-end notification channels, USER-WIDE. The per-chat notifyOnTurnEnd
  // flag decides WHETHER a finished turn notifies; these decide HOW (push via
  // netguc-push, email via SMTP). Absent = on, preserving the both-channels
  // behavior from before these existed. They gate ONLY turn-end notifications
  // — failure/exception alerts (parked-draft hold, stall watchdog) are
  // deliberately not covered: a global channel-off must never
  // silently suppress a something-went-wrong alert.
  turnEndNotifyPush: z.optional(z.boolean()),
  turnEndNotifyEmail: z.optional(z.boolean()),
  // When true, a turn-end notification fires ONLY if the chat has no
  // outstanding wake handles it is watching (active/firing — see
  // wake.types.ts). A coordinator thread that just registered
  // `cc-meta wait --async` isn't done, it's waiting — the notification that
  // matters is "idle with NOTHING pending, come kick me off". Absent/false =
  // notify on every turn-end (the pre-pref behavior).
  turnEndNotifyOnlyWhenIdle: z.optional(z.boolean()),
  // When true, an agent-initiated `cc-meta notify` push also sends an email
  // mirror (same title/body). Absent/false = push only.
  notifyAlsoEmail: z.optional(z.boolean()),
  // Labels auto-applied to every thread created through the meta listener
  // (`cc-meta new`) — every thread on that path is agent-created, so no flag
  // is needed. Stale/deleted ids are skipped at apply time, never an error.
  // null clears the set via preferences/update.
  agentThreadLabelIds: z.optional(z.nullable(z.array(z.string()))),
  // Auto-archive threads created through the meta listener: they drop out of
  // the inbox at creation (still findable via labels / --filter all). The
  // intended pairing with agentThreadLabelIds — inbox stays clean, threads
  // stay findable.
  agentThreadAutoArchive: z.optional(z.boolean()),
  // Reveal private (hidden-from-default-list) chats INLINE in the normal inbox
  // and all-chats/list surfaces. Default absent/false = the always-hide behavior
  // (private chats reachable only via the Advanced Search page). When true, the
  // list RPCs pass includePrivate through so private chats appear in the normal
  // lists — a persistent "show private" mode. Advanced Search and cc-meta are
  // unaffected (Advanced Search is the always-on reveal regardless of this).
  showPrivateChats: z.optional(z.boolean()),
  // Hide the signed-in user's email address from the app header (top bar).
  // Absent/false = the header shows it (the pre-pref behavior). This is app
  // CHROME privacy, not a secret: the User section of the settings page always
  // shows the address regardless, so the user can still see which account they
  // are on. Server-backed (not localStorage) because it should follow the user
  // to every device they screen-share/record from.
  hideEmailInHeader: z.optional(z.boolean()),
  // Executor-picker display, USER-WIDE (server-backed like hideEmailInHeader
  // so the choice follows the user to every device). Two independent toggles:
  // showDeprecatedExecutors — the account-suffixed 'claude-code-<account>'
  // executors are deprecated (see project.types.ts isDeprecatedExecutor) and
  // HIDDEN from the executor pickers by default; true re-shows them. A
  // currently-selected deprecated executor is always shown in its own picker
  // regardless, so an existing project/thread never renders an empty or
  // silently-changed selection.
  showDeprecatedExecutors: z.optional(z.boolean()),
  // executorDisplayAliases — show the short account aliases ('gpt'/'pub'/
  // 'ham', the one table in frontend lib/executor-display.ts) in place of the
  // full account names in the executor/account select lists. Absent = ON (the
  // aliased display is the default); false = raw names.
  executorDisplayAliases: z.optional(z.boolean()),
  // A personal, reusable library of "common prompts" surfaced by a picker in
  // the chat composer (all three surfaces) and CRUD-managed on the settings
  // page. Selecting one drops its text into the composer input for the user to
  // edit/send. Personal reusable content expected on every device, so it lives
  // here (server-side) rather than in localStorage. null clears the set via
  // preferences/update.
  savedPrompts: z.optional(
    z.nullable(
      z.array(z.object({ id: z.string(), title: z.string(), text: z.string() })),
    ),
  ),
  // Context-length nudge config, USER-WIDE (the turnCeilingHours precedent for
  // user-wide turn governance). `enabled` is the single GLOBAL DEFAULT for
  // every thread: absent = ON (nudge everything); false = off, only per-chat
  // force-on threads nudge. Effective per-thread decision =
  // `chatSettings.contextNudge ?? enabled` (lib/context-nudge.ts
  // isNudgeEligible; codex carved out of default-on). The old master-ref /
  // `allThreads` gating was removed 2026-07-25 — a stored `allThreads` key on
  // legacy rows is stripped by this non-strict schema, never an error.
  // Threshold resolution (all in lib/context-nudge.ts resolveThresholds,
  // which takes the prefs AND the thread's effective model — the %-of-window
  // upgrade had to add that parameter, it could not change only the function
  // body as an earlier revision of this comment claimed): an explicit absolute
  // softTokens/firmTokens wins; else, when the effective model has a
  // MODEL_CONTEXT_WINDOWS entry, softPct/firmPct (defaults 45/75) of that
  // window CAPPED at the absolute defaults (the min-cap — the absolutes are a
  // policy ceiling on thread size, see resolveThresholds); else the absolute
  // defaults (90k/150k) from
  // shared/context-nudge.types.ts. Sub-keys are nullable on the wire: preferences/update
  // deep-merges contextNudge (project-service.ts), and null deletes a sub-key
  // (stored bodies never hold null) — so one toggle write can't clobber the
  // sibling thresholds.
  // The repeat* sub-keys tune the band-2 REPEAT cadence (the curve is
  // documented in shared/context-nudge.types.ts): `repeatBaseTokens` is the
  // interval right at the firm threshold, `repeatHalvingTokens` is how many
  // tokens above firm halve that interval, and `repeatFloorTokens` is the
  // smallest interval the curve ever reaches (the floor is what makes
  // "nudge on every turn" reachable for a very large thread). Absent = the
  // fraction-of-firm defaults, so a retuned firm moves the whole curve. Same
  // absolute-token style, same null-deletes-a-sub-key merge as soft/firm. A
  // user who wants the old two-shot behaviour back sets repeatBaseTokens and
  // repeatFloorTokens above any context they will ever reach.
  contextNudge: z.optional(z.object({
    decisionSource: z.optional(z.nullable(z.enum(['thread-history', 'provider-context']))),
    historySoftTokens: z.optional(z.nullable(z.number())),
    historyFirmTokens: z.optional(z.nullable(z.number())),
    enabled: z.optional(z.nullable(z.boolean())),
    softTokens: z.optional(z.nullable(z.number())),
    firmTokens: z.optional(z.nullable(z.number())),
    // Percent-of-window overrides (defaults 45/75). Only consulted when the
    // thread's effective model has a known context window, and capped at the
    // absolute defaults (a pct can only LOWER a threshold — to raise one, set
    // the absolute pref, which wins outright).
    softPct: z.optional(z.nullable(z.number())),
    firmPct: z.optional(z.nullable(z.number())),
    repeatBaseTokens: z.optional(z.nullable(z.number())),
    repeatHalvingTokens: z.optional(z.nullable(z.number())),
    repeatFloorTokens: z.optional(z.nullable(z.number())),
  })),
});

export type UserPreferences = z.infer<typeof UserPreferencesSchema>;
