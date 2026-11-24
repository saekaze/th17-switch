// Touhou 17 for Nintendo Switch: native entry point.
//
// This replaces ports/th17/web/cpp/sdl/BrowserRuntime.cpp. The
// ApplicationState it drives is upstream's (title, runs, endings, replays,
// audio and text); the browser parts become: files on the SD card instead of
// IDBFS, a vsync-paced 60 Hz loop instead of requestAnimationFrame, and
// Switch buttons / the touch screen feeding the same keyboard map, gamepad
// sampling and TouchController the web launcher forwards into.
#include "ApplicationState.hpp"
#include "Platform.hpp"
#include "../game/GameKeyboard.hpp"
#include "../../../portable/input/TouchController.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#ifdef __SWITCH__
#include <switch.h>
#include <malloc.h>
#endif

namespace th17::sdl {
namespace {
std::unique_ptr<ApplicationState> app;
struct Key {const char* code;const char* sdl;u32 scan,vk;bool hosted=false;SDL_Scancode native=SDL_SCANCODE_UNKNOWN;};
#include "../../../portable/input/KeyboardMap.inc"
touhou::input::TouchController gestures;
GameInput controls;
double window_start=0;u32 window_ticks=0;float measured_fps=60;
// Presentation counts committed frames, independently of simulation ticks
// (upstream's on-screen FPS figure, drawn by SceneDisplay).
double presentation_start=-1;u32 presentation_frames=0;float presentation_fps=60;

Key* key(const char* code){for(auto& k:keyboard_map)if(!std::strcmp(k.code,code))return &k;return nullptr;}
void host_key(const char* code,bool down){if(auto* k=key(code))k->hosted=down;}
void clear_inputs(){
    for(auto& k:keyboard_map)k.hosted=false;gestures.reset();controls={};
    if(app){app->keyboard_state={};app->touch={};if(app->scene())app->scene()->battle.player->motion.touch={};}
}
// Upstream touch_state (single player: seat 0).
touhou::input::TouchState touch_state(){
    touhou::input::TouchState value;
    if(app&&app->title)value.instance=1+int(app->title->state.screen)*256+app->title->state.substate;
    if(!app||!app->scene()||!app->run_input)return value;
    if(app->pause&&app->pause->state.screen!=PauseScreen::Inactive)return value;
    if(app->progress.practice_record){value.context=3;return value;}
    auto& stage=*app->scene();auto& player=stage.battle.pilot(0);auto& motion=player.motion;
    value.context=stage.battle.dialogue_present()?2:1;
    value.instance=app->progress.stage;value.ready=player.life.state==1&&!(app->progress.scene_flags&(4|0x70|0x4000|0x10000));
    value.x=motion.position.x;value.y=motion.position.y;value.fast=float(motion.normal_speed)/128;value.slow=float(motion.focus_speed)/128;
    value.min_x=-184;value.max_x=184;value.min_y=32;value.max_y=432;return value;
}

// Switch controller as TH17's gamepad (upstream samples an SDL joystick the
// same way): Key Config binds shot / bomb / focus / pause to these button
// numbers, and the Switch defaults below give the layout of the other ports.
//   0 B  1 A  2 L/ZL  3 R/ZR  4 +  5 X  6 Y
// The d-pad and sticks only move - they are never buttons, so Key Config
// cannot pick them. R/ZR also holds Ctrl (skip dialogue) and X / Y keep their
// Enter / Retry shortcuts while no action is bound to them. − = snapshot.
constexpr i16 switch_defaults[4]{0,1,2,4}; // shot B, bomb A, focus L, pause +
struct PadInput {u32 buttons=0;float x=0,y=0;bool minus=false;};
#ifdef __SWITCH__
PadState pad;
PadInput read_pad(){
    padUpdate(&pad);PadInput p;if(!padIsConnected(&pad))return p;
    const u64 held=padGetButtons(&pad);const auto stick=padGetStickPos(&pad,0);
    const u64 map[7]{HidNpadButton_B,HidNpadButton_A,HidNpadButton_L|HidNpadButton_ZL,HidNpadButton_R|HidNpadButton_ZR,HidNpadButton_Plus,HidNpadButton_X,HidNpadButton_Y};
    for(u32 b=0;b<7;++b)if(held&map[b])p.buttons|=1u<<b;
    p.x=float(stick.x)/32768;p.y=-float(stick.y)/32768;
    if(held&HidNpadButton_Left)p.x=-1;if(held&HidNpadButton_Right)p.x=1;if(held&HidNpadButton_Up)p.y=-1;if(held&HidNpadButton_Down)p.y=1;
    p.minus=held&HidNpadButton_Minus;return p;
}
#else
// Desktop test builds: the first SDL game controller, same button numbers.
SDL_GameController* desktop_pad=nullptr;
PadInput read_pad(){
    PadInput p;
    if(!desktop_pad)for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)){desktop_pad=SDL_GameControllerOpen(i);break;}
    if(!desktop_pad)return p;
    if(!SDL_GameControllerGetAttached(desktop_pad)){SDL_GameControllerClose(desktop_pad);desktop_pad=nullptr;return p;}
    const SDL_GameControllerButton map[7]{SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,SDL_CONTROLLER_BUTTON_START,SDL_CONTROLLER_BUTTON_Y,SDL_CONTROLLER_BUTTON_X};
    for(u32 b=0;b<7;++b)if(SDL_GameControllerGetButton(desktop_pad,map[b]))p.buttons|=1u<<b;
    p.x=float(SDL_GameControllerGetAxis(desktop_pad,SDL_CONTROLLER_AXIS_LEFTX))/32768;p.y=float(SDL_GameControllerGetAxis(desktop_pad,SDL_CONTROLLER_AXIS_LEFTY))/32768;
    if(SDL_GameControllerGetButton(desktop_pad,SDL_CONTROLLER_BUTTON_DPAD_LEFT))p.x=-1;if(SDL_GameControllerGetButton(desktop_pad,SDL_CONTROLLER_BUTTON_DPAD_RIGHT))p.x=1;
    if(SDL_GameControllerGetButton(desktop_pad,SDL_CONTROLLER_BUTTON_DPAD_UP))p.y=-1;if(SDL_GameControllerGetButton(desktop_pad,SDL_CONTROLLER_BUTTON_DPAD_DOWN))p.y=1;
    p.minus=SDL_GameControllerGetButton(desktop_pad,SDL_CONTROLLER_BUTTON_BACK);return p;
}
#endif
bool snapshot_held=false,snapshot_requested=false;

// Upstream sample_controller: bound buttons -> actions, half-way stick -> arrows.
u32 controller_keys(const PadInput& pad){
    u32 keys=0;constexpr u32 actions[]={1,2,8,0x100};
    for(u32 i=0;i<4;i++){const i32 binding=app->controller.values[i];if(binding>=0&&(pad.buttons&(1u<<(u32(binding)&31))))keys|=actions[i];}
    if(pad.x<-.5f)keys|=0x40;if(pad.x>.5f)keys|=0x80;if(pad.y<-.5f)keys|=0x10;if(pad.y>.5f)keys|=0x20;return keys;
}
bool bound(i32 button){for(u32 i=0;i<4;i++)if(app->controller.values[i]==button)return true;return false;}

void touch_event(const SDL_Event& e){
    // SDL2 reports touches normalised to the whole screen; the game wants the
    // 4:3 picture, which is pillarboxed inside it.
    const auto& p=app->graphics.backend.picture;if(p.width<=0||p.height<=0)return;
    const float x=(e.tfinger.x*p.drawable_width-p.x)/p.width,y=(e.tfinger.y*p.drawable_height-p.y)/p.height;
    const int type=e.type==SDL_FINGERDOWN?0:e.type==SDL_FINGERUP?2:1; // TouchController: 0 down, 1 move, 2 up
    if(type==0&&(x<0||x>1||y<0||y>1))return;
    gestures.pointer(type,int(e.tfinger.fingerId),std::clamp(x,0.f,1.f),std::clamp(y,0.f,1.f),SDL_GetTicks(),touch_state(),false);
}

// Test hook (desktop only): TH17_SCRIPT=file replays "frame mask" lines of
// held game bits, for unattended runs.
struct ScriptInput {std::vector<std::pair<u32,u32>> steps;size_t next=0;u32 held=0;bool active=false;} script;
u32 script_frame=0;

bool sample_and_tick(){
    const PadInput pad=read_pad();
    host_key("ControlLeft",pad.buttons&(1u<<3));
    host_key("Enter",(pad.buttons&(1u<<5))&&!bound(5));host_key("KeyR",(pad.buttons&(1u<<6))&&!bound(6));
    if(pad.minus&&!snapshot_held)snapshot_requested=true;snapshot_held=pad.minus;
    bool keys[256]{};const auto* physical=SDL_GetKeyboardState(nullptr);
    for(const auto& k:keyboard_map)if(k.hosted||(k.native!=SDL_SCANCODE_UNKNOWN&&physical[k.native])){if(k.vk<256)keys[k.vk]=true;if(k.vk>=160&&k.vk<=165)keys[16+(k.vk-160)/2]=true;if(k.scan==28||k.scan==156)keys[13]=true;}
    app->keyboard_state.format=2;for(u32 i=0;i<256;i++)app->keyboard_state.keys[i]=keys[i]?128:0;
    const auto sample=gestures.sample(touch_state(),SDL_GetTicks(),keys[16],keys[37]||keys[38]||keys[39]||keys[40]);
    for(u32 i=0;i<256;i++)keys[i]=keys[i]||sample.keys[i];app->touch={sample.motion,sample.x,sample.y};
    u32 held=keyboard_keys(keys)|controller_keys(pad);
    if(script.active){while(script.next<script.steps.size()&&script.steps[script.next].first<=script_frame)script.held=script.steps[script.next++].second;held|=script.held;}
    ++script_frame;
    controls.update(held);app->controller_buttons=pad.buttons;
    const bool ok=app->step(controls.held,controls.pressed,controls.repeated,measured_fps,false);
    const double now=host::seconds();if(!window_start)window_start=now;
    if(ok){window_ticks++;const double duration=now-window_start;if(duration>=1){measured_fps=float(window_ticks/duration);window_ticks=0;window_start=now;}}
    return ok;
}

// Switch port: '−' saves the current frame as a BMP into snapshot/ next to
// the saves, like the PC game's snapshot folder.
bool snapshot(const char* name=nullptr){
    auto& gpu=app->graphics.backend;gpu.flush();gpu.read(GraphicsDevice::screen);auto* image=app->graphics.pixels(GraphicsDevice::screen);if(!image||image->pixels.empty())return false;
    const auto dir=host::paths().save_file("snapshot");host::make_directory(dir);char file[64];std::string path;
    if(name)path=dir+"/"+name;
    else for(u32 n=0;n<1000;++n){std::snprintf(file,sizeof(file),"/th17_%03u.bmp",n);path=dir+file;if(!host::exists(path))break;}
    const u32 w=image->width,h=image->height,row=w*3,pad=(4-row%4)%4,size=54+(row+pad)*h;std::vector<u8> bmp(size,0);
    auto put=[&](u32 at,u32 v,u32 n){for(u32 i=0;i<n;++i)bmp[at+i]=u8(v>>(8*i));};
    bmp[0]='B';bmp[1]='M';put(2,size,4);put(10,54,4);put(14,40,4);put(18,w,4);put(22,h,4);put(26,1,2);put(28,24,2);put(34,size-54,4);
    for(u32 y=0;y<h;++y){const u8* src=image->pixels.data()+(h-1-y)*image->pitch;u8* dst=bmp.data()+54+y*(row+pad);for(u32 x=0;x<w;++x){dst[x*3]=src[x*4];dst[x*3+1]=src[x*4+1];dst[x*3+2]=src[x*4+2];}}
    const bool ok=host::write_file(path,bmp);host::log("snapshot %s: %s",path.c_str(),ok?"ok":"failed");return ok;
}

void fatal(const std::string& message){
    host::log("fatal: %s",message.c_str());
    // Tear down the game (GL context, window, audio) so the console can take
    // the framebuffer, then wait for +.
    app.reset();SDL_Quit();
#ifdef __SWITCH__
    consoleInit(nullptr);
    std::printf("\n  Touhou 17: Wily Beast and Weakest Creature - Switch port\n\n  Error: %s\n\n",message.c_str());
    std::printf("  Data folder: %s\n  Needed: th17.dat (v1.00b) and msgothic.ttc. Optional: thbgm.dat\n\n  Press + to exit.\n",host::paths().data.c_str());
    consoleUpdate(nullptr);
    while(appletMainLoop()){padUpdate(&pad);if(padGetButtonsDown(&pad)&HidNpadButton_Plus)break;consoleUpdate(nullptr);}
    consoleExit(nullptr);
#else
    std::fprintf(stderr,"%s\n",message.c_str());
#endif
}

bool start(){
    app=std::make_unique<ApplicationState>();
    app->graphics.render_scale=1;app->audio_device.music_enabled=true;
    const double begin=host::seconds();
    if(!app->initialize(true))return false;
    host::log("game initialised in %.1fs",host::seconds()-begin);
    // First launch (no th17.cfg yet) leaves every action on button 0; use the
    // Switch layout instead. Key Config changes are kept afterwards.
    auto& v=app->controller.values;
    if(v[0]==v[1]&&v[1]==v[2]&&v[2]==v[3]){for(u32 i=0;i<4;i++)v[i]=switch_defaults[i];if(!app->save(app->controller))return false;}
    for(auto& k:keyboard_map)k.native=SDL_GetScancodeFromName(k.sdl);
    clear_inputs();return true;
}

// Memory in use, for the log. Switch: what malloc has handed out, out of the
// memory the process has. The kernel's own "used" figure is no help: libnx
// reserves the whole heap at start, so it always reads full.
void log_memory(const char* when){
#ifdef __SWITCH__
    const struct mallinfo heap=mallinfo();
    u64 total=0;svcGetInfo(&total,InfoType_TotalMemorySize,CUR_PROCESS_HANDLE,0);
    host::log("memory %s: %llu MB allocated of %llu MB",when,(unsigned long long)(heap.uordblks>>20),(unsigned long long)(total>>20));
#else
    FILE* f=std::fopen("/proc/self/status","r");if(!f)return;char line[256];
    while(std::fgets(line,sizeof line,f))if(!std::strncmp(line,"VmRSS:",6)){line[std::strcspn(line,"\n")]=0;host::log("memory %s: %s",when,line+6);}
    std::fclose(f);
#endif
}

void load_script(){
#ifndef __SWITCH__
    const char* file=std::getenv("TH17_SCRIPT");if(!file)return;
    FILE* f=std::fopen(file,"r");if(!f){host::log("script %s: not found",file);return;}
    unsigned frame=0,mask=0;while(std::fscanf(f,"%u %x",&frame,&mask)==2)script.steps.push_back({frame,mask});std::fclose(f);
    script.active=true;host::log("script %s: %zu steps",file,script.steps.size());
#endif
}
}
}

int main(int argc,char** argv){
    using namespace th17::sdl;namespace host=th17::host;using th17::u32;using th17::i32;
    host::locate_data(argc,argv);
#ifdef __SWITCH__
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);
    appletSetFocusHandlingMode(AppletFocusHandlingMode_SuspendHomeSleep);
#endif
    host::log("th17-switch: data %s",host::paths().data.c_str());
    if(!host::exists(host::paths().data_file("th17.dat"))){fatal("th17.dat not found");return 1;}
    // SDL_Init here is the SDL3-style wrapper from compat/SDL3/SDL.h: true = success.
    if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_AUDIO)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)){fatal(SDL_GetError());return 1;}
    if(!start()){fatal(app?app->failure:"start-up failed");return 1;}
    load_script();
    // Exactly one game tick per vsync (60 Hz). A slow frame slows the game
    // down for that frame instead of running extra catch-up ticks in the next
    // one: catch-up bursts are what froze spell captures in the TH10 port
    // (r6), where a 30 ms capture frame turned into a run of 2-4 tick frames.
#ifndef __SWITCH__
    const char* frames_env=std::getenv("TH17_FRAMES");const u32 frame_limit=frames_env?u32(std::strtoul(frames_env,nullptr,10)):0;
    const char* nosync=std::getenv("TH17_NOVSYNC");if(nosync)SDL_GL_SetSwapInterval(0);
    // Test hooks: TH17_SNAP_EVERY=n saves a frame every n frames;
    // TH17_TEST_TOOLS=hex turns practice tools (PracticeTool bits) on in runs.
    const char* snap_env=std::getenv("TH17_SNAP_EVERY");const u32 snap_every=snap_env?u32(std::strtoul(snap_env,nullptr,10)):0;
    // TH17_LOGICAL=1: skip GPU draws (upstream's rollback re-simulation mode),
    // for fast logic-only runs under the sanitizers.
    const char* logical_env=std::getenv("TH17_LOGICAL");
    // A number: logic-only up to that frame, drawn from there on.
    const u32 logical_until=logical_env?(std::strtoul(logical_env,nullptr,10)>1?u32(std::strtoul(logical_env,nullptr,10)):~0u):0;
    const bool trace=std::getenv("TH17_TRACE")!=nullptr;th17::i32 last_trace=-1;
    const char* stage_env=std::getenv("TH17_TEST_STAGE");const u32 test_stage=stage_env?u32(std::strtoul(stage_env,nullptr,10)):0;
    const char* tools_env=std::getenv("TH17_TEST_TOOLS");const u32 test_tools=tools_env?u32(std::strtoul(tools_env,nullptr,16)):0;
#endif
    bool running=true,ok=true;u32 frame_count=0;
    double spike_window=0;u32 spike_lines=0;i32 last_where=-1;
    double worst=0,total=0;u32 slow=0;
    presentation_start=host::seconds();
    while(running&&ok
#ifdef __SWITCH__
          &&appletMainLoop()
#endif
    ){
        SDL_Event e;while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT)running=false;
            else if(e.type==SDL_FINGERDOWN||e.type==SDL_FINGERUP||e.type==SDL_FINGERMOTION)touch_event(e);
            else if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_LOST)clear_inputs();
        }
        if(app->display)app->display->presentation_fps=presentation_fps;
        const double tick_begin=host::seconds();
        app->graphics.backend.defer=true;
#ifndef __SWITCH__
        if(test_tools&&app->scene()&&!app->progress.practice_record){app->practice.enabled=true;app->practice.tools=test_tools;}
        if(logical_until)app->graphics.logical_drawing=frame_count<logical_until;
        // TH17_TEST_STAGE=n: a Game Start loads stage n instead (7 = Extra,
        // which GameStart then plays at Extra difficulty).
        if(test_stage&&app->loading_destination==th17::AppScene::Game&&app->progress.stage==1)app->progress.stage=app->progress.initial_stage=th17::i32(test_stage);
#endif
        ok=sample_and_tick();
        const double tick_time=host::seconds()-tick_begin;total+=tick_time;worst=std::max(worst,tick_time);if(tick_time>1./60)++slow;
        if(ok&&snapshot_requested){snapshot_requested=false;snapshot();}
#ifndef __SWITCH__
        if(ok&&snap_every&&frame_count%snap_every==0){char name[32];std::snprintf(name,sizeof name,"f%06u.bmp",frame_count);snapshot(name);}
#endif
        // Slow frames go to the log (at most 20 a minute, so logging cannot
        // add hitches of its own), with where in the game they happened.
        if(tick_time>.025){
            if(host::seconds()-spike_window>60){spike_window=host::seconds();spike_lines=0;}
            if(spike_lines++<20){const auto* p=app->projection();host::log("slow frame %u: %.1fms scene %d stage %d age %d pause %d",frame_count,tick_time*1000,p[0],p[2],p[5],p[14]);}
        }
        {const auto* p=app->projection();const i32 where=p[0]*16+p[2];if(where!=last_where){last_where=where;char when[48];std::snprintf(when,sizeof when,"scene %d stage %d",p[0],p[2]);log_memory(when);}}
        app->graphics.backend.commit();app->graphics.backend.defer=false;app->audio_device.pump();
        if(ok)++presentation_frames;++frame_count;
        const double elapsed=host::seconds()-presentation_start;if(elapsed>=1){presentation_fps=float(presentation_frames/elapsed);presentation_start=host::seconds();presentation_frames=0;}
        if(ok&&app->exiting)running=false; // title menu Quit
#ifndef __SWITCH__
        if(frame_limit&&frame_count>=frame_limit)running=false;
        if(trace&&app->scene()){const auto* p=app->projection();if(p[5]>0&&p[5]%120==0&&p[5]!=last_trace){last_trace=p[5];host::log("trace stage %d age %d score %d lives %d bombs %d power %d replay %d",p[2],p[5],p[8],p[9],p[10],p[11],int(app->progress.practice_record));}}
        if(frame_count%600==0){const auto* p=app->projection();host::log("frame %u: scene %d flow %d stage %d score %d lives %d bombs %d power %d tick avg %.2fms worst %.2fms slow %u",frame_count,p[0],p[1],p[2],p[8],p[9],p[10],p[11],total/600*1000,worst*1000,slow);total=worst=0;slow=0;}
#endif
    }
    if(!ok){fatal(app->failure.empty()?"game stopped":app->failure);return 1;}
    app->save_settings();
    app.reset();
    SDL_Quit();
    return 0;
}
