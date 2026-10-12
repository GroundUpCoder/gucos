// Generic, mobile-friendly JSON tree viewer with folding.
//
// Renders unknown JSON shapes with collapsible objects/arrays. Used as the
// fallback in the rawcc event viewer when no specialized viewer matches.
import { useState } from 'react';
import { ChevronRight, ChevronDown } from 'lucide-react';

type Json = null | boolean | number | string | Json[] | { [k: string]: Json };

export function JsonTreeView({
  value,
  defaultExpanded = true,
  maxInitialDepth = 2,
}: {
  value: unknown;
  defaultExpanded?: boolean;
  maxInitialDepth?: number;
}) {
  return (
    <div className="font-mono text-xs leading-relaxed break-words">
      <JsonNode
        nodeKey={null}
        value={value as Json}
        depth={0}
        defaultExpanded={defaultExpanded}
        maxInitialDepth={maxInitialDepth}
      />
    </div>
  );
}

function JsonNode({
  nodeKey,
  value,
  depth,
  defaultExpanded,
  maxInitialDepth,
}: {
  nodeKey: string | number | null;
  value: Json;
  depth: number;
  defaultExpanded: boolean;
  maxInitialDepth: number;
}) {
  const shouldExpand = defaultExpanded && depth < maxInitialDepth;
  const [expanded, setExpanded] = useState(shouldExpand);

  const keyLabel = nodeKey === null
    ? null
    : typeof nodeKey === 'number'
      ? <span className="text-muted-foreground">[{nodeKey}]</span>
      : <span className="text-sky-600 dark:text-sky-400">{JSON.stringify(nodeKey)}</span>;

  if (value === null) {
    return (
      <div className="flex items-baseline gap-1">
        {keyLabel && <>{keyLabel}<span className="text-muted-foreground">:</span></>}
        <span className="text-muted-foreground italic">null</span>
      </div>
    );
  }

  if (typeof value === 'boolean') {
    return (
      <div className="flex items-baseline gap-1">
        {keyLabel && <>{keyLabel}<span className="text-muted-foreground">:</span></>}
        <span className="text-purple-600 dark:text-purple-400">{String(value)}</span>
      </div>
    );
  }

  if (typeof value === 'number') {
    return (
      <div className="flex items-baseline gap-1">
        {keyLabel && <>{keyLabel}<span className="text-muted-foreground">:</span></>}
        <span className="text-amber-600 dark:text-amber-400">{value}</span>
      </div>
    );
  }

  if (typeof value === 'string') {
    return (
      <div className="flex items-baseline gap-1">
        {keyLabel && <>{keyLabel}<span className="text-muted-foreground">:</span></>}
        <StringValue value={value} />
      </div>
    );
  }

  // Array or object
  const isArray = Array.isArray(value);
  const entries: [string | number, Json][] = isArray
    ? (value as Json[]).map((v, i) => [i, v])
    : Object.entries(value as { [k: string]: Json });
  const count = entries.length;
  const openBracket = isArray ? '[' : '{';
  const closeBracket = isArray ? ']' : '}';

  if (count === 0) {
    return (
      <div className="flex items-baseline gap-1">
        {keyLabel && <>{keyLabel}<span className="text-muted-foreground">:</span></>}
        <span className="text-muted-foreground">{openBracket}{closeBracket}</span>
      </div>
    );
  }

  return (
    <div>
      <button
        type="button"
        onClick={() => setExpanded(p => !p)}
        className="flex items-baseline gap-1 w-full text-left hover:bg-muted/40 rounded px-0.5 -mx-0.5"
      >
        {expanded
          ? <ChevronDown className="w-3 h-3 shrink-0 text-muted-foreground translate-y-0.5" />
          : <ChevronRight className="w-3 h-3 shrink-0 text-muted-foreground translate-y-0.5" />}
        {keyLabel && <>{keyLabel}<span className="text-muted-foreground">:</span></>}
        <span className="text-muted-foreground">{openBracket}</span>
        {!expanded && (
          <>
            <span className="text-muted-foreground italic ml-1">
              {count} {isArray ? (count === 1 ? 'item' : 'items') : (count === 1 ? 'key' : 'keys')}
            </span>
            <span className="text-muted-foreground">{closeBracket}</span>
          </>
        )}
      </button>
      {expanded && (
        <>
          <div className="ml-4 border-l border-border/50 pl-2">
            {entries.map(([k, v]) => (
              <JsonNode
                key={String(k)}
                nodeKey={k}
                value={v}
                depth={depth + 1}
                defaultExpanded={defaultExpanded}
                maxInitialDepth={maxInitialDepth}
              />
            ))}
          </div>
          <div className="flex items-baseline gap-1 ml-0.5">
            <span className="text-muted-foreground">{closeBracket}</span>
          </div>
        </>
      )}
    </div>
  );
}

// Truncate strings that are long enough to cause clutter. Click to toggle.
const TRUNCATE_LEN = 80;

function StringValue({ value }: { value: string }) {
  const hasNewline = value.includes('\n');
  const firstLine = hasNewline ? value.slice(0, value.indexOf('\n')) : value;
  const needsTruncate = value.length > TRUNCATE_LEN || hasNewline;
  const [truncated, setTruncated] = useState(true);

  if (!needsTruncate) {
    return (
      <span className="text-emerald-700 dark:text-emerald-400 whitespace-pre-wrap break-words">
        "{value}"
      </span>
    );
  }

  if (truncated) {
    const shown = firstLine.length > TRUNCATE_LEN ? firstLine.slice(0, TRUNCATE_LEN) : firstLine;
    return (
      <button
        type="button"
        onClick={() => setTruncated(false)}
        title={`Click to expand (${value.length} chars)`}
        className="text-left text-emerald-700 dark:text-emerald-400 break-words hover:bg-muted/40 rounded px-0.5 -mx-0.5 cursor-pointer"
      >
        "{shown}"<span className="text-muted-foreground ml-0.5">…</span>
      </button>
    );
  }

  return (
    <button
      type="button"
      onClick={() => setTruncated(true)}
      title="Click to collapse"
      className="text-left text-emerald-700 dark:text-emerald-400 whitespace-pre-wrap break-words hover:bg-muted/40 rounded px-0.5 -mx-0.5 cursor-pointer font-mono"
    >
      "{value}"
    </button>
  );
}
