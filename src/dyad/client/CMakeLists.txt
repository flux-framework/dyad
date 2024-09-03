set(DYAD_CLIENT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad_client.c)
set(DYAD_CLIENT_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_logging.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_profiler.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_structures_int.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../dtl/dyad_dtl_api.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../utils/utils.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/../utils/murmur3.h
                              ${CMAKE_CURRENT_SOURCE_DIR}/dyad_client_int.h)
set(DYAD_CLIENT_PUBLIC_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_rc.h
                             ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_dtl.h
                             ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_envs.h
                             ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/client/dyad_client.h)

add_library(${PROJECT_NAME}_client SHARED ${DYAD_CLIENT_SRC}
            ${DYAD_CLIENT_PUBLIC_HEADERS} ${DYAD_CLIENT_PRIVATE_HEADERS})
set_target_properties(${PROJECT_NAME}_client PROPERTIES CMAKE_INSTALL_RPATH
                      "${CMAKE_INSTALL_PREFIX}/${DYAD_LIBDIR}")
target_link_libraries(${PROJECT_NAME}_client PRIVATE Jansson::Jansson flux::core)
target_link_libraries(${PROJECT_NAME}_client PRIVATE ${PROJECT_NAME}_utils
                      ${PROJECT_NAME}_murmur3 ${PROJECT_NAME}_dtl)
target_link_libraries(${PROJECT_NAME}_client PUBLIC ${PROJECT_NAME}_ctx)

target_compile_definitions(${PROJECT_NAME}_client PUBLIC BUILDING_DYAD=1)
target_compile_definitions(${PROJECT_NAME}_client PUBLIC DYAD_HAS_CONFIG)
target_include_directories(${PROJECT_NAME}_client PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>  # will be used for generated code
    $<INSTALL_INTERFACE:${DYAD_INSTALL_INCLUDE_DIR}>) # wil be used for sub projects
target_include_directories(${PROJECT_NAME}_client SYSTEM PRIVATE ${JANSSON_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME}_client SYSTEM PRIVATE ${FluxCore_INCLUDE_DIRS})

if (TARGET DYAD_C_FLAGS_werror)
  target_link_libraries(${PROJECT_NAME}_client PRIVATE DYAD_C_FLAGS_werror)
endif ()

if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    target_link_libraries(${PROJECT_NAME}_client PRIVATE perfflowaspect::perfflowaspect)
    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${perfflowaspect_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME}_client PRIVATE DYAD_PERFFLOW=1)
endif()
if(DYAD_PROFILER STREQUAL "DFTRACER")
    target_link_libraries(${PROJECT_NAME}_client PRIVATE ${DFTRACER_LIBRARIES})
endif()

install(
        TARGETS ${PROJECT_NAME}_client
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BIN_DIR}
)
if(NOT "${DYAD_CLIENT_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${DYAD_CLIENT_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()