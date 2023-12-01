cmake_minimum_required(VERSION 2.8.12)
#------------------------------------------------------------------------------
# Version information
#------------------------------------------------------------------------------

set(DYAD_VERSION_MAJOR "0")
set(DYAD_VERSION_MINOR "2")
set(DYAD_VERSION_PATCH "0")
set(DYAD_PACKAGE ${PROJECT_NAME})
set(DYAD_PACKAGE_NAME ${PROJECT_NAME})
set(DYAD_PACKAGE_VERSION "${DYAD_VERSION_MAJOR}.${DYAD_VERSION_MINOR}.${DYAD_VERSION_PATCH}")
set(DYAD_PACKAGE_VERSION_MAJOR "${DYAD_VERSION_MAJOR}.${DYAD_VERSION_MINOR}")
set(DYAD_PACKAGE_VERSION_MINOR "${DYAD_VERSION_PATCH}")
set(DYAD_PACKAGE_STRING "${DYAD_PACKAGE_NAME} ${DYAD_PACKAGE_VERSION}")
set(DYAD_PACKAGE_TARNAME "${DYAD_PACKAGE}")

project(dyad LANGUAGES C CXX)
#------------------------------------------------------------------------------
# Internal Includes for header and libraries
#------------------------------------------------------------------------------
link_directories(${CMAKE_BINARY_DIR}/lib)
include_directories(${CMAKE_BINARY_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src) # required for private includes
# Do we want to use RPath to simplify linking
set(CMAKE_MACOSX_RPATH 1)
set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_C_VISIBILITY_PRESET hidden)
#set(CMAKE_CXX_VISIBILITY_PRESET hidden)
#------------------------------------------------------------------------------
# Internal Paths for cmake libraries and Setup install and output Directories
#------------------------------------------------------------------------------
# This sets where to look for dependent libraries
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_BINARY_DIR} ${CMAKE_INSTALL_PREFIX})
# This sets where to look for dependent library's cmake files
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_BINARY_DIR}/lib/cmake)

if (NOT DYAD_INSTALL_BIN_DIR)
    set(DYAD_INSTALL_BIN_DIR ${CMAKE_INSTALL_PREFIX}/bin)
    message("-- " "[${PROJECT_NAME}] DYAD_INSTALL_BIN_DIR " ${DYAD_INSTALL_BIN_DIR})
endif ()
if (NOT DYAD_INSTALL_LIB_DIR)
    set(DYAD_INSTALL_LIB_DIR ${CMAKE_INSTALL_PREFIX}/lib)
    message("-- " "[${PROJECT_NAME}] DYAD_INSTALL_LIB_DIR " ${DYAD_INSTALL_LIB_DIR})
endif ()
if (NOT DYAD_INSTALL_INCLUDE_DIR)
    set(DYAD_INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include)
    message("-- " "[${PROJECT_NAME}] DYAD_INSTALL_INCLUDE_DIR " ${DYAD_INSTALL_INCLUDE_DIR})
endif ()
if (NOT DYAD_INSTALL_DATA_DIR)
    set(DYAD_INSTALL_DATA_DIR ${CMAKE_INSTALL_PREFIX}/share)
    message("-- " "[${PROJECT_NAME}] DYAD_INSTALL_DATA_DIR " ${DYAD_INSTALL_DATA_DIR})
endif ()

if (NOT CMAKE_INSTALL_RPATH)
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib:${CMAKE_INSTALL_PREFIX}/lib64")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif ()
#------------------------------------------------------------------------------
if (NOT DYAD_EXTERNALLY_CONFIGURED)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE PATH "Single Directory for all Executables.")
    set(CMAKE_INCLUDE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/include CACHE PATH "Store the headers.")
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE PATH "Single Directory for all Libraries")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE PATH "Single Directory for all static libraries.")
endif ()
#-----------------------------------------------------------------------------
# Targets built within this project are exported at Install time for use
# by other projects.
#-----------------------------------------------------------------------------
if (NOT DYAD_EXPORTED_TARGETS)
    set(DYAD_EXPORTED_TARGETS "dyad-targets")
endif ()
#------------------------------------------------------------------------------
# Build options
#------------------------------------------------------------------------------
# option(ENABLE_DYAD_DEBUG "Include debugging prints and logging" OFF) # This is not needed as we have CMAKE_BUILD_TYPE
# This is verbose, maybe an alternate might help simplify
#option(BUILD_URPC "Build DYAD's URPC code" OFF)
#option(ENABLE_PERFFLOW "Build with PerfFlow Aspect support" OFF)
#option(ENABLE_UCX_DTL "Build DYAD's UCX data transport layer" OFF)

set(DYAD_CONTROL_PLANE "FLUX_RPC" CACHE STRING "Protocol to use for DYAD's Control Plane")
set_property(CACHE DYAD_CONTROL_PLANE PROPERTY STRINGS FLUX_RPC)
set(DYAD_DATA_PLANE "FLUX_RPC" CACHE STRING "Protocol to use for DYAD's Data Plane")
set_property(CACHE DYAD_DATA_PLANE PROPERTY STRINGS FLUX_RPC UCX)
set(DYAD_PROFILER "NONE" CACHE STRING "Protocol to use for DYAD's Data Plane")
set_property(CACHE DYAD_PROFILER PROPERTY STRINGS PERFFLOW_ASPECT CALIPER NONE)

#------------------------------------------------------------------------------
# Build Type based configurations
#------------------------------------------------------------------------------
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} -std=c99)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -std=c++11)
if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} -DDYAD_FULL_DEBUG=1 -DDYAD_LOGGING_ON=1 )
    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -DDYAD_FULL_DEBUG=1 -DDYAD_LOGGING_ON=1)
endif()
#------------------------------------------------------------------------------
# Disallow in-source build
#------------------------------------------------------------------------------
if ("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR
            "${PROJECT_NAME} requires an out of source Build. "
            "Please create a separate binary directory and run CMake there.")
endif ()
#------------------------------------------------------------------------------
# Set a default build type if none was specified
#------------------------------------------------------------------------------
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
            "MinSizeRel" "RelWithDebInfo")
endif ()
#------------------------------------------------------------------------------
# Dependencies
#------------------------------------------------------------------------------
set(DEPENDENCY_LIB "")
# Required Dependencies
# =============================================================================
# Import pkg-config for finding FluxCore
find_package(PkgConfig REQUIRED)
if (${PkgConfig_FOUND})
    message(STATUS "[${PROJECT_NAME}] found PkgConfig at ${PkgConfig_INCLUDE_DIRS}")
    include_directories(${PkgConfig_INCLUDE_DIRS})
    set(DEPENDENCY_LIB ${DEPENDENCY_LIB} ${PkgConfig_LIBRARIES})
else ()
    message(FATAL_ERROR "-- [${PROJECT_NAME}] PkgConfig is needed for ${PROJECT_NAME} build")
endif ()
find_package(FluxCore REQUIRED)
if (${FluxCore_FOUND})
    message(STATUS "[${PROJECT_NAME}] found FluxCore at ${FluxCore_INCLUDE_DIRS}")
    include_directories(${FluxCore_INCLUDE_DIRS})
    set(DEPENDENCY_LIB ${DEPENDENCY_LIB} ${FluxCore_LIBRARIES})
else ()
    message(FATAL_ERROR "-- [${PROJECT_NAME}] FluxCore is needed for ${PROJECT_NAME} build")
endif ()
find_package(Jansson 2.10 REQUIRED)
if (${JANSSON_FOUND})
    message(STATUS "[${PROJECT_NAME}] found Jansson at ${JANSSON_INCLUDE_DIRS}")
    include_directories(${JANSSON_INCLUDE_DIRS})
    set(DEPENDENCY_LIB ${DEPENDENCY_LIB} ${JANSSON_LIBRARIES})
else ()
    message(FATAL_ERROR "-- [${PROJECT_NAME}] Jansson is needed for ${PROJECT_NAME} build")
endif ()

# Optional Dependencies
# =============================================================================
if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    find_package(perfflowaspect REQUIRED)
    if (${perfflowaspect_FOUND})
        message(STATUS "[${PROJECT_NAME}] found perfflowaspect at ${perfflowaspect_INCLUDE_DIRS}")
        include_directories(${perfflowaspect_INCLUDE_DIRS})
        set(DEPENDENCY_LIB ${DEPENDENCY_LIB} ${perfflowaspect_LIBRARIES})
    else ()
        message(FATAL_ERROR "-- [${PROJECT_NAME}] ucx is needed for ${PROJECT_NAME} build")
    endif ()
endif()
if(DYAD_DATA_PLANE STREQUAL "UCX")
    find_package(ucx 1.6 REQUIRED)
    if (${ucx_FOUND})
        message(STATUS "[${PROJECT_NAME}] found ucx at ${ucx_INCLUDE_DIRS}")
        include_directories(${ucx_INCLUDE_DIRS})
        set(DEPENDENCY_LIB ${DEPENDENCY_LIB} ${ucx_LIBRARIES})
    else ()
        message(FATAL_ERROR "-- [${PROJECT_NAME}] ucx is needed for ${PROJECT_NAME} build")
    endif ()
endif()

function(dyad_install_headers public_headers current_dir)
    message("-- [${PROJECT_NAME}] " "installing headers ${public_headers}")
    foreach (header ${public_headers})
        file(RELATIVE_PATH header_file_path "${PROJECT_SOURCE_DIR}/src" "${header}")
        message("-- [${PROJECT_NAME}] " "installing header ${header_file_path}")
        get_filename_component(header_directory_path "${header_file_path}" DIRECTORY)
        install(
                FILES ${header}
                DESTINATION "${CMAKE_INSTALL_PREFIX}/include/${header_directory_path}"
        )
        file(COPY ${header}
                DESTINATION "${CMAKE_INCLUDE_OUTPUT_DIRECTORY}/${header_directory_path}")
    endforeach ()
endfunction()

add_subdirectory(src/dyad)

#-----------------------------------------------------------------------------
# Configure the config.cmake file for the build directory
#-----------------------------------------------------------------------------
configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/CMake/${PROJECT_NAME}-config.cmake.build.in
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/cmake/${PROJECT_NAME}/${PROJECT_NAME}-config.cmake @ONLY
)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/CMake/${PROJECT_NAME}-config.cmake.install.in
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/cmake/${PROJECT_NAME}/install/${PROJECT_NAME}-config.cmake @ONLY
)
install(
        FILES
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/cmake/${PROJECT_NAME}/install/${PROJECT_NAME}-config.cmake
        DESTINATION
        ${DYAD_INSTALL_LIB_DIR}/cmake/${PROJECT_NAME}
)
#-----------------------------------------------------------------------------
# Configure the ${PROJECT_NAME}-config-version .cmake file for the install directory
#-----------------------------------------------------------------------------
configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/CMake/${PROJECT_NAME}-config-version.cmake.in
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/cmake/${PROJECT_NAME}/${PROJECT_NAME}-config-version.cmake @ONLY
)

install(
        FILES
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/cmake/${PROJECT_NAME}/${PROJECT_NAME}-config-version.cmake
        DESTINATION
        ${DYAD_INSTALL_LIB_DIR}/cmake/${PROJECT_NAME}
)

export(EXPORT ${DYAD_EXPORTED_TARGETS}
        FILE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/cmake/${PROJECT_NAME}/${PROJECT_NAME}Targets.cmake")

install(EXPORT
        ${DYAD_EXPORTED_TARGETS}
        DESTINATION
        ${DYAD_INSTALL_LIB_DIR}/cmake/${PROJECT_NAME}
        FILE
        ${DYAD_EXPORTED_TARGETS}.cmake
        )