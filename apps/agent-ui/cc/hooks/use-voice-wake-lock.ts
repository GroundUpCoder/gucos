import { useCallback, useEffect, useRef } from 'react';
import { useKeepScreenAwake } from './use-keep-screen-awake.js';
import { useWakeLockAcquire, useWakeLockRelease } from '../contexts/WakeLockContext.js';

/** Voice toggles the existing control on start/off on end, including failures
 * and navigation. Idle composers never turn off a manually enabled lock. */
export function useVoiceWakeLock() {
  const [, setKeepScreenAwake] = useKeepScreenAwake();
  const acquire = useWakeLockAcquire();
  const release = useWakeLockRelease();
  const active = useRef(false);
  const end = useCallback(() => {
    if (!active.current) return;
    active.current = false;
    setKeepScreenAwake(false);
    release();
  }, [setKeepScreenAwake, release]);
  const begin = useCallback(() => {
    active.current = true;
    setKeepScreenAwake(true);
    // Start inside the mic click, before awaiting microphone permission.
    void acquire();
  }, [setKeepScreenAwake, acquire]);
  useEffect(() => end, [end]);
  return { begin, end };
}
