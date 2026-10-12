// Speech recognition hook wrapping the browser's built-in Web Speech API
// (SpeechRecognition / webkitSpeechRecognition) — browser-native STT with no
// backend route and no API key. Chrome/Edge support it (on-device in recent
// Chrome, otherwise the browser's speech service); Safari 14.1+ exposes
// webkitSpeechRecognition backed by Siri dictation. Firefox has no support —
// consumers hide the mic via `supported`.
//
// State machine: idle → listening → (stopListening returns final text) → idle
//
// Recognition runs in continuous mode: it listens until the user taps the mic
// again (or the engine gives up after a long silence), so pauses between
// sentences don't end the session. Interim results are exposed via
// `interimText` so the consumer can stream live text into the composer while
// the user speaks.

import { useCallback, useEffect, useRef, useState } from "react";

// TypeScript doesn't include Web Speech API types in its DOM lib.
// Declare the subset we need.
interface SpeechRecognition extends EventTarget {
  continuous: boolean;
  interimResults: boolean;
  lang: string;
  onstart: ((this: SpeechRecognition, ev: Event) => any) | null;
  onend: ((this: SpeechRecognition, ev: Event) => any) | null;
  onerror: ((this: SpeechRecognition, ev: SpeechRecognitionErrorEvent) => any) | null;
  onresult: ((this: SpeechRecognition, ev: SpeechRecognitionEvent) => any) | null;
  start(): void;
  stop(): void;
  abort(): void;
}

interface SpeechRecognitionEvent extends Event {
  readonly resultIndex: number;
  readonly results: SpeechRecognitionResultList;
}

interface SpeechRecognitionErrorEvent extends Event {
  readonly error: string;
  readonly message: string;
}

declare var SpeechRecognition: {
  prototype: SpeechRecognition;
  new (): SpeechRecognition;
};

declare var webkitSpeechRecognition: {
  prototype: SpeechRecognition;
  new (): SpeechRecognition;
};

export type RecognitionState = "idle" | "listening";

export interface UseSpeechRecognitionOptions {
  /** Called when recognition ends, is cancelled, or fails to start. */
  onEnd?: () => void;
  /** Auto-stop after this many ms of silence. 0 or undefined = disabled (manual stop only). */
  silenceAutoStopMs?: number;
}

export interface SpeechRecognitionHook {
  state: RecognitionState;
  /** Whether the browser supports SpeechRecognition. */
  supported: boolean;
  error: string | null;
  /** Full combined transcript (final + tentative). Empty when idle. */
  interimText: string;
  /** Only finalized (committed) portions of the transcript. */
  finalText: string;
  /** Only tentative (non-finalized, may change) portion of the transcript. */
  tentativeText: string;
  /** Request mic permission and start listening. */
  startListening: () => void;
  /** Stop listening and return the final recognized text. */
  stopListening: () => Promise<string>;
  /** Abort listening without returning text. */
  cancel: () => void;
}

// Browser compatibility: standard name or Apple's legacy prefix.
const SpeechRecognitionCtor: typeof SpeechRecognition | null =
  (window as any).SpeechRecognition || (window as any).webkitSpeechRecognition || null;

export function useSpeechRecognition(options: UseSpeechRecognitionOptions = {}): SpeechRecognitionHook {
  const { silenceAutoStopMs = 0 } = options;
  const onEndRef = useRef(options.onEnd);
  onEndRef.current = options.onEnd;
  const [state, setState] = useState<RecognitionState>("idle");
  const [error, setError] = useState<string | null>(null);
  const [interimText, setInterimText] = useState("");
  const [finalText, setFinalText] = useState("");
  const [tentativeText, setTentativeText] = useState("");

  const recognitionRef = useRef<SpeechRecognition | null>(null);
  const resolveRef = useRef<((text: string) => void) | null>(null);
  // startListening is memoized with [] deps, so its inner onresult would
  // otherwise close over silenceAutoStopMs from the FIRST render. The
  // preference that controls it loads asynchronously after mount, so that
  // first-render value is always 0 — i.e. silence auto-stop would never arm.
  // Keep the latest value in a ref and read it at timer-arming time instead.
  const silenceAutoStopMsRef = useRef(silenceAutoStopMs);
  silenceAutoStopMsRef.current = silenceAutoStopMs;
  const finalTextRef = useRef("");
  // Mirrors the latest tentative (non-finalized) transcript. onresult keeps it
  // in sync — and clears it to "" once the engine finalizes that text — so we
  // can commit it on stop without double-counting text already in finalTextRef.
  const tentativeTextRef = useRef("");
  const silenceTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  const supported = SpeechRecognitionCtor !== null;

  const clearSilenceTimer = () => {
    if (silenceTimerRef.current) {
      clearTimeout(silenceTimerRef.current);
      silenceTimerRef.current = null;
    }
  };

  // Clean up on unmount.
  useEffect(() => {
    return () => {
      clearSilenceTimer();
      recognitionRef.current?.abort();
      onEndRef.current?.();
    };
  }, []);

  const startListening = useCallback(() => {
    if (!SpeechRecognitionCtor) {
      setError(
        "Speech recognition is not supported in this browser. Use Chrome or Edge.",
      );
      onEndRef.current?.();
      return;
    }

    setError(null);
    setInterimText("");
    setFinalText("");
    setTentativeText("");
    finalTextRef.current = "";
    tentativeTextRef.current = "";

    const recognition = new SpeechRecognitionCtor();
    recognition.interimResults = true;
    // Continuous: keep listening across pauses until the user taps stop.
    // Without it the engine finalizes after the first silence, which is
    // useless for dictating multi-sentence prompts.
    recognition.continuous = true;
    recognition.lang = ""; // let the browser auto-detect

    recognition.onstart = () => {
      setState("listening");
    };

    recognition.onresult = (event: SpeechRecognitionEvent) => {
      let interim = "";
      for (let i = event.resultIndex; i < event.results.length; i++) {
        const result = event.results[i];
        if (result.isFinal) {
          const transcript = result[0].transcript;
          const existing = finalTextRef.current;
          // Android Chrome (and some other Chromium-based browsers)
          // sometimes returns cumulative transcripts: each final result
          // contains the full session text, not just the new phrase.
          // Detect this: if the transcript starts with what we already
          // have and is longer, it's cumulative — use the full transcript
          // instead of blindly appending (which would duplicate text).
          if (existing && transcript.length > existing.length && transcript.startsWith(existing)) {
            finalTextRef.current = transcript;
          } else {
            finalTextRef.current += transcript;
          }
        } else {
          interim += result[0].transcript;
        }
      }
      tentativeTextRef.current = interim;
      setInterimText(finalTextRef.current + interim);
      setFinalText(finalTextRef.current);
      setTentativeText(interim);

      // Silence auto-stop (only when enabled via options). Read the latest
      // value from the ref — see silenceAutoStopMsRef declaration above.
      const silenceMs = silenceAutoStopMsRef.current;
      if (silenceMs > 0) {
        clearSilenceTimer();
        silenceTimerRef.current = setTimeout(() => {
          const recognition = recognitionRef.current;
          if (recognition) {
            // Set up a pending resolve so onend delivers the text.
            // This follows the same path as a manual stopListening().
            if (!resolveRef.current) {
              resolveRef.current = () => {};
            }
            recognition.stop();
          }
        }, silenceMs);
      }
    };

    recognition.onerror = (event: SpeechRecognitionErrorEvent) => {
      // "no-speech" fires when the user doesn't say anything — not a real
      // error, just means silence was detected. Let onend handle it.
      if (event.error === "no-speech") return;

      const messages: Record<string, string> = {
        "not-allowed":
          "Microphone access was denied. Enable it in your browser settings and try again.",
        "audio-capture":
          "No microphone found. Connect a microphone and try again.",
        "network":
          "Speech recognition requires a network connection.",
        "service-not-allowed":
          "Speech recognition is blocked by your browser settings.",
      };
      const msg =
        messages[event.error] ||
        `Speech recognition error: ${event.error}`;
      setError(msg);
    };

    recognition.onend = () => {
      clearSilenceTimer();
      // Commit any text still tentative when recognition ends. Stopping before
      // the engine finalizes the last phrase (or an engine that doesn't
      // finalize on stop) would otherwise silently drop it. tentativeTextRef is
      // "" once the engine has finalized that text, so this never duplicates.
      if (tentativeTextRef.current) {
        finalTextRef.current += tentativeTextRef.current;
        tentativeTextRef.current = "";
      }
      setState("idle");
      onEndRef.current?.();
      setTentativeText("");
      setInterimText(finalTextRef.current);
      setFinalText(finalTextRef.current);
      // If we have a pending resolve (user called stopListening or auto-stop),
      // resolve it with the final text.
      if (resolveRef.current) {
        const text = finalTextRef.current.trim();
        resolveRef.current(text);
        resolveRef.current = null;
      }
      recognitionRef.current = null;
    };

    recognitionRef.current = recognition;
    try {
      recognition.start();
    } catch (err) {
      recognitionRef.current = null;
      setError(err instanceof Error ? err.message : "Could not start speech recognition");
      onEndRef.current?.();
    }
  }, []);

  const stopListening = useCallback(() => {
    return new Promise<string>((resolve) => {
      const recognition = recognitionRef.current;
      if (!recognition) {
        resolve("");
        return;
      }
      resolveRef.current = resolve;
      recognition.stop();
    });
  }, []);

  const cancel = useCallback(() => {
    clearSilenceTimer();
    const recognition = recognitionRef.current;
    if (recognition) {
      recognition.onend = null; // prevent onend from firing resolve
      recognition.abort();
      recognitionRef.current = null;
    }
    resolveRef.current = null;
    finalTextRef.current = "";
    tentativeTextRef.current = "";
    setInterimText("");
    setFinalText("");
    setTentativeText("");
    setState("idle");
    onEndRef.current?.();
  }, []);

  return {
    state,
    supported,
    error,
    interimText,
    finalText,
    tentativeText,
    startListening,
    stopListening,
    cancel,
  };
}
