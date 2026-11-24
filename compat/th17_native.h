// Native host services the TH17 compatibility headers and patched sources
// call into (implemented in src/Platform.cpp).
#pragma once
#include <cstddef>
#include <string>
typedef struct _TTF_Font TTF_Font;
namespace th17native {
// Maps an absolute game path ("/th17.dat", "/msgothic.ttc", "/thcrap/...")
// into the data folder. The result stays valid until the next few calls.
const char* path(const char* game_path);
// Data and save folders (sdmc:/switch/touhou/touhou17 on Switch).
const std::string& data_directory();
const std::string& save_directory();
// CP932 -> UTF-16 table in the layout of upstream's fonts/cp932.bin
// (65536 little-endian u16, index = byte or lead<<8|trail).
const unsigned char* cp932_table(std::size_t* size);
// Opens a font file once, keeps its bytes in memory and serves every later
// open of the same file (any size) from that copy.
TTF_Font* open_font(const char* game_path, int size);
}
