// Numeric C continuation backend with native calls and JSPI suspension.
// Framed locals and operands live across a transfer have memory slots.
self.MemoryTrampoline = { lower(W, mod, cg, options) {
  const {WT_I32:I32,WT_I64:I64,WT_F32:F32,WT_F64:F64,WT_V128:V128,WT_EMPTY:EMPTY,MOP:M,ALU:A,WastBuilder:B}=W;
  if (mod.funcImports.some(i => /^__(?:mmap|munmap|mprotect|msync|vm_load|vm_store|vm_copy|vm_fill)$/.test(i.functionName)))
    throw new Error('Legacy virtual-memory imports cannot be linked into new programs; rebuild mapping code against the private-snapshot mmap API');
  cg.enableFork = options.compilerOptions?.enableFork === true;
  const cloneNames=new Set(['fork','vfork','clone','__fork_impl','__vfork_impl','__clone_impl']);
  if (!cg.enableFork && (mod.funcImports.some(i => cloneNames.has(i.functionName)) || [...cg.dynamic?.externalFunctions.values() || []].some(name=>cloneNames.has(name))))
    throw new Error('process cloning requires --enable-fork; prefer posix_spawn for fresh subprocesses');

  mod.addExport('__fork_enabled', 3, mod.addGlobalI32(cg.enableFork ? 1 : 0, false));
  // The record lives at the address in the per-instance __thread_state global:
  // 1024 for the main thread, a thread's own arena base otherwise. LIMIT is
  // the top of that thread's C stack; zero means the process default.
  const T=1024, TS=cg.threadStateGlobalIdx, CUR=0, TOP=4, TAG=8, BUDGET=12, YIELDS=16, DEPTH=20, ROOT_ASYNC=24, LIMIT=28, PAYLOAD=32;
  const PREV=0,FUNC=4,PC=8,DEST=12,SHADOW=16,SIZE=20,EH=24,JUMPS=28,RESULT=32,HEADER=48;
  // Public DSO definitions are roots even before their Wasm exports are
  // materialized. Otherwise the inliner incorrectly grants a single-use
  // size-budget bypass to bodies that must remain externally callable.
  for(const symbol of cg.dynamic?.symbols || [])if(symbol.kind==='function')mod.addrTakenFuncs.add(symbol.value-1);
  // Optimize ordinary WAST before assigning frame layouts or resume PCs.
  const inlineEnabled=options.compilerOptions?.wastInline!==false&&!options.compilerOptions?.noInline;
  const nativeEnabled=options.compilerOptions?.nativeFunctions!==false;
  const jspi=nativeEnabled&&options.compilerOptions?.jspi!==false;
  // Shared across instances on this thread. A nonzero value means an engine
  // stack exists which fork cannot reconstruct. Reserve the final record word.
  const ENGINE_DEPTH=2044;
  // Validate deferred callable conversions before inlining/dead-body elision,
  // including builds which disable inlining.
  const inlineEffects=self.CompilerJS.FUNCTION_EFFECTS.analyze(W,mod,cg);
  const isNative=id=>nativeEnabled&&id>=mod.funcImports.length&&inlineEffects.functions[id].eligible;
  // Approximate encoded bytes, not raw WAST node counts. Framed branches
  // introduce PC/operand stores, dispatch, and budget/handler checkpoints;
  // framed locals use memory slots. The final analysis below remains authoritative.
  // Cache by body identity: bottom-up inlining replaces arrays when they change.
  const inlineCosts=new WeakMap();
  function inlineNodeCost(n,native){
    if(n instanceof W.WSrcLoc)return 0;
    if(n instanceof W.WCall || n instanceof W.WCallIndirect){
      const directNative=n instanceof W.WCall?isNative(n.funcIdx):cg.dynamic?.nativeImports.has(n.nativeCall);
      return native?4:directNative?44:120;
    }
    if(n instanceof W.WLocalGet)return native?3:7;
    if(n instanceof W.WLocalSet || n instanceof W.WLocalTee)return native?3:12;
    if(n instanceof W.WIf || n instanceof W.WElse || n instanceof W.WBr || n instanceof W.WBrIf || n instanceof W.WReturn)return native?3:60;
    if(n instanceof W.WBrTable)return native?4+n.targets.length*2:60+n.targets.length*12;
    if(n instanceof W.WBlock || n instanceof W.WLoop || n instanceof W.WEnd)return native?2:0;
    if(n instanceof W.WMop)return 5;
    if(n instanceof W.WConst)return 5;
    if(n instanceof W.WGlobalGet || n instanceof W.WGlobalSet)return 3;
    return 2;
  }
  function bodyCost(body,native){
    let costs=inlineCosts.get(body);if(!costs){costs={};inlineCosts.set(body,costs);}
    return costs[native]??(costs[native]=body.reduce((sum,n)=>sum+inlineNodeCost(n,native),0));
  }
  const costModel=({caller,callee,body,params,singleUse})=>{
    if(isNative(caller))return null; // ordinary native WAST budgets still apply
    const growth=bodyCost(body,false)+params*12+60-(isNative(callee)?44:120);
    return {growth,netGrowth:growth-(singleUse?bodyCost(body,isNative(callee)):0),maxGrowth:2048};
  };
  // Literal ownership still names the original functions. Preserve their live
  // set before inlining moves address constants into callers and elides owners.
  cg.literalLiveBeforeInlining=W.computeLiveFunctions(mod);
  mod.preContinuationInline=W.inlineFunctions(mod,{enabled:inlineEnabled,costModel});
  mod.inlineOptions={enabled:false};
  // Descriptor identities must remain stable (function pointers and dynamic
  // relocations use them). Elide unreachable bodies without renumbering slots,
  // before descriptors would otherwise root every inlined-away definition.
  const live=W.computeLiveFunctions(mod);let elidedFunctions=0;
  if(live)for(let i=0;i<live.length;i++)if(!live[i]){
    const def=mod.funcDefs[i];def.wast=[new W.WUnreachable()];def.locals=[];
    def.fnMeta=null;def.controlFlow={version:2,structured:true};def.elided=true;elidedFunctions++;
  }
  // Legacy scalar SSA is an explicit diagnostic/optimization opt-in. The
  // production structured-first path never builds a value graph for ordinary code.
  const useSSA=options.compilerOptions?.ssa===true;
  const ssaEligibilityStart=performance.now();
  const ssaExecution=useSSA?self.CompilerJS.FUNCTION_EFFECTS.execution(W,mod,cg,self.CompilerJS.FUNCTION_EFFECTS.analyze(W,mod,cg)):null;
  const ssaEligibilityMs=performance.now()-ssaEligibilityStart;
  const ssaStats=useSSA
    ? self.CompilerJS.SCALAR_SSA.optimize(W,mod,{onSSA:options.onSSA,execution:ssaExecution,licm:options.compilerOptions?.ssaLICM===true,verify:options.compilerOptions?.verifySSA,memory:options.compilerOptions?.ssaMemory,sccp:options.compilerOptions?.ssaSCCP,induction:options.compilerOptions?.ssaInduction}) : null;
  if(ssaStats)ssaStats.eligibilityMilliseconds=ssaEligibilityMs;
  const nImp=mod.funcImports.length, originals=mod.funcDefs.slice(), count=nImp+originals.length;
  const analysis=self.CompilerJS.FUNCTION_EFFECTS.analyze(W,mod,cg);
  const execution=self.CompilerJS.FUNCTION_EFFECTS.execution(W,mod,cg,analysis);
  const structuredStats=!useSSA&&!options.compilerOptions?.noFold&&options.compilerOptions?.structuredOptimize!==false
    ?self.CompilerJS.STRUCTURED_OPT.optimize(W,mod,execution):null;
  const numeric=t=>{if(!t||t.tag!=='num')throw new Error('Memory-frame ABI supports numeric C values; Wasm reference extensions are not supported');return t;};
  const hasVectors=self.CompilerJS.SIMD128.moduleUsesSIMD(mod);
  const vectorScratch=hasVectors?[V128]:[];
  // Preserve the scalar ABI exactly. Only vector values require a wider,
  // aligned slot; parameter prefixes have the same layout in every module.
  function layout(types){let size=0;const offsets=types.map(t=>{const width=t.num===123?16:8;size=(size+width-1)&-width;const off=size;size+=width;return off;});return {offsets,size};}
  const paramOffset=(types,i)=>layout(types).offsets[i];
  function frameLayout(inf,ir){
    const locals=layout(inf.locals);inf.localOffsets=locals.offsets.map(o=>HEADER+o);
    inf.stackSlot=inf.locals.some(t=>t.num===123)||inf.sig.results.some(t=>t.num===123)||ir?.before.some(ts=>ts?.some(t=>t.num===123))?16:8;
    inf.stackOffset=HEADER+locals.size;if(inf.stackSlot===16)inf.stackOffset=(inf.stackOffset+15)&~15;
    inf.size=(inf.stackOffset+inf.stackSlot*((ir?.maxStack||0)+2)+15)&~15;
  }
  const loadOp=t=>({127:M.I32_LOAD,126:M.I64_LOAD,125:M.F32_LOAD,124:M.F64_LOAD})[numeric(t).num];
  const storeOp=t=>({127:M.I32_STORE,126:M.I64_STORE,125:M.F32_STORE,124:M.F64_STORE})[numeric(t).num];
  const sig=i=>mod.typeDefs[i<nImp?mod.funcImports[i].typeId:originals[i-nImp].typeId];
  const typeId=i=>i<nImp?mod.funcImports[i].typeId:originals[i-nImp].typeId;
  const names=new Map(mod.funcNames.map(n=>[n.idx,n.name]));
  // Use semantic definitions, independent of whether debug names are emitted.
  const setjmpId=[...cg.funcDefToWasmFuncIdx].find(([f])=>f.name==='setjmp')?.[1];
  // Private raw-scalar libffi entry retains a local signature-dispatch adapter.
  // Its typed JSPI entry does not require its callers to own a frame.
  const ffiId=[...cg.funcDefToWasmFuncIdx].find(([f])=>f.name==='__wasm_posix_ffi_call')?.[1];
  const jumpTag=[...cg.exceptionToWasmTagIdx].find(([tag])=>tag.name==='__LongJump')?.[1];
  function localsOf(def,s){const a=s.params.slice();for(const l of def.locals)for(let j=0;j<l.count;j++)a.push(l.type);a.forEach(numeric);return a;}
  const infos=Array.from({length:count},(_,i)=>({id:i,sig:sig(i),locals:i<nImp?sig(i).params.slice():localsOf(originals[i-nImp],sig(i))}));
  for(const inf of infos){inf.native=nativeEnabled&&(analysis.functions[inf.id].eligible&&inf.id>=nImp||jspi&&execution[inf.id].eligible);inf.engine=inf.native&&!analysis.functions[inf.id].eligible;}
  // Keep the strict native contract distinct from the suspendable JSPI
  // contract. A local capture/ffi adapter can also provide the latter.
  for(const symbol of cg.dynamic?.symbols || [])if(symbol.kind==='function'){
    const id=symbol.value-1;
    if(infos[id]?.native&&analysis.functions[id].eligible)symbol.execution='native-v1';
    else if(execution[id]?.clone)symbol.execution='clone-v1';
    else if(jspi&&id!==setjmpId)symbol.execution='jspi-v1';
  }
  if(ffiId!==undefined){
    const inf=infos[ffiId];
    if(inf.sig.params.length!==4||inf.sig.params.some(t=>t.num!==127)||(inf.sig.results.length>1||inf.sig.results.some(t=>t.num!==127)))
      throw new Error('Invalid __wasm_posix_ffi_call ABI: '+JSON.stringify(inf.sig));
    inf.native=false;inf.engine=false;
  }
  // A single direct capture can resume by re-entering its enclosing empty
  // blocks/loops. Prefixes execute only on the initial path; the complete
  // function stays inside the handler, including code after the lexical scope.
  // Keep the general backend for operand snapshots, hidden temporaries, EH,
  // conditional ancestors, indirect captures, DSOs and cloning executables.
  const structuredCaptures=[];
  const structuredEnabled=jspi&&!cg.dynamic&&!cg.enableFork&&ffiId===undefined&&
    options.compilerOptions?.structuredSetjmp!==false&&jumpTag!==undefined;
  const captureGlobals=[...mod.globalImports.map(g=>g.type),...mod.globals.map(g=>g.type)];
  if(structuredEnabled)for(const inf of infos){
    if(inf.id<nImp||inf.native||inf.id===setjmpId||execution[inf.id].indirect||execution[inf.id].compatibility)continue;
    const def=originals[inf.id-nImp],nodes=def.wast;
    const sites=nodes.flatMap((n,i)=>n instanceof W.WCall&&n.funcIdx===setjmpId?[i]:[]);
    if(sites.length!==1||nodes.some(n=>n instanceof W.WTryTable||n instanceof W.WThrow||n instanceof W.WRaw))continue;
    const at=sites[0];if(nodes[at].continuationRegions?.length)continue;
    const ir=self.CompilerJS.CONTROL_FLOW_IR.build(W,{nodes,locals:inf.locals,signature:inf.sig,globals:captureGlobals,sig,types:mod.typeDefs,name:names.get(inf.id),callEffect:()=>false});
    if(ir.before[at]?.length!==1)continue;
    const ancestors=[...ir.control.values()].filter(c=>c.start<at&&c.end>at).sort((a,b)=>a.start-b.start);
    if(ancestors.some(c=>!(c.node instanceof W.WBlock||c.node instanceof W.WLoop)||c.base.length||c.results.length||c.entryDead))continue;
    inf.native=true;inf.engine=true;inf.structuredCapture={at,ancestors};structuredCaptures.push(inf);
  }
  // Reserve the helper before rewriting bodies; the ordinary frame backend
  // emits its implementation below. Its node ABI also permits mixed native /
  // framed longjmp chains without changing jmp_buf or runtime contracts.
  const structuredSave=structuredCaptures.length?mod.addFunctionDefinition(mod.addFunctionTypeId(Array(7).fill(I32),[I32])):null;
  if(structuredSave!==null)mod.funcNames.push({idx:structuredSave,name:'__continuation_save'});
  for(const inf of structuredCaptures){
    const def=originals[inf.id-nImp],{at,ancestors}=inf.structuredCapture,original=def.wast,b=new B();
    const top=inf.locals.length,fp=top+1,resume=top+2,value=top+3,node=top+4,env=top+5;
    def.locals.push({count:6,type:I32});inf.locals.push(...Array(6).fill(I32));inf.structuredTopLocal=top;
    const getTop=()=>{b.globalGet(TS);b.mop(M.I32_LOAD,TOP,2);};
    const putTop=f=>{b.globalGet(TS);f();b.mop(M.I32_STORE,TOP,2);};
    getTop();b.localTee(top);b.if_(I32);b.localGet(top);b.else_();b.globalGet(TS);b.i32Const(3072);b.aop(I32,A.OP_ADD);b.end();b.localSet(fp);
    b.localGet(fp);b.i32Const(HEADER);b.aop(I32,A.OP_ADD);b.localTee(node);b.globalGet(cg.stackPointerGlobalIdx);b.aop(I32,A.OP_GE,false);
    b.localGet(node);b.localGet(fp);b.aop(I32,A.OP_LT,false);b.aop(I32,A.OP_OR);b.if_(EMPTY);b.unreachable();b.end();
    b.localGet(fp);b.i32Const(0);b.i32Const(HEADER);b.memoryFill();putTop(()=>b.localGet(node));
    b.block(inf.sig.results[0]||EMPTY);const done=b.ctrl.at(-1);
    b.loop();const retry=b.ctrl.at(-1);
    b.block({tag:'typeidx',idx:mod.addFunctionTypeId([],[I32,I32])});b.tryTable(EMPTY,[[0,jumpTag,0]]);
    const emit=n=>{
      if(n instanceof W.WReturn)b.nodes.push(new W.WBr(done));
      else if(n instanceof W.WBr&&n.target?.isFuncLabel)b.nodes.push(new W.WBr(done));
      else if(n instanceof W.WBrIf&&n.target?.isFuncLabel)b.nodes.push(new W.WBrIf(done));
      else if(n instanceof W.WBrTable)b.nodes.push(new W.WBrTable(n.targets.map(t=>t.isFuncLabel?done:t),n.defaultTarget.isFuncLabel?done:n.defaultTarget));
      else b.nodes.push(n);
    };
    let start=0;
    for(const c of ancestors){
      if(c.start>start){b.localGet(resume);b.aop(I32,A.OP_EQZ);b.if_(EMPTY);original.slice(start,c.start).forEach(emit);b.end();}
      emit(c.node);start=c.start+1;
    }
    b.localGet(resume);b.if_(I32);b.i32Const(0);b.localSet(resume);b.localGet(value);
    b.else_();original.slice(start,at).forEach(emit);b.localSet(env);
    b.localGet(fp);b.localGet(env);for(let i=0;i<5;i++)b.i32Const(0);b.call(structuredSave);b.drop();b.i32Const(0);b.end();
    original.slice(at+1).forEach(emit);b.nodes.push(new W.WBr(done));
    b.end();b.unreachable();b.end();b.localSet(value);b.localSet(node);
    b.localGet(node);b.mop(M.I32_LOAD,28,2);b.localGet(fp);b.aop(I32,A.OP_NE);b.if_(EMPTY);b.localGet(node);b.localGet(value);b.throw_(jumpTag);b.end();
    b.localGet(node);b.mop(M.I32_LOAD,16,2);b.globalSet(cg.stackPointerGlobalIdx);b.nodes.at(-1).stackRestore=true;
    b.i32Const(1);b.localSet(resume);b.nodes.push(new W.WBr(retry));b.end();b.unreachable();b.end();
    def.wast=b.nodes;W.validate(def.wast,null);
  }
  const needsFrames=!!cg.dynamic || cg.enableFork || infos.some(inf=>inf.id>=nImp&&!inf.native&&!originals[inf.id-nImp].elided);
  const stackWrites=def=>(def.wast||[]).some(n=>n instanceof W.WGlobalSet&&n.idx===cg.stackPointerGlobalIdx);
  // alloca's caller-frees ABI can retain SP across a normal return. A caller
  // without local allocations may still need exception cleanup after that call.
  // Imports and dynamic callbacks have no stack-balance contract; keep them
  // conservative. Ordinary fixed-frame C functions restore SP on every return.
  const retainsStack=infos.map(inf=>inf.id<nImp || !!originals[inf.id-nImp]?.fnMeta?.usesAlloca ||
    (!originals[inf.id-nImp]?.fnMeta && stackWrites(originals[inf.id-nImp])));
  const callbackTargets=[...mod.addrTakenFuncs].filter(id=>id<count);
  let changed=true;
  while(changed){changed=false;
    for(const inf of infos){
      if(inf.id<nImp||retainsStack[inf.id])continue;
      const def=originals[inf.id-nImp];
      if(def.fnMeta?.frameSize>0&&!def.fnMeta.usesAlloca)continue;
      if((def.wast||[]).some(n=>n instanceof W.WCall?retainsStack[n.funcIdx]:n instanceof W.WCallIndirect&&(cg.dynamic||callbackTargets.some(id=>retainsStack[id])))){
        retainsStack[inf.id]=true;changed=true;
      }
    }
  }
  for(const inf of infos){inf.sig.params.forEach(numeric);inf.sig.results.forEach(numeric);if(inf.sig.results.length>1)throw new Error('Multiple native C return values are unsupported');}
  const globals=[...mod.globalImports.map(g=>g.type),...mod.globals.map(g=>g.type)];
  const IR=self.CompilerJS.CONTROL_FLOW_IR;
  const engineImport=n=>cg.dynamic?.nativeImports.has(n.nativeCall)||(jspi&&cg.dynamic?.engineImports.has(n.nativeCall));
  // Closed, non-cloning callbacks can stay on the engine stack even when
  // their caller owns a setjmp frame. A captured callee has a typed entry
  // wrapper; only calling setjmp itself indirectly captures this caller.
  const indirectCapture=[...mod.addrTakenFuncs].includes(setjmpId)||!!cg.dynamic;
  const closedCallbacks=nativeEnabled&&jspi&&!cg.enableFork&&
    callbackTargets.every(id=>!execution[id].compatibility);
  const captureSignature=type=>{
    const s=mod.typeDefs[type];
    return s.params.length===1&&s.params[0].num===127&&s.results.length===1&&s.results[0].num===127;
  };
  const engineCall=n=>engineImport(n)||(closedCallbacks&&
    (n.noCapture===true||!indirectCapture||!captureSignature(n.typeIdx)));
  // A non-cloning capture callee owns its own frame. Calling its typed entry
  // does not capture this caller, just as for the indirect callback path.
  const engineDirect=n=>infos[n.funcIdx].native||(closedCallbacks&&n.funcIdx!==setjmpId&&
    !execution[n.funcIdx].clone&&!execution[n.funcIdx].compatibility);
  const callEffect=n=>n instanceof W.WCall?!engineDirect(n):!engineCall(n);
  // Framed/dynamic calls may execute scheduler, compatibility, or module-sync
  // hooks introduced after body analysis. Keep their exceptional edges.
  const callMayUnwind=n=>!!cg.dynamic||callEffect(n)||self.CompilerJS.FUNCTION_EFFECTS.operationMayUnwind(W,n,analysis.functions);
  function analyze(inf){
    const ir=IR.build(W,{nodes:originals[inf.id-nImp].wast,locals:inf.locals,signature:inf.sig,globals,sig,types:mod.typeDefs,name:names.get(inf.id),callEffect,callMayUnwind});
    const disabled=options.compilerOptions?.cleanRegions===false || !nativeEnabled;
    const regions=IR.regions(W,ir,{callEffect,stackGlobal:cg.stackPointerGlobalIdx,disabled});
    Object.assign(inf,ir);inf.ir=ir;inf.regions=regions;
    inf.blocks=IR.schedule(ir,regions);inf.blockAt=new Map(inf.blocks.map(b=>[b.start,b]));
    inf.regionLocals=[...new Set(regions.flatMap(r=>r.locals))];
    inf.regionLocalMap=new Map(inf.regionLocals.map((id,i)=>[id,(hasVectors?9:8)+i]));
    frameLayout(inf,ir);
    options.onControlFlowIR?.(names.get(inf.id),IR.dump(ir,regions));
  }
  for(const inf of infos){if(inf.id>=nImp&&!inf.native)analyze(inf);else frameLayout(inf);}
  for(const tag of mod.tags){const ps=mod.typeDefs[tag.typeIdx].params;ps.forEach(numeric);if(PAYLOAD+layout(ps).size>ENGINE_DEPTH)throw new Error('Exception payload exceeds the thread record capacity');}
  // Optional, per-instance diagnostic globals; disabled builds pay no cost.
  const metrics={};
  if(options.compilerOptions?.instrumentDispatch)for(const name of ['steps','frames','native_calls','yields','peak_frame_bytes']){
    metrics[name]=mod.addGlobalI32(0,true);mod.addExport('__dispatch_'+name,3,metrics[name]);
  }
  function increment(b,name){if(metrics[name]===undefined)return;b.globalGet(metrics[name]);b.i32Const(1);b.aop(I32,A.OP_ADD);b.globalSet(metrics[name]);}
  function peakFrames(b){
    if(metrics.peak_frame_bytes===undefined)return;
    const bytes=()=>{tg(b,TOP);b.globalGet(TS);b.i32Const(3072);b.aop(I32,A.OP_ADD);b.aop(I32,A.OP_SUB);};
    b.globalGet(metrics.peak_frame_bytes);bytes();b.aop(I32,A.OP_LT,false);b.if_(EMPTY);bytes();b.globalSet(metrics.peak_frame_bytes);b.end();
  }
  // Check allocations, including subtraction wraparound. Restoring a compiler-
  // saved SP is already bounded; old objects lacking that fact keep the check.
  let stackChecks=0,elidedStackChecks=0,exceptionCleanups=0,elidedExceptionCleanups=0;
  for(const inf of infos)if(inf.native&&inf.id>=nImp){
    const def=originals[inf.id-nImp];
    if(!stackWrites(def))continue;
    const tmp=inf.locals.length,limit=tmp+1;def.locals.push({count:2,type:I32});
    const b=new B();
    for(const n of def.wast){
      if(n instanceof W.WGlobalSet&&n.idx===cg.stackPointerGlobalIdx){
        if(n.stackRestore){elidedStackChecks++;b.nodes.push(n);continue;}
        stackChecks++;
        b.localTee(tmp);
        if(needsFrames){
          b.globalGet(TS);b.mop(M.I32_LOAD,TOP,2);b.localTee(limit);b.if_(I32);
          b.localGet(limit);
          b.else_();b.globalGet(TS);b.i32Const(3072);b.aop(I32,A.OP_ADD);b.end();
        }else {b.globalGet(TS);b.i32Const(3072);b.aop(I32,A.OP_ADD);}
        b.aop(I32,A.OP_LT,false);
        b.localGet(tmp);b.globalGet(TS);b.mop(M.I32_LOAD,LIMIT,2);b.localTee(limit);b.if_(I32);
        b.localGet(limit);
        b.else_();b.i32Const(cg.stackPages*65536);b.end();
        b.aop(I32,A.OP_GT,false);b.aop(I32,A.OP_OR);b.if_(EMPTY);b.unreachable();b.end();b.localGet(tmp);
      }
      b.nodes.push(n);
    }
    def.wast=b.nodes;W.validate(def.wast,null);
  }
  for(const inf of infos)if(inf.native&&inf.id>=nImp&&metrics.native_calls!==undefined){const b=new B();increment(b,'native_calls');originals[inf.id-nImp].wast=[...b.nodes,...originals[inf.id-nImp].wast];}
  const stepType=mod.addFunctionTypeId([I32],[]);
  const publicFunctions=new Set((cg.dynamic?.symbols||[]).filter(s=>s.kind==='function').map(s=>s.value-1));
  const framedIndirect=ffiId!==undefined || infos.some(inf=>inf.id>=nImp&&!inf.native&&originals[inf.id-nImp].wast.some(n=>n instanceof W.WCallIndirect&&!engineCall(n)));
  let elidedAdapters=0;
  for(const inf of infos){
    if(!needsFrames || inf.native&&!((framedIndirect||cg.dynamic)&&mod.addrTakenFuncs.has(inf.id))&&!publicFunctions.has(inf.id)){
      inf.step=-1;elidedAdapters++;continue;
    }
    inf.step=mod.addFunctionDefinition(stepType);mod.funcNames.push({idx:inf.step,name:'__step_'+(names.get(inf.id)||inf.id)});}
  const dynamic=cg.dynamic;
  function symbolic(b,kind,value){b.i32Const(value);if(dynamic)b.nodes.at(-1).dynamic={kind,value};}
  const functionID=(b,idx)=>symbolic(b,'function',idx+1);
  const signatureID=(b,idx)=>symbolic(b,'signature',idx);
  const tagID=(b,idx)=>symbolic(b,'tag',idx);
  function descriptorAt(b,id){
    if(!dynamic){b.i32Const(descriptor);id();b.i32Const(16);b.aop(I32,A.OP_MUL);b.aop(I32,A.OP_ADD);return;}
    // Two-level process-wide registry: 1024 pages of 1024 descriptors.
    symbolic(b,'registry',0);id();b.i32Const(10);b.aop(I32,A.OP_SHR_U);b.i32Const(4);b.aop(I32,A.OP_MUL);b.aop(I32,A.OP_ADD);b.mop(M.I32_LOAD,0,2);
    id();b.i32Const(1023);b.aop(I32,A.OP_AND);b.i32Const(16);b.aop(I32,A.OP_MUL);b.aop(I32,A.OP_ADD);
  }
  const descriptor=needsFrames?cg.allocateStatic((count+1)*16,16):0;
  if(needsFrames)for(const inf of infos){const offset=descriptor-cg.stackPages*65536+(inf.id+1)*16;for(const [j,v] of [inf.step+1,inf.size,typeId(inf.id),inf.locals.length].entries())for(let k=0;k<4;k++)cg.staticData[offset+j*4+k]=(v>>>(8*k))&255;}
  if(dynamic)dynamic.descriptors=infos.map(inf=>({slot:inf.id+1,step:inf.step+1,size:inf.size,type:typeId(inf.id),locals:inf.locals.length}));
  const setDef=(idx,b,locals)=>{const d=mod.funcDefs[idx-nImp];d.wast=b.nodes;d.locals=locals.map(type=>({count:1,type}));d.body=[];d.fnMeta=null;W.validate(d.wast,null);};
  const add=(name,ps,rs)=>{const idx=mod.addFunctionDefinition(mod.addFunctionTypeId(ps,rs));mod.funcNames.push({idx,name});return idx;};
  function ld(b,base,off=0,t=I32){base();if(t.num===123)b.simd(0,[],0,off,0);else b.mop(loadOp(t),off,t.num===126||t.num===124?3:2);}
  function st(b,base,off,value,t=I32){base();value();if(t.num===123)b.simd(11,[],0,off,0);else b.mop(storeOp(t),off,t.num===126||t.num===124?3:2);}
  const absolute=(b,off)=>()=>{b.globalGet(TS);if(off){b.i32Const(off);b.aop(I32,A.OP_ADD);}};
  const tg=(b,off)=>ld(b,()=>b.globalGet(TS),off);
  const ts=(b,off,v)=>{st(b,()=>b.globalGet(TS),off,v);if(off===TOP)peakFrames(b);};
  const frame=b=>()=>b.localGet(0);
  const fg=(b,off,t=I32)=>ld(b,frame(b),off,t);
  const fs=(b,off,v,t=I32)=>st(b,frame(b),off,v,t);
  // Typed callback adapters preserve the platform's accepted unused trailing
  // arguments without spilling an activation into the continuation arena.
  // Exact-signature callbacks take one ordinary call_indirect.
  const callbackAdapters=new Map();
  let signatureTable=0;
  function needsCallbackAdapter(type){
    if(dynamic)return true; // A later DSO can introduce another signature.
    const s=mod.typeDefs[type];
    return callbackTargets.some(id=>{
      const actual=infos[id].sig;
      return actual.params.length<s.params.length && actual.results.length===s.results.length &&
        actual.results.every((t,i)=>t.num===s.results[i].num) && actual.params.every((t,i)=>t.num===s.params[i].num);
    });
  }
  function callbackAdapter(type){
    if(callbackAdapters.has(type))return callbackAdapters.get(type);
    const s=mod.typeDefs[type],target=s.params.length,actual=target+1;
    const id=add('__engine_callback_'+type,[...s.params,I32],s.results),b=new B();
    callbackAdapters.set(type,id);
    if(dynamic){symbolic(b,'control',0);const load=new W.WRaw([0xfe,0x10,2,0]);load.signature={params:[I32],results:[I32]};b.nodes.push(load);symbolic(b,'generation',0);b.aop(I32,A.OP_NE);b.if_(EMPTY);b.call(dynamic.syncFunction);b.end();}
    b.localGet(target);b.i32Const(1);b.aop(I32,A.OP_LT,false);b.localGet(target);b.i32Const(dynamic?1048575:count);b.aop(I32,A.OP_GT,false);b.aop(I32,A.OP_OR);b.if_(EMPTY);b.unreachable();b.end();
    if(needsFrames){descriptorAt(b,()=>b.localGet(target));b.mop(M.I32_LOAD,8,2);}
    else {
      if(!signatureTable){
        signatureTable=cg.allocateStatic((count+1)*4,4);
        for(const inf of infos){const off=signatureTable-cg.stackPages*65536+(inf.id+1)*4,v=typeId(inf.id);for(let k=0;k<4;k++)cg.staticData[off+k]=(v>>>(8*k))&255;}
      }
      b.i32Const(signatureTable);b.localGet(target);b.i32Const(4);b.aop(I32,A.OP_MUL);b.aop(I32,A.OP_ADD);b.mop(M.I32_LOAD,0,2);
    }
    b.localSet(actual);
    for(let k=s.params.length;k>=0;k--){
      const prefix=mod.addFunctionTypeId(s.params.slice(0,k),s.results);
      b.localGet(actual);signatureID(b,prefix);b.aop(I32,A.OP_EQ);b.if_(EMPTY);
      for(let j=0;j<k;j++)b.localGet(j);
      b.localGet(target);b.callIndirect(prefix);b.ret();b.end();
    }
    b.unreachable();setDef(id,b,[I32]);return id;
  }
  for(const inf of infos)if(inf.engine&&inf.id>=nImp){
    const def=originals[inf.id-nImp];
    def.wast=def.wast.map(n=>n instanceof W.WCallIndirect&&!engineImport(n)&&needsCallbackAdapter(n.typeIdx)?new W.WCall(callbackAdapter(n.typeIdx)):n);
  }
  // JSPI preserves these typed calls on the engine stack. Wasm exceptions
  // unwind them normally; restore the C shadow stack and the cloning barrier
  // on every exception, including JS failures and longjmp across instances.
  for(const inf of infos)if(inf.engine&&inf.id>=nImp){
    const def=originals[inf.id-nImp],b=new B();
    if(!inf.structuredCapture&&!cg.enableFork&&!cg.dynamic?.shared&&!stackWrites(def)&&!retainsStack[inf.id])continue;
    // Normal C epilogues already restore fixed frames. In a wholly native
    // image there are no later scheduler/frame hooks to invalidate this proof.
    if(!needsFrames && analysis.functions[inf.id].noUnwind && options.compilerOptions?.unwindOptimizations!==false){elidedExceptionCleanups++;continue;}
    exceptionCleanups++;
    const sp=inf.sig.params.length+def.locals.reduce((n,l)=>n+l.count,0);
    def.locals.push({count:1,type:I32});
    b.globalGet(cg.stackPointerGlobalIdx);b.localSet(sp);
    const depth=delta=>{if(cg.enableFork || cg.dynamic?.shared)ts(b,ENGINE_DEPTH,()=>{tg(b,ENGINE_DEPTH);b.i32Const(delta);b.aop(I32,A.OP_ADD);});};
    depth(1);
    const result=inf.sig.results[0]||EMPTY;
    b.block(result);const done=b.ctrl.at(-1);
    b.block(W.WT_EXNREF);b.tryTable(EMPTY,[[3,0,0]]);
    b.block(result);const returned=b.ctrl.at(-1);
    for(const n of def.wast){
      if(n instanceof W.WReturn)b.nodes.push(new W.WBr(returned));
      else if(n instanceof W.WBr&&n.target?.isFuncLabel)b.nodes.push(new W.WBr(returned));
      else if(n instanceof W.WBrIf&&n.target?.isFuncLabel)b.nodes.push(new W.WBrIf(returned));
      else if(n instanceof W.WBrTable)b.nodes.push(new W.WBrTable(n.targets.map(t=>t.isFuncLabel?returned:t),n.defaultTarget.isFuncLabel?returned:n.defaultTarget));
      else b.nodes.push(n);
    }
    b.end();depth(-1);if(inf.structuredCapture)ts(b,TOP,()=>b.localGet(inf.structuredTopLocal));b.nodes.push(new W.WBr(done));
    b.end();b.unreachable();b.end();depth(-1);if(inf.structuredCapture)ts(b,TOP,()=>b.localGet(inf.structuredTopLocal));b.localGet(sp);b.globalSet(cg.stackPointerGlobalIdx);b.throwRef();
    b.end();def.wast=b.nodes;W.validate(def.wast,null);
  }
  if(!needsFrames){finishModule();return;}
  // Continuation nodes live in the saving frame's growable tail. Re-arming
  // a buffer reuses sufficient capacity. Header: next, env, capacity, PC,
  // shadow SP, operand offset, operand bytes, owner, region count, padding.
  // Payload: operand prefix, then {destination, size, bytes (8-byte padded)}
  // for each live compiler temporary. Actual C variables are never restored.
  const setjmpType=dynamic?mod.addFunctionTypeId([I32],[I32]):setjmpId===undefined?null:typeId(setjmpId);
  const saveJumpIdx=structuredSave??(setjmpId===undefined&&!dynamic?null:add('__continuation_save',Array(7).fill(I32),[I32]));
  const restoreJumpIdx=jumpTag===undefined?null:add('__continuation_restore',[I32],[]);
  if(saveJumpIdx!==null){const b=new B(); // fp, env, offset, prefix bytes, pc, total bytes, regions; node, size, end
    const node=()=>b.localGet(7);
    fg(b,JUMPS);b.localSet(7);
    b.block();b.loop();node();b.aop(I32,A.OP_EQZ);b.brIf(1);
    ld(b,node,4);b.localGet(1);b.aop(I32,A.OP_EQ);
    ld(b,node,8);b.localGet(5);b.aop(I32,A.OP_GE,false);b.aop(I32,A.OP_AND);b.brIf(1);
    ld(b,node);b.localSet(7);b.br(0);b.end();b.end();
    node();b.aop(I32,A.OP_EQZ);b.if_(EMPTY);
    b.localGet(5);b.i32Const(55);b.aop(I32,A.OP_ADD);b.i32Const(-16);b.aop(I32,A.OP_AND);b.localSet(8);
    tg(b,TOP);b.localTee(7);b.localGet(8);b.aop(I32,A.OP_ADD);b.localTee(9);
    b.globalGet(cg.stackPointerGlobalIdx);b.aop(I32,A.OP_GE,false);b.if_(EMPTY);b.unreachable();b.end();
    st(b,node,0,()=>fg(b,JUMPS));st(b,node,4,()=>b.localGet(1));
    st(b,node,8,()=>b.localGet(5));st(b,node,28,()=>b.localGet(0));
    fs(b,JUMPS,node);fs(b,SIZE,()=>{b.localGet(9);b.localGet(0);b.aop(I32,A.OP_SUB);});ts(b,TOP,()=>b.localGet(9));b.end();
    st(b,node,12,()=>b.localGet(4));st(b,node,16,()=>b.globalGet(cg.stackPointerGlobalIdx));
    st(b,node,20,()=>b.localGet(2));st(b,node,24,()=>b.localGet(3));st(b,node,32,()=>b.localGet(6));
    node();b.i32Const(40);b.aop(I32,A.OP_ADD);b.localGet(0);b.localGet(2);b.aop(I32,A.OP_ADD);b.localGet(3);b.memoryCopy();
    st(b,()=>b.localGet(1),0,node);node();setDef(saveJumpIdx,b,[I32,I32,I32]);
  }
  if(restoreJumpIdx!==null){const b=new B(); // node; cursor, remaining, size
    b.localGet(0);b.i32Const(40);b.aop(I32,A.OP_ADD);fg(b,24);b.aop(I32,A.OP_ADD);b.localSet(1);
    fg(b,32);b.localSet(2);b.block();b.loop();b.localGet(2);b.aop(I32,A.OP_EQZ);b.brIf(1);
    ld(b,()=>b.localGet(1),4);b.localSet(3);
    ld(b,()=>b.localGet(1));b.localGet(1);b.i32Const(8);b.aop(I32,A.OP_ADD);b.localGet(3);b.memoryCopy();
    b.localGet(1);b.localGet(3);b.i32Const(15);b.aop(I32,A.OP_ADD);b.i32Const(-8);b.aop(I32,A.OP_AND);b.aop(I32,A.OP_ADD);b.localSet(1);
    b.localGet(2);b.i32Const(1);b.aop(I32,A.OP_SUB);b.localSet(2);b.br(0);b.end();b.end();setDef(restoreJumpIdx,b,[I32,I32,I32]);
  }
  const popIdx=add('__frame_pop',[I32],[]);
  {const b=new B();ts(b,CUR,()=>fg(b,PREV));ts(b,TOP,()=>b.localGet(0));setDef(popIdx,b,[]);}
  const allocIdx=add('__frame_push',[I32,I32,I32],[I32]);
  {const b=new B(); // id (table slot), parent, destination; locals fp,size
    if(dynamic){symbolic(b,'control',0);const load=new W.WRaw([0xfe,0x10,2,0]);load.signature={params:[I32],results:[I32]};b.nodes.push(load);symbolic(b,'generation',0);b.aop(I32,A.OP_NE);b.if_(EMPTY);b.call(dynamic.syncFunction);b.end();}
    b.localGet(0);b.i32Const(1);b.aop(I32,A.OP_LT,false);b.localGet(0);b.i32Const(dynamic?1048575:count);b.aop(I32,A.OP_GT,false);b.aop(I32,A.OP_OR);b.if_(EMPTY);b.unreachable();b.end();
    tg(b,TOP);b.localSet(3);descriptorAt(b,()=>b.localGet(0));b.mop(M.I32_LOAD,4,2);b.localSet(4);
    b.localGet(3);b.localGet(4);b.aop(I32,A.OP_ADD);b.globalGet(cg.stackPointerGlobalIdx);b.aop(I32,A.OP_GE,false);b.if_(EMPTY);b.unreachable();b.end();
    b.localGet(3);b.i32Const(0);b.localGet(4);b.memoryFill();
    const base=()=>b.localGet(3);
    st(b,base,PREV,()=>b.localGet(1));st(b,base,FUNC,()=>b.localGet(0));st(b,base,DEST,()=>b.localGet(2));st(b,base,SIZE,()=>b.localGet(4));st(b,base,SHADOW,()=>b.globalGet(cg.stackPointerGlobalIdx));
    ts(b,TOP,()=>{b.localGet(3);b.localGet(4);b.aop(I32,A.OP_ADD);});ts(b,CUR,base);increment(b,'frames');b.localGet(3);setDef(allocIdx,b,[I32,I32]);
  }
  const runIdx=add('__trampoline',[I32],[]);
  {const b=new B();b.block();b.loop();tg(b,CUR);b.localGet(0);b.aop(I32,A.OP_EQ);b.brIf(1);tg(b,BUDGET);b.aop(I32,A.OP_EQZ);b.if_(EMPTY);
      ts(b,BUDGET,()=>b.i32Const(65536));
      tg(b,DEPTH);b.i32Const(1);b.aop(I32,A.OP_EQ);tg(b,ROOT_ASYNC);b.aop(I32,A.OP_AND);b.if_(EMPTY);
      increment(b,'yields');ts(b,YIELDS,()=>{tg(b,YIELDS);b.i32Const(1);b.aop(I32,A.OP_ADD);});
      b.block();const done=b.ctrl.at(-1);
      for(let i=mod.tags.length-1;i>=0;i--){const ps=mod.typeDefs[mod.tags[i].typeIdx].params;b.block(ps.length===0?EMPTY:ps.length===1?ps[0]:{tag:'typeidx',idx:mod.addFunctionTypeId([],ps)});}
      if(mod.tags.length)b.tryTable(EMPTY,mod.tags.map((_,i)=>[0,i,i]));b.call(0);if(mod.tags.length)b.end();b.nodes.push(new W.WBr(done));
      for(let i=0;i<mod.tags.length;i++){b.end();pending(b,i,mod.typeDefs[mod.tags[i].typeIdx].params);b.nodes.push(new W.WBr(done));}b.end();
      b.end();b.end();
      tg(b,CUR);b.localTee(5);descriptorAt(b,()=>{b.localGet(5);b.mop(M.I32_LOAD,FUNC,2);});b.mop(M.I32_LOAD,0,2);b.callIndirect(stepType);b.br(0);b.end();b.end();setDef(runIdx,b,[I32,I64,F32,F64,I32,...(hasVectors?[I32,I32,V128]:[])]);}
  const scratch=[I32,I64,F32,F64]; // step locals: index 0 fp, 1..4 typed scratch, 5 newfp, 6 target, 7 condition
  function tmp(t){return ({127:1,126:2,125:3,124:4,123:8})[numeric(t).num];}
  function saveValue(b,off,t){b.localSet(tmp(t));fs(b,off,()=>b.localGet(tmp(t)),t);}
  function saveStack(b,inf,types){for(let i=types.length-1;i>=0;i--)saveValue(b,inf.stackOffset+i*inf.stackSlot,types[i]);}
  function restoreStack(b,inf,types){for(let i=0;i<types.length;i++)fg(b,inf.stackOffset+i*inf.stackSlot,types[i]);}
  function finish(b,inf){for(let i=inf.sig.results.length-1;i>=0;i--){const t=inf.sig.results[i];b.localSet(tmp(t));st(b,()=>fg(b,DEST),i*8,()=>b.localGet(tmp(t)),t);}b.localGet(0);b.call(popIdx);b.ret();}
  function pending(b,tag,types){for(let i=types.length-1;i>=0;i--){b.localSet(tmp(types[i]));st(b,absolute(b,PAYLOAD+paramOffset(types,i)),0,()=>b.localGet(tmp(types[i])),types[i]);}ts(b,TAG,()=>dynamic?tagID(b,tag):b.i32Const(tag+1));}
  function exceptionDispatch(b,inf){
    tg(b,TAG);b.if_(EMPTY);
    if(jumpTag!==undefined){
      tg(b,TAG);if(dynamic)tagID(b,jumpTag);else b.i32Const(jumpTag+1);b.aop(I32,A.OP_EQ);b.if_(EMPTY);
      ld(b,absolute(b,PAYLOAD));b.localSet(5);
      ld(b,()=>b.localGet(5),28);b.localGet(0);b.aop(I32,A.OP_EQ);b.if_(EMPTY);
      // Restore only the expression continuation, never roll back C locals.
      b.localGet(5);b.call(restoreJumpIdx);
      b.localGet(0);ld(b,()=>b.localGet(5),20);b.aop(I32,A.OP_ADD);b.localSet(6);
      b.localGet(6);b.localGet(5);b.i32Const(40);b.aop(I32,A.OP_ADD);ld(b,()=>b.localGet(5),24);b.memoryCopy();
      st(b,()=>{b.localGet(6);ld(b,()=>b.localGet(5),24);b.aop(I32,A.OP_ADD);},0,()=>ld(b,absolute(b,PAYLOAD+8)));
      fs(b,PC,()=>ld(b,()=>b.localGet(5),12));ld(b,()=>b.localGet(5),16);b.globalSet(cg.stackPointerGlobalIdx);
      ts(b,TAG,()=>b.i32Const(0));b.ret();b.end();b.end();
    }
    if(inf.blocks){
      // Group identical lexical handler lists instead of emitting one arm per basic block.
      const groups=new Map();for(const block of inf.blocks){const hs=inf.handlers[block.end-1]||[];if(!hs.length)continue;const key=hs.map(c=>`${c.kind}:${c.tagIdx}:${inf.target(c.target)}`).join(',');if(!groups.has(key))groups.set(key,{hs,ids:[]});groups.get(key).ids.push(block.id);}
      for(const {hs,ids} of groups.values()){
        ids.forEach((id,j)=>{fg(b,EH);b.i32Const(id);b.aop(I32,A.OP_EQ);if(j)b.aop(I32,A.OP_OR);});b.if_(EMPTY);
        for(const c of hs){
          if(c.kind!==0&&c.kind!==2)throw new Error('Reference exception catches are unsupported by the memory-frame ABI');
          if(c.kind===0){tg(b,TAG);if(dynamic)tagID(b,c.tagIdx);else b.i32Const(c.tagIdx+1);b.aop(I32,A.OP_EQ);b.if_(EMPTY);}
          const label=inf.control.get(c.target),types=c.kind===0?mod.typeDefs[mod.tags[c.tagIdx].typeIdx].params:[];
          for(let j=0;j<types.length;j++)fs(b,inf.stackOffset+(label.base.length+j)*inf.stackSlot,()=>ld(b,absolute(b,PAYLOAD+paramOffset(types,j)),0,types[j]),types[j]);
          fs(b,PC,()=>b.i32Const(inf.blockAt.get(inf.target(c.target)).id));ts(b,TAG,()=>b.i32Const(0));b.ret();
          if(c.kind===0)b.end();
        }b.end();
      }
    }
    fg(b,SHADOW);b.globalSet(cg.stackPointerGlobalIdx);b.localGet(0);b.call(popIdx);b.ret();b.end();
  }
  function bridgeImport(b,inf){
    // Convert typed Wasm exceptions from a host callback into explicit propagation.
    b.block();for(let i=mod.tags.length-1;i>=0;i--){const ps=mod.typeDefs[mod.tags[i].typeIdx].params;b.block(ps.length===0?EMPTY:ps.length===1?ps[0]:{tag:'typeidx',idx:mod.addFunctionTypeId([],ps)});}
    if(mod.tags.length)b.tryTable(EMPTY,mod.tags.map((_,i)=>[0,i,i]));
    inf.sig.params.forEach((t,i)=>fg(b,inf.localOffsets[i],t));b.call(inf.id);finish(b,inf);
    if(mod.tags.length){b.end();b.unreachable();}
    for(let i=0;i<mod.tags.length;i++){b.end();pending(b,i,mod.typeDefs[mod.tags[i].typeIdx].params);b.ret();}b.end();
  }
  for(const inf of infos){
    if(inf.step<0)continue;
    const b=new B();increment(b,'steps');exceptionDispatch(b,inf);
    if(inf.id===ffiId){
      // (target, argument count, eight-byte argument slots, result slot).
      // Schedule through the ordinary continuation stack, preserving yields,
      // cross-module calls and longjmp instead of re-entering Wasm from JS.
      fg(b,PC);b.if_(EMPTY);if(inf.sig.results.length)b.i32Const(0);finish(b,inf);b.end();
      fg(b,HEADER);b.localGet(0);fg(b,HEADER+24);b.call(allocIdx);b.localSet(5);
      // A valid CIF supplies every declared parameter. GObject additionally
      // permits callbacks to ignore trailing arguments. Do not mistake the
      // callee's local-slot capacity for a maximum permitted CIF arity, and
      // never copy ignored arguments past its allocation (native adapters can
      // now have no scratch locals at all).
      descriptorAt(b,()=>fg(b,HEADER));b.mop(M.I32_LOAD,12,2);b.localSet(6);
      fg(b,HEADER+8);b.localGet(6);b.aop(I32,A.OP_LT,false);b.if_(EMPTY);
      fg(b,HEADER+8);b.localSet(6);b.end();
      b.localGet(5);b.i32Const(HEADER);b.aop(I32,A.OP_ADD);
      fg(b,HEADER+16);b.localGet(6);b.i32Const(8);b.aop(I32,A.OP_MUL);b.memoryCopy();
      fs(b,PC,()=>b.i32Const(1));b.ret();
      setDef(inf.step,b,[...scratch,I32,I32,I32,...vectorScratch]);continue;
    }
    if(inf.id<nImp||inf.native){
      // Indirect callers cannot charge a statically unknown native target.
      // Charge at its step adapter instead, before executing its typed body.
      if(inf.native){
        const charge=analysis.functions[inf.id].cost;
        tg(b,BUDGET);b.i32Const(charge);b.aop(I32,A.OP_LT,false);b.if_(EMPTY);
        ts(b,BUDGET,()=>b.i32Const(0));b.ret();b.end();
        ts(b,BUDGET,()=>{tg(b,BUDGET);b.i32Const(charge);b.aop(I32,A.OP_SUB);});
      }
      bridgeImport(b,inf);setDef(inf.step,b,[...scratch,I32,I32,I32,...vectorScratch]);continue;
    }
    // Bound the browser optimizer's input size. All continuation state already
    // lives in the C frame, so a jump across groups can return to the step wrapper
    // without copying locals or adding a C frame.
    const groupSize=256;
    const groups=[];
    for(let start=0;start<inf.blocks.length;start+=groupSize)groups.push(inf.blocks.slice(start,start+groupSize));
    const grouped=groups.length>1;
    const bodies=grouped?groups.map((_,i)=>add('__step_'+(names.get(inf.id)||inf.id)+'_part'+i,[I32],[I32])):[inf.step];
    if(grouped){
      b.loop();const dispatch=b.ctrl.at(-1);b.block();const invalid=b.ctrl.at(-1),labels=[];
      for(let i=groups.length-1;i>=0;i--){b.block();labels[i]=b.ctrl.at(-1);}
      fg(b,PC);b.i32Const(groupSize);b.aop(I32,A.OP_DIV,false);b.nodes.push(new W.WBrTable(labels,invalid));
      for(const body of bodies){b.end();b.localGet(0);b.call(body);b.nodes.push(new W.WBrIf(dispatch));b.ret();}
      b.end();b.unreachable();b.end();setDef(inf.step,b,[...scratch,I32,I32,I32,...vectorScratch]);
    }
    const entry=b;
    for(let group=0;group<groups.length;group++){
    const blocks=groups[group],b=grouped?new B():entry;
    const setPC=id=>fs(b,PC,()=>b.i32Const(id));
    // Per-group loop/switch; a cross-group jump resumes in the wrapper.
    b.loop();const loop=b.ctrl.at(-1);b.block();const invalid=b.ctrl.at(-1);
    const labels=[];for(let j=blocks.length-1;j>=0;j--){b.block();labels[j]=b.ctrl.at(-1);}
    fg(b,PC);if(blocks[0].id){b.i32Const(blocks[0].id);b.aop(I32,A.OP_SUB);}b.nodes.push(new W.WBrTable(labels,invalid));
    const edge=(target,types)=>{
      saveStack(b,inf,types);
      if(target?.isFuncLabel){inf.sig.results.forEach((t,j)=>fg(b,inf.stackOffset+(types.length-inf.sig.results.length+j)*inf.stackSlot,t));finish(b,inf);return;}
      const at=typeof target==='number'?target:inf.target(target),entry=inf.before[at]||[];
      if(typeof target!=='number'){
        const c=inf.control.get(target),arity=target instanceof W.WLoop?0:c.results.length;
        for(let j=0;j<arity;j++){const t=entry[c.base.length+j];fs(b,inf.stackOffset+(c.base.length+j)*inf.stackSlot,()=>fg(b,inf.stackOffset+(types.length-arity+j)*inf.stackSlot,t),t);}
      }
      setPC(inf.blockAt.get(at).id);b.nodes.push(new W.WBr(loop));
    };
    const guarded=inf.handlers.some(h=>h.length);
    for(const block of blocks){
      b.end();
      // A signal checkpoint must lie inside the function's physical catch
      // region, not in the generated scaffolding between switch iterations.
      const safe=!guarded||(inf.handlers[block.start]?.length&&inf.handlers[block.end-1]?.length);
      // Native calls run uninterrupted. Charge an estimated cost at the
      // framed boundary; this is accounting, not a latency/work bound.
      let charge=1;
      for(let k=block.start;k<block.end;k++){const n=inf.nodes[k];if(n instanceof W.WCall&&infos[n.funcIdx].native)charge+=analysis.functions[n.funcIdx].cost;}
      charge=Math.min(charge,65536);
      tg(b,BUDGET);if(charge===1)b.aop(I32,A.OP_EQZ);else {b.i32Const(charge);b.aop(I32,A.OP_LT,false);}b.if_(EMPTY);
      if(charge!==1)ts(b,BUDGET,()=>b.i32Const(0));
      if(safe){fs(b,EH,()=>b.i32Const(block.id));b.ret();}
      b.else_();ts(b,BUDGET,()=>{tg(b,BUDGET);b.i32Const(charge);b.aop(I32,A.OP_SUB);});b.end();
      fs(b,EH,()=>b.i32Const(block.id));
      const initial=inf.before[block.start];if(initial===null){b.unreachable();continue;}restoreStack(b,inf,initial||[]);
      let terminated=false;
      if(block.region){
        // No capture or handler change occurs inside this region. JSPI keeps
        // its Wasm locals alive across suspension. Spill shared locals at every
        // normal, branch, return or typed-exception exit, before dispatching.
        const flushRegion=()=>{for(const id of block.region.spillLocals)fs(b,inf.localOffsets[id],()=>b.localGet(inf.regionLocalMap.get(id)),inf.locals[id]);};
        for(const id of block.region.entryLocals){fg(b,inf.localOffsets[id],inf.locals[id]);b.localSet(inf.regionLocalMap.get(id));}
        const catches=jspi&&mod.tags.length;
        if(catches||block.region.exits.length)saveStack(b,inf,initial||[]);
        const exits=new Map();
        for(const target of block.region.exits){
          const types=!target||target.isFuncLabel?inf.sig.results:target instanceof W.WLoop?[]:inf.control.get(target).results;
          b.block(types.length===0?EMPTY:types.length===1?types[0]:{tag:'typeidx',idx:mod.addFunctionTypeId([],types)});
          exits.set(target,{label:b.ctrl.at(-1),types});
        }
        if(catches){
          b.block();
          for(let i=mod.tags.length-1;i>=0;i--){const ps=mod.typeDefs[mod.tags[i].typeIdx].params;b.block(ps.length===0?EMPTY:ps.length===1?ps[0]:{tag:'typeidx',idx:mod.addFunctionTypeId([],ps)});}
          b.tryTable(EMPTY,mod.tags.map((_,i)=>[0,i,i]));
        }
        if(catches||exits.size)restoreStack(b,inf,initial||[]);
        for(let j=block.start;j<block.end;j++){
          const n=inf.nodes[j],local=inf.regionLocalMap.get(n.idx);
          if(n instanceof W.WLocalGet){if(local!==undefined)b.localGet(local);else fg(b,inf.localOffsets[n.idx],inf.locals[n.idx]);}
          else if(n instanceof W.WLocalSet||n instanceof W.WLocalTee){
            if(local!==undefined){if(n instanceof W.WLocalTee)b.localTee(local);else b.localSet(local);}
            else {const t=inf.locals[n.idx];saveValue(b,inf.localOffsets[n.idx],t);if(n instanceof W.WLocalTee)b.localGet(tmp(t));}
          }else if(n instanceof W.WGlobalSet&&n.idx===cg.stackPointerGlobalIdx&&!n.stackRestore){
            // Same allocation bounds as the framed path; restores are proven.
            b.localTee(1);tg(b,TOP);b.aop(I32,A.OP_LT,false);b.localGet(1);tg(b,LIMIT);b.aop(I32,A.OP_GT,false);b.aop(I32,A.OP_OR);b.if_(EMPTY);b.unreachable();b.end();b.localGet(1);b.nodes.push(n);
          }else if(n instanceof W.WReturn)b.nodes.push(new W.WBr(exits.get(null).label));
          else if(n instanceof W.WBr||n instanceof W.WBrIf)b.nodes.push(exits.has(n.target)?new n.constructor(exits.get(n.target).label):n);
          else if(n instanceof W.WBrTable)b.nodes.push(new W.WBrTable(n.targets.map(t=>exits.get(t)?.label||t),exits.get(n.defaultTarget)?.label||n.defaultTarget));
          else if(n instanceof W.WCallIndirect&&!engineImport(n)&&needsCallbackAdapter(n.typeIdx))b.call(callbackAdapter(n.typeIdx));
          else b.nodes.push(n);
        }
        flushRegion();
        if(catches){
          // All operations share the entry's lexical handler. Normal exit
          // spills only once; longjmp/typed exceptions return to that handler.
          edge(block.end,inf.before[block.end]||[]);
          b.end();b.unreachable();
          for(let i=0;i<mod.tags.length;i++){b.end();flushRegion();pending(b,i,mod.typeDefs[mod.tags[i].typeIdx].params);b.ret();}
          b.end();b.unreachable();terminated=true;
        }else if(exits.size){edge(block.end,inf.before[block.end]||[]);terminated=true;}
        for(const [target,{types}] of [...exits].reverse()){
          b.end();flushRegion();
          if(!target)finish(b,inf);else edge(target,types);
        }
      }else for(let j=block.start;j<block.end;j++){
        const n=inf.nodes[j],before=inf.before[j];if(before===null)continue;
        if(n instanceof W.WLocalGet)fg(b,inf.localOffsets[n.idx],inf.locals[n.idx]);
        else if(n instanceof W.WLocalSet||n instanceof W.WLocalTee){const t=inf.locals[n.idx];saveValue(b,inf.localOffsets[n.idx],t);if(n instanceof W.WLocalTee)b.localGet(tmp(t));}
        else if(n instanceof W.WBlock||n instanceof W.WLoop||n instanceof W.WTryTable||n instanceof W.WEnd){}
        else if(n instanceof W.WIf){b.localSet(7);const types=before.slice(0,-1);saveStack(b,inf,types);b.localGet(7);b.if_(EMPTY);setPC(inf.blockAt.get(j+1).id);b.else_();const c=inf.control.get(n);setPC(inf.blockAt.get(c.elseAt===null?c.end+1:c.elseAt+1).id);b.end();b.nodes.push(new W.WBr(loop));terminated=true;break;}
        else if(n instanceof W.WElse){const c=[...inf.control.values()].find(c=>c.elseAt===j);edge(c.end+1,before);terminated=true;break;}
        else if(n instanceof W.WBr){edge(n.target,before);terminated=true;break;}
        else if(n instanceof W.WBrIf){b.localSet(7);const types=before.slice(0,-1);saveStack(b,inf,types);b.localGet(7);b.if_(EMPTY);restoreStack(b,inf,types);edge(n.target,types);b.end();restoreStack(b,inf,types);edge(j+1,types);terminated=true;break;}
        else if(n instanceof W.WBrTable){
          b.localSet(7);const types=before.slice(0,-1);saveStack(b,inf,types);
          const unique=[...new Set([...n.targets,n.defaultTarget])],destLabels=new Map();b.block();const bad=b.ctrl.at(-1);for(let k=unique.length-1;k>=0;k--){b.block();destLabels.set(unique[k],b.ctrl.at(-1));}b.localGet(7);b.nodes.push(new W.WBrTable(n.targets.map(t=>destLabels.get(t)),destLabels.get(n.defaultTarget)));
          for(const target of unique){b.end();restoreStack(b,inf,types);edge(target,types);}b.end();b.unreachable();terminated=true;break;
        }else if(n instanceof W.WCall||n instanceof W.WCallIndirect){
          const direct=n instanceof W.WCall,s=direct?sig(n.funcIdx):mod.typeDefs[n.typeIdx];
          if((direct&&engineDirect(n))||(!direct&&engineCall(n))){
            const callNode=!direct&&!engineImport(n)&&needsCallbackAdapter(n.typeIdx)?new W.WCall(callbackAdapter(n.typeIdx)):n;
            const throws=jspi&&(direct?(!infos[n.funcIdx].native||infos[n.funcIdx].engine):!cg.dynamic?.nativeImports.has(n.nativeCall));
            if(!throws||!mod.tags.length){b.nodes.push(callNode);continue;}
            // Catch at this local frame boundary so its lexical handler or
            // setjmp owner receives exceptions unwound through engine calls.
            saveStack(b,inf,before);
            b.block();const done=b.ctrl.at(-1);
            for(let i=mod.tags.length-1;i>=0;i--){const ps=mod.typeDefs[mod.tags[i].typeIdx].params;b.block(ps.length===0?EMPTY:ps.length===1?ps[0]:{tag:'typeidx',idx:mod.addFunctionTypeId([],ps)});}
            b.tryTable(EMPTY,mod.tags.map((_,i)=>[0,i,i]));
            restoreStack(b,inf,before);b.nodes.push(callNode);saveStack(b,inf,inf.after[j]);b.nodes.push(new W.WBr(done));b.end();b.unreachable();
            for(let i=0;i<mod.tags.length;i++){b.end();pending(b,i,mod.typeDefs[mod.tags[i].typeIdx].params);b.ret();}b.end();
            restoreStack(b,inf,inf.after[j]);continue;
          }
          if(!direct)b.localSet(6);saveStack(b,inf,direct?before:before.slice(0,-1));
          const prefix=before.length-s.params.length-(direct?0:1);
          setPC(inf.blockAt.get(j+1).id);
          if((setjmpId!==undefined||dynamic)&&(direct?n.funcIdx===setjmpId:n.typeIdx===setjmpType)){
            if(!direct){b.localGet(6);if(setjmpId!==undefined)functionID(b,setjmpId);else symbolic(b,'setjmp',0);b.aop(I32,A.OP_EQ);b.localGet(6);b.aop(I32,A.OP_EQZ);b.aop(I32,A.OP_EQZ);b.aop(I32,A.OP_AND);b.if_(EMPTY);}
            const regions=n.continuationRegions||[],bytes=prefix*inf.stackSlot+regions.reduce((sum,r)=>sum+8+((r.size+7)&~7),0);
            b.localGet(0);fg(b,inf.stackOffset+prefix*inf.stackSlot);b.i32Const(inf.stackOffset);b.i32Const(prefix*inf.stackSlot);b.i32Const(inf.blockAt.get(j+1).id);b.i32Const(bytes);b.i32Const(regions.length);b.call(saveJumpIdx);b.localSet(5);
            let offset=40+prefix*inf.stackSlot;
            for(const region of regions){
              const address=()=>{if(region.slot!==undefined){b.localGet(0);b.i32Const(inf.localOffsets[region.slot]);}else {fg(b,inf.localOffsets[region.local]);b.i32Const(region.offset);}b.aop(I32,A.OP_ADD);};
              st(b,()=>b.localGet(5),offset,address);st(b,()=>b.localGet(5),offset+4,()=>b.i32Const(region.size));
              b.localGet(5);b.i32Const(offset+8);b.aop(I32,A.OP_ADD);address();b.i32Const(region.size);b.memoryCopy();offset+=8+((region.size+7)&~7);
            }
            fs(b,inf.stackOffset+prefix*inf.stackSlot,()=>b.i32Const(0));b.ret();
            if(!direct)b.end();else {terminated=true;break;}
          }
          if(direct)functionID(b,n.funcIdx);else {
            b.localGet(6);b.i32Const(1);b.aop(I32,A.OP_LT,false);b.localGet(6);b.i32Const(dynamic?1048575:count);b.aop(I32,A.OP_GT,false);b.aop(I32,A.OP_OR);b.if_(EMPTY);b.unreachable();b.end();
            // C callback libraries commonly pass unused trailing arguments.
            // Accept only a matching result and parameter prefix. Never write
            // discarded arguments into the callee's (possibly smaller) frame.
            descriptorAt(b,()=>b.localGet(6));b.mop(M.I32_LOAD,8,2);b.localSet(1);
            b.i32Const(s.params.length);b.localSet(7);
            b.localGet(1);signatureID(b,n.typeIdx);b.aop(I32,A.OP_NE);b.if_(EMPTY);
            b.i32Const(-1);b.localSet(7);
            for(let k=0;k<s.params.length;k++){
              b.localGet(1);signatureID(b,mod.addFunctionTypeId(s.params.slice(0,k),s.results));b.aop(I32,A.OP_EQ);b.if_(EMPTY);b.i32Const(k);b.localSet(7);b.end();
            }
            b.localGet(7);b.i32Const(0);b.aop(I32,A.OP_LT,true);b.if_(EMPTY);b.unreachable();b.end();b.end();b.localGet(6);
          }
          b.localGet(0);b.localGet(0);b.i32Const(inf.stackOffset+prefix*inf.stackSlot);b.aop(I32,A.OP_ADD);b.call(allocIdx);b.localSet(5);
          s.params.forEach((t,k)=>{
            if(!direct){b.localGet(7);b.i32Const(k);b.aop(I32,A.OP_GT,true);b.if_(EMPTY);}
            st(b,()=>b.localGet(5),HEADER+paramOffset(s.params,k),()=>fg(b,inf.stackOffset+(prefix+k)*inf.stackSlot,t),t);
            if(!direct)b.end();
          });b.ret();terminated=true;break;
        }else if(n instanceof W.WReturn){finish(b,inf);terminated=true;break;}
        else if(n instanceof W.WThrow){pending(b,n.tagIdx,mod.typeDefs[mod.tags[n.tagIdx].typeIdx].params);b.ret();terminated=true;break;}
        else if(n instanceof W.WGlobalSet&&n.idx===cg.stackPointerGlobalIdx&&!n.stackRestore){
          b.localTee(1);tg(b,TOP);b.aop(I32,A.OP_LT,false);b.localGet(1);tg(b,LIMIT);b.aop(I32,A.OP_GT,false);b.aop(I32,A.OP_OR);b.if_(EMPTY);b.unreachable();b.end();b.localGet(1);b.nodes.push(n);
        }else {b.nodes.push(n);if(n instanceof W.WUnreachable||(n instanceof W.WRaw&&n.bytes[0]===0)){terminated=true;break;}}
      }
      if(!terminated){if(block.end===inf.nodes.length)finish(b,inf);else edge(block.end,inf.before[block.end]||[]);}
    }
    b.end();
    if(grouped){
      // Return one for a cross-group jump, zero for a real step exit. Keeping
      // transfers in the wrapper avoids adding a signal checkpoint outside a
      // guarded exception scope. Invalid PCs still trap.
      fg(b,PC);b.i32Const(inf.blocks.length);b.aop(I32,A.OP_GE,false);b.if_(EMPTY);b.unreachable();b.end();b.ret();
      const transfer=b.nodes.at(-1);
      b.nodes=b.nodes.flatMap(n=>n instanceof W.WReturn?[new W.WConst(I32,n===transfer?1:0),n]:[n]);
    }else b.unreachable();
    b.end();if(grouped)b.unreachable();setDef(bodies[group],b,[...scratch,I32,I32,I32,...vectorScratch,...inf.regionLocals.map(id=>inf.locals[id])]);
    }
  }
  // Native definitions keep their typed bodies. Their step adapters above keep
  // indirect calls and table identities compatible with framed definitions.
  for(const inf of infos){if(inf.id<nImp||inf.native)continue;
    const b=new B(),fp=inf.sig.params.length,parent=fp+1,sp=fp+2,top=fp+3,depth=fp+4,tag=fp+5;
    tg(b,TOP);b.if_(EMPTY);b.else_();ts(b,TOP,()=>{b.globalGet(TS);b.i32Const(3072);b.aop(I32,A.OP_ADD);});b.end();
    tg(b,LIMIT);b.if_(EMPTY);b.else_();ts(b,LIMIT,()=>b.i32Const(cg.stackPages*65536));b.end();
    // Host promise rejection can unwind out of the dispatcher without its
    // ordinary frame-pop path. Restore the entire entry boundary on unwind.
    b.globalGet(cg.stackPointerGlobalIdx);b.localSet(sp);tg(b,TOP);b.localSet(top);
    tg(b,DEPTH);b.localSet(depth);tg(b,TAG);b.localSet(tag);tg(b,CUR);b.localSet(parent);
    b.block(inf.sig.results[0]||EMPTY);const done=b.ctrl.at(-1);
    b.block(W.WT_EXNREF);b.tryTable(EMPTY,[[3,0,0]]);
    ts(b,DEPTH,()=>{tg(b,DEPTH);b.i32Const(1);b.aop(I32,A.OP_ADD);});
    b.localGet(parent);b.aop(I32,A.OP_EQZ);b.if_(EMPTY);
    ts(b,ROOT_ASYNC,()=>b.i32Const(mod.exports.some(e=>e.kind===0&&(e.name==='main'||e.name==='__thread_main')&&e.index===inf.id)?1:0));b.end();functionID(b,inf.id);b.localGet(parent);b.i32Const(0);b.call(allocIdx);b.localSet(fp);
    st(b,()=>b.localGet(fp),DEST,()=>{b.localGet(fp);b.i32Const(RESULT);b.aop(I32,A.OP_ADD);});
    inf.sig.params.forEach((t,i)=>st(b,()=>b.localGet(fp),inf.localOffsets[i],()=>b.localGet(i),t));
    b.localGet(parent);b.call(runIdx);
    ts(b,DEPTH,()=>{tg(b,DEPTH);b.i32Const(1);b.aop(I32,A.OP_SUB);});
    for(let i=0;i<mod.tags.length;i++){tg(b,TAG);if(dynamic)tagID(b,i);else b.i32Const(i+1);b.aop(I32,A.OP_EQ);b.if_(EMPTY);ts(b,TAG,()=>b.i32Const(0));mod.typeDefs[mod.tags[i].typeIdx].params.forEach((t,j)=>ld(b,absolute(b,PAYLOAD+paramOffset(mod.typeDefs[mod.tags[i].typeIdx].params,j)),0,t));b.throw_(i);b.end();}
    inf.sig.results.forEach((t,i)=>ld(b,()=>b.localGet(fp),RESULT+i*8,t));b.nodes.push(new W.WBr(done));
    b.end();b.unreachable();b.end();
    b.localGet(sp);b.globalSet(cg.stackPointerGlobalIdx);
    ts(b,CUR,()=>b.localGet(parent));ts(b,TOP,()=>b.localGet(top));
    ts(b,DEPTH,()=>b.localGet(depth));ts(b,TAG,()=>b.localGet(tag));
    b.throwRef();b.end();setDef(inf.id,b,[I32,I32,I32,I32,I32,I32]);
  }
  // Indirect step targets must survive the ordinary Wasm tree shaker.
  for(let i=0;i<mod.funcDefs.length;i++)mod.addrTakenFuncs.add(nImp+i);
  mod.localNames=[];
  mod.addExport('__trampoline',0,runIdx);
  // A fork child resumes after the snapshotted import, without re-entering main.
  if(cg.enableFork){
  const resumeIdx=add('__fork_resume',[I32],[I32]);
  {const b=new B();b.i32Const(0);b.call(runIdx);ts(b,DEPTH,()=>b.i32Const(0));
    for(let i=0;i<mod.tags.length;i++){tg(b,TAG);if(dynamic)tagID(b,i);else b.i32Const(i+1);b.aop(I32,A.OP_EQ);b.if_(EMPTY);ts(b,TAG,()=>b.i32Const(0));mod.typeDefs[mod.tags[i].typeIdx].params.forEach((t,j)=>ld(b,absolute(b,PAYLOAD+paramOffset(mod.typeDefs[mod.tags[i].typeIdx].params,j)),0,t));b.throw_(i);b.end();}
    fg(b,RESULT);setDef(resumeIdx,b,[]);mod.addExport('__fork_resume',0,resumeIdx);}
  }
  finishModule();
  function finishModule(){
  // C objects are in memory, but stack/heap cursors and exception identities
  // may still be numeric Wasm globals. Include every mutable runtime global.
  if(cg.enableFork)[...mod.globalImports,...mod.globals].forEach((g,i)=>{if(g.isMutable){numeric(g.type);mod.addExport('__fork_global_'+i,3,i);}});
  mod.addExport('__thread_state',3,TS);mod.addExport('__stack_pointer',3,cg.stackPointerGlobalIdx);
  options.onTrampoline?.({ssa:ssaStats,structuredOptimization:structuredStats,structuredCaptures:structuredCaptures.map(f=>names.get(f.id)||String(f.id)),needsFrames,stackChecks,elidedStackChecks,exceptionCleanups,elidedExceptionCleanups,elidedAdapters,mode:jspi?'jspi':'continuations',cleanRegions:infos.reduce((n,f)=>n+(f.regions?.length||0),0),regionFunctions:infos.filter(f=>f.regions?.length).map(f=>({name:names.get(f.id),regions:f.regions.length,privateLocals:f.regions.reduce((n,r)=>n+r.privateLocals.length,0),registerLocals:f.regionLocals.length,blocks:f.ir.blocks.length,resumeBlocks:f.blocks.length})),inline:mod.preContinuationInline,elidedFunctions,instrumented:!!options.compilerOptions?.instrumentDispatch,functions:originals.length,nativeFunctions:infos.filter(f=>f.id>=nImp&&f.native&&!originals[f.id-nImp]?.elided).length,nativeNames:infos.filter(f=>f.id>=nImp&&f.native&&!originals[f.id-nImp]?.elided).map(f=>names.get(f.id)||String(f.id)),functionEffects:analysis.functions.map((f,id)=>({...f,execution:execution[id],name:names.get(id)||String(id),native:infos[id].native&&!originals[id-nImp]?.elided,elided:!!originals[id-nImp]?.elided})),imports:nImp,frameBytes:needsFrames?infos.reduce((n,f)=>n+f.size,0):0,threadAddress:T});
  }
}};
