#include <SDL3/SDL.h>
#include <stdio.h>
int main(void){
 if(!SDL_Init(SDL_INIT_VIDEO))return 1;
 if(SDL_CreateWindow("unsupported",30,30,0x2))return 2;
 SDL_Window *w=SDL_CreateWindow("IME contract",320,200,SDL_WINDOW_RESIZABLE);if(!w)return 3;
 if(SDL_SetWindowFullscreen(w,1))return 4;
 SDL_StartTextInput(w);puts("IME-READY");fflush(stdout);
 for(;;){SDL_Event e;while(SDL_PollEvent(&e)){
  if(e.type==SDL_EVENT_KEY_UP){printf("UP:%d:%d:%d\n",e.key.scancode,e.key.mod,SDL_GetModState());fflush(stdout);}
  if(e.type==SDL_EVENT_TEXT_EDITING){printf("EDIT:%s:%d:%d\n",e.edit.text,e.edit.start,e.edit.length);fflush(stdout);}
  if(e.type==SDL_EVENT_TEXT_INPUT){printf("TEXT:%s\n",e.text.text);fflush(stdout);}
  if(e.type==SDL_EVENT_KEY_DOWN&&e.key.key=='q'){SDL_StopTextInput(w);puts("IME-STOPPED");fflush(stdout);}
  if(e.type==SDL_EVENT_KEY_DOWN&&e.key.key=='x')return 0;
 }SDL_Delay(5);}
}
