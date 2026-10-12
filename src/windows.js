// Each process owns its transferred canvas and GPU devices. The page only
// selects a VT; it never composites application windows or brokers a GPU.
class VirtualTerminal {
  static active = null;
  static terminals = new Set();
  static select(vt) {
    if (this.active === vt) return;
    this.active?.canvas.blur();
    if (document.pointerLockElement) document.exitPointerLock();
    this.active = vt;
    document.getElementById('terminal').hidden = !!vt;
    document.getElementById('terminal-focus').setAttribute('aria-selected',String(!vt));
    for (const item of this.terminals) {
      const active = item === vt && !item.programHidden;
      item.element.hidden = !active;
      item.task.setAttribute('aria-selected', String(active));
      item.visibility?.(active && !document.hidden);
    }
    if (vt) {vt.canvas.focus();vt.task.scrollIntoView({block:'nearest',inline:'nearest'});}
    else window.shell?.terminal.focus();
  }
  constructor(canvas, close, resize, focusTerminal) {
    Object.assign(this,{canvas,resize,focusTerminal});
    this.element=document.createElement('section');this.element.className='vt';this.element.hidden=true;
    this.element.append(canvas);document.getElementById('screens').append(this.element);
    this.task=document.createElement('button');this.task.hidden=true;this.task.onclick=()=>this.focus();
    this.task.setAttribute('role','tab');document.getElementById('tasks').append(this.task);
    this.title=this.task;this.geometry={};
    this.close=close;VirtualTerminal.terminals.add(this);
    this.observer=new ResizeObserver(()=>this.requestResize());this.observer.observe(this.element);
  }
  requestResize() {
    if (!this.shown || !this.resizable || this.element.hidden) return;
    const r=this.element.getBoundingClientRect();const w=Math.max(1,Math.round(r.width)),h=Math.max(1,Math.round(r.height));
    if(this.lastW===w&&this.lastH===h)return;this.lastW=w;this.lastH=h;this.resize(w,h);
  }
  apply() {}
  focus(){if(this.shown&&!this.programHidden)VirtualTerminal.select(this);}
  show(event={}) {
    if(event.title)this.title.textContent=event.title;
    if(event.resizable!==undefined)this.resizable=event.resizable;
    if(event.hidden!==undefined)this.programHidden=event.hidden;
    if(!this.shown){this.shown=true;this.task.hidden=!!this.programHidden;if(!this.programHidden)this.focus();}
    this.requestResize();
  }
  setVisible(visible){this.programHidden=!visible;this.task.hidden=!visible;if(!visible&&VirtualTerminal.active===this)VirtualTerminal.select(null);else if(visible)this.focus();}
  destroy(){this.observer.disconnect();VirtualTerminal.terminals.delete(this);if(VirtualTerminal.active===this)VirtualTerminal.select(null);this.element.remove();this.task.remove();}
}
window.addEventListener('keydown',event=>{
  if(!event.altKey||!/^Digit[1-9]$/.test(event.code))return;
  event.preventDefault();event.stopImmediatePropagation();const n=Number(event.code.slice(5))-1;
  VirtualTerminal.select(n===0?null:[...VirtualTerminal.terminals].filter(v=>v.shown&&!v.programHidden)[n-1]||null);
},true);
document.addEventListener('visibilitychange',()=>{for(const vt of VirtualTerminal.terminals)vt.visibility?.(vt===VirtualTerminal.active&&!document.hidden);});
