set(DYAD_CORE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad_core.c)
set(DYAD_CORE_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_logging.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_profiler.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_structures_int.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../dtl/dyad_dtl_api.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../utils/utils.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../utils/murmur3.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/dyad_core_int.h)
set(DYAD_CORE_PUBLIC_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_rc.h
                             ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_dtl.h
                             ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_envs.h
                             ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/core/dyad_core.h
                             ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/core/dyad_ctx.h)

set(DYAD_CTX_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad_ctx.c)
set(DYAD_CTX_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_logging.h
                             ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_profiler.h)
set(DYAD_CTX_PUBLIC_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/core/dyad_ctx.h
                            ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_rc.h
                            ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_dtl.h
                            ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_envs.h)

add_library(${PROJECT_NAME}_ctx SHARED ${DYAD_CTX_SRC}
            ${DYAD_CTX_PUBLIC_HEADERS} ${DYAD_CTX_PRIVATE_HEADERS})
target_compile_definitions(${PROJECT_NAME}_ctx PUBLIC BUILDING_DYAD=1)
target_compile_definitions(${PROJECT_NAME}_ctx PUBLIC DYAD_HAS_CONFIG)
target_include_directories(${PROJECT_NAME}_ctx PUBLIC
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:${DYAD_INSTALL_INCLUDE_DIR}>)
target_include_directories(${PROJECT_NAME}_ctx SYSTEM PRIVATE ${JANSSON_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME}_ctx SYSTEM PRIVATE ${FluxCore_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}_ctx PRIVATE ${PROJECT_NAME}_dtl ${PROJECT_NAME}_utils)

add_library(${PROJECT_NAME}_core SHARED ${DYAD_CORE_SRC}
            ${DYAD_CORE_PUBLIC_HEADERS} ${DYAD_CORE_PRIVATE_HEADERS})
target_link_libraries(${PROJECT_NAME}_core PRIVATE Jansson::Jansson flux::core)
target_link_libraries(${PROJECT_NAME}_core PRIVATE ${PROJECT_NAME}_utils
                      ${PROJECT_NAME}_murmur3 ${PROJECT_NAME}_dtl)
target_link_libraries(${PROJECT_NAME}_core PUBLIC ${PROJECT_NAME}_ctx)

target_compile_definitions(${PROJECT_NAME}_core PUBLIC BUILDING_DYAD=1)
target_compile_definitions(${PROJECT_NAME}_core PUBLIC DYAD_HAS_CONFIG)
target_include_directories(${PROJECT_NAME}_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>  # will be used for generated code
    $<INSTALL_INTERFACE:${DYAD_INSTALL_INCLUDEDIR}>) # wil be used for sub projects
target_include_directories(${PROJECT_NAME}_core SYSTEM PRIVATE ${JANSSON_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME}_core SYSTEM PRIVATE ${FluxCore_INCLUDE_DIRS})

add_library(${PROJECT_NAME}_ctx SHARED ${DYAD_CTX_SRC}
            ${DYAD_CTX_PRIVATE_HEADERS})
target_compile_definitions(${PROJECT_NAME}_ctx PUBLIC BUILDING_DYAD=1)
target_compile_definitions(${PROJECT_NAME}_ctx PUBLIC DYAD_HAS_CONFIG)
target_include_directories(${PROJECT_NAME}_ctx SYSTEM PRIVATE ${JANSSON_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME}_ctx SYSTEM PRIVATE ${FluxCore_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}_ctx PRIVATE ${PROJECT_NAME}_dtl ${PROJECT_NAME}_utils)

dyad_add_werror_if_needed(${PROJECT_NAME}_core)
dyad_add_werror_if_needed(${PROJECT_NAME}_ctx)

if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    target_link_libraries(${PROJECT_NAME}_core PRIVATE perfflowaspect::perfflowaspect)
    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${perfflowaspect_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME}_core PRIVATE DYAD_PERFFLOW=1)
endif()
if(DYAD_PROFILER STREQUAL "DFTRACER")
    target_link_libraries(${PROJECT_NAME}_core PRIVATE ${DFTRACER_LIBRARIES})
endif()

set(DYAD_CLIENT_PUBLIC_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/core/dyad.h)
add_library(${PROJECT_NAME}_client INTERFACE)
target_link_libraries(${PROJECT_NAME}_client INTERFACE ${PROJECT_NAME}_ctx ${PROJECT_NAME}_core)
target_include_directories(${PROJECT_NAME}_client INTERFACE 
  $<INSTALL_INTERFACE:${DYAD_INSTALL_INCLUDE_DIR}>)

install(
        TARGETS ${PROJECT_NAME}_core ${PROJECT_NAME}_ctx ${PROJECT_NAME}_client
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BINDIR}
)

if(NOT "${DYAD_CORE_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${DYAD_CORE_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()
if(NOT "${DYAD_CTX_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${DYAD_CTX_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()
if(NOT "${DYAD_CLIENT_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${DYAD_CLIENT_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()
