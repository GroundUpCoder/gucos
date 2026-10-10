import assert from 'node:assert/strict';
import {browserSession} from '../tools/browser.mjs';
const s=await browserSession({gpu:true});
try {
 await s.command('true');
 await s.page.evaluate(async()=>shell.busybox.write('/tmp/vt.c',new TextEncoder().encode(`
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
 SDL_Init(SDL_INIT_VIDEO);
 SDL_Window *w=SDL_CreateWindow(argv[1],240,160,SDL_WINDOW_RESIZABLE|SDL_WINDOW_FULLSCREEN);
 if(!w){puts(SDL_GetError());return 1;}
 if(SDL_CreateWindow("second",20,20,0)){puts("SECOND-WINDOW-ACCEPTED");return 2;}
 printf("READY-%s\\n",argv[1]);fflush(stdout);
 for(int n=0;;n++){
  SDL_Event e;while(SDL_PollEvent(&e)){
   if(e.type==SDL_EVENT_QUIT)return 0;
   if(e.type==SDL_EVENT_KEY_DOWN&&e.key.key=='q')return 0;
   if(e.type==SDL_EVENT_KEY_DOWN&&e.key.key=='x')abort();
   if(e.type==SDL_EVENT_KEY_UP){printf("UP-%s-%d\\n",argv[1],e.key.key);fflush(stdout);}
   if(e.type==SDL_EVENT_MOUSE_BUTTON_UP){printf("BUTTON-UP-%s\\n",argv[1]);fflush(stdout);}
  }
  SDL_Surface *v=SDL_GetWindowSurface(w);if(!v)return 3;
  SDL_FillSurfaceRect(v,0,SDL_MapSurfaceRGB(v,n%255,argc>2?180:40,90));
  if(!SDL_UpdateWindowSurface(w))return 4;
  if(n%10==0){printf("FRAME-%s-%d\\n",argv[1],n);fflush(stdout);}
 }
}`)));
 console.log(await s.command('cc /tmp/vt.c -o /tmp/vt\n/tmp/vt first &\n/tmp/vt second extra &'));
 await s.page.waitForFunction(()=>[...shell.busybox.windows.values()].filter(v=>v.shown).length===2);
 const visible=()=>s.page.locator('.vt:visible');
 await s.page.waitForTimeout(1500);
 assert.equal(await visible().count(),1);
 assert.equal(await s.page.locator('#terminal').isVisible(),false);
 await s.page.getByRole('tab',{name:/first/}).click();
 await visible().locator('canvas').focus();await s.page.keyboard.down('b');
 const box=await visible().locator('canvas').boundingBox();await s.page.mouse.move(box.x+20,box.y+20);await s.page.mouse.down();
 const releaseStart=await s.page.evaluate(()=>shell.busybox.log.length);
 await s.page.getByRole('tab',{name:/second/}).evaluate(button=>button.click());
 await s.page.waitForFunction(n=>{const tail=shell.busybox.log.slice(n);return tail.includes('UP-first-98')&&tail.includes('BUTTON-UP-first');},releaseStart);
 await s.page.keyboard.up('b');await s.page.mouse.up();
 await s.page.setViewportSize({width:700,height:500});
 await s.page.waitForTimeout(500);
 const size=await visible().locator('canvas').evaluate(c=>[+c.dataset.logicalWidth,+c.dataset.logicalHeight]);
 // The resize event updates the backing canvas in the process; its page message
 // must also update logical input geometry.
 assert.deepEqual(size,[700,468]);
 await s.page.locator('#terminal-focus').click();assert.equal(await visible().count(),0);
 const before=await s.page.evaluate(()=>shell.busybox.log);
 await s.page.waitForTimeout(2200);
 const after=await s.page.evaluate(()=>shell.busybox.log);
 for(const name of ['first','second']){
  const frames=log=>[...log.matchAll(new RegExp('FRAME-'+name+'-(\\d+)','g'))].map(m=>+m[1]).at(-1)||0;
  const diff=frames(after)-frames(before);assert(diff>=10&&diff<=30,`${name}: hidden frames ${diff}`);
 }
 assert.match(await s.command('echo BACKGROUND-TERMINAL-PASS'),/BACKGROUND-TERMINAL-PASS/);
 await s.page.getByRole('tab',{name:/first/}).click();await visible().locator('canvas').press('x');
 await s.page.waitForFunction(()=>[...shell.busybox.windows.values()].filter(v=>v.shown).length===1);
 await s.page.getByRole('tab',{name:/second/}).click();await visible().locator('canvas').press('q');
 await s.page.waitForFunction(()=>[...shell.busybox.windows.values()].filter(v=>v.shown).length===0);
 assert.equal(await s.page.locator('#terminal').isVisible(),true);
 assert.match(await s.command('test ! -e /run/wm.sock\necho RECOVERY-PASS'),/RECOVERY-PASS/);
 console.log('PASS: two SDL VTs, single visible surface, terminal switching, key release, resize, bounded hidden progress, crash and exit recovery; no WM socket');
} finally {await s.close();}
