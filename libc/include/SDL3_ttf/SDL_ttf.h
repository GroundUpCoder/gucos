
#pragma once
#include <SDL3/SDL.h>
__require_source("__SDL_ttf.c");

/* The internal TTF_Font holds an FT_Face plus current size/style/hinting/
   kerning state and cached pixel metrics. */
typedef struct TTF_Font TTF_Font;

/* Font style flags (TTF_SetFontStyle / TTF_GetFontStyle). Bold and italic
   are synthesized (embolden / shear) when the face has no such variant —
   same approach as upstream SDL_ttf. */
typedef Uint32 TTF_FontStyleFlags;
#define TTF_STYLE_NORMAL        0x00
#define TTF_STYLE_BOLD          0x01
#define TTF_STYLE_ITALIC        0x02
#define TTF_STYLE_UNDERLINE     0x04
#define TTF_STYLE_STRIKETHROUGH 0x08

/* Hinting (TTF_SetFontHinting / TTF_GetFontHinting). Upstream's
   TTF_HINTING_LIGHT_SUBPIXEL does not exist here (no subpixel output). */
typedef enum TTF_HintingFlags {
    TTF_HINTING_INVALID = -1,
    TTF_HINTING_NORMAL = 0,
    TTF_HINTING_LIGHT = 1,
    TTF_HINTING_MONO = 2,
    TTF_HINTING_NONE = 3
} TTF_HintingFlags;

/* Init is refcounted: TTF_WasInit returns the number of successful inits not
   yet paired with a TTF_Quit. Fonts must be opened after TTF_Init. */
bool TTF_Init(void);
void TTF_Quit(void);
int TTF_WasInit(void);

/* Open 'file' at 'ptsize' (72 DPI, so points == pixels). TrueType faces are
   supported (the baked FreeType module set has no CFF driver — a .otf fails
   with a named error). NULL + SDL error on failure. */
TTF_Font *TTF_OpenFont(const char *file, float ptsize);
void TTF_CloseFont(TTF_Font *font);

bool TTF_SetFontSize(TTF_Font *font, float ptsize);
bool TTF_SetFontSizeDPI(TTF_Font *font, float ptsize, int hdpi, int vdpi);
float TTF_GetFontSize(TTF_Font *font);
bool TTF_GetFontDPI(TTF_Font *font, int *hdpi, int *vdpi);

void TTF_SetFontStyle(TTF_Font *font, TTF_FontStyleFlags style);
TTF_FontStyleFlags TTF_GetFontStyle(const TTF_Font *font);
void TTF_SetFontHinting(TTF_Font *font, TTF_HintingFlags hinting);
TTF_HintingFlags TTF_GetFontHinting(const TTF_Font *font);
/* Kerning defaults ON; it is the 'kern' table only (FT_Get_Kerning), so
   GPOS-only faces (the baked Noto family) report no pairs — as upstream
   SDL_ttf without HarfBuzz. */
void TTF_SetFontKerning(TTF_Font *font, bool enabled);
bool TTF_GetFontKerning(const TTF_Font *font);

/* Pixel metrics at the current size (ascent > 0, descent < 0). */
int TTF_GetFontHeight(const TTF_Font *font);
int TTF_GetFontAscent(const TTF_Font *font);
int TTF_GetFontDescent(const TTF_Font *font);
int TTF_GetFontLineSkip(const TTF_Font *font);
void TTF_SetFontLineSkip(TTF_Font *font, int lineskip);
bool TTF_FontIsFixedWidth(const TTF_Font *font);
const char *TTF_GetFontFamilyName(const TTF_Font *font);
const char *TTF_GetFontStyleName(const TTF_Font *font);

bool TTF_FontHasGlyph(TTF_Font *font, Uint32 ch);
bool TTF_GetGlyphMetrics(TTF_Font *font, Uint32 ch, int *minx, int *maxx, int *miny, int *maxy, int *advance);
bool TTF_GetGlyphKerning(TTF_Font *font, Uint32 previous_ch, Uint32 ch, int *kerning);

/* Measure without rasterizing. 'length' is BYTES, 0 = null-terminated (every
   text-taking call below follows that rule; text is UTF-8, invalid bytes
   decode to U+FFFD). The reported w/h equal the surface the matching
   renderer returns. Wrapped: wrap at word boundaries past wrap_width px;
   wrap_width 0 = wrap only on newlines. */
bool TTF_GetStringSize(TTF_Font *font, const char *text, size_t length, int *w, int *h);
bool TTF_GetStringSizeWrapped(TTF_Font *font, const char *text, size_t length, int wrap_width, int *w, int *h);
bool TTF_MeasureString(TTF_Font *font, const char *text, size_t length, int max_width, int *measured_width, size_t *measured_length);

/* Renderers: a new RGBA32 heap SDL_Surface (SDL_DestroySurface frees), NULL
   + SDL error on failure (including zero-width text). Single-line renderers
   do NOT wrap on newlines. fg/bg alpha 0 is treated as opaque, as upstream. */
SDL_Surface *TTF_RenderText_Solid(TTF_Font *font, const char *text, size_t length, SDL_Color fg);
SDL_Surface *TTF_RenderText_Solid_Wrapped(TTF_Font *font, const char *text, size_t length, SDL_Color fg, int wrapLength);
SDL_Surface *TTF_RenderText_Shaded(TTF_Font *font, const char *text, size_t length, SDL_Color fg, SDL_Color bg);
SDL_Surface *TTF_RenderText_Shaded_Wrapped(TTF_Font *font, const char *text, size_t length, SDL_Color fg, SDL_Color bg, int wrap_width);
SDL_Surface *TTF_RenderText_Blended(TTF_Font *font, const char *text, size_t length, SDL_Color fg);
SDL_Surface *TTF_RenderText_Blended_Wrapped(TTF_Font *font, const char *text, size_t length, SDL_Color fg, int wrap_width);
SDL_Surface *TTF_RenderGlyph_Solid(TTF_Font *font, Uint32 ch, SDL_Color fg);
SDL_Surface *TTF_RenderGlyph_Shaded(TTF_Font *font, Uint32 ch, SDL_Color fg, SDL_Color bg);
SDL_Surface *TTF_RenderGlyph_Blended(TTF_Font *font, Uint32 ch, SDL_Color fg);

/* SDL_ttf shares SDL's per-thread error string (the IMG_GetError pattern). */
const char *TTF_GetError(void);
  