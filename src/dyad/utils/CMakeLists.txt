add_subdirectory(base64)

set(DYAD_UTILS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/utils.c
                   ${CMAKE_CURRENT_SOURCE_DIR}/read_all.c)
set(DYAD_UTILS_PRIVATE_HEADERS  ${CMAKE_CURRENT_SOURCE_DIR}/utils.h
                                ${CMAKE_CURRENT_SOURCE_DIR}/read_all.h)
set(DYAD_UTILS_PUBLIC_HEADERS )

set(DYAD_MURMUR3_SRC ${CMAKE_CURRENT_SOURCE_DIR}/murmur3.c)
set(DYAD_MURMUR3_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/murmur3.h)
set(DYAD_MURMUR3_PUBLIC_HEADERS )

add_library(utils SHARED ${DYAD_UTILS_SRC})
target_link_libraries(utils PUBLIC flux::core base64)

add_library(murmur3 SHARED ${DYAD_MURMUR3_SRC})