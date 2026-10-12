// Adapted from gucchat/backend/src/shared/ai.types.ts
// Key change: AssistantMessage uses contentBlocks ONLY (no flat content string)
import { z } from 'zod/v4-mini';

export const ImageSourceSchema = z.object({
  type: z.literal('base64'),
  media_type: z.enum(['image/jpeg', 'image/png', 'image/gif', 'image/webp']),
  data: z.string(),
});

// A retained voice recording attached to a user message. The block carries a
// REFERENCE (mediaId) into the server-side media store (DATA_DIR/media/v1 +
// the `media` metadata table — see lib/media-service.ts), never the bytes:
// the transcript rides the normal text block beside it, and the UI streams
// the audio from the authenticated GET /api/media/:mediaId route. Audio
// blocks are NEVER forwarded to any executor (buildStdinMessage omits them;
// codex/title/judge projections read only text blocks). durationMs is set
// only when reliably known — Chrome MediaRecorder webm has no header
// duration, so it is usually absent.
export const AudioContentSchema = z.object({
  mediaId: z.string(),
  /** Content type of the stored object (e.g. audio/webm, audio/mp4). */
  mediaType: z.string(),
  byteSize: z.number(),
  durationMs: z.optional(z.number()),
});

export const UserContentBlockSchema = z.union([
  z.object({ type: z.literal('text'), text: z.string() }),
  z.object({ type: z.literal('image'), source: ImageSourceSchema }),
  z.object({ type: z.literal('audio'), audio: AudioContentSchema }),
]);

export const UserContentSchema = z.union([
  z.string(),
  z.array(UserContentBlockSchema),
]);

// Set on user messages auto-sent from a parked draft by the best-judgement
// judge at turn-end (the user wrote the draft, but the SEND decision — and for
// kind='modified' the final text — was the judge's). Drives the in-thread
// badge; `reason` preserves the judge's rationale for posterity. See
// shared/parked.types.ts and lib/parked-send-service.ts.
export const UserMessageParkedSendSchema = z.object({
  parkedId: z.string(),
  kind: z.enum(['verbatim', 'modified']),
  reason: z.string(),
});

// Set on user messages sent through the meta thread-tools API (the `cc-meta
// send` script — a meta agent messaging this thread, never typed by the human).
// Drives the in-thread badge; `note` preserves the sender's stated intent.
// The origin fields are FORWARD provenance ("this message came from thread A"
// — the badge deep-links there); the reverse index is the provenance-edge
// store (shared/provenance.types.ts). EVERY field here must stay z.optional:
// saveMessages validates with MessageSchema and a required field would make
// every legacy metaSend message fail validation (see chat-service.ts).
export const UserMessageMetaSendSchema = z.object({
  note: z.optional(z.string()),
  /** Chat id of the thread whose turn invoked cc-meta (CC_CHAT_ID). Absent on
   *  legacy messages and on sends from outside a cc turn. */
  originChatId: z.optional(z.string()),
  originProjectId: z.optional(z.string()),
  /** How this message arrived: a send into an existing thread, the kickoff of
   *  a thread the origin created, the kickoff of a thread the origin redirected
   *  into via tailcall (origin is being continued here), or an `auto kick`
   *  nudge that revived a stalled/orphaned idle thread. */
  relation: z.optional(z.enum(['send', 'create', 'tailcall', 'kick'])),
});

// Set on the triggering user message of a turn where the MASTER context-length
// nudge fired (see lib/context-nudge.ts). Purely a UI marker — the actual
// reminder is a TRANSIENT prepend to the outgoing turn content, never persisted
// as a message. A turn-start info block would be clobbered by the first stream
// event (chat-runner-service.ts replaces the assistant contentBlocks array), so
// the marker rides the user message like metaSend/parkedSend. Every field
// z.optional: saveMessages validates whole chat files, so a required field would
// wedge any legacy chat once it grew one of these.
export const UserMessageContextNudgeSchema = z.object({
  source: z.optional(z.enum(['thread-history', 'provider-context'])),
  /** 1 = soft band, 2 = firm band. */
  band: z.optional(z.number()),
  /** The measured context tokens that triggered the nudge. */
  tokens: z.optional(z.number()),
  /** The exact reminder text sent to the model (audit / badge tooltip). */
  text: z.optional(z.string()),
  /** True when this was a band-2 REPEAT on the escalating cadence rather than
   *  a fresh band crossing. Drives the "repeat" marker on the badge. */
  repeat: z.optional(z.boolean()),
  /** Growth in tokens since the previous fired nudge (repeats only). */
  growthTokens: z.optional(z.number()),
});

// Set on user messages injected by a fired wake handle (a watched thread went
// idle and this turn is the registered one-shot callback — never typed by the
// human). Drives the in-thread badge. See shared/wake.types.ts.
export const UserMessageWakeSchema = z.object({
  handleId: z.string(),
  targetChatId: z.string(),
  /** WakeOutcome of the target turn ('completed'|'stopped'|'failed'|'orphaned'),
   *  or 'timer' for a timer wake (no target turn exists). */
  outcome: z.string(),
  /** Wake kind ('timer' for a fired timer; absent = chatIdle). Drives the
   *  badge label. Optional like every marker field — saveMessages validates
   *  whole chat files. */
  kind: z.optional(z.string()),
  note: z.optional(z.string()),
});

// Set on user messages injected when a user's email reply to a decision request
// arrives (the email-decision channel — an emailReply wake fired, never typed by
// the human). Drives the in-thread badge. See shared/wake.types.ts and
// lib/email-decision-poller.ts. Every field z.optional like UserMessageMetaSend:
// saveMessages validates whole chat files, so a required field would wedge any
// legacy chat once it grew one of these.
export const UserMessageEmailReplySchema = z.object({
  handleId: z.optional(z.string()),
  /** The correlation token the reply matched. */
  token: z.optional(z.string()),
  /** The reply's From (already sender-verified against the request recipient). */
  from: z.optional(z.string()),
  /** The reply's Subject. */
  subject: z.optional(z.string()),
  /** The reply's own Message-ID, for provenance. */
  messageId: z.optional(z.string()),
});

// Set on the corrective user message the background-task guard auto-sends
// after aborting a turn that tried to background a process (see
// lib/background-task-guard.ts + the wiring in chat-runner-service.ts's
// runStreamLoop). Never typed by the human. `detail` is the offending
// tool/command text, surfaced in the badge tooltip.
export const UserMessageGuardSchema = z.object({
  kind: z.literal('background-task'),
  detail: z.string(),
});

// On-demand plain-text summary of a long message, generated once (DeepSeek
// tool call, see lib/message-summary-service.ts) when the user first toggles
// the summary view, then persisted on the message and reused forever. Safe to
// cache indefinitely: messages are immutable once their turn ends.
export const MessageSummarySchema = z.object({
  text: z.string(),
  createdAt: z.number(),
  /** Provider/model that produced the summary, for posterity. */
  model: z.string(),
});

// Set on the fixed user-authored reply that Recovery v1 injects at turn-end
// when an owner-authored rule matched the agent's last message (see
// shared/recovery.types.ts and lib/recovery-service.ts). Never typed by the
// human. Drives the in-thread "Recovery" badge — and, load-bearing, it is one
// of the four loop controls: the turn this message starts is suppressed from
// firing recovery again.
//
// EVERY field is z.optional, exactly like UserMessageMetaSend: saveMessages
// validates the WHOLE chat file with MessageSchema, so a required field would
// wedge any chat that already carries one of these if a reader ever saw it
// first. For the same reason Recovery adds its own marker instead of a new
// `metaSend.relation` enum member — widening that enum is roll-forward-only.
export const UserMessageRecoverySchema = z.object({
  ruleId: z.optional(z.string()),
  ruleName: z.optional(z.string()),
  /** The assistant message whose text matched — the (message, rule) pair is
   *  the durable execution claim key. */
  sourceAssistantMessageId: z.optional(z.string()),
  triggeredAt: z.optional(z.number()),
});

export const UserMessageSchema = z.object({
  role: z.literal('user'),
  id: z.optional(z.uuidv7()),
  createdAt: z.number(),
  // Stamped on every saveMessages call while this message is the tail of the
  // list. For an assistant this converges on stream-completion time; for a
  // user message it tracks when it was last the active tip of the chat.
  updatedAt: z.optional(z.number()),
  content: UserContentSchema,
  deletedAt: z.optional(z.number()),
  queued: z.optional(z.boolean()),
  parkedSend: z.optional(UserMessageParkedSendSchema),
  metaSend: z.optional(UserMessageMetaSendSchema),
  contextNudge: z.optional(UserMessageContextNudgeSchema),
  wake: z.optional(UserMessageWakeSchema),
  emailReply: z.optional(UserMessageEmailReplySchema),
  guard: z.optional(UserMessageGuardSchema),
  recovery: z.optional(UserMessageRecoverySchema),
  /** Q1-only legacy parity evidence. It is inert transcript metadata and is
   * never consulted for execution or recovery. */
  shadowParity: z.optional(z.object({
    itemId: z.uuidv7(), ordinal: z.number().check(z.int(), z.nonnegative()),
    originKind: z.enum(['human', 'meta', 'wake', 'parked', 'autostart']),
    idleJobRunId: z.optional(z.uuidv7()), policyDigest: z.string(),
  })),
  summary: z.optional(MessageSummarySchema),
});

export const UsageSchema = z.object({
  inputTokens: z.number(),
  outputTokens: z.number(),
  cacheCreationTokens: z.optional(z.number()),
  /** 5-minute-TTL share of cacheCreationTokens, from the CLI result's
   *  usage.cache_creation. Turn-level; 5m + 1h can sum to LESS than
   *  cacheCreationTokens when result.usage under-reports a multi-step turn
   *  (modelUsage, the authoritative aggregate source, has no TTL split). */
  cacheCreation5mTokens: z.optional(z.number()),
  /** 1-hour-TTL share of cacheCreationTokens — Anthropic bills 1h writes at
   *  2× base input vs 1.25× for 5m (see shared/pricing.ts). */
  cacheCreation1hTokens: z.optional(z.number()),
  cacheReadInputTokens: z.optional(z.number()),
  totalTokens: z.optional(z.number()),
  /** The HONEST size of the context that was sent to the model on this turn:
   *  the last main-loop message_delta's input + cache-write + cache-read
   *  tokens (ONE API call's footprint), captured last-one-wins by the claude
   *  provider. Distinct from inputTokens+cacheRead, which the result event
   *  overwrites with modelUsage — the SUM across every API call in the turn
   *  (a long agentic turn overcounts ~10×). Consumed by the context-length
   *  nudge (lib/context-nudge.ts). Absent for codex turns (no per-call usage
   *  signal exists) and for messages written before this field. */
  contextTokens: z.optional(z.number()),
  /** Cost reported by the CLI (providerCostUsd pass-through). */
  providerCostUsd: z.optional(z.number()),
  /** Cost computed from token counts × known model list prices. */
  computedCostUsd: z.optional(z.number()),
  /** Web search requests from the CLI result event's server_tool_use. */
  webSearchRequests: z.optional(z.number()),
  /** Web fetch requests from the CLI result event's server_tool_use. */
  webFetchRequests: z.optional(z.number()),
  // Rate limit info from the CLI
  rateLimitUtilization: z.optional(z.number()),
  rateLimitResetsAt: z.optional(z.number()),
  rateLimitType: z.optional(z.string()),
});

export const ContentBlockSchema = z.union([
  z.object({
    type: z.literal('text'),
    content: z.string(),
  }),
  z.object({
    type: z.literal('info'),
    content: z.string(),
  }),
  z.object({
    type: z.literal('thinking'),
    content: z.string(),
    signature: z.optional(z.string()),
  }),
  z.object({
    type: z.literal('toolUse'),
    toolUseId: z.optional(z.string()),
    name: z.string(),
    input: z.string(),
  }),
  z.object({
    type: z.literal('toolResult'),
    toolUseId: z.optional(z.string()),
    content: z.string(),
    isError: z.optional(z.boolean()),
  }),
  // Fallback for unrecognized CLI events/subtypes/blocks — we don't want to
  // lose data when the Claude Code CLI adds new event shapes we haven't
  // taught the parser about. Instead of dropping them (or just warning),
  // we persist the raw event structure so it shows up in the thread.
  //
  // `data` holds the full, structured event. It is OPTIONAL: when the event
  // is too large to persist in full, `data` is omitted and a string-truncated
  // representation appears in `truncatedData` instead. Exactly one of
  // `data` / `truncatedData` will be present in practice.
  z.object({
    type: z.literal('unknown'),
    label: z.string(),
    data: z.optional(z.unknown()),
    truncatedData: z.optional(z.string()),
  }),
]);

export type ContentBlock = z.infer<typeof ContentBlockSchema>;

// Warnings can be plain strings (legacy / unpositioned) or objects with a position
// relative to contentBlocks. afterBlockIndex = -1 means "before all blocks";
// afterBlockIndex = contentBlocks.length - 1 means "at the end of the stream so far".
// Missing afterBlockIndex falls back to the bottom aggregate panel.
export const WarningSchema = z.union([
  z.string(),
  z.object({
    message: z.string(),
    afterBlockIndex: z.optional(z.number()),
    timestamp: z.optional(z.number()),
  }),
]);

export type Warning = z.infer<typeof WarningSchema>;

export const StopReasonSchema = z.enum(['stop', 'length', 'toolCall', 'refusal']);

export const AssistantQueueAttemptSchema = z.discriminatedUnion('phase', [
  z.object({ itemId: z.uuidv7(), attemptId: z.uuidv7(), phase: z.literal('streaming') }),
  z.object({
    itemId: z.uuidv7(), attemptId: z.uuidv7(), phase: z.literal('terminal'),
    terminalKind: z.enum(['completed', 'provider-error', 'stopped', 'refused']), terminalAt: z.number(),
  }),
]);

// Key difference from gucchat: contentBlocks is REQUIRED, no flat 'content' string.
// Claude Code always produces structured blocks.
export const AssistantMessageSchema = z.object({
  role: z.literal('assistant'),
  id: z.optional(z.uuidv7()),
  createdAt: z.number(),
  updatedAt: z.optional(z.number()),
  model: z.string(),
  usage: UsageSchema,
  contentBlocks: z.array(ContentBlockSchema),
  stopReason: StopReasonSchema,
  /** Q1 legacy-runner terminal proof for retry classification only. It is
   * written after the provider path terminates and is never a scheduler fact. */
  legacyExecution: z.optional(z.object({
    operationId: z.uuidv7(), terminalAt: z.number(),
    outcome: z.enum(['completed', 'provider-error', 'stopped']),
  })),
  claudeCodeSessionId: z.optional(z.string()),
  claudeCodeModel: z.optional(z.string()),
  /** The model name actually used to compute usage.computedCostUsd at
   *  write time. For non-Anthropic wrapper executors this is the provider's
   *  own model (e.g. deepseek-v4-pro), which can differ from claudeCodeModel.
   *  Persisted so the read-time recompute in getChatCost uses the SAME key
   *  write-time did, instead of diverging. */
  pricingModel: z.optional(z.string()),
  /** The Anthropic account (a human LABEL, never a token) that actually ran
   *  this turn — captured at turn time from the executor + the project's
   *  authIdentityAnthropic setting via resolveAnthropicAccount(). Persisted
   *  PER MESSAGE (not read live at render time) because authIdentityAnthropic
   *  is project-level and MUTABLE: a past turn must show the account that
   *  DID run it, not whatever the setting says now. 'default' | 'josephkimgpt'
   *  | 'josephkimpublic' | 'hornyassmain'; absent for executors with no
   *  meaningful Anthropic account (wrappers, codex, mock) and for old
   *  messages written before this field existed. */
  anthropicAccount: z.optional(z.string()),
  /** The Codex funding account (a human LABEL, never a token) that ran this
   *  turn — the codex twin of anthropicAccount, via resolveCodexAccount().
   *  'default' | a CODEX_ACCOUNTS entry; absent for non-codex executors and
   *  for codex messages written before this field existed (those ran under
   *  the default home). Captured per message for the same reason: the
   *  project default is mutable, and a fork could in principle pin
   *  differently from its source. */
  codexAccount: z.optional(z.string()),
  durationMs: z.optional(z.number()),
  numTurns: z.optional(z.number()),
  permissionDenials: z.optional(z.array(z.string())),
  warnings: z.optional(z.array(WarningSchema)),
  errors: z.optional(z.array(z.string())),
  deletedAt: z.optional(z.number()),
  summary: z.optional(MessageSummarySchema),
  // Dark Q0 compatibility field. No production writer emits it before Q2.
  queueAttempt: z.optional(AssistantQueueAttemptSchema),
});

export const MessageSchema = z.union([
  UserMessageSchema,
  AssistantMessageSchema,
]);

export type ImageSource = z.infer<typeof ImageSourceSchema>;
export type AudioContent = z.infer<typeof AudioContentSchema>;
export type UserContentBlock = z.infer<typeof UserContentBlockSchema>;
export type UserContent = z.infer<typeof UserContentSchema>;
export type UserMessageParkedSend = z.infer<typeof UserMessageParkedSendSchema>;
export type MessageSummary = z.infer<typeof MessageSummarySchema>;
export type UserMessageMetaSend = z.infer<typeof UserMessageMetaSendSchema>;
export type UserMessageContextNudge = z.infer<typeof UserMessageContextNudgeSchema>;
export type UserMessageWake = z.infer<typeof UserMessageWakeSchema>;
export type UserMessageEmailReply = z.infer<typeof UserMessageEmailReplySchema>;
export type UserMessageGuard = z.infer<typeof UserMessageGuardSchema>;
export type UserMessageRecovery = z.infer<typeof UserMessageRecoverySchema>;
export type UserMessage = z.infer<typeof UserMessageSchema>;
export type Usage = z.infer<typeof UsageSchema>;
export type StopReason = z.infer<typeof StopReasonSchema>;
export type AssistantMessage = z.infer<typeof AssistantMessageSchema>;
export type AssistantQueueAttempt = z.infer<typeof AssistantQueueAttemptSchema>;
export type Message = z.infer<typeof MessageSchema>;

export function getUserTextContent(content: UserContent): string {
  if (typeof content === 'string') return content;
  return content
    .filter((b): b is { type: 'text'; text: string } => b.type === 'text')
    .map(b => b.text)
    .join('\n');
}

export function userContentHasImages(content: UserContent): boolean {
  if (typeof content === 'string') return false;
  return content.some(b => b.type === 'image');
}

export function userContentAudioBlocks(
  content: UserContent,
): Extract<UserContentBlock, { type: 'audio' }>[] {
  if (typeof content === 'string') return [];
  return content.filter(
    (b): b is Extract<UserContentBlock, { type: 'audio' }> => b.type === 'audio',
  );
}

/** Replace the TEXT of a user message while preserving every non-text block
 *  (images, audio) in their original order. The single edit primitive shared
 *  by the parked best-judgement rewrite, the queued-message editor, and the
 *  parked-tray editor — so a text edit can never silently drop an attachment. */
export function replaceUserTextContent(content: UserContent, newText: string): UserContent {
  if (typeof content === 'string') return newText;
  const nonText = content.filter(b => b.type !== 'text');
  if (nonText.length === 0) return newText;
  return [{ type: 'text' as const, text: newText }, ...nonText];
}
