add_test(dspaces_start ${CMAKE_CURRENT_SOURCE_DIR}/dspaces_start.sh)
set_property(TEST dspaces_start APPEND PROPERTY ENVIRONMENT DSPACES_DEFAULT_VAR_SIZE=${DSPACES_DEFAULT_VAR_SIZE})
set_property(TEST dspaces_start APPEND PROPERTY ENVIRONMENT DSPACES_MAX_VERSIONS=$ENV{DSPACES_MAX_VERSIONS})
set_property(TEST dspaces_start APPEND PROPERTY ENVIRONMENT DSPACES_PPN=$ENV{DSPACES_PPN})
set_property(TEST dspaces_start APPEND PROPERTY ENVIRONMENT DSPACES_ROOT=${DSPACES_ROOT})
set_property(TEST dspaces_start APPEND PROPERTY ENVIRONMENT DSPACES_HG_CONNECTION_STR=$ENV{DSPACES_HG_CONNECTION_STR})

add_test(dspaces_stop ${CMAKE_CURRENT_SOURCE_DIR}/dspaces_stop.sh)
set_property(TEST dspaces_stop APPEND PROPERTY ENVIRONMENT DSPACES_ROOT=${DSPACES_ROOT})
