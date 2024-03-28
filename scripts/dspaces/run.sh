#!/bin/bash
test_case=$1
NUM_NODES=$2
PPN=$3
NUM_ITERS=$4
NUM_FILES=$5
REQUEST_SIZE=$6
HG_CONNECTION_STR="$7"

source ./setup-env.sh

EXEC_DIR="${GITHUB_WORKSPACE}/build/bin"

echo Starting DataSpaces
# Setup DYAD
./dspaces_start.sh $(( REQUEST_SIZE * NUM_ITERS )) ${NUM_FILES} ${NUM_NODES} ${SERVER_PPN} ${HG_CONNECTION_STR}

echo "Running Test $WORKLOAD"
flux run -N ${NUM_NODES} --tasks-per-node=${PPN} ${EXEC_DIR}/unit_test --filename dp_${NUM_NODES}_${PPN} \
    --ppn ${PPN} --iteration ${NUM_ITERS} --number_of_files ${NUM_FILES} \
    --server_ppn ${SERVER_PPN} --request_size ${REQUEST_SIZE} --timing_dir ${TIMING_DIR} ${test_case}

echo Stopping DataSpaces
./dspaces_stop.sh
