#!/bin/bash

export DYAD_PATH_PRODUCER=${DYAD_PATH}_producer
mkdir -p ${DYAD_PATH_PRODUCER}
echo "Loading DYAD module"

echo flux module load ${DYAD_INSTALL_PREFIX}/lib/dyad.so  $DYAD_PATH_PRODUCER $DYAD_DTL_MODE
flux module load ${DYAD_INSTALL_PREFIX}/lib/dyad.so  $DYAD_PATH_PRODUCER $DYAD_DTL_MODE

echo ${GITHUB_WORKSPACE}/docs/demos/ecp_feb_2023/cpp_prod 10 $DYAD_PATH_PRODUCER
${GITHUB_WORKSPACE}/docs/demos/ecp_feb_2023/cpp_prod 10 $DYAD_PATH_PRODUCER