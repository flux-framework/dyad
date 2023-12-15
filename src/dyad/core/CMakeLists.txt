set(DYAD_CORE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad_core.c)
set(DYAD_CORE_PRIVATE_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/dyad_envs.h ${CMAKE_CURRENT_SOURCE_DIR}/dyad_core.h)
set(DYAD_CORE_PUBLIC_HEADER )

add_library(${PROJECT_NAME}_core SHARED ${DYAD_CORE_SRC})
target_link_libraries(${PROJECT_NAME}_core PUBLIC Jansson::Jansson flux::core)
target_link_libraries(${PROJECT_NAME}_core PRIVATE ${PROJECT_NAME}_utils ${PROJECT_NAME}_murmur3 ${PROJECT_NAME}_dtl)
target_compile_definitions(${PROJECT_NAME}_core PUBLIC BUILDING_DYAD=1)

if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    target_link_libraries(${PROJECT_NAME}_core PUBLIC perfflowaspect::perfflowaspect)
    target_compile_definitions(${PROJECT_NAME}_core DYAD_PERFFLOW=1)
endif()
install(
        TARGETS ${PROJECT_NAME}_core
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BIN_DIR}
)
if(NOT "${DYAD_CORE_PUBLIC_HEADER}" STREQUAL "")
    dyad_install_headers("${DYAD_CORE_PUBLIC_HEADER}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()