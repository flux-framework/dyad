find_package(MPI)

if (MPI_FOUND)
    set(DYAD_TEST_SHUFFLE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/shuffle.cpp
                              ${CMAKE_CURRENT_SOURCE_DIR}/worker.cpp)
    set(DYAD_TEST_SHUFFLE_PRIVATE_HEADERS  ${CMAKE_CURRENT_SOURCE_DIR}/worker.hpp
                                           ${CMAKE_CURRENT_SOURCE_DIR}/../src/dyad/utils/utils.h
                                           ${CMAKE_CURRENT_SOURCE_DIR}/../src/dyad/utils/read_all.h)
    set(DYAD_TEST_SHUFFLE_PUBLIC_HEADERS)
    
    
    add_executable(shuffle ${CMAKE_CURRENT_SOURCE_DIR}/shuffle.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/worker.cpp)
    target_compile_definitions(shuffle PUBLIC DYAD_HAS_CONFIG)
    target_include_directories(shuffle PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${DYAD_INSTALL_INCLUDE_DIR}>)
    target_link_libraries(shuffle PUBLIC ${PROJECT_NAME}_utils MPI::MPI_CXX)
    
    
    if(DYAD_LOGGER STREQUAL "CPP_LOGGER")
        target_link_libraries(shuffle PRIVATE ${CPP_LOGGER_LIBRARIES})
    endif()
    if(DYAD_PROFILER STREQUAL "DLIO_PROFILER")
        target_link_libraries(shuffle PRIVATE ${DLIO_PROFILER_LIBRARIES})
    endif()
    
    
    if (TARGET DYAD_CXX_FLAGS_werror)
        target_link_libraries(shuffle PRIVATE DYAD_CXX_FLAGS_werror)
    endif ()
endif (MPI_FOUND)
