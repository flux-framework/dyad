#!/bin/bash
test_case=$1
NUM_NODES=$2
PPN=$3
NUM_ITERS=$4
NUM_FILES=$5
REQUEST_SIZE=$6
HG_CONNECTION_STR=$7
TIMING_DIR=$8
SCRIPT_DIR=$9

ulimit -c unlimited

test_cores=$(( NUM_NODES*32 ))

curr_dir=$(pwd)

cd $TIMING_DIR

source $SCRIPT_DIR/setup-env.sh

EXEC_DIR="${GITHUB_WORKSPACE}/build/bin"

# export DSPACES_DEBUG=1

echo Starting DataSpaces
# Setup DYAD
$SCRIPT_DIR/dspaces_start.sh $(( REQUEST_SIZE * NUM_ITERS )) ${NUM_FILES} ${NUM_NODES} ${SERVER_PPN} ${HG_CONNECTION_STR}

echo "Running Test $WORKLOAD"
flux run -N ${NUM_NODES} --cores=$test_cores --tasks-per-node=${PPN} \
    ${EXEC_DIR}/unit_test --filename dp_${NUM_NODES}_${PPN} \
    --ppn ${PPN} --iteration ${NUM_ITERS} --number_of_files ${NUM_FILES} \
    --server_ppn ${SERVER_PPN} --request_size ${REQUEST_SIZE} --timing_dir ${TIMING_DIR} ${test_case}

echo Stopping DataSpaces
$SCRIPT_DIR/dspaces_stop.sh

cd $curr_dir
