import { useId } from 'react';
import { cn } from '../../lib/utils.js';
import { inferSourceLanguage, type SourceLanguage } from '../../lib/source-language.js';
import { sourceViewPreferenceEligible, useSourceViewMode } from '../../lib/source-view-preference.js';
import { LazySyntaxHighlighter } from './LazySyntaxHighlighter.js';

export type SourceCodeViewProps = {
  content: string;
  fileName?: string;
  language?: SourceLanguage | null;
  mode?: 'source' | 'plain';
  className?: string;
  testId?: string;
};

const sourceStyle = {
  background: '#282c34', color: '#abb2bf', padding: '0.75rem 1rem', margin: 0,
  overflow: 'visible', whiteSpace: 'pre', wordBreak: 'normal',
} as const;

export function SourceCodeView({ content, fileName, language, mode, className, testId }: SourceCodeViewProps) {
  const inferred = language === undefined ? inferSourceLanguage(fileName ?? '') : language;
  const eligible = sourceViewPreferenceEligible({ mode, fileName, language: inferred });
  const [preference, setPreference] = useSourceViewMode(eligible);
  const sourceMode = mode ?? (eligible && preference === 'code' ? 'source' : 'plain');
  const activeLanguage = sourceMode === 'source' ? inferred : null;
  const scrollingMode = mode !== undefined || activeLanguage !== null;
  const hintId = `source-scroll-hint-${useId().replace(/:/g, '')}`;
  const displayName = fileName?.split('/').pop();
  const label = displayName ? `Source code: ${displayName}` : `Source code: ${activeLanguage ?? 'plain text'}`;
  // Chromium drops a terminal newline from Selection#toString when the range
  // ends at a text-node boundary. A trailing BR preserves that source byte in
  // an actual select-all operation without changing code.textContent.
  const codeFallback = <pre style={sourceStyle}><code>{content}{content.endsWith('\n') ? <br aria-hidden="true" /> : null}</code></pre>;

  return (
    <div
      data-testid={testId ?? 'source-code-view'}
      data-language={activeLanguage ?? 'plain'}
      className={cn('source-code-view min-w-0 max-w-full rounded bg-[#282c34] text-sm', className)}
    >
      {eligible && (
        <div className="flex min-w-0 justify-end p-1.5" data-testid="source-display-toolbar">
          <div role="group" aria-label="Source display" className="inline-flex max-w-full rounded-md border border-neutral-600 bg-[#282c34] p-0.5">
            {(['plain', 'code'] as const).map(option => (
              <button
                key={option}
                type="button"
                aria-pressed={preference === option}
                onClick={() => setPreference(option)}
                className="min-h-9 min-w-14 rounded px-3 py-1.5 text-sm font-medium capitalize text-neutral-200 outline-none aria-pressed:bg-neutral-100 aria-pressed:text-neutral-900 focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2 focus-visible:ring-offset-[#282c34] [@media(pointer:coarse)]:min-h-11"
              >
                {option === 'plain' ? 'Plain' : 'Code'}
              </button>
            ))}
          </div>
        </div>
      )}
      {scrollingMode ? (
        <>
          <div
            tabIndex={0}
            role="region"
            aria-label={label}
            aria-describedby={hintId}
            className="min-w-0 max-w-full overflow-x-auto rounded outline-none focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2"
          >
            {activeLanguage ? (
              <LazySyntaxHighlighter
                content={content}
                language={activeLanguage}
                customStyle={sourceStyle}
                fallback={codeFallback}
              />
            ) : codeFallback}
          </div>
          <span id={hintId} className="sr-only">Scroll horizontally to view long lines.</span>
        </>
      ) : (
        <pre className="m-0 overflow-x-auto whitespace-pre-wrap break-all px-4 py-3 font-mono text-[#abb2bf]">{content}</pre>
      )}
    </div>
  );
}
