set(DYAD_MODULE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad.c)
set(DYAD_MODULE_PRIVATE_HEADERS )
set(DYAD_MODULE_PUBLIC_HEADERS )

add_library(${PROJECT_NAME} SHARED ${DYAD_MODULE_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PROJECT_NAME} PUBLIC Jansson::Jansson flux::core)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_dtl)
target_compile_definitions(${PROJECT_NAME} PUBLIC BUILDING_DYAD=1)
target_compile_definitions(${PROJECT_NAME} PUBLIC DYAD_HAS_CONFIG)
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>)

if (TARGET DYAD_C_FLAGS_werror)
  target_link_libraries(${PROJECT_NAME} PUBLIC DYAD_C_FLAGS_werror)
endif ()

if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    target_link_libraries(${PROJECT_NAME} PUBLIC perfflowaspect::perfflowaspect)
    target_compile_definitions(${PROJECT_NAME} PUBLIC DYAD_PERFFLOW=1)
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
