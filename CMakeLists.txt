cmake_minimum_required(VERSION 3.16)
project(th17_switch C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
# Upstream builds with -O2/-O3; -O2 here, set explicitly below.
set(CMAKE_CXX_FLAGS_RELEASE "")
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Upstream sources, fetched by scripts/fetch_upstream.sh.
set(UP "${CMAKE_SOURCE_DIR}/upstream")
set(TH17 "${UP}/ports/th17")
if(NOT EXISTS "${TH17}/web/cpp/sdl/Application.cpp")
    message(FATAL_ERROR "upstream/ is missing: run scripts/fetch_upstream.sh first.")
endif()
file(GLOB TH17_GAME CONFIGURE_DEPENDS "${TH17}/web/cpp/game/*.cpp")
# Upstream's SDL host minus the browser runtime (replaced by src/main.cpp)
# and the ImGui practice overlay (thprac; not part of this port yet).
file(GLOB TH17_HOST CONFIGURE_DEPENDS "${TH17}/web/cpp/sdl/*.cpp")
list(REMOVE_ITEM TH17_HOST "${TH17}/web/cpp/sdl/BrowserRuntime.cpp" "${TH17}/web/cpp/sdl/ThpracUi.cpp")
file(GLOB PORT_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM PORT_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")

# CP932 -> UTF-16 table (upstream's fonts/cp932.bin, built into the program).
set(CP932_TABLE ${CMAKE_BINARY_DIR}/generated/cp932_table.inc)
add_custom_command(OUTPUT ${CP932_TABLE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/generated
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/gen_cp932.py ${CP932_TABLE}
    DEPENDS ${CMAKE_SOURCE_DIR}/scripts/gen_cp932.py
    COMMENT "Generating CP932 table" VERBATIM)

add_library(th17_core OBJECT ${TH17_GAME} ${TH17_HOST} "${TH17}/portable/sdl/Renderer.cpp" ${PORT_SOURCES} ${CP932_TABLE})
target_include_directories(th17_core PUBLIC
    compat src
    "${TH17}/web/cpp/sdl" "${TH17}/web/cpp/game"
    "${TH17}/portable/sdl" "${TH17}/portable/input"
    "${UP}/common/include"
    ${CMAKE_BINARY_DIR}/generated)
# Same code-generation contract as the upstream web build: no FMA
# contraction (bit-exact float order with the original, so replays and the
# RNG match), no strict aliasing, no exceptions/RTTI. GCC defaults to
# -ffp-contract=fast on AArch64, so this flag is load-bearing.
target_compile_options(th17_core PUBLIC
    -O2 -ffp-contract=off -fno-strict-aliasing
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>
    -Wno-attributes $<$<CXX_COMPILER_ID:GNU>:-Wno-class-memaccess> -Wno-misleading-indentation -Wno-parentheses)
target_compile_definitions(th17_core PUBLIC TH_NATIVE_PLATFORM=1 TH17_DEVELOPMENT_HARNESS=0)

add_executable(touhou17 src/main.cpp)
target_link_libraries(touhou17 PRIVATE th17_core)

if(NINTENDO_SWITCH)
    # ---- Nintendo Switch (devkitA64; configure with the devkitPro Switch
    # toolchain file, which defines NINTENDO_SWITCH) ----
    # Layout/naming follows the Saekaze Touhou ports: touhou17.nro +
    # sdmc:/switch/touhou/touhou17/ (th17.dat, thbgm.dat, msgothic.ttc).
    if(NOT DEFINED ENV{DEVKITPRO})
        set(ENV{DEVKITPRO} "/opt/devkitpro")
    endif()
    set(TH_DEVKITPRO "$ENV{DEVKITPRO}")
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/platform/switch/icon.jpg")
        message(FATAL_ERROR "platform/switch/icon.jpg (256x256 JPEG) not found.")
    endif()
    target_compile_definitions(th17_core PUBLIC __SWITCH__)
    target_include_directories(th17_core PUBLIC
        ${TH_DEVKITPRO}/portlibs/switch/include
        ${TH_DEVKITPRO}/libnx/include)
    target_link_directories(touhou17 PRIVATE
        ${TH_DEVKITPRO}/portlibs/switch/lib
        ${TH_DEVKITPRO}/libnx/lib)
    # Static link order matters: dependents first, dependencies after.
    target_link_libraries(touhou17 PRIVATE
        SDL2_ttf SDL2 EGL glapi drm_nouveau GLESv2 freetype harfbuzz png bz2 z nx m)

    # Default (all languages): romanised title. The original Japanese title is
    # patched into the Japanese slot only - same as the other ports.
    nx_generate_nacp(OUTPUT "${CMAKE_BINARY_DIR}/touhou17-base.nacp"
        NAME "Touhou 17: Wily Beast and Weakest Creature"
        AUTHOR "ZUN / Team Shanghai Alice"
        VERSION "1.00b-r1")
    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/touhou17.nacp"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/nacp_lang.py"
                "${CMAKE_BINARY_DIR}/touhou17-base.nacp" "${CMAKE_BINARY_DIR}/touhou17.nacp"
                --lang Japanese
                --name "東方鬼形獣　～ Wily Beast and Weakest Creature"
                --author "上海アリス幻樂団"
                --fill-empty-name "Touhou 17: Wily Beast and Weakest Creature"
                --fill-empty-author "ZUN / Team Shanghai Alice"
        DEPENDS "${CMAKE_BINARY_DIR}/touhou17-base.nacp" "${CMAKE_SOURCE_DIR}/scripts/nacp_lang.py"
        COMMENT "Patching Japanese title into NACP"
        VERBATIM)
    nx_create_nro(touhou17
        ICON "${CMAKE_SOURCE_DIR}/platform/switch/icon.jpg"
        NACP "${CMAKE_BINARY_DIR}/touhou17.nacp")
else()
    # ---- Linux test build (same sources, SDL2 + SDL2_ttf + GLES3) ----
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2TTF REQUIRED SDL2_ttf)
    target_include_directories(th17_core PUBLIC ${SDL2_INCLUDE_DIRS} ${SDL2TTF_INCLUDE_DIRS})
    find_package(Threads REQUIRED)
    target_link_libraries(touhou17 PRIVATE ${SDL2TTF_LIBRARIES} ${SDL2_LIBRARIES} GLESv2 m Threads::Threads)
endif()
