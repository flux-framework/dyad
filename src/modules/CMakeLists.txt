set(dyad_mod_src
    "${CMAKE_CURRENT_SOURCE_DIR}/dyad.c"
)
set(dyad_mod_headers
)

add_library(dyad MODULE ${dyad_mod_src} ${dyad_mod_headers})
target_compile_features(dyad PRIVATE c_std_99)
target_link_libraries(dyad PUBLIC Jansson::Jansson flux::core)
target_link_libraries(dyad PRIVATE dyad_dtl)
target_compile_definitions(dyad BUILDING_DYAD=1)
# TODO figure out how to set RPATH, if needed

if (ENABLE_PERFFLOW)
    target_link_libraries(dyad PUBLIC perfflowaspect::perfflowaspect)
    target_compile_definitions(dyad DYAD_PERFFLOW=1)
endif (ENABLE_PERFFLOW)

set(modules_targets "dyad")

if (BUILD_URPC)
    set(urpc_src
        "${CMAKE_CURRENT_SOURCE_DIR}/urpc.c"
    )
    set(urpc_headers
        "${CMAKE_CURRENT_SOURCE_DIR/urpc_ctx.h}"
    )

    add_library(urpc MODULE ${urpc_src} ${urpc_headers})
    target_link_libraries(urpc PUBLIC Jansson::Jansson flux::core)
    target_link_libraries(urpc PRIVATE utils)
    
    if (ENABLE_PERFFLOW)
        target_link_libraries(urpc PUBLIC perfflowaspect::perfflowaspect)
    endif (ENABLE_PERFFLOW)
    set(modules_targets "${modules_targets} urpc")
endif (BUILD_URPC)

install(
    TARGETS ${modules_targets}
    LIBRARY DESTINATION ${DYAD_LIBDIR}
)