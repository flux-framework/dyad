# This minimum version is currently arbitrary.
# TODO change once we pick a real minimum version
cmake_minimum_required(VERSION 3.16)

project(dyad
    VERSION 0.2.0
    LANGUAGES C CXX
)

option(ENABLE_DYAD_DEBUG "Include debugging prints and logging" OFF)
option(BUILD_URPC "Build DYAD's URPC code" OFF)
option(ENABLE_PERFFLOW "Build with PerfFlow Aspect support" OFF)
option(ENABLE_UCX_DTL "Build DYAD's UCX data transport layer" OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Import pkg-config
# This is mainly needed for finding FluxCore
find_package(PkgConfig REQUIRED)
find_package(FluxCore REQUIRED)
find_package(Jansson REQUIRED VERSION 2.10)
find_package(perfflowaspect CONFIG)
find_package(ucx CONFIG VERSION 1.6)

if (ENABLE_PERFFLOW AND NOT perfflowaspect_FOUND)
    message(FATAL_ERROR "Requested PerfFlow Aspect support, but could not find PerfFlow")
endif (ENABLE_PERFFLOW AND NOT perfflowaspect_FOUND)
if (ENABLE_UCX_DTL AND NOT ucx_FOUND)
    message(FATAL_ERROR "Requested the UCX DTL, but could not find UCX")
endif (ENABLE_UCX_DTL AND NOT ucx_FOUND)

if (ENABLE_DYAD_DEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDYAD_FULL_DEBUG=1 -DDYAD_LOGGING_ON=1")
    set(CMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS} -DDYAD_FULL_DEBUG=1 -DDYAD_LOGGING_ON=1"
endif (ENABLE_DYAD_DEBUG)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(DYAD_BINDIR "bin")
set(DYAD_INCLUDEDIR "include")
set(DYAD_LIBDIR "lib")

add_subdirectory(src)