set(dyad_wrapper_src
    "${CMAKE_CURRENT_SOURCE_DIR}/wrapper.c"
)
set(dyad_wrapper_headers
)
set(dyad_wrapper_install_headers)

add_library(dyad_wrapper MODULE ${dyad_wrapper_src} ${dyad_wrapper_headers} ${dyad_wrapper_install_headers})
target_compile_features(dyad_wrapper PRIVATE c_std_99)
target_link_libraries(dyad_wrapper PUBLIC dyad_core flux::core)
target_link_libraries(dyad_wrapper PRIVATE utils)
target_compile_definitions(dyad_wrapper BUILDING_DYAD=1)

set(flux_barrier_src
    "${CMAKE_CURRENT_SOURCE_DIR}/flux_barrier.c"
)
set(flux_barrier_headers
)

add_executable(flux_barrier ${flux_barrier_src} ${flux_barrier_headers})
target_link_libraries(flux_barrier PUBLIC flux::core)

install(
    TARGETS dyad_wrapper flux_barrier
    LIBRARY DESTINATION ${DYAD_LIBDIR}
    RUNTIME DESTINATION ${DYAD_BINDIR}
)