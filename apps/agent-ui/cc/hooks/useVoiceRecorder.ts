// Voice recorder hook wrapping the browser MediaRecorder API.
//
// State machine: idle → requesting → recording → (stopRecording returns blob)
// Error states surface permission denials and unsupported browsers cleanly.
//
// Safari produces audio/mp4 (AAC in MP4 container); Chrome produces
// audio/webm (Opus in WebM). Both are accepted by ElevenLabs Scribe.

import { useCallback, useEffect, useRef, useState } from "react";

export type RecorderState = "idle" | "requesting" | "recording";

export interface VoiceRecorder {
  state: RecorderState;
  /** Whether this browser can record audio (MediaRecorder + getUserMedia). */
  supported: boolean;
  error: string | null;
  /** Request mic permission and start recording. */
  startRecording: () => Promise<void>;
  /** Stop recording and return the audio blob + MIME type. */
  stopRecording: () => Promise<{ blob: Blob; mimeType: string } | null>;
  /** Cancel recording without producing a blob. */
  cancel: () => void;
}

/**
 * Returns the best supported audio MIME type for MediaRecorder on this
 * browser. Prefer MP4/AAC when available: iOS Safari can report WebM support
 * and successfully record Opus-in-WebM, yet its native audio element may stay
 * stuck loading that duration-less recording. Chrome/Firefox fall back to
 * WebM/Opus when they do not expose MP4 recording support.
 */
export function getSupportedMimeType(
  isTypeSupported: (mime: string) => boolean = (mime) => MediaRecorder.isTypeSupported(mime),
): string | null {
  if (typeof MediaRecorder === "undefined") return null;
  const preferred = [
    "audio/mp4",
    "audio/webm;codecs=opus",
    "audio/webm",
    "audio/mpeg",
  ];
  for (const mime of preferred) {
    if (isTypeSupported(mime)) return mime;
  }
  return null;
}

export function useVoiceRecorder(onEnd?: () => void): VoiceRecorder {
  const onEndRef = useRef(onEnd);
  onEndRef.current = onEnd;
  const generationRef = useRef(0);
  const [state, setState] = useState<RecorderState>("idle");
  const [error, setError] = useState<string | null>(null);

  // Refs held across the recording lifecycle — cleaned up on unmount.
  const streamRef = useRef<MediaStream | null>(null);
  const recorderRef = useRef<MediaRecorder | null>(null);
  const chunksRef = useRef<Blob[]>([]);
  const mimeTypeRef = useRef<string>("audio/webm");
  // Resolve the promise returned by stopRecording().
  const resolveRef = useRef<
    ((result: { blob: Blob; mimeType: string } | null) => void) | null
  >(null);

  const supported =
    typeof MediaRecorder !== "undefined" &&
    typeof navigator !== "undefined" &&
    !!navigator.mediaDevices?.getUserMedia &&
    getSupportedMimeType() !== null;

  // Clean up media on unmount.
  useEffect(() => {
    return () => {
      generationRef.current++;
      onEndRef.current?.();
      if (recorderRef.current?.state === "recording") {
        recorderRef.current.stop();
      }
      streamRef.current?.getTracks().forEach((t) => t.stop());
    };
  }, []);

  const startRecording = useCallback(async () => {
    const generation = ++generationRef.current;
    setError(null);
    setState("requesting");

    // Check browser support.
    if (!navigator.mediaDevices?.getUserMedia) {
      setError(
        "Microphone access is not available in this browser. Use a modern browser with HTTPS.",
      );
      setState("idle");
      onEndRef.current?.();
      return;
    }

    const mimeType = getSupportedMimeType();
    if (!mimeType) {
      setError(
        "This browser does not support any supported audio recording format.",
      );
      setState("idle");
      onEndRef.current?.();
      return;
    }
    mimeTypeRef.current = mimeType;

    let stream: MediaStream;
    try {
      stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    } catch (err) {
      if (generation !== generationRef.current) return;
      const msg = err instanceof Error ? err.message : String(err);
      if (
        msg.includes("NotAllowed") ||
        msg.includes("Permission") ||
        msg.includes("denied")
      ) {
        setError(
          "Microphone access was denied. Enable it in your browser settings and try again.",
        );
      } else if (msg.includes("NotFound") || msg.includes("devices")) {
        setError("No microphone found. Connect a microphone and try again.");
      } else {
        setError(`Could not access microphone: ${msg}`);
      }
      setState("idle");
      onEndRef.current?.();
      return;
    }

    if (generation !== generationRef.current) {
      stream.getTracks().forEach((t) => t.stop());
      return;
    }
    streamRef.current = stream;

    try {
      const recorder = new MediaRecorder(stream, {
        mimeType,
        audioBitsPerSecond: 32000,
      });

      chunksRef.current = [];
      recorder.ondataavailable = (e: BlobEvent) => {
        if (e.data.size > 0) chunksRef.current.push(e.data);
      };

      // A microphone track can end without an error or a manual stop.
      recorder.onstop = () => {
        if (generation !== generationRef.current) return;
        const blob = new Blob(chunksRef.current, {
          type: mimeTypeRef.current.split(";")[0],
        });
        // Clean up the mic.
        streamRef.current?.getTracks().forEach((t) => t.stop());
        streamRef.current = null;
        recorderRef.current = null;
        const resolve = resolveRef.current;
        resolveRef.current = null;
        setState("idle");
        onEndRef.current?.();
        resolve?.({ blob, mimeType: mimeTypeRef.current });
      };

      recorder.onerror = () => {
        if (generation !== generationRef.current) return;
        setError("Audio recording failed. Try again.");
        setState("idle");
        onEndRef.current?.();
        stream.getTracks().forEach((t) => t.stop());
        streamRef.current = null;
      };

      recorderRef.current = recorder;
      // timeslice=1000 fires ondataavailable every second, ensuring data is
      // collected even if the browser delays stop-triggered events.
      recorder.start(1000);
      setState("recording");
    } catch (err) {
      stream.getTracks().forEach((t) => t.stop());
      streamRef.current = null;
      recorderRef.current = null;
      setError(err instanceof Error ? err.message : "Could not start recording");
      setState("idle");
      onEndRef.current?.();
    }
  }, []);

  const stopRecording = useCallback(() => {
    return new Promise<{ blob: Blob; mimeType: string } | null>((resolve) => {
      const recorder = recorderRef.current;
      if (!recorder || recorder.state !== "recording") {
        resolve(null);
        return;
      }

      resolveRef.current = resolve;
      recorder.stop();
    });
  }, []);

  const cancel = useCallback(() => {
    generationRef.current++;
    onEndRef.current?.();
    const recorder = recorderRef.current;
    if (recorder && recorder.state === "recording") {
      // Remove the onstop handler so stopRecording's promise doesn't resolve.
      recorder.onstop = null;
      recorder.stop();
    }
    streamRef.current?.getTracks().forEach((t) => t.stop());
    streamRef.current = null;
    recorderRef.current = null;
    resolveRef.current?.call(null, null);
    resolveRef.current = null;
    setState("idle");
  }, []);

  return { state, supported, error, startRecording, stopRecording, cancel };
}
