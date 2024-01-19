set(DYAD_CORE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad_core.c)
set(DYAD_CORE_PRIVATE_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/dyad_envs.h ${CMAKE_CURRENT_SOURCE_DIR}/dyad_core.h)
set(DYAD_CORE_PUBLIC_HEADER)

add_library(${PROJECT_NAME}_core SHARED ${DYAD_CORE_SRC}
            ${DYAD_CORE_PUBLIC_HEADER} ${DYAD_CORE_PRIVATE_HEADER})
set_target_properties(${PROJECT_NAME}_core PROPERTIES CMAKE_INSTALL_RPATH
                      "${CMAKE_INSTALL_PREFIX}/${DYAD_LIBDIR}")
target_link_libraries(${PROJECT_NAME}_core PRIVATE Jansson::Jansson flux::core flux::optparse)
target_link_libraries(${PROJECT_NAME}_core PRIVATE ${PROJECT_NAME}_utils ${PROJECT_NAME}_murmur3 ${PROJECT_NAME}_dtl)

target_compile_definitions(${PROJECT_NAME}_core PUBLIC BUILDING_DYAD=1)
target_compile_definitions(${PROJECT_NAME}_core PUBLIC DYAD_HAS_CONFIG)
target_include_directories(${PROJECT_NAME}_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>  # will be used for generated code
    $<INSTALL_INTERFACE:${DYAD_INSTALL_INCLUDE_DIR}>) # wil be used for sub projects
target_include_directories(${PROJECT_NAME}_core SYSTEM PRIVATE ${JANSSON_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME}_core SYSTEM PRIVATE ${FluxCore_INCLUDE_DIRS})

if (TARGET DYAD_C_FLAGS_werror)
  target_link_libraries(${PROJECT_NAME}_core PRIVATE DYAD_C_FLAGS_werror)
endif ()

if(DYAD_PROFILER STREQUAL "PERFFLOW_ASPECT")
    target_link_libraries(${PROJECT_NAME}_core PRIVATE perfflowaspect::perfflowaspect)
    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${perfflowaspect_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME}_core PRIVATE DYAD_PERFFLOW=1)
endif()
if(DYAD_PROFILER STREQUAL "DLIO_PROFILER")
    target_link_libraries(${PROJECT_NAME}_core PRIVATE ${DLIO_PROFILER_LIBRARIES})
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
