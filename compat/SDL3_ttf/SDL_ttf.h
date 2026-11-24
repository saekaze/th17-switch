// SDL3_ttf -> SDL2_ttf compatibility layer for the TH17 sources.
//
// TH17 rasterizes text on demand through SDL_ttf (eagler RuntimeFontRaster and
// the thcrap language-pack path in FontLocalization.cpp). SDL2_ttf has the
// same FreeType renderer; this maps the SDL3_ttf names and bool results onto
// it. Font paths go through th17native::path like every other game path.
#pragma once
#include "../SDL3/SDL.h"
#include <SDL2/SDL_ttf.h>
#include <string>

namespace th17compat {
// SDL2_ttf has no size getter; remember what each font was opened/set with.
float& font_size_slot(TTF_Font* font);
void forget_font(TTF_Font* font);
inline bool ttf_init() { return ::TTF_Init() == 0; }
inline TTF_Font* open_font(const char* path, float size) {
    // th17native::open_font serves the file from memory, so a font face is
    // never read from the SD card again once the game is running.
    TTF_Font* font = th17native::open_font(path, int(size + .5f));
    if (font) font_size_slot(font) = float(int(size + .5f));
    return font;
}
inline void close_font(TTF_Font* font) { if (!font) return; forget_font(font); ::TTF_CloseFont(font); }
inline float get_font_size(TTF_Font* font) { return font_size_slot(font); }
inline bool set_font_size(TTF_Font* font, float size) {
    if (::TTF_SetFontSize(font, int(size + .5f)) != 0) return false;
    font_size_slot(font) = float(int(size + .5f)); return true;
}
// SDL2_ttf has no fallback chain; glyphs missing from a language pack's font
// fall back to its own .notdef glyph instead of MS Gothic.
inline bool add_fallback_font(TTF_Font*, TTF_Font*) { return true; }
inline bool font_has_glyph(TTF_Font* font, Uint32 ch) { return ::TTF_GlyphIsProvided32(font, ch) != 0; }
inline bool glyph_metrics(TTF_Font* font, Uint32 ch, int* minx, int* maxx, int* miny, int* maxy, int* advance) {
    return ::TTF_GlyphMetrics32(font, ch, minx, maxx, miny, maxy, advance) == 0;
}
inline SDL_Surface* render_glyph_blended(TTF_Font* font, Uint32 ch, SDL_Color fg) { return ::TTF_RenderGlyph32_Blended(font, ch, fg); }
inline SDL_Surface* render_text_blended(TTF_Font* font, const char* text, size_t length, SDL_Color fg) {
    if (length == 0) return nullptr;
    const std::string copy(text, length);
    return ::TTF_RenderUTF8_Blended(font, copy.c_str(), fg);
}
inline bool string_size(TTF_Font* font, const char* text, size_t length, int* w, int* h) {
    const std::string copy(text, length);
    return ::TTF_SizeUTF8(font, copy.c_str(), w, h) == 0;
}
inline void set_kerning(TTF_Font* font, bool enabled) { ::TTF_SetFontKerning(font, enabled ? 1 : 0); }
}

#define TTF_Init th17compat::ttf_init
#define TTF_OpenFont th17compat::open_font
#define TTF_CloseFont th17compat::close_font
#define TTF_GetFontSize th17compat::get_font_size
#define TTF_SetFontSize th17compat::set_font_size
#define TTF_AddFallbackFont th17compat::add_fallback_font
#define TTF_FontHasGlyph th17compat::font_has_glyph
#define TTF_GetGlyphMetrics th17compat::glyph_metrics
#define TTF_RenderGlyph_Blended th17compat::render_glyph_blended
#define TTF_RenderText_Blended th17compat::render_text_blended
#define TTF_GetStringSize th17compat::string_size
#define TTF_SetFontKerning th17compat::set_kerning
