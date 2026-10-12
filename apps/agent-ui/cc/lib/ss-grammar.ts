// SS lexical highlighting, adapted from self-hosting/vscode/syntaxes/ss.tmLanguage.json
// and ss.js at 64468b9f6f5f08927454b6def71529894296c515 (2026-09-07).
// Keep this small Prism grammar in the lazy syntax chunk; no compiler or LSP is needed.
type Grammar = Record<string, unknown>;

export default function ss(Prism: { languages: Record<string, Grammar> }) {
  const interpolation: Grammar = {
    'interpolation-punctuation': { pattern: /^\$\{|\}$/, alias: 'punctuation' },
  };
  const grammar: Grammar = {
    comment: [
      { pattern: /\/\*[\s\S]*?(?:\*\/|$)/, greedy: true },
      { pattern: /\/\/[^\r\n]*/, greedy: true },
    ],
    'triple-string': {
      pattern: /"""(?:\\[\s\S]|(?!""")[^\\])*(?:"""|$)/,
      greedy: true,
      alias: 'string',
    },
    // Same bounded-brace strategy as Prism's JavaScript template strings.
    'template-string': {
      pattern: /`(?:\\[\s\S]|\$\{(?:[^{}]|\{(?:[^{}]|\{[^}]*\})*\})+\}|(?!\$\{)[^\\`])*(?:`|$)/,
      greedy: true,
      inside: {
        'template-punctuation': { pattern: /^`|`$/, alias: 'string' },
        interpolation: {
          pattern: /((?:^|[^\\])(?:\\{2})*)\$\{(?:[^{}]|\{(?:[^{}]|\{[^}]*\})*\})+\}/,
          lookbehind: true,
          inside: interpolation,
        },
        string: /[\s\S]+/,
      },
    },
    string: { pattern: /"(?:\\[\s\S]|[^"\\])*(?:"|$)/, greedy: true },
    char: { pattern: /'(?:\\[\s\S]|[^'\\])*(?:'|$)/, greedy: true },
    // Escaped identifiers are never keywords (e.g. @class).
    'escaped-identifier': { pattern: /@[a-zA-Z_]\w*/, alias: 'variable' },
    number: /\b(?:0[xX][\da-fA-F]+|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)(?:[iu](?:8|16|32|64)|f(?:32|64))?\b/,
    intrinsic: { pattern: /\b__[a-zA-Z_]\w*\b/, alias: 'builtin' },
    'function-definition': { pattern: /(\bfn\s+)[a-zA-Z_]\w*/, lookbehind: true, alias: 'function' },
    'class-definition': { pattern: /(\b(?:class|newtype|interface|package|exception)\s+)[a-zA-Z_]\w*/, lookbehind: true, alias: 'class-name' },
    keyword: /\b(?:if|else|while|for|switch|case|default|break|continue|fallthrough|return|throw|try|catch|finally|using|match|new|as|is|not|instanceof|in|unreachable|fn|class|nominal|newtype|interface|package|var|final|const|static|alias|import|export|exception|from|null|self)\b/,
    boolean: /\b(?:true|false)\b/,
    builtin: /\b(?:i8|i16|i32|i64|f32|f64|u8|u16|u32|u64|bool|str|exnref|externref|anyref|funcref)\b/,
    function: /\b[a-zA-Z_]\w*(?=\s*\()/,
    constant: /\b[A-Z][A-Z0-9_]+\b/,
    'class-name': /\b[A-Z][a-zA-Z0-9_]*\b/,
    operator: /\*\*=?|%%=?|<<=?|>>=?|\?\?|\?\.|\.\.\.|&&|\|\||\+\+|--|[+\-*/%&|^!=<>]=?|[~.]/,
    punctuation: /[{}[\];(),:?$]/,
  };
  interpolation.rest = grammar;
  Prism.languages.ss = grammar;
}

ss.displayName = 'ss';
ss.aliases = [] as string[];
