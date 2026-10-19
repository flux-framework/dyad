add_test(unit_dyad_core ${CMAKE_BINARY_DIR}/bin/unit_test --reporter mpi_console gen_path_key)
add_test(unit_dyad_core_dtl_finalize ${CMAKE_BINARY_DIR}/bin/unit_test --reporter mpi_console "dyad_dtl_finalize reports an invalid mode")
