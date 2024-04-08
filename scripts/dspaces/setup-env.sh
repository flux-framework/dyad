#!/bin/bash

export VSC_DEBUG_CONF_DIR=/g/g90/lumsden1/ws/dyad_sc24_paper_dspaces/dyad/.vscode

module load gcc/10.3.1
module load python/3.9.12
module load openmpi/4.1.2
# Configurations
export QUEUE=pbatch
export TIME="15m"
export SERVER_PPN=1

export GITHUB_WORKSPACE=/g/g90/lumsden1/ws/dyad_sc24_paper_dspaces/dyad
export SPACK_DIR=/g/g90/lumsden1/ws/spack
export SPACK_ENV=/g/g90/lumsden1/ws/dyad_sc24_paper_dspaces/baseline_env
export SPACK_VIEW=/g/g90/lumsden1/ws/dyad_sc24_paper_dspaces/baseline_env/.spack-env/view

# Activate Environments
source ${SPACK_DIR}/share/spack/setup-env.sh
spack env activate -p ${SPACK_ENV}

# Derived PATHS
export PATH=${PATH}:${SPACK_VIEW}/bin:${SPACK_VIEW}/sbin
export LD_LIBRARY_PATH=/usr/lib64:${SPACK_VIEW}/lib:${SPACK_VIEW}/lib64:${LD_LIBRARY_PATH}

unset LUA_PATH
unset LUA_CPATH
