add_subdirectory(base64)

set(DYAD_UTILS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/utils.c
                   ${CMAKE_CURRENT_SOURCE_DIR}/read_all.c)
set(DYAD_UTILS_PRIVATE_HEADERS  ${CMAKE_CURRENT_SOURCE_DIR}/utils.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_structures.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/read_all.h)
set(DYAD_UTILS_PUBLIC_HEADERS)

set(DYAD_MURMUR3_SRC ${CMAKE_CURRENT_SOURCE_DIR}/murmur3.c)
set(DYAD_MURMUR3_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/murmur3.h)
set(DYAD_MURMUR3_PUBLIC_HEADERS)

add_library(${PROJECT_NAME}_utils SHARED ${DYAD_UTILS_SRC}
            ${DYAD_UTILS_PRIVATE_HEADERS} ${DYAD_UTILS_PUBLIC_HEADERS})
set_target_properties(${PROJECT_NAME}_utils PROPERTIES CMAKE_INSTALL_RPATH
                      "${CMAKE_INSTALL_PREFIX}/${DYAD_INSTALL_LIBDIR}")
target_link_libraries(${PROJECT_NAME}_utils PUBLIC
                      ${PROJECT_NAME}_base64
                      ${PROJECT_NAME}_murmur3)

if(DYAD_LOGGER STREQUAL "CPP_LOGGER")
    target_link_libraries(${PROJECT_NAME}_utils PRIVATE ${CPP_LOGGER_LIBRARIES})
endif()
if(DYAD_PROFILER STREQUAL "DFTRACER")
    target_link_libraries(${PROJECT_NAME}_utils PRIVATE ${DFTRACER_LIBRARIES})
endif()

target_compile_definitions(${PROJECT_NAME}_utils PUBLIC DYAD_HAS_CONFIG)
target_include_directories(${PROJECT_NAME}_utils PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:${DYAD_INSTALL_INCLUDEDIR}>)

add_library(${PROJECT_NAME}_murmur3 SHARED ${DYAD_MURMUR3_SRC})
set_target_properties(${PROJECT_NAME}_murmur3 PROPERTIES CMAKE_INSTALL_RPATH
                      "${CMAKE_INSTALL_PREFIX}/${DYAD_INSTALL_LIBDIR}")

add_executable(test_cmp_canonical_path_prefix test_cmp_canonical_path_prefix.c
               ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_structures.h)
target_compile_definitions(test_cmp_canonical_path_prefix PUBLIC DYAD_HAS_CONFIG)
target_link_libraries(test_cmp_canonical_path_prefix PUBLIC ${PROJECT_NAME}_utils)

add_executable(test_murmur3 test_murmur3.c)
target_compile_definitions(test_murmur3 PUBLIC DYAD_HAS_CONFIG)
target_link_libraries(test_murmur3 PUBLIC ${PROJECT_NAME}_murmur3)

if(DYAD_LOGGER STREQUAL "CPP_LOGGER")
    target_link_libraries(test_cmp_canonical_path_prefix PRIVATE ${CPP_LOGGER_LIBRARIES})
endif()
if(DYAD_PROFILER STREQUAL "DFTRACER")
    target_link_libraries(test_cmp_canonical_path_prefix PRIVATE ${DTRACER_LIBRARIES})
endif()


if (TARGET DYAD_C_FLAGS_werror)
  target_link_libraries(${PROJECT_NAME}_utils PRIVATE DYAD_C_FLAGS_werror)
  target_link_libraries(${PROJECT_NAME}_murmur3 PRIVATE DYAD_C_FLAGS_werror)
  target_link_libraries(test_murmur3 PRIVATE DYAD_C_FLAGS_werror)
  target_link_libraries(test_cmp_canonical_path_prefix PRIVATE DYAD_C_FLAGS_werror)
endif ()

install(
        TARGETS ${PROJECT_NAME}_utils
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BINDIR}
)
if(NOT "${DYAD_UTILS_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${DYAD_UTILS_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()
install(
        TARGETS ${PROJECT_NAME}_murmur3
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BINDIR}
)
if(NOT "${DYAD_MURMUR3_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${DYAD_UTILS_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()
