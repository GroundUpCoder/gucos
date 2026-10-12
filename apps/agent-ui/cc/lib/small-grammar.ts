// Small (.wc) lexical highlighting, adapted from small/vscode/syntaxes/small.tmLanguage.json
// and checked against small.js (working checkout, 2026-09-21). See the development
// journal for source hashes. This lazy Prism grammar does not run the compiler.
type Grammar = Record<string, unknown>;

export default function small(Prism: { languages: Record<string, Grammar> }) {
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
    // Bounded nesting, as in cc's SS/Prism template highlighting. Lexical only:
    // deeply nested or incomplete interpolation can fall back to string coloring.
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
    directive: {
      pattern: /(^[ \t]*)#include\b/m,
      lookbehind: true,
      alias: 'keyword',
    },
    annotation: { pattern: /@[A-Za-z_]\w*/, alias: 'builtin' },
    number: /\b(?:0[xX][\da-fA-F](?:[\da-fA-F_]*[\da-fA-F])?[lL]?|0[bB][01](?:[01_]*[01])?[lL]?|\d(?:[\d_]*\d)?(?:\.\d(?:[\d_]*\d)?)?(?:[eE][+-]?\d(?:[\d_]*\d)?)?[fFdDlL]?)\b/,
    intrinsic: { pattern: /\b__[A-Za-z_]\w*\b/, alias: 'builtin' },
    'class-definition': { pattern: /(\b(?:class|interface|enum|struct)\s+)[A-Za-z_]\w*/, lookbehind: true, alias: 'class-name' },
    keyword: /\b(?:if|else|for|while|do|break|continue|return|switch|case|default|try|catch|finally|throw|constexpr|static_assert|typedef|import|as|class|struct|new|instanceof|template|tuple|static|final|enum|abstract|interface|public|protected|private|func|void|using|namespace|extends|implements|from|constructor)\b/,
    boolean: /\b(?:true|false)\b/,
    constant: /\bnull\b|\b[A-Z][A-Z0-9_]+\b/,
    builtin: /\b(?:auto|bool|byte|short|int|long|float|double|char|str|any|extern|i31|array|this|super)\b/,
    function: /\b[A-Za-z_]\w*(?=\s*(?:<[^;{}()]*>)?\s*\()/,
    'class-name': /\b[A-Z][A-Za-z0-9_]*\b/,
    operator: /<<=?|>>=?|\?\?|\.\.\.|::|&&|\|\||\+\+|--|[+\-*/%&|^!=<>]=?|[~?]/,
    punctuation: /[{}[\];(),.:]/,
  };
  interpolation.rest = grammar;
  Prism.languages.small = grammar;
}

small.displayName = 'small';
small.aliases = [] as string[];
