add_subdirectory(flux_module)
add_subdirectory(margo)

set(DYAD_EXE "dyad_exe")
set(DYAD_EXE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dyad_exe.c)
set(DYAD_EXE_PRIVATE_HEADERS)
set(DYAD_EXE_PUBLIC_HEADERS)

add_executable(${DYAD_EXE} ${DYAD_EXE_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dyad/common/dyad_dtl.h
)
# The target we use in this CMAKE file is "dyad_exe"
# to avoid using an identical target name as the flux module
# But, the output binary is still named "dyad".
set_property(TARGET ${DYAD_EXE} PROPERTY OUTPUT_NAME dyad)
target_compile_definitions(${DYAD_EXE} PUBLIC DYAD_HAS_CONFIG)

install(
  TARGETS ${DYAD_EXE}
  EXPORT  ${DYAD_EXPORTED_TARGETS}
  LIBRARY DESTINATION ${DYAD_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${DYAD_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${DYAD_INSTALL_BINDIR}
)

if(NOT "${DYAD_EXE_PUBLIC_HEADERS}" STREQUAL "")
  dyad_install_headers("${DYAD_EXE_PUBLIC_HEADERS}" ${CMAKE_CURRENT_SOURCE_DIR})
endif()
