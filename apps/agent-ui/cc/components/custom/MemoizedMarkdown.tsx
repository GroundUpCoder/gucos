import { memo, useMemo } from 'react';
import ReactMarkdown, { type Options as ReactMarkdownOptions, defaultUrlTransform } from 'react-markdown';
import remarkGfm from 'remark-gfm';
import { CodeBlock } from './CodeBlock.js';
import { remarkFenceState } from '../../lib/fenced-block.js';

type MemoizedMarkdownProps = {
  children: string;
  components?: ReactMarkdownOptions['components'];
};

const defaultComponents: ReactMarkdownOptions['components'] = {
  code: CodeBlock,
  pre: ({ children }) => <div className="not-prose">{children}</div>,
  table: ({ children }) => (
    <div className="overflow-x-auto">
      <table>{children}</table>
    </div>
  ),
};

// react-markdown's default urlTransform strips schemes outside its allowlist
// (http/https/mailto/tel + relative). s3:// is needed so backend can issue
// presigned URLs for images that live in S3; without this passthrough the
// scheme gets silently dropped before our img/a component handlers see it.
const urlTransform: NonNullable<ReactMarkdownOptions['urlTransform']> = (url) => {
  if (url.startsWith('s3://')) return url;
  return defaultUrlTransform(url);
};

export const MemoizedMarkdown = memo(function MemoizedMarkdown({
  children,
  components,
}: MemoizedMarkdownProps) {
  const mergedComponents = useMemo(
    () => (components ? { ...defaultComponents, ...components } : defaultComponents),
    [components]
  );

  return (
    <ReactMarkdown
      remarkPlugins={[remarkGfm, remarkFenceState]}
      components={mergedComponents}
      urlTransform={urlTransform}
    >
      {children}
    </ReactMarkdown>
  );
});
