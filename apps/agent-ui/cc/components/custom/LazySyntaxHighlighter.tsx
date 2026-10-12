import { useEffect, useState } from 'react';
import type { CSSProperties, ReactNode } from 'react';
import type { SourceLanguage } from '../../lib/source-language.js';
import type { SyntaxRuntimeProps } from '../../lib/syntax-highlighter-runtime.js';

type Runtime = React.ComponentType<SyntaxRuntimeProps>;
type LoadState = { status: 'idle' | 'pending' } | { status: 'ready'; Runtime: Runtime } | { status: 'failed' };
let loadState: LoadState = { status: 'idle' };
let loadPromise: Promise<void> | null = null;
const listeners = new Set<() => void>();

function notify() { for (const listener of listeners) listener(); }

function startLoad() {
  if (loadState.status !== 'idle') return;
  loadState = { status: 'pending' };
  loadPromise = import('../../lib/syntax-highlighter-runtime.js')
    .then(module => { loadState = { status: 'ready', Runtime: module.SyntaxHighlighterRuntime }; notify(); })
    .catch(error => {
      loadState = { status: 'failed' };
      console.error('Syntax highlighting unavailable; showing plain source.', error);
      notify();
    });
}

type Props = {
  content: string;
  language: SourceLanguage;
  customStyle?: CSSProperties;
  codeTagProps?: Record<string, unknown>;
  fallback: ReactNode;
};

export function LazySyntaxHighlighter({ fallback, content, ...props }: Props) {
  const [, rerender] = useState(0);
  useEffect(() => {
    const listener = () => rerender(value => value + 1);
    listeners.add(listener);
    startLoad();
    return () => { listeners.delete(listener); };
  }, []);
  if (loadState.status !== 'ready') return fallback;
  const Runtime = loadState.Runtime;
  return <Runtime {...props}>{content}</Runtime>;
}

void loadPromise;
