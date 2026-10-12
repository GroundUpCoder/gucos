import { useEffect, useState } from 'react';

export type ImageSizeLimit = '1' | '5' | '10' | 'none';

export const IMAGE_SIZE_LIMITS: ImageSizeLimit[] = ['1', '5', '10', 'none'];

export const IMAGE_SIZE_LIMIT_LABELS: Record<ImageSizeLimit, string> = {
  '1': '1 MB',
  '5': '5 MB',
  '10': '10 MB',
  'none': 'None',
};

export function imageSizeLimitBytes(limit: ImageSizeLimit): number | null {
  if (limit === 'none') return null;
  return parseInt(limit, 10) * 1024 * 1024;
}

const STORAGE_KEY = 'guccc.imageSizeLimit';
const DEFAULT: ImageSizeLimit = '1';

function isValid(v: unknown): v is ImageSizeLimit {
  return v === '1' || v === '5' || v === '10' || v === 'none';
}

function read(): ImageSizeLimit {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    return isValid(raw) ? raw : DEFAULT;
  } catch {
    return DEFAULT;
  }
}

export function useImageSizeLimit(): [ImageSizeLimit, (next: ImageSizeLimit) => void] {
  const [limit, setLimit] = useState<ImageSizeLimit>(read);

  useEffect(() => {
    const onStorage = (e: StorageEvent) => {
      if (e.key === STORAGE_KEY) setLimit(read());
    };
    window.addEventListener('storage', onStorage);
    return () => window.removeEventListener('storage', onStorage);
  }, []);

  const update = (next: ImageSizeLimit) => {
    try { localStorage.setItem(STORAGE_KEY, next); } catch { /* noop */ }
    setLimit(next);
  };

  return [limit, update];
}
