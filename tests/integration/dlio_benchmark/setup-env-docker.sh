#!/bin/bash

module load python/3.9.12
module load openmpi/4.1.2

# Configurations
export DLIO_WORKLOAD=dyad_unet3d_demo
export NUM_NODES=1
export PPN=1
export GENERATE_DATA="0"

export DYAD_INSTALL_PREFIX=/mount/shared/install
export DYAD_KVS_NAMESPACE=dyad
export DYAD_DTL_MODE=UCX
export DYAD_PATH="/mount/ssd"
export GITHUB_WORKSPACE=/mount/shared/code
export SPACK_DIR=/mount/shared/spack
export DLIO_DATA_DIR=/mount/pfs


# DLIO Profiler Configurations
export DLIO_PROFILER_ENABLE=0

# Derived Configurations
export DYAD_DLIO_RUN_LOG=dyad_${DLIO_WORKLOAD}_${NUM_NODES}_${PPN}.log
export CONFIG_ARG="--config-dir=${GITHUB_WORKSPACE}/tests/integration/dlio_benchmark/configs"


# Derived PATHS
export PATH=${PATH}:${DYAD_INSTALL_PREFIX}/bin:${DYAD_INSTALL_PREFIX}/sbin
export LD_LIBRARY_PATH=/usr/lib64:${DYAD_INSTALL_PREFIX}/lib:${LD_LIBRARY_PATH}
export PYTHONPATH=${GITHUB_WORKSPACE}/tests/integration/dlio_benchmark:${GITHUB_WORKSPACE}/pydyad:$PYTHONPATH


mkdir -p ${DLIO_DATA_DIR} ${DYAD_PATH}
unset LUA_PATH
unset LUA_CPATH
