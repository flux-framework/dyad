#!/bin/bash

timing_root_dir=$1

echo "#!/bin/bash"

ppns=( 1 2 4 8 16 32 64 )

echo "# Remote ppn scaling test"
for ppn in ${ppns[@]}; do
    # Run 2 node test for RemoteDataBandwidth
    echo "./corona.sh RemoteDataBandwidth 2 $ppn $timing_root_dir/2_node_remote_data false"
    # Run 2 node test for RemoteDataAggBandwidth
    echo "./corona.sh RemoteDataAggBandwidth 2 $ppn $timing_root_dir/2_node_remote_data false"
done
echo ""

num_nodes=( 4 8 16 32 64 )
ppns=( 16 )

echo "# Num nodes remote scaling"
for nn in ${num_nodes[@]}; do
    for ppn in ${ppns[@]}; do
        # Run 1 node test for LocalProcessDataBandwidth
        echo "./corona.sh RemoteDataBandwidth $nn $ppn $timing_root_dir/multi_node_remote_data false"
        # Run 1 node test for LocalNodeDataBandwidth
        echo "./corona.sh RemoteDataAggBandwidth $nn $ppn $timing_root_dir/multi_node_remote_data false"
    done
done
echo ""

num_nodes=( 1 2 4 8 16 32 64 )
echo "# Metadata Perf"
for nn in ${num_nodes[@]}; do
    # Process local mdata
    echo "./corona.sh LocalProcessDataBandwidth $nn 16 $timing_root_dir/proc_local_mdata false"
    # Service mdata
    echo "./corona.sh LocalNodeDataBandwidth $nn 16 $timing_root_dir/service_mdata false"
    # Global mdata
    echo "./corona.sh RemoteDataBandwidth $nn 16 $timing_root_dir/global_mdata false"
done
echo ""

ppns=( 1 2 4 8 16 32 64 )

echo "# Local process ppn scaling"
for ppn in ${ppns[@]}; do
    # Run 2 node test for RemoteDataBandwidth
    echo "./corona.sh LocalProcessDataBandwidth 1 $ppn $timing_root_dir/1_node_local_data false"
    # Run 2 node test for RemoteDataAggBandwidth
    echo "./corona.sh LocalNodeDataBandwidth 1 $ppn $timing_root_dir/1_node_local_data false"
done
echo ""

num_nodes=( 2 4 8 16 32 64 )
ppns=( 16 )

echo "# Local node scaling"
for nn in ${num_nodes[@]}; do
    for ppn in ${ppns[@]}; do
        # Run 2 node test for RemoteDataBandwidth
        echo "./corona.sh LocalProcessDataBandwidth $nn $ppn $timing_root_dir/multi_node_local_data false"
        # Run 2 node test for RemoteDataAggBandwidth
        echo "./corona.sh LocalNodeDataBandwidth $nn $ppn $timing_root_dir/multi_node_local_data false"
    done
done
