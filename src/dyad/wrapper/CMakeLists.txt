set(DYAD_WRAPPER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/wrapper.c)
set(DYAD_WRAPPER_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../utils/utils.h
                                 ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_dtl.h
                                 ${CMAKE_CURRENT_SOURCE_DIR}/../core/dyad_ctx.h
                                 ${CMAKE_CURRENT_SOURCE_DIR}/../core/dyad_core.h)
set(DYAD_WRAPPER_PUBLIC_HEADERS)

add_library(${PROJECT_NAME}_wrapper SHARED ${DYAD_WRAPPER_SRC}
            ${DYAD_WRAPPER_PRIVATE_HEADERS} ${DYAD_WRAPPER_PUBLIC_HEADERS})
set_target_properties(${PROJECT_NAME}_wrapper PROPERTIES CMAKE_INSTALL_RPATH
                      "${CMAKE_INSTALL_PREFIX}/${DYAD_INSTALL_LIBDIR}")
target_link_libraries(${PROJECT_NAME}_wrapper PRIVATE ${PROJECT_NAME}_ctx ${PROJECT_NAME}_core)
target_link_libraries(${PROJECT_NAME}_wrapper PRIVATE ${PROJECT_NAME}_utils flux::core)
target_compile_definitions(${PROJECT_NAME}_wrapper PUBLIC BUILDING_DYAD=1)
target_compile_definitions(${PROJECT_NAME}_wrapper PUBLIC DYAD_HAS_CONFIG)
target_include_directories(${PROJECT_NAME}_wrapper PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:${DYAD_INSTALL_INCLUDEDIR}>)
target_include_directories(${PROJECT_NAME}_wrapper SYSTEM PRIVATE ${FluxCore_INCLUDE_DIRS})

if (TARGET DYAD_C_FLAGS_werror)
  target_link_libraries(${PROJECT_NAME}_wrapper PRIVATE DYAD_C_FLAGS_werror)
endif ()

set(FLUX_BARRIER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/flux_barrier.c)
set(FLUX_BARRIER_PRIVATE_HEADERS)
set(FLUX_BARRIER_PUBLIC_HEADERS)

add_executable(${PROJECT_NAME}_flux_barrier ${FLUX_BARRIER_SRC})
target_link_libraries(${PROJECT_NAME}_flux_barrier PRIVATE flux::core)
target_include_directories(${PROJECT_NAME}_flux_barrier SYSTEM PRIVATE ${FluxCore_INCLUDE_DIRS})

install(
        TARGETS ${PROJECT_NAME}_wrapper
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BINDIR}
)

if(NOT "${DYAD_WRAPPER_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${DYAD_WRAPPER_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()

install(
        TARGETS ${PROJECT_NAME}_flux_barrier
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BINDIR}
)

if(NOT "${FLUX_BARRIER_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${FLUX_BARRIER_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()
