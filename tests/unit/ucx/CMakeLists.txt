add_executable(ucx_test ucx_test.cpp ${TEST_SRC})
target_link_libraries(ucx_test ${TEST_LIBS} ucx::ucp ucx::ucs flux::core)
add_executable(ucx_example_test test.c )
target_link_libraries(ucx_example_test ${MPI_C_LIBRARIES} ucx::ucp ucx::ucs)