set(base64_src
    "${CMAKE_CURRENT_SOURCE_DIR}/base64.c"
)
set(base64_headers
    "${CMAKE_CURRENT_SOURCE_DIR}/base64.h"
)

add_library(base64 OBJECT ${base64_src} ${base64_headers})
# TODO set C99
target_compile_features(base64 PRIVATE c_std_99)