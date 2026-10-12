import type { ComponentProps } from 'react';
import { useState, useCallback, useEffect, memo } from 'react';
import { Check, Copy, Code2, Music, AlertTriangle } from 'lucide-react';
import { AbcScore } from './AbcScore.js';
import { SourceCodeView } from './SourceCodeView.js';
import { extractFenceLanguage, normalizeFenceLanguage } from '../../lib/fence-language.js';

type CodeBlockProps = ComponentProps<'code'> & {
  inline?: boolean;
  /** Stamped by `remarkFenceState` — see lib/fenced-block.ts. */
  'data-fence-closed'?: string;
};

export function CodeBlock({
  inline,
  className,
  children,
  ...rest
}: CodeBlockProps) {
  return (
    <CodeBlockInner
      inline={inline}
      className={className}
      fenceClosed={rest['data-fence-closed']}
    >
      {children}
    </CodeBlockInner>
  );
}

type CodeBlockInnerProps = {
  inline?: boolean;
  className?: string;
  fenceClosed?: string;
  children?: React.ReactNode;
};

const CodeBlockInner = memo(function CodeBlockInner({
  inline,
  className,
  fenceClosed,
  children,
}: CodeBlockInnerProps) {
  const [copied, setCopied] = useState(false);
  const language = extractFenceLanguage(className);
  const sourceLanguage = normalizeFenceLanguage(language);
  const codeString = String(children).replace(/\n$/, '');

  const isInline = inline ?? (!className && !codeString.includes('\n'));

  // Rich rendering is gated on PROOF the fence closed, so a half-streamed block
  // stays a plain code block instead of flickering through parse failures. An
  // absent attribute means the host made no completeness claim (CodeBlock used
  // outside MemoizedMarkdown, i.e. static content) — only explicit "false"
  // withholds the score.
  const isComplete = fenceClosed !== 'false';
  const isAbc = language === 'abc' && !isInline && isComplete;

  // Rendered score by default; the toggle flips to the raw ABC source.
  const [showScore, setShowScore] = useState(true);
  // A source abcjs cannot engrave falls back to the code block, with the reason
  // surfaced rather than swallowed.
  const [fatal, setFatal] = useState<string | null>(null);
  useEffect(() => {
    setFatal(null);
  }, [codeString]);
  const handleFatal = useCallback((message: string) => setFatal(message), []);

  const handleCopy = async () => {
    // Always the ABC source — identical in the score and raw views.
    await navigator.clipboard.writeText(codeString);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  if (isInline) {
    return (
      <code className="bg-muted px-1.5 py-0.5 rounded text-base before:content-none after:content-none">
        {children}
      </code>
    );
  }

  const renderScore = isAbc && !fatal && showScore;

  return (
    <div>
      <div className="flex items-center justify-between gap-2 px-3 py-1.5 rounded-t-lg bg-[#282c34] border-b border-white/10">
        <span className="text-sm text-neutral-400 font-mono">
          {language || 'text'}
        </span>
        <div className="flex items-center gap-3">
          {isAbc && fatal && (
            <span
              className="flex items-center gap-1 text-sm text-amber-500"
              title={`Could not engrave this notation: ${fatal}`}
              data-testid="abc-fallback-notice"
            >
              <AlertTriangle className="size-4" />
              <span>Notation error</span>
            </span>
          )}
          {isAbc && !fatal && (
            <button
              onClick={() => setShowScore((v) => !v)}
              className="flex items-center gap-1 text-sm text-neutral-400 hover:text-neutral-200 transition-colors"
              title={showScore ? 'Show ABC source' : 'Show notation'}
              aria-pressed={showScore}
              data-testid="abc-toggle"
            >
              {showScore ? (
                <>
                  <Code2 className="size-4" />
                  <span>Source</span>
                </>
              ) : (
                <>
                  <Music className="size-4" />
                  <span>Score</span>
                </>
              )}
            </button>
          )}
          <button
            onClick={handleCopy}
            className="flex items-center gap-1 text-sm text-neutral-400 hover:text-neutral-200 transition-colors"
            title="Copy code"
            data-testid="code-copy"
          >
            {copied ? (
              <>
                <Check className="size-4" />
                <span>Copied!</span>
              </>
            ) : (
              <>
                <Copy className="size-4" />
                <span>Copy</span>
              </>
            )}
          </button>
        </div>
      </div>
      {renderScore ? (
        <AbcScore abc={codeString} onFatal={handleFatal} />
      ) : (
        <SourceCodeView
          content={codeString}
          language={sourceLanguage}
          mode={sourceLanguage ? 'source' : 'plain'}
          className="rounded-t-none"
          testId="fenced-source-code-view"
        />
      )}
    </div>
  );
});
