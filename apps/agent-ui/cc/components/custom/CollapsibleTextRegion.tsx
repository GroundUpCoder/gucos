import { useCallback, useLayoutEffect, useRef, useState } from 'react';
import { ChevronDown, ChevronUp } from 'lucide-react';
import { isOverflowing } from '../../lib/transcript-compaction.js';

type CollapsibleTextRegionProps = {
  text: string;
  expanded: boolean;
  onToggle: (next: boolean) => void;
  regionId: string;
  testIdPrefix: string;
  labels: { expand: (characters: number) => string; collapse: string; description: string };
  onOverflowChange?: (overflows: boolean) => void;
  className?: string;
  /** Explicitly remeasure when a reader-size preference changes. */
  measurementKey?: string;
};

export function CollapsibleTextRegion({
  text,
  expanded,
  onToggle,
  regionId,
  testIdPrefix,
  labels,
  onOverflowChange,
  className,
  measurementKey,
}: CollapsibleTextRegionProps) {
  const regionRef = useRef<HTMLDivElement>(null);
  const buttonRef = useRef<HTMLButtonElement>(null);
  const previousExpandedRef = useRef(expanded);
  // An expanded region may be remounting after whole-message collapse. In
  // that case the parent's expanded intent proves the disclosure was useful,
  // so preserve the sticky control without measuring an unclamped element.
  const [overflows, setOverflows] = useState(expanded);

  const measure = useCallback(() => {
    if (expanded || !regionRef.current) return;
    const next = isOverflowing(regionRef.current);
    setOverflows(next);
    onOverflowChange?.(next);
  }, [expanded, onOverflowChange]);

  useLayoutEffect(() => {
    measure();
    if (expanded || !regionRef.current || typeof ResizeObserver === 'undefined') return;
    const observer = new ResizeObserver(measure);
    observer.observe(regionRef.current);
    return () => observer.disconnect();
  }, [text, expanded, measurementKey, measure]);

  useLayoutEffect(() => {
    const collapsing = previousExpandedRef.current && !expanded;
    previousExpandedRef.current = expanded;
    if (collapsing) buttonRef.current?.scrollIntoView({ block: 'nearest' });
  }, [expanded]);

  return (
    <>
      <div
        ref={regionRef}
        id={regionId}
        data-testid={`${testIdPrefix}-region`}
        className={`${expanded ? '' : 'line-clamp-6 md:line-clamp-5'} ${className ?? ''}`}
      >
        {text}
      </div>
      {overflows && (
        <>
          <button
            ref={buttonRef}
            type="button"
            data-testid={`${testIdPrefix}-toggle`}
            aria-pressed={expanded}
            aria-controls={regionId}
            aria-describedby={`${regionId}-description`}
            onClick={() => onToggle(!expanded)}
            className="min-h-11 w-full rounded-sm text-left flex items-center gap-1 text-sm font-medium text-muted-foreground hover:text-foreground focus-visible:ring-2 focus-visible:ring-ring focus-visible:outline-none"
          >
            {expanded ? <ChevronUp className="h-4 w-4 shrink-0" /> : <ChevronDown className="h-4 w-4 shrink-0" />}
            <span>{expanded ? labels.collapse : labels.expand(text.length)}</span>
          </button>
          <span id={`${regionId}-description`} className="sr-only">
            {labels.description}
          </span>
        </>
      )}
    </>
  );
}
