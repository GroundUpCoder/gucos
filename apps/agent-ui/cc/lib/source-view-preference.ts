import { useSyncExternalStore } from 'react';
import { isRegisteredSourceLanguage, type SourceLanguage } from './source-language.js';

export type SourceViewMode = 'plain' | 'code';

export const SOURCE_VIEW_MODE_KEY = 'guccc-source-view-mode-v1';
export const SOURCE_VIEW_MODE_EVENT = 'guccc:source-view-mode';

let memoryMode: SourceViewMode = 'plain';
const listeners = new Set<() => void>();

export function parseSourceViewMode(value: string | null | undefined): SourceViewMode {
  return value === 'code' ? 'code' : 'plain';
}

export function sourceViewModeFromStorageEvent(
  event: Pick<StorageEvent, 'key' | 'newValue'>,
): SourceViewMode | null {
  if (event.key !== SOURCE_VIEW_MODE_KEY && event.key !== null) return null;
  return parseSourceViewMode(event.newValue);
}

export function readSourceViewMode(storage?: Pick<Storage, 'getItem'>): SourceViewMode {
  try {
    if (!storage) return memoryMode;
    const stored = parseSourceViewMode(storage.getItem(SOURCE_VIEW_MODE_KEY));
    memoryMode = stored;
    return stored;
  } catch {
    return memoryMode;
  }
}

function notifySourceViewMode() {
  for (const listener of listeners) listener();
}

export function writeSourceViewMode(
  mode: SourceViewMode,
  storage?: Pick<Storage, 'setItem' | 'removeItem'>,
  eventTarget?: Pick<EventTarget, 'dispatchEvent'>,
): void {
  memoryMode = mode;
  try {
    if (mode === 'code') storage?.setItem(SOURCE_VIEW_MODE_KEY, 'code');
    else storage?.removeItem(SOURCE_VIEW_MODE_KEY);
  } catch {
    // The in-document preference still works when storage is unavailable.
  }
  eventTarget?.dispatchEvent(new Event(SOURCE_VIEW_MODE_EVENT));
  notifySourceViewMode();
}

export function sourceViewPreferenceEligible(input: {
  mode?: 'source' | 'plain';
  fileName?: string;
  language: SourceLanguage | null;
}): boolean {
  return input.mode === undefined
    && Boolean(input.fileName?.trim())
    && input.language !== null
    && isRegisteredSourceLanguage(input.language);
}

function getSnapshot(): SourceViewMode {
  return readSourceViewMode(typeof window === 'undefined' ? undefined : window.localStorage);
}

function subscribe(listener: () => void): () => void {
  listeners.add(listener);
  if (typeof window !== 'undefined') {
    const onStorage = (event: StorageEvent) => {
      const next = sourceViewModeFromStorageEvent(event);
      if (next === null) return;
      memoryMode = next;
      notifySourceViewMode();
    };
    const onLocal = () => {
      readSourceViewMode(window.localStorage);
      notifySourceViewMode();
    };
    window.addEventListener('storage', onStorage);
    window.addEventListener(SOURCE_VIEW_MODE_EVENT, onLocal);
    return () => {
      listeners.delete(listener);
      window.removeEventListener('storage', onStorage);
      window.removeEventListener(SOURCE_VIEW_MODE_EVENT, onLocal);
    };
  }
  return () => { listeners.delete(listener); };
}

const disabledSubscribe = () => () => {};
const plainSnapshot = (): SourceViewMode => 'plain';

export function useSourceViewMode(enabled = true): readonly [SourceViewMode, (mode: SourceViewMode) => void] {
  const mode = useSyncExternalStore(
    enabled ? subscribe : disabledSubscribe,
    enabled ? getSnapshot : plainSnapshot,
    plainSnapshot,
  );
  return [mode, next => writeSourceViewMode(
    next,
    typeof window === 'undefined' ? undefined : window.localStorage,
    typeof window === 'undefined' ? undefined : window,
  )] as const;
}
