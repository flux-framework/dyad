#!/bin/bash

timing_root_dir=$1

ppns=( 1 2 4 8 16 32 64 )

for ppn in ${ppns[@]}; do
    # Run 2 node test for RemoteDataBandwidth
    ./corona.sh RemoteDataBandwidth 2 $ppn $timing_root_dir/2_node_remote_data false
    # Run 2 node test for RemoteDataAggBandwidth
    ./corona.sh RemoteDataAggBandwidth 2 $ppn $timing_root_dir/2_node_remote_data false
done

num_nodes=( 4 8 16 32 64 )
ppns=( 16 )

for nn in ${num_nodes[@]}; do
    for ppn in ${ppns[@]}; do
        # Run 1 node test for LocalProcessDataBandwidth
        ./corona.sh RemoteDataBandwidth $nn $ppn $timing_root_dir/multi_node_remote_data false
        # Run 1 node test for LocalNodeDataBandwidth
        ./corona.sh RemoteDataAggBandwidth $nn $ppn $timing_root_dir/multi_node_remote_data false
    done
done

num_nodes=( 1 2 4 8 16 32 64 )
for nn in ${num_nodes[@]}; do
    # Process local mdata
    ./corona.sh LocalProcessDataBandwidth $nn 64 $timing_root_dir/proc_local_mdata false
    # Service mdata
    ./corona.sh LocalNodeDataBandwidth $nn 64 $timing_root_dir/service_mdata false
    # Global mdata
    ./corona.sh RemoteDataBandwidth $nn 64 $timing_root_dir/global_mdata false
done

ppns=( 1 2 4 8 16 32 64 )

for ppn in ${ppns[@]}; do
    # Run 2 node test for RemoteDataBandwidth
    ./corona.sh LocalProcessDataBandwidth 1 $ppn $timing_root_dir/1_node_local_data false
    # Run 2 node test for RemoteDataAggBandwidth
    ./corona.sh LocalNodeDataBandwidth 1 $ppn $timing_root_dir/1_node_local_data false
done

num_nodes=( 2 4 8 16 32 64 )
ppns=( 64 )

for nn in ${num_nodes[@]}; do
    for ppn in ${ppns[@]}; do
        # Run 2 node test for RemoteDataBandwidth
        ./corona.sh LocalProcessDataBandwidth $nn $ppn $timing_root_dir/multi_node_local_data false
        # Run 2 node test for RemoteDataAggBandwidth
        ./corona.sh LocalNodeDataBandwidth $nn $ppn $timing_root_dir/multi_node_local_data false
    done
done
