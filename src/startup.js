// Startup status is a view of worker-reported work, never a simulated timer.
const startup = (() => {
  const panel=document.getElementById('startup'), title=document.getElementById('startup-title');
  const summary=document.getElementById('startup-summary'), bar=document.getElementById('startup-progress');
  const list=document.getElementById('startup-downloads'), toggle=document.getElementById('startup-toggle');
  const retry=document.getElementById('startup-retry');
  let active=true, items=new Map(), rows=new Map(), hideTimer;
  const bytes=n=>n>=1048576?(n/1048576).toFixed(1)+' MB':Math.ceil(n/1024)+' KB';
  function hide(){panel.hidden=true;toggle.setAttribute('aria-expanded','false');}
  function show(){clearTimeout(hideTimer);panel.hidden=false;toggle.setAttribute('aria-expanded','true');}
  toggle.onclick=()=>panel.hidden?show():hide();
  document.getElementById('startup-hide').onclick=hide;
  retry.onclick=()=>location.reload();
  function render(){
    const all=[...items.values()], done=all.filter(x=>x.state==='done').length;
    const text=all.length?`${done} of ${all.length} downloads ready · ${all.length-done} remaining`:'No downloads needed';
    if(summary.textContent!==text)summary.textContent=text;
    bar.max=all.length||1;bar.value=all.length?done:1;
    for(const item of all){
      let row=rows.get(item.id);
      if(!row){row=document.createElement('li');const name=document.createElement('span'), state=document.createElement('span');name.textContent=item.name;row.append(name,state);list.append(row);rows.set(item.id,row);}
      row.dataset.state=item.state;
      row.lastChild.textContent=item.state==='done'?'Ready':item.state==='failed'?'Failed':item.state==='verifying'?'Verifying…':item.state==='downloading'?(item.loaded?`${bytes(item.loaded)}${item.total?' / '+bytes(item.total):''}`:'Downloading…'):'Waiting';
    }
  }
  function update(data){
    if(!active)return;
    if(data.phase){title.textContent=data.phase;toggle.textContent=data.phase;}
    if(data.items){items=new Map(data.items.map(x=>[x.id,{...x,state:'waiting'}]));rows.clear();list.replaceChildren();render();}
    if(data.item){items.set(data.item.id,{...items.get(data.item.id),...data.item});render();}
  }
  function begin(){active=true;clearTimeout(hideTimer);retry.hidden=true;items.clear();rows.clear();list.replaceChildren();title.textContent='Opening your disk…';toggle.textContent='Starting…';summary.textContent='Checking what is needed';bar.removeAttribute('value');show();}
  function finish(){active=false;title.textContent='Your OS is ready';toggle.textContent='Startup ✓';hideTimer=setTimeout(hide,700);}
  function fail(message){if(!active)return;active=false;clearTimeout(hideTimer);title.textContent='Startup could not finish';summary.textContent=message;toggle.textContent='Startup failed';retry.hidden=false;show();}
  window.addEventListener('error',event=>fail(event.message||'An unexpected startup error occurred.'));
  window.addEventListener('unhandledrejection',event=>fail(event.reason?.message||String(event.reason)));
  return {begin,update,finish,fail,hide};
})();
