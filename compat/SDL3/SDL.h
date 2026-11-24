// SDL3 -> SDL2 compatibility layer for the TH17 sources.
//
// The upstream runtime targets SDL3 (Emscripten port). Switch homebrew ships
// SDL2, so this header provides the SDL3 surface TH17 uses on top of SDL2.
// Only what the sources call is covered; anything else is a compile error on
// purpose. SDL3 calls that return bool (true = success) are wrapped where
// SDL2 returns 0 on success.
//
// Absolute game paths ("/th17.dat", "/msgothic.ttc", "/thcrap/...") are
// mapped into the data folder by th17native::path (src/Platform.cpp).
#pragma once
#include <SDL2/SDL.h>
#include "../th17_native.h"
#include <cstdint>
#include <sys/stat.h>

namespace th17compat {
inline bool init(Uint32 flags) { return ::SDL_Init(flags) == 0; }
inline bool init_subsystem(Uint32 flags) { return ::SDL_InitSubSystem(flags) == 0; }
inline SDL_Window* create_window(const char* title, int w, int h, Uint64 flags) {
    return ::SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, Uint32(flags));
}
// The SDL2 call itself, for the patched Renderer (native window placement).
inline SDL_Window* sdl2_window(const char* title, int x, int y, int w, int h, Uint32 flags) {
    return ::SDL_CreateWindow(title, x, y, w, h, flags);
}
inline int version() {
    SDL_version v; ::SDL_GetVersion(&v);
    return v.major * 1000000 + v.minor * 1000 + v.patch;
}
inline Uint64 ticks_ns() {
    static const Uint64 frequency = ::SDL_GetPerformanceFrequency();
    const Uint64 count = ::SDL_GetPerformanceCounter();
    return Uint64((long double)count * 1000000000.0L / (long double)frequency);
}

// ---- files ----------------------------------------------------------------
inline void* load_file(const char* path, size_t* size) { return ::SDL_LoadFile(th17native::path(path), size); }
inline SDL_RWops* io_from_file(const char* path, const char* mode) { return ::SDL_RWFromFile(th17native::path(path), mode); }
inline size_t read_io(SDL_RWops* f, void* p, size_t n) { return ::SDL_RWread(f, p, 1, n); }
inline size_t write_io(SDL_RWops* f, const void* p, size_t n) { return ::SDL_RWwrite(f, p, 1, n); }
inline bool close_io(SDL_RWops* f) { return ::SDL_RWclose(f) == 0; }
inline Sint64 seek_io(SDL_RWops* f, Sint64 offset, int whence) { return ::SDL_RWseek(f, offset, whence); }

enum PathType { SDL_PATHTYPE_NONE, SDL_PATHTYPE_FILE, SDL_PATHTYPE_DIRECTORY, SDL_PATHTYPE_OTHER };
struct PathInfo { PathType type = SDL_PATHTYPE_NONE; Uint64 size = 0; };
inline bool path_info(const char* path, PathInfo* info) {
    struct stat st {};
    if (::stat(th17native::path(path), &st) != 0) { if (info) *info = {}; return false; }
    if (info) {
        info->type = S_ISREG(st.st_mode) ? SDL_PATHTYPE_FILE : S_ISDIR(st.st_mode) ? SDL_PATHTYPE_DIRECTORY : SDL_PATHTYPE_OTHER;
        info->size = Uint64(st.st_size);
    }
    return true;
}

// ---- surfaces -------------------------------------------------------------
inline SDL_Surface* convert_surface(SDL_Surface* s, Uint32 format) { return ::SDL_ConvertSurfaceFormat(s, format, 0); }
inline void destroy_surface(SDL_Surface* s) { ::SDL_FreeSurface(s); }

// ---- audio ----------------------------------------------------------------
// SDL3's spec layout is {format, channels, freq}; SDL2's differs, so the game
// gets its own struct and the device is opened in queue mode.
struct AudioSpec3 { SDL_AudioFormat format; int channels; int freq; };
struct AudioStream3 { SDL_AudioDeviceID device = 0; };
inline AudioStream3* open_audio_stream(int, const AudioSpec3* spec, void*, void*) {
    SDL_AudioSpec want{}, have{};
    want.freq = spec->freq; want.format = spec->format; want.channels = Uint8(spec->channels);
    want.samples = 2048;
    const SDL_AudioDeviceID device = ::SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!device) return nullptr;
    auto* s = new AudioStream3; s->device = device; return s;
}
inline int queued(AudioStream3* s) { return int(::SDL_GetQueuedAudioSize(s->device)); }
inline bool put(AudioStream3* s, const void* data, int bytes) { return ::SDL_QueueAudio(s->device, data, Uint32(bytes)) == 0; }
inline void clear(AudioStream3* s) { ::SDL_ClearQueuedAudio(s->device); }
inline bool resume(AudioStream3* s) { ::SDL_PauseAudioDevice(s->device, 0); return true; }
inline bool pause(AudioStream3* s) { ::SDL_PauseAudioDevice(s->device, 1); return true; }
inline void destroy(AudioStream3* s) { if (!s) return; ::SDL_CloseAudioDevice(s->device); delete s; }
}

#define SDL_Init th17compat::init
#define SDL_InitSubSystem th17compat::init_subsystem
#define SDL_CreateWindow th17compat::create_window
#define SDL_GL_DestroyContext SDL_GL_DeleteContext
#define SDL_GetVersion th17compat::version
#define SDL_GetTicksNS th17compat::ticks_ns
#define SDL_GetWindowSizeInPixels SDL_GL_GetDrawableSize
// Browser-only hints: harmless names where this SDL2 does not define them.
#ifndef SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR
#define SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR "TH17_EMSCRIPTEN_CANVAS_SELECTOR"
#endif
#ifndef SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT
#define SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT "TH17_EMSCRIPTEN_KEYBOARD_ELEMENT"
#endif
#define SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES "TH17_AUDIO_DEVICE_SAMPLE_FRAMES"

typedef SDL_RWops SDL_IOStream;
#define SDL_LoadFile th17compat::load_file
#define SDL_IOFromFile th17compat::io_from_file
#define SDL_GetIOSize SDL_RWsize
#define SDL_ReadIO th17compat::read_io
#define SDL_WriteIO th17compat::write_io
#define SDL_CloseIO th17compat::close_io
#define SDL_SeekIO th17compat::seek_io
#define SDL_IO_SEEK_SET RW_SEEK_SET
#define SDL_PathInfo th17compat::PathInfo
#define SDL_GetPathInfo th17compat::path_info
using th17compat::SDL_PATHTYPE_FILE;
using th17compat::SDL_PATHTYPE_DIRECTORY;

#define SDL_ConvertSurface th17compat::convert_surface
#define SDL_DestroySurface th17compat::destroy_surface

#define SDL_AudioSpec th17compat::AudioSpec3
#define SDL_AudioStream th17compat::AudioStream3
#define SDL_AUDIO_F32 AUDIO_F32SYS
#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK 0
#define SDL_OpenAudioDeviceStream th17compat::open_audio_stream
#define SDL_GetAudioStreamQueued th17compat::queued
#define SDL_PutAudioStreamData th17compat::put
#define SDL_ClearAudioStream th17compat::clear
#define SDL_ResumeAudioStreamDevice th17compat::resume
#define SDL_PauseAudioStreamDevice th17compat::pause
#define SDL_DestroyAudioStream th17compat::destroy
