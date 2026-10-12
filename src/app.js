const $ = id => document.getElementById(id);
let ui;
const print = text => ui.print(text);
const normalize = p => '/' + p.split('/').reduce((a, x) => { if (x === '..') a.pop(); else if (x && x !== '.') a.push(x); return a; }, []).join('/');
const unbase64 = text => {
  const decoded = atob(text), bytes = new Uint8Array(decoded.length);
  // Uint8Array.from(string, ...) first collects the iterable's characters.
  // Avoid that large temporary array when loading the bundled source archive.
  for (let i = 0; i < decoded.length; i++) bytes[i] = decoded.charCodeAt(i);
  return bytes;
};
// The bundle is one or more gzip sections (tools/bundle.mjs): the first holds
// every payload key, later ones carry parts of the large source maps that merge
// into it, so no section's JSON approaches the engine's string length limit.
const payload = {};
for (const text of $('bundle').textContent.trim().split(/\s+/)) {
  const section = JSON.parse(await new Response(new Blob([unbase64(text)]).stream().pipeThrough(new DecompressionStream('gzip'))).text());
  for (const [key, value] of Object.entries(section)) payload[key] = key in payload && value && typeof value === 'object' && !Array.isArray(value) ? Object.assign(payload[key], value) : value;
}
$('bundle').remove();

const storageNames=installationStorage(payload.repository);
try { await resetInstallation(storageNames); }
catch (error) {
  $('status').textContent = 'Reset failed';
  $('terminal').textContent = `Reset failed: ${error.message}. Close other tabs using this disk, then reload to continue or use ?reset=1 to retry.`;
  throw error;
}

if(payload.licensesURL)$('license-link').href=payload.licensesURL;
const terminalStyle = document.createElement('style'); terminalStyle.textContent = payload.terminalCSS; document.head.append(terminalStyle);
const libraryURL = URL.createObjectURL(new Blob([payload.terminalJS], {type:'text/javascript'}));
await new Promise((resolve,reject) => { const script=document.createElement('script'); script.src=libraryURL; script.onload=resolve; script.onerror=reject; document.head.append(script); });
URL.revokeObjectURL(libraryURL);
const terminal = new window.Terminal({convertEol:true,cursorBlink:true,fontSize:15,fontFamily:'ui-monospace, SFMono-Regular, Menlo, Consolas, monospace',scrollback:5000,theme:{background:'#0b1018',foreground:'#d6e1ef',cursor:'#78dfb2',selectionBackground:'#364b68'}});
const fit = new window.FitAddon.FitAddon(); terminal.loadAddon(fit); terminal.open($('terminal')); fit.fit();
new ResizeObserver(() => fit.fit()).observe($('terminal'));

const files = payload.files;
const edited = {};
try { const cached=JSON.parse(localStorage.getItem(storageNames.edits) || '{}'); for(const [path,text] of Object.entries(cached))edited[legacyPath(path)]=text; Object.assign(files, edited); } catch {}
const executables = new Map();
const processes = new Map();
let cwd=HOME_DIRECTORY, previousCwd=HOME_DIRECTORY;
const resolvePath=p=>normalize(p.startsWith('/')?p:cwd+'/'+p);
async function directoryExists(path){if(payload.hosted&&busybox?.worker)return ((await busybox.metadata(path))?.mode&0xf000)===0x4000;return SYSTEM_DIRECTORIES.includes(path)||Object.keys(files).some(p=>p.startsWith(path+'/'));}
let nextPid = 2;
let busybox;
const workerURL = payload.workerURL ? new URL(payload.workerURL,location.href).href : URL.createObjectURL(new Blob([payload.worker], { type: 'text/javascript' }));
function save(path, text) {
  path=resolvePath(path); files[path]=edited[path]=text;
  if(payload.hosted&&busybox?.ready&&busybox.worker){delete edited[path];return busybox.write(path,new TextEncoder().encode(text));}
  try { localStorage.setItem(storageNames.edits, JSON.stringify(edited)); } catch { print('Source saved in memory; local storage unavailable.\n'); }
}
async function edit(path) { path=resolvePath(path);const text=payload.hosted&&busybox?.worker?new TextDecoder().decode(await busybox.read(path)||new Uint8Array()):files[path]??'';ui.edit(path,text); }
function finish(proc, state, result) {
  if (proc.state !== 'running') return;
  proc.state = state;
  proc.result = result;
  proc.worker.terminate();
  proc.win?.destroy();
  proc.resolve(result);
}
function spawn(name, message, transfers = []) {
  const pid = nextPid++;
  const worker = new Worker(workerURL, { name: `process-${pid}-${name}` });
  const proc = { pid, name, worker, state: 'running', output: '', frames: 0 };
  proc.done = new Promise(resolve => proc.resolve = resolve);
  proc.visible = new Promise(resolve => proc.resolveVisible = resolve);
  if (ui.busy) ui.foreground = proc;
  processes.set(pid, proc);
  worker.onmessage = ({ data }) => {
    if (data.type === 'output') { proc.output += data.text; print(data.text); }
    if (data.type === 'status') { $('status').textContent = data.text; }
    if (data.type === 'compiled') {
      print(`[${pid}] ${data.stats.functions} functions, ${data.stats.bytes} bytes, ${data.stats.milliseconds} ms\n`);
      finish(proc, 'exited', { code: 0, ...data });
      $('status').textContent = 'Build complete';
    }
    if (data.type === 'window') {
      proc.win.show({...data.event, title: `[${pid}] ${data.event.title || name}`});
      proc.resolveVisible();
    }
    if (data.type === 'frame') { proc.frames = data.frames; proc.win.show(); proc.resolveVisible(); }
    if (data.type === 'exit') { print(`[${pid}] exited ${data.code}\n`); finish(proc, 'exited', data); }
    if (data.type === 'error') {
      print(`[${pid}] ${data.text}\n`); finish(proc, 'failed', { code: 1, error: data.text });
      $('status').textContent = 'Process failed';
    }
  };
  worker.onerror = event => {
    print(`[${pid}] worker: ${event.message}\n`);
    finish(proc, 'failed', { code: 1, error: event.message });
  };
  worker.postMessage({ type: 'start', pid, ...message }, transfers);
  return proc;
}
function kill(pid) {
  const p = processes.get(Number(pid));
  if (!p || p.state !== 'running') return false;
  finish(p, 'killed', { code: 137 });
  print(`[${pid}] killed\n`);
  return true;
}
async function build(inputs, output, includes = [], compilerOptions = {}) {
  const proc = spawn('cc', { action: 'compile', files, inputs:inputs.map(resolvePath), includes:includes.map(resolvePath), compilerOptions });
  const result = await proc.done;
  if (result.code) throw new Error(result.error || 'Compilation stopped');
  executables.set(resolvePath(output), result);
  return result;
}
function run(path, args = [], stdin = '') {
  path = resolvePath(path);
  const executable = executables.get(path);
  if (!executable) throw new Error(`No executable ${path}; compile it with cc first.`);
  const canvas = document.createElement('canvas');
  canvas.width = 640; canvas.height = 400; canvas.tabIndex = 0;
  const offscreen = canvas.transferControlToOffscreen();
  const proc = spawn(path, { action: 'run', bytes: executable.bytes, args: [path, ...args],
    stdin, interactive: stdin===null, cwd, directories:SYSTEM_DIRECTORIES, files: { ...files, ...(typeof distributionProcessFiles==='function'?distributionProcessFiles():{}) }, canvas: offscreen }, [offscreen]);
  const win = new VirtualTerminal(canvas, () => kill(proc.pid), (width,height) => proc.worker.postMessage({type:'resize',width,height}), () => terminal.focus());
  win.title.textContent=`[${proc.pid}] ${path}`;
  Object.assign(proc, { win, screen:win.element, canvas, title:win.title });
  const scans = { Enter:40, Escape:41, Backspace:42, Tab:43, Space:44, ArrowRight:79, ArrowLeft:80, ArrowDown:81, ArrowUp:82, ControlLeft:224, ShiftLeft:225, AltLeft:226, ControlRight:228, ShiftRight:229, AltRight:230 };
  const held = new Map();
  function key(e, down) {
    e.preventDefault();
    let scan = scans[e.code] || (/^Key[A-Z]$/.test(e.code) ? e.code.charCodeAt(3) - 65 + 4 : /^Digit[1-9]$/.test(e.code) ? Number(e.code[5]) + 29 : e.code === 'Digit0' ? 39 : 0);
    let sym = [...e.key].length === 1 ? e.key.codePointAt(0) : ({ Enter:13, Escape:27, Backspace:8, Tab:9 })[e.code] || (scan | 0x40000000);
    const message = { type:'key', down, scan, sym, repeat:e.repeat, mod:(e.shiftKey ? 3 : 0) | (e.ctrlKey ? 192 : 0) | (e.altKey ? 768 : 0) };
    if (down) held.set(e.code, message); else held.delete(e.code);
    proc.worker.postMessage(message);
  }
  canvas.addEventListener('keydown', e => key(e, true));
  canvas.addEventListener('keyup', e => key(e, false));
  canvas.addEventListener('blur', () => { for (const event of held.values()) proc.worker.postMessage({ ...event, down:false, repeat:false }); held.clear(); });
  print(`[${proc.pid}] started ${path}\n`);
  return proc;
}
function words(line) {
  const tokens = []; let word = '', quote = '', active = false;
  for (let i = 0; i < line.length; i++) {
    const c = line[i];
    if (c === '\\' && quote !== "'") { if (++i >= line.length) throw new Error('Trailing escape'); word += line[i]; active = true; }
    else if (quote) { if (c === quote) quote = ''; else word += c; }
    else if (c === '"' || c === "'") { quote = c; active = true; }
    else if (/\s/.test(c)) { if (active) tokens.push(word); word = ''; active = false; }
    else { word += c; active = true; }
  }
  if (quote) throw new Error('Unclosed quote');
  if (active) tokens.push(word);
  return tokens;
}
async function command(line, echo = true, interactive = false) {
  if (echo) print(`$ ${line}\n`);
  const [cmd, ...args] = words(line);
  const background=args.at(-1)==='&'; if(background)args.pop();
  if (!cmd) return;
  if (cmd === 'help') print('cc FILE... [-I DIR] [-o NAME]  compile C\n./NAME [ARGS...] [&]            run a worker process\nvi FILE                       edit in the terminal\nsh [ARGS...]                  real BusyBox ash + shared OPFS disk\npwd · cd [DIR] · ls [-aFdR] [PATH...] · sh [ARGS...] · cat FILE · ps · kill PID · fg PID · clear · licenses\nCtrl-C interrupts; Ctrl-D sends EOF. Up/Down: history. Tab: completion.\nvi: i/a/o insert, Esc normal, hjkl/arrows, gg/G, 0/$, w/b, dd/yy/p, x, u, Ctrl-R, /search, n, :w, :q, :wq, :q!\nWindows: drag title to move, corner to resize, double-click title to maximize.\n');
  else if (cmd === 'pwd') { if(args.some(a=>a!=='-L'&&a!=='-P'))throw new Error('pwd: unsupported argument'); print(cwd+'\n'); }
  else if (cmd === 'cd') {
    if(args.length>1)throw new Error('cd: too many arguments');
    const path=args[0]==='-'?previousCwd:resolvePath(!args[0]||args[0]==='~'?HOME_DIRECTORY:args[0]);
    if(!await directoryExists(path))throw new Error(`cd: ${args[0]}: ${files[path]!==undefined?'Not a directory':'No such file or directory'}`);
    previousCwd=cwd;cwd=path;if(args[0]==='-')print(cwd+'\n');
  }
  else if (cmd === 'clear') terminal.clear();
  else if (cmd === 'vi' || cmd === 'vim' || cmd === 'edit') await edit(args[0] || 'untitled.c');
  else if (cmd === 'licenses') {
    const notices={...payload.platformLicenses,...payload.packageLicenses};
    if(!args.length)print(Object.entries(notices).map(([name,url])=>name+': '+new URL(url,location.href).href).join('\n')+'\n');
    else for(const name of args){
      if(!notices[name])throw new Error('Unknown license notice: '+name);
      const response=await fetch(new URL(notices[name],location.href));
      if(!response.ok)throw new Error('License download failed: '+response.status);
      print(await response.text());
    }
  }
  else if (cmd === 'fg') { const proc=processes.get(Number(args[0])); if(!proc||proc.state!=='running')throw new Error('No running process with that PID'); return proc.pid; }
  else if (cmd === 'cat') { const path=resolvePath(args[0]||'');const bytes=payload.hosted&&busybox?.worker?await busybox.read(path):null;const s = payload.hosted&&busybox?.worker?(bytes?new TextDecoder().decode(bytes):undefined):files[path]; if (s === undefined) throw new Error('File not found'); print(s + '\n'); }
  else if (cmd === 'ls') {
    const disk=payload.hosted&&busybox?.worker?await busybox.metadata():null;
    print(listDirectory(args,disk?disk.files:[...Object.keys(files),...executables.keys()],cwd,disk?.directories));
  }
  else if (cmd === 'sh') { if(!payload.hosted||!busybox?.worker)busybox=new BusyBoxSession(payload,ui,terminal);window.shell.busybox=busybox;return busybox.start(args); }
  else if (cmd === 'ps') print([...processes.values()].map(p => `${p.pid}\t${p.state}\t${p.name}`).join('\n') + '\n');
  else if (cmd === 'kill') { if (!kill(args[0])) throw new Error('No running process with that PID'); }
  else if (typeof DISTRIBUTION_COMMANDS!=='undefined'&&Object.hasOwn(DISTRIBUTION_COMMANDS,cmd)) return DISTRIBUTION_COMMANDS[cmd](interactive);
  else if (cmd === 'cc') {
    const inputs = [], includes = []; let output = 'a.out';
    for (let i = 0; i < args.length; i++) {
      const a = args[i];
      if (a === '-o' || a === '-I') { const value = args[++i]; if (!value) throw new Error(`${a} needs an argument`); if (a === '-o') output = value; else includes.push(value); }
      else if (a.startsWith('-I')) includes.push(a.slice(2));
      else if (a.startsWith('-')) throw new Error(`Unsupported compiler option ${a}`);
      else inputs.push(a);
    }
    if (!inputs.length) throw new Error('cc needs a C source file');
    return build(inputs, output, includes);
  } else return run(cmd, args, interactive&&!background?null:'').pid;
}
ui = new ShellTerminal(terminal, command, save);
window.addEventListener('gucos-ready',()=>{$('new-terminal').disabled=false;$('new-agent').disabled=false;});
$('new-terminal').onclick=()=>busybox?.newTerminal().catch(error=>{$('status').textContent=error.message;});
$('new-agent').onclick=()=>openAgentVT();
$('terminal-focus').onclick=()=>VirtualTerminal.select(null);
$('import-files').onclick=()=>{
  if(!busybox?.worker){print('Start the shell before importing files.\n');return;}
  $('import-picker').click();
};
$('import-picker').onchange=async event=>{
  const chosen=[...event.target.files];event.target.value='';
  try{
    for(const file of chosen){
      const name=file.name.replace(/[\/\\\x00-\x1f]/g,'_');if(!name||name==='.'||name==='..')continue;
      let path='/home/user/Downloads/'+name,index=1;
      while(await busybox.metadata(path))path='/home/user/Downloads/'+(index++)+'-'+name;
      await busybox.write(path,new Uint8Array(await file.arrayBuffer()));
      print('Imported '+path+'\n');
    }
    $('status').textContent='Imported '+chosen.length+' file(s) to Downloads';
  }catch(error){$('status').textContent='Import failed: '+error.message;print('Import failed: '+error.message+'\n');}
};

$('export-file').onclick=async()=>{
  if(!busybox?.worker){print('Start the shell before exporting files.\n');return;}
  const path=prompt('File to download (full path):','/home/user/');
  if(!path)return;
  try{
    const metadata=await busybox.metadata(path);
    if(!metadata||(metadata.mode&0xf000)!==0x8000)throw new Error('Choose an existing regular file.');
    const bytes=await busybox.read(path);if(!bytes)throw new Error('The file is no longer available.');
    const url=URL.createObjectURL(new Blob([bytes]));
    const link=document.createElement('a');link.href=url;link.download=path.split('/').pop();link.click();
    setTimeout(()=>URL.revokeObjectURL(url),60000);
  }catch(error){$('status').textContent='Export failed: '+error.message;print('Export failed: '+error.message+'\n');}
};

$('reset-install').onclick=()=>{const url=new URL(location.href);url.searchParams.set('reset','1');location.assign(url);};
print('\x1b[1;38;5;78mgucos\x1b[0m  C in your browser\n\n');
print('vi hello.c              edit a file  (:wq saves and exits)\ncc hello.c -o hello      compile\n./hello                 run\npwd · cd · ls            navigate files\nhelp                    commands and keys\n\n');
$('status').textContent = `${typeof WebAssembly.Suspending === 'function' ? 'JSPI' : 'JSPI unavailable'} · ${navigator.gpu ? 'WebGPU' : 'WebGPU unavailable'}`;
window.shell = { command, build, run, kill, files, executables, processes, terminal, ui, get cwd(){return cwd;}, packageKeys:payload.packageKeys||{}, toolchain:payload.toolchain, ready:true };
terminal.focus();
if(crossOriginIsolated&&new URLSearchParams(location.search).get('shell')!=='bootstrap'){
  ui.busy=true;
  const requestedApp=new URLSearchParams(location.search).get('app');
  command(typeof distributionStartupCommand==='function'?distributionStartupCommand(requestedApp):'sh',false,true).catch(error=>print(`BusyBox: ${error.message}\n`)).finally(()=>{ui.busy=false;ui.prompt();});
}else{startup.hide();print('Bootstrap fallback shell (private process snapshots).\n');ui.prompt();}

const agentViews=new Map();
function openAgentVT(threadId=''){
  if(!busybox?.ready){$('status').textContent='Wait for gucOS to finish starting.';return;}
  if(agentViews.has(threadId)){agentViews.get(threadId).focus();return;}
  const frame=document.createElement('iframe');frame.className='agent-frame';frame.title='Agent conversations';
  frame.src=new URL(payload.agentHTMLURL,location.href).href+(threadId?'#'+encodeURIComponent(threadId):'');
  const win=new VirtualTerminal(frame,()=>{},()=>{},()=>terminal.focus());win.title.textContent=threadId?'Agent '+threadId.slice(0,6):'Agents';win.show();agentViews.set(threadId,win);
  frame.addEventListener('load',()=>{frame.contentWindow.addEventListener('keydown',e=>{if(e.altKey&&/^Digit[1-9]$/.test(e.code)){e.preventDefault();window.dispatchEvent(new KeyboardEvent('keydown',{code:e.code,altKey:true}));}});});
  return win;
}
window.shell.openAgent=openAgentVT;
window.shell.agentBridge={
  session:()=>busybox,
  open:openAgentVT,
  title:(id,title)=>{const vt=agentViews.get(id);if(vt)vt.title.textContent=title;},
  close:id=>{const vt=agentViews.get(id);if(vt){vt.destroy();agentViews.delete(id);}},
};
