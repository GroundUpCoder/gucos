import { useState, useCallback, type ReactNode } from 'react';
import { ChevronRight, Check, Copy } from 'lucide-react';

type MessageInfoBarProps = {
  collapsed: boolean;
  onToggleCollapse: () => void;
  children: ReactNode;
  copyText?: string;
  /** Extra action buttons rendered just left of the copy button. */
  actions?: ReactNode;
  className?: string;
};

export function MessageInfoBar({
  collapsed,
  onToggleCollapse,
  children,
  copyText,
  actions,
  className,
}: MessageInfoBarProps) {
  const [copied, setCopied] = useState(false);

  const handleCopy = useCallback(async () => {
    if (!copyText) return;
    await navigator.clipboard.writeText(copyText);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  }, [copyText]);

  const hasCopyText = copyText && copyText.length > 0;

  return (
    <div className={`flex items-start gap-1 ${className ?? ''}`}>
      <button
        type="button"
        onClick={onToggleCollapse}
        aria-expanded={!collapsed}
        aria-label={collapsed ? 'Expand message' : 'Collapse message'}
        className="shrink-0 p-0.5 text-muted-foreground hover:text-foreground transition-colors"
        title={collapsed ? 'Expand message' : 'Collapse message'}
      >
        <ChevronRight
          className={`w-4 h-4 transition-transform ${!collapsed ? 'rotate-90' : ''}`}
        />
      </button>

      <div className="flex-1 min-w-0">
        {children}
      </div>

      {actions}

      {hasCopyText && (
        <button
          onClick={handleCopy}
          className="shrink-0 text-muted-foreground hover:text-foreground transition-colors p-1"
          title={copied ? 'Copied!' : 'Copy message'}
        >
          {copied ? <Check className="w-4 h-4" /> : <Copy className="w-4 h-4" />}
        </button>
      )}
    </div>
  );
}
