import { useCallback, useEffect, useState } from 'react';

// User preference: keep the device screen awake while the app is open. Defaults
// to OFF. The composer button toggles it manually; starting voice switches it
// on and ending voice switches it off. Stored in localStorage (client-only, like
// the other lightweight UI prefs).
//
// Same-document instances (e.g. AppLayout, which owns the wake lock, and the
// composer toggle) don't receive the `storage` event — that only fires in
// *other* tabs. So update() also dispatches a custom event, letting every hook
// instance in this document react immediately (start/stop the screen-awake hold
// the moment the user taps the button, no reload needed).

const STORAGE_KEY = 'guccc.keepScreenAwake';
const SYNC_EVENT = 'guccc:keepScreenAwake';

// Pure parse of the stored value. Default is OFF, so only an explicit 'true'
// enables the hold; null (never set) and anything else read as false.
export function parseKeepScreenAwake(raw: string | null): boolean {
  return raw === 'true';
}

function read(): boolean {
  try {
    return parseKeepScreenAwake(localStorage.getItem(STORAGE_KEY));
  } catch {
    return false;
  }
}

export function useKeepScreenAwake(): [boolean, (next: boolean) => void] {
  const [enabled, setEnabled] = useState<boolean>(read);

  useEffect(() => {
    const onStorage = (e: StorageEvent) => {
      if (e.key === STORAGE_KEY) setEnabled(read());
    };
    const onSync = () => setEnabled(read());
    window.addEventListener('storage', onStorage);
    window.addEventListener(SYNC_EVENT, onSync);
    return () => {
      window.removeEventListener('storage', onStorage);
      window.removeEventListener(SYNC_EVENT, onSync);
    };
  }, []);

  const update = useCallback((next: boolean) => {
    try { localStorage.setItem(STORAGE_KEY, String(next)); } catch { /* noop */ }
    setEnabled(next);
    window.dispatchEvent(new Event(SYNC_EVENT));
  }, []);

  return [enabled, update];
}
