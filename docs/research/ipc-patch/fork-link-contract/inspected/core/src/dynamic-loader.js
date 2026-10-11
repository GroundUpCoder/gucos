// Resident PIC modules. C-to-C transfers use the shared continuation registry;
// JavaScript participates only in installation and symbol lookup.
// Shared control words: generation, catalog head, next table slot, lock,
// worker-list head, next global-scope ordinal (all u32, in that order).
// Catalog header (32 bytes): next, Wasm length, data base, table base,
// global ordinal (0 = local), path length, binding JSON length, reserved;
// followed by Wasm bytes, UTF-8 path and immutable symbol bindings.
// Worker (16 bytes): next, thread record, TLS-map head, active.
// TLS map (32 bytes): module handle, aligned base, allocation, next,
// map allocation, then reserved words. Only loader operations allocate;
// call-boundary installation reads these records without malloc or waits.
self.DynamicLoader = (() => {
  const SECTION = "wasmposix.dylink",
    PAGE = 65536,
    CHUNK = 16384;
  // Cache text for cheap worker transport and parsed metadata within each
  // worker. Structured-cloning the large object graph costs more than parsing.
  const metadataCache = new WeakMap(), metadataTexts = new WeakMap();
  const metadataText = module => {
    if(metadataTexts.has(module))return metadataTexts.get(module);
    const sections=WebAssembly.Module.customSections(module,SECTION);
    if(sections.length>1)throw new Error("Duplicate dynamic metadata");
    const text=sections.length?new TextDecoder().decode(sections[0]):null;
    metadataTexts.set(module,text);return text;
  };
  const metadata = (module, supplied = null) => {
    if (metadataCache.has(module)) return metadataCache.get(module);
    const text=supplied ?? metadataText(module);
    const m=text===null?null:JSON.parse(text);
    if (m && ![1, 2, 3, 4].includes(m.version)) throw new Error("Unsupported dynamic ABI");
    metadataTexts.set(module,text);metadataCache.set(module,m);
    return m;
  };
  // Address-free validation plans can be reused with immutable metadata.
  const validationPlans = new WeakMap();
  const validationPlan = meta => {
    let plan=validationPlans.get(meta);
    if(!plan){
      plan={descriptors:new Map(meta.descriptors.map(d=>[d.slot,d])),
        native:new Set(meta.nativeImports||[]),engine:new Set(meta.engineImports||[])};
      validationPlans.set(meta,plan);
    }
    return plan;
  };
  const identity = (s) => {
    let h = 2166136261;
    for (let i = 0; i < s.length; i++)
      h = Math.imul(h ^ s.charCodeAt(i), 16777619);
    return h >>> 0 || 1;
  };
  const align = (n, a = 16) => Math.ceil(n / a) * a;
  class Loader {
    constructor(
      module,
      imports,
      {
        state,
        thread,
        tls,
        worker,
        allocate,
        readLibrary,
        path,
        compiledModules,
      } = {},
    ) {
      this.imports = imports;
      this.readLibrary = readLibrary;
      this.restored = !!state;
      this.memory = imports.c.memory;
      this.table = new WebAssembly.Table({ element: "anyfunc", initial: 1 });
      this.stack = new WebAssembly.Global(
        { value: "i32", mutable: true },
        thread ? thread.record + thread.size : metadata(module).stack,
      );
      this.thread = new WebAssembly.Global(
        { value: "i32", mutable: true },
        thread ? thread.record : 1024,
      );
      this.generation = new WebAssembly.Global(
        { value: "i32", mutable: true },
        0,
      );
      this.modules = [];
      this.globals = new Map();
      this.signatures = new Map();
      this.tags = new Map();
      this.exceptionTags = new Map();
      this.tlsSaved = tls || {};
      this.allocateOverride = allocate;
      this.compiledModules = new Map(compiledModules || []);
      for(const [,module,meta] of compiledModules || [])if(meta)metadata(module,meta);
      this.allocations = new Map();
      this.retained = new Set();
      const meta = metadata(module);
      this.bootstrap = !!meta?.sharedLibc && !state;
      if (!meta || meta.shared)
        throw new Error("Expected a dynamic executable");
      this.canClone = meta.version >= 4 && meta.execution?.clone === true;
      this.checkExecution(meta, path || '<executable>');
      let cursor = align(meta.stack + meta.memorySize);
      const take = (n) => {
        const p = cursor;
        cursor = align(cursor + n);
        return p;
      };
      this.state = state || {
        memoryBase: meta.stack,
        tableBase: 1,
        registry: take(4096),
        control: take(32),
      };
      const chunks = new Map();
      if (!state)
        for (let i = 0; i <= meta.tableSize >>> 10; i++)
          chunks.set(i, take(CHUNK));
      if (!state) this.state.heap = cursor;
      if (this.bootstrap) this.state.bootstrapStart = cursor;
      this.ensureMemory(this.state.heap);
      if (!state) {
        new Uint8Array(
          this.memory.buffer,
          this.state.registry,
          this.state.heap - this.state.registry,
        ).fill(0);
        const v = this.view();
        for (const [i, p] of chunks)
          v.setUint32(this.state.registry + i * 4, p, true);
        v.setUint32(this.state.control + 8, meta.tableSize + 1, true);
      }
      const rec = {
        module,
        meta,
        memoryBase: this.state.memoryBase,
        tableBase: 1,
        tlsBase: thread ? this.view().getUint32(thread.record + 2104, true) : 0,
        handle: 1,
        path: path || "<main>",
        deferred: !!meta.deferredMain,
        bindingGlobals: new Map(),
        global: true,
        globalOrder: 0,
      };
      this.main = rec;
      this.registerSymbols(rec);
      this.intern(rec);
      this.instance = this.instantiate(rec);
      this.processInstance = {
        exports: new Proxy(
          {},
          {
            get: (_, name) => {
              const direct = rec.instance.exports[name] ?? this.runtime?.instance.exports[name];
              if (direct !== undefined) return direct;
              // Host event callbacks can live in optional shared runtimes
              // (SDL, for example), not just the executable or libc.
              // Python extensions use RTLD_LOCAL, so their SDL dependency is
              // absent from globals. Host input callbacks still need to reach
              // that runtime without exposing its symbols to guest dlsym.
              const symbol = this.globals.get(name) ||
                (typeof name === "string" && name.startsWith("__sdl_")
                  ? this.modules.map(r => r.symbols.get(name)).find(Boolean)
                  : undefined);
              return symbol?.kind === "function"
                ? symbol.owner.instance?.exports["__dl_fn_" + symbol.value]
                : undefined;
            },
            ownKeys: () => Reflect.ownKeys(rec.instance.exports),
            getOwnPropertyDescriptor: (_, name) =>
              Object.getOwnPropertyDescriptor(rec.instance.exports, name)
                ? { enumerable: true, configurable: true }
                : undefined,
          },
        ),
      };
      this.modules.push(rec);
      if (!state) {
        this.instance.exports.__dylink_init();
        // Native startup can enter a DSO before any continuation frame exists.
        // Every module must use the process stack limit, not its link-time size.
        this.view().setUint32(this.thread.value + 28, this.stack.value, true);
        this.relocate(rec);
        this.installDescriptors(rec);
      }
      // Main TLS is private even for the first thread; allocate after malloc exists.
      this.initialized = false;
      this.worker = worker || thread?.dynamicWorker || 0;
    }
    view() {
      return new DataView(this.memory.buffer);
    }
    ensureMemory(end) {
      if (end > this.memory.buffer.byteLength)
        this.memory.grow(
          Math.ceil((end - this.memory.buffer.byteLength) / PAGE),
        );
    }
    intern(rec) {
      rec.sharedExceptionTags = WebAssembly.Module.imports(rec.module).some(
        im => im.module === 'dl.tag' && im.kind === 'tag',
      );
      if (rec.meta.tags.length && this.modules.some(r => r.meta.tags.length &&
          r.sharedExceptionTags !== rec.sharedExceptionTags)) {
        throw new Error('Dynamic exception ABI mismatch; rebuild the executable and its shared libraries');
      }
      rec.types = rec.meta.signatures.map((s) => {
        const k = JSON.stringify(s);
        if (!this.signatures.has(k)) {
          const id = identity(k);
          if ([...this.signatures].some(([key, v]) => v === id && key !== k))
            throw new Error("Dynamic signature identity collision");
          this.signatures.set(k, id);
        }
        return this.signatures.get(k);
      });
      rec.tagIds = [];
      for (const t of rec.meta.tags) {
        if (!this.tags.has(t.name)) {
          const id = identity(t.name);
          if ([...this.tags].some(([name, v]) => v === id && name !== t.name))
            throw new Error("Dynamic exception identity collision");
          this.tags.set(t.name, id);
        }
        rec.tagIds[t.index] = this.tags.get(t.name);
      }
    }
    registerSymbols(rec) {
      rec.symbols = new Map(
        rec.meta.symbols.map((s) => [s.name, { ...s, owner: rec }]),
      );
      if (rec.global)
        for (const [n, s] of rec.symbols)
          if (!this.globals.has(n)) this.globals.set(n, s);
    }
    rebuildGlobals() {
      this.globals = new Map(this.main.symbols);
      for (const rec of this.modules
        .filter((r) => r !== this.main && r.global)
        .sort((a, b) => a.globalOrder - b.globalOrder))
        for (const [name, s] of rec.symbols)
          if (!this.globals.has(name)) this.globals.set(name, s);
    }
    address(s) {
      return s.kind === "function"
        ? s.owner.tableBase + s.value - 1
        : s.kind === "tls"
          ? s.owner.tlsBase + s.value
          : s.owner.memoryBase + s.value;
    }
    resolve(name, rec, kind) {
      const s =
        this.globals.get(name) ||
        (this.bindingIndex ? this.bindingIndex.get(name) :
          (this.bindingScope || this.scope(rec)).map((r) => r.symbols.get(name)).find(Boolean));
      if (!s) throw new Error("Undefined dynamic symbol: " + name);
      if (kind && s.kind !== kind)
        throw new Error("Dynamic symbol kind mismatch: " + name);
      rec.bindingTargets?.set(name, s);
      return s;
    }
    scope(rec) {
      const out = [],
        queue = [rec],
        seen = new Set();
      while (queue.length) {
        const r = queue.shift();
        if (seen.has(r)) continue;
        seen.add(r);
        out.push(r);
        for (const name of r.meta.needed || []) {
          const dep = (this.pending || [])
            .concat(this.modules)
            .find(
              (d) =>
                d.meta.soname === name ||
                d.path === name ||
                d.path.split("/").pop() === name,
            );
          if (dep) queue.push(dep);
        }
      }
      return out;
    }
    async read(name, requester = this.main) {
      const old = this.modules.concat(this.pending || []).find(
        (r) =>
          r.path === name ||
          r.meta.soname === name ||
          (!name.includes("/") && r.path.split("/").pop() === name),
      );
      if (old) return { record: old };
      if (!this.readLibrary) throw new Error("No library reader for " + name);
      const found = await this.readLibrary(
        name,
        requester.path,
        requester.meta.runpath || [],
      );
      if (!found)
        throw new Error(
          "Cannot open shared library: " +
            name +
            " (needed by " +
            requester.path +
            ")",
        );
      if(found.module && found.metadata)metadata(found.module,found.metadata);
      return found.bytes ? found : { bytes: found, path: name };
    }
    bindMain() {
      const rec = this.main;
      if (!rec.meta.deferredMain) return;
      rec.deferred = false;
      for (const { request, global } of rec.bindingGlobals.values())
        global.value = this.value(request, rec);
      this.validateBindings(rec);
      if (!this.restored) {
        // Internal relocations already ran before libc/environment setup. Replaying
        // them would overwrite live pointers such as environ and allocator state.
        for (const r of rec.meta.relocations)
          if (
            r.kind.startsWith("external") ||
            (r.kind === "function" && rec.meta.externalFunctions?.[r.value - 1])
          ) {
            const value = (this.value(r, rec) + (r.offset || 0)) >>> 0;
            this.view().setUint32(rec.memoryBase + r.at, value, true);
            const range = rec.meta.tlsRanges.find(
              (t) => r.at >= t.at && r.at + 4 <= t.at + t.size,
            );
            if (range)
              this.view().setUint32(
                rec.tlsBase + range.offset + r.at - range.at,
                value,
                true,
              );
          }
      }
    }
    validateBindings(rec) {
      const plan=validationPlan(rec.meta);
      for (const r of rec.meta.imports)
        if (
          ["externalFunction", "external", "externalTLS", "setjmp"].includes(
            r.kind,
          )
        )
          this.value(r, rec);
      for (const r of rec.meta.relocations) this.value(r, rec);
      for (const [idx, name] of Object.entries(rec.meta.externalFunctions)) {
        const target = this.resolve(name, rec, "function");
        if((rec.meta.noUnwindABIs?.[name]||'') !== (target.noUnwindABI||''))throw new Error('__nothrow type mismatch: '+name);
        if((rec.meta.memoryABIs?.[name]||'') !== (target.memoryABI||''))throw new Error('Wasm pointer memory ABI mismatch: '+name);
        const type = plan.descriptors.get(Number(idx) + 1).type;
        if (rec.types[type] !== target.owner.types[target.type])
          throw new Error("Dynamic function signature mismatch: " + name);
        if (plan.native.has(name) && target.execution !== 'native-v1')
          throw new Error('Native execution contract mismatch: '+name+'; relink the consumer against this library');
        if (plan.engine.has(name) && !['native-v1','jspi-v1'].includes(target.execution || 'jspi-v1'))
          throw new Error('JSPI execution contract mismatch: '+name+'; relink the consumer against this library');
      }
    }
    value(r, rec) {

      if (
        rec.deferred &&
        (r.kind.startsWith("external") ||
          (r.kind === "function" && rec.meta.externalFunctions?.[r.value - 1]))
      )
        return 0;
      const externalName =
        r.kind === "externalFunction"
          ? r.nameSymbol
          : r.kind === "external" || r.kind === "externalTLS"
            ? r.name
            : r.kind === "function"
              ? rec.meta.externalFunctions?.[r.value - 1]
              : null;
      if (externalName && rec.bindingValues) {
        const b = rec.bindingValues.get(externalName);
        if (!b)
          throw new Error("Missing saved dynamic binding: " + externalName);
        return b.tls
          ? (b.tls === 1
              ? this.main.tlsBase
              : this.tlsFor(this.worker, b.tls)) + b.offset
          : b.value;
      }
      // Saved worker bindings were validated before publication. Their
      // providers may not have been instantiated yet during reconstruction.
      if(['externalFunction','external','externalTLS'].includes(r.kind)){
        const name=r.nameSymbol||r.name, kind=r.kind==='externalFunction'?'function':r.kind==='externalTLS'?'tls':'data';
        const provider=this.resolve(name,rec,kind);
        if((rec.meta.noUnwindABIs?.[name]||'')!==(provider.noUnwindABI||''))throw new Error('__nothrow type mismatch: '+name);
        if((rec.meta.memoryABIs?.[name]||'')!==(provider.memoryABI||''))throw new Error('Wasm pointer memory ABI mismatch: '+name);
      }
      switch (r.kind) {
        case "memoryBase":
          return rec.memoryBase;
        case "tableBase":
          return rec.tableBase;
        case "stack":
          return this.stack;
        case "thread":
          return this.thread;
        case "tlsBase":
          return rec.tlsGlobal;
        case "heap":
          return this.state.heap;
        case "registry":
          return this.state.registry;
        case "generation":
          return this.generation;
        case "control":
          return this.state.control;
        case "setjmp": {
          const saved = rec.bindingValues?.get("setjmp");
          if (saved) return saved.value;
          const sym =
            this.globals.get("setjmp") ||
            (this.bindingScope || [])
              .map((r) => r.symbols?.get("setjmp"))
              .find(Boolean);
          if (sym) rec.bindingTargets?.set("setjmp", sym);
          return sym ? this.address(sym) : 0;
        }
        case "signature":
          return rec.types[r.value];
        case "tag":
          return rec.tagIds[r.value];
        case "externalFunction":
          return this.address(this.resolve(r.nameSymbol, rec, "function"));
        case "external":
          return this.address(this.resolve(r.name, rec, "data"));
        case "externalTLS":
          return this.address(this.resolve(r.name, rec, "tls"));
        case "function": {
          const imp = rec.meta.externalFunctions?.[r.value - 1];
          return imp
            ? this.address(this.resolve(imp, rec, "function"))
            : rec.tableBase + r.value - 1;
        }
        case "data":
          return rec.memoryBase + r.value - rec.meta.stack;
        default:
          throw new Error("Unknown dynamic relocation " + r.kind);
      }
    }
    instantiate(rec) {
      if (this.table.length < rec.tableBase + rec.meta.tableSize)
        this.table.grow(rec.tableBase + rec.meta.tableSize - this.table.length);
      rec.tlsGlobal = new WebAssembly.Global(
        { value: "i32", mutable: true },
        rec.tlsBase,
      );
      const dl = { table: this.table };
      for (const r of rec.meta.imports) {
        if (
          rec.meta.deferredMain &&
          ["externalFunction", "external", "externalTLS", "setjmp"].includes(
            r.kind,
          )
        ) {
          const global = new WebAssembly.Global(
            { value: "i32", mutable: true },
            0,
          );
          rec.bindingGlobals.set(r.importName, { request: r, global });
          dl[r.importName] = global;
        } else dl[r.importName] = this.value(r, rec);
      }
      const functions = Object.create(null);
      const tags = Object.create(null);
      for (const im of WebAssembly.Module.imports(rec.module)) {
        if (im.module !== 'dl.tag' || im.kind !== 'tag') continue;
        const meta = rec.meta.tags.find(t => t.name === im.name);
        if (!meta || !Array.isArray(meta.params)) throw new Error('Missing dynamic exception signature: ' + im.name);
        const signature = JSON.stringify(meta.params);
        let shared = this.exceptionTags.get(im.name);
        if (shared && shared.signature !== signature) throw new Error('Dynamic exception signature mismatch: ' + im.name);
        if (!shared) {
          const types = {127:'i32',126:'i64',125:'f32',124:'f64'};
          shared = {signature, tag:new WebAssembly.Tag({parameters:meta.params.map(t => {
            if (!types[t]) throw new Error('Unsupported dynamic exception parameter');
            return types[t];
          })})};
          this.exceptionTags.set(im.name, shared);
        }
        tags[im.name] = shared.tag;
      }
      for (const im of WebAssembly.Module.imports(rec.module))
        if (im.module === "dl.func")
          functions[im.name] = () => {
            throw new Error(
              "External C function entered through a host adapter: " + im.name,
            );
          };
      for(const im of WebAssembly.Module.imports(rec.module)) if(im.kind==='memory' && im.module==='c' && /^memory[1-9][0-9]*$/.test(im.name))
        if(!this.imports.c[im.name])throw new Error('Additional memory '+im.name+' must be supplied by the executable before loading libraries or starting threads');
      if (this.main.meta.memoryPolicy === 'private-snapshot-v1' && WebAssembly.Module.imports(rec.module).some(i=>i.kind==='function' && i.name==='__mmap'))
        throw new Error('Legacy mmap library must be rebuilt for private-snapshot memory: '+rec.path);
      rec.instance = new WebAssembly.Instance(rec.module, {
        ...this.imports,
        dl,
        "dl.func": functions,
        "dl.tag": tags,
      });
      return rec.instance;
    }
    relocate(rec) {
      const v = this.view();
      for (const r of rec.meta.relocations)
        v.setUint32(
          rec.memoryBase + r.at,
          (this.value(r, rec) + (r.offset || 0)) >>> 0,
          true,
        );
    }
    installDescriptors(rec) {
      const v = this.view();
      for (const d of rec.meta.descriptors) {
        const id = rec.tableBase + d.slot - 1,
          chunk = v.getUint32(this.state.registry + (id >>> 10) * 4, true);
        if (!chunk) throw new Error("Missing descriptor page");
        const at = chunk + (id & 1023) * 16;
        [
          d.step ? rec.tableBase + d.step - 1 : 0,
          d.size,
          rec.types[d.type],
          d.locals,
        ].forEach((n, i) => v.setUint32(at + i * 4, n, true));
      }
    }
    async alloc(n, a = 16) {
      if (!n) return 0;
      if (this.bootstrap) {
        const p = align(this.state.heap, a),
          end = align(p + n);
        if (!Number.isSafeInteger(end) || end >= 0x80000000)
          throw new Error("Bootstrap memory exhausted");
        this.ensureMemory(end);
        new Uint8Array(this.memory.buffer, p, end - p).fill(0);
        this.state.heap = end;
        this.state.bootstrapEnd = end;
        return p;
      }
      if (this.allocateOverride) return this.allocateOverride(n, a);
      const provider = this.runtime || this.main;
      const sym = provider.symbols.get("malloc");
      if (!sym) throw new Error("Dynamic executable must export malloc");
      const fn = provider.instance.exports["__dl_fn_" + sym.value];
      const p = Number(await WebAssembly.promising(fn)(n + a - 1)) >>> 0;
      if (!p) throw new Error("Dynamic loader allocation failed");
      const address = align(p, a);
      this.allocations.set(address, p);
      this.transaction?.push({ address, raw: p });
      return address;
    }
    async initialize() {
      if (this.initialized) return;
      this.installing = true;
      try {
        if (this.main.meta.tlsSize) {
          if (!this.main.tlsBase)
            this.main.tlsBase = await this.alloc(
              this.main.meta.tlsSize,
              this.main.meta.tlsAlign,
            );
          this.main.instance.exports.__tls_init(this.main.tlsBase);
        }
        if (!this.worker)
          this.worker = await this.registerWorker(this.thread.value);
        this.initialized = true;
        this.syncModules();
      } finally {
        this.installing = false;
      }
    }
    sync() {
      if (
        this.installing ||
        this.generation.value ===
          Atomics.load(
            new Int32Array(this.memory.buffer),
            this.state.control >> 2,
          )
      )
        return;
      this.installing = true;
      try {
        this.syncModules();
      } finally {
        this.installing = false;
      }
    }
    tlsFor(worker, handle) {
      for (
        let p = this.view().getUint32(worker + 8, true);
        p;
        p = this.view().getUint32(p + 12, true)
      )
        if (this.view().getUint32(p, true) === handle)
          return this.view().getUint32(p + 4, true);
      throw new Error("Missing reserved module TLS");
    }
    async reserveTLS(worker, handle, size, alignment) {
      if (!size) return null;
      const base = await this.alloc(size, alignment),
        raw = this.allocations.get(base) || base,
        node = await this.alloc(32);
      const v = this.view();
      v.setUint32(node + 16, this.allocations.get(node) || node, true);
      v.setUint32(node, handle, true);
      v.setUint32(node + 4, base, true);
      v.setUint32(node + 8, raw, true);
      return { worker, node, base };
    }
    publishTLS({ worker, node }) {
      const v = this.view();
      v.setUint32(node + 12, v.getUint32(worker + 8, true), true);
      Atomics.store(
        new Int32Array(this.memory.buffer),
        (worker + 8) >> 2,
        node,
      );
    }
    async registerWorker(record) {
      const worker = await this.alloc(16),
        v = this.view();
      v.setUint32(worker, 0, true);
      v.setUint32(worker + 4, record, true);
      v.setUint32(worker + 8, 0, true);
      v.setUint32(worker + 12, 1, true);
      for (const rec of this.modules)
        if (rec !== this.main && rec.meta.tlsSize)
          this.publishTLS(
            await this.reserveTLS(
              worker,
              rec.handle,
              rec.meta.tlsSize,
              rec.meta.tlsAlign,
            ),
          );
      const view = this.view();
      view.setUint32(
        worker,
        view.getUint32(this.state.control + 16, true),
        true,
      );
      Atomics.store(
        new Int32Array(this.memory.buffer),
        (this.state.control + 16) >> 2,
        worker,
      );
      return worker;
    }
    async acquire() {
      const lock = (this.state.control + 12) >> 2;
      while (
        Atomics.compareExchange(
          new Int32Array(this.memory.buffer),
          lock,
          0,
          1,
        ) !== 0
      )
        await new Promise((r) => setTimeout(r, 1));
    }
    unlock() {
      const i32 = new Int32Array(this.memory.buffer),
        lock = (this.state.control + 12) >> 2;
      Atomics.store(i32, lock, 0);
      Atomics.notify(i32, lock);
    }
    async prepareThread(record) {
      await this.acquire();
      this.installing = true;
      this.transaction = [];
      try {
        this.syncModules();
        return await this.registerWorker(record);
      } catch (error) {
        for (const { raw } of this.transaction) await this.freeRaw(raw);
        throw error;
      } finally {
        this.transaction = null;
        this.allocations.clear();
        this.installing = false;
        this.unlock();
      }
    }
    checkExecution(meta, path) {
      if (!meta || meta.version < 4)
        throw new Error('Legacy execution ABI: '+path+'; relink with a compiler declaring cloning capability');
      const clone=meta.execution?.clone ?? false;
      if (typeof clone !== 'boolean')
        throw new Error('Invalid cloning capability declaration: '+path);
      if (clone && !this.canClone)
        throw new Error('Cloning-capable module cannot load into a non-fork process: '+path);
      if (!clone && meta.symbols.some(s=>s.execution==='clone-v1'))
        throw new Error('Cloning function in a non-fork module: '+path);
    }
    syncModules() {
      // Records are immutable and linked only after data and descriptors exist.
      const generation = Atomics.load(
        new Int32Array(this.memory.buffer),
        this.state.control >> 2,
      );
      let p = Atomics.load(
        new Int32Array(this.memory.buffer),
        (this.state.control + 4) >> 2,
      );
      while (p) {
        if (!this.modules.some((r) => r.handle === p)) {
          const v = this.view(),
            len = v.getUint32(p + 4, true),
            module =
              this.compiledModules.get(p) ||
              new WebAssembly.Module(
                new Uint8Array(this.memory.buffer, p + 32, len).slice(),
              ),
            meta = metadata(module);
          this.checkExecution(meta, '<worker module>');
          const rec = {
            module,
            meta,
            handle: p,
            memoryBase: v.getUint32(p + 8, true),
            tableBase: v.getUint32(p + 12, true),
            global: !!v.getUint32(p + 16, true),
            globalOrder: v.getUint32(p + 16, true),
            bindingValues: new Map(
              JSON.parse(
                new TextDecoder().decode(
                  new Uint8Array(
                    this.memory.buffer,
                    p + 32 + len + v.getUint32(p + 20, true),
                    v.getUint32(p + 24, true),
                  ),
                ),
              ),
            ),
            path: new TextDecoder().decode(
              new Uint8Array(
                this.memory.buffer,
                p + 32 + len,
                v.getUint32(p + 20, true),
              ),
            ),
            tlsBase: meta.tlsSize ? this.tlsFor(this.worker, p) : 0,
          };
          this.intern(rec);
          this.registerSymbols(rec);
          this.instantiate(rec);
          if (!this.tlsSaved[p]) rec.instance.exports.__tls_init(rec.tlsBase);
          this.modules.push(rec);
        } else {
          const rec = this.modules.find((r) => r.handle === p);
          if (!rec.global && this.view().getUint32(p + 16, true)) {
            rec.global = true;
            rec.globalOrder = this.view().getUint32(p + 16, true);
          }
        }
        p = Atomics.load(new Int32Array(this.memory.buffer), p >> 2);
      }
      this.rebuildGlobals();
      this.runtime = this.modules.find((r) => r.meta.runtime) || null;
      if (this.restored && this.main.meta.deferredMain) this.bindMain();
      this.generation.value = generation;
    }
    async prepareRuntime() {
      await this.initialize();
      if (this.bootstrap) await this.loadGroup(null, null, 258, true, true);
    }
    async startup() {
      await this.prepareRuntime();
      if (this.pendingConstructors) {
        const run = this.pendingConstructors;
        this.pendingConstructors = null;
        await run();
        return;
      }
      await this.initialize();
      if (this.restored) {
        this.bindMain();
        return;
      }
      if (this.main.meta.needed?.length)
        await this.loadGroup(null, null, 258, true);
    }
    async load(bytes, path, flags = 2, module = null) {
      return this.loadGroup(bytes, path, flags, false, false, module);
    }
    async loadGroup(bytes, path, flags, startup, deferConstructors = false, compiledModule = null) {
      await this.initialize();
      await this.acquire();
      this.installing = true;
      this.transaction = [];
      const pending = [],
        roots = [];
      let result,
        committed = false;
      try {
        this.syncModules();
        this.pending = pending; // deduplicate explicit roots already loaded as dependencies
        const visit = async (bytes, path, compiledModule = null) => {
          let rec = this.modules.concat(pending).find((r) => r.path === path);
          if (rec) return rec;
          const module = compiledModule || new WebAssembly.Module(bytes),
            meta = metadata(module);
          if (!meta?.shared)
            throw new Error("Not a wasm-posix shared library: " + path);
          this.checkExecution(meta, path);
          if (meta.runtime && ((!this.main.meta.sharedLibc && this.main.symbols.has('malloc')) || this.modules.concat(pending).some(r=>r.meta.runtime && r.meta.soname!==meta.soname)))
            throw new Error('Cannot load a second process libc: '+path);
          rec = this.modules
            .concat(pending)
            .find((r) => meta.soname && r.meta.soname === meta.soname);
          if (rec) return rec;
          rec = {
            module,
            meta,
            path,
            bytes,
            global: !!(flags & 256),
            tlsBase: 0,
            bindingTargets: new Map(),
          };
          pending.push(rec);
          for (const name of meta.needed || []) {
            const existing = this.modules
              .concat(pending)
              .find(
                (r) =>
                  r.path === name ||
                  r.meta.soname === name ||
                  r.path.split("/").pop() === name,
              );
            if (!existing) {
              const f = await this.read(name, rec);
              if (!f.record) await visit(f.bytes, f.path, f.module);
            }
          }
          return rec;
        };
        if (startup) {
          for (const name of this.main.meta.needed) {
            const f = await this.read(name);
            roots.push(f.record || (await visit(f.bytes, f.path, f.module)));
          }
        } else {
          const old = this.modules.find((r) => r.path === path);
          if (old) roots.push(old);
          else {
            if (flags & 4) throw new Error("Library is not loaded: " + path);
            roots.push(await visit(bytes, path, compiledModule));
          }
        }
        this.pending = pending;
        this.bindingScope = startup
          ? this.scope(this.main)
          : this.scope(roots[0]);
        pending.sort(
          (a, b) => this.bindingScope.indexOf(a) - this.bindingScope.indexOf(b),
        );
        let nextTable = this.view().getUint32(this.state.control + 8, true);
        for (const rec of pending) {
          const { meta } = rec;
          rec.tableBase = nextTable;
          nextTable += meta.tableSize;
          if (nextTable >= 1048576)
            throw new Error("Dynamic function table exhausted");
          rec.memoryBase =
            (await this.alloc(
              meta.memorySize + (meta.stack % meta.memoryAlign),
              meta.memoryAlign,
            )) +
            (meta.stack % meta.memoryAlign);
          this.intern(rec);
          rec.symbols = new Map(
            meta.symbols.map((s) => [s.name, { ...s, owner: rec }]),
          );
          // Reserve catalog identities before binding, including cyclic TLS references.
          const names = new Set(
            [
              ...Object.values(meta.externalFunctions),
              ...meta.imports.map((r) => r.name || r.nameSymbol),
              ...meta.relocations.map((r) => r.name || r.nameSymbol),
            ].filter(Boolean),
          );
          rec.pathBytes = new TextEncoder().encode(rec.path);
          rec.bindingCapacity =
            new TextEncoder().encode(
              JSON.stringify(
                [...names].map((n) => [
                  n,
                  { tls: 4294967295, offset: 4294967295, value: 4294967295 },
                ]),
              ),
            ).length + 64;
          rec.handle = await this.alloc(
            32 + rec.bytes.length + rec.pathBytes.length + rec.bindingCapacity,
          );
          rec.reservations = [];
          if (meta.tlsSize)
            for (
              let p = this.view().getUint32(this.state.control + 16, true);
              p;
              p = this.view().getUint32(p, true)
            )
              if (this.view().getUint32(p + 12, true))
                rec.reservations.push(
                  await this.reserveTLS(
                    p,
                    rec.handle,
                    meta.tlsSize,
                    meta.tlsAlign,
                  ),
                );
          rec.tlsBase =
            rec.reservations.find((r) => r.worker === this.worker)?.base || 0;
        }
        // Global startup lookup is executable first, then breadth-first dependencies.
        if (startup) {
          this.main.deferred = false;
          const order = this.scope(this.main);
          pending.sort((a, b) => order.indexOf(a) - order.indexOf(b));
        }
        // One address-bearing index per load transaction preserves lookup order.
        // It is never shared across processes or retained across dlopen changes.
        this.bindingIndex=new Map();
        for(const rec of this.bindingScope)for(const [name,symbol] of rec.symbols)
          if(!this.bindingIndex.has(name))this.bindingIndex.set(name,symbol);
        for (const rec of pending) this.validateBindings(rec);
        if (startup) this.validateBindings(this.main);
        for (const rec of pending) {
          const { meta, tableBase } = rec;
          for (
            let i = tableBase >>> 10;
            i <= (tableBase + meta.tableSize - 1) >>> 10;
            i++
          )
            if (!this.view().getUint32(this.state.registry + i * 4, true)) {
              const page = await this.alloc(CHUNK);
              new Uint8Array(this.memory.buffer, page, CHUNK).fill(0);
              this.view().setUint32(this.state.registry + i * 4, page, true);
              this.retained.add(page);
            }
        }
        // Finish every bootstrap reservation before importing libc's heap base.
        for (const rec of pending) {
          this.instantiate(rec);
          rec.instance.exports.__dylink_init();
        }
        for (const rec of pending) {
          this.relocate(rec);
          rec.instance.exports.__tls_init(rec.tlsBase);
          this.installDescriptors(rec);
        }
        for (const rec of pending) {
          rec.bindingBytes = new TextEncoder().encode(
            JSON.stringify(
              [...rec.bindingTargets].map(([name, s]) => [
                name,
                s.kind === "tls"
                  ? { tls: s.owner.handle, offset: s.value }
                  : { value: this.address(s) },
              ]),
            ),
          );
          if (rec.bindingBytes.length > rec.bindingCapacity)
            throw new Error("Dynamic binding catalog overflow");
        }
        for (const rec of pending) {
          const { meta, tableBase, handle, bytes, pathBytes, bindingBytes } =
            rec;
          for (const r of rec.reservations) this.publishTLS(r);
          const mem = new Uint8Array(this.memory.buffer);
          mem.set(bytes, handle + 32);
          mem.set(pathBytes, handle + 32 + bytes.length);
          mem.set(bindingBytes, handle + 32 + bytes.length + pathBytes.length);
          const view = this.view();
          view.setUint32(
            handle,
            pending[pending.indexOf(rec) + 1]?.handle || 0,
            true,
          );
          view.setUint32(handle + 4, bytes.length, true);
          view.setUint32(handle + 8, rec.memoryBase, true);
          view.setUint32(handle + 12, tableBase, true);
          rec.globalOrder = rec.global
            ? Atomics.add(
                new Int32Array(this.memory.buffer),
                (this.state.control + 20) >> 2,
                1,
              ) + 1
            : 0;
          view.setUint32(handle + 16, rec.globalOrder, true);
          view.setUint32(handle + 24, bindingBytes.length, true);
          view.setInt32(handle + 28, this.thread.value, true);
          view.setUint32(handle + 20, pathBytes.length, true);
          this.modules.push(rec);
          this.registerSymbols(rec);
        }
        if (pending.length) {
          let tail = this.view().getUint32(this.state.control + 4, true);
          if (tail) {
            while (this.view().getUint32(tail, true))
              tail = this.view().getUint32(tail, true);
          }
          Atomics.store(
            new Int32Array(this.memory.buffer),
            (tail || this.state.control + 4) >> 2,
            pending[0].handle,
          );
        }
        this.view().setUint32(this.state.control + 8, nextTable, true);
        Atomics.add(
          new Int32Array(this.memory.buffer),
          this.state.control >> 2,
          1,
        );
        committed = true;
        this.syncModules();
        if (startup) this.bindMain();
        if (this.bootstrap) this.bootstrap = false;
        result = roots[0]?.handle;
      } catch (error) {
        if (!committed)
          for (const { address, raw } of this.transaction)
            if (!this.retained.has(address)) await this.freeRaw(raw);
        throw error;
      } finally {
        this.pending = null;
        this.bindingScope = null;
        this.bindingIndex = null;
        this.transaction = null;
        this.allocations.clear();
        this.installing = false;
        this.unlock();
      }
      const finish = async () => {
        const seen = new Set(),
          order = [];
        const walk = (rec) => {
          if (seen.has(rec)) return;
          seen.add(rec);
          for (const dep of this.scope(rec).slice(1)) walk(dep);
          order.push(rec);
        };
        for (const root of roots) walk(root);
        try {
          for (const rec of order) {
            if (pending.includes(rec)) {
              const init = rec.instance.exports.__wasm_call_ctors;
              if (init) {
                // Wasm traps bypass catch_all_ref. A failed dlopen constructor
                // is recoverable, so discard its activation storage at this
                // known entry boundary without rolling back C object writes.
                const thread=this.thread.value,stack=this.stack.value;
                const offsets=[0,4,8,20,24,2044];
                const saved=offsets.map(off=>this.view().getUint32(thread+off,true));
                try { await WebAssembly.promising(init)(); }
                catch (error) {
                  offsets.forEach((off,i)=>this.view().setUint32(thread+off,saved[i],true));
                  this.stack.value=stack;
                  throw error;
                }
              }
            } else await this.waitInitialized(rec, flags);
          }
          for (const rec of pending)
            Atomics.store(
              new Int32Array(this.memory.buffer),
              (rec.handle + 28) >> 2,
              0,
            );
        } catch (e) {
          for (const rec of pending)
            Atomics.store(
              new Int32Array(this.memory.buffer),
              (rec.handle + 28) >> 2,
              -1,
            );
          throw e;
        } finally {
          for (const rec of pending)
            Atomics.notify(
              new Int32Array(this.memory.buffer),
              (rec.handle + 28) >> 2,
            );
        }
        if (flags & 256)
          for (const root of roots)
            for (const rec of this.scope(root)) this.reopen(rec, flags);
      };
      if (deferConstructors) this.pendingConstructors = finish;
      else await finish();
      return result;
    }
    async waitInitialized(rec, flags) {
      for (;;) {
        const state = new Int32Array(this.memory.buffer),
          slot = (rec.handle + 28) >> 2;
        const owner = Atomics.load(state, slot);
        if (owner === -1)
          throw new Error("Library constructor failed: " + rec.path);
        if (owner === 0 || owner === this.thread.value)
          return this.reopen(rec, flags);
        await Atomics.waitAsync(state, slot, owner).value;
      }
    }
    async freeRaw(raw) {
      if (raw >= this.state.bootstrapStart && raw < this.state.bootstrapEnd)
        return;
      const provider = this.runtime || this.main;
      const sym = provider.symbols.get("free");
      if (raw && sym)
        await WebAssembly.promising(
          provider.instance.exports["__dl_fn_" + sym.value],
        )(raw);
    }
    async disposeThread(worker = this.worker) {
      await this.acquire();
      this.installing = true;
      try {
        this.view().setUint32(worker + 12, 0, true);
        let p = this.view().getUint32(worker + 8, true);
        this.view().setUint32(worker + 8, 0, true);
        while (p) {
          const next = this.view().getUint32(p + 12, true),
            raw = this.view().getUint32(p + 8, true),
            nodeRaw = this.view().getUint32(p + 16, true);
          await this.freeRaw(raw);
          await this.freeRaw(nodeRaw);
          p = next;
        }
      } finally {
        this.installing = false;
        this.unlock();
      }
    }
    reopen(rec, flags) {
      const handle = rec.handle,
        group = this.scope(rec);
      for (const rec of group)
        if (flags & 256 && !rec.global) {
          rec.global = true;
          rec.globalOrder =
            Atomics.add(
              new Int32Array(this.memory.buffer),
              (this.state.control + 20) >> 2,
              1,
            ) + 1;
          Atomics.store(
            new Int32Array(this.memory.buffer),
            (rec.handle + 16) >> 2,
            rec.globalOrder,
          );
          Atomics.add(
            new Int32Array(this.memory.buffer),
            this.state.control >> 2,
            1,
          );
          this.syncModules();
        }
      return handle;
    }
    symbol(handle, name) {
      const rec =
        handle === 0 || handle === 1
          ? this.main
          : this.modules.find((r) => r.handle === handle);
      if (!rec) throw new Error("Invalid dynamic library handle");
      const s =
        handle === 0 || handle === 1
          ? this.globals.get(name)
          : this.scope(rec)
              .map((r) => r.symbols.get(name))
              .find(Boolean);
      if (!s) throw new Error("Undefined dynamic symbol: " + name);
      return this.address(s);
    }
    close(handle) {
      if (handle !== 1 && !this.modules.some((r) => r.handle === handle))
        throw new Error("Invalid dynamic library handle");
      return 0;
    }
    compiled() {
      return this.modules
        .filter((r) => r !== this.main)
        .map((r) => [r.handle, r.module, metadataText(r.module)]);
    }
    snapshot() {
      return {
        state: this.state,
        compiledModules: this.compiled(),
        worker: this.worker,
        tls: Object.fromEntries(this.modules.map((r) => [r.handle, r.tlsBase])),
      };
    }
  }
  return { metadata, metadataText, Loader };
})();
