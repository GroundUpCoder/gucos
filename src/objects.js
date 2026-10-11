// Relocatable object files and the linker (docs/OBJECTS.md).
//
// An object is a WebAssembly module whose function bodies carry padded
// immediates for every index the linker patches, plus a "wasmposix.obj"
// custom section with the symbol table, relocations, static-data ranges and
// the per-function facts the memory-frame backend needs. The linker merges
// objects and archive members, keeps only the reachable functions and data,
// then lowers the result through the same trampoline and finalization as a
// whole-program compile.
self.WasmObjects = (() => {
const OBJECT_BASE = 256 * 65536;      // static data address 0 inside an object
const UNDEFINED_BASE = 0x80000000;    // placeholder addresses of objects defined elsewhere
const SECTION = "wasmposix.obj";
const VERSION = 9;
const SCALARS = "wasmposix.scalar-globals";
const UNWIND_TYPES = "wasmposix.noexcept-types";

// ---- byte helpers ----
function lebU(out, v) { v >>>= 0; do { let b = v & 0x7F; v >>>= 7; if (v) b |= 0x80; out.push(b); } while (v); }
function lebI(out, v) { v |= 0; for (;;) { const b = v & 0x7F; v >>= 7; if ((v === 0 && !(b & 0x40)) || (v === -1 && (b & 0x40))) { out.push(b); return; } out.push(b | 0x80); } }
function str(out, s) { const bytes = new TextEncoder().encode(s); lebU(out, bytes.length); for (const b of bytes) out.push(b); }
class Reader {
  constructor(bytes, pos = 0, end = bytes.length) { this.b = bytes; this.p = pos; this.end = end; }
  u8() { if (this.p >= this.end) throw new Error("object file truncated"); return this.b[this.p++]; }
  u32() { let r = 0, s = 0, b; do { b = this.u8(); r |= (b & 0x7F) << s; s += 7; } while (b & 0x80); return r >>> 0; }
  i32() { let r = 0, s = 0, b; do { b = this.u8(); r |= (b & 0x7F) << s; s += 7; } while (b & 0x80); if (s < 32 && (b & 0x40)) r |= -1 << s; return r | 0; }
  i64() { let r = 0n, s = 0n, b; do { b = this.u8(); r |= BigInt(b & 0x7F) << s; s += 7n; } while (b & 0x80); if (s < 64n && (b & 0x40)) r |= -1n << s; return BigInt.asIntN(64, r); }
  f32() { const v = new DataView(this.b.buffer, this.b.byteOffset + this.p, 4).getFloat32(0, true); this.p += 4; return v; }
  f64() { const v = new DataView(this.b.buffer, this.b.byteOffset + this.p, 8).getFloat64(0, true); this.p += 8; return v; }
  str() { const n = this.u32(); const s = new TextDecoder().decode(this.b.subarray(this.p, this.p + n)); this.p += n; return s; }
  bytes(n) { const s = this.b.subarray(this.p, this.p + n); this.p += n; return s; }
  done() { return this.p >= this.end; }
}

// Linking consumes metadata, not executable machine code. Compiling a large
// provider merely to inspect its custom section duplicates the runtime's JIT
// memory in the compiler worker. Validation retains malformed-input diagnostics.
// Contract-only providers may be executable images; they are never dependencies.
function readSharedLibrary(bytes, requireShared=true) {
  if (!WebAssembly.validate(bytes)) throw new Error("Invalid shared WebAssembly module");
  const r = new Reader(bytes, 8);
  let info = null, count = 0;
  while (!r.done()) {
    const id = r.u8(), length = r.u32(), end = r.p + length;
    if (id === 0) {
      const section = new Reader(bytes, r.p, end);
      if (section.str() === "wasmposix.dylink") {
        if (++count !== 1) throw new Error("Duplicate dynamic metadata");
        info = JSON.parse(new TextDecoder().decode(section.bytes(end - section.p)));
      }
    }
    r.p = end;
  }
  if (![1,2,3,4].includes(info?.version) || (requireShared && !info.shared)) throw new Error("Not a wasm-posix shared library");
  return info;
}

// ---- value types ----
function W() { return self.CompilerJS.WAST; }
function wtOf(byte) {
  const w = W();
  switch (byte) { case 0x7F: return w.WT_I32; case 0x7E: return w.WT_I64; case 0x7D: return w.WT_F32; case 0x7C: return w.WT_F64; case 0x7B: return w.WT_V128; }
  throw new Error(`object files hold numeric C code only (value type 0x${byte.toString(16)})`);
}
function wtByte(wt) { if (wt.tag !== "num") throw new Error("object files hold numeric C code only"); return wt.num; }

// ---- object writer ----
// `result` is generateCode's relocatable output: {wmod, cg, units}.
function writeObject(result, opts) {
  const { wmod, cg, units } = result;
  const w = W();
  const out = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];
  const section = (id, content) => { out.push(id); lebU(out, content.length); for (const b of content) out.push(b); };
  let buf = [];
  // Type section: function types only.
  lebU(buf, wmod.typeDefs.length);
  for (const td of wmod.typeDefs) {
    if (td.kind !== "func") throw new Error("object files hold numeric C code only (GC types)");
    buf.push(0x60); lebU(buf, td.params.length); for (const p of td.params) buf.push(wtByte(p));
    lebU(buf, td.results.length); for (const r of td.results) buf.push(wtByte(r));
  }
  section(1, buf);
  buf = [];
  if (wmod.globalImports.length) throw new Error("object files hold numeric C code only (imported globals)");
  lebU(buf, wmod.funcImports.length);
  for (const imp of wmod.funcImports) { str(buf, imp.moduleName); str(buf, imp.functionName); buf.push(0x00); lebU(buf, imp.typeId); }
  section(2, buf);
  buf = []; lebU(buf, wmod.funcDefs.length); for (const d of wmod.funcDefs) lebU(buf, d.typeId); section(3, buf);
  // Globals: the three runtime globals at the indices function bodies use.
  buf = []; lebU(buf, wmod.globals.length);
  for (const g of wmod.globals) { buf.push(wtByte(g.type)); buf.push(g.isMutable ? 1 : 0); for (const b of g.initExpr) buf.push(b); }
  section(6, buf);
  // Code section with relocatable immediates.
  const funcs = [];
  buf = []; lebU(buf, wmod.funcDefs.length);
  wmod.funcDefs.forEach((def, di) => {
    const body = [];
    lebU(body, def.locals.length);
    for (const loc of def.locals) { lebU(body, loc.count); body.push(wtByte(loc.type)); }
    const info = { relocs: [], calls: new Set(), regions: [], srcLocs: [], controlFlow: def.controlFlow || null, fnMeta: def.fnMeta || null, summary: self.CompilerJS.FUNCTION_EFFECTS.summarize(w, def, cg.stackPointerGlobalIdx) };
    const start = body.length;
    if (!def.wast) throw new Error("object writer needs instruction trees");
    w.serialize(def.wast, body, {
      onOrdinaryMemory: offset => (info.summary.ordinaryMemory ||= []).push(offset),
      onStackRestore: offset => (info.summary.stackRestores ||= []).push(offset),
      onSrcLoc: (offset, fileIdx, line) => info.srcLocs.push({ offset, fileIdx, line }),
      onReloc: (offset, kind, value, node) => {
        if ((kind === "call" || kind === "type") && node.continuationRegions) info.regions.push({ offset, regions: node.continuationRegions });
        if (kind === "type" && node.noCapture === true) (info.summary.nonCapturingCalls ||= []).push(offset);
        if ((kind === "call" || kind === "type") && node.noUnwind === true) (info.summary.noUnwindCalls ||= []).push(offset);
        if (kind === "call") {
          info.calls.add(value);
        } else if (kind === "data" || kind === "func" || kind === "tls") info.relocs.push({ offset, kind, value: value >>> 0 });
      },
    });
    body.push(0x0B);
    funcs.push(info);
    lebU(buf, body.length); for (const b of body) buf.push(b);
    void start;
  });
  section(10, buf);
  // Data: sparse active segments at blob offsets.
  const data = cg.staticData, segments = [];
  { let i = 0; const n = data.length;
    while (i < n) {
      while (i < n && data[i] === 0) i++;
      if (i >= n) break;
      const spanStart = i; let last = i;
      while (i < n) {
        if (data[i] !== 0) { last = i++; continue; }
        let z = i; while (z < n && data[z] === 0) z++;
        if (z - i >= 64 || z >= n) break;
        i = z;
      }
      segments.push({ offset: spanStart, bytes: data.slice(spanStart, last + 1) });
    } }
  buf = []; lebU(buf, segments.length);
  for (const seg of segments) { buf.push(0x00, 0x41); lebI(buf, seg.offset); buf.push(0x0B); lebU(buf, seg.bytes.length); for (const b of seg.bytes) buf.push(b); }
  section(11, buf);
  // Symbols.
  const isStatic = (d) => d.storageClass === self.CompilerJS.Types.StorageClass.STATIC;
  const funcByIdx = new Map();
  for (const [fdef, idx] of cg.funcDefToWasmFuncIdx) funcByIdx.set(idx, fdef);
  const exportsOf = new Map();
  for (const unit of units) for (const [name, decl] of unit.exportDirectives) {
    const fdef = decl.definition || decl; const idx = cg.funcDefToWasmFuncIdx.get(fdef);
    if (idx === undefined) continue;
    if (!exportsOf.has(idx)) exportsOf.set(idx, []); exportsOf.get(idx).push(name);
  }
  const nameOf = new Map(wmod.funcNames.map((n) => [n.idx, n.name]));
  const funcSymbols = [];
  const total = wmod.funcImports.length + wmod.funcDefs.length;
  for (let idx = 0; idx < total; idx++) {
    const fdef = funcByIdx.get(idx);
    const imported = idx < wmod.funcImports.length;
    let flags = imported ? 0 : 1;
    if (fdef && isStatic(fdef)) flags |= 2;
    if (!fdef && !imported) flags |= 2;
    // An external inline definition is emitted by every object that has it.
    if (fdef && !imported && fdef.isInline && !(flags & 2)) flags |= 16;
    if (imported && fdef && fdef.__undefinedExtern) flags |= 4;
    if (imported && !(flags & 4)) flags |= 8;
    const imp = imported ? wmod.funcImports[idx] : null;
    funcSymbols.push({ name: fdef ? fdef.name : (nameOf.get(idx) || `__fn${idx}`), flags, idx, memoryABI:self.CompilerJS.Types.memoryABI(fdef?.type), noUnwindABI:self.CompilerJS.Types.noUnwindABI(fdef?.type),
      declaredThrows:!!fdef?.declaredThrows, requiresNoUnwind:!!fdef?.requiresNoUnwind,
      module: imp ? imp.moduleName : "", importName: imp ? imp.functionName : "", exports: exportsOf.get(idx) || [] });
  }
  const dataSymbols = [], dataIndexOf = new Map();
  const seenVars = new Set();
  const addVar = (v) => {
    const d = v.definition || v;
    if (seenVars.has(d)) return; seenVars.add(d);
    const addr = cg.globalArrayAddrs.get(d);
    if (addr === undefined) return;
    if (d.__undefinedExtern) {
      const u = cg.undefinedObjects.find((x) => x.decl === d);
      dataIndexOf.set(d, dataSymbols.length);
      dataSymbols.push({ name: d.name, memoryABI:self.CompilerJS.Types.memoryABI(d.type), noUnwindABI:self.CompilerJS.Types.noUnwindABI(d.type), flags: d.isThreadLocal ? 32 : 0, offset: u.base >>> 0, size: u.size });
    } else {
      dataIndexOf.set(d, dataSymbols.length);
      dataSymbols.push({ name: d.name, memoryABI:self.CompilerJS.Types.memoryABI(d.type), noUnwindABI:self.CompilerJS.Types.noUnwindABI(d.type), flags: 1 | (isStatic(d) ? 2 : 0) | (d.isThreadLocal ? 32 : 0), offset: addr - OBJECT_BASE, size: cg.sizeOf(d.type) });
    }
  };
  for (const unit of units) for (const v of [...unit.definedVariables, ...unit.externVariables, ...unit.localExternVariables]) addVar(v);
  const tags = [];
  for (const [tag, idx] of cg.exceptionToWasmTagIdx) tags.push({ name: tag.name, idx, typeIdx: wmod.tags[idx].typeIdx });
  const ranges = cg.staticRanges.map((r) => {
    if (r.owner.kind === "func") { const idx = cg.funcDefToWasmFuncIdx.get(r.owner.decl); return { start: r.start, len: r.len, kind: 1, idx: idx === undefined ? 0 : idx }; }
    if (r.owner.kind === "data") { const di = dataIndexOf.get(r.owner.decl); return { start: r.start, len: r.len, kind: di === undefined ? 0 : 2, idx: di || 0 }; }
    return { start: r.start, len: r.len, kind: 0, idx: 0 };
  });
  buf = [];
  str(buf, SECTION);
  lebU(buf, VERSION); lebU(buf, OBJECT_BASE); lebU(buf, data.length); lebU(buf, cg.minStackBytes || 0);
  str(buf, (opts && opts.sourceName) || "");
  lebU(buf, funcSymbols.length);
  for (const s of funcSymbols) { str(buf, s.name); lebU(buf, s.flags); lebU(buf, s.idx); str(buf, s.module); str(buf, s.importName); lebU(buf, s.exports.length); for (const e of s.exports) str(buf, e); }
  lebU(buf, dataSymbols.length);
  for (const s of dataSymbols) { str(buf, s.name); lebU(buf, s.flags); lebU(buf, s.offset); lebU(buf, s.size); }
  lebU(buf, tags.length); for (const t of tags) { str(buf, t.name); lebU(buf, t.idx); lebU(buf, t.typeIdx); }
  lebU(buf, ranges.length); for (const r of ranges) { lebU(buf, r.start); lebU(buf, r.len); lebU(buf, r.kind); lebU(buf, r.idx); }
  lebU(buf, cg.dataRelocs.length); for (const r of cg.dataRelocs) { lebU(buf, r.offset); lebU(buf, r.value >>> 0); }
  lebU(buf, funcs.length);
  for (const f of funcs) {
    lebU(buf, f.relocs.length); for (const r of f.relocs) { lebU(buf, r.offset); lebU(buf, r.kind === "func" ? 1 : r.kind === "tls" ? 2 : 0); lebU(buf, r.value); }
    lebU(buf, f.calls.size); for (const c of f.calls) lebU(buf, c);
    lebU(buf, f.regions.length);
    for (const g of f.regions) { lebU(buf, g.offset); lebU(buf, g.regions.length); for (const r of g.regions) { lebU(buf, r.slot !== undefined ? 1 : 0); lebU(buf, r.slot !== undefined ? r.slot : r.local); lebU(buf, r.offset || 0); lebU(buf, r.size); } }
    lebU(buf, f.srcLocs.length); for (const l of f.srcLocs) { lebU(buf, l.offset); lebU(buf, l.fileIdx); lebU(buf, l.line); }
    str(buf, JSON.stringify({ controlFlow: f.controlFlow, fnMeta: f.fnMeta, summary: f.summary }));
  }
  lebU(buf, cg.sourceMapFiles.length); for (const f of cg.sourceMapFiles) str(buf, f);
  const required = units.requiredSources || [], conditional = units.conditionalSources || [];
  lebU(buf, required.length); for (const r of required) str(buf, r);
  lebU(buf, conditional.length); for (const c of conditional) { str(buf, c.symbol); str(buf, c.source); }
  lebU(buf,cg.tlsRanges.length);
  for(const r of cg.tlsRanges){lebU(buf,r.addr-OBJECT_BASE);lebU(buf,r.size);lebU(buf,r.align);}
  lebU(buf,cg.staticAlignment||16);
  const constructors = result.constructors || [];
  lebU(buf,constructors.length);
  for(const c of constructors){lebU(buf,c.idx);lebU(buf,c.priority);}
  str(buf,JSON.stringify(funcSymbols.map(s=>s.memoryABI)));
  str(buf,JSON.stringify(dataSymbols.map(s=>s.memoryABI)));
  section(0, buf);
  if ([...funcSymbols,...dataSymbols].some(s=>s.noUnwindABI || s.declaredThrows || s.requiresNoUnwind)) {
    buf=[];str(buf,UNWIND_TYPES);str(buf,JSON.stringify({functions:funcSymbols.map(s=>s.noUnwindABI),data:dataSymbols.map(s=>s.noUnwindABI),throws:funcSymbols.map(s=>s.declaredThrows),required:funcSymbols.map(s=>s.requiresNoUnwind)}));section(0,buf);
  }
  // Optional proof hints, ignored by older linkers. Machine code and the
  // object ABI remain conservative and independently linkable without them.
  const scalars=[];
  for(const [d,addr] of cg.globalArrayAddrs){
    const t=d.type.removeQualifiers(),size=cg.sizeOf(t);
    if(d.__undefinedExtern||d.addressTaken||d.isThreadLocal||!t.isScalar()||![4,8].includes(size)||self.CompilerJS.Types.memoryABI(d.type))continue;
    const type=t.isFloatingPoint()?(size===4?0x7d:0x7c):(size===4?0x7f:0x7e);
    scalars.push({name:d.name,offset:addr-OBJECT_BASE,size,type});
  }
  buf=[];str(buf,SCALARS);str(buf,JSON.stringify(scalars));section(0,buf);
  // Name section for external tools.
  buf = []; str(buf, "name"); { const sub = []; lebU(sub, funcSymbols.length); for (const s of funcSymbols) { lebU(sub, s.idx); str(sub, s.name); } buf.push(0x01); lebU(buf, sub.length); for (const b of sub) buf.push(b); }
  section(0, buf);
  if(self.CompilerJS.SIMD128.moduleUsesSIMD(wmod)) {
    buf=[];str(buf,"target_features");lebU(buf,1);buf.push(0x2b);str(buf,"simd128");section(0,buf);
  }
  return new Uint8Array(out);
}

// ---- object reader ----
function readObject(bytes, name) {
  if (bytes.length < 8 || bytes[0] !== 0 || bytes[1] !== 0x61 || bytes[2] !== 0x73 || bytes[3] !== 0x6d) throw new Error(`${name}: not a wasm object file`);
  const obj = { name, types: [], imports: [], funcTypes: [], bodies: [], segments: [], info: null };
  const r = new Reader(bytes, 8);
  while (!r.done()) {
    const id = r.u8(), size = r.u32(), end = r.p + size;
    const s = new Reader(bytes, r.p, end);
    if (id === 1) { const n = s.u32(); for (let i = 0; i < n; i++) { if (s.u8() !== 0x60) throw new Error(`${name}: non-function type`); const np = s.u32(); const params = []; for (let j = 0; j < np; j++) params.push(wtOf(s.u8())); const nr = s.u32(); const results = []; for (let j = 0; j < nr; j++) results.push(wtOf(s.u8())); obj.types.push({ params, results }); } }
    else if (id === 2) { const n = s.u32(); for (let i = 0; i < n; i++) { const module = s.str(), field = s.str(), kind = s.u8(); if (kind !== 0) throw new Error(`${name}: unsupported import kind`); obj.imports.push({ module, name: field, typeIdx: s.u32() }); } }
    else if (id === 3) { const n = s.u32(); for (let i = 0; i < n; i++) obj.funcTypes.push(s.u32()); }
    else if (id === 10) { const n = s.u32(); for (let i = 0; i < n; i++) { const len = s.u32(); obj.bodies.push({ start: s.p, end: s.p + len }); s.p += len; } }
    else if (id === 11) { const n = s.u32(); for (let i = 0; i < n; i++) { if (s.u8() !== 0) throw new Error(`${name}: passive data segment`); if (s.u8() !== 0x41) throw new Error(`${name}: data offset`); const offset = s.i32(); if (s.u8() !== 0x0B) throw new Error(`${name}: data offset end`); const len = s.u32(); obj.segments.push({ offset, bytes: s.bytes(len) }); } }
    else if (id === 0) { const sname = s.str(); if (sname === SECTION) obj.info = readInfo(s, name); else if(sname===SCALARS)obj.scalars=JSON.parse(s.str()); else if(sname===UNWIND_TYPES)obj.unwindTypes=JSON.parse(s.str()); }
    r.p = end;
  }
  if (!obj.info) throw new Error(`${name}: missing ${SECTION} section (not built by this compiler's cc -c)`);
  obj.bytes = bytes;
  const info = obj.info;
  info.funcSymbols.forEach((sym,i)=>{
    sym.noUnwindABI=obj.unwindTypes?.functions[i]||'';
    sym.declaredThrows=obj.unwindTypes?.throws?.[i]===true;
    sym.requiresNoUnwind=obj.unwindTypes?.required?.[i]===true;
  });
  info.dataSymbols.forEach((sym,i)=>sym.noUnwindABI=obj.unwindTypes?.data[i]||'');
  obj.blob = new Uint8Array(info.blobSize);
  for (const seg of obj.segments) obj.blob.set(seg.bytes, seg.offset);
  info.ranges.sort((a, b) => a.start - b.start);
  info.dataRelocs.sort((a, b) => a.offset - b.offset);
  obj.globalDefs = new Map();
  for (const sym of info.funcSymbols) { sym.kind = "func"; if ((sym.flags & 1) && !(sym.flags & 2)) obj.globalDefs.set(sym.name, sym); }
  for (const sym of info.dataSymbols) { sym.kind = "data"; if ((sym.flags & 1) && !(sym.flags & 2)) obj.globalDefs.set(sym.name, sym); }
  return obj;
}
function readInfo(s, name) {
  const version = s.u32(); if (version < 1 || version > VERSION) throw new Error(`${name}: object version ${version} is not ${VERSION}`);
  const info = { version, tlsRanges: [], base: s.u32(), blobSize: s.u32(), minStackBytes: s.u32(), sourceName: s.str(), funcSymbols: [], dataSymbols: [], tags: [], ranges: [], dataRelocs: [], funcs: [], files: [], required: [], conditional: [] };
  let n = s.u32(); for (let i = 0; i < n; i++) { const sym = { name: s.str(), flags: s.u32(), idx: s.u32(), module: s.str(), importName: s.str(), exports: [] }; const ne = s.u32(); for (let j = 0; j < ne; j++) sym.exports.push(s.str()); info.funcSymbols.push(sym); }
  n = s.u32(); for (let i = 0; i < n; i++) info.dataSymbols.push({ name: s.str(), flags: s.u32(), offset: s.u32(), size: s.u32() });
  n = s.u32(); for (let i = 0; i < n; i++) info.tags.push({ name: s.str(), idx: s.u32(), typeIdx: s.u32() });
  n = s.u32(); for (let i = 0; i < n; i++) info.ranges.push({ start: s.u32(), len: s.u32(), kind: s.u32(), idx: s.u32() });
  n = s.u32(); for (let i = 0; i < n; i++) info.dataRelocs.push({ offset: s.u32(), value: s.u32() });
  n = s.u32();
  for (let i = 0; i < n; i++) {
    const f = { relocs: [], calls: [], regions: [], srcLocs: [] };
    let m = s.u32(); for (let j = 0; j < m; j++) f.relocs.push({ offset: s.u32(), kind: ["data","func","tls"][s.u32()], value: s.u32() });
    m = s.u32(); for (let j = 0; j < m; j++) f.calls.push(s.u32());
    m = s.u32(); for (let j = 0; j < m; j++) { const g = { offset: s.u32(), regions: [] }; const k = s.u32(); for (let q = 0; q < k; q++) { const isSlot = s.u32(); const v = s.u32(); const offset = s.u32() | 0; const size = s.u32(); g.regions.push(isSlot ? { slot: v, size } : { local: v, offset, size }); } f.regions.push(g); }
    m = s.u32(); for (let j = 0; j < m; j++) f.srcLocs.push({ offset: s.u32(), fileIdx: s.u32(), line: s.u32() });
    if (version >= 2) { const facts = JSON.parse(s.str()); f.controlFlow = facts.controlFlow; f.fnMeta = facts.fnMeta; f.summary = facts.summary; }
    info.funcs.push(f);
  }
  n = s.u32(); for (let i = 0; i < n; i++) info.files.push(s.str());
  n = s.u32(); for (let i = 0; i < n; i++) info.required.push(s.str());
  n = s.u32(); for (let i = 0; i < n; i++) info.conditional.push({ symbol: s.str(), source: s.str() });
  if(version>=3){n=s.u32();for(let i=0;i<n;i++)info.tlsRanges.push({offset:s.u32(),size:s.u32(),align:s.u32()});}
  info.staticAlignment=version>=4?s.u32():16;
  info.constructors=[];
  if(version>=5){n=s.u32();for(let i=0;i<n;i++)info.constructors.push({idx:s.u32(),priority:s.u32()});}
  if(version>=8){const abis=JSON.parse(s.str());info.funcSymbols.forEach((sym,i)=>sym.memoryABI=abis[i]||'');if(version>=9){const data=JSON.parse(s.str());info.dataSymbols.forEach((sym,i)=>sym.memoryABI=data[i]||'');}}
  return info;
}

// ---- function body decoder ----
// Rebuilds the instruction tree of one function; ctx maps every index into
// the linked module: mapFunc(localIdx), mapType(localIdx), mapTag(localIdx),
// constAt(offset) -> resolved i32 value for a relocated constant,
// regionsAt(offset), srcLocAt(offset) -> WSrcLoc node.
let aopTable = null;
function aopFor(opcode) {
  if (!aopTable) {
    const w = W(); aopTable = new Map();
    for (const wt of [w.WT_I32, w.WT_I64, w.WT_F32, w.WT_F64]) for (let op = 0; op <= 44; op++) for (const sign of [true, false]) {
      try { const code = w.getaop(wt, op, sign); if (!aopTable.has(code)) aopTable.set(code, { wt, op, sign }); } catch (e) { /* invalid combination */ }
    }
  }
  return aopTable.get(opcode);
}
const ATOMIC_SIGS = { 0x00: [2, 1], 0x01: [3, 1, true], 0x10: [1, 1], 0x17: [2, 0], 0x1E: [2, 1], 0x25: [2, 1], 0x2C: [2, 1], 0x33: [2, 1], 0x3A: [2, 1], 0x41: [2, 1], 0x48: [3, 1] };
// [argument count, result count, final argument is i64, result is i64].
Object.assign(ATOMIC_SIGS, {
  0x11: [1, 1, false, true], 0x12: [1, 1], 0x13: [1, 1],
  0x14: [1, 1, false, true], 0x15: [1, 1, false, true], 0x16: [1, 1, false, true],
  0x18: [2, 0, true], 0x19: [2, 0], 0x1A: [2, 0],
  0x1B: [2, 0, true], 0x1C: [2, 0, true], 0x1D: [2, 0, true],
});
function decodeBody(bytes, start, end, ctx) {
  const w = W();
  const r = new Reader(bytes, start, end);
  const locals = [];
  const nl = r.u32(); for (let i = 0; i < nl; i++) { const count = r.u32(); locals.push({ count, type: wtOf(r.u8()) }); }
  const bodyStart = r.p;
  const nodes = [], stack = [], funcLabel = { isFuncLabel: true };
  const label = (d) => { if (d === stack.length) return funcLabel; if (d > stack.length) throw new Error("branch depth out of range"); return stack[stack.length - 1 - d]; };
  const blockType = () => {
    const b = bytes[r.p];
    if (b === 0x40) { r.p++; return w.WT_EMPTY; }
    if (b >= 0x7B && b <= 0x7F) { r.p++; return wtOf(b); }
    return { tag: "typeidx", idx: ctx.mapType(r.i32()) };
  };
  const raw = (byteList, params, results) => { const n = new w.WRaw(byteList); n.signature = { params, results }; return n; };
  while (r.p < end) {
    const at = r.p - bodyStart;
    const loc = ctx.srcLocAt(at); if (loc) nodes.push(loc);
    const op = r.u8();
    let n;
    switch (op) {
      case 0x00: n = new w.WUnreachable(); break;
      case 0x01: n = new w.WNop(); break;
      case 0x02: n = new w.WBlock(blockType()); stack.push(n); break;
      case 0x03: n = new w.WLoop(blockType()); stack.push(n); break;
      case 0x04: n = new w.WIf(blockType()); stack.push(n); break;
      case 0x05: n = new w.WElse(); break;
      case 0x0B: if (!stack.length) { if (r.p !== end) throw new Error("end outside a block"); n = null; break; } stack.pop(); n = new w.WEnd(); break;
      case 0x0C: n = new w.WBr(label(r.u32())); break;
      case 0x0D: n = new w.WBrIf(label(r.u32())); break;
      case 0x0E: { const count = r.u32(); const targets = []; for (let i = 0; i < count; i++) targets.push(label(r.u32())); n = new w.WBrTable(targets, label(r.u32())); break; }
      case 0x0F: n = new w.WReturn(); break;
      case 0x10: { const off = r.p - bodyStart; n = new w.WCall(ctx.mapFunc(r.u32())); if(ctx.noUnwindAt?.(off)) n.noUnwind=true; const regions = ctx.regionsAt(off); if (regions) n.continuationRegions = regions; break; }
      case 0x11: { const off = r.p - bodyStart; n = new w.WCallIndirect(ctx.mapType(r.u32()), r.u8()); if(ctx.noCaptureAt?.(off)) n.noCapture=true; if(ctx.noUnwindAt?.(off)) n.noUnwind=true; const regions = ctx.regionsAt(off); if (regions) n.continuationRegions = regions; break; }
      case 0x08: n = new w.WThrow(ctx.mapTag(r.u32())); break;
      case 0x0A: n = new w.WThrowRef(); break;
      case 0x1F: { const bt = blockType(); const count = r.u32(); const catches = []; for (let i = 0; i < count; i++) { const kind = r.u8(); const c = { kind }; if (kind === 0 || kind === 1) c.tagIdx = ctx.mapTag(r.u32()); c.target = label(r.u32()); catches.push(c); } n = new w.WTryTable(bt, catches); stack.push(n); break; }
      case 0x1A: n = new w.WDrop(); break;
      case 0x20: n = new w.WLocalGet(r.u32()); break;
      case 0x21: n = new w.WLocalSet(r.u32()); break;
      case 0x22: n = new w.WLocalTee(r.u32()); break;
      case 0x23: n = new w.WGlobalGet(r.u32()); break;
      case 0x24: n = new w.WGlobalSet(r.u32()); if (ctx.stackRestoreAt?.(at)) n.stackRestore = true; break;
      case 0x3F: n = new w.WMemorySize(r.u32()); break;
      case 0x40: n = new w.WMemoryGrow(r.u32()); break;
      case 0x41: { const off = r.p - bodyStart; const v = r.i32(); const resolved = ctx.constAt(off); n = new w.WConst(w.WT_I32, resolved === undefined ? v : resolved); if(resolved!==undefined)n.linkAddress=true; if(ctx.tlsAt?.(off))n.reloc="tls"; if(ctx.dynamicAt?.(off))n.dynamic=ctx.dynamicAt(off); break; }
      case 0x42: n = new w.WConst(w.WT_I64, r.i64()); break;
      case 0x43: n = new w.WConst(w.WT_F32, r.f32()); break;
      case 0x44: n = new w.WConst(w.WT_F64, r.f64()); break;
      case 0xFC: {
        const sub = r.u32();
        if (sub <= 7) n = new w.WTruncSat(sub);
        else if (sub === 10) { const destination = r.u32(), source = r.u32(); n = new w.WMemoryCopy(destination, source); }
        else if (sub === 11) { n = new w.WMemoryFill(r.u32()); }
        else throw new Error(`unsupported 0xFC opcode ${sub}`);
        break;
      }
      case 0xFD: {
        const sub=r.u32(),spec=self.CompilerJS.SIMD128.byOpcode.get(sub);
        if(!spec)throw Error(`unsupported SIMD opcode ${sub}`);
        let memory=0,offset=0,align=0;
        if(spec.memoryBytes){const flags=r.u32();align=flags&63;memory=flags&64?r.u32():0;offset=r.u32();if(2**align>spec.memoryBytes)throw Error('Invalid SIMD memory alignment');}
        const count=spec.immediate==='bytes'||spec.immediate==='shuffle'?16:spec.immediate==='lane'?1:0;
        const imms=Array.from(r.bytes(count));
        if(spec.immediate==='shuffle'&&imms.some(i=>i>=32)||spec.immediate==='lane'&&imms[0]>=spec.lanes)throw Error('Invalid SIMD lane immediate');
        n=new w.WSimd(sub,imms,memory,offset,align);break;
      }
      case 0xFE: {
        const sub = r.u8();
        if (sub === 0x03) { r.u8(); n = raw([0xFE, 0x03, 0x00], [], []); break; }
        const sig = ATOMIC_SIGS[sub]; if (!sig) throw new Error(`unsupported atomic opcode ${sub}`);
        const align = r.u32(), memory = align & 64 ? r.u32() : 0, offset = r.u32(); const b = [0xFE, sub]; lebU(b, align); if(align&64)lebU(b,memory); lebU(b, offset);
        const params = []; for (let i = 0; i < sig[0]; i++) params.push(sig[2] && i === sig[0] - 1 ? w.WT_I64 : w.WT_I32);
        n = raw(b, params, sig[1] ? [sig[3] ? w.WT_I64 : w.WT_I32] : []); n.memory=memory; break;
      }
      default: {
        if (op >= 0x28 && op <= 0x3E) { const ordinary=ctx.ordinaryMemoryOffsets?.has(r.p-bodyStart-1), flags = r.u32(), memory = flags & 64 ? r.u32() : 0, offset = r.u32(); n = new w.WMop(op, offset, flags & 63, memory); if(ordinary)n.ordinaryMemory=true; break; }
        if (op >= 0xC0 && op <= 0xC4) { const t = op <= 0xC1 ? w.WT_I32 : w.WT_I64; n = raw([op], [t], [t]); break; }
        const a = aopFor(op);
        if (!a) throw new Error(`unsupported opcode 0x${op.toString(16)} in object code`);
        n = new w.WAop(a.wt, a.op, a.sign); break;
      }
    }
    if (n) nodes.push(n);
  }
  if (stack.length) throw new Error("unclosed block in object code");
  return { locals, nodes };
}

// ---- archives (ar format, GNU long names) ----
function readArchive(bytes, name) {
  const dec = new TextDecoder();
  if (bytes.length < 8 || dec.decode(bytes.subarray(0, 8)) !== "!<arch>\n") throw new Error(`${name}: not an archive`);
  const members = []; let p = 8, longNames = null;
  while (p + 60 <= bytes.length) {
    const header = dec.decode(bytes.subarray(p, p + 60));
    let mname = header.slice(0, 16).trim();
    const size = parseInt(header.slice(48, 58).trim(), 10);
    const body = bytes.subarray(p + 60, p + 60 + size);
    p += 60 + size + (size & 1);
    if (mname === "//") { longNames = dec.decode(body); continue; }
    if (mname === "/" || mname === "/SYM64/") continue;
    if (mname.startsWith("/") && longNames) { const off = parseInt(mname.slice(1), 10); mname = longNames.slice(off, longNames.indexOf("/\n", off)); }
    else if (mname.endsWith("/")) mname = mname.slice(0, -1);
    members.push({ name: mname, bytes: body });
  }
  return members;
}
function writeArchive(members) {
  const enc = new TextEncoder();
  const parts = [enc.encode("!<arch>\n")];
  let longNames = "";
  const named = members.map((m) => {
    if (m.name.length <= 15) return { field: m.name + "/", bytes: m.bytes };
    const field = "/" + longNames.length; longNames += m.name + "/\n"; return { field, bytes: m.bytes };
  });
  const header = (field, size) => enc.encode(field.padEnd(16) + "0".padEnd(12) + "0".padEnd(6) + "0".padEnd(6) + "644".padEnd(8) + String(size).padEnd(10) + "`\n");
  if (longNames) { const b = enc.encode(longNames); parts.push(header("//", b.length), b); if (b.length & 1) parts.push(enc.encode("\n")); }
  for (const m of named) { parts.push(header(m.field, m.bytes.length), m.bytes); if (m.bytes.length & 1) parts.push(enc.encode("\n")); }
  let total = 0; for (const p of parts) total += p.length;
  const out = new Uint8Array(total); let o = 0; for (const p of parts) { out.set(p, o); o += p.length; }
  return out;
}

// ---- linker ----
// inputs: [{name, bytes}] objects in command-line order; archives: [{name, bytes}].
// options: {writeErr, emitNames, requiredMembers, compilerOptions}
function link(inputs, archives, options) {
  const started = performance.now();
  const w = W(), C = self.CompilerJS;
  const writeErr = options.writeErr || (() => {});
  const fail = (msg) => { const e = new Error(msg); e.linkFailed = true; throw e; };
  const shared=!!options.compilerOptions?.shared;
  const runtime=!!options.compilerOptions?.runtime,sharedLibc=!!options.compilerOptions?.sharedLibc;
  const libraries=options.compilerOptions?.sharedLibraries||[];
  const exports=new Map();
  for(const lib of libraries)for(const sym of lib.symbols)if(!exports.has(sym.name))exports.set(sym.name,sym);
  const objects = inputs.map((i) => readObject(i.bytes, i.name));
  const dynamic=shared||libraries.length>0||!!options.compilerOptions?.dynamic||objects.some(o=>o.info.required.includes("__dl.c"));
  if(shared&&archives.some(a=>/(?:^|\/)libc\.a$/.test(a.name)))fail("Shared libraries bind the executable libc; omit libc.a");
  const members = [];
  for (const a of archives) for (const m of readArchive(a.bytes, a.name)) members.push({ archive: a.name, name: m.name, bytes: m.bytes, obj: null });
  const parseMember = (m) => { if (!m.obj) { m.obj = readObject(m.bytes, `${m.archive}(${m.name})`); m.obj.member = m.name; } return m.obj; };
  // Index in archive order as lookups advance. Keep parsing lazy: a malformed
  // member after the last needed definition must remain unexamined. Each
  // member's symbols are visited once, rather than once per undefined name.
  const memberNames = new Map(), archiveDefinitions = new Map();
  for (const m of members) {
    if (!memberNames.has(m.name)) memberNames.set(m.name,m);
    if (m.name.endsWith('.o')&&!memberNames.has(m.name.slice(0,-2)))memberNames.set(m.name.slice(0,-2),m);
  }
  let indexedMembers=0;
  const findDefinition = name => {
    while (!archiveDefinitions.has(name)&&indexedMembers<members.length) {
      const m=members[indexedMembers++],obj=parseMember(m);
      for (const symbol of obj.globalDefs.keys())if(!archiveDefinitions.has(symbol))archiveDefinitions.set(symbol,m);
      for (const sym of obj.info.funcSymbols)if((sym.flags&8)&&!hostProviders.has(sym.name))hostProviders.set(sym.name,{obj,sym});
    }
    return archiveDefinitions.get(name);
  };
  // Symbol resolution: explicit objects first, then archive members pulled by
  // required-source name or by an undefined symbol they define.
  const defs = new Map(), pulled = [], pulledSet = new Set(), hostProviders = new Map();
  const forcedMembers = new Set(options.requiredMembers || []);
  const undefinedNames = new Set(shared?[]:["main"]), required = new Set(forcedMembers);
  const pull = (obj) => {
    if (pulledSet.has(obj)) return; pulledSet.add(obj); pulled.push(obj);
    for (const [name, sym] of obj.globalDefs) {
      const prev = defs.get(name);
      if (prev && (sym.flags & 16)) continue;
      if (prev && !(prev.sym.flags & 16)) fail(`duplicate symbol '${name}' in ${obj.name} (already defined in ${prev.obj.name})`);
      defs.set(name, { obj, sym });
    }
    for (const sym of obj.info.funcSymbols) if (sym.flags & 4) undefinedNames.add(sym.name);
    for (const sym of obj.info.dataSymbols) if (!(sym.flags & 1)) undefinedNames.add(sym.name);
    for (const r of obj.info.required) required.add(r);
  };
  for (const obj of objects) pull(obj);
  // A dynamic executable owns one libc; plugins bind to these definitions.
  if(dynamic&&!shared&&!sharedLibc)for(const m of members)if(/(?:^|\/)libc\.a$/.test(m.archive)&&!/^__(?:SDL|sdl3|webgpu|emscripten)/.test(m.name))pull(parseMember(m));
  for (;;) {
    let progress = false;
    for (const name of required) {
      if(sharedLibc&&name!=='__alloca.c'&&!/^__(?:SDL|sdl3|webgpu|emscripten)/.test(name))continue;
      const m = memberNames.get(name);
      if (m && !pulledSet.has(m.obj || parseMember(m))) {
        const obj=parseMember(m);
        // A source-required optional runtime may now be supplied by a DSO
        // (notably SDL). Do not create a second copy of its process state.
        if (dynamic && !forcedMembers.has(name) && obj.globalDefs.size && [...obj.globalDefs.keys()].every(n=>exports.has(n)||defs.has(n))) continue;
        pull(obj); progress = true;
      }
    }
    for (const name of undefinedNames) {
      if (defs.has(name)||exports.has(name)) continue;
      const m=findDefinition(name);
      if(m){const obj=parseMember(m);if(!pulledSet.has(obj)){pull(obj);progress=true;}}
    }
    if (!progress) break;
  }
  if(!dynamic&&pulled.some(o=>o.info.required.includes("__dl.c")))return link(inputs,archives,{...options,compilerOptions:{...options.compilerOptions,dynamic:true}});
  // Resolve every function slot of every pulled object.
  const unwindTypesByName = new Map();
  for (const obj of pulled) for (const sym of [...obj.info.funcSymbols,...obj.info.dataSymbols]) if (!(sym.flags & 2)) {
    const abi=sym.noUnwindABI||'', prior=unwindTypesByName.get(sym.name);
    if (prior !== undefined && prior !== abi) fail('__nothrow type mismatch: '+sym.name);
    unwindTypesByName.set(sym.name,abi);
  }
  for (const obj of pulled) {
    obj.funcTarget = [];
    for (const sym of obj.info.funcSymbols) {
      if (sym.flags & 1) { obj.funcTarget[sym.idx] = { kind: "def", obj, idx: sym.idx, sym }; continue; }
      if (sym.flags & 4) {
        const d = defs.get(sym.name);
        if (d && d.sym.kind === "func") { if((sym.noUnwindABI||'')!==(d.sym.noUnwindABI||''))fail('__nothrow type mismatch: '+sym.name); if((sym.memoryABI||'')!==(d.sym.memoryABI||''))fail('Wasm pointer memory ABI mismatch: '+sym.name); obj.funcTarget[sym.idx] = { kind: "def", obj: d.obj, idx: d.sym.idx, sym: d.sym }; continue; }
        if (d) fail(`'${sym.name}' is used as a function in ${obj.name} but defined as an object in ${d.obj.name}`);
        // A host import declared by any pulled object satisfies the reference.
        let host = null, hostObject = null;
        for (const o of pulled) { host = o.info.funcSymbols.find((s) => (s.flags & 8) && s.name === sym.name); if (host) { hostObject = o; break; } }
        if (!host && hostProviders.has(sym.name)) { const provider=hostProviders.get(sym.name); host=provider.sym; hostObject=provider.obj; }
        if (host) { if((sym.noUnwindABI||'')!==(host.noUnwindABI||''))fail('__nothrow type mismatch: '+sym.name); obj.funcTarget[sym.idx] = { kind: "import", module: host.module, name: host.importName, type: hostObject.types[hostObject.imports[host.idx].typeIdx] }; continue; }
        // Explicit opt-in: unresolved functions become host-supplied env imports,
        // matching whole-program --allow-undefined. Data still needs storage.
        if(exports.has(sym.name)&&exports.get(sym.name).kind!=='function')fail(`Dynamic symbol kind mismatch: ${sym.name}`);
        if (shared || exports.has(sym.name) || options.compilerOptions?.allowUndefined) {
          obj.funcTarget[sym.idx] = { kind: "import", module: shared || exports.has(sym.name) ? "dl.func" : "env", name: sym.name, type: obj.types[obj.imports[sym.idx].typeIdx] };
          continue;
        }
        fail(`undefined symbol '${sym.name}' referenced from ${obj.name}`);
      }
      obj.funcTarget[sym.idx] = { kind: "import", module: sym.module, name: sym.importName, type: obj.types[obj.imports[sym.idx].typeIdx] };
    }
    obj.dataTarget = new Map();
    for (const sym of obj.info.dataSymbols) {
      if (sym.flags & 1) continue;
      const d = defs.get(sym.name);
      const provider=d?.sym||exports.get(sym.name);if(provider&&(sym.noUnwindABI||'')!==(provider.noUnwindABI||''))fail('__nothrow type mismatch: '+sym.name);if(provider&&(sym.memoryABI||'')!==(provider.memoryABI||''))fail('Wasm pointer memory ABI mismatch: '+sym.name);
      if(!d&&exports.has(sym.name)&&exports.get(sym.name).kind!==(sym.flags&32?'tls':'data'))fail(`Dynamic symbol kind mismatch: ${sym.name}`);
      if (!d && (shared||exports.has(sym.name))) { obj.dataTarget.set(sym,{external:sym.name,tls:!!(sym.flags&32)}); continue; }
      if (!d) fail(`undefined symbol '${sym.name}' referenced from ${obj.name}`);
      if (d.sym.kind !== "data") fail(`'${sym.name}' is used as an object in ${obj.name} but defined as a function in ${d.obj.name}`);
      if (!!(sym.flags & 32) !== !!(d.sym.flags & 32)) fail(`TLS declaration mismatch for '${sym.name}'`);
      obj.dataTarget.set(sym, d);
    }
  }
  // Classify an address value found in code or data of `obj`.
  const classify = (obj, value, kind) => {
    if (value === 0) return { kind: "null" };
    if (kind === "func" || value < obj.info.base) return { kind: "func", obj, idx: value - 1 };
    if (value >= UNDEFINED_BASE) {
      let best = null;
      for (const sym of obj.info.dataSymbols) if (!(sym.flags & 1) && sym.offset <= value && (!best || sym.offset > best.offset)) best = sym;
      if (!best) fail(`${obj.name}: unresolved placeholder address`);
      const d = obj.dataTarget.get(best);
      if(d.external)return {kind:"external",name:d.external,tls:d.tls,offset:value-best.offset};
      return { kind: "data", obj: d.obj, offset: d.sym.offset + (value - best.offset) };
    }
    return { kind: "data", obj, offset: value - obj.info.base };
  };
  const rangeAt = (obj, offset) => {
    const rs = obj.info.ranges; let lo = 0, hi = rs.length - 1, found = -1;
    while (lo <= hi) { const mid = (lo + hi) >> 1; if (rs[mid].start <= offset) { found = mid; lo = mid + 1; } else hi = mid - 1; }
    if (found >= 0 && offset < rs[found].start + rs[found].len) return found;
    // Addresses one past an object or inside padding keep the nearest range alive.
    return found;
  };
  // Reachability over functions and static-data ranges.
  const conditionalRoots = new Map(), deferredRoots = new Set();
  if(!dynamic)for(const obj of pulled)for(const sym of obj.info.funcSymbols){
    const trigger=obj.info.sourceName==='__pthread.c'&&sym.name==='__thread_main' ? '__thread_spawn' :
      obj.info.sourceName==='__clone.c'&&sym.name==='__clone_entry' ? '__clone_impl' : null;
    if(trigger&&(sym.flags&1)&&sym.exports.includes(sym.name)){
      conditionalRoots.set(trigger,{obj,idx:sym.idx});deferredRoots.add(obj.name+'#'+sym.idx);
    }
  }
  const liveFunc = new Map(), liveRange = new Map();
  const queue = [];
  const markFunc = (obj, idx) => { const key = obj.name + "#" + idx; if (liveFunc.has(key)) return; liveFunc.set(key, { obj, idx }); queue.push({ kind: "func", obj, idx }); };
  const markRange = (obj, ri) => { if (ri < 0) return; const key = obj.name + "@" + ri; if (liveRange.has(key)) return; liveRange.set(key, { obj, ri }); queue.push({ kind: "range", obj, ri }); };
  const markTarget = (t) => {
    if (t.kind === "func") { const ft = t.obj.funcTarget[t.idx]; if (ft && ft.kind === "def") markFunc(ft.obj, ft.idx); else if (ft) markImportUse(ft); }
    else if(t.kind!=="external" && t.kind!=="null") markRange(t.obj, rangeAt(t.obj, t.offset));
  };
  const importUses = new Map();
  const cloneNames=new Set(['fork','vfork','clone','__fork_impl','__vfork_impl','__clone_impl']);
  const publish = name => options.compilerOptions?.enableFork || !cloneNames.has(name);
  const markImportUse = (ft) => {
    const key = ft.module + "\0" + ft.name;
    if (!importUses.has(key)) importUses.set(key, ft);
    const entry=ft.module==='c'&&conditionalRoots.get(ft.name);
    if(entry)markFunc(entry.obj,entry.idx);
  };
  if(dynamic&&(!shared||runtime))for(const obj of pulled)for(const sym of obj.info.funcSymbols)if((sym.flags&8)&&publish(sym.name))markImportUse(obj.funcTarget[sym.idx]);
  const mainDef = defs.get("main");
  if (!shared && (!mainDef || mainDef.sym.kind !== "func")) fail("undefined symbol 'main'");
  if(mainDef)markFunc(mainDef.obj, mainDef.sym.idx);
  // Deferred conversions must be proved even if AST folding removed their
  // original use. Keep their bodies until the pre-inlining effect check.
  for (const obj of pulled) for (const sym of obj.info.funcSymbols) if (sym.requiresNoUnwind) {
    const ft=obj.funcTarget[sym.idx];
    if(ft.kind==='def')markFunc(ft.obj,ft.idx);else markImportUse(ft);
  }
  if(dynamic)for(const {obj,sym} of defs.values())if(sym.kind==="func"){if(publish(sym.name))markFunc(obj,sym.idx);}else markRange(obj,rangeAt(obj,sym.offset));
  const allocaDef = defs.get("alloca"); if (allocaDef && allocaDef.sym.kind === "func") markFunc(allocaDef.obj, allocaDef.sym.idx);
  for (const obj of pulled) {
    for (const c of obj.info.constructors) markFunc(obj,c.idx);
    for (const sym of obj.info.funcSymbols) if ((sym.flags & 1) && sym.exports.length&&!deferredRoots.has(obj.name+'#'+sym.idx)) markFunc(obj, sym.idx);
    obj.info.ranges.forEach((r, ri) => { if (r.kind === 0) markRange(obj, ri); });
  }
  const dataRelocsIn = (obj, start, end) => {
    const rs = obj.info.dataRelocs; const out = []; let lo = 0, hi = rs.length;
    while (lo < hi) { const mid = (lo + hi) >> 1; if (rs[mid].offset < start) lo = mid + 1; else hi = mid; }
    for (let i = lo; i < rs.length && rs[i].offset < end; i++) out.push(rs[i]);
    return out;
  };
  while (queue.length) {
    const item = queue.pop();
    if (item.kind === "func") {
      const { obj, idx } = item; const di = idx - obj.imports.length; const f = obj.info.funcs[di];
      for (const c of f.calls) { const ft = obj.funcTarget[c]; if (ft.kind === "def") markFunc(ft.obj, ft.idx); else markImportUse(ft); }
      for (const r of f.relocs) markTarget(classify(obj, r.value, r.kind));
      obj.info.ranges.forEach((r, ri) => { if (r.kind === 1 && r.idx === idx) markRange(obj, ri); });
    } else {
      const { obj, ri } = item; const r = obj.info.ranges[ri];
      for (const rel of dataRelocsIn(obj, r.start, r.start + r.len)) markTarget(classify(obj, rel.value, "auto"));
    }
  }
  // Layout: one aligned block per pulled object, dead ranges left zero.
  let minStack = 0; for (const obj of pulled) minStack = Math.max(minStack, obj.info.minStackBytes);
  const stackPages = Math.max(256, Math.ceil(minStack / 65536));
  const base = stackPages * 65536;
  let cursor = 0;
  for (const obj of pulled) { cursor = Math.ceil((base+cursor-obj.info.base)/obj.info.staticAlignment)*obj.info.staticAlignment+obj.info.base-base; obj.linkBase = base + cursor; cursor += obj.info.blobSize; }
  let image = new Uint8Array(cursor);
  for (const { obj, ri } of liveRange.values()) { const r = obj.info.ranges[ri]; image.set(obj.blob.subarray(r.start, r.start + r.len), obj.linkBase - base + r.start); }
  // Module skeleton in generateCode's order: runtime globals, __cc_yield, host imports.
  const wmod = new C.WasmModule();
  wmod.inlineOptions = { enabled: false };
  const cg = {
    stackPages, relocatable: false, literalRanges: [], sourceMapFiles: [], sourceMapEntries: [],
    staticAlignment:Math.max(16,...pulled.map(o=>o.info.staticAlignment)),staticData: image, staticDataOffset: image.length,
    allocateStatic(size, align) {
      if (!align) align = 4;
      const aligned = Math.ceil((base + this.staticDataOffset) / align) * align - base;
      const grown = new Uint8Array(aligned + size); grown.set(this.staticData); this.staticData = grown;
      this.staticDataOffset = aligned + size;
      return base + aligned;
    },
    funcDefToWasmFuncIdx: new Map(), exceptionToWasmTagIdx: new Map(),
  };
  if(dynamic)cg.dynamic={shared, memoryABIs:{}, noUnwindABIs:{}, symbols:[], relocations:[], externalFunctions:new Map(), nativeImports:new Set(),engineImports:new Set()};
  cg.stackPointerGlobalIdx = wmod.addGlobalI32(stackPages * 65536, true);
  cg.heapBaseGlobalIdx = wmod.addGlobalI32(0, false);
  cg.threadStateGlobalIdx = wmod.addGlobalI32(1024, true);
  C.TLS.addGlobals(wmod,cg);
  cg.tlsRanges=[];
  for(const obj of pulled)for(const r of obj.info.tlsRanges)if(liveRange.has(obj.name+"@"+rangeAt(obj,r.offset)))cg.tlsRanges.push({addr:obj.linkBase+r.offset,size:r.size,align:r.align});
  if (options.emitNames !== false) wmod.globalNames.push({ idx: 0, name: "__stack_pointer" }, { idx: 1, name: "__heap_base" }, { idx: 2, name: "__thread_state" });
  const typeId = (t) => wmod.addFunctionTypeId(t.params, t.results);
  wmod.addFunctionImport("c", "__cc_yield", wmod.addFunctionTypeId([], []));
  if(dynamic)cg.dynamic.syncFunction=wmod.addFunctionImport("c","__dl_sync",wmod.addFunctionTypeId([],[]));
  const importIdx = new Map([["c\0__cc_yield", 0]]);
  for (const [key, ft] of importUses) {
    const idx = wmod.addFunctionImport(ft.module, ft.name, typeId(ft.type));
    if(JSON.parse(unwindTypesByName.get(ft.name)||'[]').includes(''))wmod.funcImports[idx].noUnwind=true;
    importIdx.set(key, idx);
    if(ft.module==="dl.func")cg.dynamic.externalFunctions.set(idx,ft.name);
    if (options.emitNames !== false) wmod.funcNames.push({ idx, name: ft.name });
  }
  // Every live function gets its final index before any body is decoded.
  const live = [...liveFunc.values()];
  const finalIdx = new Map();
  for (const { obj, idx } of live) {
    const t = obj.types[obj.funcTypes[idx - obj.imports.length]];
    const fi = wmod.addFunctionDefinition(typeId(t));
    finalIdx.set(obj.name + "#" + idx, fi);
    const sym = obj.info.funcSymbols[idx];
    if (options.emitNames !== false) wmod.funcNames.push({ idx: fi, name: sym.name });
    for (const e of sym.exports) { if (wmod.exports.some((x) => x.name === e)) fail(`export '${e}' defined twice`); wmod.addExport(e, 0, fi); }
    if (sym.name === "main" && obj === mainDef?.obj) wmod.addExport("main", 0, fi);
    if (sym.name === "alloca" && allocaDef && obj === allocaDef.obj) wmod.addExport("alloca", 0, fi);
    if (!(sym.flags & 2)) cg.funcDefToWasmFuncIdx.set({ name: sym.name }, fi);
  }
  const tagIdx = new Map();
  const mapTagOf = (obj) => (local) => {
    const t = obj.info.tags.find((x) => x.idx === local); if (!t) fail(`${obj.name}: unknown exception tag ${local}`);
    if (!tagIdx.has(t.name)) { const idx = wmod.addTag(typeId(obj.types[t.typeIdx])); tagIdx.set(t.name, idx); cg.exceptionToWasmTagIdx.set({ name: t.name }, idx); }
    return tagIdx.get(t.name);
  };
  const resolveFunc = (obj, local) => {
    const ft = obj.funcTarget[local];
    if (ft.kind === "def") { const fi = finalIdx.get(ft.obj.name + "#" + ft.idx); if (fi === undefined) fail(`internal: dead function referenced (${ft.sym.name})`); return fi; }
    return importIdx.get(ft.module + "\0" + ft.name);
  };
  // Traditional link-only probes intentionally declare e.g. char fcntl()
  // without its real prototype. Resolve the symbol, but keep Wasm well typed:
  // executing an incompatible call traps instead of corrupting the C stack.
  // Address relocations still use the canonical function identity.
  const incompatibleCalls = new Map();
  const resolveCall = (obj, local) => {
    const source = obj.info.funcSymbols[local], resolved = obj.funcTarget[local];
    const provider = resolved.kind==='def' ? resolved.sym : exports.get(source.name);
    if(provider && (source.noUnwindABI||'') !== (provider.noUnwindABI||''))fail('__nothrow type mismatch: '+source.name);
    if(provider && (source.memoryABI||'') !== (provider.memoryABI||''))fail('Wasm pointer memory ABI mismatch: '+source.name);
    const target = resolveFunc(obj, local);
    const sourceType = typeId(obj.types[local < obj.imports.length ? obj.imports[local].typeIdx : obj.funcTypes[local - obj.imports.length]]);
    const targetType = target < wmod.funcImports.length ? wmod.funcImports[target].typeId : wmod.funcDefs[target - wmod.funcImports.length].typeId;
    if (sourceType === targetType) return target;
    const key = target + ":" + sourceType;
    if (!incompatibleCalls.has(key)) {
      const idx = wmod.addFunctionDefinition(sourceType);
      wmod.funcDefs[idx - wmod.funcImports.length].wast = [new w.WUnreachable()];
      if (options.emitNames !== false) wmod.funcNames.push({idx, name:"__incompatible_call_" + obj.info.funcSymbols[local].name});
      incompatibleCalls.set(key, idx);
    }
    return incompatibleCalls.get(key);
  };
  if(dynamic){
    // Every module recognizes the same continuation longjmp protocol.
    const idx=wmod.addTag(wmod.addFunctionTypeId([w.WT_I32,w.WT_I32],[]));
    tagIdx.set('__LongJump',idx);cg.exceptionToWasmTagIdx.set({name:'__LongJump'},idx);
    for(const [idx,name] of cg.dynamic.externalFunctions)if(name==='setjmp')cg.funcDefToWasmFuncIdx.set({name},idx);
  }
  const addressRelocation = (obj,value,kind) => {
    const t=classify(obj,value,kind);
    if(t.kind==="null")return null;
    if(t.kind==='external')return {kind:t.tls?'externalTLS':'external',name:t.name,offset:t.offset};
    if(t.kind==='func')return {kind:'function',value:resolveFunc(obj,t.idx)+1};
    return {kind:kind==='tls'?'tls':'data',value:t.obj.linkBase+t.offset};
  };
  const resolveAddr = (obj, value, kind) => {
    const t = classify(obj, value, kind);
    if (t.kind === "null") return 0;
    if (t.kind === "func") { const fi = resolveFunc(obj, t.idx); wmod.addrTakenFuncs.add(fi); return fi + 1; }
    if(t.kind==="external")return 0;
    return t.obj.linkBase + t.offset;
  };
  const fileIdx = new Map();
  const mapFile = (name) => { if (!fileIdx.has(name)) { fileIdx.set(name, cg.sourceMapFiles.length); cg.sourceMapFiles.push(name); } return fileIdx.get(name); };
  for (const { obj, idx } of live) {
    const di = idx - obj.imports.length, f = obj.info.funcs[di], body = obj.bodies[di];
    const consts = new Map(), tlsOffsets = new Set(), dynamicOffsets=new Map();
    for (const r of f.relocs) { consts.set(r.offset, resolveAddr(obj, r.value, r.kind)); const dr=dynamic?addressRelocation(obj,r.value,r.kind):null; if(dr)dynamicOffsets.set(r.offset,dr); if(r.kind === "tls"&&dr?.kind!=="externalTLS") tlsOffsets.add(r.offset); }
    const regions = new Map(); for (const g of f.regions) regions.set(g.offset, g.regions);
    const srcLocs = new Map(); for (const l of f.srcLocs) srcLocs.set(l.offset, new w.WSrcLoc(mapFile(obj.info.files[l.fileIdx]), l.line));
    const mapTag = mapTagOf(obj);
    const decoded = decodeBody(obj.bytes, body.start, body.end, {
      mapFunc: (local) => resolveCall(obj, local),
      mapType: (local) => typeId(obj.types[local]),
      mapTag,
      constAt: (off) => consts.get(off),
      tlsAt: (off) => tlsOffsets.has(off),
      dynamicAt: off=>dynamicOffsets.get(off),
      regionsAt: (off) => regions.get(off),
      noCaptureAt: off => (f.summary?.nonCapturingCalls || []).includes(off),
      ordinaryMemoryOffsets: new Set(f.summary?.ordinaryMemory || []),
      noUnwindAt: off => (f.summary?.noUnwindCalls || []).includes(off),
      stackRestoreAt: off => (f.summary?.stackRestores || []).includes(off),
      srcLocAt: (off) => srcLocs.get(off),
    });
    const def = wmod.funcDefs[finalIdx.get(obj.name + "#" + idx) - wmod.funcImports.length];
    def.locals = decoded.locals; def.wast = decoded.nodes;
    def.controlFlow = f.controlFlow || null; // v1 objects remain continuation-only
    // Symbol resolution and instrumentation invalidate instruction summaries.
    def.fnMeta = f.fnMeta || null;
    def.functionSummary = null;
    w.validate(def.wast, null);
  }
  // Data words holding function pointers or addresses.
  for (const { obj, ri } of liveRange.values()) {
    const r = obj.info.ranges[ri];
    for (const rel of dataRelocsIn(obj, r.start, r.start + r.len)) {
      const v = resolveAddr(obj, rel.value, "auto"), at = obj.linkBase - base + rel.offset;
      if(dynamic){const relocation=addressRelocation(obj,rel.value,"auto");if(relocation)cg.dynamic.relocations.push({at,...relocation});}
      cg.staticData[at] = v & 255; cg.staticData[at + 1] = (v >>> 8) & 255; cg.staticData[at + 2] = (v >>> 16) & 255; cg.staticData[at + 3] = (v >>> 24) & 255;
    }
  }
  let trampoline;
  const linkOptions = { writeErr, fatalExit: (code) => fail(`link failed (${code})`), compilerOptions: { emitNames: options.emitNames !== false, ...(options.compilerOptions || {}) }, onTrampoline: stats => { trampoline = stats; options.onTrampoline?.(stats); }, onControlFlowIR: options.onControlFlowIR, onSSA: options.onSSA };
  // Declaration policy and deferred obligations follow the resolved symbol,
  // not the caller's local function index or an optional debug name.
  for (const obj of pulled) for (const sym of obj.info.funcSymbols) if (sym.declaredThrows || sym.requiresNoUnwind) {
    const id=resolveFunc(obj,sym.idx);if(id===undefined)continue;
    const meta=id<wmod.funcImports.length?wmod.funcImports[id]:(wmod.funcDefs[id-wmod.funcImports.length].fnMeta ||= {});
    if(sym.declaredThrows){meta.declaredThrows=true;meta.noinline=true;}
    if(sym.requiresNoUnwind)meta.requiresNoUnwind=true;
  }
  if(cg.tlsRanges.length&&live.some(({obj,idx})=>obj.info.version<3&&obj.info.funcSymbols[idx].name==='pthread_create'))fail('TLS requires rebuilding the legacy pthread runtime');
  if (pulled.some(o=>o.info.version<6) && live.some(({obj,idx})=>/^(pthread_(cond|sigmask|create)|sigprocmask$)/.test(obj.info.funcSymbols[idx].name)))
    fail('pthread condition/signal ABI changed; rebuild legacy objects and archives');
  if (pulled.some(o=>o.info.version<7) && live.some(({obj,idx})=>/^(fork|pthread_create|pthread_atfork)$/.test(obj.info.funcSymbols[idx].name)))
    fail('fork/pthread runtime ABI changed; rebuild legacy objects and archives');
  promoteLinkedScalars(wmod,cg,pulled,liveRange,rangeAt,options);
  for(const def of wmod.funcDefs)for(const n of def.wast||[])delete n.linkAddress;
  C.TLS.prepare(w,wmod,cg);
  if(dynamic)for(const [name,{obj,sym}] of defs){
    if(sym.kind==='func'){const fi=finalIdx.get(obj.name+'#'+sym.idx);if(fi===undefined)continue;cg.dynamic.symbols.push({name,kind:'function',memoryABI:sym.memoryABI||'',...(sym.noUnwindABI?{noUnwindABI:sym.noUnwindABI}:{}),value:fi+1,type:wmod.funcDefs[fi-wmod.funcImports.length].typeId});}
    else {const addr=obj.linkBase+sym.offset,r=cg.tlsRanges.find(r=>addr>=r.addr&&addr<r.addr+r.size);cg.dynamic.symbols.push({name,memoryABI:sym.memoryABI||'',...(sym.noUnwindABI?{noUnwindABI:sym.noUnwindABI}:{}),kind:sym.flags&32?'tls':'data',value:r&&sym.flags&32?r.offset+addr-r.addr:addr-base});}
  }
  if(dynamic&&(!shared||runtime)){
    const names=new Set(cg.dynamic.symbols.map(s=>s.name));
    for(const obj of pulled)for(const sym of obj.info.funcSymbols)if((sym.flags&8)&&!names.has(sym.name)){
      const fi=resolveFunc(obj,sym.idx);if(fi===undefined)continue;
      cg.dynamic.symbols.push({name:sym.name,kind:'function',...(sym.noUnwindABI?{noUnwindABI:sym.noUnwindABI}:{}),value:fi+1,type:wmod.funcImports[fi].typeId});names.add(sym.name);
    }
  }
  if(dynamic)for(const obj of pulled)for(const sym of [...obj.info.funcSymbols,...obj.info.dataSymbols])if(!(sym.flags&1)){if(sym.noUnwindABI)cg.dynamic.noUnwindABIs[sym.name]=sym.noUnwindABI;const old=cg.dynamic.memoryABIs[sym.name];if(old!==undefined&&old!==(sym.memoryABI||''))fail('Wasm pointer memory ABI mismatch: '+sym.name);cg.dynamic.memoryABIs[sym.name]=sym.memoryABI||'';}
  C.emitConstructors(wmod,pulled.flatMap(obj=>obj.info.constructors.map(c=>({idx:resolveFunc(obj,c.idx),priority:c.priority}))));
  // A linked DSO can supply a checked native or JSPI execution contract.
  // Keep the external symbol identity/binding, but call its typed table entry
  // directly rather than creating a continuation frame. In a non-fork image,
  // an absent function contract means JSPI; module admission enforces this.
  if(dynamic && options.compilerOptions?.nativeFunctions!==false && options.compilerOptions?.nativeSharedFunctions!==false){
    const contracts=new Map((options.compilerOptions.runtimeContracts||[]).map(s=>[s.name,s]));
    for(const [name,symbol] of exports)contracts.set(name,symbol);
    for(const def of wmod.funcDefs){
      const nodes=[];
      for(const n of def.wast || []){
        const name=n instanceof w.WCall?cg.dynamic.externalFunctions.get(n.funcIdx):null;
        const contract=contracts.get(name)?.execution || (name!=='setjmp' && !options.compilerOptions?.enableFork ? 'jspi-v1' : undefined);
        if(name && (contract==='native-v1'||(options.compilerOptions?.jspi!==false&&contract==='jspi-v1'))){
          const target=new w.WConst(w.WT_I32,n.funcIdx+1);target.dynamic={kind:'function',value:n.funcIdx+1};
          const call=new w.WCallIndirect(wmod.funcImports[n.funcIdx].typeId,0);
          if(n.noUnwind)call.noUnwind=true;
          call.nativeCall=name;if(n.continuationRegions)call.continuationRegions=n.continuationRegions.map(r=>({...r}));
          nodes.push(target,call);
          if(contract==='native-v1')cg.dynamic.nativeImports.add(name);
          else cg.dynamic.engineImports.add(name);
        }else nodes.push(n);
      }
      def.wast=nodes;
    }
  }
  cg.requiredMemory=0;
  for(const obj of pulled)for(const sym of [...obj.info.funcSymbols,...obj.info.dataSymbols])
    for(const [,memory] of JSON.parse(sym.memoryABI||'[]'))cg.requiredMemory=Math.max(cg.requiredMemory,memory);
  const afterLink = performance.now();
  self.MemoryTrampoline.lower(w, wmod, cg, linkOptions);
  const bytes = C.finalizeProgram(wmod, cg, linkOptions);
  const names = live.map(({obj,idx}) => obj.info.funcSymbols[idx].name);
  const lowered = live.map(({obj,idx}) => obj.info.funcs[idx-obj.imports.length].controlFlow?.restructured);
  return { bytes, stats: { functions: live.length, names,
    dispatchFunctions:lowered.filter(mode => mode === 'dispatcher').length,
    restructuredFunctions:lowered.filter(mode => mode === 'non-ssa-structured').length,
    trampoline, objects: pulled.map((o) => o.name), bytes: bytes.length,
    phaseMilliseconds:{linkAndGC:afterLink-started,backend:performance.now()-afterLink} } };
}

// Promote only scalar storage whose complete linked use set is direct loads
// and stores. References from legacy objects participate in the same proof;
// missing optional hints merely leave their definitions in linear memory.
function promoteLinkedScalars(mod,cg,objects,liveRanges,rangeAt,options){
  const w=W(),M=w.MOP,A=w.ALU,o=options.compilerOptions||{};
  if(cg.dynamic||o.enableFork||o.registerGlobals===false||mod.funcImports.some(f=>['__thread_spawn','__vfork_impl','__clone_impl'].includes(f.functionName)))return;
  if(objects.some(obj=>[...obj.info.funcSymbols,...obj.info.dataSymbols].some(s=>s.memoryABI)))return;
  const all=mod.funcDefs.flatMap(f=>f.wast||[]);
  if(all.some(n=>n.memory>0||n.destination>0||n.source>0))return;
  const candidates=new Map();
  for(const obj of objects)for(const s of obj.scalars||[]){
    if(![0x7f,0x7e,0x7d,0x7c].includes(s.type)||s.size!==([0x7e,0x7c].includes(s.type)?8:4)||s.offset<0||s.offset+s.size>obj.blob.length)continue;
    const ri=rangeAt(obj,s.offset);if(!liveRanges.has(obj.name+'@'+ri))continue;
    candidates.set(obj.linkBase+s.offset,{...s,addr:obj.linkBase+s.offset,uses:[],bad:false});
  }
  // A pointer initializer publishes an address even if its target has no
  // address-taking in its own translation unit. Relocated initial values also
  // need runtime initialization, so retain them in memory for now.
  for(const {obj,ri} of liveRanges.values())for(const r of obj.info.dataRelocs){
    const range=obj.info.ranges[ri];if(r.offset<range.start||r.offset>=range.start+range.len)continue;
    const raw=new DataView(cg.staticData.buffer).getUint32(obj.linkBase-cg.stackPages*65536+r.offset,true);
    for(const c of candidates.values())if(raw>=c.addr&&raw<c.addr+c.size||obj.linkBase+r.offset>=c.addr&&obj.linkBase+r.offset<c.addr+c.size)c.bad=true;
  }
  const load={127:M.I32_LOAD,126:M.I64_LOAD,125:M.F32_LOAD,124:M.F64_LOAD};
  const store={127:M.I32_STORE,126:M.I64_STORE,125:M.F32_STORE,124:M.F64_STORE};
  const unary=n=>n.op===A.OP_EQZ||(n.op>=A.OP_CLZ&&n.op<=A.OP_POPCNT)||(n.op>=A.OP_ABS&&n.op<=A.OP_SQRT)||n.op>=A.OP_WRAP_I64;
  for(const def of mod.funcDefs){const ns=def.wast||[];
    for(let i=0;i<ns.length;i++){
      const n=ns[i];if(!(n instanceof w.WConst)||!n.linkAddress)continue;
      const address=Number(n.value),c=candidates.get(address);
      if(!c){for(const s of candidates.values())if(address>s.addr&&address<s.addr+s.size)s.bad=true;continue;}
      let depth=0,found=false;
      for(let j=i+1;j<ns.length;j++){
        const v=ns[j];if(v instanceof w.WSrcLoc||v instanceof w.WNop)continue;
        if(v instanceof w.WMop&&v.memory===0&&v.offset===0&&((depth===0&&v.opcode===load[c.type])||(depth===1&&v.opcode===store[c.type]))){c.uses.push({def,address:n,op:v,write:depth===1});found=true;break;}
        if(v instanceof w.WConst||v instanceof w.WLocalGet||v instanceof w.WGlobalGet){depth++;continue;}
        if(v instanceof w.WLocalTee&&depth>=1)continue;
        if(v instanceof w.WAop&&depth>=(unary(v)?1:2)){if(!unary(v))depth--;continue;}
        if(v instanceof w.WMop&&v.opcode<=M.I64_LOAD32_U&&depth>=1)continue;
        break;
      }
      if(!found)c.bad=true;
    }
  }
  const replace=new Map(),remove=new Set(),view=new DataView(cg.staticData.buffer);
  for(const c of candidates.values()){
    if(c.bad||!c.uses.length)continue;
    const off=c.addr-cg.stackPages*65536;
    const id=c.type===127?mod.addGlobalI32(view.getInt32(off,true),true):c.type===126?mod.addGlobalI64(view.getBigInt64(off,true),true):c.type===125?mod.addGlobalF32(view.getFloat32(off,true),true):mod.addGlobalF64(view.getFloat64(off,true),true);
    mod.globalNames.push({idx:id,name:c.name});
    for(const u of c.uses){remove.add(u.address);replace.set(u.op,u.write?new w.WGlobalSet(id):new w.WGlobalGet(id));}
    cg.staticData.fill(0,off,off+c.size);
  }
  for(const def of mod.funcDefs)if(def.wast)def.wast=def.wast.filter(n=>!remove.has(n)).map(n=>replace.get(n)||n);
}

// PIC output retains explicit relocations through continuation lowering. No
// guessing whether an arbitrary integer happens to look like an address.
function finalizeDynamic(mod,cg,options){
  const w=W(),I=w.WT_I32,B=w.WastBuilder,A=w.ALU,base=cg.stackPages*65536,d=cg.dynamic;
  const spec={memoryPolicy:'private-snapshot-v1',version:4,execution:{clone:options.compilerOptions.enableFork===true},nativeImports:[...d.nativeImports],engineImports:[...d.engineImports],shared:d.shared,runtime:!!options.compilerOptions.runtime,sharedLibc:!!options.compilerOptions.sharedLibc,
    needed:(options.compilerOptions.sharedLibraries||[]).map(l=>l.name),
    soname:options.compilerOptions.soname||null,runpath:options.compilerOptions.runpath||[],
    deferredMain:!d.shared&&(options.compilerOptions.sharedLibraries||[]).length>0,stack:base,memorySize:cg.staticDataOffset,memoryAlign:cg.staticAlignment,
    tlsSize:cg.tlsSize,tlsAlign:cg.tlsAlign,tlsRanges:cg.tlsRanges.map(r=>({at:r.addr-base,offset:r.offset,size:r.size})),
    memoryABIs:d.memoryABIs,...(Object.keys(d.noUnwindABIs).length?{noUnwindABIs:d.noUnwindABIs}:{}),externalFunctions:Object.fromEntries(d.externalFunctions),symbols:d.symbols,relocations:d.relocations,descriptors:d.descriptors,
    signatures:mod.typeDefs.map(t=>({params:t.params.map(t=>t.num),results:t.results.map(t=>t.num)})),
    tags:[...cg.exceptionToWasmTagIdx].map(([tag,index])=>({name:tag.name,index,params:mod.typeDefs[mod.tags[index].typeIdx].params.map(t=>t.num)})),imports:[]};
  const replacements=new Map(),named=new Map();
  function imported(name,request,replace,mutable=false){
    if(named.has(name))return named.get(name);
    const idx=replace===undefined?mod.addGlobalI32(0,mutable):replace;
    replacements.set(idx,{moduleName:'dl',nameBytes:new TextEncoder().encode(name),type:I,isMutable:mutable});
    named.set(name,idx);spec.imports.push({importName:name,...request});return idx;
  }
  const memoryBase=imported('memory_base',{kind:'memoryBase'}),tableBase=imported('table_base',{kind:'tableBase'});
  imported('stack_pointer',{kind:'stack'},cg.stackPointerGlobalIdx,true);
  imported('thread_state',{kind:'thread'},cg.threadStateGlobalIdx,true);
  imported('tls_base',{kind:'tlsBase'},cg.tlsBaseGlobalIdx,true);
  imported('heap_base',{kind:'heap'},cg.heapBaseGlobalIdx);
  const expr=(b,r)=>{
    if(r.kind==='tls'){b.i32Const(r.value);return;}
    if(r.kind==='data'){b.globalGet(memoryBase);b.i32Const(r.value-base);b.aop(I,A.OP_ADD);return;}
    if(r.kind==='function'){
      const name=d.externalFunctions.get(r.value-1);
      if(name)b.globalGet(imported('function:'+name,{kind:'externalFunction',nameSymbol:name},undefined,spec.deferredMain));
      else {b.globalGet(tableBase);b.i32Const(r.value-1);b.aop(I,A.OP_ADD);}return;
    }
    const key=r.kind+':'+(r.name??r.value??'');
    b.globalGet(imported(key,r,undefined,r.kind==='generation'||(spec.deferredMain&&['externalFunction','external','externalTLS','setjmp'].includes(r.kind))));
    if(r.offset){b.i32Const(r.offset);b.aop(I,A.OP_ADD);}
    // The original TLS address expression already adds our TLS base.
    if(r.kind==='externalTLS'){b.globalGet(cg.tlsBaseGlobalIdx);b.aop(I,A.OP_SUB);}
  };
  for(const def of mod.funcDefs){if(!def.wast)throw new Error('Dynamic ABI requires instruction bodies');
    const b=new B();for(const n of def.wast){if(n.dynamic){const r=n.dynamic;if(r.kind==='tls')r.value=Number(n.value);expr(b,r);}else b.nodes.push(n);}def.wast=b.nodes;
  }
  // One process initialization; subsequent pthread instances only install code.
  const init=new B();init.globalGet(memoryBase);init.i32Const(0);init.i32Const(spec.memorySize);init.memoryFill();
  const data=cg.staticData;let i=0;
  while(i<data.length){while(i<data.length&&!data[i])i++;if(i===data.length)break;const start=i;let end=i;
    while(i<data.length){if(data[i]){end=++i;continue;}let j=i;while(j<data.length&&!data[j])j++;if(j-i>=64||j===data.length)break;i=j;}
    const seg=mod.dataSegments.length;mod.addDataSegment(0,data.slice(start,end),true);
    init.globalGet(memoryBase);init.i32Const(start);init.aop(I,A.OP_ADD);init.i32Const(0);init.i32Const(end-start);
    const bytes=[0xfc,8];lebU(bytes,seg);bytes.push(0);const node=new w.WRaw(bytes);node.signature={params:[I,I,I],results:[]};init.nodes.push(node);
  }
  const fi=mod.addFunctionDefinition(mod.addFunctionTypeId([],[]));mod.funcDefs[fi-mod.funcImports.length].wast=init.nodes;mod.addExport('__dylink_init',0,fi);
  mod.addMemoryImport('c','memory',1,16384,true);
  let extraMemories=cg.requiredMemory||0;for(const d of mod.funcDefs)for(const n of d.wast||[])extraMemories=Math.max(extraMemories,n.memory||(n instanceof w.WMemoryCopy?Math.max(n.destination,n.source):n instanceof w.WRaw&&n.bytes[0]===254&&n.bytes[2]===66?n.bytes[3]:0));
  for(let i=1;i<=extraMemories;i++)mod.addExport('memory'+i,2,mod.addMemoryImport('c','memory'+i,1,16384,true));mod.addExport('memory',2,0);mod.addExport('__indirect_function_table',1,0);mod.addExport('__heap_base',3,cg.heapBaseGlobalIdx);
  for(const s of spec.symbols)if(s.kind==='function'&&!mod.exports.some(e=>e.name==='__dl_fn_'+s.value))mod.addExport('__dl_fn_'+s.value,0,s.value-1);
  // Global imports must precede definitions. Remap once, after all symbolic
  // globals have been discovered, including those created by the backend.
  if(mod.globalImports.length)throw new Error('Dynamic objects with custom global imports are unsupported');
  const remap=new Map(),defs=[];mod.globalImports=[...replacements.values()];let n=0;
  for(const idx of replacements.keys())remap.set(idx,n++);
  mod.globals.forEach((g,idx)=>{if(!replacements.has(idx)){remap.set(idx,n++);defs.push(g);}});mod.globals=defs;
  for(const def of mod.funcDefs)for(const node of def.wast||[])if(node instanceof w.WGlobalGet||node instanceof w.WGlobalSet)node.idx=remap.get(node.idx);
  for(const e of mod.exports)if(e.kind===3)e.index=remap.get(e.index);
  for(const e of mod.globalNames)e.idx=remap.get(e.idx);
  mod.dynamicLink={tableGlobal:remap.get(tableBase),tags:spec.tags};
  // All table slots are ABI identities, including adapters and TLS helpers.
  for(let i=0;i<mod.funcDefs.length;i++)mod.addrTakenFuncs.add(mod.funcImports.length+i);
  spec.tableSize=mod.funcImports.length+mod.funcDefs.length;
  spec.signatures=mod.typeDefs.map(t=>({params:t.params.map(t=>t.num),results:t.results.map(t=>t.num)}));
  mod.sourceMapFiles=cg.sourceMapFiles;mod.sourceMapEntries=cg.sourceMapEntries;
  const bytes=mod.emit(),payload=[];str(payload,'wasmposix.dylink');
  const json=new TextEncoder().encode(JSON.stringify(spec));const head=[0];lebU(head,payload.length+json.length);
  const result=new Uint8Array(bytes.length+head.length+payload.length+json.length);result.set(bytes);result.set(head,bytes.length);result.set(payload,bytes.length+head.length);result.set(json,bytes.length+head.length+payload.length);
  if(!WebAssembly.validate(result)){try{new WebAssembly.Module(result);}catch(error){throw new Error('Invalid dynamic WebAssembly output: '+error.message);}throw new Error('Invalid dynamic WebAssembly output');}return result;
}

return { readSharedLibrary, finalizeDynamic, OBJECT_BASE, writeObject, readObject, decodeBody, readArchive, writeArchive, link };
})();
