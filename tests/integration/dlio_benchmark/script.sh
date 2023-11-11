#!/bin/bash

set -x

#DLIO_ENV=$1
DYAD_PROJECT_DIR=$1
SPACK_ENV=$2
#DLIO_ENV=/home/haridev/projects/dyad/venv
#DYAD_PROJECT_DIR=/home/haridev/projects/dyad
#SPACK_ENV=/home/haridev/projects/dyad/spack-env/.spack-env/view

#source ${DLIO_ENV}/bin/activate
#export PYTHONPATH=${DYAD_PROJECT_DIR}/tests/integration/dlio_benchmark:${DYAD_PROJECT_DIR}/pydyad:$PYTHONPATH
##export LD_LIBRARY_PATH=${DLIO_ENV}/lib:${DLIO_ENV}/lib64:${SPACK_ENV}/lib:${SPACK_ENV}:lib64:${LD_LIBRARY_PATH}
#export DYAD_KVS_NAMESPACE=test
#export DYAD_DTL_MODE=UCX
#export DYAD_PATH=$PWD/dyad
export DYAD_LOCAL_TEST=1
mkdir -p $DYAD_PATH/0
mkdir -p $DYAD_PATH/1
rm -rf $DYAD_PATH/0/* $DYAD_PATH/1/*

export DLIO_DATA_DIR=${DYAD_PROJECT_DIR}/tests/integration/dlio_benchmark/data
mkdir -p ${DLIO_DATA_DIR}

flux kvs namespace create ${DYAD_KVS_NAMESPACE}

echo Loading modules
flux exec -r 0 flux module load ${SPACK_ENV}/lib/dyad.so  $DYAD_PATH/0 $DYAD_DTL_MODE
flux exec -r 1 flux module load ${SPACK_ENV}/lib/dyad.so  $DYAD_PATH/1 $DYAD_DTL_MODE

echo Generate Data
flux run -N 2 --tasks-per-node 1 dlio_benchmark --config-dir=${DYAD_PROJECT_DIR}/tests/integration/dlio_benchmark/configs workload=dyad_unet3d ++workload.dataset.data_folder=${DLIO_DATA_DIR} ++workload.workflow.generate_data=True ++workload.workflow.train=False

echo Run Training
flux run -N 2 --tasks-per-node 1 dlio_benchmark --config-dir=${DYAD_PROJECT_DIR}/tests/integration/dlio_benchmark/configs workload=dyad_unet3d ++workload.dataset.data_folder=${DLIO_DATA_DIR} ++workload.workflow.generate_data=False ++workload.workflow.train=True

flux kvs namespace remove ${DYAD_KVS_NAMESPACE}
flux exec -r 0 flux module unload dyad
flux exec -r 1 flux module unload dyad