import { useEffect, useState } from 'react';

export type ReaderSize = 'small' | 'medium' | 'large' | 'xlarge';

// Class names MUST appear as literal strings so the Tailwind v4 JIT
// scanner picks them up — never build them by concatenation.
export const READER_SIZE_PROSE_CLASSES: Record<ReaderSize, string> = {
  small: 'prose-sm',
  medium: '',
  large: 'prose-lg',
  xlarge: 'prose-xl',
};

export const READER_SIZE_TEXT_CLASSES: Record<ReaderSize, string> = {
  small: 'text-sm',
  medium: 'text-base',
  large: 'text-lg',
  xlarge: 'text-xl',
};

export const READER_SIZE_LABELS: Record<ReaderSize, string> = {
  small: 'Small',
  medium: 'Medium',
  large: 'Large',
  xlarge: 'X-Large',
};

export const READER_SIZES: ReaderSize[] = ['small', 'medium', 'large', 'xlarge'];

const STORAGE_KEY = 'guccc.readerSize';
const DEFAULT: ReaderSize = 'large';

function isValid(v: unknown): v is ReaderSize {
  return v === 'small' || v === 'medium' || v === 'large' || v === 'xlarge';
}

function read(): ReaderSize {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    return isValid(raw) ? raw : DEFAULT;
  } catch {
    return DEFAULT;
  }
}

export function useReaderSize(): [ReaderSize, (next: ReaderSize) => void] {
  const [size, setSize] = useState<ReaderSize>(read);

  useEffect(() => {
    const onStorage = (e: StorageEvent) => {
      if (e.key === STORAGE_KEY) setSize(read());
    };
    window.addEventListener('storage', onStorage);
    return () => window.removeEventListener('storage', onStorage);
  }, []);

  const update = (next: ReaderSize) => {
    try { localStorage.setItem(STORAGE_KEY, next); } catch { /* noop */ }
    setSize(next);
  };

  return [size, update];
}
