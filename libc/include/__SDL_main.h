
#pragma once
#include <SDL.h>

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]);
SDL_AppResult SDL_AppIterate(void *appstate);
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event);
void SDL_AppQuit(void *appstate, SDL_AppResult result);

void exit(int status);

static void *__sdl_main_appstate;
static int __sdl_main_status;

/* Read by the host after the frame loop stops (SDL_Quit below cleared the
   frame callback): the process exit status the callbacks chose. Exported so
   the runtime can distinguish SDL_APP_FAILURE (1) from SUCCESS (0) without
   an in-frame exit() — Dawn-tier teardown must drain pending GPU work
   BEFORE the exit handshake (WM.md spike S3), which exit() would skip. */
int __sdl_app_result(void) { return __sdl_main_status; }
__export __sdl_app_result = __sdl_app_result;

static void __sdl_main_finish(SDL_AppResult result) {
    __sdl_main_status = (result == SDL_APP_SUCCESS) ? 0 : 1;
    SDL_AppQuit(__sdl_main_appstate, result);
    /* SDL3's SDL_QuitMainCallbacks calls SDL_Quit after SDL_AppQuit. Here it
       also clears the frame callback, which ends the host frame loop; the
       host then drains pending GPU work and exits with __sdl_app_result(). */
    SDL_Quit();
}

static void __sdl_main_frame(void) {
    SDL_Event __sdl_main_ev;
    SDL_AppResult r;
    while (SDL_PollEvent(&__sdl_main_ev)) {
        r = SDL_AppEvent(__sdl_main_appstate, &__sdl_main_ev);
        if (r != SDL_APP_CONTINUE) { __sdl_main_finish(r); return; }
    }
    r = SDL_AppIterate(__sdl_main_appstate);
    if (r != SDL_APP_CONTINUE) __sdl_main_finish(r);
}

int main(int argc, char **argv) {
    SDL_AppResult r = SDL_AppInit(&__sdl_main_appstate, argc, argv);
    if (r != SDL_APP_CONTINUE) {
        /* SDL3: AppQuit runs even when AppInit itself terminates the app. */
        SDL_AppQuit(__sdl_main_appstate, r);
        SDL_Quit();
        return (r == SDL_APP_SUCCESS) ? 0 : 1;
    }
    __setAnimationFrameFunc(__sdl_main_frame);
    return 0;   /* the host frame loop drives __sdl_main_frame from here */
}
  