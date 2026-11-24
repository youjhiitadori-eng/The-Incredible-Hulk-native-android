cmake_minimum_required(VERSION 3.18)
project(hulk C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# app/src/main/cpp -> naik 4 level = root repo
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(TP "${REPO_ROOT}/third_party")

if(NOT EXISTS "${TP}/SDL/CMakeLists.txt")
    message(FATAL_ERROR "third_party kosong. Jalankan: bash scripts/fetch_deps.sh")
endif()

set(SDL_SHARED ON  CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
add_subdirectory("${TP}/SDL" "${CMAKE_BINARY_DIR}/SDL2")

# SDLActivity memuat library bernama "main", jadi target engine WAJIB bernama main
add_library(main SHARED
    main.cpp
    third_party_impl.cpp
)

target_include_directories(main PRIVATE
    "${TP}/SDL/include"
    "${TP}/miniaudio"
    "${TP}/stb"
    "${REPO_ROOT}/engine"
)

if(TARGET SDL2::SDL2)
    set(SDL_LIB SDL2::SDL2)
elseif(TARGET SDL2-shared)
    set(SDL_LIB SDL2-shared)
else()
    set(SDL_LIB SDL2)
endif()

if(TARGET SDL2::SDL2main)
    set(SDLMAIN_LIB SDL2::SDL2main)
elseif(TARGET SDL2main)
    set(SDLMAIN_LIB SDL2main)
else()
    set(SDLMAIN_LIB "")
endif()

target_link_libraries(main PRIVATE ${SDL_LIB} ${SDLMAIN_LIB} GLESv2 EGL android log dl)
target_compile_options(main PRIVATE -Os -Wall)
