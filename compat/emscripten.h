// Native stand-in for <emscripten.h>: the TH17 sources that include it only
// use it for browser-side snippets, which the native build drops.
#pragma once
#define EM_ASM(...) ((void)0)
#define EM_ASM_INT(...) 0
#define EM_ASM_DOUBLE(...) 0.0
#define EM_JS(ret, name, params, ...) static ret name params { return ret(); }
#define EMSCRIPTEN_KEEPALIVE
