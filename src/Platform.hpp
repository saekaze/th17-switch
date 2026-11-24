// Switch host: paths, files and logging shared by the devices.
#pragma once
#include "../upstream/ports/th17/web/cpp/game/Types.hpp"
#include <string>
#include <vector>

namespace th17::host {
// Game data folder (th17.dat, thbgm.dat, msgothic.ttc) and save folder. On
// Switch both are the folder the NRO lives in (sdmc:/switch/touhou/touhou17 by
// default), the same layout as the other Saekaze ports. Saves sit next to the
// data like the PC release: scoreth17.dat, th17.cfg, replay/, snapshot/.
struct Paths {
    std::string data="sdmc:/switch/touhou/touhou17";
    std::string save="sdmc:/switch/touhou/touhou17";
    std::string data_file(const char* name)const{return data+"/"+name;}
    std::string save_file(const char* name)const{return save+"/"+name;}
};
Paths& paths();
void locate_data(int argc,char** argv);

void log(const char* fmt,...);
bool exists(const std::string& path);
bool make_directory(const std::string& path);
bool read_file(const std::string& path,std::vector<u8>& data,u64 limit);
// Writes through a temporary file and renames (FAT: the old file is removed
// first, rename does not replace).
bool write_file(const std::string& path,const u8* data,size_t size);
inline bool write_file(const std::string& path,const std::vector<u8>& data){return write_file(path,data.data(),data.size());}
double seconds();
}
