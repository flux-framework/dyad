include_directories(${CMAKE_SOURCE_DIR}/src)  # usual source code of the current project
include_directories(${CMAKE_BINARY_DIR}/include)
add_subdirectory(utils)
add_subdirectory(dtl)
add_subdirectory(core)
add_subdirectory(client)
add_subdirectory(service)
add_subdirectory(wrapper)
add_subdirectory(stream)
