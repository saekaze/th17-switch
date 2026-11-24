#include "Platform.hpp"
#include "th17_native.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <sys/stat.h>
#include <unordered_map>

namespace th17::host {
Paths& paths(){static Paths p;return p;}

namespace {
FILE* log_file=nullptr;
std::string directory_of(const char* path){
    std::string s=path?path:"";const auto slash=s.find_last_of('/');
    return slash==std::string::npos?std::string():s.substr(0,slash);
}
}

void locate_data(int argc,char** argv){
    std::vector<std::string> candidates;
#ifdef __SWITCH__
    // hbmenu passes the NRO path as argv[0]; the folder next to it wins.
    if(argc>0&&argv[0]){const auto dir=directory_of(argv[0]);if(!dir.empty())candidates.push_back(dir);}
    // Data folders: thNN / touhouN (any capitalisation - FAT is case-insensitive)
    // directly on the SD card, in switch/, in a shared touhou/ or switch/touhou/
    // folder, or in games/ and roms/.
    for(const char* root:{"sdmc:/switch/","sdmc:/","sdmc:/touhou/","sdmc:/switch/touhou/","sdmc:/games/","sdmc:/roms/"})
        for(const char* name:{"th17","touhou17","touhou 17"})candidates.push_back(std::string(root)+name);
#else
    if(argc>1&&argv[1])candidates.push_back(argv[1]);
    candidates.push_back(".");
#endif
    for(const auto& c:candidates)if(exists(c+"/th17.dat")){paths().data=paths().save=c;break;}
    if(!exists(paths().data+"/th17.dat")&&!candidates.empty())paths().data=paths().save=candidates.front();
    make_directory(paths().save);
    log_file=std::fopen(paths().save_file("th17-switch.log").c_str(),"w");
}

void log(const char* fmt,...){
    va_list ap;va_start(ap,fmt);char line[1024];std::vsnprintf(line,sizeof line,fmt,ap);va_end(ap);
    std::fprintf(stderr,"%s\n",line);
    if(log_file){std::fprintf(log_file,"%s\n",line);std::fflush(log_file);}
}
bool exists(const std::string& path){struct stat st;return stat(path.c_str(),&st)==0;}
bool make_directory(const std::string& path){return mkdir(path.c_str(),0777)==0||exists(path);}
bool read_file(const std::string& path,std::vector<u8>& data,u64 limit){
    FILE* f=std::fopen(path.c_str(),"rb");if(!f)return false;
    std::fseek(f,0,SEEK_END);const long size=std::ftell(f);std::fseek(f,0,SEEK_SET);
    if(size<0||u64(size)>limit){std::fclose(f);return false;}
    data.resize(size_t(size));const bool ok=data.empty()||std::fread(data.data(),1,data.size(),f)==data.size();std::fclose(f);return ok;
}
bool write_file(const std::string& path,const u8* data,size_t size){
    const auto temporary=path+".tmp";FILE* f=std::fopen(temporary.c_str(),"wb");if(!f)return false;
    const bool written=!size||std::fwrite(data,1,size,f)==size;const bool closed=std::fclose(f)==0;
    if(!written||!closed){std::remove(temporary.c_str());return false;}
    std::remove(path.c_str()); // FAT (sdmc) rename does not replace.
    return std::rename(temporary.c_str(),path.c_str())==0;
}
double seconds(){return double(SDL_GetPerformanceCounter())/double(SDL_GetPerformanceFrequency());}
}

namespace th17native {
using th17::host::paths;
const char* path(const char* game_path){
    // A small ring: callers use the result immediately (open/stat/load).
    thread_local std::array<std::string,8> ring;thread_local unsigned next=0;
    if(!game_path)return "";
    if(game_path[0]!='/')return game_path; // already a host path (sdmc:/...)
    // Desktop builds: an absolute data folder is already a host path too.
    for(const auto* root:{&paths().data,&paths().save}){const auto n=root->size();
        if(n&&(*root)[0]=='/'&&!std::strncmp(game_path,root->c_str(),n)&&(game_path[n]=='/'||!game_path[n]))return game_path;}
    auto& slot=ring[next++%ring.size()];slot=paths().data+game_path;return slot.c_str();
}
const std::string& data_directory(){return paths().data;}
const std::string& save_directory(){return paths().save;}

using u16=unsigned short;
#include "cp932_table.inc"
const unsigned char* cp932_table(std::size_t* size){
    static std::array<unsigned char,131072> bytes=[]{std::array<unsigned char,131072> b{};for(unsigned n=0;n<65536;++n){b[n*2]=cp932_map[n]&255;b[n*2+1]=cp932_map[n]>>8;}return b;}();
    if(size)*size=bytes.size();return bytes.data();
}

TTF_Font* open_font(const char* game_path,int size){
    // Fonts are opened lazily, per text style, while the game runs. Keep each
    // file's bytes after the first open so later opens never touch the SD card.
    static std::map<std::string,std::vector<unsigned char>> files;static std::mutex lock;
    const std::string file=path(game_path);
    const std::vector<unsigned char>* bytes=nullptr;
    {
        std::lock_guard<std::mutex> guard(lock);auto it=files.find(file);
        if(it==files.end()){
            std::vector<th17::u8> data;
            if(!th17::host::read_file(file,data,64ull*1024*1024))return nullptr;
            it=files.emplace(file,std::vector<unsigned char>(data.begin(),data.end())).first;
            th17::host::log("font %s: %u KB in memory",file.c_str(),unsigned(it->second.size()/1024));
        }
        bytes=&it->second;
    }
    SDL_RWops* rw=SDL_RWFromConstMem(bytes->data(),int(bytes->size()));if(!rw)return nullptr;
    return TTF_OpenFontRW(rw,1,size);
}
}

namespace th17compat {
namespace {std::unordered_map<TTF_Font*,float>& sizes(){static std::unordered_map<TTF_Font*,float> m;return m;}}
float& font_size_slot(TTF_Font* font){return sizes()[font];}
void forget_font(TTF_Font* font){sizes().erase(font);}
}
