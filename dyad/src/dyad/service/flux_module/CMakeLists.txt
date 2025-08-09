# Our FLUX module: dyad.so
# it will be installed in /install/lib64/dyad.so
set(DYAD_FLUX_MODULE "dyad")

set(DYAD_FLUX_MODULE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad.c)
set(DYAD_FLUX_MODULE_PRIVATE_HEADERS ${CMAKE_SOURCE_DIR}/include/dyad/common/dyad_envs.h
                                ${CMAKE_SOURCE_DIR}/include/dyad/common/dyad_dtl.h
                                ${CMAKE_SOURCE_DIR}/include/dyad/common/dyad_rc.h
                                ${CMAKE_SOURCE_DIR}/include/dyad/core/dyad_ctx.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../../common/dyad_logging.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../../common/dyad_structures_int.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../../common/dyad_profiler.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../../dtl/dyad_dtl_api.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/read_all.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/utils.h)
set(DYAD_FLUX_MODULE_PUBLIC_HEADERS)

add_library(${DYAD_FLUX_MODULE} SHARED ${DYAD_FLUX_MODULE_SRC}
            ${DYAD_FLUX_MODULE_PRIVATE_HEADERS} ${DYAD_FLUX_MODULE_PUBLIC_HEADERS})
set_target_properties(${DYAD_FLUX_MODULE} PROPERTIES PREFIX "")
target_link_libraries(${DYAD_FLUX_MODULE} PRIVATE Jansson::Jansson)
target_link_libraries(${DYAD_FLUX_MODULE} PRIVATE ${PROJECT_NAME}_dtl)
target_link_libraries(${DYAD_FLUX_MODULE} PRIVATE ${PROJECT_NAME}_ctx)
target_link_libraries(${DYAD_FLUX_MODULE} PRIVATE ${PROJECT_NAME}_utils)
target_compile_definitions(${DYAD_FLUX_MODULE} PUBLIC BUILDING_DYAD=1)
target_compile_definitions(${DYAD_FLUX_MODULE} PUBLIC DYAD_HAS_CONFIG)
target_include_directories(${DYAD_FLUX_MODULE} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:${DYAD_INSTALL_INCLUDEDIR}>)
target_include_directories(${DYAD_FLUX_MODULE} SYSTEM PRIVATE ${JANSSON_INCLUDE_DIRS})
target_include_directories(${DYAD_FLUX_MODULE} SYSTEM PRIVATE ${FluxCore_INCLUDE_DIRS})

dyad_add_werror_if_needed(${DYAD_FLUX_MODULE})

if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    target_link_libraries(${DYAD_FLUX_MODULE} PRIVATE perfflowaspect::perfflowaspect)
    target_include_directories(${DYAD_FLUX_MODULE} SYSTEM PRIVATE ${perfflowaspect_INCLUDE_DIRS})
    target_compile_definitions(${DYAD_FLUX_MODULE} PRIVATE DYAD_PERFFLOW=1)
endif()

if(DYAD_PROFILER STREQUAL "DFTRACER")
    target_link_libraries(${DYAD_FLUX_MODULE} PRIVATE ${DFTRACER_LIBRARIES})
endif()
install(
        TARGETS ${DYAD_FLUX_MODULE}
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BINDIR}
)
if(NOT "${DYAD_FLUX_MODULE_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${DYAD_FLUX_MODULE_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()
