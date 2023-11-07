#!/bin/bash

set -x

DLIO_ENV=$1
DYAD_PROJECT_DIR=$2
DLIO_ENV=/home/haridev/projects/dyad/venv
DYAD_PROJECT_DIR=/home/haridev/projects/dyad

source ${DLIO_ENV}/bin/activate
export PYTHONPATH=${DYAD_PROJECT_DIR}/tests/integration/dlio_benchmark:$PYTHONPATH
export LD_LIBRARY_PATH=${DLIO_ENV}/lib:${DLIO_ENV}/lib64:${SPACK_ENV}/lib:${SPACK_ENV}/lib64:${LD_LIBRARY_PATH}
export DYAD_KVS_NAMESPACE=test
export DYAD_DTL_MODE=UCX
export DYAD_PATH=$PWD/dyad
export DYAD_PATH_PRODUCER=$DYAD_PATH
export DYAD_PATH_CONSUMER=$DYAD_PATH
mkdir -p $DYAD_PATH

echo Generate Data
dlio_benchmark --config-dir=${DYAD_PROJECT_DIR}/tests/integration/dlio_benchmark/configs workload=dyad_unet3d ++workload.workflow.generate_data=True ++workload.workflow.train=False

echo Run Training
dlio_benchmark --config-dir=${DYAD_PROJECT_DIR}/tests/integration/dlio_benchmark/configs workload=dyad_unet3d ++workload.workflow.generate_data=False ++workload.workflow.train=True
