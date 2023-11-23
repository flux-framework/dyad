set(DYAD_MODULE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad.c)
set(DYAD_MODULE_PRIVATE_HEADERS )
set(DYAD_MODULE_PUBLIC_HEADERS )

add_library(dyad SHARED ${DYAD_MODULE_SRC})
target_link_libraries(dyad PUBLIC Jansson::Jansson flux::core)
target_link_libraries(dyad PRIVATE dyad_dtl)
target_compile_definitions(dyad PUBLIC BUILDING_DYAD=1)

if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    target_link_libraries(dyad PUBLIC perfflowaspect::perfflowaspect)
    target_compile_definitions(dyad PUBLIC DYAD_PERFFLOW=1)
endif()

set(DYAD_MODULE_TARGETS dyad)

#if (BUILD_URPC)
#    set(urpc_src
#        "${CMAKE_CURRENT_SOURCE_DIR}/urpc.c"
#    )
#    set(urpc_headers
#        "${CMAKE_CURRENT_SOURCE_DIR/urpc_ctx.h}"
#    )
#
#    add_library(urpc MODULE ${urpc_src} ${urpc_headers})
#    target_link_libraries(urpc PUBLIC Jansson::Jansson flux::core)
#    target_link_libraries(urpc PRIVATE utils)
#
#    if (ENABLE_PERFFLOW)
#        target_link_libraries(urpc PUBLIC perfflowaspect::perfflowaspect)
#    endif (ENABLE_PERFFLOW)
#    set(modules_targets "${modules_targets} urpc")
#endif (BUILD_URPC)

install(
    TARGETS ${DYAD_MODULE_TARGETS}
    LIBRARY DESTINATION ${DYAD_LIBDIR}
)