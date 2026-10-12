import { rpcClient } from '../rpc/rpc-client.js';
import type { SavedPrompt } from '../components/ChatComposer.js';

/** Limits shared by the settings "Saved Prompts" editor and the composer's
 *  inline "Save to prompts" action — one source of truth so the two entry
 *  points can never drift on what's allowed. */
export const MAX_SAVED_PROMPTS = 50;
export const MAX_PROMPT_TITLE_LEN = 100;
export const MAX_PROMPT_TEXT_LEN = 10000;

/** A user-friendly default title for a to-be-saved prompt: the first line,
 *  trimmed to ~40 chars (title is what the picker lists by). */
export function defaultPromptTitle(text: string): string {
  const firstLine = text.trim().split('\n', 1)[0].trim();
  return firstLine.length > 40 ? `${firstLine.slice(0, 40).trimEnd()}…` : firstLine;
}

/** Validate + append a new saved prompt to `current`, persist the whole list
 *  server-side (preferences/update), and return the new array so the caller can
 *  update local state (so the composer's picker shows it immediately, no reload).
 *
 *  Throws an Error with a user-facing message on any guard failure (empty
 *  fields, over the count cap, over a length cap) — callers surface it as a
 *  toast. Mirrors the guards in UserSettingsPage's PromptEditor. */
export async function saveNewPrompt(
  current: SavedPrompt[],
  input: { title: string; text: string },
): Promise<SavedPrompt[]> {
  const title = input.title.trim();
  const text = input.text.trim();
  if (!title || !text) {
    throw new Error('Both a title and prompt text are required');
  }
  if (title.length > MAX_PROMPT_TITLE_LEN) {
    throw new Error(`Title must be ${MAX_PROMPT_TITLE_LEN} characters or fewer`);
  }
  if (text.length > MAX_PROMPT_TEXT_LEN) {
    throw new Error(`Prompt must be ${MAX_PROMPT_TEXT_LEN} characters or fewer`);
  }
  if (current.length >= MAX_SAVED_PROMPTS) {
    throw new Error(`You can save at most ${MAX_SAVED_PROMPTS} prompts`);
  }
  const next = [...current, { id: crypto.randomUUID(), title, text }];
  await rpcClient.call('preferences/update', { savedPrompts: next });
  return next;
}
