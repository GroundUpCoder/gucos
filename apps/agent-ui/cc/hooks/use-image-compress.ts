import { useEffect, useState } from 'react';

const STORAGE_KEY = 'guccc.imageCompress';
const DEFAULT = true;

function read(): boolean {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (raw === 'false') return false;
    return DEFAULT;
  } catch {
    return DEFAULT;
  }
}

export function useImageCompress(): [boolean, (next: boolean) => void] {
  const [compress, setCompress] = useState<boolean>(read);

  useEffect(() => {
    const onStorage = (e: StorageEvent) => {
      if (e.key === STORAGE_KEY) setCompress(read());
    };
    window.addEventListener('storage', onStorage);
    return () => window.removeEventListener('storage', onStorage);
  }, []);

  const update = (next: boolean) => {
    try { localStorage.setItem(STORAGE_KEY, String(next)); } catch { /* noop */ }
    setCompress(next);
  };

  return [compress, update];
}
