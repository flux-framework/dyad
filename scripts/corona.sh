#!/bin/bash
test_case=$1
NUM_NODES=$2
PPN=$3
source ./setup-env.sh ${test_case} $NUM_NODES $PPN
rm *.core flux.log
rm -rf logs/* profiler/*
flux alloc -q $QUEUE -t $TIME -N $NUM_NODES -o per-resource.count=${BROKERS_PER_NODE} --exclusive --broker-opts=--setattr=log-filename=./logs/flux.log ./run.sh $test_case $NUM_NODES $PPN
