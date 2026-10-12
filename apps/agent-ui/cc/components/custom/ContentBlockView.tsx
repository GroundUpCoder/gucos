import { useState, useMemo } from 'react';
import { ChevronRight, Terminal, AlertCircle, Brain, Info, Copy, Check, FileText, Pencil, Search, Globe, Bot, ListChecks, HelpCircle, PanelTop } from 'lucide-react';
import { MemoizedMarkdown } from './MemoizedMarkdown.js';
import { CodeBlock } from './CodeBlock.js';
import { JsonTreeView } from './JsonTreeView.js';
import {GuestTextBlock as TextBlock} from '../../../GuestTextBlock';
import type { ContentBlock } from '../../shared/ai.types.js';
import type { Options as ReactMarkdownOptions } from 'react-markdown';

const VIDEO_EXTENSIONS = new Set(['mp4', 'webm', 'mov', 'avi', 'mkv', 'm4v', 'ogv']);
const AUDIO_EXTENSIONS = new Set(['mp3', 'wav', 'ogg', 'flac', 'm4a', 'aac']);

// === Tool-specific rendering ===

// eslint-disable-next-line @typescript-eslint/no-explicit-any
type ParsedTool = Record<string, any>;

const BASH_KNOWN_FIELDS = new Set(['command', 'description', 'dangerouslyDisableSandbox', 'run_in_background', 'timeout', 'rerun']);
const READ_KNOWN_FIELDS = new Set(['file_path', 'limit', 'offset', 'pages']);
const EDIT_KNOWN_FIELDS = new Set(['file_path', 'old_string', 'new_string', 'replace_all']);
const WRITE_KNOWN_FIELDS = new Set(['file_path', 'content']);
const GREP_KNOWN_FIELDS = new Set(['pattern', 'path', 'type', 'glob', 'output_mode', 'head_limit', 'offset', 'context', '-A', '-B', '-C', '-i', '-n', 'multiline']);
const GLOB_KNOWN_FIELDS = new Set(['pattern', 'path']);
const WEBSEARCH_KNOWN_FIELDS = new Set(['query', 'allowed_domains', 'blocked_domains']);
const WEBFETCH_KNOWN_FIELDS = new Set(['url', 'prompt']);
const AGENT_KNOWN_FIELDS = new Set(['description', 'prompt', 'subagent_type', 'model', 'run_in_background', 'isolation']);
const TODOWRITE_KNOWN_FIELDS = new Set(['todos']);

const EXT_TO_LANG: Record<string, string> = {
  ts: 'typescript', tsx: 'typescript', js: 'javascript', jsx: 'javascript',
  py: 'python', sh: 'bash', bash: 'bash', json: 'json', md: 'markdown',
  css: 'css', html: 'html', sql: 'sql', yaml: 'yaml', yml: 'yaml',
};

const TOOL_ICONS: Record<string, React.ComponentType<{ className?: string }>> = {
  Bash: Terminal, Read: FileText, Edit: Pencil, Write: FileText,
  Grep: Search, Glob: Search, WebSearch: Search, WebFetch: Globe,
  Agent: Bot, TodoWrite: ListChecks,
};

const TODO_ICONS: Record<string, string> = { completed: '✓', in_progress: '◉', pending: '○' };
const TODO_STYLES: Record<string, string> = {
  completed: 'text-green-600 dark:text-green-400',
  in_progress: 'text-blue-600 dark:text-blue-400',
  pending: 'text-muted-foreground',
};

function trySpecialRender(name: string, input: string): { preview: string; content: React.ReactNode } | null {
  let parsed: ParsedTool;
  try { parsed = JSON.parse(input); } catch { return null; }
  const keys = Object.keys(parsed);

  switch (name) {
    case 'Bash': {
      if (!('command' in parsed) || keys.some(k => !BASH_KNOWN_FIELDS.has(k))) return null;
      return {
        preview: (parsed.command || '').slice(0, 80),
        content: (
          <div className="mt-2 ml-5 space-y-1.5">
            {parsed.description && <div className="text-xs text-muted-foreground">{parsed.description}</div>}
            <CodeBlock className="language-bash">{parsed.command}</CodeBlock>
            {(parsed.timeout || parsed.run_in_background || parsed.dangerouslyDisableSandbox || parsed.rerun) && (
              <div className="flex gap-3 text-xs text-muted-foreground">
                {parsed.rerun && <span>rerun: {parsed.rerun}</span>}
                {parsed.timeout && <span>timeout: {parsed.timeout}ms</span>}
                {parsed.run_in_background && <span>background</span>}
                {parsed.dangerouslyDisableSandbox && <span className="text-destructive">sandbox disabled</span>}
              </div>
            )}
          </div>
        ),
      };
    }
    case 'Read': {
      if (!('file_path' in parsed) || keys.some(k => !READ_KNOWN_FIELDS.has(k))) return null;
      return {
        preview: parsed.file_path || '',
        content: (
          <div className="mt-2 ml-5 space-y-1.5">
            <div className="font-mono text-xs px-2 py-1 rounded bg-muted/70">{parsed.file_path}</div>
            {(parsed.limit != null || parsed.offset != null || parsed.pages) && (
              <div className="flex gap-3 text-xs text-muted-foreground">
                {parsed.offset != null && <span>offset: {parsed.offset}</span>}
                {parsed.limit != null && <span>limit: {parsed.limit}</span>}
                {parsed.pages && <span>pages: {parsed.pages}</span>}
              </div>
            )}
          </div>
        ),
      };
    }
    case 'Edit': {
      if (!('file_path' in parsed) || keys.some(k => !EDIT_KNOWN_FIELDS.has(k))) return null;
      return {
        preview: parsed.file_path || '',
        content: (
          <div className="mt-2 ml-5 space-y-1.5">
            <div className="font-mono text-xs px-2 py-1 rounded bg-muted/70">{parsed.file_path}</div>
            {parsed.replace_all && <div className="text-xs text-muted-foreground italic">replace all occurrences</div>}
            {'old_string' in parsed && (
              <pre className="font-mono text-xs px-2 py-1 rounded whitespace-pre-wrap break-words border-l-2 border-red-400 dark:border-red-500/70 bg-red-50 dark:bg-red-950/30 text-red-700 dark:text-red-300">{parsed.old_string}</pre>
            )}
            {'new_string' in parsed && (
              <pre className="font-mono text-xs px-2 py-1 rounded whitespace-pre-wrap break-words border-l-2 border-green-400 dark:border-green-500/70 bg-green-50 dark:bg-green-950/30 text-green-700 dark:text-green-300">{parsed.new_string}</pre>
            )}
          </div>
        ),
      };
    }
    case 'Write': {
      if (!('file_path' in parsed) || keys.some(k => !WRITE_KNOWN_FIELDS.has(k))) return null;
      const ext = (parsed.file_path as string)?.split('.').pop()?.toLowerCase() ?? '';
      const lang = EXT_TO_LANG[ext] ?? '';
      return {
        preview: parsed.file_path || '',
        content: (
          <div className="mt-2 ml-5 space-y-1.5">
            <div className="font-mono text-xs px-2 py-1 rounded bg-muted/70">{parsed.file_path}</div>
            {'content' in parsed && <CodeBlock className={lang ? `language-${lang}` : ''}>{parsed.content}</CodeBlock>}
          </div>
        ),
      };
    }
    case 'Grep': {
      if (!('pattern' in parsed) || keys.some(k => !GREP_KNOWN_FIELDS.has(k))) return null;
      return {
        preview: parsed.pattern || '',
        content: (
          <div className="mt-2 ml-5 space-y-1.5">
            <code className="text-xs font-mono px-2 py-1 rounded bg-muted/70 block">/{parsed.pattern}/</code>
            {keys.length > 1 && (
              <div className="flex flex-wrap gap-3 text-xs text-muted-foreground">
                {parsed.path && <span>path: {parsed.path}</span>}
                {parsed.type && <span>type: {parsed.type}</span>}
                {parsed.glob && <span>glob: {parsed.glob}</span>}
                {parsed.output_mode && <span>mode: {parsed.output_mode}</span>}
                {parsed['-i'] && <span>case-insensitive</span>}
                {parsed.multiline && <span>multiline</span>}
                {parsed['-n'] != null && <span>line numbers: {String(parsed['-n'])}</span>}
                {parsed.context != null && <span>context: {parsed.context}</span>}
                {parsed['-A'] != null && <span>after: {parsed['-A']}</span>}
                {parsed['-B'] != null && <span>before: {parsed['-B']}</span>}
                {parsed['-C'] != null && <span>context: {parsed['-C']}</span>}
                {parsed.head_limit != null && <span>limit: {parsed.head_limit}</span>}
                {parsed.offset != null && <span>offset: {parsed.offset}</span>}
              </div>
            )}
          </div>
        ),
      };
    }
    case 'Glob': {
      if (!('pattern' in parsed) || keys.some(k => !GLOB_KNOWN_FIELDS.has(k))) return null;
      return {
        preview: parsed.pattern || '',
        content: (
          <div className="mt-2 ml-5 space-y-1.5">
            <code className="text-xs font-mono px-2 py-1 rounded bg-muted/70 block">{parsed.pattern}</code>
            {parsed.path && <div className="text-xs text-muted-foreground">path: {parsed.path}</div>}
          </div>
        ),
      };
    }
    case 'WebSearch': {
      if (!('query' in parsed) || keys.some(k => !WEBSEARCH_KNOWN_FIELDS.has(k))) return null;
      return {
        preview: (parsed.query || '').slice(0, 60),
        content: (
          <div className="mt-2 ml-5 space-y-1.5">
            <div className="text-sm font-medium">{parsed.query}</div>
            {Array.isArray(parsed.allowed_domains) && parsed.allowed_domains.length > 0 && (
              <div className="text-xs text-muted-foreground">only: {parsed.allowed_domains.join(', ')}</div>
            )}
            {Array.isArray(parsed.blocked_domains) && parsed.blocked_domains.length > 0 && (
              <div className="text-xs text-muted-foreground">blocked: {parsed.blocked_domains.join(', ')}</div>
            )}
          </div>
        ),
      };
    }
    case 'WebFetch': {
      if (!('url' in parsed) || keys.some(k => !WEBFETCH_KNOWN_FIELDS.has(k))) return null;
      return {
        preview: (parsed.url || '').slice(0, 60),
        content: (
          <div className="mt-2 ml-5 space-y-1.5">
            {parsed.prompt && <div className="text-xs text-muted-foreground">{parsed.prompt}</div>}
            <div className="font-mono text-xs px-2 py-1 rounded bg-muted/70 break-all">{parsed.url}</div>
          </div>
        ),
      };
    }
    case 'Agent': {
      if (!('description' in parsed) || keys.some(k => !AGENT_KNOWN_FIELDS.has(k))) return null;
      return {
        preview: parsed.description || '',
        content: (
          <div className="mt-2 ml-5 space-y-1.5">
            <div className="text-sm font-semibold">{parsed.description}</div>
            {'prompt' in parsed && (
              <div className="text-xs px-2 py-1 rounded bg-muted/70 whitespace-pre-wrap break-words max-h-32 overflow-y-auto">
                {parsed.prompt}
              </div>
            )}
            {(parsed.subagent_type || parsed.model || parsed.run_in_background || parsed.isolation) && (
              <div className="flex flex-wrap gap-3 text-xs text-muted-foreground">
                {parsed.subagent_type && <span>type: {parsed.subagent_type}</span>}
                {parsed.model && <span>model: {parsed.model}</span>}
                {parsed.run_in_background && <span>background</span>}
                {parsed.isolation && <span>isolation: {parsed.isolation}</span>}
              </div>
            )}
          </div>
        ),
      };
    }
    case 'TodoWrite': {
      if (!('todos' in parsed) || keys.some(k => !TODOWRITE_KNOWN_FIELDS.has(k))) return null;
      const todos = Array.isArray(parsed.todos) ? parsed.todos : [];
      return {
        preview: `${todos.length} item${todos.length !== 1 ? 's' : ''}`,
        content: (
          <div className="mt-2 ml-5 space-y-1">
            {todos.map((todo: ParsedTool, i: number) => {
              const style = TODO_STYLES[todo.status] ?? 'text-muted-foreground';
              const icon = TODO_ICONS[todo.status] ?? '○';
              return (
                <div key={i} className={`flex items-start gap-1.5 text-xs ${style}`}>
                  <span className="shrink-0 mt-px font-mono">{icon}</span>
                  <span>{todo.content}</span>
                </div>
              );
            })}
          </div>
        ),
      };
    }
  }
  return null;
}

/** Check if a link looks like a relative file path (not a URL, not an anchor) */
function isRelativePath(href: string): boolean {
  if (!href) return false;
  if (/^https?:\/\//.test(href)) return false;
  if (href.startsWith('#')) return false;
  if (href.startsWith('mailto:')) return false;
  if (href.startsWith('s3://')) return false;
  return true;
}

function isS3Uri(href: string): boolean {
  return href.startsWith('s3://');
}

function mediaKindForExt(ext: string): 'video' | 'audio' | 'img' {
  if (VIDEO_EXTENSIONS.has(ext)) return 'video';
  if (AUDIO_EXTENSIONS.has(ext)) return 'audio';
  return 'img';
}

export function ContentBlockView({ block }: { block: ContentBlock }) {
  switch (block.type) {
    case 'text':
      return <TextBlock content={block.content} />;
    case 'thinking':
      return <ThinkingBlock content={block.content} />;
    case 'toolUse':
      return <ToolUseBlock name={block.name} input={block.input} />;
    case 'toolResult':
      return <ToolResultBlock content={block.content} isError={block.isError} />;
    case 'info':
      return <InfoBlock content={block.content} />;
    case 'unknown':
      return <UnknownBlock label={block.label} data={block.data} truncatedData={block.truncatedData} />;
    default:
      return null;
  }
}

function ThinkingBlock({ content }: { content: string }) {
  const [open, setOpen] = useState(false);
  if (!content.trim()) return null;

  const previewEnd = Math.min(
    content.indexOf('\n') > 0 ? content.indexOf('\n') : 100,
    100,
  );
  const preview = content.slice(0, previewEnd).trim();
  const hasMore = content.length > preview.length;

  return (
    <div>
      <button
        onClick={() => setOpen(!open)}
        className="flex items-center gap-2 text-sm text-muted-foreground hover:text-foreground transition-colors w-full text-left"
      >
        <ChevronRight className={`w-3.5 h-3.5 shrink-0 transition-transform ${open ? 'rotate-90' : ''}`} />
        <Brain className="w-3.5 h-3.5 shrink-0" />
        <span className="font-medium">Thinking</span>
        {!open && preview && (
          <span className="truncate opacity-60 italic">
            {preview}{hasMore && '...'}
          </span>
        )}
      </button>
      {open && (
        <div className="mt-2 ml-5 p-3 rounded-md bg-muted/50 border border-border/50 text-sm text-muted-foreground whitespace-pre-wrap">
          {content}
        </div>
      )}
    </div>
  );
}

function CopyButton({ text, className }: { text: string; className?: string }) {
  const [copied, setCopied] = useState(false);
  const handleCopy = async () => {
    await navigator.clipboard.writeText(text);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };
  return (
    <button
      onClick={handleCopy}
      className={`flex items-center gap-1 text-xs text-muted-foreground hover:text-foreground transition-colors ${className ?? ''}`}
      title="Copy"
    >
      {copied ? <Check className="w-3.5 h-3.5" /> : <Copy className="w-3.5 h-3.5" />}
      {copied && <span>Copied!</span>}
    </button>
  );
}

function ToolUseBlock({ name, input }: { name: string; input: string }) {
  const [open, setOpen] = useState(false);
  const special = useMemo(() => trySpecialRender(name, input), [name, input]);

  const Icon = TOOL_ICONS[name] || Terminal;
  const preview = special?.preview || input.split('\n')[0].slice(0, 60);

  return (
    <div>
      <button
        onClick={() => setOpen(!open)}
        className="flex items-center gap-2 text-sm text-muted-foreground hover:text-foreground transition-colors w-full text-left"
      >
        <ChevronRight className={`w-3.5 h-3.5 shrink-0 transition-transform ${open ? 'rotate-90' : ''}`} />
        <Icon className="w-3.5 h-3.5 shrink-0" />
        <span className="font-medium">{name}</span>
        {!open && preview && (
          <code className="truncate opacity-60 font-mono text-xs">{preview}</code>
        )}
      </button>
      {open && (special?.content || (
        <div className="mt-2 ml-5 rounded-md border border-border/50 overflow-hidden">
          <div className="flex items-center justify-between px-3 py-1.5 bg-[#282c34] border-b border-white/10">
            <span className="text-xs text-neutral-400 font-mono">{name}</span>
            <CopyButton text={input} />
          </div>
          <pre className="px-3 py-2 text-xs overflow-x-auto whitespace-pre-wrap break-all font-mono bg-[#282c34] text-[#abb2bf]">
            {input}
          </pre>
        </div>
      ))}
    </div>
  );
}

function ToolResultBlock({ content, isError }: { content: string; isError?: boolean }) {
  const [open, setOpen] = useState(false);
  const preview = content.slice(0, 100) + (content.length > 100 ? '...' : '');

  return (
    <div>
      <button
        onClick={() => setOpen(!open)}
        className="flex items-center gap-2 text-sm text-muted-foreground hover:text-foreground transition-colors w-full text-left"
      >
        <ChevronRight className={`w-3.5 h-3.5 shrink-0 transition-transform ${open ? 'rotate-90' : ''}`} />
        {isError ? (
          <AlertCircle className="w-3.5 h-3.5 text-destructive shrink-0" />
        ) : (
          <Terminal className="w-3.5 h-3.5 shrink-0" />
        )}
        <span className={`truncate opacity-60 ${isError ? 'text-destructive' : ''}`}>{preview}</span>
      </button>
      {open && (
        <div className={`mt-2 ml-5 rounded-md border overflow-hidden ${isError ? 'border-destructive/30' : 'border-border/50'}`}>
          <div className="flex items-center justify-between px-3 py-1.5 bg-[#282c34] border-b border-white/10">
            <span className="text-xs text-neutral-400 font-mono">
              {isError ? 'error' : 'output'}
            </span>
            <CopyButton text={content} />
          </div>
          <pre className="px-3 py-2 text-xs overflow-x-auto whitespace-pre-wrap break-all max-h-64 overflow-y-auto font-mono bg-[#282c34] text-[#abb2bf]">
            {content}
          </pre>
        </div>
      )}
    </div>
  );
}

function InfoBlock({ content }: { content: string }) {
  return (
    <div className="flex items-center gap-1.5 text-xs text-muted-foreground py-0.5">
      <Info className="w-3.5 h-3.5 shrink-0" />
      <span>{content}</span>
    </div>
  );
}

function UnknownBlock({
  label,
  data,
  truncatedData,
}: {
  label: string;
  data?: unknown;
  truncatedData?: string;
}) {
  const [open, setOpen] = useState(false);
  return (
    <div data-testid="unknown-block">
      <button
        onClick={() => setOpen(v => !v)}
        className="flex items-center gap-2 text-sm text-muted-foreground hover:text-foreground transition-colors w-full text-left"
      >
        <ChevronRight className={`w-3.5 h-3.5 shrink-0 transition-transform ${open ? 'rotate-90' : ''}`} />
        <HelpCircle className="w-3.5 h-3.5 shrink-0 text-amber-600 dark:text-amber-400" />
        <span className="font-medium">{label}</span>
        {truncatedData !== undefined && (
          <span className="text-xs opacity-60 italic">(truncated)</span>
        )}
      </button>
      {open && (
        <div className="mt-2 ml-5 p-3 rounded-md bg-muted/50 border border-border/50">
          {data !== undefined ? (
            <JsonTreeView value={data} maxInitialDepth={2} />
          ) : truncatedData !== undefined ? (
            <pre className="text-xs font-mono whitespace-pre-wrap break-words">{truncatedData}</pre>
          ) : (
            <span className="text-xs text-muted-foreground italic">no data</span>
          )}
        </div>
      )}
    </div>
  );
}
