#!/bin/bash
test_case=$1
NUM_NODES=$2
PPN=$3
source ./setup-env.sh $test_case $NUM_NODES $PPN


pushd ${GITHUB_WORKSPACE}/build
echo Starting DYAD
# Setup DYAD
ctest -R dyad_start -VV 

# Setup Local Directories
echo Setting up local directories
flux run -N $((NUM_NODES*BROKERS_PER_NODE)) --tasks-per-node=1 mkdir -p $DYAD_PATH
flux run -N $((NUM_NODES*BROKERS_PER_NODE)) --tasks-per-node=1 rm -rf $DYAD_PATH/*

echo "Running Test $WORKLOAD"
ctest -R ${WORKLOAD}$ -VV 

echo Stopping DYAD
ctest -R dyad_stop -VV
