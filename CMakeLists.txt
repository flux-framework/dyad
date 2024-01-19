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

#------------------------------------------------------------------------------
# Internal Includes for header and libraries
#------------------------------------------------------------------------------
# Get installation directories -- these get used in various places;
# best to just make them available
option (LIBDIR_AS_LIB OFF)
if (NOT LIBDIR_AS_LIB)
    include(GNUInstallDirs)
endif ()

if (CMAKE_INSTALL_LIBDIR)
   set(DYAD_LIBDIR ${CMAKE_INSTALL_LIBDIR})
   set(DYAD_INSTALL_INCLUDE_DIR
       ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})
   set(DYAD_INSTALL_DOCDIR
       ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DOCDIR})
   set(DYAD_INSTALL_SYSCONFDIR
       ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}/modulefiles)
else ()
   set(DYAD_LIBDIR "lib")
   set(DYAD_INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include")
   set(DYAD_INSTALL_DOCDIR "${CMAKE_INSTALL_PREFIX}/doc")
   set(DYAD_INSTALL_SYSCONFDIR "${CMAKE_INSTALL_PREFIX}/etc/modulefiles")
endif ()
link_directories(${CMAKE_BINARY_DIR}/${DYAD_LIBDIR})
include_directories(${CMAKE_BINARY_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src) # required for private includes

#------------------------------------------------------------------------------
# Internal Paths for cmake libraries and Setup install and output Directories
#------------------------------------------------------------------------------
# This sets where to look for dependent libraries
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_BINARY_DIR} ${CMAKE_INSTALL_PREFIX})
# This sets where to look for dependent library's cmake files
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/${DYAD_LIBDIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/share/cmake)

#------------------------------------------------------------------------------
if (NOT DYAD_EXTERNALLY_CONFIGURED)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE PATH "Single Directory for all Executables.")
    set(CMAKE_INCLUDE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/include CACHE PATH "Store the headers.")
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${DYAD_LIBDIR} CACHE PATH "Single Directory for all Libraries")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${DYAD_LIBDIR} CACHE PATH "Single Directory for all static libraries.")
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

# This option is off by default because non-developers should not use
# this option under normal circumstances.
option(DYAD_WARNINGS_AS_ERRORS
  "Build with warnings promoted to errors." OFF)
mark_as_advanced(DYAD_WARNINGS_AS_ERRORS)

option (USE_CLANG_LIBCXX OFF)
if (USE_CLANG_LIBCXX)
    set (GCC_TOOLCHAIN "")
endif (USE_CLANG_LIBCXX)

set(DYAD_CONTROL_PLANE "FLUX_RPC" CACHE STRING "Protocol to use for DYAD's Control Plane")
set_property(CACHE DYAD_CONTROL_PLANE PROPERTY STRINGS FLUX_RPC)
set(DYAD_DATA_PLANE "FLUX_RPC" CACHE STRING "Protocol to use for DYAD's Data Plane")
set_property(CACHE DYAD_DATA_PLANE PROPERTY STRINGS FLUX_RPC UCX)
set(DYAD_PROFILER "NONE" CACHE STRING "Profiler to use for DYAD")
set_property(CACHE DYAD_PROFILER PROPERTY STRINGS PERFFLOW_ASPECT CALIPER NONE)


include(ExternalProject)
include(SetupCompiler)

#------------------------------------------------------------------------------
# Build Type based configurations
#------------------------------------------------------------------------------

if(CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release") # Setting default as Release
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DYAD_FULL_DEBUG 1)
    set(DYAD_LOGGING_ON 1)
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
                DESTINATION "include/${header_directory_path}"
        )
        file(COPY ${header}
                DESTINATION "${CMAKE_INCLUDE_OUTPUT_DIRECTORY}/${header_directory_path}")
    endforeach ()
endfunction()

include(DYADUtils)
add_subdirectory(src/dyad)
cmake_policy(SET CMP0079 NEW)


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
        ${DYAD_LIBDIR}/cmake/${PROJECT_NAME}
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
        ${DYAD_LIBDIR}/cmake/${PROJECT_NAME}
)

export(EXPORT ${DYAD_EXPORTED_TARGETS}
        FILE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/cmake/${PROJECT_NAME}/${PROJECT_NAME}Targets.cmake")

# Write the configure file
configure_file("${CMAKE_SOURCE_DIR}/cmake/configure_files/dyad_config.hpp.in"
               "${CMAKE_BINARY_DIR}/src/dyad/dyad_config.hpp" @ONLY)


################################################################
# Install DYAD
################################################################

include(CMakePackageConfigHelpers)

install(FILES
        "${CMAKE_BINARY_DIR}/src/dyad/dyad_config.hpp"
        DESTINATION
        ${DYAD_INSTALL_INCLUDE_DIR}/dyad)

# Write the version file. This is independent of build/install tree.
write_basic_package_version_file(
  DYADConfigVersion.cmake
  VERSION "${DYAD_PACKAGE_VERSION}"
  COMPATIBILITY SameMajorVersion)

install(EXPORT
        ${DYAD_EXPORTED_TARGETS}
        DESTINATION
        ${DYAD_LIBDIR}/cmake/${PROJECT_NAME}
        FILE
        ${DYAD_EXPORTED_TARGETS}.cmake
        )

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
string(APPEND _str
  "  DYAD_CONTROL_PLANE:          ${DYAD_CONTROL_PLANE}\n")
string(APPEND _str
  "  DYAD_DATA_PLANE:             ${DYAD_DATA_PLANE}\n")
string(APPEND _str
  "  DYAD_PROFILER:               ${DYAD_PROFILER}\n")
string(APPEND _str
  "  FluxCore_ROOT_DIR:           ${FluxCore_ROOT_DIR}\n")
string(APPEND _str
  "  JANSSON_INCLUDE_DIR:         ${JANSSON_INCLUDE_DIR}\n")

#Print the true/false variables
append_str_tf(_str
  DYAD_GNU_LINUX
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
