set(urpc_client_src
    "${CMAKE_CURRENT_SOURCE_DIR}/urpc_client.c"
)
set(urpc_client_headers
    "${CMAKE_CURRENT_SOURCE_DIR}/urpc_client.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/urpc_ctx.h"
)
set(urpc_client_install_headers
)

add_library(urpc_client ${urpc_client_src} ${urpc_client_headers})
target_compile_features(urpc_client PRIVATE c_std_99)
target_link_libraries(urpc_client PUBLIC flux::core)
target_link_libraries(urpc_client PRIVATE utils)

if (ENABLE_PERFFLOW)
    target_link_libraries(urpc_client PUBLIC perfflowaspect::perfflowaspect)
    target_compile_definitions(urpc_client PRIVATE DYAD_PERFFLOW=1)
endif (ENABLE_PERFFLOW)

install(
    TARGETS urpc_client
    LIBRARY DESTINATION ${DYAD_LIBDIR}
)