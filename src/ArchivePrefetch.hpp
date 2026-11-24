// Switch port: reads th17.dat entries ahead of time on another core.
//
// A stage change loads the next stage's files (stNN.std/.ecl/.msg and its
// enemy and background ANMs, ~10 MB after LZSS). Doing that on the game
// thread made the stage change one long frame; the files are known while the
// previous stage is still being played, so they are read and decompressed in
// the background and handed over when the game asks for them.
#pragma once
#include "BackgroundJob.hpp"
#include "Types.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace th17::host {
class ArchivePrefetch {
    BackgroundJob job;
    std::unordered_set<std::string> requested;
    std::unordered_map<std::string,std::vector<u8>> ready;
    std::string archive_path;
public:
    // Drops anything prefetched earlier and starts reading `names` from the
    // archive at `path` on the worker.
    void start(const std::string& path,std::vector<std::string> names);
    // Moves a prefetched entry into `bytes` (waiting for the worker if it is
    // still reading). False if `name` was not requested or could not be read.
    bool take(const std::string& name,std::vector<u8>& bytes);
    void clear();
};
ArchivePrefetch& archive_prefetch();
}
