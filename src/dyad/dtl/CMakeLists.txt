# DTL Interface
set(DTL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad_dtl_api.c)
set(DTL_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/dyad_dtl_api.h
                        ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_structures.h
                        ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_dtl.h
                        ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_rc.h)
set(DTL_PUBLIC_HEADERS)

# Flux implementation for DTL
set(FLUX_DTL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/flux_dtl.c)
set(FLUX_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/flux_dtl.h)
set(FLUX_PUBLIC_HEADERS)

# UCX implementation for DTL
set(UCX_DTL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/ucx_dtl.c ${CMAKE_CURRENT_SOURCE_DIR}/ucx_ep_cache.cpp)
set(UCX_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/ucx_dtl.h ${CMAKE_CURRENT_SOURCE_DIR}/ucx_ep_cache.h)
set(UCX_PUBLIC_HEADERS)

list(APPEND DTL_SRC ${FLUX_DTL_SRC})
list(APPEND DTL_PRIVATE_HEADERS ${FLUX_PRIVATE_HEADERS})
list(APPEND DTL_PUBLIC_HEADERS ${FLUX_PUBLIC_HEADERS})

if(DYAD_ENABLE_UCX_DTL OR DYAD_ENABLE_UCX_DATA_RMA)
    list(APPEND DTL_SRC ${UCX_DTL_SRC})
    list(APPEND DTL_PRIVATE_HEADERS ${UCX_PRIVATE_HEADERS})
    list(APPEND DTL_PUBLIC_HEADERS ${UCX_PUBLIC_HEADERS})
endif()

add_library(${PROJECT_NAME}_dtl SHARED ${DTL_SRC} ${DTL_PUBLIC_HEADERS} ${DTL_PRIVATE_HEADERS})
target_link_libraries(${PROJECT_NAME}_dtl PRIVATE ${PROJECT_NAME}_utils Jansson::Jansson flux::core flux::optparse)
set_target_properties(${PROJECT_NAME}_dtl PROPERTIES CMAKE_INSTALL_RPATH
                      "${CMAKE_INSTALL_PREFIX}/${DYAD_INSTALL_LIBDIR}")

if(DYAD_ENABLE_UCX_DTL OR DYAD_ENABLE_UCX_DATA_RMA)
    target_link_libraries(${PROJECT_NAME}_dtl PRIVATE ucx::ucp ucx::ucs)
    target_include_directories(${PROJECT_NAME}_dtl SYSTEM PUBLIC ${ucx_INCLUDE_DIRS})
endif()

target_compile_definitions(${PROJECT_NAME}_dtl PUBLIC DYAD_HAS_CONFIG)
target_include_directories(${PROJECT_NAME}_dtl PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:${DYAD_INSTALL_INCLUDEDIR}>)
target_include_directories(${PROJECT_NAME}_dtl SYSTEM PUBLIC ${JANSSON_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME}_dtl SYSTEM PUBLIC ${FluxCore_INCLUDE_DIRS})

dyad_add_werror_if_needed(${PROJECT_NAME}_dtl)
if(DYAD_LOGGER STREQUAL "CPP_LOGGER")
    target_link_libraries(${PROJECT_NAME}_dtl PRIVATE ${CPP_LOGGER_LIBRARIES})
endif()
if(DYAD_PROFILER STREQUAL "DFTRACER")
    target_link_libraries(${PROJECT_NAME}_dtl PRIVATE ${DFTRACER_LIBRARIES})
endif()

install(
        TARGETS ${PROJECT_NAME}_dtl
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BINDIR}
)
if(NOT ${DTL_PUBLIC_HEADERS} STREQUAL "")
    dyad_install_headers("${DTL_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()
