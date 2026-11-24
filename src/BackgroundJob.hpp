#pragma once
// Switch port: one job at a time on another CPU core (core 2 on Switch, away
// from the game thread on core 0). Used to read and decompress the next
// stage's files ahead of time (ArchivePrefetch), so the stage change does
// not do that LZSS work on the game thread.
#include <functional>
#ifdef __SWITCH__
#include <switch.h>
#else
#include <thread>
#endif
namespace th17::host {
class BackgroundJob {
    std::function<void()> job;
#ifdef __SWITCH__
    Thread thread{};bool running=false;
    static void entry(void* self){static_cast<BackgroundJob*>(self)->job();}
#else
    std::thread thread;
#endif
public:
    BackgroundJob()=default;BackgroundJob(const BackgroundJob&)=delete;BackgroundJob& operator=(const BackgroundJob&)=delete;
    ~BackgroundJob(){wait();}
    // Waits for the previous job, then starts this one (runs it inline if no
    // thread can be created).
    void run(std::function<void()> next){
        wait();job=std::move(next);
#ifdef __SWITCH__
        // Core 2, away from the game thread on core 0.
        if(R_SUCCEEDED(threadCreate(&thread,entry,this,nullptr,512*1024,0x2C,2))){
            if(R_SUCCEEDED(threadStart(&thread))){running=true;return;}
            threadClose(&thread);
        }
        job();
#else
        thread=std::thread([this]{job();});
#endif
    }
    void wait(){
#ifdef __SWITCH__
        if(running){threadWaitForExit(&thread);threadClose(&thread);running=false;}
#else
        if(thread.joinable())thread.join();
#endif
    }
};
}
