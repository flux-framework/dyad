#!/bin/bash

# Do 2 and 1 node runs for Remote and Local tests with varying ppn
ppns=( 1 2 4 8 16 32 64 )

for ppn in ${ppns[@]}; do
    # Run 2 node test for RemoteDataBandwidth
    ./corona.sh RemoteDataBandwidth 2 $ppn
    # Run 2 node test for RemoteDataAggBandwidth
    ./corona.sh RemoteDataAggBandwidth 2 $ppn
    # Run 1 node test for LocalProcessDataBandwidth
    ./corona.sh LocalProcessDataBandwidth 1 $ppn
    # Run 1 node test for LocalNodeDataBandwidth
    ./corona.sh LocalNodeDataBandwidth 1 $ppn
done

# Do local tests with varying number of nodes and ppn
num_nodes=( 2 4 8 16 32 64 )
ppns=( 16 32 64 )

for nn in ${num_nodes[@]}; do
    for ppn in ${ppns[@]}; do
        # Run 1 node test for LocalProcessDataBandwidth
        ./corona.sh LocalProcessDataBandwidth $nn $ppn
        # Run 1 node test for LocalNodeDataBandwidth
        ./corona.sh LocalNodeDataBandwidth $nn $ppn
    done
done

# Do remote tests with varying number of nodes and ppn
num_nodes=( 4 8 16 32 64 )
ppns=( 16 32 64 )

for nn in ${num_nodes[@]}; do
    for ppn in ${ppns[@]}; do
        # Run 1 node test for RemoteDataBandwidth
        ./corona.sh RemoteDataBandwidth $nn $ppn
        # Run 1 node test for RemoteDataAggDataBandwidth
        ./corona.sh RemoteDataAggBandwidth $nn $ppn
    done
done
