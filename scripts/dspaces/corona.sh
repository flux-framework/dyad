#!/bin/bash

test_case=$1
num_nodes=$2
ppn=$3

num_iters=16
num_files=16
request_size=65536
hg_conn_str="ucx+rc"

source ./setup-env.sh

flux alloc -q $QUEUE -t $TIME -N $NUM_NODES --exclusive ./run.sh $test_case $num_nodes $ppn $num_iters $num_files $request_size $hg_conn_str
