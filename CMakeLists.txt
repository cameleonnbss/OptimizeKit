cmake_minimum_required(VERSION 3.20)
project(OptimizeKit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(OptimizeKit WIN32
    src/app/main.cpp
    src/app/cli.cpp
    src/core/common.cpp
    src/core/sysinfo.cpp
    src/core/tweaks.cpp
    src/core/cleaner.cpp
    src/core/engine.cpp
    src/core/ping.cpp
    src/core/drivers.cpp
    src/core/gameboost.cpp
    src/ui/ui.cpp
    resources/OptimizeKit.rc
)

target_include_directories(OptimizeKit PRIVATE src src/core)

target_link_libraries(OptimizeKit PRIVATE
    d2d1 dwrote dwrite windowscodecs
    user32 gdi32 shell32 advapi32 ole32 oleaut32
    shlwapi iphlpapi ws2_32 winmm uxtheme dwmapi uuid
    powrprof setupapi psapi
)

if (MINGW)
    target_link_options(OptimizeKit PRIVATE -static -static-libgcc -static-libstdc++ -municode)
    target_compile_options(OptimizeKit PRIVATE -O2 -Wall -Wno-unknown-pragmas)
else()
    target_compile_options(OptimizeKit PRIVATE /O2 /W3 /utf-8)
endif()
