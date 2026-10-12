// Engraved ABC notation for a ```abc fenced block.
//
// Reflow — the point of the feature. abcjs's `responsive: "resize"` alone does
// NOT reflow: it hands the SVG a viewBox and 100% width (see abcjs
// src/write/draw/set-paper-size.js — `setResponsiveWidth`), so a narrowed pane
// SCALES the engraving down rather than rewrapping it. Bars-per-line is decided
// by `staffwidth` at engrave time (default 740px, engraver-controller.js:58).
// True reflow therefore means RE-RENDERING with a new `staffwidth` whenever the
// container's width changes, which is what the ResizeObserver below does.
// `responsive: "resize"` is still passed so the SVG never overflows its box
// between a resize and the re-render that follows it.
//
// Dark mode — abcjs already defaults `foregroundColor` to the SVG paint keyword
// `currentColor` and applies it as fill/stroke on every drawn element, so the
// score simply inherits the container's CSS color (`text-foreground`). That is
// why a CSS `invert` filter is not just unnecessary but wrong: abc supports
// per-voice colour overrides, and inverting would corrupt them.
//
// abcjs is loaded with a dynamic import so its parser/engraver stays out of the
// main bundle — a thread with no notation in it pays nothing.

import { useCallback, useEffect, useRef, useState } from 'react';
import { AlertTriangle, Loader2, Play, Square } from 'lucide-react';
import { abcSynthOptions } from '../../lib/abc-synth.js';

type AbcModule = typeof import('abcjs');
type TuneObject = import('abcjs').TuneObject;
type MidiBuffer = import('abcjs').MidiBuffer;

/** Shared across every block; the first score on a page pays the load. */
let abcjsPromise: Promise<AbcModule> | null = null;
function loadAbcjs(): Promise<AbcModule> {
  if (!abcjsPromise) {
    abcjsPromise = import('abcjs').then((m) => (m as { default?: AbcModule }).default ?? m);
  }
  return abcjsPromise;
}

// Re-engraving on every observed pixel would thrash during a drag-resize, and
// sub-pixel jitter (scrollbar appearing, etc.) would re-render for no visible
// gain. Quantizing the measured width to a bucket makes the work proportional
// to real layout changes while staying fine enough that reflow feels live.
const WIDTH_BUCKET_PX = 16;
/** Below this, abcjs produces unreadable output regardless of wrapping. */
const MIN_STAFF_WIDTH_PX = 160;

export function bucketWidth(width: number): number {
  return Math.max(MIN_STAFF_WIDTH_PX, Math.round(width / WIDTH_BUCKET_PX) * WIDTH_BUCKET_PX);
}

type AbcScoreProps = {
  abc: string;
  /** Raised when the source cannot be engraved — host falls back to a code block. */
  onFatal: (message: string) => void;
};

type PlayState = 'idle' | 'loading' | 'playing';

export function AbcScore({ abc, onFatal }: AbcScoreProps) {
  const hostRef = useRef<HTMLDivElement>(null);
  const [staffWidth, setStaffWidth] = useState<number | null>(null);
  const [warnings, setWarnings] = useState<string[]>([]);
  // Playback. The tune object produced by the engrave feeds the synth, so a
  // score that engraved is a score that can play; audio support is probed once
  // abcjs loads. The synth is created lazily on the first Play (an AudioContext
  // needs a user gesture, and the samples must NOT be fetched until asked for).
  const [tuneObject, setTuneObject] = useState<TuneObject | null>(null);
  const [audioSupported, setAudioSupported] = useState(false);
  const [playState, setPlayState] = useState<PlayState>('idle');
  const [playError, setPlayError] = useState<string | null>(null);
  const synthRef = useRef<MidiBuffer | null>(null);
  // onFatal is called from an effect; keep it off the effect's dep list so a
  // parent re-render cannot retrigger engraving.
  const onFatalRef = useRef(onFatal);
  useEffect(() => {
    onFatalRef.current = onFatal;
  }, [onFatal]);

  // Measure the container and re-measure on every resize (window OR pane).
  useEffect(() => {
    const host = hostRef.current;
    if (!host) return;

    const measure = () => {
      const width = host.getBoundingClientRect().width;
      // A hidden/zero-width host (e.g. the raw-source view is showing) must not
      // clobber a good width with a degenerate one.
      if (width > 0) setStaffWidth(bucketWidth(width));
    };
    measure();

    if (typeof ResizeObserver === 'undefined') {
      window.addEventListener('resize', measure);
      return () => window.removeEventListener('resize', measure);
    }
    const observer = new ResizeObserver(measure);
    observer.observe(host);
    return () => observer.disconnect();
  }, []);

  // Engrave. Re-runs when the source changes or the width bucket moves.
  useEffect(() => {
    if (staffWidth === null) return;
    const host = hostRef.current;
    if (!host) return;

    let cancelled = false;

    loadAbcjs()
      .then((abcjs) => {
        if (cancelled || !hostRef.current) return;
        const tunes = abcjs.renderAbc(hostRef.current, abc, {
          add_classes: true,
          responsive: 'resize',
          staffwidth: staffWidth,
          // `wrap` is what actually rewraps the music. Without it ABC's own
          // semantics win: a newline in the tune body is a HARD score line
          // break, so a 4-line source engraves as 4 staff lines at every width
          // (measured: 4 lines at both 1400px and 420px before this was added).
          // With `wrap` + `staffwidth`, abcjs recomputes the breaks to fill the
          // available width — fewer bars per line as the pane narrows, which is
          // the actual requested behaviour. Only honoured when `staffwidth` is
          // also set (abc_tunebook_svg.js:119), which it always is here.
          wrap: {
            minSpacing: 1.8,
            maxSpacing: 2.7,
            preferredMeasuresPerLine: 4,
          },
          // Leave foregroundColor unset: abcjs's own default is `currentColor`,
          // which is precisely the theme-following behaviour we want.
          paddingtop: 0,
          paddingbottom: 8,
          paddingleft: 0,
          paddingright: 0,
        });

        if (cancelled) return;

        // abcjs is lenient — it renders something for almost any input and
        // reports problems as warnings. Treat "produced no tune at all" as
        // fatal, but keep a warned-yet-engraved score visible and SURFACE the
        // warnings rather than hiding either the music or the problem.
        const tune = tunes?.[0] as { warnings?: string[]; lines?: unknown[] } | undefined;
        const engravedSomething = !!hostRef.current?.querySelector('svg');
        if (!tune || !engravedSomething) {
          onFatalRef.current('abc notation produced no engraved output');
          return;
        }
        setWarnings(Array.isArray(tune.warnings) ? tune.warnings : []);
        // Feed playback. supportsAudio() is false where there's no Web Audio /
        // AudioContext (some embedded webviews) — then the Play button is hidden
        // rather than offering a control that can't work.
        setTuneObject(tunes?.[0] ?? null);
        try {
          setAudioSupported(abcjs.synth.supportsAudio());
        } catch {
          setAudioSupported(false);
        }
      })
      .catch((err: unknown) => {
        if (cancelled) return;
        onFatalRef.current(err instanceof Error ? err.message : String(err));
      });

    return () => {
      cancelled = true;
    };
  }, [abc, staffWidth]);

  // Tear down any live synth when the tune changes or the component unmounts —
  // otherwise a stopped-mid-play buffer keeps its AudioContext nodes around.
  const disposeSynth = useCallback(() => {
    const synth = synthRef.current;
    synthRef.current = null;
    if (synth) {
      try {
        synth.stop();
      } catch {
        // Already stopped / never started — nothing to do.
      }
    }
  }, []);
  useEffect(() => {
    // A new source is a new tune; stop the old one and reset the control.
    disposeSynth();
    setPlayState('idle');
    setPlayError(null);
  }, [abc, disposeSynth]);
  useEffect(() => () => disposeSynth(), [disposeSynth]);

  const handlePlay = useCallback(async () => {
    if (!tuneObject) return;
    setPlayError(null);
    setPlayState('loading');
    try {
      const abcjs = await loadAbcjs();
      // Build a fresh buffer each play so a re-play after end works cleanly.
      const synth = new abcjs.synth.CreateSynth();
      synthRef.current = synth;
      await synth.init({
        visualObj: tuneObject,
        options: abcSynthOptions(),
        // Fires when playback reaches the end naturally.
        onEnded: () => {
          if (synthRef.current === synth) {
            disposeSynth();
            setPlayState('idle');
          }
        },
      });
      // A tune change during init/prime may have disposed this synth already.
      if (synthRef.current !== synth) return;
      await synth.prime();
      if (synthRef.current !== synth) return;
      synth.start();
      setPlayState('playing');
    } catch (err: unknown) {
      disposeSynth();
      setPlayState('idle');
      setPlayError(err instanceof Error ? err.message : String(err));
    }
  }, [tuneObject, disposeSynth]);

  const handleStop = useCallback(() => {
    disposeSynth();
    setPlayState('idle');
  }, [disposeSynth]);

  return (
    <div className="bg-background text-foreground px-3 py-2" data-testid="abc-score">
      {/* overflow-hidden enforces the no-horizontal-scroll criterion even in the
          instant between a resize and its re-engrave. */}
      <div ref={hostRef} className="w-full overflow-hidden" data-testid="abc-score-host" />
      {audioSupported && tuneObject && (
        <div className="mt-1 flex items-center gap-2" data-testid="abc-player">
          <button
            onClick={playState === 'playing' ? handleStop : handlePlay}
            disabled={playState === 'loading'}
            className="flex items-center gap-1 rounded border border-border px-2 py-1 text-sm text-foreground hover:bg-muted transition-colors disabled:opacity-60"
            title={playState === 'playing' ? 'Stop' : 'Play'}
            aria-pressed={playState === 'playing'}
            data-testid="abc-play-button"
            data-play-state={playState}
          >
            {playState === 'loading' ? (
              <>
                <Loader2 className="size-4 animate-spin" />
                <span>Loading…</span>
              </>
            ) : playState === 'playing' ? (
              <>
                <Square className="size-4" />
                <span>Stop</span>
              </>
            ) : (
              <>
                <Play className="size-4" />
                <span>Play</span>
              </>
            )}
          </button>
          {playError && (
            <span
              className="flex items-center gap-1 text-sm text-amber-600 dark:text-amber-500"
              title={playError}
              data-testid="abc-play-error"
            >
              <AlertTriangle className="size-4 shrink-0" />
              <span>Playback failed</span>
            </span>
          )}
        </div>
      )}
      {warnings.length > 0 && (
        <details className="mt-1 text-sm text-amber-600 dark:text-amber-500" data-testid="abc-warnings">
          <summary className="cursor-pointer flex items-center gap-1">
            <AlertTriangle className="size-4 shrink-0" />
            <span>
              {warnings.length} notation warning{warnings.length === 1 ? '' : 's'}
            </span>
          </summary>
          <ul className="mt-1 pl-5 list-disc space-y-0.5">
            {warnings.map((w, i) => (
              <li key={i} className="font-mono break-words">
                {w}
              </li>
            ))}
          </ul>
        </details>
      )}
    </div>
  );
}
