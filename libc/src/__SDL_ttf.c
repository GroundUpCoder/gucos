
#include <SDL3_ttf/SDL_ttf.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#include <stdlib.h>
#include <string.h>

/* 26.6 fixed-point helpers (FreeType conventions). */
#define TTF_FLOOR(x) ((int)(((x) & -64) / 64))
#define TTF_CEIL(x)  ((int)((((x) + 63) & -64) / 64))

/* Render modes, internal. */
#define TTF_R_SOLID   0
#define TTF_R_SHADED  1
#define TTF_R_BLENDED 2

struct TTF_Font {
    FT_Face face;
    float ptsize;
    int hdpi, vdpi;
    TTF_FontStyleFlags style;
    TTF_HintingFlags hinting;
    int ft_load_target;              /* FT_LOAD_TARGET_* / FT_LOAD_NO_HINTING */
    bool use_kerning;
    /* Pixel metrics at the current size — upstream SDL_ttf's derivation from
       the face's design metrics (scalable branch). */
    int height, ascent, descent, lineskip;
    int line_thickness, underline_top_row, strikethrough_top_row;
};

static FT_Library ttf_library;
static int ttf_init_count;

const char *TTF_GetError(void) { return SDL_GetError(); }

bool TTF_Init(void) {
    if (ttf_init_count == 0) {
        FT_Error e = FT_Init_FreeType(&ttf_library);
        if (e) { SDL_SetError("TTF_Init: FT_Init_FreeType failed (FreeType error %d)", (int)e); return 0; }
    }
    ttf_init_count++;
    return 1;
}

void TTF_Quit(void) {
    if (ttf_init_count > 0 && --ttf_init_count == 0) {
        FT_Done_FreeType(ttf_library);
        ttf_library = NULL;
    }
}

int TTF_WasInit(void) { return ttf_init_count; }

/* Metric derivation — upstream UpdateFontStyle's scalable branch, including
   the underline/strikethrough height growth so a styled render never clips
   its own rule. */
static void ttf_update_metrics(TTF_Font *font) {
    FT_Face face = font->face;
    int underline_offset;
    if (FT_IS_SCALABLE(face)) {
        FT_Fixed scale = face->size->metrics.y_scale;
        font->ascent   = TTF_CEIL(FT_MulFix(face->ascender, scale));
        font->descent  = TTF_CEIL(FT_MulFix(face->descender, scale));
        font->height   = TTF_CEIL(FT_MulFix(face->ascender - face->descender, scale));
        font->lineskip = TTF_CEIL(FT_MulFix(face->height, scale));
        underline_offset     = TTF_FLOOR(FT_MulFix(face->underline_position, scale));
        font->line_thickness = TTF_FLOOR(FT_MulFix(face->underline_thickness, scale));
    } else {
        font->ascent   = TTF_CEIL(face->size->metrics.ascender);
        font->descent  = TTF_CEIL(face->size->metrics.descender);
        font->height   = TTF_CEIL(face->size->metrics.height);
        font->lineskip = font->height;
        underline_offset     = font->descent / 2;
        font->line_thickness = 1;
    }
    if (font->line_thickness < 1) font->line_thickness = 1;
    font->underline_top_row     = font->ascent - underline_offset - 1;
    font->strikethrough_top_row = font->height / 2;
    if (font->underline_top_row < 0) font->underline_top_row = 0;
    if (font->strikethrough_top_row < 0) font->strikethrough_top_row = 0;
    if (font->style & TTF_STYLE_UNDERLINE) {
        int bottom = font->underline_top_row + font->line_thickness;
        if (bottom > font->height) font->height = bottom;
    }
    if (font->style & TTF_STYLE_STRIKETHROUGH) {
        int bottom = font->strikethrough_top_row + font->line_thickness;
        if (bottom > font->height) font->height = bottom;
    }
}

bool TTF_SetFontSizeDPI(TTF_Font *font, float ptsize, int hdpi, int vdpi) {
    if (!font) return SDL_InvalidParamError("font");
    if (ptsize <= 0.0f) return SDL_InvalidParamError("ptsize");
    if (hdpi <= 0 && vdpi <= 0) { hdpi = font->hdpi; vdpi = font->vdpi; }
    else if (hdpi <= 0) hdpi = vdpi;
    else if (vdpi <= 0) vdpi = hdpi;
    FT_Error e = FT_Set_Char_Size(font->face, 0, (FT_F26Dot6)(ptsize * 64.0f),
                                  (FT_UInt)hdpi, (FT_UInt)vdpi);
    if (e) return SDL_SetError("TTF_SetFontSizeDPI: FT_Set_Char_Size failed (FreeType error %d)", (int)e);
    font->ptsize = ptsize;
    font->hdpi = hdpi;
    font->vdpi = vdpi;
    ttf_update_metrics(font);
    return 1;
}

bool TTF_SetFontSize(TTF_Font *font, float ptsize) {
    return TTF_SetFontSizeDPI(font, ptsize, 0, 0);
}

float TTF_GetFontSize(TTF_Font *font) {
    if (!font) { SDL_InvalidParamError("font"); return 0.0f; }
    return font->ptsize;
}

bool TTF_GetFontDPI(TTF_Font *font, int *hdpi, int *vdpi) {
    if (!font) return SDL_InvalidParamError("font");
    if (hdpi) *hdpi = font->hdpi;
    if (vdpi) *vdpi = font->vdpi;
    return 1;
}

TTF_Font *TTF_OpenFont(const char *file, float ptsize) {
    if (!ttf_init_count) { SDL_SetError("TTF_OpenFont: library not initialized (call TTF_Init first)"); return NULL; }
    if (!file) { SDL_InvalidParamError("file"); return NULL; }
    TTF_Font *font = (TTF_Font *)calloc(1, sizeof *font);
    if (!font) { SDL_SetError("Out of memory"); return NULL; }
    FT_Error e = FT_New_Face(ttf_library, file, 0, &font->face);
    if (e) {
        free(font);
        SDL_SetError("TTF_OpenFont('%s'): FT_New_Face failed (FreeType error %d)", file, (int)e);
        return NULL;
    }
    font->style = TTF_STYLE_NORMAL;
    font->hinting = TTF_HINTING_NORMAL;
    font->ft_load_target = FT_LOAD_TARGET_NORMAL;
    font->use_kerning = FT_HAS_KERNING(font->face) ? 1 : 0;
    font->hdpi = 72;                 /* upstream TTF_DEFAULT_DPI */
    font->vdpi = 72;
    if (!TTF_SetFontSizeDPI(font, ptsize, 0, 0)) {
        FT_Done_Face(font->face);
        free(font);
        return NULL;
    }
    return font;
}

void TTF_CloseFont(TTF_Font *font) {
    if (!font) return;
    FT_Done_Face(font->face);
    free(font);
}

void TTF_SetFontStyle(TTF_Font *font, TTF_FontStyleFlags style) {
    if (!font) { SDL_InvalidParamError("font"); return; }
    font->style = style & (TTF_STYLE_BOLD | TTF_STYLE_ITALIC |
                           TTF_STYLE_UNDERLINE | TTF_STYLE_STRIKETHROUGH);
    ttf_update_metrics(font);
}

TTF_FontStyleFlags TTF_GetFontStyle(const TTF_Font *font) {
    if (!font) { SDL_InvalidParamError("font"); return TTF_STYLE_NORMAL; }
    return font->style;
}

void TTF_SetFontHinting(TTF_Font *font, TTF_HintingFlags hinting) {
    if (!font) { SDL_InvalidParamError("font"); return; }
    if (hinting == TTF_HINTING_LIGHT)      font->ft_load_target = FT_LOAD_TARGET_LIGHT;
    else if (hinting == TTF_HINTING_MONO)  font->ft_load_target = FT_LOAD_TARGET_MONO;
    else if (hinting == TTF_HINTING_NONE)  font->ft_load_target = FT_LOAD_NO_HINTING;
    else { hinting = TTF_HINTING_NORMAL;   font->ft_load_target = FT_LOAD_TARGET_NORMAL; }
    font->hinting = hinting;
}

TTF_HintingFlags TTF_GetFontHinting(const TTF_Font *font) {
    if (!font) { SDL_InvalidParamError("font"); return TTF_HINTING_INVALID; }
    return font->hinting;
}

void TTF_SetFontKerning(TTF_Font *font, bool enabled) {
    if (!font) { SDL_InvalidParamError("font"); return; }
    font->use_kerning = (enabled && FT_HAS_KERNING(font->face)) ? 1 : 0;
}

bool TTF_GetFontKerning(const TTF_Font *font) {
    if (!font) { SDL_InvalidParamError("font"); return 0; }
    return font->use_kerning;
}

int TTF_GetFontHeight(const TTF_Font *font) {
    if (!font) { SDL_InvalidParamError("font"); return 0; }
    return font->height;
}

int TTF_GetFontAscent(const TTF_Font *font) {
    if (!font) { SDL_InvalidParamError("font"); return 0; }
    return font->ascent;
}

int TTF_GetFontDescent(const TTF_Font *font) {
    if (!font) { SDL_InvalidParamError("font"); return 0; }
    return font->descent;
}

int TTF_GetFontLineSkip(const TTF_Font *font) {
    if (!font) { SDL_InvalidParamError("font"); return 0; }
    return font->lineskip;
}

void TTF_SetFontLineSkip(TTF_Font *font, int lineskip) {
    if (!font) { SDL_InvalidParamError("font"); return; }
    font->lineskip = lineskip;
}

bool TTF_FontIsFixedWidth(const TTF_Font *font) {
    if (!font) { SDL_InvalidParamError("font"); return 0; }
    return FT_IS_FIXED_WIDTH(font->face) ? 1 : 0;
}

const char *TTF_GetFontFamilyName(const TTF_Font *font) {
    if (!font) { SDL_InvalidParamError("font"); return NULL; }
    return font->face->family_name;
}

const char *TTF_GetFontStyleName(const TTF_Font *font) {
    if (!font) { SDL_InvalidParamError("font"); return NULL; }
    return font->face->style_name;
}

bool TTF_FontHasGlyph(TTF_Font *font, Uint32 ch) {
    if (!font) return SDL_InvalidParamError("font");
    return FT_Get_Char_Index(font->face, (FT_ULong)ch) != 0;
}

/* Load (and optionally render) one glyph with the font's style synthesis
   applied — bold = embolden, italic = oblique shear (ftsynth.c), the same
   pattern the gdi32 rasterizer uses. */
static bool ttf_load_glyph(TTF_Font *font, FT_UInt idx, int render, FT_Render_Mode rmode) {
    FT_Error e = FT_Load_Glyph(font->face, idx, FT_LOAD_DEFAULT | font->ft_load_target);
    if (e) return SDL_SetError("FT_Load_Glyph failed (FreeType error %d)", (int)e);
    if (font->style & TTF_STYLE_BOLD)   FT_GlyphSlot_Embolden(font->face->glyph);
    if (font->style & TTF_STYLE_ITALIC) FT_GlyphSlot_Oblique(font->face->glyph);
    if (render) {
        e = FT_Render_Glyph(font->face->glyph, rmode);
        if (e) return SDL_SetError("FT_Render_Glyph failed (FreeType error %d)", (int)e);
    }
    return 1;
}

bool TTF_GetGlyphMetrics(TTF_Font *font, Uint32 ch, int *minx, int *maxx, int *miny, int *maxy, int *advance) {
    if (!font) return SDL_InvalidParamError("font");
    FT_UInt idx = FT_Get_Char_Index(font->face, (FT_ULong)ch);
    if (!ttf_load_glyph(font, idx, 0, FT_RENDER_MODE_NORMAL)) return 0;
    FT_Glyph_Metrics *gm = &font->face->glyph->metrics;
    if (minx) *minx = TTF_FLOOR(gm->horiBearingX);
    if (maxx) *maxx = TTF_CEIL(gm->horiBearingX + gm->width);
    if (maxy) *maxy = TTF_CEIL(gm->horiBearingY);
    if (miny) *miny = TTF_CEIL(gm->horiBearingY) - TTF_CEIL(gm->height);
    if (advance) *advance = TTF_CEIL(font->face->glyph->advance.x);
    return 1;
}

bool TTF_GetGlyphKerning(TTF_Font *font, Uint32 previous_ch, Uint32 ch, int *kerning) {
    if (!font) return SDL_InvalidParamError("font");
    if (kerning) *kerning = 0;
    if (!font->use_kerning) return 1;
    FT_UInt a = FT_Get_Char_Index(font->face, (FT_ULong)previous_ch);
    FT_UInt b = FT_Get_Char_Index(font->face, (FT_ULong)ch);
    if (a && b) {
        FT_Vector d;
        FT_Get_Kerning(font->face, a, b, FT_KERNING_DEFAULT, &d);
        if (kerning) *kerning = (int)(d.x >> 6);
    }
    return 1;
}

/* Decode one UTF-8 codepoint at *p (advancing, never past end). Invalid,
   truncated, overlong, surrogate or out-of-range sequences decode to U+FFFD
   consuming one byte — the SDL_StepUTF8 rule, so bad input renders
   predictably (replacement/.notdef) instead of as garbage. */
static Uint32 ttf_step_utf8(const char **p, const char *end) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned char c = s[0];
    Uint32 cp;
    int len, i;
    if (c < 0x80) { *p += 1; return c; }
    else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
    else { *p += 1; return 0xFFFD; }
    if (*p + len > end) { *p += 1; return 0xFFFD; }
    for (i = 1; i < len; i++) {
        if ((s[i] & 0xC0) != 0x80) { *p += 1; return 0xFFFD; }
        cp = (cp << 6) | (Uint32)(s[i] & 0x3F);
    }
    if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000)
        || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) { *p += len; return 0xFFFD; }
    *p += len;
    return cp;
}

/* Measure text[0..len) as ONE line — upstream TTF_Size_Internal's shape: the
   pen advances (kerned), ink extents are tracked separately, minx may go
   negative (left overhang) and -minx becomes the pen start offset, so the
   surface is exactly wide enough for both ink and advance. */
static bool ttf_measure_line(TTF_Font *font, const char *text, size_t len, int *out_w, int *out_xstart) {
    FT_Face face = font->face;
    FT_Pos x = 0, minx = 0, maxx = 0;
    FT_UInt prev = 0;
    const char *p = text, *end = text + len;
    while (p < end) {
        Uint32 cp = ttf_step_utf8(&p, end);
        FT_UInt idx = FT_Get_Char_Index(face, (FT_ULong)cp);
        if (font->use_kerning && prev && idx) {
            FT_Vector d;
            FT_Get_Kerning(face, prev, idx, FT_KERNING_DEFAULT, &d);
            x += d.x;
        }
        if (!ttf_load_glyph(font, idx, 0, FT_RENDER_MODE_NORMAL)) return 0;
        {
            FT_Glyph_Metrics *gm = &face->glyph->metrics;
            FT_Pos gminx = x + gm->horiBearingX;
            FT_Pos gmaxx = x + gm->horiBearingX + gm->width;
            if (gminx < minx) minx = gminx;
            if (gmaxx > maxx) maxx = gmaxx;
        }
        x += face->glyph->advance.x;
        if (x > maxx) maxx = x;
        prev = idx;
    }
    *out_w = TTF_CEIL(maxx) - TTF_FLOOR(minx);
    *out_xstart = -TTF_FLOOR(minx);
    return 1;
}

/* Greedy word wrap: byte ranges, breaking on newlines always, and — when
   wrap_width > 0 — at the last space before the pen exceeds wrap_width px
   (a word longer than the width splits at the first glyph that no longer
   fits). Returns a malloc'd array of {start,len} pairs. */
typedef struct { size_t start, len; } TTF_LineRange;

static TTF_LineRange *ttf_split_lines(TTF_Font *font, const char *text, size_t length,
                                      int wrapped, int wrap_width, int *out_n) {
    size_t cap = 8, n = 0;
    TTF_LineRange *lines = (TTF_LineRange *)malloc(cap * sizeof *lines);
    if (!lines) { SDL_SetError("Out of memory"); return NULL; }
    if (!wrapped) {
        lines[0].start = 0; lines[0].len = length; *out_n = 1;
        return lines;
    }
    {
        FT_Face face = font->face;
        size_t line_start = 0;
        size_t break_at = 0;          /* byte AFTER the last space on this line */
        int have_break = 0;
        FT_Pos x = 0;
        FT_UInt prev = 0;
        const char *p = text, *end = text + length;
        while (p < end) {
            const char *gstart = p;
            Uint32 cp = ttf_step_utf8(&p, end);
            size_t gpos = (size_t)(gstart - text);
            if (cp == 10) {           /* hard newline: line ends before it */
                if (n == cap) { cap *= 2; TTF_LineRange *nl = (TTF_LineRange *)realloc(lines, cap * sizeof *lines); if (!nl) { free(lines); SDL_SetError("Out of memory"); return NULL; } lines = nl; }
                lines[n].start = line_start; lines[n].len = gpos - line_start; n++;
                line_start = (size_t)(p - text);
                have_break = 0; x = 0; prev = 0;
                continue;
            }
            {
                FT_UInt idx = FT_Get_Char_Index(face, (FT_ULong)cp);
                if (font->use_kerning && prev && idx) {
                    FT_Vector d;
                    FT_Get_Kerning(face, prev, idx, FT_KERNING_DEFAULT, &d);
                    x += d.x;
                }
                if (!ttf_load_glyph(font, idx, 0, FT_RENDER_MODE_NORMAL)) { free(lines); return NULL; }
                x += face->glyph->advance.x;
                prev = idx;
                if (wrap_width > 0 && TTF_CEIL(x) > wrap_width && gpos > line_start) {
                    size_t next_start;
                    size_t line_end;
                    if (have_break && break_at > line_start) {
                        line_end = break_at - 1;      /* drop the breaking space */
                        next_start = break_at;
                    } else {
                        line_end = gpos;              /* split the long word here */
                        next_start = gpos;
                    }
                    if (n == cap) { cap *= 2; TTF_LineRange *nl = (TTF_LineRange *)realloc(lines, cap * sizeof *lines); if (!nl) { free(lines); SDL_SetError("Out of memory"); return NULL; } lines = nl; }
                    lines[n].start = line_start; lines[n].len = line_end - line_start; n++;
                    line_start = next_start;
                    have_break = 0; x = 0; prev = 0;
                    p = text + next_start;            /* re-decode from the new line start */
                    continue;
                }
                if (cp == 32) { break_at = (size_t)(p - text); have_break = 1; }
            }
        }
        if (n == cap) { cap += 1; TTF_LineRange *nl = (TTF_LineRange *)realloc(lines, cap * sizeof *lines); if (!nl) { free(lines); SDL_SetError("Out of memory"); return NULL; } lines = nl; }
        lines[n].start = line_start; lines[n].len = length - line_start; n++;
        *out_n = (int)n;
        return lines;
    }
}

/* Blit one rendered glyph bitmap. cov is 0..255 coverage per pixel (MONO
   bitmaps expand bits to 0/255). Modes: SOLID writes fg only where coverage
   reaches 50% — bilevel output as documented, from the smooth raster —
   BLENDED writes fg with alpha = cov*fg.a/255 keeping the max on overlap,
   SHADED lerps the pixel toward fg by cov (surface pre-filled opaque bg). */
static void ttf_blit_glyph(FT_Bitmap *bmp, Uint8 *pixels, int sw, int sh,
                           int gx, int gy, int mode, SDL_Color fg) {
    unsigned r, c;
    for (r = 0; r < bmp->rows; r++) {
        int dy = gy + (int)r;
        if (dy < 0 || dy >= sh) continue;
        for (c = 0; c < bmp->width; c++) {
            int dx = gx + (int)c;
            unsigned cov;
            Uint8 *px;
            if (dx < 0 || dx >= sw) continue;
            if (bmp->pixel_mode == FT_PIXEL_MODE_MONO)
                cov = (bmp->buffer[r * (unsigned)bmp->pitch + (c >> 3)] & (0x80u >> (c & 7))) ? 255 : 0;
            else
                cov = bmp->buffer[r * (unsigned)bmp->pitch + c];
            if (!cov) continue;
            if (mode == TTF_R_SOLID && cov < 128) continue;
            px = pixels + ((size_t)dy * (size_t)sw + (size_t)dx) * 4;
            if (mode == TTF_R_BLENDED) {
                unsigned a = cov * fg.a / 255;
                if (a > px[3]) { px[0] = fg.r; px[1] = fg.g; px[2] = fg.b; px[3] = (Uint8)a; }
            } else if (mode == TTF_R_SHADED) {
                px[0] = (Uint8)(px[0] + (int)cov * (fg.r - px[0]) / 255);
                px[1] = (Uint8)(px[1] + (int)cov * (fg.g - px[1]) / 255);
                px[2] = (Uint8)(px[2] + (int)cov * (fg.b - px[2]) / 255);
                px[3] = 255;
            } else {
                px[0] = fg.r; px[1] = fg.g; px[2] = fg.b; px[3] = fg.a;
            }
        }
    }
}

/* Fill a solid rule (underline/strikethrough) row band across a line. */
static void ttf_fill_rule(Uint8 *pixels, int sw, int sh, int y0, int h,
                          int x0, int w, int mode, SDL_Color fg) {
    int y, x;
    for (y = y0; y < y0 + h; y++) {
        if (y < 0 || y >= sh) continue;
        for (x = x0; x < x0 + w; x++) {
            Uint8 *px;
            if (x < 0 || x >= sw) continue;
            px = pixels + ((size_t)y * (size_t)sw + (size_t)x) * 4;
            px[0] = fg.r; px[1] = fg.g; px[2] = fg.b;
            px[3] = (mode == TTF_R_SHADED) ? 255 : fg.a;
        }
    }
}

/* Render one already-measured line at pen start (xstart, baseline). All
   modes rasterize through the smooth renderer (the #464 freetype module set
   ships smooth only — no mono ftraster); SOLID gets its documented bilevel
   output by thresholding coverage at 50% in the blit. */
static bool ttf_render_line(TTF_Font *font, const char *text, size_t len,
                            Uint8 *pixels, int sw, int sh,
                            int xstart, int ytop, int mode, SDL_Color fg) {
    FT_Face face = font->face;
    FT_Pos x = (FT_Pos)xstart * 64;
    FT_UInt prev = 0;
    int baseline = ytop + font->ascent;
    FT_Render_Mode rmode = FT_RENDER_MODE_NORMAL;
    const char *p = text, *end = text + len;
    while (p < end) {
        Uint32 cp = ttf_step_utf8(&p, end);
        FT_UInt idx = FT_Get_Char_Index(face, (FT_ULong)cp);
        if (font->use_kerning && prev && idx) {
            FT_Vector d;
            FT_Get_Kerning(face, prev, idx, FT_KERNING_DEFAULT, &d);
            x += d.x;
        }
        if (!ttf_load_glyph(font, idx, 1, rmode)) return 0;
        ttf_blit_glyph(&face->glyph->bitmap, pixels, sw, sh,
                       TTF_FLOOR(x) + face->glyph->bitmap_left,
                       baseline - face->glyph->bitmap_top, mode, fg);
        x += face->glyph->advance.x;
        prev = idx;
    }
    return 1;
}

/* The one measure/render core. Measures every line (shared with the Size
   calls, so surface dims == reported dims BY CONSTRUCTION); when rendering,
   allocates the RGBA32 surface and draws glyphs + style rules. */
static SDL_Surface *ttf_render_internal(TTF_Font *font, const char *text, size_t length,
                                        SDL_Color fg, SDL_Color bg, int mode,
                                        int wrapped, int wrap_width) {
    TTF_LineRange *lines = NULL;
    int nlines = 0, w = 0, h, i;
    int *lw = NULL, *lx = NULL;
    Uint8 *pixels = NULL;
    SDL_Surface *s = NULL;

    if (!font) { SDL_InvalidParamError("font"); return NULL; }
    if (!text) { SDL_InvalidParamError("text"); return NULL; }
    if (wrap_width < 0) { SDL_InvalidParamError("wrap_width"); return NULL; }
    if (!length) length = strlen(text);
    if (fg.a == 0) fg.a = 255;       /* upstream: transparent fg means opaque */

    lines = ttf_split_lines(font, text, length, wrapped, wrap_width, &nlines);
    if (!lines) return NULL;
    lw = (int *)malloc((size_t)nlines * sizeof *lw);
    lx = (int *)malloc((size_t)nlines * sizeof *lx);
    if (!lw || !lx) { SDL_SetError("Out of memory"); goto fail; }
    for (i = 0; i < nlines; i++) {
        if (!ttf_measure_line(font, text + lines[i].start, lines[i].len, &lw[i], &lx[i])) goto fail;
        if (lw[i] > w) w = lw[i];
    }
    h = font->height + font->lineskip * (nlines - 1);
    if (w == 0) { SDL_SetError("Text has zero width"); goto fail; }
    if ((long long)w * (long long)h > (64 << 20)) {
        SDL_SetError("TTF: text surface too large (%dx%d)", w, h);
        goto fail;
    }

    pixels = (Uint8 *)malloc((size_t)w * (size_t)h * 4);
    if (!pixels) { SDL_SetError("Out of memory"); goto fail; }
    if (mode == TTF_R_SHADED) {
        /* opaque bg-colored box (upstream Shaded semantics) */
        size_t k, npx = (size_t)w * (size_t)h;
        for (k = 0; k < npx; k++) {
            Uint8 *px = pixels + k * 4;
            px[0] = bg.r; px[1] = bg.g; px[2] = bg.b; px[3] = 255;
        }
    } else {
        memset(pixels, 0, (size_t)w * (size_t)h * 4);   /* transparent */
    }

    for (i = 0; i < nlines; i++) {
        int ytop = i * font->lineskip;
        if (!ttf_render_line(font, text + lines[i].start, lines[i].len,
                             pixels, w, h, lx[i], ytop, mode, fg)) goto fail;
        if (font->style & TTF_STYLE_UNDERLINE)
            ttf_fill_rule(pixels, w, h, ytop + font->underline_top_row,
                          font->line_thickness, 0, lw[i], mode, fg);
        if (font->style & TTF_STYLE_STRIKETHROUGH)
            ttf_fill_rule(pixels, w, h, ytop + font->strikethrough_top_row,
                          font->line_thickness, 0, lw[i], mode, fg);
    }

    s = (SDL_Surface *)calloc(1, sizeof *s);
    if (!s) { SDL_SetError("Out of memory"); goto fail; }
    s->flags = IMG_SURFACE_OWNED;    /* SDL_DestroySurface reclaims pixels + s */
    s->format = SDL_PIXELFORMAT_RGBA32;
    s->w = w; s->h = h; s->pitch = w * 4;
    s->pixels = pixels; s->refcount = 1;
    free(lines); free(lw); free(lx);
    return s;

fail:
    free(lines); free(lw); free(lx); free(pixels);
    return NULL;
}

/* The shared measure core behind both Size calls — the same split+measure
   the renderer runs, so reported dims equal surface dims by construction. */
static bool ttf_size_internal(TTF_Font *font, const char *text, size_t length,
                              int wrapped, int wrap_width, int *w, int *h) {
    TTF_LineRange *lines;
    int nlines = 0, ww = 0, i;
    if (!font) return SDL_InvalidParamError("font");
    if (!text) return SDL_InvalidParamError("text");
    if (!length) length = strlen(text);
    lines = ttf_split_lines(font, text, length, wrapped, wrap_width, &nlines);
    if (!lines) return 0;
    for (i = 0; i < nlines; i++) {
        int liw, lix;
        if (!ttf_measure_line(font, text + lines[i].start, lines[i].len, &liw, &lix)) { free(lines); return 0; }
        if (liw > ww) ww = liw;
    }
    if (w) *w = ww;
    if (h) *h = font->height + font->lineskip * (nlines - 1);
    free(lines);
    return 1;
}

bool TTF_GetStringSize(TTF_Font *font, const char *text, size_t length, int *w, int *h) {
    return ttf_size_internal(font, text, length, 0, 0, w, h);
}

bool TTF_GetStringSizeWrapped(TTF_Font *font, const char *text, size_t length, int wrap_width, int *w, int *h) {
    if (wrap_width < 0) return SDL_InvalidParamError("wrap_width");
    return ttf_size_internal(font, text, length, 1, wrap_width, w, h);
}

bool TTF_MeasureString(TTF_Font *font, const char *text, size_t length, int max_width,
                       int *measured_width, size_t *measured_length) {
    FT_Pos x = 0;
    FT_UInt prev = 0;
    const char *p, *end;
    size_t fit = 0;
    if (!font) return SDL_InvalidParamError("font");
    if (!text) return SDL_InvalidParamError("text");
    if (!length) length = strlen(text);
    p = text; end = text + length;
    while (p < end) {
        Uint32 cp = ttf_step_utf8(&p, end);
        FT_UInt idx = FT_Get_Char_Index(font->face, (FT_ULong)cp);
        FT_Pos nx = x;
        if (font->use_kerning && prev && idx) {
            FT_Vector d;
            FT_Get_Kerning(font->face, prev, idx, FT_KERNING_DEFAULT, &d);
            nx += d.x;
        }
        if (!ttf_load_glyph(font, idx, 0, FT_RENDER_MODE_NORMAL)) return 0;
        nx += font->face->glyph->advance.x;
        if (max_width > 0 && TTF_CEIL(nx) > max_width) break;
        x = nx;
        fit = (size_t)(p - text);
        prev = idx;
    }
    if (measured_width) *measured_width = TTF_CEIL(x);
    if (measured_length) *measured_length = fit;
    return 1;
}

SDL_Surface *TTF_RenderText_Solid(TTF_Font *font, const char *text, size_t length, SDL_Color fg) {
    SDL_Color bg = { 0, 0, 0, 0 };
    return ttf_render_internal(font, text, length, fg, bg, TTF_R_SOLID, 0, 0);
}

SDL_Surface *TTF_RenderText_Solid_Wrapped(TTF_Font *font, const char *text, size_t length, SDL_Color fg, int wrapLength) {
    SDL_Color bg = { 0, 0, 0, 0 };
    return ttf_render_internal(font, text, length, fg, bg, TTF_R_SOLID, 1, wrapLength);
}

SDL_Surface *TTF_RenderText_Shaded(TTF_Font *font, const char *text, size_t length, SDL_Color fg, SDL_Color bg) {
    return ttf_render_internal(font, text, length, fg, bg, TTF_R_SHADED, 0, 0);
}

SDL_Surface *TTF_RenderText_Shaded_Wrapped(TTF_Font *font, const char *text, size_t length, SDL_Color fg, SDL_Color bg, int wrap_width) {
    return ttf_render_internal(font, text, length, fg, bg, TTF_R_SHADED, 1, wrap_width);
}

SDL_Surface *TTF_RenderText_Blended(TTF_Font *font, const char *text, size_t length, SDL_Color fg) {
    SDL_Color bg = { 0, 0, 0, 0 };
    return ttf_render_internal(font, text, length, fg, bg, TTF_R_BLENDED, 0, 0);
}

SDL_Surface *TTF_RenderText_Blended_Wrapped(TTF_Font *font, const char *text, size_t length, SDL_Color fg, int wrap_width) {
    SDL_Color bg = { 0, 0, 0, 0 };
    return ttf_render_internal(font, text, length, fg, bg, TTF_R_BLENDED, 1, wrap_width);
}

/* Single-codepoint renderers: encode and reuse the string core, so glyph
   surfaces measure and blend exactly like 1-char strings. */
static size_t ttf_encode_utf8(Uint32 cp, char *buf) {
    if (cp < 0x80) { buf[0] = (char)cp; return 1; }
    if (cp < 0x800) { buf[0] = (char)(0xC0 | (cp >> 6)); buf[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
    if (cp < 0x10000) { buf[0] = (char)(0xE0 | (cp >> 12)); buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); buf[2] = (char)(0x80 | (cp & 0x3F)); return 3; }
    buf[0] = (char)(0xF0 | (cp >> 18)); buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); buf[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

SDL_Surface *TTF_RenderGlyph_Solid(TTF_Font *font, Uint32 ch, SDL_Color fg) {
    char buf[4];
    SDL_Color bg = { 0, 0, 0, 0 };
    if (ch > 0x10FFFF) { SDL_InvalidParamError("ch"); return NULL; }
    return ttf_render_internal(font, buf, ttf_encode_utf8(ch, buf), fg, bg, TTF_R_SOLID, 0, 0);
}

SDL_Surface *TTF_RenderGlyph_Shaded(TTF_Font *font, Uint32 ch, SDL_Color fg, SDL_Color bg) {
    char buf[4];
    if (ch > 0x10FFFF) { SDL_InvalidParamError("ch"); return NULL; }
    return ttf_render_internal(font, buf, ttf_encode_utf8(ch, buf), fg, bg, TTF_R_SHADED, 0, 0);
}

SDL_Surface *TTF_RenderGlyph_Blended(TTF_Font *font, Uint32 ch, SDL_Color fg) {
    char buf[4];
    SDL_Color bg = { 0, 0, 0, 0 };
    if (ch > 0x10FFFF) { SDL_InvalidParamError("ch"); return NULL; }
    return ttf_render_internal(font, buf, ttf_encode_utf8(ch, buf), fg, bg, TTF_R_BLENDED, 0, 0);
}
  