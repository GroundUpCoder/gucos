// Fenced-code-block completeness — the streaming guard for rich code renderers.
//
// Why this exists: assistant markdown is rendered INCREMENTALLY as tokens
// arrive. CommonMark closes an unterminated fence at end-of-input, so remark
// happily produces a `code` node for a block that is still half-typed. A rich
// renderer (abc notation today; anything else later) that parses such a
// fragment either throws or draws garbage, and then redraws on the very next
// token — a flicker loop.
//
// The fix is to render richly only on PROOF that the fence is closed. The mdast
// `code` node carries `position` offsets into the original source, and slicing
// the source with them yields the block's raw text INCLUDING its fences (probed
// against mdast-util-from-markdown 2.0.3) — so closure is decidable, not
// guessed. `remarkFenceState` does that slice once per block and stamps the
// answer onto the rendered <code> element as `data-fence-closed`, where the
// CodeBlock component reads it.
//
// This is deliberately language-agnostic: it answers "is this block finished?"
// for every fenced block, so any future rich renderer gets the same guarantee
// without inventing a second mechanism.

/** Opening fence: up to 3 leading spaces, then 3+ backticks or tildes. */
const OPEN_FENCE = /^ {0,3}(`{3,}|~{3,})/;

/**
 * True when `raw` — the source slice of ONE fenced block, fences included — ends
 * with a closing fence matching its opening fence.
 *
 * Conservative by construction: anything we cannot PROVE closed reads as false,
 * because "not yet closed" degrades to a plain code block (always a correct
 * rendering) while a false "closed" degrades to a parse error mid-stream.
 *
 * Per CommonMark, a closing fence uses the same character as the opening one and
 * is at least as long, may be indented up to 3 spaces, and may carry only
 * trailing whitespace.
 */
export function isFenceClosed(raw: string): boolean {
  const open = OPEN_FENCE.exec(raw);
  // No fence at all => an indented code block. Those carry no info string, so
  // they can never reach a language-keyed rich renderer; report "not closed"
  // rather than invent a completeness claim we cannot support.
  if (!open) return false;

  const fenceChar = open[1][0];
  const minLen = open[1].length;

  const lines = raw.split('\n');
  // A lone opening line ("```abc") is the first thing a stream emits.
  if (lines.length < 2) return false;

  const last = lines[lines.length - 1];
  const closer = new RegExp(`^ {0,3}\\${fenceChar}{${minLen},}[ \\t]*$`);
  return closer.test(last);
}

// ─── remark plugin ──────────────────────────────────────────────────────────
// Minimal structural types: we touch only what we set, and avoid taking a
// direct dependency on unist/mdast typings (they arrive transitively through
// react-markdown, and pinning them here would be a version trap for a 12-line
// walk).

type Positioned = {
  type?: string;
  position?: { start?: { offset?: number }; end?: { offset?: number } };
  data?: { hProperties?: Record<string, unknown> } & Record<string, unknown>;
  children?: unknown[];
};

/**
 * Stamps every `code` node with `data-fence-closed="true"|"false"`.
 *
 * mdast-util-to-hast applies `data.hProperties` to the <code> element itself
 * (NOT the wrapping <pre>) — verified against mdast-util-to-hast 13.2.1 — which
 * is exactly the element react-markdown maps to the `code` component.
 */
export function remarkFenceState() {
  return function transformer(tree: Positioned, file: { value?: unknown }) {
    const source = typeof file?.value === 'string' ? file.value : String(file ?? '');

    const walk = (node: Positioned) => {
      if (node?.type === 'code') {
        const start = node.position?.start?.offset;
        const end = node.position?.end?.offset;
        // Without offsets we cannot prove closure; fail closed to the code block.
        const closed =
          typeof start === 'number' && typeof end === 'number'
            ? isFenceClosed(source.slice(start, end))
            : false;
        node.data = node.data ?? {};
        node.data.hProperties = {
          ...(node.data.hProperties ?? {}),
          'data-fence-closed': closed ? 'true' : 'false',
        };
      }
      if (Array.isArray(node?.children)) {
        for (const child of node.children) walk(child as Positioned);
      }
    };

    walk(tree);
  };
}
