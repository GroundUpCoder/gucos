// The ONE place ABC playback is configured. Everything that plays a tune
// (the inline `AbcScore` under a ```abc fence AND the `.abc` artifact view)
// builds its synth options here, so the soundfont source is set once.
//
// ── Zero third-party network calls ──
// abcjs's synth defaults `soundFontUrl` to the CDN
// paulrosen.github.io/midi-js-soundfonts, fetching one mp3 per (instrument, note)
// as `<soundFontUrl>/<instrument>-mp3/<Note>.mp3`. We instead point it at the
// same-origin `/soundfonts/FluidR3_GM/` directory that
// scripts/vendor-soundfonts.mjs populated in public/, so a Play click fetches
// ONLY from our own origin. Samples are lazy by construction: they live in
// public/ (never bundled), and abcjs requests only the notes a tune actually
// uses, only after the user hits Play.
//
// FluidR3_GM matches abcjs's own default bank, so its built-in volume tuning
// (soundFontVolumeMultiplier 3.0, applied when the URL is the default OR this
// bank) still applies — we get correct loudness for free.
import type { SynthOptions } from 'abcjs';

/** Same-origin base for the vendored soundfont samples. Trailing slash required
 *  (abcjs concatenates `<url><instrument>-mp3/<note>.mp3`). */
export const SOUNDFONT_URL = '/soundfonts/FluidR3_GM/';

/** Synth options for every ABC playback surface. */
export function abcSynthOptions(): SynthOptions {
  return { soundFontUrl: SOUNDFONT_URL };
}
