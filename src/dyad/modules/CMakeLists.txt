set(DYAD_MODULE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad.c)
set(DYAD_MODULE_PRIVATE_HEADERS )
set(DYAD_MODULE_PUBLIC_HEADERS )

add_library(${PROJECT_NAME} SHARED ${DYAD_MODULE_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PROJECT_NAME} PUBLIC Jansson::Jansson flux::core)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_dtl)
target_compile_definitions(${PROJECT_NAME} PUBLIC BUILDING_DYAD=1)

if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    target_link_libraries(${PROJECT_NAME} PUBLIC perfflowaspect::perfflowaspect)
    target_compile_definitions(${PROJECT_NAME} PUBLIC DYAD_PERFFLOW=1)
endif()

#if (BUILD_URPC)
#    set(urpc_src
#        "${CMAKE_CURRENT_SOURCE_DIR}/urpc.c"
#    )
#    set(urpc_headers
#        "${CMAKE_CURRENT_SOURCE_DIR/urpc_ctx.h}"
#    )
#
#    add_library(urpc MODULE ${urpc_src} ${urpc_headers})
#    target_link_libraries(urpc PUBLIC Jansson::Jansson flux::core)
#    target_link_libraries(urpc PRIVATE utils)
#
#    if (ENABLE_PERFFLOW)
#        target_link_libraries(urpc PUBLIC perfflowaspect::perfflowaspect)
#    endif (ENABLE_PERFFLOW)
#    set(modules_targets "${modules_targets} urpc")
#endif (BUILD_URPC)
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