#!/bin/bash

module load python/3.9.12
module load openmpi/4.1.2
test_case=$1
NUM_NODES=$2
PPN=$3
# Configurations
export WORKLOAD=${test_case}_${NUM_NODES}_${PPN}
export QUEUE=pdebug
export TIME=$((60))
export BROKERS_PER_NODE=1

export DYAD_INSTALL_PREFIX=/usr/workspace/haridev/dyad/env/spack/.spack-env/view
export DYAD_KVS_NAMESPACE=dyad
export DYAD_DTL_MODE=UCX
export DYAD_PATH="/l/ssd/haridev/dyad"
#export DYAD_PATH=/dev/shm/haridev/dyad
export GITHUB_WORKSPACE=/usr/workspace/haridev/dyad
export SPACK_DIR=/usr/workspace/haridev/spack
export SPACK_ENV=/usr/workspace/haridev/dyad/env/spack
export PYTHON_ENV=/usr/workspace/haridev/dyad/env/python

# Activate Environments
. ${SPACK_DIR}/share/spack/setup-env.sh
spack env activate -p ${SPACK_ENV}
source ${PYTHON_ENV}/bin/activate 

# Derived PATHS
export PATH=${PATH}:${DYAD_INSTALL_PREFIX}/bin:${DYAD_INSTALL_PREFIX}/sbin
export LD_LIBRARY_PATH=/usr/lib64:${DYAD_INSTALL_PREFIX}/lib:${LD_LIBRARY_PATH}
export PYTHONPATH=${GITHUB_WORKSPACE}/tests/integration/dlio_benchmark:${GITHUB_WORKSPACE}/pydyad:$PYTHONPATH

unset LUA_PATH
unset LUA_CPATH
