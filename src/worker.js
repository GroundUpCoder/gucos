let activeSDL, activeInstance, activeCanvas;
let inputBytes = new Uint8Array(), inputEOF=false, wakeInput;
function feedInput(text, eof) {
  const bytes=new TextEncoder().encode(text||'');
  const next=new Uint8Array(inputBytes.length+bytes.length); next.set(inputBytes); next.set(bytes,inputBytes.length); inputBytes=next;
  if(eof)inputEOF=true;
  if(wakeInput){wakeInput();wakeInput=null;}
}
let started = false;
const send = (type, data = {}) => postMessage({ type, ...data });
self.onmessage = async ({ data }) => {
  if (data.type === 'stdin') { feedInput(data.text,data.eof); return; }
  if (data.type === 'resize') {
    if(activeCanvas&&activeInstance) {
      activeCanvas.width=data.width; activeCanvas.height=data.height;
      activeInstance.exports.__sdl_push_window_event?.(1,0x206,data.width,data.height);
    }
    return;
  }
  if (data.type === 'key') {
    activeSDL?.pushKeyEvent(1, data.down ? 0x300 : 0x301, data.scan, data.sym, data.mod, data.repeat);
    return;
  }
  if (data.type !== 'start' || started) return;
  started = true;
  try {
    if (typeof WebAssembly.Suspending !== 'function') throw new Error('This browser needs WebAssembly JSPI.');
    if (data.action === 'compile') {
      send('status', { text: 'Compiling C → WebAssembly…' });
      const result = compile(data.files, data.inputs, data.includes, text => send('output', { text }), {compilerOptions:data.compilerOptions||{}});
      send('compiled', result);
      return;
    }
    activeCanvas=data.canvas;
    const fs = BLOCK_FS.create(new BLOCK_FS.MemoryByteStore(1024 * 1024));
    const encoder = new TextEncoder();
    for(const dir of data.directories||['/tmp'])fs.mkdir(dir,0o755);
    fs.ensureDevNodes();
    for (const [path, content] of Object.entries(data.files || {})) {
      const parts = normalize(path).split('/').filter(Boolean);
      for (let i = 1; i < parts.length; i++) fs.mkdir('/' + parts.slice(0, i).join('/'), 0o777);
      const fd = fs.open(normalize(path), 0x40 | 0x200 | 1, 0o666);
      const bytes = typeof content === 'string' ? encoder.encode(content) : content;
      if (fd < 0 || fs.write(fd, bytes, bytes.length) !== bytes.length) throw new Error(`Cannot mount ${path}`);
      fs.close(fd);
    }
    if(data.cwd)fs.chdir(data.cwd);
    fs.setStdin(encoder.encode(data.stdin || ''));
    const decoder = new TextDecoder();
    let frames = 0;
    const code = await runModule({ bytes: data.bytes, args: data.args, pid: data.pid, ppid: 1,
      env: { HOME: '/home/user', PWD:data.cwd||'/', TMPDIR: '/tmp' }, getBrowserSDL: data.canvas,
      blockFsFactory: async ctx => {
        const imports=fs.toWasmEnv(ctx);
        if(data.interactive) {
          const read=imports.read;
          imports.read=new WebAssembly.Suspending(async(fd,ptr,count)=>{
            if(fd!==0)return read(fd,ptr,count);
            count=count>>>0;
            if(!count)return 0;
            while(!inputBytes.length&&!inputEOF)await ctx.awaitImport(new Promise(resolve=>wakeInput=resolve));
            const n=Math.min(count,inputBytes.length);
            new Uint8Array(ctx.getMemory().buffer,ptr,n).set(inputBytes.subarray(0,n));
            inputBytes=inputBytes.slice(n);return n;
          });
        }
        return {c:imports};
      },
      writeOut: b => send('output', { text: typeof b === 'string' ? b : decoder.decode(b, { stream: true }) }),
      writeErr: b => send('output', { text: typeof b === 'string' ? b : new TextDecoder().decode(b) }),
      notifyWindow: event => {
        if (event.type === 'present') { if (++frames % 30 === 0 || frames === 1) send('frame', { frames }); }
        else send('window', { event });
      },
      onReady: ({ sdl, instance }) => { activeSDL = sdl; activeInstance=instance; send('ready'); }
    });
    send('exit', { code: code | 0 });
  } catch (error) { send('error', { text: error.stack || String(error) }); }
};
