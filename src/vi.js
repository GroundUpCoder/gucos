// A small vi-style editor in xterm's alternate screen, backed by the shell VFS.
class ViEditor {
  constructor(term, path, text, save, close) {
    Object.assign(this, {term,path,saveFile:save,onClose:close});
    this.lines = text.split('\n'); this.row=0; this.col=0; this.top=0; this.left=0;
    this.mode='normal'; this.saved=text; this.undo=[]; this.redo=[]; this.register=[]; this.pending=''; this.count=''; this.message=''; this.search='';
    term.write('\x1b[?1049h\x1b[?25h'); this.render();
  }
  snapshot() { return {lines:[...this.lines],row:this.row,col:this.col}; }
  checkpoint() { this.undo.push(this.snapshot()); if(this.undo.length>200)this.undo.shift(); this.redo=[]; }
  restore(state) { this.lines=state.lines; this.row=state.row; this.col=state.col; }
  get text() { return this.lines.join('\n'); }
  get dirty() { return this.text!==this.saved; }
  clamp() { this.row=Math.max(0,Math.min(this.row,this.lines.length-1)); this.col=Math.max(0,Math.min(this.col,Math.max(0,this.lines[this.row].length-(this.mode==='insert'?0:1)))); }
  safe(s) { return s.replace(/\t/g,'    ').replace(/[\x00-\x08\x0b-\x1f\x7f]/g,'?'); }
  render() {
    if(this.closed)return;
    this.clamp(); const height=Math.max(1,this.term.rows-2), width=this.term.cols;
    if(this.row<this.top)this.top=this.row; if(this.row>=this.top+height)this.top=this.row-height+1;
    const column=this.safe(this.lines[this.row].slice(0,this.col)).length;
    if(column<this.left)this.left=column; if(column>=this.left+width)this.left=column-width+1;
    let out='\x1b[?25l\x1b[H';
    for(let y=0;y<height;y++) {
      const line=this.lines[this.top+y];
      out+='\x1b[2K'+(line===undefined?'\x1b[34m~\x1b[0m':this.safe(line).slice(this.left,this.left+width));
      out+='\r\n';
    }
    const status=` ${this.mode.toUpperCase()}  ${this.path}${this.dirty?' [+]':''}   ${this.row+1}:${this.col+1}   :w write  :q quit`;
    out+='\x1b[2K\x1b[7m'+this.safe(status).slice(0,width).padEnd(width)+'\x1b[0m\r\n\x1b[2K';
    out+=this.safe(this.mode==='command'?':'+this.command:this.mode==='search'?'/'+this.command:this.message).slice(0,width);
    if(this.mode==='command'||this.mode==='search')out+=`\x1b[${this.term.rows};${Math.min(width,this.command.length+2)}H`;
    else out+=`\x1b[${this.row-this.top+1};${column-this.left+1}H`;
    out+=(this.mode==='insert'?'\x1b[6 q':'\x1b[2 q')+'\x1b[?25h'; this.term.write(out);
  }
  insert(text) {
    const line=this.lines[this.row], chunks=text.replace(/\r\n?/g,'\n').split('\n');
    const prefix=line.slice(0,this.col), suffix=line.slice(this.col);
    this.lines.splice(this.row,1,...chunks.map((s,i)=>(i===0?prefix:'')+s+(i===chunks.length-1?suffix:'')));
    this.row+=chunks.length-1; this.col=chunks.length===1?prefix.length+chunks[0].length:chunks.at(-1).length;
  }
  paste(text) { if(this.mode!=='insert'){this.message='Press i to insert pasted text';this.render();return;} this.insert(text);this.render(); }
  write(path=this.path) {
    this.path=normalize(path); this.saveFile(this.path,this.text); this.saved=this.text;
    this.message=`"${this.path}" ${this.lines.length} lines written`;
  }
  close() { this.closed=true; this.term.write('\x1b[?1049l\x1b[2 q'); this.onClose(); }
  ex() {
    const cmd=this.command.trim(); this.mode='normal';
    if(/^(wq|x)(\s|$)/.test(cmd)){this.write(cmd.split(/\s+/)[1]||this.path);this.close();return;}
    if(/^w(\s|$)/.test(cmd))this.write(cmd.slice(1).trim()||this.path);
    else if(cmd==='q!') {this.close();return;}
    else if(cmd==='q') {if(this.dirty)this.message='Unsaved changes. Use :wq or :q!';else {this.close();return;}}
    else if(/^\d+$/.test(cmd)){this.row=Number(cmd)-1;this.col=0;}
    else this.message='Commands: :w [file], :q, :q!, :wq, :x, :NUMBER';
  }
  find() {
    if(!this.search)return;
    for(let n=0;n<=this.lines.length;n++) {
      const row=(this.row+n)%this.lines.length;
      const col=this.lines[row].indexOf(this.search,n===0?this.col+1:0);
      if(col>=0){this.row=row;this.col=col;return;}
    }
    this.message=`Pattern not found: ${this.search}`;
  }
  key(key) {
    this.message='';
    if(this.mode==='command'||this.mode==='search') {
      if(key==='Escape'){this.mode='normal';this.command='';}
      else if(key==='Enter'){if(this.mode==='command')this.ex();else{this.search=this.command;this.mode='normal';this.find();}}
      else if(key==='Backspace')this.command=this.command.slice(0,-1);
      else if(key.length===1&&key>=' ')this.command+=key;
      this.render();return;
    }
    if(this.mode==='insert') {
      if(key==='Escape'){this.mode='normal';this.col=Math.max(0,this.col-1);}
      else if(key==='Backspace') {if(this.col){this.lines[this.row]=this.lines[this.row].slice(0,this.col-1)+this.lines[this.row].slice(this.col);this.col--;}else if(this.row){this.col=this.lines[this.row-1].length;this.lines.splice(this.row-1,2,this.lines[this.row-1]+this.lines[this.row]);this.row--;}}
      else if(key==='Delete'){this.lines[this.row]=this.lines[this.row].slice(0,this.col)+this.lines[this.row].slice(this.col+1);}
      else if(key==='Enter')this.insert('\n');
      else if(key==='Tab')this.insert('  ');
      else if(key==='ArrowLeft')this.col--;
      else if(key==='ArrowRight')this.col++;
      else if(key==='ArrowUp')this.row--;
      else if(key==='ArrowDown')this.row++;
      else if(key.length===1&&key>=' ')this.insert(key);
      this.render();return;
    }
    if(key==='Escape'){this.pending='';this.count='';this.render();return;}
    if(/^\d$/.test(key)&&(key!=='0'||this.count)){this.count+=key;return;}
    const n=Math.min(10000,Number(this.count)||1);
    const pending=this.pending; this.pending='';
    if(pending) {
      if(pending==='g'&&key==='g'){this.row=this.count?n-1:0;this.col=0;}
      else if((pending==='d'||pending==='y')&&key===pending){
        this.register=this.lines.slice(this.row,this.row+n);
        if(pending==='d'){this.checkpoint();this.lines.splice(this.row,n);if(!this.lines.length)this.lines=[''];this.col=0;}
      } else if(pending==='d'&&key==='w'){
        this.checkpoint();const rest=this.lines[this.row].slice(this.col);const len=(rest.match(/^(?:\w+|[^\w\s]+)?\s*/)||[''])[0].length||1;
        this.lines[this.row]=this.lines[this.row].slice(0,this.col)+rest.slice(len);
      }
      this.count='';this.render();return;
    }
    if(['d','y','g'].includes(key)){this.pending=key;return;}
    const hadCount=!!this.count;this.count='';
    if(key==='i'||key==='a'||key==='I'||key==='A'||key==='o'||key==='O'){
      this.checkpoint();this.mode='insert';
      if(key==='a')this.col=Math.min(this.col+1,this.lines[this.row].length);
      if(key==='I')this.col=this.lines[this.row].search(/\S|$/);
      if(key==='A')this.col=this.lines[this.row].length;
      if(key==='o'||key==='O'){if(key==='o')this.row++;this.lines.splice(this.row,0,'');this.col=0;}
    } else if(key==='h'||key==='ArrowLeft')this.col-=n;
    else if(key==='l'||key==='ArrowRight')this.col+=n;
    else if(key==='j'||key==='ArrowDown')this.row+=n;
    else if(key==='k'||key==='ArrowUp')this.row-=n;
    else if(key==='0'||key==='Home')this.col=0;
    else if(key==='$'||key==='End')this.col=this.lines[this.row].length-1;
    else if(key==='G'){this.row=hadCount?n-1:this.lines.length-1;this.col=0;}
    else if(key==='w'){for(let i=0;i<n;i++){const rest=this.lines[this.row].slice(this.col);const length=(rest.match(/^(?:\w+|[^\w\s]+)?\s*/)||[''])[0].length||1;this.col+=length;if(this.col>=this.lines[this.row].length&&this.row<this.lines.length-1){this.row++;this.col=0;}}}
    else if(key==='b'){for(let i=0;i<n;i++){if(this.col===0&&this.row){this.row--;this.col=this.lines[this.row].length;}const prefix=this.lines[this.row].slice(0,this.col);this.col=Math.max(0,prefix.replace(/\s+$/,'').search(/(?:\w+|[^\w\s]+)$/));}}
    else if(key==='x'||key==='Delete'){this.checkpoint();this.lines[this.row]=this.lines[this.row].slice(0,this.col)+this.lines[this.row].slice(this.col+n);}
    else if(key==='p'&&this.register.length){this.checkpoint();this.lines.splice(this.row+1,0,...this.register);this.row++;this.col=0;}
    else if(key==='u'&&this.undo.length){this.redo.push(this.snapshot());this.restore(this.undo.pop());}
    else if(key==='CtrlR'&&this.redo.length){this.undo.push(this.snapshot());this.restore(this.redo.pop());}
    else if(key==='PageDown'||key==='CtrlD')this.row+=Math.max(1,Math.floor(this.term.rows/2));
    else if(key==='PageUp'||key==='CtrlU')this.row-=Math.max(1,Math.floor(this.term.rows/2));
    else if(key===':'){this.mode='command';this.command='';}
    else if(key==='/'){this.mode='search';this.command='';}
    else if(key==='n')this.find();
    this.render();
  }
}
