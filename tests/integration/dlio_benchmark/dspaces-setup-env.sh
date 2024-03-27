#!/bin/bash

# TODO make sure python mod matches Spack environment if there is python in there
module load python/3.9.12
module load openmpi/4.1.2

# Configurations
export DLIO_WORKLOAD=dspaces_mummi #dyad_mummi #dyad_unet3d_large # unet3d_base dyad_unet3d dyad_unet3d_small resnet50_base dyad_resnet50 unet3d_base_large mummi_base dyad_mummi
export NUM_NODES=64 # 1 for small config, 64 for full config
export PPN=8 # 1 for small config, 8 for full config
export QUEUE=pbatch
export TIME=$((60))
export BROKERS_PER_NODE=1
export GENERATE_DATA="0"
export DSPACES_HG_STRING="ofi+verbs"

export MAX_DIM_LENGTH_FOR_FILES=8000

export GITHUB_WORKSPACE=/g/g90/lumsden1/ws/dyad_sc24_paper_dspaces/dyad
export SPACK_DIR=/g/g90/lumsden1/ws/spack
export SPACK_ENV=/g/g90/lumsden1/ws/dyad_sc24_paper_dspaces/baseline_env
export SPACK_VIEW=$SPACK_ENV/.spack-env/view
export PYTHON_ENV=/g/g90/lumsden1/ws/dyad_sc24_paper_dspaces/mummi_venv
export DLIO_DATA_DIR=/p/lustre1/lumsden1/dyad_mummi #  dyad_resnet50 dyad_unet3d_basic
#export DLIO_DATA_DIR=/p/lustre2/haridev/dyad/dlio_benchmark/dyad_unet3d_basic #  dyad_resnet50

# DLIO Profiler Configurations
export DLIO_PROFILER_ENABLE=0
export DLIO_PROFILER_INC_METADATA=1
export DLIO_PROFILER_DATA_DIR=${DLIO_DATA_DIR}:${DYAD_PATH}
export DLIO_PROFILER_LOG_FILE=/g/g90/lumsden1/ws/dyad_sc24_paper_dspaces/dyad/tests/integration/dlio_benchmark/profiler/dspaces
export DLIO_PROFILER_LOG_LEVEL=ERROR
#export GOTCHA_DEBUG=3

export DLIO_PROFILER_BIND_SIGNALS=0
export MV2_BCAST_HWLOC_TOPOLOGY=0
export HDF5_USE_FILE_LOCKING=0

#mkdir -p ${DYAD_PATH}
mkdir -p ${DLIO_PROFILER_LOG_FILE}
# Activate Environments
. ${SPACK_DIR}/share/spack/setup-env.sh
spack env activate -p ${SPACK_ENV}
source ${PYTHON_ENV}/bin/activate 

# Derived Configurations
export DSPACES_DLIO_RUN_LOG=dyad_${DLIO_WORKLOAD}_${NUM_NODES}_${PPN}_${BROKERS_PER_NODE}.log
export CONFIG_ARG="--config-dir=${GITHUB_WORKSPACE}/tests/integration/dlio_benchmark/configs"
#export CONFIG_ARG=""

# Derived PATHS
export PATH=${PATH}:${SPACK_VIEW}/bin:${SPACK_VIEW}/sbin
export LD_LIBRARY_PATH=/usr/lib64:${SPACK_VIEW}/lib:${SPACK_VIEW}/lib64:${LD_LIBRARY_PATH}
export PYTHONPATH=${GITHUB_WORKSPACE}/tests/integration/dlio_benchmark:$PYTHONPATH

unset LUA_PATH
unset LUA_CPATH
