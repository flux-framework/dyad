set(DYAD_MODULE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad.c)
set(DYAD_MODULE_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_envs.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_dtl.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_rc.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_logging.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_structures_int.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_profiler.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../dtl/dyad_dtl_api.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../utils/read_all.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../utils/utils.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/core/dyad_ctx.h)
set(DYAD_MODULE_PUBLIC_HEADERS)

add_library(${PROJECT_NAME} SHARED ${DYAD_MODULE_SRC}
            ${DYAD_MODULE_PRIVATE_HEADERS} ${DYAD_MODULE_PUBLIC_HEADERS})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
set_target_properties(${PROJECT_NAME} PROPERTIES CMAKE_INSTALL_RPATH
                      "${DYAD_INSTALL_LIBDIR}")
target_link_libraries(${PROJECT_NAME} PRIVATE Jansson::Jansson)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_dtl)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_ctx)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_utils)
target_compile_definitions(${PROJECT_NAME} PUBLIC BUILDING_DYAD=1)
target_compile_definitions(${PROJECT_NAME} PUBLIC DYAD_HAS_CONFIG)
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:${DYAD_INSTALL_INCLUDEDIR}>)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${JANSSON_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${FluxCore_INCLUDE_DIRS})

add_executable(test_opt_parse test_opt_parse.c
               ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_envs.h
               ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_dtl.h
               ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_rc.h
               ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_logging.h
               ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_structures_int.h)
target_compile_definitions(test_opt_parse PUBLIC DYAD_HAS_CONFIG)
target_include_directories(test_opt_parse PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:${DYAD_INSTALL_INCLUDEDIR}>)

dyad_add_werror_if_needed(${PROJECT_NAME})
dyad_add_werror_if_needed(test_opt_parse)

if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    target_link_libraries(${PROJECT_NAME} PRIVATE perfflowaspect::perfflowaspect)
    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${perfflowaspect_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE DYAD_PERFFLOW=1)
endif()

if(DYAD_PROFILER STREQUAL "DFTRACER")
    target_link_libraries(${PROJECT_NAME} PRIVATE ${DFTRACER_LIBRARIES})
endif()
install(
        TARGETS ${PROJECT_NAME}
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BINDIR}
)
if(NOT "${DYAD_MODULE_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${DYAD_MODULE_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()
