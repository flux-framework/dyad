add_subdirectory(base64)

set(utils_src
    "${CMAKE_CURRENT_SOURCE_DIR}/utils.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/read_all.c"
)
set(utils_headers
    "${CMAKE_CURRENT_SOURCE_DIR}/utils.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/read_all.h"
)

set(murmur3_src
    "${CMAKE_CURRENT_SOURCE_DIR}/murmur3.c"
)
set(murmur3_headers
    "${CMAKE_CURRENT_SOURCE_DIR}/murmur3.h"
)

add_library(utils OBJECT ${utils_src} ${utils_headers})
target_include_directories(utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(utils PUBLIC flux::core base64)
target_compile_features(utils PRIVATE c_std_99)

add_library(murmur3 OBJECT ${murmur3_src} ${murmur3_headers})
target_include_directories(murmur3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(murmur3 PRIVATE c_std_99)