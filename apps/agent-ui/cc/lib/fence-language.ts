import { isRegisteredSourceLanguage, type SourceLanguage } from './source-language.js';

const FENCE_ALIASES: Readonly<Record<string, SourceLanguage>> = {
  small: 'small', wc: 'small',
  js: 'javascript', javascript: 'javascript', jsx: 'jsx',
  ts: 'typescript', typescript: 'typescript', tsx: 'tsx',
  json: 'json', jsonc: 'json', css: 'css',
  html: 'markup', xml: 'markup', svg: 'markup', markup: 'markup',
  yaml: 'yaml', yml: 'yaml', shell: 'bash', sh: 'bash', bash: 'bash', zsh: 'bash',
  py: 'python', python: 'python', rb: 'ruby', ruby: 'ruby', go: 'go',
  rust: 'rust', java: 'java', c: 'c', 'c++': 'cpp', cpp: 'cpp',
  'c#': 'csharp', cs: 'csharp', csharp: 'csharp', swift: 'swift',
  kotlin: 'kotlin', kt: 'kotlin', scala: 'scala', sql: 'sql',
  graphql: 'graphql', gql: 'graphql', docker: 'docker', dockerfile: 'docker',
  makefile: 'makefile', make: 'makefile',
  ini: 'ini', markdown: 'markdown', md: 'markdown',
};

export function extractFenceLanguage(className?: string): string {
  if (!className) return '';
  for (const token of className.split(/\s+/)) {
    if (token.startsWith('language-') && token.length > 'language-'.length) {
      return token.slice('language-'.length);
    }
  }
  return '';
}

export function normalizeFenceLanguage(label: string): SourceLanguage | null {
  const normalized = label.toLowerCase();
  return FENCE_ALIASES[normalized]
    ?? (isRegisteredSourceLanguage(normalized) ? normalized : null);
}
