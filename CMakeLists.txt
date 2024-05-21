cmake_minimum_required(VERSION 3.12)
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

# Convenience defines
string(TOUPPER "${PROJECT_NAME}" UPPER_PROJECT_NAME)
string(TOLOWER "${PROJECT_NAME}" LOWER_PROJECT_NAME)
set(DYAD_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

#------------------------------------------------------------------------------
# Internal Includes for header and libraries
#------------------------------------------------------------------------------
# Get installation directories -- these get used in various places;
# best to just make them available
option (DYAD_LIBDIR_AS_LIB OFF)
if (NOT DYAD_LIBDIR_AS_LIB)
    include(GNUInstallDirs)
endif ()
#
if (CMAKE_INSTALL_LIBDIR)
   message(STATUS "CMAKE_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR}")
   set(DYAD_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
   set(DYAD_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})
   set(DYAD_INSTALL_INCLUDEDIR
       ${CMAKE_INSTALL_INCLUDEDIR})
   set(DYAD_INSTALL_DOCDIR
       ${CMAKE_INSTALL_DOCDIR})
   set(DYAD_INSTALL_SYSCONFDIR
       ${CMAKE_INSTALL_SYSCONFDIR}/modulefiles)
else ()
   set(DYAD_INSTALL_BINDIR "bin")
   set(DYAD_INSTALL_LIBDIR "lib")
   set(DYAD_INSTALL_INCLUDEDIR "include")
   set(DYAD_INSTALL_DOCDIR "doc")
   set(DYAD_INSTALL_SYSCONFDIR "etc/modulefiles")
   message(STATUS "DYAD_INSTALL_LIBDIR set to ${DYAD_INSTALL_LIBDIR}")
endif ()

#------------------------------------------------------------------------------
# Internal Paths for cmake libraries and Setup install and output Directories
#------------------------------------------------------------------------------
# This sets where to look for dependent libraries
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_BINARY_DIR} ${CMAKE_INSTALL_PREFIX})
# This sets where to look for dependent library's cmake files
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/${DYAD_INSTALL_LIBDIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/share/cmake)

#------------------------------------------------------------------------------
# This doesn't really need to be in an 'if' statement since they're cache variables
# if (NOT DYAD_EXTERNALLY_CONFIGURED)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${DYAD_INSTALL_BINDIR} CACHE PATH "Single Directory for all Executables.")
set(CMAKE_INCLUDE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${DYAD_INSTALL_INCLUDEDIR} CACHE PATH "Store the headers.")
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${DYAD_INSTALL_LIBDIR} CACHE PATH "Single Directory for all Libraries")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${DYAD_INSTALL_LIBDIR} CACHE PATH "Single Directory for all static libraries.")
# endif ()

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

# This option is off by default because non-developers should not use
# this option under normal circumstances.
option(DYAD_WARNINGS_AS_ERRORS
  "Build with warnings promoted to errors." OFF)
mark_as_advanced(DYAD_WARNINGS_AS_ERRORS)

option (DYAD_USE_CLANG_LIBCXX OFF)
if (DYAD_USE_CLANG_LIBCXX)
    set (GCC_TOOLCHAIN "")
endif (DYAD_USE_CLANG_LIBCXX)

option (DYAD_ENABLE_UCX_DATA "Allow to dynamically select UCX for DYAD's Data Plane" OFF)
if (DYAD_ENABLE_UCX_DATA)
    set (DYAD_ENABLE_UCX_DTL 1)
else ()
    option (DYAD_ENABLE_UCX_DATA_RMA "Use UCX's RMA for DYAD's Data Plane" ON)
endif ()

if (DYAD_ENABLE_UCX_DATA_RMA)
    set (DYAD_ENABLE_UCX_RMA 1)
endif ()

set(DYAD_PROFILER "NONE" CACHE STRING "Profiler to use for DYAD")
set_property(CACHE DYAD_PROFILER PROPERTY STRINGS PERFFLOW_ASPECT CALIPER DFTRACER NONE)
set(DYAD_LOGGER "NONE" CACHE STRING "Logger to use for DYAD")
set_property(CACHE DYAD_LOGGER PROPERTY STRINGS FLUX CPP_LOGGER NONE)
set(DYAD_LOGGER_LEVEL "NONE" CACHE STRING "Logging level to use for DYAD")
set_property(CACHE DYAD_LOGGER_LEVEL PROPERTY STRINGS DEBUG INFO WARN ERROR NONE)
option(DYAD_ENABLE_TESTS "Enable dyad tests" OFF)

#------------------------------------------------------------------------------
# Compiler setup
#------------------------------------------------------------------------------
include(ExternalProject)
# It requires include(GNUInstallDirs) before if that is needed
include(SetupCompiler)

#------------------------------------------------------------------------------
# Build Type based configurations
#------------------------------------------------------------------------------

if(CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release") # Setting default as Release
endif()

if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    set(DYAD_PROFILER_PERFFLOW_ASPECT 1)
elseif(DYAD_PROFILER STREQUAL "CALIPER")
    set(DYAD_PROFILER_CALIPER 1)
elseif(DYAD_PROFILER STREQUAL "DFTRACER")
    set(DYAD_PROFILER_DFTRACER 1)
else()
    set(DYAD_PROFILER_NONE 1)
endif()
message(STATUS "[${PROJECT_NAME}] setting DYAD_PROFILER as ${DYAD_PROFILER}")

if(DYAD_LOGGER STREQUAL "FLUX")
    set(DYAD_LOGGER_FLUX 1)
elseif(DYAD_LOGGER STREQUAL "CPP_LOGGER")
    set(DYAD_LOGGER_CPP_LOGGER 1)
endif()
message(STATUS "[${PROJECT_NAME}] setting DYAD_LOGGER as ${DYAD_LOGGER}")


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (DYAD_LOGGER STREQUAL "NONE")
        message(STATUS "-- [${PROJECT_NAME}] disabling logger as no logger is selected")
        set(DYAD_LOGGER_NO_LOG 1)
    else()
        if (DYAD_LOGGER_LEVEL STREQUAL "NONE" OR DYAD_LOGGER_LEVEL STREQUAL "DEBUG")
            set(DYAD_LOGGER_LEVEL_ERROR 1)
            set(DYAD_LOGGER_LEVEL_WARN 1)
            set(DYAD_LOGGER_LEVEL_INFO 1)
            set(DYAD_LOGGER_LEVEL_DEBUG 1)
        elseif(DYAD_LOGGER_LEVEL STREQUAL "ERROR")
            set(DYAD_LOGGER_LEVEL_ERROR 1)
        elseif(DYAD_LOGGER_LEVEL STREQUAL "WARN")
            set(DYAD_LOGGER_LEVEL_ERROR 1)
            set(DYAD_LOGGER_LEVEL_WARN 1)
        elseif(DYAD_LOGGER_LEVEL STREQUAL "INFO")
            set(DYAD_LOGGER_LEVEL_ERROR 1)
            set(DYAD_LOGGER_LEVEL_WARN 1)
            set(DYAD_LOGGER_LEVEL_INFO 1)
        else()
            set(DYAD_LOGGER_NO_LOG 1)
        endif()
        message(STATUS "[${PROJECT_NAME}] setting DYAD_LOGGER_LEVEL as ${DYAD_LOGGER_LEVEL}")
    endif()
else()
    set(DYAD_LOGGER_NO_LOG 1)
endif()

if (NOT DEFINED BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ON)
endif ()
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

# Required Dependencies
# =============================================================================
# Import pkg-config for finding FluxCore
find_package(PkgConfig REQUIRED)
if (${PkgConfig_FOUND})
    message(STATUS "[${PROJECT_NAME}] found PkgConfig at ${PkgConfig_INCLUDE_DIRS}")
    include_directories(${PkgConfig_INCLUDE_DIRS})
else ()
    message(FATAL_ERROR "-- [${PROJECT_NAME}] PkgConfig is needed for ${PROJECT_NAME} build")
endif ()
find_package(FluxCore REQUIRED)
if (${FluxCore_FOUND})
    message(STATUS "[${PROJECT_NAME}] found FluxCore at ${FluxCore_INCLUDE_DIRS}")
else ()
    message(FATAL_ERROR "-- [${PROJECT_NAME}] FluxCore is needed for ${PROJECT_NAME} build")
endif ()
find_package(Jansson 2.10 REQUIRED)
if (${JANSSON_FOUND})  # Note: this find_package does not properly name its '_FOUND' variable. This is the correct name
    message(STATUS "[${PROJECT_NAME}] found Jansson at ${JANSSON_INCLUDE_DIRS}")
else ()
    message(FATAL_ERROR "-- [${PROJECT_NAME}] Jansson is needed for ${PROJECT_NAME} build")
endif ()

# Optional Dependencies
# =============================================================================
if (DYAD_LOGGER STREQUAL "CPP_LOGGER")
    find_package(cpp-logger REQUIRED
                 HINTS ${CPP_LOGGER_DIR} ${cpp-logger_DIR}
                       ${CPP_LOGGER_PATH} ${cpp-logger_PATH}
                       $ENV{CPP_LOGGER_DIR} $ENV{cpp-logger_DIR}
                       $ENV{CPP_LOGGER_PATH} $ENV{cpp-logger_PATH})
    if (${cpp-logger_FOUND})
        message(STATUS "[${PROJECT_NAME}] found cpp-logger at ${CPP_LOGGER_INCLUDE_DIRS}")
        include_directories(${CPP_LOGGER_INCLUDE_DIRS})
        set(DEPENDENCY_LIB ${DEPENDENCY_LIB} ${CPP_LOGGER_LIBRARIES})
    else ()
        message(FATAL_ERROR "-- [${PROJECT_NAME}] cpp-logger is not found but selected in cmake options for ${PROJECT_NAME} build")
    endif ()
endif()
if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    find_package(perfflowaspect REQUIRED)
    if (${perfflowaspect_FOUND})
        message(STATUS "[${PROJECT_NAME}] found perfflowaspect at ${perfflowaspect_INCLUDE_DIRS}")
    else ()
        message(FATAL_ERROR "-- [${PROJECT_NAME}] perfflowaspect is not found but selected in cmake options for ${PROJECT_NAME} build")
    endif ()
elseif(DYAD_PROFILER STREQUAL "DFTRACER")
    find_package(dftracer REQUIRED IMPORTED)
    if (${DFTRACER_FOUND})
        message(STATUS "[${PROJECT_NAME}] found dftracer at ${DFTRACER_INCLUDE_DIRS}")
        include_directories(${DFTRACER_INCLUDE_DIRS})
        set(DEPENDENCY_LIB ${DEPENDENCY_LIB} ${DFTRACER_LIBRARIES})
    else ()
        message(FATAL_ERROR "-- [${PROJECT_NAME}] dftracer is not found but selected in cmake options ${PROJECT_NAME} build")
    endif ()
endif()
if(DYAD_ENABLE_UCX_DATA OR DYAD_ENABLE_UCX_DATA_RMA)
    find_package(ucx 1.6 REQUIRED)
    if (${ucx_FOUND})
        message(STATUS "[${PROJECT_NAME}] found ucx at ${ucx_INCLUDE_DIRS}")
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
                DESTINATION "include/${header_directory_path}"
        )
        file(COPY ${header}
                DESTINATION "${CMAKE_INCLUDE_OUTPUT_DIRECTORY}/${header_directory_path}")
    endforeach ()
endfunction()

include(DYADUtils)
include_directories(${CMAKE_SOURCE_DIR}/include)  # public header
add_subdirectory(src/dyad)
#cmake_policy(SET CMP0079 NEW) # In case that we need more control over the target building order


#-----------------------------------------------------------------------------
# Configure the config.cmake file for the build directory
#-----------------------------------------------------------------------------
# configure_file(
#         ${CMAKE_CURRENT_SOURCE_DIR}/CMake/${PROJECT_NAME}-config.cmake.build.in
#         ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/cmake/${PROJECT_NAME}/${PROJECT_NAME}-config.cmake @ONLY
# )
# 
# configure_file(
#         ${CMAKE_CURRENT_SOURCE_DIR}/CMake/${PROJECT_NAME}-config.cmake.install.in
#         ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/cmake/${PROJECT_NAME}/install/${PROJECT_NAME}-config.cmake @ONLY
# )
# install(
#         FILES
#         ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/cmake/${PROJECT_NAME}/install/${PROJECT_NAME}-config.cmake
#         DESTINATION
#         ${DYAD_LIBDIR}/cmake/${PROJECT_NAME}
# )
#-----------------------------------------------------------------------------
# Configure the ${PROJECT_NAME}-config-version .cmake file for the install directory
#-----------------------------------------------------------------------------
# configure_file(
#         ${CMAKE_CURRENT_SOURCE_DIR}/CMake/${PROJECT_NAME}-config-version.cmake.in
#         ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/cmake/${PROJECT_NAME}/${PROJECT_NAME}-config-version.cmake @ONLY
# )
# 
# install(
#         FILES
#         ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/cmake/${PROJECT_NAME}/${PROJECT_NAME}-config-version.cmake
#         DESTINATION
#         ${DYAD_LIBDIR}/cmake/${PROJECT_NAME}
# )
# 
# export(EXPORT ${DYAD_EXPORTED_TARGETS}
#         FILE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/cmake/${PROJECT_NAME}/${PROJECT_NAME}Targets.cmake")

# Write the configure file
configure_file("${CMAKE_SOURCE_DIR}/cmake/configure_files/dyad_config.hpp.in"
               "${CMAKE_INCLUDE_OUTPUT_DIRECTORY}/dyad/dyad_config.hpp" @ONLY)


################################################################
# Install DYAD
################################################################

include(CMakePackageConfigHelpers)

set(DYAD_BUILD_CMAKE_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/cmake/${PROJECT_NAME}")
set(DYAD_INSTALL_CMAKE_DIR "${DYAD_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
set(DYAD_INSTALL_TMP_CMAKE_DIR "${CMAKE_BINARY_DIR}/install_cmake_scripts")

# Write the version file. This is independent of build/install tree.
write_basic_package_version_file(
  ${DYAD_BUILD_CMAKE_DIR}/DYADConfigVersion.cmake
  VERSION "${DYAD_PACKAGE_VERSION}"
  COMPATIBILITY SameMajorVersion)
file(COPY ${DYAD_BUILD_CMAKE_DIR}/DYADConfigVersion.cmake
  DESTINATION ${DYAD_INSTALL_TMP_CMAKE_DIR})

#...........................................
# TODO: There are two different places where cmake config is handled currently.
# Need to clean up and merge

# if (WIP_package_config)
# This is for the build tree
#set(INCLUDE_INSTALL_DIRS
#  "${CMAKE_SOURCE_DIR}/src/dyad/common"
#  "${CMAKE_SOURCE_DIR}/src/dyad/core"
#  "${CMAKE_SOURCE_DIR}/src/dyad/dtl"
#  "${CMAKE_SOURCE_DIR}/src/dyad/module"
#  "${CMAKE_SOURCE_DIR}/src/dyad/wrapper"
#  "${CMAKE_SOURCE_DIR}/src/dyad/utils"
#  "${CMAKE_SOURCE_DIR}/src/dyad/utils/base64")
#  "${CMAKE_SOURCE_DIR}/include/dyad/stream")
set(EXTRA_CMAKE_MODULE_DIR "${CMAKE_SOURCE_DIR}/cmake/modules")
set(INCLUDE_INSTALL_DIRS ${CMAKE_INCLUDE_OUTPUT_DIRECTORY})
set(LIB_INSTALL_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/configure_files/DYADConfig.cmake.in
  "${DYAD_BUILD_CMAKE_DIR}/DYADConfig.cmake"
  INSTALL_DESTINATION "${DYAD_BUILD_CMAKE_DIR}"
  PATH_VARS INCLUDE_INSTALL_DIRS LIB_INSTALL_DIR)

# Write the configure file for the install tree
set(EXTRA_CMAKE_MODULE_DIR "")
set(INCLUDE_INSTALL_DIRS "${DYAD_INSTALL_INCLUDEDIR}")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${DYAD_INSTALL_LIBDIR}")
configure_package_config_file(cmake/configure_files/DYADConfig.cmake.in
  "${DYAD_INSTALL_TMP_CMAKE_DIR}/DYADConfig.cmake"
  INSTALL_DESTINATION "${DYAD_INSTALL_CMAKE_DIR}"
  PATH_VARS INCLUDE_INSTALL_DIRS LIB_INSTALL_DIR)

# Install export
install(EXPORT ${DYAD_EXPORTED_TARGETS} # DYADTargets
        NAMESPACE DYAD::
        FILE DYADTargets.cmake
        DESTINATION "${DYAD_INSTALL_CMAKE_DIR}")

# Install the cmake stuff
install(DIRECTORY "${DYAD_INSTALL_TMP_CMAKE_DIR}/" # Trailing slash ensures directory structure is not copied
  DESTINATION "${DYAD_INSTALL_CMAKE_DIR}"
  FILES_MATCHING PATTERN "*.cmake")

install(DIRECTORY cmake/modules
        DESTINATION "${DYAD_INSTALL_CMAKE_DIR}"
        FILES_MATCHING PATTERN "*.cmake")
# endif (WIP_package_config)
#...........................................

install(FILES
        "${CMAKE_INCLUDE_OUTPUT_DIRECTORY}/dyad/dyad_config.hpp"
        DESTINATION
        ${DYAD_INSTALL_INCLUDEDIR}/dyad)

# install(EXPORT
#         ${DYAD_EXPORTED_TARGETS}
#         DESTINATION
#         ${DYAD_LIBDIR}/cmake/${PROJECT_NAME}
#         FILE
#         ${DYAD_EXPORTED_TARGETS}.cmake
#         )

# Install license and readme
install(FILES
        "${PROJECT_SOURCE_DIR}/NOTICE"
        "${PROJECT_SOURCE_DIR}/LICENSE"
        "${PROJECT_SOURCE_DIR}/README.md"
        DESTINATION
        ${DYAD_INSTALL_DOCDIR})


###############################################################################
#                      Print out configuration summary
###############################################################################

if (CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
  set(DYAD_GNU_LINUX TRUE)
endif ()

# NOTE: message() outputs to stderr by default. We now use a string to
# maintain this information and then have cmake echo it to stdout. The
# only side effects are that if you use the CMake GUI, you won't see
# this output anymore (they only report stderr) and that if you add
# something to the list, you must remember your newline!
set(_str "\n== DYAD Configuration Summary ==\n\n")
string(APPEND _str "  PROJECT_SOURCE_DIR:   ${PROJECT_SOURCE_DIR}\n"
  "  PROJECT_BINARY_DIR:   ${PROJECT_BINARY_DIR}\n\n"
  "  CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}\n"
  "  CMAKE_BUILD_TYPE:     ${CMAKE_BUILD_TYPE}\n\n")
if (CMAKE_BUILD_TYPE MATCHES None)
  string(APPEND _str
    "  CXX FLAGS:            ${CMAKE_CXX_FLAGS}\n")
elseif (CMAKE_BUILD_TYPE MATCHES Release)
  string(APPEND _str
    "  CXX FLAGS:            ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}\n")
  string(APPEND _str
    "  C FLAGS:              ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_RELEASE}\n")
elseif (CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
  string(APPEND _str
    "  CXX FLAGS:            ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}\n")
  string(APPEND _str
    "  C FLAGS:              ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_RELWITHDEBINFO}\n")
elseif (CMAKE_BUILD_TYPE MATCHES Debug)
  string(APPEND _str
    "  CXX FLAGS:            ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}\n")
  string(APPEND _str
    "  C FLAGS:              ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG}\n")
endif ()
string(APPEND _str "\n")

string(APPEND _str
  "  DYAD_PACKAGE_VERSION:        ${DYAD_PACKAGE_VERSION}\n")
string(APPEND _str
  "  DYAD_GIT_VERSION:            ${DYAD_GIT_VERSION}\n")
# Comment out since they'll be printed again below
# string(APPEND _str
#   "  DYAD_ENABLE_UCX_DATA:        ${DYAD_ENABLE_UCX_DATA}\n")
# string(APPEND _str
#   "  DYAD_ENABLE_UCX_DATA_RMA:    ${DYAD_ENABLE_UCX_DATA_RMA}\n")
string(APPEND _str
        "  DYAD_ENABLE_TESTS:    ${DYAD_ENABLE_TESTS}\n")
string(APPEND _str
  "  DYAD_PROFILER:               ${DYAD_PROFILER}\n")
string(APPEND _str
  "  DYAD_LOGGER:                 ${DYAD_LOGGER}\n")
string(APPEND _str
  "  DYAD_LOGGER_LEVEL:           ${DYAD_LOGGER_LEVEL}\n")
string(APPEND _str
  "  FluxCore_ROOT_DIR:           ${FluxCore_ROOT_DIR}\n")
string(APPEND _str
  "  JANSSON_INCLUDE_DIR:         ${JANSSON_INCLUDE_DIR}\n")
string(APPEND _str
  "  GLIBC_VERSION:               ${GLIBC_VERSION}\n")

#Print the true/false variables
append_str_tf(_str
  DYAD_GNU_LINUX
  DYAD_ENABLE_UCX_DATA
  DYAD_ENABLE_UCX_DATA_RMA
  DYAD_LIBDIR_AS_LIB
  DYAD_USE_CLANG_LIBCXX
  DYAD_WARNINGS_AS_ERRORS
)
string(APPEND _str
  "\n== End DYAD Configuration Summary ==\n")

# Output to stdout
execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${_str}")
set(_str)


###############################################################################
# Write a basic modulefile
###############################################################################

set(DYAD_MODULEFILE_NAME "dyad-${DYAD_PACKAGE_VERSION}.lua")
#  CACHE STRING
#  "The name of the DYAD modulefile to install. Must end in .lua.")

if (NOT (DYAD_MODULEFILE_NAME MATCHES ".+\.lua"))
  message(WARNING
    "DYAD_MODULEFILE_NAME must have extension \".lua\". Appending.")
  set(DYAD_MODULEFILE_NAME "${DYAD_MODULEFILE_NAME}.lua"
    CACHE STRING "" FORCE)
endif ()

configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/configure_files/dyad_module.lua.in"
  "${CMAKE_BINARY_DIR}/dyad_module.lua.install"
  @ONLY)

install(FILES "${CMAKE_BINARY_DIR}/dyad_module.lua.install"
  RENAME "${DYAD_MODULEFILE_NAME}"
  DESTINATION
  "${DYAD_INSTALL_SYSCONFDIR}")


if (DYAD_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()