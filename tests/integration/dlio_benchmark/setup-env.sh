#!/bin/bash

module load python/3.9.12
module load openmpi/4.1.2

# Configurations
export DLIO_WORKLOAD=dyad_unet3d # unet3d_base dyad_unet3d dyad_unet3d_small
export NUM_NODES=2
export PPN=8
export BROKERS_PER_NODE=1
export GENERATE_DATA="0"

export DYAD_INSTALL_PREFIX=/usr/workspace/haridev/dyad/env/spack/.spack-env/view
export DYAD_KVS_NAMESPACE=dyad
export DYAD_DTL_MODE=UCX
export DYAD_PATH=/l/ssd/haridev/dyad
export DYAD_PATH=/dev/shm/haridev/dyad
export GITHUB_WORKSPACE=/usr/workspace/haridev/dyad
export SPACK_DIR=/usr/workspace/haridev/spack-new
export SPACK_ENV=/usr/workspace/haridev/dyad/env/spack
export PYTHON_ENV=/usr/workspace/haridev/dyad/env/python
export DLIO_DATA_DIR=/p/lustre2/haridev/dyad/dlio_benchmark/dyad_unet3d_basic

# DLIO Profiler Configurations
export DLIO_PROFILER_ENABLE=1
export DLIO_PROFILER_INC_METADATA=1
export DLIO_PROFILER_DATA_DIR=${DLIO_DATA_DIR}:${DYAD_PATH}
export DLIO_PROFILER_LOG_FILE=/usr/workspace/haridev/dyad/tests/integration/dlio_benchmark/profiler/dyad
export DYAD_LOG_DIR=/usr/workspace/haridev/dyad/tests/integration/dlio_benchmark/logs
export DLIO_PROFILER_LOG_LEVEL=ERROR
#export GOTCHA_DEBUG=3

export DLIO_PROFILER_BIND_SIGNALS=0
export MV2_BCAST_HWLOC_TOPOLOGY=0

mkdir -p ${DYAD_PATH}
mkdir -p ${DLIO_PROFILER_LOG_FILE}
# Activate Environments
. ${SPACK_DIR}/share/spack/setup-env.sh
spack env activate -p ${SPACK_ENV}
source ${PYTHON_ENV}/bin/activate 

# Derived Configurations
export DYAD_DLIO_RUN_LOG=dyad_${DLIO_WORKLOAD}_${NUM_NODES}_${PPN}_${BROKERS_PER_NODE}.log
export CONFIG_ARG="--config-dir=${GITHUB_WORKSPACE}/tests/integration/dlio_benchmark/configs"
#export CONFIG_ARG=""

# Derived PATHS
export PATH=${PATH}:${DYAD_INSTALL_PREFIX}/bin:${DYAD_INSTALL_PREFIX}/sbin
export LD_LIBRARY_PATH=/usr/lib64:${DYAD_INSTALL_PREFIX}/lib:${LD_LIBRARY_PATH}
export PYTHONPATH=${GITHUB_WORKSPACE}/tests/integration/dlio_benchmark:${GITHUB_WORKSPACE}/pydyad:$PYTHONPATH

unset LUA_PATH
unset LUA_CPATH
