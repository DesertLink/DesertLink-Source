cmake_minimum_required(VERSION 3.20)
project(DesertLinkPrivateStorage LANGUAGES CXX C)

if(NOT WIN32)
    message(FATAL_ERROR "DesertLinkPrivateStorage targets Windows only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG master
)
FetchContent_MakeAvailable(minhook)

add_library(DesertLinkPrivateStorage SHARED
    src/dllmain.cpp
    src/private_storage.cpp
    src/core/log.cpp
    src/core/paths.cpp
    src/game/mem.cpp
    src/game/farhook.cpp
    src/game/addresses.cpp
)

target_include_directories(
    DesertLinkPrivateStorage
    PRIVATE
        src
        ${minhook_SOURCE_DIR}/src/hde
)

target_link_libraries(DesertLinkPrivateStorage PRIVATE minhook user32)

target_compile_definitions(
    DesertLinkPrivateStorage
    PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)

target_compile_options(DesertLinkPrivateStorage PRIVATE /utf-8 /W3 /EHsc)

set_target_properties(
    DesertLinkPrivateStorage
    PROPERTIES
        PREFIX ""
        SUFFIX ".asi"
        OUTPUT_NAME "DesertLinkPrivateStorage"
)

if(MSVC)
    target_compile_options(DesertLinkPrivateStorage PRIVATE /guard:cf)
    target_link_options(
        DesertLinkPrivateStorage
        PRIVATE
            /DYNAMICBASE
            /HIGHENTROPYVA
            /NXCOMPAT
            /guard:cf
    )
endif()
