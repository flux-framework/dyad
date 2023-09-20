#!/bin/bash

export DYAD_PATH_CONSUMER=${DYAD_PATH}_consumer
mkdir -p ${DYAD_PATH_CONSUMER}

echo "Loading DYAD module"

#flux module load ${DYAD_INSTALL_PREFIX}/lib/dyad.so  $DYAD_PATH_CONSUMER $DYAD_DTL_MODE
${GITHUB_WORKSPACE}/docs/demos/ecp_feb_2023/cpp_cons 10 $DYAD_PATH_CONSUMER