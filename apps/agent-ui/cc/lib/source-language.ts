export const REGISTERED_SOURCE_LANGUAGES = [
  'javascript', 'jsx', 'typescript', 'tsx', 'json', 'css', 'markup', 'yaml',
  'bash', 'python', 'ruby', 'go', 'rust', 'java', 'c', 'cpp', 'csharp',
  'swift', 'kotlin', 'scala', 'sql', 'graphql', 'docker', 'makefile', 'ini',
  'markdown', 'ss', 'small',
] as const;

export type SourceLanguage = (typeof REGISTERED_SOURCE_LANGUAGES)[number];

const REGISTERED = new Set<string>(REGISTERED_SOURCE_LANGUAGES);

export function isRegisteredSourceLanguage(value: string): value is SourceLanguage {
  return REGISTERED.has(value);
}

const SUFFIX_LANGUAGES: Readonly<Record<string, SourceLanguage | null>> = {
  js: 'javascript', mjs: 'javascript', cjs: 'javascript', jsx: 'jsx',
  ts: 'typescript', mts: 'typescript', cts: 'typescript', tsx: 'tsx',
  json: 'json', jsonc: 'json', css: 'css', html: 'markup', htm: 'markup',
  xml: 'markup', svg: 'markup', yaml: 'yaml', yml: 'yaml', toml: null,
  ini: null, cfg: null, conf: null, env: null, sh: 'bash', bash: 'bash',
  zsh: 'bash', fish: null, py: 'python', rb: 'ruby', go: 'go', rs: 'rust',
  java: 'java', c: 'c', h: 'c', cpp: 'cpp', cc: 'cpp', cxx: 'cpp',
  hpp: 'cpp', hh: 'cpp', hxx: 'cpp', cs: 'csharp', swift: 'swift',
  kt: 'kotlin', kts: 'kotlin', scala: 'scala', sql: 'sql',
  graphql: 'graphql', gql: 'graphql', proto: null,
  md: null, mdx: null, markdown: null, abc: null, txt: null, log: null,
  csv: null, tsv: null, lock: null, ss: 'ss', wc: 'small', gitignore: null,
};

const BYTE_ONLY_SUFFIXES = new Set([
  'png', 'jpg', 'jpeg', 'gif', 'webp', 'ico', 'bmp',
  'mp3', 'wav', 'ogg', 'flac', 'm4a', 'aac',
  'mp4', 'webm', 'mov', 'avi', 'mkv', 'm4v', 'ogv', 'mgp', 'mgpp',
]);

function basename(fileName: string): string {
  return fileName.replace(/\\/g, '/').split('/').pop()?.toLowerCase() ?? '';
}

function suffix(name: string): string {
  const dot = name.lastIndexOf('.');
  return dot >= 0 ? name.slice(dot + 1) : '';
}

export function inferSourceLanguage(fileName: string): SourceLanguage | null {
  const name = basename(fileName);
  const ext = suffix(name);
  if (BYTE_ONLY_SUFFIXES.has(ext)) return null;
  if (Object.prototype.hasOwnProperty.call(SUFFIX_LANGUAGES, ext)) {
    return SUFFIX_LANGUAGES[ext];
  }
  if (name === 'dockerfile' || name.startsWith('dockerfile.')) return 'docker';
  if (name === 'makefile' || name === 'gnumakefile') return 'makefile';
  if (name === 'gemfile' || name === 'rakefile') return 'ruby';
  if (name === 'procfile') return 'bash';
  return null;
}

export function isSourceCodeFile(fileName: string): boolean {
  return inferSourceLanguage(fileName) !== null;
}
