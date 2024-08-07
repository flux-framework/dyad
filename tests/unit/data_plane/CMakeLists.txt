
set(files 16)
set(ts 65536)
set(ops 16)
set(ppns 1 2 4 8 16 32 64)

function(add_dp_remote_test node ppn files ts ops)
    # Remote Streaming RPC over RDMA
    set(test_name unit_remote_data_${node}_${ppn})
    add_test(${test_name} flux run -N ${node} --tasks-per-node ${ppn} ${CMAKE_BINARY_DIR}/bin/unit_test --filename dp_${node}_${ppn} --ppn ${ppn} --pfs $ENV{DYAD_PFS_DIR} --dmd $ENV{DYAD_DMD_DIR} --iteration ${ops} --number_of_files ${files} --request_size ${ts} --reporter mpi_console RemoteDataBandwidth)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_KVS_NAMESPACE=${DYAD_KEYSPACE})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_MODULE_SO=${CMAKE_BINARY_DIR}/${DYAD_LIBDIR}/dyad.so)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_LOG_DIR=${DYAD_LOG_DIR})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_DTL_MODE=UCX)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_PATH_CONSUMER=$ENV{DYAD_DMD_DIR})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_PATH_PRODUCER=$ENV{DYAD_DMD_DIR})
    # Remote Agg
    set(test_name unit_remote_agg_data_${node}_${ppn})
    add_test(${test_name} flux run -N ${node} --tasks-per-node ${ppn} ${CMAKE_BINARY_DIR}/bin/unit_test --filename dp_${node}_${ppn} --ppn ${ppn} --pfs $ENV{DYAD_PFS_DIR} --dmd $ENV{DYAD_DMD_DIR} --iteration ${ops} --number_of_files ${files} --request_size ${ts} --reporter mpi_console RemoteDataAggBandwidth)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_KVS_NAMESPACE=${DYAD_KEYSPACE})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_MODULE_SO=${CMAKE_BINARY_DIR}/${DYAD_LIBDIR}/dyad.so)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_LOG_DIR=${DYAD_LOG_DIR})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_DTL_MODE=UCX)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_PATH_CONSUMER=$ENV{DYAD_DMD_DIR})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_PATH_PRODUCER=$ENV{DYAD_DMD_DIR})
endfunction()

function(add_dp_local_test node ppn files ts ops)
    # Process Local
    set(test_name unit_process_local_data_${node}_${ppn})
    add_test(${test_name} flux run -N ${node} --tasks-per-node ${ppn} ${CMAKE_BINARY_DIR}/bin/unit_test --filename dp_${node}_${ppn} --ppn ${ppn} --pfs $ENV{DYAD_PFS_DIR} --dmd $ENV{DYAD_DMD_DIR} --iteration ${ops} --number_of_files ${files} --request_size ${ts} --reporter compact LocalProcessDataBandwidth)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_KVS_NAMESPACE=${DYAD_KEYSPACE})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_MODULE_SO=${CMAKE_BINARY_DIR}/${DYAD_LIBDIR}/dyad.so)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_LOG_DIR=${DYAD_LOG_DIR})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_DTL_MODE=UCX)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_PATH_CONSUMER=$ENV{DYAD_DMD_DIR})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_PATH_PRODUCER=$ENV{DYAD_DMD_DIR})
    # Node Local
    set(test_name unit_node_local_data_${node}_${ppn})
    add_test(${test_name} flux run -N ${node} --tasks-per-node ${ppn} ${CMAKE_BINARY_DIR}/bin/unit_test --filename dp_${node}_${ppn} --ppn ${ppn} --pfs $ENV{DYAD_PFS_DIR} --dmd $ENV{DYAD_DMD_DIR} --iteration ${ops} --number_of_files ${files} --request_size ${ts} --reporter compact LocalNodeDataBandwidth)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_KVS_NAMESPACE=${DYAD_KEYSPACE})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_MODULE_SO=${CMAKE_BINARY_DIR}/${DYAD_LIBDIR}/dyad.so)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_LOG_DIR=${DYAD_LOG_DIR})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_DTL_MODE=UCX)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_PATH_CONSUMER=$ENV{DYAD_DMD_DIR})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DYAD_PATH_PRODUCER=$ENV{DYAD_DMD_DIR})
endfunction()


foreach (ppn ${ppns})
    add_dp_remote_test(2 ${ppn} ${files} ${ts} ${ops})
    add_dp_local_test(1 ${ppn} ${files} ${ts} ${ops})
endforeach ()
set(nodes 2 4 8 16 32 64)
set(ppns 16 32 64)
foreach (node ${nodes})
    foreach (ppn ${ppns})
        add_dp_local_test(${node} ${ppn} ${files} ${ts} ${ops})
    endforeach ()
endforeach ()
set(nodes 4 8 16 32 64)
foreach (node ${nodes})
    foreach (ppn ${ppns})
        add_dp_remote_test(${node} ${ppn} ${files} ${ts} ${ops})
    endforeach ()
endforeach ()
