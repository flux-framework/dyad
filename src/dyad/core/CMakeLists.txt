set(DYAD_CORE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad_core.c)
set(DYAD_CORE_PRIVATE_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/dyad_envs.h)
set(DYAD_CORE_PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/dyad_core.h)

add_library(dyad_core SHARED ${DYAD_CORE_SRC})
target_link_libraries(dyad_core PUBLIC Jansson::Jansson flux::core)
target_link_libraries(dyad_core PRIVATE utils murmur3 dyad_dtl)
target_compile_definitions(dyad_core PUBLIC BUILDING_DYAD=1)

if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    target_link_libraries(dyad_core PUBLIC perfflowaspect::perfflowaspect)
    target_compile_definitions(dyad_core DYAD_PERFFLOW=1)
endif()

install(
    FILES ${dyad_core_install_headers}
    DESTINATION include
)
install(
    TARGETS dyad_core
    LIBRARY DESTINATION ${DYAD_LIBDIR}
)