set(DYAD_CORE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad_core.c)
set(DYAD_CORE_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_rc.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_dtl.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_envs.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_logging.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_profiler.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../dtl/dyad_dtl_api.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../utils/utils.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../utils/murmur3.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/dyad_ctx.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/dyad_core.h)
set(DYAD_CORE_PUBLIC_HEADERS)

set(DYAD_CTX_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad_ctx.c)
set(DYAD_CTX_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/dyad_ctx.h
                             ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_rc.h
                             ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_dtl.h
                             ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_envs.h
                             ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_logging.h
                             ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_profiler.h)

add_library(${PROJECT_NAME}_core SHARED ${DYAD_CORE_SRC}
            ${DYAD_CORE_PUBLIC_HEADERS} ${DYAD_CORE_PRIVATE_HEADERS})
set_target_properties(${PROJECT_NAME}_core PROPERTIES CMAKE_INSTALL_RPATH
                      "${CMAKE_INSTALL_PREFIX}/${DYAD_INSTALL_LIBDIR}")
target_link_libraries(${PROJECT_NAME}_core PRIVATE Jansson::Jansson flux::core)
target_link_libraries(${PROJECT_NAME}_core PRIVATE ${PROJECT_NAME}_ctx ${PROJECT_NAME}_utils
                      ${PROJECT_NAME}_murmur3 ${PROJECT_NAME}_dtl)

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

if (TARGET DYAD_C_FLAGS_werror)
  target_link_libraries(${PROJECT_NAME}_core PRIVATE DYAD_C_FLAGS_werror)
  target_link_libraries(${PROJECT_NAME}_ctx PRIVATE DYAD_C_FLAGS_werror)
endif ()

if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    target_link_libraries(${PROJECT_NAME}_core PRIVATE perfflowaspect::perfflowaspect)
    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${perfflowaspect_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME}_core PRIVATE DYAD_PERFFLOW=1)
endif()
if(DYAD_PROFILER STREQUAL "DFTRACER")
    target_link_libraries(${PROJECT_NAME}_core PRIVATE ${DFTRACER_LIBRARIES})
endif()
install(
        TARGETS ${PROJECT_NAME}_core ${PROJECT_NAME}_ctx
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BINDIR}
)
if(NOT "${DYAD_CORE_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${DYAD_CORE_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()
