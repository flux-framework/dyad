set(DYAD_MODULE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad.c)
set(DYAD_MODULE_PRIVATE_HEADERS)
set(DYAD_MODULE_PUBLIC_HEADERS)

add_library(${PROJECT_NAME} SHARED ${DYAD_MODULE_SRC}
            ${DYAD_MODULE_PRIVATE_HEADERS} ${DYAD_MODULE_PUBLIC_HEADERS})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
set_target_properties(${PROJECT_NAME} PROPERTIES CMAKE_INSTALL_RPATH
                      "${CMAKE_INSTALL_PREFIX}/${DYAD_LIBDIR}")
target_link_libraries(${PROJECT_NAME} PRIVATE Jansson::Jansson flux::core)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_dtl)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_utils)
target_link_libraries(${PROJECT_NAME} PRIVATE flux::optparse)
target_compile_definitions(${PROJECT_NAME} PUBLIC BUILDING_DYAD=1)
target_compile_definitions(${PROJECT_NAME} PUBLIC DYAD_HAS_CONFIG)
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:${DYAD_INSTALL_INCLUDE_DIR}>)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${JANSSON_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${FluxCore_INCLUDE_DIRS})

if (TARGET DYAD_C_FLAGS_werror)
  target_link_libraries(${PROJECT_NAME} PRIVATE DYAD_C_FLAGS_werror)
endif ()

if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    target_link_libraries(${PROJECT_NAME} PRIVATE perfflowaspect::perfflowaspect)
    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${perfflowaspect_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE DYAD_PERFFLOW=1)
endif()

if(DYAD_PROFILER STREQUAL "DLIO_PROFILER")
    target_link_libraries(${PROJECT_NAME} PRIVATE ${DLIO_PROFILER_LIBRARIES})
endif()
install(
        TARGETS ${PROJECT_NAME}
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BIN_DIR}
)
if(NOT "${DYAD_MODULE_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${DYAD_MODULE_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()
