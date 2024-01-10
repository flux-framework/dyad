set(DYAD_URPC_CLIENT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/urpc_client.c)
set(DYAD_URPC_CLIENT_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/urpc_client.h ${CMAKE_CURRENT_SOURCE_DIR}/urpc_ctx.h)
set(DYAD_URPC_CLIENT_PUBLIC_HEADERS )

add_library(urpc_client ${DYAD_URPC_CLIENT_SRC})
target_link_libraries(urpc_client PUBLIC flux::core)
target_link_libraries(urpc_client PRIVATE utils)

if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    target_link_libraries(urpc_client PUBLIC perfflowaspect::perfflowaspect)
    target_compile_definitions(urpc_client PRIVATE DYAD_PERFFLOW=1)
endif()

install(
        TARGETS urpc_client
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BIN_DIR}
)
if(NOT "${DYAD_URPC_CLIENT_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${DYAD_URPC_CLIENT_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()