set(DYAD_WRAPPER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/wrapper.c)
set(DYAD_WRAPPER_PRIVATE_HEADERS )
set(DYAD_WRAPPER_PUBLIC_HEADERS )

add_library(dyad_wrapper SHARED ${DYAD_WRAPPER_SRC})
target_link_libraries(dyad_wrapper PUBLIC dyad_core flux::core)
target_link_libraries(dyad_wrapper PRIVATE utils)
target_compile_definitions(dyad_wrapper PUBLIC BUILDING_DYAD=1)

set(FLUX_BARRIER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/flux_barrier.c)
set(FLUX_BARRIER_PRIVATE_HEADERS )
set(FLUX_BARRIER_PUBLIC_HEADERS )

add_executable(flux_barrier ${FLUX_BARRIER_SRC})
target_link_libraries(flux_barrier PUBLIC flux::core)
install(
        TARGETS dyad_wrapper
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BIN_DIR}
)
if(NOT "${DYAD_WRAPPER_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${DYAD_WRAPPER_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()
install(
        TARGETS flux_barrier
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BIN_DIR}
)
if(NOT "${FLUX_BARRIER_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${FLUX_BARRIER_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()