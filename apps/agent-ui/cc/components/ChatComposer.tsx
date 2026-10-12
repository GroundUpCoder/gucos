import { useState, useEffect, useCallback, useRef, type ReactNode, type FormEvent, type KeyboardEvent } from 'react';
import { Link } from 'react-router-dom';
import { Send, Loader2, Mic, Coffee, X, Clock, BookMarked, BookmarkPlus } from 'lucide-react';
import { toast } from 'sonner';
import {
  DropdownMenu,
  DropdownMenuTrigger,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuSeparator,
} from './ui/dropdown-menu.js';
import { useSpeechRecognition } from '../hooks/useSpeechRecognition.js';
import { useVoiceWakeLock } from '../hooks/use-voice-wake-lock.js';
import { useVoiceRecorder } from '../hooks/useVoiceRecorder.js';
import { useKeepScreenAwake } from '../hooks/use-keep-screen-awake.js';
import { useWakeLockAcquire } from '../contexts/WakeLockContext.js';
import type { ImagePasteApi } from '../hooks/use-image-paste.js';
import { useIsMobile } from '../hooks/use-mobile.js';
import { defaultPromptTitle } from '../lib/saved-prompts.js';
import { getAuthHeaders } from '../rpc/rpc-client.js';
import type { AudioContent, UserContent, UserContentBlock } from '../shared/ai.types.js';
import type { UserPreferences } from '../shared/user-preferences.types.js';

type SttMechanism = NonNullable<UserPreferences['sttMechanism']>;

/** A saved "common prompt" — a personal, reusable prompt text surfaced by the
 *  composer's prompt picker. Stored server-side in UserPreferences.savedPrompts. */
export type SavedPrompt = NonNullable<UserPreferences['savedPrompts']>[number];

/** Transcribe a recorded audio blob via the server proxy (POST /api/transcribe
 *  → ElevenLabs Scribe). The ElevenLabs key never reaches the browser. The
 *  server also RETAINS the recording and (additively) returns its media
 *  metadata; older/stub servers that return only { text } yield media: null. */
async function transcribeViaProxy(blob: Blob, mimeType: string): Promise<{
  text: string;
  media: AudioContent | null;
}> {
  const contentType = mimeType.split(';')[0];
  const resp = await fetch('/api/transcribe', {
    method: 'POST',
    headers: { 'Content-Type': contentType, ...(await getAuthHeaders()) },
    credentials: 'include',
    body: blob,
  });
  if (!resp.ok) {
    const err = await resp.json().catch(() => ({ error: 'Transcription failed' }));
    throw new Error(`${err.error || 'Transcription failed'} [sent ${blob.size}B as ${contentType}]`);
  }
  const data = await resp.json() as {
    text: string;
    media?: { mediaId: string; mediaType: string; byteSize: number; durationMs?: number };
  };
  const media: AudioContent | null = data.media
    ? {
        mediaId: data.media.mediaId,
        mediaType: data.media.mediaType,
        byteSize: data.media.byteSize,
        ...(typeof data.media.durationMs === 'number' ? { durationMs: data.media.durationMs } : {}),
      }
    : null;
  return { text: (data.text || '').trim(), media };
}

export interface ChatComposerProps {
  initialDraft?: string;
  onDraftChange?: (value: string) => void;
  /** Called when user submits. Component clears input and manages sending state. */
  onSubmit: (content: UserContent) => Promise<void>;
  /** When provided, shows a "Park" button that saves the message without sending.
   *  Component clears input and manages its own parking state, same as onSubmit. */
  onPark?: (content: UserContent) => Promise<void>;
  /** Disables textarea and submit button entirely (e.g. no project configured) */
  disabled?: boolean;
  /** Placeholder text for the textarea */
  placeholder?: string;
  /** Optional label shown above the textarea (e.g. project name) */
  label?: string;
  /** Interactive node rendered in place of the plain `label` span (e.g. a
   *  project picker dropdown). Takes precedence over `label` when provided. */
  labelSlot?: ReactNode;
  /** Content rendered between the spacer and submit button (e.g. Stop button) */
  extraActions?: ReactNode;
  /** Custom content for the submit button. Default: spinner when sending, Send icon otherwise */
  submitContent?: ReactNode;
  /** Additional disabled condition for submit button (OR'd with internal checks) */
  submitDisabled?: boolean;
  /** Submit button tooltip */
  submitTitle?: string;
  /** Prefix for data-testid attributes on inner elements */
  testId?: string;
  /** When this value changes, the textarea is focused (desktop only). Pass a chat/project ID. */
  focusDependency?: unknown;
  /** When true, automatically submits after voice transcription completes (instead of just populating the textarea). */
  autoSubmitAfterVoice?: boolean;
  /** When true, the mic auto-stops after 2s of silence (no need to tap the mic again). */
  voiceSilenceAutoStop?: boolean;
  /** Which speech-to-text engine to use. Default: 'web-speech' (browser, on-device). */
  sttMechanism?: SttMechanism;
  /** Personal reusable "common prompts". When non-empty, a prompt-picker button
   *  appears in the composer; selecting one drops its text into the input. */
  savedPrompts?: SavedPrompt[];
  /** When provided, a "Save to prompts" button appears; clicking it opens an
   *  inline title editor and, on confirm, calls this with the composer's current
   *  text. The PARENT owns persistence (mirrors how `savedPrompts` is a prop, not
   *  owned here) so the new prompt can be appended to that composer's picker
   *  immediately. Throwing rejects with a user-facing message (surfaced as a toast). */
  onSavePrompt?: (prompt: { title: string; text: string }) => Promise<void>;
  /** The image-paste/drop state this composer shows and sends. The PAGE owner
   *  creates the single `useImagePaste()` instance (also used by its drag
   *  overlay) and passes it here, so a dropped image lands in the SAME
   *  `pendingImages` this composer renders and `buildContent()` sends. Passing
   *  it in (rather than the composer calling `useImagePaste()` itself) is what
   *  makes drag-drop attach work — otherwise the overlay and composer would own
   *  two isolated instances and the drop would be a dead-end. Paste stays wired
   *  via this same instance's `handlePaste` on the textarea. */
  imagePaste: ImagePasteApi;
}

export function ChatComposer({
  onSubmit,
  initialDraft = '',
  onDraftChange,
  onPark,
  disabled = false,
  placeholder,
  label,
  labelSlot,
  extraActions,
  submitContent,
  submitDisabled = false,
  submitTitle = 'Send',
  testId = 'chat',
  focusDependency,
  autoSubmitAfterVoice = false,
  voiceSilenceAutoStop = false,
  sttMechanism = 'web-speech',
  savedPrompts,
  onSavePrompt,
  imagePaste,
}: ChatComposerProps) {
  const isMobile = useIsMobile();
  const useWebSpeech = sttMechanism === 'web-speech';
  const [input, setInput] = useState(initialDraft);
  const draftCallback = useRef(onDraftChange);draftCallback.current = onDraftChange;
  useEffect(() => { draftCallback.current?.(input); }, [input]);
  const [sending, setSending] = useState(false);
  const [parking, setParking] = useState(false);
  const [transcribing, setTranscribing] = useState(false);
  // Inline "Save to prompts" editor state: whether the title popover is open,
  // the (editable, pre-filled) title, and whether the save RPC is in flight.
  const [savePromptOpen, setSavePromptOpen] = useState(false);
  const [savePromptTitle, setSavePromptTitle] = useState('');
  const [savingPrompt, setSavingPrompt] = useState(false);
  // Both hooks are always called (React rules); we branch on sttMechanism at
  // event-handler time. Silence auto-stop is a web-speech-only concept (the
  // recorder has no interim/silence detection).
  const { begin: beginVoiceWakeLock, end: endVoiceWakeLock } = useVoiceWakeLock();
  const voice = useSpeechRecognition({ silenceAutoStopMs: useWebSpeech && voiceSilenceAutoStop ? 2000 : 0, onEnd: endVoiceWakeLock });
  const recorder = useVoiceRecorder(endVoiceWakeLock);
  // Manual "keep screen awake" toggle, surfaced as a button next to the mic
  // controls. Defaults OFF. When enabled, acquires a screen wake lock eagerly
  // inside the click handler (iOS Safari requires a user gesture), then the
  // app-wide hook in AppLayout maintains it across visibility changes.
  const [keepScreenAwake, setKeepScreenAwake] = useKeepScreenAwake();
  const acquireWakeLock = useWakeLockAcquire();
  const voiceBaseRef = useRef('');
  const stoppingManuallyRef = useRef(false);
  const prevVoiceStateRef = useRef(voice.state);
  // Retained voice recordings pending attachment to the NEXT send/park from
  // THIS composer instance. Held in a ref (source of truth — the
  // auto-submit-after-voice path runs inside stale closures, which a ref is
  // immune to) mirrored by a count in state (so send/park button eligibility
  // re-renders; there is deliberately no other chrome).
  // Rules (the documented, predictable re-record behavior):
  //   - every successful transcription that the server retained APPENDS its
  //     recording here, in order — recording twice attaches two clips, just
  //     as the transcripts concatenate in the text;
  //   - all pending recordings ride the next send OR park and are cleared
  //     with the text/images; on a FAILED send/park they are restored, so a
  //     retry re-attaches them (the recording itself is archival
  //     server-side either way);
  //   - pending audio alone makes send/park eligible (audio-only messages),
  //     but auto-submit still requires transcript text — an empty transcript
  //     never auto-sends;
  //   - a failed transcription attaches nothing;
  //   - a cancelled recording (send while recording) is discarded entirely.
  // Scoped to this component instance, so audio can never leak into another
  // tab or chat — there is no backend guessing about attachment targets.
  const pendingAudioRef = useRef<AudioContent[]>([]);
  const [pendingAudioCount, setPendingAudioCount] = useState(0);
  const setPendingAudio = useCallback((next: AudioContent[]) => {
    pendingAudioRef.current = next;
    setPendingAudioCount(next.length);
  }, []);
  const { pendingImages, handlePaste, removePendingImage, clearPendingImages, buildContent } = imagePaste;
  const textareaRef = useRef<HTMLTextAreaElement>(null);

  // Merge the pending recordings into outgoing content and clear them — the
  // ONE place audio joins a message, shared by send, park, and both
  // auto-submit paths. Returns the taken clips so a failed submit can
  // restore them (restore prepends, so anything recorded during the failed
  // attempt keeps its relative order).
  const takePendingAudio = useCallback((content: UserContent): { content: UserContent; taken: AudioContent[] } => {
    const taken = pendingAudioRef.current;
    if (taken.length === 0) return { content, taken };
    setPendingAudio([]);
    const blocks: UserContentBlock[] = typeof content === 'string'
      ? (content.length > 0 ? [{ type: 'text', text: content }] : [])
      : [...content];
    for (const a of taken) blocks.push({ type: 'audio', audio: a });
    return { content: blocks, taken };
  }, [setPendingAudio]);

  const restorePendingAudio = useCallback((taken: AudioContent[]) => {
    if (taken.length > 0) setPendingAudio([...taken, ...pendingAudioRef.current]);
  }, [setPendingAudio]);

  // Auto-resize textarea — useEffect so it also fires when voice dictation
  // streams text in via setInput (which doesn't raise a change event).
  useEffect(() => {
    const ta = textareaRef.current;
    if (!ta) return;
    ta.style.height = 'auto';
    ta.style.height = `${ta.scrollHeight}px`;
  }, [input]);

  // Auto-resize the voice display div (used instead of textarea during listening).
  const voiceDisplayRef = useRef<HTMLDivElement>(null);
  useEffect(() => {
    const el = voiceDisplayRef.current;
    if (!el) return;
    el.style.height = 'auto';
    el.style.height = `${el.scrollHeight}px`;
  }, [input, voice.tentativeText]);

  // Focus textarea when focusDependency changes (desktop only)
  useEffect(() => {
    if (focusDependency !== undefined && !isMobile) {
      textareaRef.current?.focus();
    }
  }, [focusDependency, isMobile]);

  // Voice (web-speech): stream FINAL text into the textarea while listening.
  // Only finalized (committed) text goes in the textarea; tentative
  // text is rendered separately so the user can see what might change.
  useEffect(() => {
    if (useWebSpeech && voice.state === 'listening') {
      const base = voiceBaseRef.current.trim();
      const final = voice.finalText;
      setInput(base ? `${base} ${final}` : final);
    }
  }, [voice.finalText, voice.state, useWebSpeech]);

  // Voice: surface errors from either engine as toasts.
  useEffect(() => {
    if (voice.error) toast.error(voice.error);
  }, [voice.error]);
  useEffect(() => {
    if (recorder.error) toast.error(recorder.error);
  }, [recorder.error]);

  // Handle auto-stop: when the silence timer fires and recognition ends on
  // its own (rather than the user tapping the mic), finalize the text into
  // the textarea and optionally auto-submit.
  useEffect(() => {
    const wasListening = prevVoiceStateRef.current === 'listening';
    const nowIdle = voice.state === 'idle';
    prevVoiceStateRef.current = voice.state;

    if (!useWebSpeech) return;
    if (!wasListening || !nowIdle) return;

    // Manual stop (user tapped the mic): handleVoice owns finalizing and
    // submitting. Consume the flag here so this effect doesn't also submit.
    // Previously handleVoice reset the flag itself right after stopListening()
    // resolved (a microtask) — which ran before this effect, so the guard was
    // already false by the time the effect checked it, causing a double submit.
    if (stoppingManuallyRef.current) {
      stoppingManuallyRef.current = false;
      return;
    }

    // Auto-stop (silence timer ended recognition on its own): finalize the
    // text into the textarea and optionally auto-submit.
    const base = voiceBaseRef.current.trim();
    const finalText = voice.finalText;
    const fullText = base ? `${base} ${finalText}` : finalText;
    if (fullText) setInput(fullText);

    if (autoSubmitAfterVoice && !disabled && (fullText.trim() || pendingImages.length > 0)) {
      const { content, taken } = takePendingAudio(buildContent(fullText.trim()));
      clearPendingImages();
      setInput('');
      setSending(true);
      onSubmit(content)
        .catch(err => {
          restorePendingAudio(taken);
          setInput(cur => (cur === '' ? fullText : cur));
          toast.error(err instanceof Error ? err.message : 'Failed to send');
        })
        .finally(() => setSending(false));
    }
  }, [voice.state, voice.finalText, autoSubmitAfterVoice, disabled, pendingImages.length, buildContent, takePendingAudio, restorePendingAudio, clearPendingImages, onSubmit, useWebSpeech]);

  // Drop merged dictation text into the composer and, if auto-submit is on,
  // send it immediately. Shared by both STT engines' completion paths.
  const finalizeDictation = useCallback(async (fullText: string) => {
    setInput(fullText);
    if (autoSubmitAfterVoice && !disabled && (fullText.trim() || pendingImages.length > 0)) {
      const { content, taken } = takePendingAudio(buildContent(fullText.trim()));
      clearPendingImages();
      setInput('');
      setSending(true);
      try {
        await onSubmit(content);
      } catch (err) {
        // Retry state: restore the audio (and the transcript, if the box is
        // still empty) so the user can just hit send again.
        restorePendingAudio(taken);
        setInput(cur => (cur === '' ? fullText : cur));
        toast.error(err instanceof Error ? err.message : 'Failed to send');
      } finally {
        setSending(false);
      }
    }
  }, [autoSubmitAfterVoice, disabled, pendingImages.length, buildContent, takePendingAudio, restorePendingAudio, clearPendingImages, onSubmit]);

  const handleVoice = useCallback(async () => {
    // ── web-speech path: live interim streaming, manual or silence stop ──
    if (useWebSpeech) {
      if (voice.state === 'idle') {
        voiceBaseRef.current = input;
        beginVoiceWakeLock();
        voice.startListening();
      } else if (voice.state === 'listening') {
        // The idle-transition effect resets stoppingManuallyRef once it observes
        // the listening→idle transition — don't reset it here (that raced with
        // the effect and caused a double submission).
        stoppingManuallyRef.current = true;
        setTranscribing(true);
        const final = await voice.stopListening();
        setTranscribing(false);
        if (final) {
          const base = voiceBaseRef.current.trim();
          await finalizeDictation(base ? `${base} ${final}` : final);
        }
      }
      return;
    }

    // ── elevenlabs-scribe path: record → stop → proxy-transcribe ──
    if (recorder.state === 'idle') {
      voiceBaseRef.current = input;
      beginVoiceWakeLock();
      await recorder.startRecording();
    } else if (recorder.state === 'recording') {
      const result = await recorder.stopRecording();
      if (!result) return;
      setTranscribing(true);
      try {
        const { text, media } = await transcribeViaProxy(result.blob, result.mimeType);
        // The server retained the recording — queue it for the next send/park
        // (see pendingAudioRef's rules). Appended BEFORE finalizeDictation so
        // the auto-submit path already carries it.
        if (media) setPendingAudio([...pendingAudioRef.current, media]);
        const base = voiceBaseRef.current.trim();
        if (text) await finalizeDictation(base ? `${base} ${text}` : text);
      } catch (err) {
        toast.error(err instanceof Error ? err.message : 'Transcription failed');
      } finally {
        setTranscribing(false);
      }
    }
  }, [beginVoiceWakeLock, useWebSpeech, voice.state, voice.startListening, voice.stopListening, recorder.state, recorder.startRecording, recorder.stopRecording, input, finalizeDictation, setPendingAudio]);

  const doSubmit = useCallback(async () => {
    if (disabled) return;
    if (!input.trim() && pendingImages.length === 0 && pendingAudioRef.current.length === 0) return;
    // Stop dictation on send — the streamed text is already in `input`;
    // without this the still-running recognizer's next interim result would
    // resurrect the old text into the just-cleared composer. The recorder is
    // cancelled too so an in-progress recording is discarded, not transcribed.
    if (voice.state === 'listening') voice.cancel();
    if (recorder.state === 'recording' || recorder.state === 'requesting') recorder.cancel();
    const sentText = input.trim();
    const { content, taken } = takePendingAudio(buildContent(sentText));
    setInput('');
    clearPendingImages();
    setSending(true);
    try {
      await onSubmit(content);
    } catch (err) {
      restorePendingAudio(taken);
      setInput(cur => (cur === '' ? sentText : cur));
      toast.error(err instanceof Error ? err.message : 'Failed to send');
    } finally {
      setSending(false);
    }
  }, [disabled, input, pendingImages, voice, recorder, buildContent, takePendingAudio, restorePendingAudio, clearPendingImages, onSubmit]);

  // Park: same content-building/clearing as doSubmit, but hands off to onPark
  // (which persists without sending) instead of onSubmit.
  const doPark = useCallback(async () => {
    if (disabled || !onPark) return;
    if (!input.trim() && pendingImages.length === 0 && pendingAudioRef.current.length === 0) return;
    if (voice.state === 'listening') voice.cancel();
    if (recorder.state === 'recording' || recorder.state === 'requesting') recorder.cancel();
    const parkedText = input.trim();
    const { content, taken } = takePendingAudio(buildContent(parkedText));
    setInput('');
    clearPendingImages();
    setParking(true);
    try {
      await onPark(content);
    } catch (err) {
      restorePendingAudio(taken);
      setInput(cur => (cur === '' ? parkedText : cur));
      toast.error(err instanceof Error ? err.message : 'Failed to park');
    } finally {
      setParking(false);
    }
  }, [disabled, onPark, input, pendingImages, voice, recorder, buildContent, takePendingAudio, restorePendingAudio, clearPendingImages]);

  // Open the inline "Save to prompts" editor, pre-filling the title from the
  // current input (first line / ~40 chars — editable, since the picker lists by
  // title). No-op if there's nothing to save.
  const openSavePrompt = useCallback(() => {
    const text = input.trim();
    if (!text) return;
    setSavePromptTitle(defaultPromptTitle(text));
    setSavePromptOpen(true);
  }, [input]);

  // Confirm the save: hand the current text + chosen title to the parent (which
  // owns persistence). On success close the editor; on failure surface the
  // parent's user-facing message (cap hit, length, etc.) and keep it open.
  const confirmSavePrompt = useCallback(async () => {
    if (!onSavePrompt) return;
    const text = input.trim();
    const title = savePromptTitle.trim();
    if (!text || !title) return;
    setSavingPrompt(true);
    try {
      await onSavePrompt({ title, text });
      setSavePromptOpen(false);
      toast.success('Saved to prompts');
    } catch (err) {
      toast.error(err instanceof Error ? err.message : 'Could not save prompt');
    } finally {
      setSavingPrompt(false);
    }
  }, [onSavePrompt, input, savePromptTitle]);

  const handleSubmit = useCallback((e: FormEvent) => {
    e.preventDefault();
    doSubmit();
  }, [doSubmit]);

  const handleKeyDown = useCallback((e: KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === 'Enter' && !isMobile && !e.shiftKey) {
      e.preventDefault();
      doSubmit();
    }
  }, [isMobile, doSubmit]);

  const defaultPlaceholder = isMobile
    ? 'Type a message...'
    : 'Type a message... (Paste images or Enter to send)';

  const hasContent = input.trim().length > 0 || pendingImages.length > 0 || pendingAudioCount > 0;
  const canSubmit = !sending && !parking && !submitDisabled && !disabled && hasContent;
  // Park stays available even while a turn runs (submitDisabled doesn't gate it),
  // so you can stash a follow-up without waiting for the current turn to finish.
  const canPark = !sending && !parking && !disabled && hasContent;

  // Mic UI state, unified across the two engines.
  const webSpeechListening = useWebSpeech && voice.state === 'listening';
  const recording = !useWebSpeech && recorder.state === 'recording';
  const requesting = !useWebSpeech && recorder.state === 'requesting';
  const micActive = webSpeechListening || recording; // red, pulsing
  // Show the mic only when the active engine can actually run here. web-speech
  // hides when unsupported (e.g. Firefox); elevenlabs needs MediaRecorder+mic.
  const micSupported = useWebSpeech ? voice.supported : recorder.supported;
  const micTitle =
    webSpeechListening ? 'Stop listening' :
    recording ? 'Stop recording' :
    requesting ? 'Requesting microphone…' :
    transcribing ? 'Transcribing…' :
    'Voice input';

  return (
    <form
      onSubmit={handleSubmit}
      autoComplete="off"
      className="p-2 md:p-3 border-t border-border flex flex-col shrink-0"
      data-testid={disabled ? `${testId}-disabled` : `${testId}-form`}
    >
      {labelSlot ? (
        <div className="px-1" data-testid={`${testId}-label`}>
          {labelSlot}
        </div>
      ) : label ? (
        <span className="text-xs text-muted-foreground px-1" data-testid={`${testId}-label`}>
          {label}
        </span>
      ) : null}
      {pendingImages.length > 0 && (
        <div className="flex gap-2 px-1 py-1 flex-wrap">
          {pendingImages.map((img, i) => (
            <div key={i} className="relative">
              <img
                src={`data:${img.mediaType};base64,${img.base64}`}
                alt=""
                data-testid={`${testId}-pending-image`}
                className="w-16 h-16 object-cover rounded border border-border"
              />
              <button
                type="button"
                onClick={() => removePendingImage(i)}
                className="absolute -top-1 -right-1 w-4 h-4 bg-destructive text-destructive-foreground rounded-full flex items-center justify-center"
              >
                <X className="w-3 h-3" />
              </button>
            </div>
          ))}
        </div>
      )}
      {webSpeechListening ? (
        /* During web-speech dictation: swap the textarea for a styled div so
           tentative text can be highlighted in-place (textarea can't do
           mixed styling). The textarea is readOnly during listening anyway.
           The elevenlabs path has no interim text, so it keeps the textarea. */
        <div
          ref={voiceDisplayRef}
          className={`w-full px-3 py-2.5 border rounded-md bg-background text-foreground border-border text-base min-h-0 max-h-[200px] overflow-y-auto whitespace-pre-wrap break-words ${
            disabled ? 'bg-muted text-muted-foreground cursor-not-allowed' : ''
          }`}
          data-testid={`${testId}-input`}
        >
          {voice.finalText
            ? (() => {
                const base = voiceBaseRef.current.trim();
                const display = base ? `${base} ${voice.finalText}` : voice.finalText;
                return <>{display}{voice.tentativeText ? ' ' : ''}</>;
              })()
            : (() => {
                const base = voiceBaseRef.current.trim();
                return base ? <>{base} </> : null;
              })()}
          {voice.tentativeText && (
            <span className="bg-amber-200/60 dark:bg-amber-800/40 rounded-sm">
              {voice.tentativeText}
            </span>
          )}
          {!voice.finalText && !voice.tentativeText && !voiceBaseRef.current.trim() && (
            <span className="text-muted-foreground/50">{placeholder ?? defaultPlaceholder}</span>
          )}
        </div>
      ) : (
        <textarea
          ref={textareaRef}
          value={input}
          onChange={e => setInput(e.target.value)}
          onKeyDown={handleKeyDown}
          onPaste={handlePaste}
          placeholder={placeholder ?? defaultPlaceholder}
          rows={1}
          readOnly={disabled}
          disabled={disabled}
          className={`w-full px-3 py-2.5 border rounded-md bg-background text-foreground border-border focus:outline-none focus:ring-2 focus:ring-ring resize-none text-base min-h-0 max-h-[200px] overflow-y-auto ${
            disabled ? 'bg-muted text-muted-foreground cursor-not-allowed' : ''
          }`}
          data-testid={`${testId}-input`}
        />
      )}
      <div className="flex items-center gap-2">
        {/* Voice input button — web-speech (on-device) or elevenlabs-scribe
            (record → server proxy → ElevenLabs), per the user's STT setting. */}
        {micSupported && (
          <button
            type="button"
            onClick={handleVoice}
            disabled={transcribing || requesting}
            data-testid={`${testId}-voice`}
            title={micTitle}
            className={`p-2 rounded-md transition-colors shrink-0 ${
              micActive
                ? 'bg-red-100 dark:bg-red-950 text-red-600 dark:text-red-400 hover:bg-red-200 dark:hover:bg-red-900'
                : 'text-muted-foreground hover:text-foreground hover:bg-muted/50'
            } disabled:opacity-50`}
            aria-label={micActive ? micTitle : 'Start voice input'}
          >
            {transcribing || requesting ? (
              <Loader2 className="w-5 h-5 animate-spin" />
            ) : micActive ? (
              <span className="relative inline-flex">
                <Mic className="w-5 h-5" />
                <span className="absolute -top-0.5 -right-0.5 w-2.5 h-2.5 bg-red-500 rounded-full animate-pulse" />
              </span>
            ) : (
              <Mic className="w-5 h-5" />
            )}
          </button>
        )}

        {/* Keep-screen-awake toggle — holds a screen wake lock so the device
            doesn't dim/lock during dictation or long reads. Manual, defaults off. */}
        <button
          type="button"
          onClick={() => {
            const next = !keepScreenAwake;
            setKeepScreenAwake(next);
            if (next) void acquireWakeLock();
          }}
          aria-pressed={keepScreenAwake}
          data-testid={`${testId}-keep-awake`}
          title={keepScreenAwake ? 'Screen stays awake (tap to allow sleep)' : 'Keep screen awake'}
          className={`p-2 rounded-md transition-colors shrink-0 ${
            keepScreenAwake
              ? 'text-primary hover:bg-muted/50'
              : 'text-muted-foreground hover:text-foreground hover:bg-muted/50'
          }`}
          aria-label={keepScreenAwake ? 'Allow screen to sleep' : 'Keep screen awake'}
        >
          <Coffee className="w-5 h-5" />
        </button>

        {/* Common prompts — a personal, reusable prompt library. Selecting an
            entry drops its text into the composer for the user to edit/send.
            Hidden while web-speech dictation is active (the textarea is swapped
            for a read-only display div, so setInput wouldn't be visible). Only
            rendered when the user actually has saved prompts. */}
        {!webSpeechListening && savedPrompts && savedPrompts.length > 0 && (
          <DropdownMenu>
            <DropdownMenuTrigger asChild>
              <button
                type="button"
                data-testid={`${testId}-prompts`}
                title="Common prompts"
                aria-label="Insert a saved prompt"
                className="p-2 rounded-md transition-colors shrink-0 text-muted-foreground hover:text-foreground hover:bg-muted/50"
              >
                <BookMarked className="w-5 h-5" />
              </button>
            </DropdownMenuTrigger>
            <DropdownMenuContent
              align="start"
              className="max-w-[min(20rem,90vw)]"
              data-testid={`${testId}-prompts-menu`}
            >
              {savedPrompts.map(prompt => (
                <DropdownMenuItem
                  key={prompt.id}
                  data-testid={`${testId}-prompt-${prompt.id}`}
                  onSelect={() => {
                    setInput(prompt.text);
                    // Radix restores focus to the trigger on close; defer so the
                    // textarea wins focus and the caret lands in the message box.
                    setTimeout(() => textareaRef.current?.focus(), 0);
                  }}
                >
                  <span className="truncate">{prompt.title}</span>
                </DropdownMenuItem>
              ))}
              <DropdownMenuSeparator />
              <DropdownMenuItem asChild>
                <Link to="/settings" data-testid={`${testId}-prompts-manage`}>
                  Manage prompts…
                </Link>
              </DropdownMenuItem>
            </DropdownMenuContent>
          </DropdownMenu>
        )}

        <div className="flex-1" />

        {extraActions}

        {/* Save to prompts — capture the current input as a new common prompt
            without leaving the composer. Hidden during web-speech dictation
            (the input is a read-only display div then), disabled when empty.
            Opens an inline title editor; the parent owns persistence. */}
        {onSavePrompt && !webSpeechListening && (
          <div className="relative shrink-0">
            <button
              type="button"
              onClick={() => (savePromptOpen ? setSavePromptOpen(false) : openSavePrompt())}
              disabled={!hasContent || sending || parking}
              aria-expanded={savePromptOpen}
              data-testid={`${testId}-save-prompt`}
              title="Save to prompts"
              aria-label="Save this message as a common prompt"
              className="p-2 rounded-md transition-colors text-muted-foreground hover:text-foreground hover:bg-muted/50 disabled:opacity-50"
            >
              <BookmarkPlus className="w-5 h-5" />
            </button>
            {savePromptOpen && (
              <div
                className="absolute bottom-full right-0 mb-2 z-20 w-[min(20rem,80vw)] rounded-md border border-border bg-background p-2.5 shadow-lg space-y-2"
                data-testid={`${testId}-save-prompt-popover`}
              >
                <p className="text-xs text-muted-foreground">Save as a common prompt</p>
                <input
                  type="text"
                  value={savePromptTitle}
                  onChange={e => setSavePromptTitle(e.target.value)}
                  onKeyDown={e => {
                    if (e.key === 'Enter') { e.preventDefault(); void confirmSavePrompt(); }
                    if (e.key === 'Escape') { e.preventDefault(); setSavePromptOpen(false); }
                  }}
                  autoFocus
                  placeholder="Title (e.g. Code review)"
                  data-testid={`${testId}-save-prompt-title`}
                  className="w-full px-2.5 py-1.5 text-sm border border-border rounded-md bg-background focus:outline-none focus:ring-2 focus:ring-ring"
                />
                <div className="flex items-center justify-end gap-2">
                  <button
                    type="button"
                    onClick={() => setSavePromptOpen(false)}
                    disabled={savingPrompt}
                    data-testid={`${testId}-save-prompt-cancel`}
                    className="px-3 py-1.5 text-sm border border-border rounded-md hover:bg-muted/50 disabled:opacity-50"
                  >
                    Cancel
                  </button>
                  <button
                    type="button"
                    onClick={() => void confirmSavePrompt()}
                    disabled={savingPrompt || savePromptTitle.trim().length === 0}
                    data-testid={`${testId}-save-prompt-confirm`}
                    className="px-3 py-1.5 text-sm bg-primary text-primary-foreground rounded-md hover:opacity-90 disabled:opacity-50"
                  >
                    {savingPrompt ? 'Saving…' : 'Save'}
                  </button>
                </div>
              </div>
            )}
          </div>
        )}

        {onPark && (
          <button
            type="button"
            onClick={doPark}
            disabled={!canPark}
            className="px-3 py-2.5 border border-border text-foreground rounded-md hover:bg-muted/50 disabled:opacity-50 shrink-0"
            title="Park — save without sending"
            data-testid={`${testId}-park`}
          >
            {parking ? (
              <Loader2 className="w-4 h-4 animate-spin" />
            ) : (
              <Clock className="w-4 h-4" />
            )}
          </button>
        )}

        <button
          type="submit"
          disabled={!canSubmit}
          className="px-3 py-2.5 bg-primary text-primary-foreground rounded-md hover:opacity-90 disabled:opacity-50 shrink-0"
          title={submitTitle}
          data-testid={`${testId}-send`}
        >
          {submitContent ?? (sending ? (
            <Loader2 className="w-4 h-4 animate-spin" />
          ) : (
            <Send className="w-4 h-4" />
          ))}
        </button>
      </div>
    </form>
  );
}
