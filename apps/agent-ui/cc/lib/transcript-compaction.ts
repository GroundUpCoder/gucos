import type { Message } from '../shared/ai.types.js';
import { getUserTextContent } from '../shared/ai.types.js';

export const OVERFLOW_SLACK_PX = 1;

export function isUserTextCompactable(message: Message): boolean {
  if (message.role !== 'user') return false;
  if (message.deletedAt) return false;
  if (message.queued) return false;
  return getUserTextContent(message.content).trim().length > 0;
}

export function isOverflowing(measurement: { scrollHeight: number; clientHeight: number }): boolean {
  return measurement.scrollHeight - measurement.clientHeight > OVERFLOW_SLACK_PX;
}

export function userTextToggleLabels(locale?: string) {
  return {
    expand: (characters: number) => `Show full message · ${characters.toLocaleString(locale)} characters`,
    collapse: 'Show less',
    description: 'Visual size control. The full message text is already available to screen readers.',
  };
}

/**
 * Return DOM-safe UTF-16 offsets for a case-insensitive match.
 *
 * JavaScript case conversion can change UTF-16 length (`İ` -> `i` + combining
 * dot). Offsets from such a folded string cannot be used in a Range over the
 * original text node. Folding one code point at a time lets us reject those
 * cases and deliberately use the caller's whole-element fallback instead.
 */
export function findLengthPreservingCaseInsensitiveMatch(text: string, query: string): { start: number; end: number } | null {
  if (!query) return null;
  const foldWithoutLengthChanges = (value: string): string | null => {
    let folded = '';
    for (const character of value) {
      const next = character.toLocaleLowerCase();
      if (next.length !== character.length) return null;
      folded += next;
    }
    return folded;
  };
  const haystack = foldWithoutLengthChanges(text);
  const needle = foldWithoutLengthChanges(query);
  if (haystack == null || needle == null) return null;
  const start = haystack.indexOf(needle);
  return start < 0 ? null : { start, end: start + needle.length };
}
