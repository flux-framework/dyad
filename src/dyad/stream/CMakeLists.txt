set(DYAD_FSTREAM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad_stream_core.cpp)
set(DYAD_FSTREAM_PRIVATE_HEADERS )
set(DYAD_FSTREAM_PUBLIC_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/stream/dyad_stream_api.hpp
                                ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/stream/dyad_params.hpp
                                ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/stream/dyad_stream_core.hpp)

add_library(${PROJECT_NAME}_fstream SHARED ${DYAD_FSTREAM_SRC} ${DYAD_FSTREAM_PRIVATE_HEADERS} ${DYAD_FSTREAM_PUBLIC_HEADERS})
target_compile_features(${PROJECT_NAME}_fstream PRIVATE )
target_link_libraries(${PROJECT_NAME}_fstream PUBLIC ${PROJECT_NAME}_core flux::core)
target_include_directories(${PROJECT_NAME}_fstream PUBLIC $<INSTALL_INTERFACE:${DYAD_INSTALL_INCLUDE_DIR}>)

if (ENABLE_PERFFLOW)
    target_link_libraries(${PROJECT_NAME}_fstream PUBLIC perfflowaspect::perfflowaspect)
    target_compile_definitions(${PROJECT_NAME}_fstream PRIVATE DYAD_PERFFLOW=1)
endif (ENABLE_PERFFLOW)

install(
        TARGETS ${PROJECT_NAME}_fstream
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BIN_DIR}
)
if(NOT "${DYAD_FSTREAM_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${DYAD_FSTREAM_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()