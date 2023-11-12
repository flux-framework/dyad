set(dtl_src
    "${CMAKE_CURRENT_SOURCE_DIR}/dyad_dtl_impl.c"
)
set(dtl_headers
    "${CMAKE_CURRENT_SOURCE_DIR}/dyad_dtl_impl.h"
)
set(dtl_install_headers
    "${CMAKE_CURRENT_SOURCE_DIR}/dyad_flux_log.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/dyad_rc.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/dyad_dtl.h"
)

# Files for FLUX_RPC DTL
set(flux_dtl_src
    "${CMAKE_CURRENT_SOURCE_DIR}/flux_dtl.c"
)
set(flux_dtl_headers
    "${CMAKE_CURRENT_SOURCE_DIR}/flux_dtl.h"
)
# Files for UCX DTL
set(ucx_dtl_src
    "${CMAKE_CURRENT_SOURCE_DIR}/ucx_dt.c"
)
set(ucx_dtl_headers
    "${CMAKE_CURRENT_SOURCE_DIR}/ucx_dt.h"
)

if (ENABLE_UCX_DTL)
    set(dtl_src "${dtl_src} ${ucx_dtl_src}")
    set(dtl_headers "${dtl_headers} ${ucx_dtl_headers}")
endif (ENABLE_UCX_DTL)

add_library(dyad_dtl OBJECT ${dtl_src} ${dtl_headers} ${dtl_install_headers})
target_compile_features(dyad_dtl PRIVATE c_std_99)
target_link_libraries(dyad_dtl PUBLIC utils Jansson::Jansson flux::core)

if (ENABLE_UCX_DTL)
    target_link_libraries(dyad_dtl PUBLIC ucx::ucp ucx::ucs)
    target_compile_definitions(dyad_dtl PRIVATE DYAD_ENABLE_UCX=1)
endif (ENABLE_UCX_DTL)

install(
    FILES ${dtl_install_headers}
    DESTINATION ${DYAD_INCLUDEDIR}
)