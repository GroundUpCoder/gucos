
#pragma once
__require_source("__SDL_events.c");
typedef Uint32 SDL_TimerID;
typedef Uint32 (*SDL_TimerCallback)(void *userdata,SDL_TimerID timerID,Uint32 interval);
SDL_TimerID SDL_AddTimer(Uint32 interval,SDL_TimerCallback callback,void *userdata);
bool SDL_RemoveTimer(SDL_TimerID timerID);
void __sdl_cancel_timers(void);
int __sdl_timer_wait_bound(int timeout);
typedef int SDL_SpinLock;
void SDL_LockSpinlock(SDL_SpinLock *lock);
bool SDL_TryLockSpinlock(SDL_SpinLock *lock);
void SDL_UnlockSpinlock(SDL_SpinLock *lock);
typedef bool (*SDL_EventFilter)(void *userdata,SDL_Event *event);
void SDL_SetEventFilter(SDL_EventFilter filter,void *userdata);
bool SDL_GetEventFilter(SDL_EventFilter *filter,void **userdata);
bool SDL_AddEventWatch(SDL_EventFilter filter,void *userdata);
void SDL_RemoveEventWatch(SDL_EventFilter filter,void *userdata);
void SDL_FilterEvents(SDL_EventFilter filter,void *userdata);
void SDL_SetEventEnabled(Uint32 type,bool enabled);
bool SDL_EventEnabled(Uint32 type);
