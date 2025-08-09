#!/bin/bash

test_case=$1
num_nodes=$2
ppn=$3
timing_root_dir=$4
use_alloc=$5

num_iters=16
num_files=16
request_size=65536
hg_conn_str="ofi+verbs"

extra_flux_flags="--setattr=system.bank=ice4hpc"

source ./setup-env.sh

timing_dir=$timing_root_dir/${test_case}_${num_nodes}_${ppn}

if [ -d $timing_dir ]; then
    echo "Dump directory $timing_dir already exists"
    exit 1
fi

mkdir -p $timing_dir

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

if $use_alloc; then
    flux alloc -q $QUEUE -t $TIME -N $num_nodes --exclusive $extra_flux_flags ./run.sh $test_case $num_nodes $ppn $num_iters $num_files $request_size $hg_conn_str $timing_dir $SCRIPT_DIR
else
    flux batch -q $QUEUE -t $TIME -N $num_nodes --output=$timing_dir/run.out --error=$timing_dir/run.err --exclusive $extra_flux_flags ./run.sh $test_case $num_nodes $ppn $num_iters $num_files $request_size $hg_conn_str $timing_dir $SCRIPT_DIR
fi
