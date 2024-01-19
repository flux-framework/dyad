include_directories(${CMAKE_SOURCE_DIR}/src)  # usual source code of the current project

add_subdirectory(utils)
add_subdirectory(dtl)
add_subdirectory(core)
add_subdirectory(modules)
add_subdirectory(wrapper)
add_subdirectory(stream)

