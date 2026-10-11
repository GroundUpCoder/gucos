// Browser compiler driver: structured emission and selective continuations.
function preprocessor(files, includes, extra) {
  const C = self.CompilerJS;
  const pp = C.createDefaultPPRegistry(extra.compilerOptions);
  pp.sourceDateEpoch = 0;
  pp.pathIdentity = normalize;
  for (const define of extra.defines || []) { const at=define.indexOf('='); pp.defines.set(at<0?define:define.slice(0,at),at<0?'1':define.slice(at+1)); }
  for (const name of extra.undefines || []) pp.defines.delete(name);
  pp.fileReader = path => { const text = files[normalize(path)] ?? null; if (text !== null && extra.onRead) extra.onRead(normalize(path)); return text === null ? null : C.decodeCompilerSource(text, extra.sourceEncoding); };
  pp.includePaths = [...new Set([...includes.map(normalize), '/usr/local/include', '/usr/include'])];
  pp.sourceRoots = (extra.sourceRoots || []).map(r => ({ prefix: r.prefix, dir: normalize(r.dir) }));
  pp.standardHeaders.set('SDL3/SDL.h', '#include <SDL.h>\n');
  return pp;
}
function compile(files, inputs, includes = [], writeErr = () => {}, extra = {}) {
  const C = self.CompilerJS;
  if(extra.compilerOptions?.shared||extra.compilerOptions?.dynamic){
    const objects=inputs.map(input=>({name:input,bytes:compileObject(files,input,includes,writeErr,extra).bytes}));
    const archives=extra.compilerOptions.shared?[]:[{name:'/usr/lib/libc.a',bytes:canonicalLibcArchive(writeErr,extra).bytes}];
    return linkProgram(objects,archives,writeErr,extra);
  }
  const pp = preprocessor(files, includes, extra);
  const compilerOptions = { gcSections: true, emitNames: true, ...(extra.compilerOptions || {}) };
  const options = { compilerOptions, writeErr, onControlFlowIR: extra.onControlFlowIR, onSSA: extra.onSSA, onLoweredFunction: extra.onLoweredFunction, fatalExit: code => { throw new Error(`Compiler failed (${code})`); } };
  const fs = { readFileSync(path) {
    const text = pp.fileReader(path);
    if (text === null) throw new Error(`No such source: ${path}`);
    return text;
  }};
  const start = performance.now();
  const units = C.parseAllUnits(fs, pp, inputs.map(normalize), options);
  if(units.some(unit => unit.requiredSources.has("__dl.c")))return compile(files,inputs,includes,writeErr,{...extra,compilerOptions:{...compilerOptions,dynamic:true}});
  const afterFrontend=performance.now();
  const linked = C.linkTranslationUnits(units, compilerOptions);
  if (linked.errors.length) {
    // Header-free declarations need the same archive resolution as object
    // inputs. Keep the source-link path for programs it already resolves.
    if (linked.errors.every(e => /^Undefined symbol '/.test(e.message))) {
      const afterSourceLink = performance.now();
      // Recompile the original roots, never filenames inferred from tokens
      // in headers or included .c files. Explicit filesystem requirements
      // are separate translation units recorded by source resolution.
      const sourceFiles = [...inputs.map(normalize), ...units.requiredSourceFiles];
      const objects = sourceFiles.map(input => ({name:input, bytes:compileObject(files,input,includes,writeErr,extra).bytes}));
      const afterObjects = performance.now();
      const libc = canonicalLibcArchive(writeErr, extra);
      const afterLibc = performance.now();
      const result = linkProgram(objects, [{name:'libc.a', bytes:libc.bytes}], writeErr, {...extra, compilerOptions});
      return {bytes:result.bytes, stats:{...result.stats,
        compilationMode:'object-fallback',
        fallback:{reason:'unresolved-source-symbols',
          symbols:linked.errors.map(e => /^Undefined symbol '([^']+)'/.exec(e.message)[1]),
          sources:sourceFiles, libc:libc.origin,
          garbageCollection:'linker-reachability'},
        phaseMilliseconds:{frontend:afterFrontend-start, sourceLink:afterSourceLink-afterFrontend,
          objectCompilation:afterObjects-afterSourceLink, libcBuild:afterLibc-afterObjects,
          ...result.stats.phaseMilliseconds},
        milliseconds:Math.round(performance.now()-start)}};
    }
    throw new Error(linked.errors.map(e => e.message).join('\n'));
  }
  C.gcSectionsPass(units, compilerOptions);
  const afterLink=performance.now();
  let trampoline; options.onTrampoline=stats=>trampoline=stats;
  const functions = [];
  const dispatchFunctions = [];
  options.onEmittedFunction = fn => functions.push(fn.name);
  const restructuredFunctions = [];
  options.onLoweredFunction = fn => {
    (fn.controlFlow.restructured === "non-ssa-structured" ? restructuredFunctions : dispatchFunctions).push(fn.name);
    extra.onLoweredFunction?.(fn);
  };
  const bytes = C.generateCode(units, 'program.wasm', options);
  return { bytes, stats: { compilationMode:'whole-program', functions: functions.length, dispatchFunctions: dispatchFunctions.length, restructuredFunctions: restructuredFunctions.length,
    names: functions, trampoline, bytes: bytes.length, phaseMilliseconds:{frontend:afterFrontend-start,linkAndGC:afterLink-afterFrontend,backend:performance.now()-afterLink}, milliseconds: Math.round(performance.now() - start) } };
}

// Separate compilation (docs/OBJECTS.md): one translation unit to a
// relocatable object. Undefined functions and objects become symbols the
// linker resolves; required sources are recorded, not compiled.
function compileObject(files, input, includes = [], writeErr = () => {}, extra = {}) {
  const C = self.CompilerJS;
  const pp = preprocessor(files, includes, extra);
  const compilerOptions = { gcSections: false, emitNames: true, ...(extra.compilerOptions || {}), relocatable: true, allowUndefined: true };
  const options = { compilerOptions, writeErr, onControlFlowIR: extra.onControlFlowIR, onSSA: extra.onSSA, onLoweredFunction: extra.onLoweredFunction, fatalExit: code => { throw new Error(`Compiler failed (${code})`); } };
  const fs = { readFileSync(path) {
    const text = pp.fileReader(path);
    if (text === null) throw new Error(`No such source: ${path}`);
    return text;
  }};
  const start = performance.now();
  const units = C.parseAllUnits(fs, pp, [normalize(input)], options);
  const linked = C.linkTranslationUnits(units, compilerOptions);
  if (linked.errors.length) throw new Error(linked.errors.map(e => e.message).join('\n'));
  const result = C.generateCode(units, 'object.wasm', options);
  const bytes = self.WasmObjects.writeObject(result, { sourceName: extra.sourceName || normalize(input) });
  return { bytes, stats: { functions: result.wmod.funcDefs.length, bytes: bytes.length, milliseconds: Math.round(performance.now() - start) } };
}
// Link objects and archives into a program. `objects` and `archives` are
// [{name, bytes}]; the C library archive is passed by the caller.
function linkProgram(objects, archives, writeErr = () => {}, extra = {}) {
  const start = performance.now();
  const result = self.WasmObjects.link(objects, archives, {
    writeErr, requiredMembers: ['__alloca.c', ...(extra.requiredMembers || [])],
    compilerOptions: extra.compilerOptions || {}, onTrampoline: extra.onTrampoline, onControlFlowIR: extra.onControlFlowIR, onSSA: extra.onSSA,
  });
  result.stats.milliseconds = Math.round(performance.now() - start);
  return result;
}
// The C library as an archive of one object per builtin source. Sources
// that use Wasm reference types cannot take the memory-frame ABI and are
// left out by the provider manifest.
// Only target/ABI choices belong to a canonical libc build. User macros,
// encodings, source roots, diagnostics hooks and optimization flags do not.
function libcCompilerOptions(options = {}) {
  const target=Object.fromEntries(self.CompilerJS.getStdlibProvider().manifest.abiOptions.map(name=>[name,options[name]===true]));
  target.sse=target.simd128 && target.sse;
  target.sse2=target.simd128 && target.sse2;
  return target;
}
// Cache only successful implicit builds. A compiler replacement gets its own
// cache; user-supplied archives and explicit rebuilds never populate it.
const canonicalLibcArchives = new WeakMap();
function canonicalLibcArchive(writeErr, extra = {}) {
  if (extra.libcArchive) return {bytes:extra.libcArchive, origin:'supplied'};
  const compiler = self.CompilerJS;
  const compilerOptions = libcCompilerOptions(extra.compilerOptions);
  const key = JSON.stringify(compilerOptions);
  let archives = canonicalLibcArchives.get(compiler)?.get(compiler.getStdlibProvider());
  if (!archives) {
    let providers=canonicalLibcArchives.get(compiler);
    if (!providers) canonicalLibcArchives.set(compiler, providers=new WeakMap());
    providers.set(compiler.getStdlibProvider(), archives=new Map());
  }
  if (archives.has(key)) return {bytes:archives.get(key), origin:'cached'};
  const bytes = buildLibc(writeErr, {compilerOptions}).bytes;
  archives.set(key, bytes);
  return {bytes, origin:'built'};
}
function buildLibc(writeErr = () => {}, extra = {}) {
  const C = self.CompilerJS;
  const members = [];
  const skip = new Set(C.getStdlibProvider().manifest.skip);
  const compilerOptions = {...libcCompilerOptions(extra.compilerOptions), preserveHostImports:true};
  for (const [name, source] of Object.entries(C.getStdlibSources())) {
    if (skip.has(name)) continue;
    const files = { ['/' + name]: source };
    // Dynamic executables expose host-backed libc declarations to dlsym too.
    // Static links still discard unused imports during object reachability.
    const object = compileObject(files, '/' + name, [], writeErr, {compilerOptions, sourceName:name});
    members.push({ name, bytes: object.bytes });
  }
  return { bytes: self.WasmObjects.writeArchive(members), members: members.length };
}

// The shared process runtime excludes optional graphics integrations. Its
// pthread_create asks the loader for the application TLS layout, not libc's.
function buildSharedLibc(archive, writeErr=()=>{}, enableFork=false) {
  const members=self.WasmObjects.readArchive(archive,'libc.a');
  const objects=members.filter(m=>!/^__(?:SDL|sdl3|webgpu|emscripten)/.test(m.name)).map(m=>({name:m.name,bytes:m.bytes}));
  const pthread=objects.find(o=>o.name==='__pthread.c');
  pthread.bytes=compileObject({'/__pthread.c':self.CompilerJS.getStdlibSources()['__pthread.c']},'/__pthread.c',[],writeErr,
    {defines:['__WASM_POSIX_SHARED_LIBC'],compilerOptions:{preserveHostImports:true}}).bytes;
  return linkProgram(objects,[],writeErr,{compilerOptions:{shared:true,runtime:true,enableFork,soname:enableFork?'libc-fork.so.1':'libc.so.1'}});
}
