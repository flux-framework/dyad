set(dyad_core_src
    "${CMAKE_CURRENT_SOURCE_DIR}/dyad_core.c"
)
set(dyad_core_headers
)
set(dyad_core_install_headers
    "${CMAKE_CURRENT_SOURCE_DIR}/dyad_core.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/dyad_envs.h"
)

add_library(dyad_core ${dyad_core_src} ${dyad_core_headers} ${dyad_core_install_headers})
target_compile_features(dyad_core PRIVATE c_std_99)
target_link_libraries(dyad_core PUBLIC Jansson::Jansson flux::core)
# TODO see if linking dyad_dtl carries over the UCX dependency
target_link_libraries(dyad_core PRIVATE utils murmur3 dyad_dtl)
target_compile_definitions(dyad_core BUILDING_DYAD=1)

if (ENABLE_PERFFLOW)
    target_link_libraries(dyad_core PUBLIC perfflowaspect::perfflowaspect)
    target_compile_definitions(dyad_core DYAD_PERFFLOW=1)
endif (ENABLE_PERFFLOW)

install(
    FILES ${dyad_core_install_headers}
    DESTINATION include
)
install(
    TARGETS dyad_core
    LIBRARY DESTINATION ${DYAD_LIBDIR}
)