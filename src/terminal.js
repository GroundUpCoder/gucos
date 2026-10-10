class ShellTerminal {
  constructor(term, execute, save) {
    Object.assign(this,{term,execute,save});
    this.line='';this.cursor=0;this.history=[];this.historyIndex=0;this.busy=false;this.promptVisible=false;this.transcript='';this.deferred='';this.pending='';
    term.onData(data=>this.data(data));
    term.onResize(()=>{if(this.editor)this.editor.render();else if(!this.busy)this.prompt();});
    term.attachCustomKeyEventHandler(event=>{
      // Handle clear locally, including while a BusyBox process owns the tty.
      if(event.metaKey&&!event.ctrlKey&&!event.altKey&&!event.shiftKey&&event.code==='KeyK'){
        if(event.type==='keydown')term.clear();
        event.preventDefault();
        return false;
      }
      if(event.ctrlKey&&event.shiftKey&&event.code==='KeyC')return false;
      if((event.ctrlKey||event.metaKey)&&event.code==='KeyV')return false;
      return true;
    });
    term.write('\x1b[?2004h');
  }
  print(text) {
    this.transcript=(this.transcript+text).slice(-150000);
    if(this.editor){this.deferred=(this.deferred+text).slice(-100000);return;}
    if(this.promptVisible)this.term.write('\r\x1b[2K');
    this.term.write(text);
    if(this.promptVisible){if(!text.endsWith('\n'))this.term.write('\r\n');this.prompt();}
  }
  prompt() {
    if(this.editor||this.busy)return;
    this.promptVisible=true;
    const location=cwd===HOME_DIRECTORY?'~':cwd.startsWith(HOME_DIRECTORY+'/')?'~'+cwd.slice(HOME_DIRECTORY.length):cwd;
    const label=location+' $ ';
    const width=Math.max(1,this.term.cols-label.length-2), start=Math.max(0,this.cursor-width+1);
    const visible=this.line.slice(start,start+width);
    this.term.write('\r\x1b[2K\x1b[38;5;78m'+(start?'… $ ':label)+'\x1b[0m'+visible+'\r'+`\x1b[${(start?4:label.length)+this.cursor-start}C`);
  }
  edit(path,text) {
    this.promptVisible=false;
    this.editor=new ViEditor(this.term,path,text,this.save,()=>{
      this.editor=null;
      const output=this.deferred;this.deferred='';if(output)this.term.write(output);
      this.prompt();
    });
  }
  async submit() {
    const line=this.line;this.line='';this.cursor=0;this.promptVisible=false;
    this.term.write('\r\n');
    if(!line.trim()){this.prompt();return;}
    this.history.push(line);this.historyIndex=this.history.length;
    this.busy=true;this.foreground=null;this.stdinLine='';
    try {
      const result=await this.execute(line,false,true);
      const proc=typeof result==='number'?processes.get(result):null;
      if(proc&&!line.trimEnd().endsWith('&')) {
        this.foreground=proc;
        await Promise.race([proc.done,proc.visible]);
      }
    } catch(error){this.print(`error: ${error.message}\n`);}
    finally {this.foreground=null;this.busy=false;this.prompt();}
  }
  key(key) {
    if(this.editor){this.editor.key(key==='CtrlC'?'Escape':key);return;}
    if(key==='CtrlC'){
      if(this.foreground?.state==='running')kill(this.foreground.pid);
      this.term.write('^C\r\n');this.line='';this.cursor=0;if(!this.busy)this.prompt();return;
    }
    if(this.busy) {
      const p=this.foreground;if(!p||p.state!=='running')return;
      if(key==='Enter'){p.worker.postMessage({type:'stdin',text:this.stdinLine+'\n'});this.stdinLine='';this.term.write('\r\n');}
      else if(key==='CtrlD'){p.worker.postMessage({type:'stdin',text:this.stdinLine,eof:!this.stdinLine});this.stdinLine='';}
      else if(key==='Backspace'){if(this.stdinLine){this.stdinLine=this.stdinLine.slice(0,-1);this.term.write('\b \b');}}
      else if(key.length===1&&key>=' '){this.stdinLine+=key;this.term.write(key);}
      return;
    }
    if(key==='Enter'){this.submit();return;}
    if(key==='Backspace'){if(this.cursor){this.line=this.line.slice(0,this.cursor-1)+this.line.slice(this.cursor);this.cursor--;}}
    else if(key==='Delete')this.line=this.line.slice(0,this.cursor)+this.line.slice(this.cursor+1);
    else if(key==='ArrowLeft')this.cursor=Math.max(0,this.cursor-1);
    else if(key==='ArrowRight')this.cursor=Math.min(this.line.length,this.cursor+1);
    else if(key==='Home'||key==='CtrlA')this.cursor=0;
    else if(key==='End'||key==='CtrlE')this.cursor=this.line.length;
    else if(key==='CtrlU'){this.line=this.line.slice(this.cursor);this.cursor=0;}
    else if(key==='CtrlK')this.line=this.line.slice(0,this.cursor);
    else if(key==='CtrlL'){this.term.clear();}
    else if(key==='ArrowUp'){if(this.historyIndex===this.history.length)this.draft=this.line;this.historyIndex=Math.max(0,this.historyIndex-1);this.line=this.history[this.historyIndex]||'';this.cursor=this.line.length;}
    else if(key==='ArrowDown'){this.historyIndex=Math.min(this.history.length,this.historyIndex+1);this.line=this.historyIndex===this.history.length?(this.draft||''):this.history[this.historyIndex];this.cursor=this.line.length;}
    else if(key==='Tab') {
      const before=this.line.slice(0,this.cursor), token=before.split(/\s/).at(-1);
      const commands=['help','pwd','cd','sh','cc','vi','ls','cat','ps','kill','clear','fg','licenses',...Object.keys(typeof DISTRIBUTION_COMMANDS==='undefined'?{}:DISTRIBUTION_COMMANDS)];
      const parent=token.includes('/')?token.slice(0,token.lastIndexOf('/')+1):'';
      const base=resolvePath(parent||'.');
      const paths=[...Object.keys(files),...executables.keys(),...SYSTEM_DIRECTORIES].filter(s=>s.startsWith(base==='/'?'/':base+'/')).map(s=>{const relative=s.slice(base==='/'?1:base.length+1),name=relative.split('/')[0];return parent+name+(relative.includes('/')||SYSTEM_DIRECTORIES.includes(normalize(base+'/'+name))?'/':'');});
      const matches=[...new Set(before===token?[...commands,...paths]:paths)].filter(s=>s.startsWith(token));
      if(matches.length===1){const add=matches[0].slice(token.length);this.line=this.line.slice(0,this.cursor)+add+this.line.slice(this.cursor);this.cursor+=add.length;}
      else if(matches.length){this.print(matches.join('  ')+'\n');}
    } else if(key.length===1&&key>=' '){this.line=this.line.slice(0,this.cursor)+key+this.line.slice(this.cursor);this.cursor++;}
    this.prompt();
  }
  paste(text) {
    if(this.editor){this.editor.paste(text);return;}
    // Pasting at the shell edits one command; never execute embedded newlines.
    text=text.replace(/[\r\n]+/g,' ').replace(/[\x00-\x1f\x7f]/g,'');
    if(this.busy){this.stdinLine+=text;this.term.write(text);}
    else{this.line=this.line.slice(0,this.cursor)+text+this.line.slice(this.cursor);this.cursor+=text.length;this.prompt();}
  }
  data(data) {
    if(this.session){this.session.worker.postMessage({type:'input',text:data});return;}
    this.pending+=data;
    const sequences={'\x1b[A':'ArrowUp','\x1b[B':'ArrowDown','\x1b[C':'ArrowRight','\x1b[D':'ArrowLeft','\x1b[H':'Home','\x1b[F':'End','\x1bOH':'Home','\x1bOF':'End','\x1b[3~':'Delete','\x1b[5~':'PageUp','\x1b[6~':'PageDown'};
    const controls={'\r':'Enter','\n':'Enter','\x7f':'Backspace','\b':'Backspace','\t':'Tab','\x03':'CtrlC','\x04':'CtrlD','\x15':'CtrlU','\x12':'CtrlR','\x01':'CtrlA','\x05':'CtrlE','\x0b':'CtrlK','\x0c':'CtrlL','\x1b':'Escape'};
    while(this.pending){
      if(this.pending.startsWith('\x1b[200~')){const end=this.pending.indexOf('\x1b[201~');if(end<0)return;this.paste(this.pending.slice(6,end));this.pending=this.pending.slice(end+6);continue;}
      const sequence=Object.keys(sequences).find(s=>this.pending.startsWith(s));
      if(sequence){this.key(sequences[sequence]);this.pending=this.pending.slice(sequence.length);continue;}
      if(this.pending.length>1&&Object.keys(sequences).concat('\x1b[200~').some(s=>s.startsWith(this.pending)))return;
      const char=String.fromCodePoint(this.pending.codePointAt(0));this.pending=this.pending.slice(char.length);this.key(controls[char]||char);
    }
  }
}
