# DTL Interface
set(DTL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad_dtl_impl.c)
set(DTL_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/dyad_dtl_impl.h
                        ${CMAKE_CURRENT_SOURCE_DIR}/dyad_rc.h)
set(DTL_PUBLIC_HEADERS )

# Flux implementation for DTL
set(FLUX_DTL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/flux_dtl.c)
set(FLUX_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/flux_dtl.h)
set(FLUX_PUBLIC_HEADERS )

# UCX implementation for DTL
set(UCX_DTL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/ucx_dt.c)
set(UCX_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/ucx_dt.h)
set(UCX_PUBLIC_HEADERS )

if(DYAD_DATA_PLANE STREQUAL "UCX")
    set(DTL_SRC ${DTL_SRC} ${UCX_DTL_SRC})
    set(DTL_PRIVATE_HEADERS ${DTL_PRIVATE_HEADERS} ${UCX_PRIVATE_HEADERS})
    set(DTL_PUBLIC_HEADERS ${DTL_PUBLIC_HEADERS} ${UCX_PUBLIC_HEADERS})
elseif(DYAD_DATA_PLANE STREQUAL "FLUX_RPC")
    set(DTL_SRC ${DTL_SRC} ${FLUX_DTL_SRC})
    set(DTL_PRIVATE_HEADERS ${DTL_PRIVATE_HEADERS} ${FLUX_PRIVATE_HEADERS})
    set(DTL_PUBLIC_HEADERS ${DTL_PUBLIC_HEADERS} ${FLUX_PUBLIC_HEADERS})
else()
    message(FATAL_ERROR "-- [${PROJECT_NAME}] Unknown DYAD_DATA_PLANE configuration ${DYAD_DATA_PLANE}")
endif()

add_library(${PROJECT_NAME}_dtl SHARED ${DTL_SRC})
target_link_libraries(${PROJECT_NAME}_dtl PUBLIC utils Jansson::Jansson flux::core)

if(DYAD_DATA_PLANE STREQUAL "UCX")
    target_link_libraries(${PROJECT_NAME}_dtl PUBLIC ucx::ucp ucx::ucs)
    target_compile_definitions(${PROJECT_NAME}_dtl PRIVATE DYAD_ENABLE_UCX=1)
endif()

install(
    FILES ${dtl_install_headers}
    DESTINATION ${DYAD_INCLUDEDIR}
)