import type { CSSProperties } from 'react';
import SyntaxHighlighter from 'react-syntax-highlighter/dist/esm/prism-light';
import { oneDark } from 'react-syntax-highlighter/dist/esm/styles/prism';
import javascript from 'react-syntax-highlighter/dist/esm/languages/prism/javascript';
import jsx from 'react-syntax-highlighter/dist/esm/languages/prism/jsx';
import typescript from 'react-syntax-highlighter/dist/esm/languages/prism/typescript';
import tsx from 'react-syntax-highlighter/dist/esm/languages/prism/tsx';
import json from 'react-syntax-highlighter/dist/esm/languages/prism/json';
import css from 'react-syntax-highlighter/dist/esm/languages/prism/css';
import markup from 'react-syntax-highlighter/dist/esm/languages/prism/markup';
import yaml from 'react-syntax-highlighter/dist/esm/languages/prism/yaml';
import bash from 'react-syntax-highlighter/dist/esm/languages/prism/bash';
import python from 'react-syntax-highlighter/dist/esm/languages/prism/python';
import ruby from 'react-syntax-highlighter/dist/esm/languages/prism/ruby';
import go from 'react-syntax-highlighter/dist/esm/languages/prism/go';
import rust from 'react-syntax-highlighter/dist/esm/languages/prism/rust';
import java from 'react-syntax-highlighter/dist/esm/languages/prism/java';
import c from 'react-syntax-highlighter/dist/esm/languages/prism/c';
import cpp from 'react-syntax-highlighter/dist/esm/languages/prism/cpp';
import csharp from 'react-syntax-highlighter/dist/esm/languages/prism/csharp';
import swift from 'react-syntax-highlighter/dist/esm/languages/prism/swift';
import kotlin from 'react-syntax-highlighter/dist/esm/languages/prism/kotlin';
import scala from 'react-syntax-highlighter/dist/esm/languages/prism/scala';
import sql from 'react-syntax-highlighter/dist/esm/languages/prism/sql';
import graphql from 'react-syntax-highlighter/dist/esm/languages/prism/graphql';
import docker from 'react-syntax-highlighter/dist/esm/languages/prism/docker';
import makefile from 'react-syntax-highlighter/dist/esm/languages/prism/makefile';
import ini from 'react-syntax-highlighter/dist/esm/languages/prism/ini';
import markdown from 'react-syntax-highlighter/dist/esm/languages/prism/markdown';
import type { SourceLanguage } from './source-language.js';

import ss from './ss-grammar.js';
import small from './small-grammar.js';

const grammars: Record<SourceLanguage, unknown> = {
  javascript, jsx, typescript, tsx, json, css, markup, yaml, bash, python, ruby,
  go, rust, java, c, cpp, csharp, swift, kotlin, scala, sql, graphql, docker, makefile,
  ini, markdown, ss, small,
};
for (const [name, grammar] of Object.entries(grammars)) {
  SyntaxHighlighter.registerLanguage(name, grammar);
}

export type SyntaxRuntimeProps = {
  children: string;
  language: SourceLanguage;
  customStyle?: CSSProperties;
  codeTagProps?: Record<string, unknown>;
};

export function SyntaxHighlighterRuntime({ children, language, customStyle, codeTagProps }: SyntaxRuntimeProps) {
  return (
    <SyntaxHighlighter
      style={oneDark}
      language={language}
      PreTag="pre"
      CodeTag="code"
      customStyle={{ margin: 0, overflow: 'visible', whiteSpace: 'pre', wordBreak: 'normal', ...customStyle }}
      codeTagProps={codeTagProps}
    >
      {children}
    </SyntaxHighlighter>
  );
}
