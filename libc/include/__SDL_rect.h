
#pragma once
__require_source("__SDL_rect.c");
typedef struct SDL_Point {int x,y;} SDL_Point;
static inline bool SDL_RectEmpty(const SDL_Rect *r){return !r||r->w<=0||r->h<=0;}
static inline bool SDL_RectsEqual(const SDL_Rect *a,const SDL_Rect *b){return a&&b&&a->x==b->x&&a->y==b->y&&a->w==b->w&&a->h==b->h;}
static inline bool SDL_PointInRect(const SDL_Point *p,const SDL_Rect *r){return p&&r&&(Sint64)p->x>=r->x&&(Sint64)p->x<(Sint64)r->x+r->w&&(Sint64)p->y>=r->y&&(Sint64)p->y<(Sint64)r->y+r->h;}
bool SDL_HasRectIntersection(const SDL_Rect *a,const SDL_Rect *b);
bool SDL_GetRectIntersection(const SDL_Rect *a,const SDL_Rect *b,SDL_Rect *result);
bool SDL_GetRectUnion(const SDL_Rect *a,const SDL_Rect *b,SDL_Rect *result);
bool SDL_GetRectAndLineIntersection(const SDL_Rect *r,int *x1,int *y1,int *x2,int *y2);
bool SDL_GetRectEnclosingPoints(const SDL_Point *points,int count,const SDL_Rect *clip,SDL_Rect *result);
