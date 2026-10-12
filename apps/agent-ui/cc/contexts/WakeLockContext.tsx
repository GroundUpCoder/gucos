import { createContext, useContext } from 'react';
import type { ReactNode } from 'react';

// Bridges the gap between the AppLayout (which owns the wake-lock lifecycle)
// and ChatComposer (which has the toggle button + user gesture). The acquire
// callback is stable and can be called directly from onClick handlers, keeping
// the request inside the user gesture for iOS Safari compatibility.

type WakeLockContextValue = {
  acquire: () => Promise<void>;
  release: () => void;
};

const WakeLockContext = createContext<WakeLockContextValue>({
  acquire: async () => {},
  release: () => {},
});

export function WakeLockProvider({
  acquire,
  release,
  children,
}: {
  acquire: () => Promise<void>;
  release: () => void;
  children: ReactNode;
}) {
  return (
    <WakeLockContext.Provider value={{ acquire, release }}>
      {children}
    </WakeLockContext.Provider>
  );
}

/** Call wakeLock.request('screen') inside a user gesture (iOS requirement). */
export function useWakeLockAcquire(): () => Promise<void> {
  return useContext(WakeLockContext).acquire;
}

export function useWakeLockRelease(): () => void {
  return useContext(WakeLockContext).release;
}
