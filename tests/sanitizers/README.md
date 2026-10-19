# Sanitizer support

The `mpi_finalize_repro` CTest target runs only `MPI_Init()` and
`MPI_Finalize()`. Run it under LeakSanitizer after upgrading the CI image's
MPI or hwloc packages to determine whether the known external leak remains.

For sanitizer runs using Ubuntu 24.04's MPICH 4.1 packages, apply the narrow
suppression with:

```sh
LSAN_OPTIONS=suppressions=$PWD/tests/sanitizers/lsan-mpich-hwloc.supp \
ctest --test-dir build --output-on-failure
```
