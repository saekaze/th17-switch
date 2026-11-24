#include "ArchivePrefetch.hpp"
#include "Archive.hpp"
#include "Platform.hpp"
#include <cstdio>
namespace th17::host {
ArchivePrefetch& archive_prefetch(){static ArchivePrefetch p;return p;}

void ArchivePrefetch::start(const std::string& path,std::vector<std::string> names){
    clear();if(names.empty())return;
    archive_path=path;requested.insert(names.begin(),names.end());
    // The worker owns its own file handle and Archive (the game thread keeps
    // reading through its own); `ready` is only touched by the main thread
    // after job.wait().
    job.run([this,names=std::move(names)]{
        const double begin=seconds();
        FILE* file=std::fopen(archive_path.c_str(),"rb");if(!file)return;
        std::fseek(file,0,SEEK_END);const long size=std::ftell(file);
        Archive archive;
        const auto read_at=[file](u32 offset,u8* bytes,u32 count){return std::fseek(file,long(offset),SEEK_SET)==0&&std::fread(bytes,1,count,file)==count;};
        if(size>0&&archive.open(u32(size),read_at)){
            size_t total=0;
            for(const auto& name:names)for(u32 i=0;i<archive.entries.size();++i)if(archive.entries[i].name==name){
                std::vector<u8> bytes;if(archive.read(i,bytes)){total+=bytes.size();ready.emplace(name,std::move(bytes));}break;}
            log("prefetched %zu files (%zu KB) in %.0fms",ready.size(),total/1024,(seconds()-begin)*1000);
        }
        std::fclose(file);
    });
}
bool ArchivePrefetch::take(const std::string& name,std::vector<u8>& bytes){
    if(!requested.count(name))return false;
    job.wait();requested.erase(name);
    auto it=ready.find(name);if(it==ready.end())return false;
    bytes=std::move(it->second);ready.erase(it);return true;
}
void ArchivePrefetch::clear(){job.wait();requested.clear();ready.clear();}
}
