set(DYAD_COMMON_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/dyad_rc.h
                        ${CMAKE_CURRENT_SOURCE_DIR}/dyad_dtl.h
                        ${CMAKE_CURRENT_SOURCE_DIR}/dyad_envs.h
                        ${CMAKE_CURRENT_SOURCE_DIR}/dyad_logging.h
                        ${CMAKE_CURRENT_SOURCE_DIR}/dyad_profiler.h)
                    
add_library(${PROJECT_NAME}_common INTERFACE)
if (DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    target_link_libraries(${PROJECT_NAME}_common INTERFACE perfflowaspect::perfflowaspect)
    target_include_directories(${PROJECT_NAME}_common INTERFACE ${perfflowaspect_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME}_common INTERFACE DYAD_PERFFLOW=1)
endif()
if (DYAD_PROFILER STREQUAL "DLIO_PROFILER")
    target_link_libraries(${PROJECT_NAME}_common INTERFACE ${DLIO_PROFILER_LIBRARIES})
endif()
if (DYAD_LOGGER STREQUAL "CPP_LOGGER")
    target_link_libraries(${PROJECT_NAME}_common INTERFACE ${CPP_LOGGER_LIBRARIES})
endif()