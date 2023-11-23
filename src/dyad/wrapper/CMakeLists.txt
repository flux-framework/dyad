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
    TARGETS dyad_wrapper flux_barrier
    LIBRARY DESTINATION ${DYAD_LIBDIR}
    RUNTIME DESTINATION ${DYAD_BINDIR}
)