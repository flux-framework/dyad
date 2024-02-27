set(DYAD_FCACHE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/fcache.cpp)
set(DYAD_FCACHE_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../utils/murmur3.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/../common/dyad_logging.h)
set(DYAD_FCACHE_PUBLIC_HEADERS  ${CMAKE_CURRENT_SOURCE_DIR}/fcache.hpp
                                ${CMAKE_CURRENT_SOURCE_DIR}/fcache_impl.hpp)


add_library(${PROJECT_NAME}_fcache SHARED ${DYAD_FCACHE_SRC}
            ${DYAD_FCACHE_PRIVATE_HEADERS} ${DYAD_FCACHE_PUBLIC_HEADERS})
set_target_properties(${PROJECT_NAME}_fcache PROPERTIES CMAKE_INSTALL_RPATH
                      "${CMAKE_INSTALL_PREFIX}/${DYAD_LIBDIR}")

if(DYAD_LOGGER STREQUAL "CPP_LOGGER")
    target_link_libraries(${PROJECT_NAME}_fcache PRIVATE ${CPP_LOGGER_LIBRARIES})
endif()
if(DYAD_PROFILER STREQUAL "DLIO_PROFILER")
    target_link_libraries(${PROJECT_NAME}_fcache PRIVATE ${DLIO_PROFILER_LIBRARIES})
endif()

target_compile_definitions(${PROJECT_NAME}_fcache PUBLIC DYAD_HAS_CONFIG)
target_include_directories(${PROJECT_NAME}_fcache PUBLIC
    ${Boost_INCLUDE_DIRS}
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:${DYAD_INSTALL_INCLUDE_DIR}>)

add_executable(test_fcache test_fcache.cpp)
target_compile_definitions(test_fcache PUBLIC DYAD_HAS_CONFIG)
target_link_libraries(test_fcache PUBLIC ${PROJECT_NAME}_fcache)
target_link_libraries(test_fcache PRIVATE ${PROJECT_NAME}_murmur3)

if(DYAD_LOGGER STREQUAL "CPP_LOGGER")
    target_link_libraries(test_fcache PRIVATE ${CPP_LOGGER_LIBRARIES})
endif()
if(DYAD_PROFILER STREQUAL "DLIO_PROFILER")
    target_link_libraries(test_cmp_fcache PRIVATE ${DLIO_PROFILER_LIBRARIES})
endif()


if (TARGET DYAD_CXX_FLAGS_werror)
  target_link_libraries(${PROJECT_NAME}_fcache PRIVATE DYAD_CXX_FLAGS_werror)
endif ()

install(
        TARGETS ${PROJECT_NAME}_fcache
        EXPORT ${DYAD_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${DYAD_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${DYAD_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${DYAD_INSTALL_BIN_DIR}
)
if(NOT "${DYAD_FCACHE_PUBLIC_HEADERS}" STREQUAL "")
    dyad_install_headers("${DYAD_FCACHE_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()
