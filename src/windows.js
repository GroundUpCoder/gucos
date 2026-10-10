// Page-owned window chrome; the process retains its OffscreenCanvas and memory.
class DesktopWindow {
  static top = 10;
  static count = 0;
  constructor(canvas, close, resize, focusTerminal) {
    this.canvas = canvas;
    this.resize = resize;
    this.element = document.createElement('section');
    this.element.className = 'screen'; this.element.hidden = true;
    this.element.setAttribute('role', 'dialog');
    this.element.setAttribute('aria-label', 'Process window');
    this.bar = document.createElement('div'); this.bar.className = 'titlebar';
    this.title = document.createElement('span'); this.title.className = 'window-title';
    this.bar.append(this.title);
    const controls = document.createElement('div'); controls.className = 'window-controls';
    const button = (label, text, action) => {
      const b = document.createElement('button'); b.type = 'button'; b.title = label; b.setAttribute('aria-label', label); b.textContent = text;
      b.onclick = e => { e.stopPropagation(); action(); }; controls.append(b); return b;
    };
    button('Minimize', '−', () => { if(this.onMinimize){this.onMinimize();return;} this.element.hidden = true; this.minimized = true; focusTerminal(); });
    button('Maximize or restore', '□', () => this.maximize());
    button('Close process', '×', () => { if(close()!==false){this.destroy();focusTerminal();} });
    this.bar.append(controls);
    this.viewport = document.createElement('div'); this.viewport.className = 'window-content'; this.viewport.append(canvas);
    this.grip = document.createElement('div'); this.grip.className = 'resize-handle'; this.grip.setAttribute('aria-label', 'Resize window');
    this.grip.title = 'Drag to resize';
    this.element.append(this.bar, this.viewport, this.grip); document.getElementById('screens').append(this.element);
    this.task = document.createElement('button'); this.task.hidden = true; this.task.onclick = () => { this.minimized = false; this.element.hidden = false; this.focus(); };
    document.getElementById('tasks').append(this.task);
    const offset = (DesktopWindow.count++ % 7) * 28;
    this.geometry = { x: Math.min(180 + offset, innerWidth / 4), y: 72 + offset, w: Math.min(642, innerWidth - 24), h: Math.min(436, innerHeight - 90) };
    this.apply();
    this.element.addEventListener('pointerdown', () => this.focus(false));
    this.bar.addEventListener('dblclick', e => { if (!e.target.closest('button')) this.maximize(); });
    this.drag(this.bar, false); this.drag(this.grip, true);
    this.observer = new ResizeObserver(() => {
      if (this.element.hidden || !this.resizable || this.kernelGeometry) return;
      const rect = this.viewport.getBoundingClientRect();
      clearTimeout(this.resizeTimer);
      this.resizeTimer = setTimeout(() => resize(Math.max(1, Math.round(rect.width)), Math.max(1, Math.round(rect.height))), 70);
    });
    this.observer.observe(this.viewport);
    this.onViewportResize = () => { if (this.maximized) this.fill(); else { this.clamp(); this.apply(); } this.requestResize();this.onMove?.(this.geometry.x,this.geometry.y); };
    window.addEventListener('resize', this.onViewportResize);
  }
  clamp() {
    const g = this.geometry, top = this.workAreaTop || 0;
    g.w = Math.max(160, Math.min(g.w, innerWidth)); g.h = Math.max(100, Math.min(g.h, innerHeight - 32 - top));
    g.x = Math.max(0, Math.min(g.x, innerWidth - g.w)); g.y = Math.max(top, Math.min(g.y, innerHeight - 32 - g.h));
  }
  apply() { this.clamp(); const g = this.geometry; Object.assign(this.element.style, { left: g.x+'px', top:g.y+'px', width:g.w+'px', height:g.h+'px' }); }
  focus(keyboard = true) {
    if(this.onActivate){this.onActivate();if(keyboard)this.canvas.focus();return;}
    for (const el of document.querySelectorAll('.screen.active')) el.classList.remove('active');
    this.element.classList.add('active'); this.element.style.zIndex = ++DesktopWindow.top;
    if (keyboard) this.canvas.focus();
  }
  show(event = {}) {
    if (event.title) { this.title.textContent = event.title; this.task.textContent = event.title; this.element.setAttribute('aria-label', event.title); }
    if (event.resizable !== undefined) this.resizable = event.resizable;
    if (!this.shown) {
      this.shown = true; this.task.hidden = false;
      this.geometry.w = (event.width || 640) + 2; this.geometry.h = (event.height || 400) + 36; this.apply();
      this.element.hidden = !!event.hidden; this.task.hidden = !!event.hidden;
      if(!event.hidden)this.focus();
    }
  }
  fill() { const top=this.workAreaTop||0;this.geometry = { x:0, y:top, w:innerWidth, h:innerHeight-32-top }; this.apply(); }
  requestResize(){if(this.kernelGeometry&&this.resizable)this.resize(Math.max(1,this.geometry.w-2),Math.max(1,this.geometry.h-36));}
  maximize() {
    if (this.maximized) { this.geometry = this.saved; this.maximized = false; this.apply(); }
    else { this.saved = {...this.geometry}; this.maximized = true; this.fill(); }
    this.requestResize();this.onMove?.(this.geometry.x,this.geometry.y);
    this.focus();
  }
  drag(target, sizing) {
    target.addEventListener('pointerdown', e => {
      if (e.button !== 0 || e.target.closest('button') || this.maximized) return;
      e.preventDefault(); this.focus(false); target.setPointerCapture(e.pointerId);
      const start = { x:e.clientX, y:e.clientY, ...this.geometry };
      const originX = e.clientX, originY = e.clientY;
      const move = ev => {
        const dx = ev.clientX-originX, dy=ev.clientY-originY;
        this.geometry = sizing ? {...start,w:Math.max(240,start.w+dx),h:Math.max(160,start.h+dy)} : {...start,x:start.x+dx,y:start.y+dy};
        this.apply();
        if(sizing)this.requestResize();
        if(!sizing)this.onMove?.(this.geometry.x,this.geometry.y);
      };
      const end = () => { target.removeEventListener('pointermove',move); target.removeEventListener('pointerup',end); target.removeEventListener('pointercancel',end); };
      target.addEventListener('pointermove',move); target.addEventListener('pointerup',end); target.addEventListener('pointercancel',end);
    });
  }
  destroy() { clearTimeout(this.resizeTimer); this.observer.disconnect(); window.removeEventListener('resize',this.onViewportResize); this.element.remove(); this.task.remove(); }
}
