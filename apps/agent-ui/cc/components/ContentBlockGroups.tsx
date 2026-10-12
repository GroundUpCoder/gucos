import {useState} from 'react';
import {ChevronRight} from 'lucide-react';
import {ContentBlockView} from './custom/ContentBlockView';
import type {ContentBlock} from '../shared/ai.types';
type RenderItem =
  | { kind: 'block'; block: ContentBlock; idx: number }
  | { kind: 'group'; blocks: ContentBlock[]; startIdx: number };

export function groupContentBlocks(blocks: ContentBlock[]): RenderItem[] {
  const items: RenderItem[] = [];
  let i = 0;
  while (i < blocks.length) {
    if (blocks[i].type !== 'text') {
      const group: ContentBlock[] = [];
      const startIdx = i;
      while (i < blocks.length && blocks[i].type !== 'text') {
        group.push(blocks[i]);
        i++;
      }
      if (group.length === 1) {
        items.push({ kind: 'block', block: group[0], idx: startIdx });
      } else {
        items.push({ kind: 'group', blocks: group, startIdx });
      }
    } else {
      items.push({ kind: 'block', block: blocks[i], idx: i });
      i++;
    }
  }
  return items;
}

function buildGroupLabel(blocks: ContentBlock[]): string {
  const thinking = blocks.filter(b => b.type === 'thinking').length;
  const toolUse = blocks.filter(b => b.type === 'toolUse').length;
  const toolResult = blocks.filter(b => b.type === 'toolResult').length;
  const info = blocks.filter(b => b.type === 'info').length;
  const hasErrors = blocks.some(b => b.type === 'toolResult' && b.isError);

  const parts: string[] = [];
  if (thinking > 0) parts.push(`${thinking} Thinking`);
  if (toolUse > 0) parts.push(`${toolUse} Tool Call${toolUse > 1 ? 's' : ''}`);
  if (toolResult > 0) parts.push(`${toolResult} Result${toolResult > 1 ? 's' : ''}`);
  if (info > 0) parts.push(`${info} Info`);

  let label = parts.join(', ');
  if (hasErrors) label += ' (with errors)';
  return label;
}

export function ContentBlockGroupView({ blocks }: { blocks: ContentBlock[] }) {
  const [expanded, setExpanded] = useState(false);
  const label = buildGroupLabel(blocks);
  const hasErrors = blocks.some(b => b.type === 'toolResult' && b.isError);

  return (
    <div data-testid="block-group" className={`border rounded-lg ${hasErrors ? 'border-destructive/30' : 'border-border'}`}>
      <button
        data-testid="block-group-toggle"
        onClick={() => setExpanded(!expanded)}
        className="w-full flex items-center gap-2 px-3 py-2 text-sm text-muted-foreground hover:text-foreground transition-colors text-left"
      >
        <ChevronRight className={`w-3.5 h-3.5 shrink-0 transition-transform ${expanded ? 'rotate-90' : ''}`} />
        <span className="font-medium">{label}</span>
      </button>
      {expanded && (
        <div data-testid="block-group-content" className="px-3 pb-3 space-y-2">
          {blocks.map((block, i) => (
            <ContentBlockView key={i} block={block} />
          ))}
        </div>
      )}
    </div>
  );
}

