set(dyad_fstream_src
    "${CMAKE_CURRENT_SOURCE_DIR}/dyad_stream_core.cpp"
)
set(dyad_fstream_headers
)
set(dyad_fstream_install_headers
    "${CMAKE_CURRENT_SOURCE_DIR}/dyad_params.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dyad_stream_core.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dyad_stream_api.hpp"
)

add_library(dyad_fstream ${dyad_fstream_src} ${dyad_fstream_headers} ${dyad_fstream_install_headers})
target_compile_features(dyad_fstream PRIVATE cxx_std_11)
target_link_libraries(dyad_fstream PUBLIC dyad_core flux::core)

if (ENABLE_PERFFLOW)
    target_link_libraries(dyad_fstream PUBLIC perfflowaspect::perfflowaspect)
    target_compile_definitions(dyad_fstream PRIVATE DYAD_PERFFLOW=1)
endif (ENABLE_PERFFLOW)

install(
    FILES ${dyad_fstream_install_headers}
    DESTINATION ${DYAD_INCLUDEDIR}
)
install(
    TARGETS dyad_fstream
    LIBRARY DESTINATION ${DYAD_LIBDIR}
)